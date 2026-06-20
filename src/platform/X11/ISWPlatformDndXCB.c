/*
 * ISWPlatformDndXCB.c - X11 drag-and-drop backend (XDND version 5)
 *
 * The X11 implementation of the generic IswDragDrop service: a full XDND v5
 * drag source and drop target.  Interoperates with any XDND-aware application
 * (GTK, Qt, Firefox, etc.).
 *
 * This is platform-specific code by design — XDND *is* an X11 wire protocol
 * (the Xdnd* atoms, ClientMessage state machine, XdndSelection ownership,
 * XdndAware discovery).  It is reached only through the IswPlatformDndOps
 * vtable (isw_platform_xcb_dnd_ops, below); widget/application code talks to
 * the transport-neutral IswDnd* service in <ISW/IswDragDrop.h>, never to XDND.
 * A non-X backend implements the same ops over its own DnD mechanism.
 *
 * Drop target: shell windows advertise XdndAware and handle the protocol
 * messages, routing drop data to the widget under the cursor.  Widgets register
 * callbacks for drop, dragEnter, dragMotion, dragLeave.
 *
 * Drag source: widgets initiate drags via the service's IswDndStartDrag.  The
 * library grabs the pointer, tracks motion, sends XDND protocol messages to
 * foreign windows, and owns XdndSelection to provide data.
 *
 * Pure XCB — no Xlib dependencies.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/IntrinsicI.h>
#include <ISW/InitialI.h>
#include <ISW/PassivGraI.h>
#include <ISW/StringDefs.h>
#include <ISW/IswDragDrop.h>
#include <ISW/IswDragDropP.h>
#include <ISW/ISWPlatform.h>
#include "ISWContextI.h"
#include <ISW/IconView.h>
#include <ISW/ViewportP.h>
#include <ISW/ISWRender.h>
#include "ISWRenderCairoXCB.h"
#include "ISWPlatformPrivate.h"
#include <xcb/xcb_cursor.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xkb.h>
#include <xcb/xcb_keysyms.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* XDND protocol version we support */
#define XDND_VERSION 5

/* Drag threshold in pixels before a press becomes a drag */
#define DRAG_THRESHOLD 3

/* Timeout for XdndFinished reply (milliseconds) */
#define FINISHED_TIMEOUT 5000

/* X11 cursor font glyph indices */
#define XC_left_ptr     68
#define XC_hand2        60
#define XC_fleur        52
#define XC_hand1        58
#define XC_X_cursor     0
#define XC_crosshair    34

/* XCB → neutral IswEvent translation (ISWPlatformEventXCB.c).  Fills *out and
 * returns True for toolkit-semantic events; returns False for X11 protocol
 * events the toolkit does not see as IswEvents. */
extern Boolean _IswEventFromXcb(IswDisplay dpy,
                                xcb_generic_event_t *xev, IswEvent *out);

/* ------------------------------------------------------------------ */
/* Per-shell XDND state                                               */
/* ------------------------------------------------------------------ */

typedef struct _XdndState {
    /* Protocol atoms */
    xcb_atom_t XdndAware;
    xcb_atom_t XdndEnter;
    xcb_atom_t XdndPosition;
    xcb_atom_t XdndStatus;
    xcb_atom_t XdndLeave;
    xcb_atom_t XdndDrop;
    xcb_atom_t XdndFinished;
    xcb_atom_t XdndSelection;
    xcb_atom_t XdndTypeList;
    xcb_atom_t XdndActionList;
    xcb_atom_t XdndProxy;

    /* Action atoms */
    xcb_atom_t action_copy;
    xcb_atom_t action_move;
    xcb_atom_t action_link;
    xcb_atom_t action_ask;
    xcb_atom_t action_private;

    /* Common MIME types */
    xcb_atom_t text_uri_list;
    xcb_atom_t text_plain;

    /* TARGETS pseudo-type */
    xcb_atom_t targets_atom;

    /* --- Drop target state (xcb wire half) --- */
    xcb_window_t    src_window;         /* source window of incoming drag */
    xcb_timestamp_t drop_timestamp;     /* timestamp from XdndDrop */

    /* --- Drag source state (xcb wire half) --- */
    xcb_timestamp_t     drag_timestamp;

    /* Target tracking during drag (xcb wire half) */
    xcb_window_t        drag_target_win;    /* foreign window under cursor */

    /* Drag icon */
    xcb_window_t        drag_icon_win;
    Boolean             drag_icon_owned;  /* we created the pixmap */
    xcb_colormap_t      drag_icon_cmap;   /* colormap for 32-bit icon window */
    xcb_visualid_t      drag_icon_visual; /* visual for 32-bit icon window */

    /* Cursors */
    xcb_cursor_t        cursor_default;
    xcb_cursor_t        cursor_copy;
    xcb_cursor_t        cursor_move;
    xcb_cursor_t        cursor_link;
    xcb_cursor_t        cursor_reject;

    /* Platform-neutral DnD policy/state (drop configs, negotiation,
       drag/drop bookkeeping). */
    IswDndCore          core;
} XdndState;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void InternAtoms(XdndState *st, xcb_connection_t *conn);
static void CreateCursors(XdndState *st, xcb_connection_t *conn,
                          xcb_screen_t *screen);

/* Drop target handlers */
static void HandleXdndEvent(Widget w, IswPointer closure,
                            IswEvent *event, Boolean *cont);
static void HandleTargetEnter(XdndState *st, const uint32_t *data);
static void HandleTargetPosition(XdndState *st, const uint32_t *data);
static void HandleTargetDrop(XdndState *st, const uint32_t *data);
static void HandleTargetLeave(XdndState *st);
static void TargetSelectionCallback(Widget w, IswPointer closure,
                                    xcb_atom_t *selection, xcb_atom_t *type,
                                    IswPointer value, unsigned long *length,
                                    int *format);
static void SendXdndStatus(XdndState *st, Boolean accept,
                           xcb_atom_t action_atom);
static void SendXdndFinished(XdndState *st, Boolean accept,
                             xcb_atom_t action_atom);

/* Widget tree walk */
static Widget FindDropTarget(XdndState *st, int root_x, int root_y);

/* Drag source */
static void HandleDragEvent(Widget w, IswPointer closure,
                            IswEvent *event, Boolean *cont);
static void DragMotion(XdndState *st, int root_x, int root_y,
                       unsigned int modifiers);
static void DragDrop(XdndState *st);
static void DragCancel(XdndState *st);
static void DragCleanup(XdndState *st);
static xcb_window_t FindXdndAwareWindow(XdndState *st, xcb_connection_t *conn,
                                        xcb_window_t child, int *version_out);
static void SendDragEnter(XdndState *st, xcb_window_t target);
static void SendDragPosition(XdndState *st, int root_x, int root_y);
static void SendDragLeave(XdndState *st);
static void SendDragDrop(XdndState *st);
static void HandleDragStatus(XdndState *st, const uint32_t *data);
static void HandleDragFinished(XdndState *st, const uint32_t *data);
static void DragFinishedTimeout(IswPointer closure, IswIntervalId *id);

/* Drag source selection */
static Boolean DragConvertSelection(Widget w, xcb_atom_t *selection,
                                    xcb_atom_t *target, xcb_atom_t *type_return,
                                    IswPointer *value_return,
                                    unsigned long *length_return,
                                    int *format_return);
static void DragLoseSelection(Widget w, xcb_atom_t *selection);

/* Drag icon */
static void CreateDragIcon(XdndState *st);
static void MoveDragIcon(XdndState *st, int root_x, int root_y);
static void DestroyDragIcon(XdndState *st);

/* ------------------------------------------------------------------ */
/* Atom internment                                                    */
/* ------------------------------------------------------------------ */

/* Intern an atom by name on a raw connection.  Backend-local: the XDND engine
   works directly on the xcb connection, so it interns its protocol atoms here
   rather than through the display-keyed selection/atom ops. */
static xcb_atom_t
IswXcbInternAtom(xcb_connection_t *conn, const char *name, Bool only_if_exists)
{
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = 0;

    if (!conn || !name)
        return 0;

    cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0,
                             (uint16_t) strlen(name), name);
    reply = xcb_intern_atom_reply(conn, cookie, NULL);
    if (reply) {
        atom = reply->atom;
        free(reply);
    }
    return atom;
}

static void
InternAtoms(XdndState *st, xcb_connection_t *conn)
{
    st->XdndAware      = IswXcbInternAtom(conn, "XdndAware", False);
    st->XdndEnter      = IswXcbInternAtom(conn, "XdndEnter", False);
    st->XdndPosition   = IswXcbInternAtom(conn, "XdndPosition", False);
    st->XdndStatus     = IswXcbInternAtom(conn, "XdndStatus", False);
    st->XdndLeave      = IswXcbInternAtom(conn, "XdndLeave", False);
    st->XdndDrop       = IswXcbInternAtom(conn, "XdndDrop", False);
    st->XdndFinished   = IswXcbInternAtom(conn, "XdndFinished", False);
    st->XdndSelection  = IswXcbInternAtom(conn, "XdndSelection", False);
    st->XdndTypeList   = IswXcbInternAtom(conn, "XdndTypeList", False);
    st->XdndActionList = IswXcbInternAtom(conn, "XdndActionList", False);
    st->XdndProxy      = IswXcbInternAtom(conn, "XdndProxy", False);

    st->action_copy    = IswXcbInternAtom(conn, "XdndActionCopy", False);
    st->action_move    = IswXcbInternAtom(conn, "XdndActionMove", False);
    st->action_link    = IswXcbInternAtom(conn, "XdndActionLink", False);
    st->action_ask     = IswXcbInternAtom(conn, "XdndActionAsk", False);
    st->action_private = IswXcbInternAtom(conn, "XdndActionPrivate", False);

    st->text_uri_list  = IswXcbInternAtom(conn, "text/uri-list", False);
    st->text_plain     = IswXcbInternAtom(conn, "text/plain", False);

    st->targets_atom   = IswXcbInternAtom(conn, "TARGETS", False);

}

static IswDndAction
XdndAtomToAction(XdndState *st, xcb_atom_t atom)
{
    if (atom == st->action_copy)    return ISW_DND_ACTION_COPY;
    if (atom == st->action_move)    return ISW_DND_ACTION_MOVE;
    if (atom == st->action_link)    return ISW_DND_ACTION_LINK;
    if (atom == st->action_ask)     return ISW_DND_ACTION_ASK;
    if (atom == st->action_private) return ISW_DND_ACTION_PRIVATE;
    return ISW_DND_ACTION_NONE;
}

static xcb_atom_t
XdndActionToAtom(XdndState *st, IswDndAction action)
{
    if (action & ISW_DND_ACTION_COPY)    return st->action_copy;
    if (action & ISW_DND_ACTION_MOVE)    return st->action_move;
    if (action & ISW_DND_ACTION_LINK)    return st->action_link;
    if (action & ISW_DND_ACTION_ASK)     return st->action_ask;
    if (action & ISW_DND_ACTION_PRIVATE) return st->action_private;
    return XCB_ATOM_NONE;
}

static const char *
XdndAtomToString(XdndState *st, xcb_atom_t atom)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(conn, atom);
    xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply(conn, cookie, NULL);
    if (!reply)
        return NULL;
    int len = xcb_get_atom_name_name_length(reply);
    char *str = IswMalloc(len + 1);
    memcpy(str, xcb_get_atom_name_name(reply), len);
    str[len] = '\0';
    free(reply);
    return str;
}

static xcb_atom_t
XdndStringToAtom(XdndState *st, const char *name)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    return IswXcbInternAtom(conn, name, False);
}

/* ------------------------------------------------------------------ */
/* Cursor creation                                                    */
/* ------------------------------------------------------------------ */

static xcb_cursor_t
CreateGlyphCursor(xcb_connection_t *conn, unsigned int shape)
{
    static xcb_font_t cursor_font = XCB_NONE;
    xcb_cursor_t cursor;

    if (cursor_font == XCB_NONE) {
        cursor_font = xcb_generate_id(conn);
        xcb_open_font(conn, cursor_font, 6, "cursor");
    }
    cursor = xcb_generate_id(conn);
    xcb_create_glyph_cursor(conn, cursor,
                            cursor_font, cursor_font,
                            shape, shape + 1,
                            0, 0, 0,
                            65535, 65535, 65535);
    return cursor;
}

static xcb_cursor_t
LoadThemedCursor(xcb_connection_t *conn, xcb_screen_t *screen,
                 const char **names, int nnames, unsigned int shape)
{
    xcb_cursor_context_t *ctx;
    if (xcb_cursor_context_new(conn, screen, &ctx) < 0)
        return CreateGlyphCursor(conn, shape);

    xcb_cursor_t cursor = XCB_CURSOR_NONE;
    for (int i = 0; i < nnames && cursor == XCB_CURSOR_NONE; i++)
        cursor = xcb_cursor_load_cursor(ctx, names[i]);

    xcb_cursor_context_free(ctx);

    if (cursor == XCB_CURSOR_NONE)
        return CreateGlyphCursor(conn, shape);

    return cursor;
}

static void
CreateCursors(XdndState *st, xcb_connection_t *conn, xcb_screen_t *screen)
{
    static const char *default_names[] = {
        "dnd-none", "grabbing", "closedhand", "fleur", "left_ptr"
    };
    static const char *copy_names[] = {
        "dnd-copy", "copy", "left_ptr"
    };
    static const char *move_names[] = {
        "dnd-move", "move", "grabbing", "closedhand", "fleur"
    };
    static const char *link_names[] = {
        "dnd-link", "link", "alias", "left_ptr"
    };
    static const char *reject_names[] = {
        "dnd-no-drop", "no-drop", "not-allowed", "crossed_circle", "X_cursor"
    };

#define NELEM(a) (int)(sizeof(a) / sizeof(a[0]))
    st->cursor_default = LoadThemedCursor(conn, screen, default_names, NELEM(default_names), XC_fleur);
    st->cursor_copy    = LoadThemedCursor(conn, screen, copy_names,    NELEM(copy_names),    XC_hand2);
    st->cursor_move    = LoadThemedCursor(conn, screen, move_names,    NELEM(move_names),    XC_fleur);
    st->cursor_link    = LoadThemedCursor(conn, screen, link_names,    NELEM(link_names),    XC_hand1);
    st->cursor_reject  = LoadThemedCursor(conn, screen, reject_names,  NELEM(reject_names),  XC_X_cursor);
#undef NELEM
}

/* ------------------------------------------------------------------ */
/* Widget tree walk to find drop target                               */
/* ------------------------------------------------------------------ */

static Widget
FindDropTarget(XdndState *st, int root_x, int root_y)
{
    Widget shell = st->core.shell;
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(shell));

    xcb_translate_coordinates_cookie_t cookie =
        xcb_translate_coordinates(conn,
            _IswXcbScreen(IswScreenOf(shell))->root, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell))),
            (int16_t) root_x, (int16_t) root_y);
    xcb_translate_coordinates_reply_t *reply =
        xcb_translate_coordinates_reply(conn, cookie, NULL);

    if (!reply) {
        return NULL;
    }

    double sf = ISWScaleFactor(shell);
    int wx = (int)(reply->dst_x / sf + 0.5);
    int wy = (int)(reply->dst_y / sf + 0.5);
    free(reply);

    if (_IswDndFindConfig(&st->core, shell)) {
        return shell;
    }
    if (IswHasCallbacks(shell, IswNdropCallback) == IswCallbackHasSome) {
        return shell;
    }

    Widget result = _IswDndFindDropChild(&st->core, shell, wx, wy);

    /* Fallback: iterate registered DropConfigs directly.  This handles
       widgets inside Viewports whose clip windows hide them from the
       normal widget-tree walk. */
    if (!result) {
        DropConfig *dc;
        for (dc = st->core.drop_configs; dc; dc = dc->next) {
            if (dc->widget == shell || !IswIsRealized(dc->widget))
                continue;

            /* For Viewport children, the widget is reparented into a clip
               window (Viewport → clip → child).  The widget window is
               scrolled inside the clip, so translating through it gives
               the wrong screen position.  Detect this by checking if the
               grandparent is a Viewport and use its clip widget instead. */
            Widget bounds_widget = dc->widget;
            Widget parent = IswParent(dc->widget);
            Widget grandparent = parent ? IswParent(parent) : NULL;
            if (grandparent && IswIsSubclass(grandparent, viewportWidgetClass)) {
                ViewportWidget vp = (ViewportWidget) grandparent;
                if (vp->viewport.clip && IswIsRealized(vp->viewport.clip))
                    bounds_widget = vp->viewport.clip;
            }

            /* xcb_translate_coordinates returns physical (server) coords;
               descale to logical to match core geometry. */
            xcb_translate_coordinates_cookie_t tc =
                xcb_translate_coordinates(conn,
                    _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(bounds_widget)), (Widget)(bounds_widget))), _IswXcbScreen(IswScreenOf(st->core.shell))->root,
                    0, 0);
            xcb_translate_coordinates_reply_t *tr =
                xcb_translate_coordinates_reply(conn, tc, NULL);
            if (!tr) continue;
            int abs_x = (int)(tr->dst_x / sf + 0.5);
            int abs_y = (int)(tr->dst_y / sf + 0.5);
            int w = (int) bounds_widget->core.width;
            int h = (int) bounds_widget->core.height;
            free(tr);
            int lrx = (int)(root_x / sf + 0.5);
            int lry = (int)(root_y / sf + 0.5);
            if (lrx >= abs_x && lry >= abs_y &&
                lrx < abs_x + w &&
                lry < abs_y + h) {
                result = dc->widget;
                break;
            }
        }
    }

    return result;
}

/* ================================================================== */
/*                                                                    */
/* PUBLIC API                                                         */
/*                                                                    */
/* ================================================================== */

/* We need a way to find the XdndState from various contexts.
 * Store it as widget context data on the shell. */

static XContext xdnd_context = 0;

static XdndState *
GetXdndState(Widget shell)
{
    XdndState *st = NULL;
    if (xdnd_context == 0)
        return NULL;
    if (IswFindContext(IswDisplayOf(shell), _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell))),
                       xdnd_context, (void **) &st) != 0)
        return NULL;
    return st;
}

static XdndState *
GetXdndStateForWidget(Widget w)
{
    /* Walk up to the shell */
    Widget shell = w;
    while (shell && !IswIsShell(shell))
        shell = IswParent(shell);
    if (!shell)
        return NULL;
    return GetXdndState(shell);
}

/* ================================================================== */
/*                                                                    */
/* BACKEND OPS — the IswPlatformDndOps implementation                 */
/*                                                                    */
/* These are the X11 backend's drag-and-drop verbs.  The generic       */
/* IswDnd* service (bottom of this file) dispatches to them through    */
/* the _IswPlatformDnd* wrappers and the platform ops table.           */
/* ================================================================== */

static void
xcb_dnd_enable(Widget shell)
{
    xcb_connection_t *conn;
    XdndState *st;
    uint32_t version = XDND_VERSION;

    if (!IswIsRealized(shell))
        return;

    if (xdnd_context == 0)
        xdnd_context = IswUniqueContext();

    /* Check if already enabled */
    if (GetXdndState(shell))
        return;

    conn = _IswXcbConn(IswDisplayOf(shell));
    st = (XdndState *) IswCalloc(1, sizeof(XdndState));
    st->core.shell = shell;
    InternAtoms(st, conn);
    CreateCursors(st, conn, _IswXcbScreen(IswScreenOf(shell)));

    /* Store state on the shell window */
    IswSaveContext(IswDisplayOf(shell), _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell))),
                   xdnd_context, (void *) st);

    /* Advertise XdndAware */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(shell)), (Widget)(shell))),
                        st->XdndAware, XCB_ATOM_ATOM, 32, 1, &version);

    /* Register non-maskable event handler for ClientMessage and SelectionNotify */
    IswAddEventHandler(shell, (EventMask) 0, TRUE,
                      HandleXdndEvent, (IswPointer) st);

    xcb_flush(conn);
}

static void
xcb_dnd_widget_accept_drops(Widget w)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;
    /* Ensure a DropConfig exists so the widget is registered */
    _IswDndGetOrCreateConfig(&st->core, w);
}

static void
xcb_dnd_set_accepted_types(Widget w, const char **types, int num_types)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = _IswDndGetOrCreateConfig(&st->core, w);

    if (dc->accepted_types) {
        for (int i = 0; i < dc->num_accepted_types; i++)
            IswFree((char *) dc->accepted_types[i]);
        IswFree((char *) dc->accepted_types);
    }

    if (types && num_types > 0) {
        dc->accepted_types = (const char **) IswMalloc(num_types * sizeof(const char *));
        for (int i = 0; i < num_types; i++)
            dc->accepted_types[i] = IswNewString(types[i]);
        dc->num_accepted_types = num_types;
    } else {
        dc->accepted_types = NULL;
        dc->num_accepted_types = 0;
    }
}

static void
xcb_dnd_set_accepted_actions(Widget w, IswDndAction actions)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = _IswDndGetOrCreateConfig(&st->core, w);
    dc->accepted_actions = actions;
}

static void
xcb_dnd_set_drop_callback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = _IswDndGetOrCreateConfig(&st->core, w);
    dc->drop_proc = proc;
    dc->drop_closure = closure;
}

static void
xcb_dnd_set_drag_motion_callback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = _IswDndGetOrCreateConfig(&st->core, w);
    dc->motion_proc = proc;
    dc->motion_closure = closure;
}

static void
xcb_dnd_set_drag_leave_callback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = _IswDndGetOrCreateConfig(&st->core, w);
    dc->leave_proc = proc;
    dc->leave_closure = closure;
}

static Boolean
xcb_dnd_is_dragging(Widget w)
{
    XdndState *st = GetXdndStateForWidget(w);
    return st && st->core.dragging;
}

/* ================================================================== */
/*                                                                    */
/* DROP TARGET — incoming drag handling                                */
/*                                                                    */
/* ================================================================== */

static void
HandleXdndEvent(Widget w, IswPointer closure, IswEvent *iswev,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;

    if (iswev->kind != IswProtocol)
        return;

    IswProtocolId mt = iswev->protocol.message_type;
    const uint32_t *data = iswev->protocol.data;

    /* Drop target messages (we are the target) */
    if (mt == (IswProtocolId) st->XdndEnter) {
        HandleTargetEnter(st, data);
        *cont = FALSE;
    } else if (mt == (IswProtocolId) st->XdndPosition) {
        HandleTargetPosition(st, data);
        *cont = FALSE;
    } else if (mt == (IswProtocolId) st->XdndDrop) {
        HandleTargetDrop(st, data);
        *cont = FALSE;
    } else if (mt == (IswProtocolId) st->XdndLeave) {
        HandleTargetLeave(st);
        *cont = FALSE;
    }

    /* Drag source messages (we are the source) */
    if (st->core.dragging) {
        if (mt == (IswProtocolId) st->XdndStatus) {
            HandleDragStatus(st, data);
            *cont = FALSE;
        } else if (mt == (IswProtocolId) st->XdndFinished) {
            HandleDragFinished(st, data);
            *cont = FALSE;
        }
    }

    (void) w;
}

/* ------------------------------------------------------------------ */
/* XdndEnter — source entered our window                              */
/* ------------------------------------------------------------------ */

static void
HandleTargetEnter(XdndState *st, const uint32_t *data)
{
    st->src_window = data[0];
    st->core.src_version = (data[1] >> 24) & 0xFF;

    if (st->core.src_version > XDND_VERSION)
        return;

    /* Free previous type list */
    if (st->core.src_types) {
        for (int i = 0; i < st->core.src_num_types; i++)
            IswFree((char *) st->core.src_types[i]);
        IswFree((char *) st->core.src_types);
        st->core.src_types = NULL;
        st->core.src_num_types = 0;
    }

    Boolean use_type_list = (data[1] & 1);
    xcb_atom_t raw_atoms[256];
    int count = 0;

    if (use_type_list) {
        xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->src_window,
                             st->XdndTypeList, XCB_ATOM_ATOM, 0, 256);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);
        if (reply) {
            xcb_atom_t *atoms = (xcb_atom_t *) xcb_get_property_value(reply);
            count = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
            if (count > 256) count = 256;
            memcpy(raw_atoms, atoms, count * sizeof(xcb_atom_t));
            free(reply);
        }
    } else {
        for (int i = 2; i <= 4; i++) {
            if (data[i] != XCB_ATOM_NONE)
                raw_atoms[count++] = (xcb_atom_t) data[i];
        }
    }

    if (count > 0) {
        st->core.src_types = (const char **) IswMalloc(count * sizeof(const char *));
        st->core.src_num_types = count;
        for (int i = 0; i < count; i++)
            st->core.src_types[i] = XdndAtomToString(st, raw_atoms[i]);
    }

    /* Default: assume copy is offered (many sources don't advertise actions) */
    st->core.src_actions = ISW_DND_ACTION_COPY;

    st->core.hover_widget = NULL;
    st->core.negotiated_type = NULL;
    st->core.negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndPosition — source moved over our window                        */
/* ------------------------------------------------------------------ */

static void
HandleTargetPosition(XdndState *st, const uint32_t *data)
{
    st->core.drop_x = (int)(data[2] >> 16);
    st->core.drop_y = (int)(data[2] & 0xFFFF);
    xcb_atom_t proposed_atom = (xcb_atom_t) data[4];
    IswDndAction proposed = XdndAtomToAction(st, proposed_atom);
    if (proposed == ISW_DND_ACTION_NONE)
        proposed = ISW_DND_ACTION_COPY;

    /* Find the widget under the cursor */
    Widget target = FindDropTarget(st, st->core.drop_x, st->core.drop_y);

    /* Handle enter/leave transitions */
    if (target != st->core.hover_widget) {
        /* Leave old widget */
        if (st->core.hover_widget &&
            IswHasCallbacks(st->core.hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            IswCallCallbacks(st->core.hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
        } else if (st->core.hover_widget) {
            DropConfig *dc = _IswDndFindConfig(&st->core, st->core.hover_widget);
            if (dc && dc->leave_proc) {
                IswDragOverCallbackData cbd = {0};
                dc->leave_proc(st->core.hover_widget, dc->leave_closure, (IswPointer) &cbd);
            }
        }

        st->core.hover_widget = target;

        /* Enter new widget */
        if (target &&
            IswHasCallbacks(target, IswNdragEnterCallback) == IswCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            cbd.offered_types = st->core.src_types;
            cbd.num_offered_types = st->core.src_num_types;
            cbd.offered_actions = st->core.src_actions;
            cbd.proposed_action = proposed;
            IswCallCallbacks(target, IswNdragEnterCallback, (IswPointer) &cbd);
        }
    }

    Boolean accept = False;
    const char *accepted_type = NULL;
    IswDndAction accepted_action = ISW_DND_ACTION_NONE;

    if (target) {
        /* First, let the widget's dragMotion callback override */
        if (IswHasCallbacks(target, IswNdragMotionCallback) == IswCallbackHasSome) {
            xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
            double sf = ISWScaleFactor(st->core.shell);
            xcb_translate_coordinates_cookie_t tc =
                xcb_translate_coordinates(conn,
                    _IswXcbScreen(IswScreenOf(st->core.shell))->root, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(target)), (Widget)(target))),
                    (int16_t) st->core.drop_x, (int16_t) st->core.drop_y);
            xcb_translate_coordinates_reply_t *tr =
                xcb_translate_coordinates_reply(conn, tc, NULL);

            IswDragOverCallbackData cbd = {0};
            cbd.x = tr ? (int)(tr->dst_x / sf + 0.5) : 0;
            cbd.y = tr ? (int)(tr->dst_y / sf + 0.5) : 0;
            free(tr);
            cbd.offered_types = st->core.src_types;
            cbd.num_offered_types = st->core.src_num_types;
            cbd.offered_actions = st->core.src_actions;
            cbd.proposed_action = proposed;

            IswCallCallbacks(target, IswNdragMotionCallback, (IswPointer) &cbd);

            if (cbd.accepted_type != NULL) {
                accepted_type = cbd.accepted_type;
                accepted_action = cbd.accepted_action;
                accept = True;
            }
        } else {
            DropConfig *dc = _IswDndFindConfig(&st->core, target);
            if (dc && dc->motion_proc) {
                xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
                double sf = ISWScaleFactor(st->core.shell);
                xcb_translate_coordinates_cookie_t tc =
                    xcb_translate_coordinates(conn,
                        _IswXcbScreen(IswScreenOf(st->core.shell))->root, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(target)), (Widget)(target))),
                        (int16_t) st->core.drop_x, (int16_t) st->core.drop_y);
                xcb_translate_coordinates_reply_t *tr =
                    xcb_translate_coordinates_reply(conn, tc, NULL);

                IswDragOverCallbackData cbd = {0};
                cbd.x = tr ? (int)(tr->dst_x / sf + 0.5) : 0;
                cbd.y = tr ? (int)(tr->dst_y / sf + 0.5) : 0;
                free(tr);
                cbd.offered_types = st->core.src_types;
                cbd.num_offered_types = st->core.src_num_types;
                cbd.offered_actions = st->core.src_actions;
                cbd.proposed_action = proposed;

                dc->motion_proc(target, dc->motion_closure, (IswPointer) &cbd);

                if (cbd.accepted_type != NULL) {
                    accepted_type = cbd.accepted_type;
                    accepted_action = cbd.accepted_action;
                    accept = True;
                }
            }
        }

        /* If callback didn't accept, try automatic negotiation */
        if (!accept) {
            accept = _IswDndNegotiateType(&st->core, target,
                                          &accepted_type, &accepted_action);
        }
    }

    st->core.negotiated_type = accepted_type;
    st->core.negotiated_action = accepted_action;

    SendXdndStatus(st, accept,
                   accept ? XdndActionToAtom(st, accepted_action)
                          : XCB_ATOM_NONE);
}

/* ------------------------------------------------------------------ */
/* XdndDrop — source released over our window                         */
/* ------------------------------------------------------------------ */

static void
HandleTargetDrop(XdndState *st, const uint32_t *data)
{
    st->drop_timestamp = data[2];
    if (!st->core.negotiated_type || !st->core.hover_widget) {
        SendXdndFinished(st, False, XCB_ATOM_NONE);
        HandleTargetLeave(st);
        return;
    }

    xcb_atom_t type_atom = XdndStringToAtom(st, st->core.negotiated_type);
    IswGetSelectionValue(st->core.shell, st->XdndSelection,
                        type_atom,
                        TargetSelectionCallback,
                        (IswPointer) st,
                        st->drop_timestamp);
}

/* ------------------------------------------------------------------ */
/* Selection data arrival for drop target                             */
/* ------------------------------------------------------------------ */

static void
TargetSelectionCallback(Widget w, IswPointer closure,
                        xcb_atom_t *selection, xcb_atom_t *type,
                        IswPointer value, unsigned long *length,
                        int *format)
{
    XdndState *st = (XdndState *) closure;

    (void) w;
    (void) selection;

    if (!value || !length || *length == 0) {
        xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
        xcb_atom_t neg_atom = st->core.negotiated_type
            ? XdndStringToAtom(st, st->core.negotiated_type) : XCB_ATOM_NONE;
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->src_window,
                             neg_atom, XCB_ATOM_ANY, 0, 65536);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);

        if (reply) {
            char *data = (char *) xcb_get_property_value(reply);
            int data_len = xcb_get_property_value_length(reply);

            if (data && data_len > 0) {
                char *data_copy = IswMalloc(data_len);
                memcpy(data_copy, data, data_len);

                unsigned long len = data_len;
                int fmt = 8;
                xcb_atom_t tp = neg_atom;
                free(reply);
                TargetSelectionCallback(w, closure, selection,
                                        &tp, data_copy, &len, &fmt);
                return;
            }
            free(reply);
        }

        SendXdndFinished(st, False, XCB_ATOM_NONE);
        HandleTargetLeave(st);
        return;
    }

    Widget target = st->core.hover_widget;
    if (!target)
        target = FindDropTarget(st, st->core.drop_x, st->core.drop_y);

    if (target) {
        xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
        xcb_translate_coordinates_cookie_t tc =
            xcb_translate_coordinates(conn,
                _IswXcbScreen(IswScreenOf(st->core.shell))->root, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(target)), (Widget)(target))),
                (int16_t) st->core.drop_x, (int16_t) st->core.drop_y);
        xcb_translate_coordinates_reply_t *tr =
            xcb_translate_coordinates_reply(conn, tc, NULL);

        IswDropCallbackData cb;
        memset(&cb, 0, sizeof(cb));
        cb.x = tr ? tr->dst_x : 0;
        cb.y = tr ? tr->dst_y : 0;
        free(tr);

        cb.data = value;
        cb.data_length = *length;
        cb.data_type = st->core.negotiated_type;
        cb.data_format = format ? *format : 8;
        cb.action = st->core.negotiated_action;

        if (st->core.negotiated_type &&
            strcmp(st->core.negotiated_type, "text/uri-list") == 0) {
            cb.uris = _IswDndParseUriList((const char *) value, (int) *length,
                                          &cb.num_uris);
        }

        /* Deliver via Xt callback list if available, otherwise use
         * the direct callback stored in the DropConfig. */
        if (IswHasCallbacks(target, IswNdropCallback) == IswCallbackHasSome) {
            IswCallCallbacks(target, IswNdropCallback, (IswPointer) &cb);
        } else {
            DropConfig *dc = _IswDndFindConfig(&st->core, target);
            if (dc && dc->drop_proc)
                dc->drop_proc(target, dc->drop_closure, (IswPointer) &cb);
        }

        /* Free URI strings */
        if (cb.uris) {
            for (int i = 0; i < cb.num_uris; i++)
                IswFree(cb.uris[i]);
            IswFree((char *) cb.uris);
        }
    }

    IswFree(value);

    SendXdndFinished(st, True,
                     XdndActionToAtom(st, st->core.negotiated_action));

    /* Fire dragLeave on the hover widget */
    if (st->core.hover_widget &&
        IswHasCallbacks(st->core.hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        IswCallCallbacks(st->core.hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
    } else if (st->core.hover_widget) {
        DropConfig *dc = _IswDndFindConfig(&st->core, st->core.hover_widget);
        if (dc && dc->leave_proc) {
            IswDragOverCallbackData cbd = {0};
            dc->leave_proc(st->core.hover_widget, dc->leave_closure, (IswPointer) &cbd);
        }
    }

    /* Reset drop target state */
    st->src_window = 0;
    st->core.hover_widget = NULL;
    if (st->core.src_types) {
        for (int i = 0; i < st->core.src_num_types; i++)
            IswFree((char *) st->core.src_types[i]);
        IswFree((char *) st->core.src_types);
        st->core.src_types = NULL;
        st->core.src_num_types = 0;
    }
    st->core.negotiated_type = NULL;
    st->core.negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndLeave — source left our window                                 */
/* ------------------------------------------------------------------ */

static void
HandleTargetLeave(XdndState *st)
{
    if (st->core.hover_widget &&
        IswHasCallbacks(st->core.hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        IswCallCallbacks(st->core.hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
    } else if (st->core.hover_widget) {
        DropConfig *dc = _IswDndFindConfig(&st->core, st->core.hover_widget);
        if (dc && dc->leave_proc) {
            IswDragOverCallbackData cbd = {0};
            dc->leave_proc(st->core.hover_widget, dc->leave_closure, (IswPointer) &cbd);
        }
    }

    st->src_window = 0;
    st->core.hover_widget = NULL;
    if (st->core.src_types) {
        for (int i = 0; i < st->core.src_num_types; i++)
            IswFree((char *) st->core.src_types[i]);
        IswFree((char *) st->core.src_types);
        st->core.src_types = NULL;
        st->core.src_num_types = 0;
    }
    st->core.negotiated_type = NULL;
    st->core.negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndStatus / XdndFinished replies                                  */
/* ------------------------------------------------------------------ */

static void
SendXdndStatus(XdndState *st, Boolean accept, xcb_atom_t action_atom)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndStatus;
    reply.format = 32;
    reply.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));
    reply.data.data32[1] = accept ? 1 : 0;
    reply.data.data32[2] = 0;  /* empty rectangle */
    reply.data.data32[3] = 0;
    reply.data.data32[4] = accept ? action_atom : XCB_ATOM_NONE;

    xcb_send_event(conn, False, st->src_window, 0, (const char *) &reply);
    xcb_flush(conn);
}

static void
SendXdndFinished(XdndState *st, Boolean accept, xcb_atom_t action_atom)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndFinished;
    reply.format = 32;
    reply.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));
    reply.data.data32[1] = accept ? 1 : 0;
    reply.data.data32[2] = accept ? action_atom : XCB_ATOM_NONE;

    xcb_send_event(conn, False, st->src_window, 0, (const char *) &reply);
    xcb_flush(conn);
}

/* ================================================================== */
/*                                                                    */
/* DRAG SOURCE — outgoing drag handling                               */
/*                                                                    */
/* ================================================================== */

static void
xcb_dnd_start_drag(Widget source_widget,
                   IswEvent *trigger,
                   IswDragSourceDesc *desc)
{
    /* The drag is triggered by a neutral button-press event.  Its
     * coordinates are widget-local / root logical pixels (already
     * HiDPI-descaled by the dispatcher), and the drag bookkeeping below
     * (threshold math against neutral motion coords, the drag icon, and
     * IconView hit-testing) operates in that same logical space; the wire
     * protocol re-queries the pointer for physical coords where it needs
     * them.  The X timestamp comes from the neutral header. */
    if (!trigger || (trigger->kind != IswButtonDown &&
                     trigger->kind != IswButtonUp))
        return;

    XdndState *st = GetXdndStateForWidget(source_widget);
    if (!st || st->core.dragging)
        return;

    /* Don't start a drag if the source widget has an active rubber band */
    if (IswIsSubclass(source_widget, iconViewWidgetClass) &&
        IswIconViewBandActive(source_widget))
        return;

    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    st->core.dragging = True;
    st->core.drag_desc = *desc;
    st->core.drag_source = source_widget;
    st->drag_timestamp = (xcb_timestamp_t) trigger->any.time;
    st->core.drag_start_x = trigger->button.root_x;
    st->core.drag_start_y = trigger->button.root_y;
    st->core.drag_press_x = trigger->button.x;
    st->core.drag_press_y = trigger->button.y;
    st->core.drag_started = False;
    st->drag_target_win = XCB_NONE;
    st->core.drag_target_ver = 0;
    st->core.drag_status_pending = False;
    st->core.drag_target_accepted = False;
    st->core.drag_target_action = ISW_DND_ACTION_NONE;
    st->core.drag_last_x = trigger->button.root_x;
    st->core.drag_last_y = trigger->button.root_y;
    st->core.drag_position_deferred = False;
    st->drag_icon_win = XCB_NONE;
    st->drag_icon_owned = False;
    st->drag_icon_cmap = XCB_NONE;
    st->drag_icon_visual = 0;
    st->core.finished_timer = 0;

    /* Copy the type list as strings (caller's array may be transient) */
    if (desc->num_types > 0) {
        st->core.drag_desc.types = (const char **) IswMalloc(
            desc->num_types * sizeof(const char *));
        for (int i = 0; i < desc->num_types; i++)
            st->core.drag_desc.types[i] = IswNewString(desc->types[i]);
    }

    /* Own XdndSelection */
    (void) IswOwnSelection(st->core.shell, st->XdndSelection, st->drag_timestamp,
                    DragConvertSelection, DragLoseSelection, NULL);

    /* Intern type strings to atoms for the XDND wire protocol */
    xcb_atom_t *type_atoms = NULL;
    if (desc->num_types > 0) {
        type_atoms = (xcb_atom_t *) IswMalloc(desc->num_types * sizeof(xcb_atom_t));
        for (int i = 0; i < desc->num_types; i++)
            type_atoms[i] = XdndStringToAtom(st, desc->types[i]);
    }

    /* Set XdndTypeList property on our window if >3 types */
    if (desc->num_types > 3) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell))),
                            st->XdndTypeList, XCB_ATOM_ATOM, 32,
                            desc->num_types, type_atoms);
    }

    /* Eagerly convert and store drag data as a property on the source
     * window for cross-client fallback. */
    if (desc->convert) {
        for (int i = 0; i < desc->num_types; i++) {
            IswPointer data = NULL;
            unsigned long length = 0;
            int format = 8;
            if (desc->convert(st->core.drag_source, desc->types[i],
                              &data, &length, &format, desc->client_data)) {
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE,
                                    _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell))),
                                    type_atoms[i], type_atoms[i],
                                    format, length, data);
                IswFree(data);
            }
        }
        xcb_flush(conn);
    }

    IswFree((char *) type_atoms);

    /* Set XdndActionList property */
    {
        xcb_atom_t actions[5];
        int n = 0;
        if (desc->actions & ISW_DND_ACTION_COPY)    actions[n++] = st->action_copy;
        if (desc->actions & ISW_DND_ACTION_MOVE)    actions[n++] = st->action_move;
        if (desc->actions & ISW_DND_ACTION_LINK)    actions[n++] = st->action_link;
        if (desc->actions & ISW_DND_ACTION_ASK)     actions[n++] = st->action_ask;
        if (desc->actions & ISW_DND_ACTION_PRIVATE) actions[n++] = st->action_private;
        if (n > 0) {
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell))),
                                st->XdndActionList, XCB_ATOM_ATOM, 32, n, actions);
        }
    }

    /* Capture the pointer on the shell window.  During an active capture
     * all pointer events report on the grab window regardless of where
     * the cursor is, so we still track movement over foreign apps.  Using
     * the shell (not root) ensures Xt dispatches events to our handler. */
    if (IswGrabPointer(st->core.shell, False,
                       IswButtonReleaseMask | IswPointerMotionMask |
                       IswButtonMotionMask,
                       st->cursor_default,
                       st->drag_timestamp) != IswGrabSuccess) {
        DragCleanup(st);
        return;
    }

    /* The pointer capture now owns the pointer, so motion events report
     * against the shell window and must reach HandleDragEvent.  The source
     * widget is windowless, so its button press armed the windowless
     * implicit grab; flag the drag so the dispatcher yields that grab and
     * lets motion fall through to the shell. */
    {
        IswPerDisplayInput pdi = _IswGetPerDisplayInput(IswDisplayOf(st->core.shell));
        if (pdi)
            pdi->xdndDragActive = True;
    }

    /* Capture the keyboard so key events dispatch to the shell, where
     * HandleDragEvent is registered. */
    IswGrabKeyboard(st->core.shell, False, st->drag_timestamp);

    /* Install raw event handler for drag tracking */
    IswAddEventHandler(st->core.shell,
                      XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                      XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                      XCB_EVENT_MASK_KEY_RELEASE,
                      TRUE,  /* non-maskable too, for ClientMessage */
                      HandleDragEvent, (IswPointer) st);

    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* Drag event handler                                                 */
/* ------------------------------------------------------------------ */

static void
HandleDragEvent(Widget w, IswPointer closure, IswEvent *iswev,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;

    (void) w;

    if (!st->core.dragging) {
        return;
    }

    switch (iswev->kind) {
    case IswMotion: {
        int root_x = iswev->motion.root_x;
        int root_y = iswev->motion.root_y;

        if (!st->core.drag_started) {
            int dx = root_x - st->core.drag_start_x;
            int dy = root_y - st->core.drag_start_y;
            if (dx * dx + dy * dy < DRAG_THRESHOLD * DRAG_THRESHOLD)
                return;
            st->core.drag_started = True;
            CreateDragIcon(st);
        }

        DragMotion(st, root_x, root_y, iswev->motion.modifiers);
        *cont = FALSE;
        break;
    }

    case IswButtonUp: {
        if (!st->core.drag_started) {
            DragCleanup(st);
        } else if (st->drag_target_win != XCB_NONE && st->core.drag_target_accepted) {
            DragDrop(st);
        } else {
            DragCancel(st);
        }
        *cont = FALSE;
        break;
    }

    case IswKeyDown:
    case IswKeyUp: {
        uint32_t key = iswev->key.key;
        if (iswev->kind == IswKeyDown && key == IswKeyEscape) {
            DragCancel(st);
            *cont = FALSE;
        } else if (key == IswKeyShift || key == IswKeyControl) {
            /* Modifier changed — re-evaluate action and cursor */
            if (st->core.drag_started)
                DragMotion(st, st->core.drag_last_x, st->core.drag_last_y,
                           iswev->key.modifiers);
            *cont = FALSE;
        }
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Drag motion — track target windows                                 */
/* ------------------------------------------------------------------ */

static Boolean
WindowHasXdndAware(XdndState *st, xcb_connection_t *conn,
                   xcb_window_t win, int *version_out)
{
    xcb_get_property_cookie_t cookie =
        xcb_get_property(conn, False, win,
                         st->XdndAware, XCB_ATOM_ATOM, 0, 1);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(conn, cookie, NULL);

    if (reply && reply->type != XCB_ATOM_NONE &&
        xcb_get_property_value_length(reply) >= (int) sizeof(uint32_t)) {
        *version_out = (int) *(uint32_t *) xcb_get_property_value(reply);
        free(reply);
        return True;
    }
    free(reply);
    return False;
}

static xcb_window_t
FindXdndAwareWindow(XdndState *st, xcb_connection_t *conn,
                    xcb_window_t start, int *version_out)
{
    *version_out = 0;

    if (start == XCB_NONE || start == _IswXcbScreen(IswScreenOf(st->core.shell))->root)
        return XCB_NONE;

    /* Check the starting window (WM frame or unmanaged toplevel) */
    if (WindowHasXdndAware(st, conn, start, version_out))
        return start;

    /* The WM reparents the client inside a frame.  Walk down into
     * children to find the actual XdndAware client window.  Use
     * translate_coordinates to follow the pointer into subwindows. */
    xcb_window_t win = start;
    for (int depth = 0; depth < 8; depth++) {
        xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn, win);
        xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);
        if (!qpr)
            break;
        xcb_window_t child = qpr->child;
        free(qpr);

        if (child == XCB_NONE)
            break;

        if (WindowHasXdndAware(st, conn, child, version_out))
            return child;

        win = child;
    }

    return XCB_NONE;
}

static void
DragMotion(XdndState *st, int root_x, int root_y, unsigned int modifiers)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    MoveDragIcon(st, root_x, root_y);

    /* Find the window under the cursor.  The authoritative modifier mask
       comes from the pointer query; fall back to the event modifiers the
       caller passed if the query fails. */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         _IswXcbScreen(IswScreenOf(st->core.shell))->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);

    xcb_window_t child_win = XCB_NONE;
    if (qpr) {
        child_win = qpr->child;
        modifiers = qpr->mask;
        free(qpr);
    }

    /* Find the XdndAware ancestor */
    int target_version = 0;
    xcb_window_t target_win = XCB_NONE;
    if (child_win != XCB_NONE)
        target_win = FindXdndAwareWindow(st, conn, child_win, &target_version);

    /* Target window changed? */
    if (target_win != st->drag_target_win) {
        /* Leave old target */
        if (st->drag_target_win != XCB_NONE)
            SendDragLeave(st);

        st->drag_target_win = target_win;
        st->core.drag_target_ver = target_version;
        st->core.drag_target_accepted = False;
        st->core.drag_target_action = ISW_DND_ACTION_NONE;
        st->core.drag_status_pending = False;

        /* Enter new target */
        if (target_win != XCB_NONE)
            SendDragEnter(st, target_win);
    }

    /* Send position */
    if (st->drag_target_win != XCB_NONE) {
        /* Check if modifier keys changed the desired action */
        IswDndAction mod_action = _IswDndModifiersToAction(modifiers);
        (void) mod_action; /* used in SendDragPosition via drag_desc.actions */

        if (st->core.drag_status_pending) {
            /* Defer — will resend when XdndStatus arrives */
            st->core.drag_last_x = root_x;
            st->core.drag_last_y = root_y;
            st->core.drag_position_deferred = True;
        } else {
            SendDragPosition(st, root_x, root_y);
        }
    }

    /* Update cursor based on acceptance state */
    xcb_cursor_t cursor;
    if (st->drag_target_win == XCB_NONE) {
        cursor = st->cursor_default;
    } else if (!st->core.drag_target_accepted) {
        cursor = st->cursor_reject;
    } else {
        switch (st->core.drag_target_action) {
        case ISW_DND_ACTION_MOVE: cursor = st->cursor_move; break;
        case ISW_DND_ACTION_LINK: cursor = st->cursor_link; break;
        default:                  cursor = st->cursor_copy; break;
        }
    }
    _IswPlatformUpdatePointerCapture(IswDisplayOf(st->core.shell), cursor,
                                     ISW_CURRENT_TIME,
                                     IswButtonReleaseMask |
                                     IswPointerMotionMask |
                                     IswButtonMotionMask);
    _IswPlatformFlush(IswDisplayOf(st->core.shell));
}

/* ------------------------------------------------------------------ */
/* Send XDND messages to foreign target                               */
/* ------------------------------------------------------------------ */

static void
SendDragEnter(XdndState *st, xcb_window_t target)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = target;
    cm.type = st->XdndEnter;
    cm.format = 32;
    cm.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));
    cm.data.data32[1] = (XDND_VERSION << 24);

    if (st->core.drag_desc.num_types > 3) {
        cm.data.data32[1] |= 1;  /* use XdndTypeList property */
    }

    for (int i = 0; i < 3 && i < st->core.drag_desc.num_types; i++)
        cm.data.data32[2 + i] = XdndStringToAtom(st, st->core.drag_desc.types[i]);


    xcb_send_event(conn, False, target, 0, (const char *) &cm);
    xcb_flush(conn);
}

static void
SendDragPosition(XdndState *st, int root_x, int root_y)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    /* Determine action from keyboard modifiers */
    /* Query pointer for modifiers and physical root coordinates.
     * root_x/root_y from the motion event have been descaled to logical
     * pixels by the dispatcher — XdndPosition must use the X server's
     * native physical coordinates so external apps map them correctly. */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         _IswXcbScreen(IswScreenOf(st->core.shell))->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);
    unsigned int modifiers = 0;
    int phys_x = root_x, phys_y = root_y;
    if (qpr) {
        modifiers = qpr->mask;
        phys_x = qpr->root_x;
        phys_y = qpr->root_y;
        free(qpr);
    }

    IswDndAction desired = _IswDndModifiersToAction(modifiers);
    /* Constrain to offered actions */
    if (!(st->core.drag_desc.actions & desired))
        desired = ISW_DND_ACTION_COPY;  /* fallback */

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndPosition;
    cm.format = 32;
    cm.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));
    cm.data.data32[1] = 0;  /* reserved */
    cm.data.data32[2] = ((uint32_t) phys_x << 16) | ((uint32_t) phys_y & 0xFFFF);
    cm.data.data32[3] = st->drag_timestamp;
    cm.data.data32[4] = XdndActionToAtom(st, desired);


    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);

    st->core.drag_status_pending = True;
    st->core.drag_last_x = root_x;
    st->core.drag_last_y = root_y;
}

static void
SendDragLeave(XdndState *st)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndLeave;
    cm.format = 32;
    cm.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));

    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);
}

static void
SendDragDrop(XdndState *st)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndDrop;
    cm.format = 32;
    cm.data.data32[0] = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(st->core.shell)), (Widget)(st->core.shell)));
    cm.data.data32[1] = 0;  /* reserved */
    cm.data.data32[2] = st->drag_timestamp;

    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* XdndStatus handler (drag source receiving target's reply)          */
/* ------------------------------------------------------------------ */

static void
HandleDragStatus(XdndState *st, const uint32_t *data)
{
    st->core.drag_status_pending = False;
    st->core.drag_target_accepted = (data[1] & 1) != 0;
    st->core.drag_target_action = XdndAtomToAction(st, (xcb_atom_t) data[4]);

    /* If we deferred a position update, send it now */
    if (st->core.drag_position_deferred) {
        st->core.drag_position_deferred = False;
        SendDragPosition(st, st->core.drag_last_x, st->core.drag_last_y);
    }
}

/* ------------------------------------------------------------------ */
/* XdndFinished handler (drag source receiving completion)            */
/* ------------------------------------------------------------------ */

static void
HandleDragFinished(XdndState *st, const uint32_t *data)
{
    Boolean accepted = (data[1] & 1) != 0;
    IswDndAction performed = XdndAtomToAction(st, (xcb_atom_t) data[2]);

    if (st->core.finished_timer) {
        IswRemoveTimeOut(st->core.finished_timer);
        st->core.finished_timer = 0;
    }

    if (st->core.drag_desc.finished) {
        st->core.drag_desc.finished(st->core.drag_source, performed,
                               accepted, st->core.drag_desc.client_data);
    }

    DragCleanup(st);
}

static void
DragFinishedTimeout(IswPointer closure, IswIntervalId *id)
{
    XdndState *st = (XdndState *) closure;
    (void) id;

    st->core.finished_timer = 0;

    if (st->core.drag_desc.finished) {
        st->core.drag_desc.finished(st->core.drag_source, ISW_DND_ACTION_NONE,
                               False, st->core.drag_desc.client_data);
    }

    DragCleanup(st);
}

/* ------------------------------------------------------------------ */
/* Drag completion                                                    */
/* ------------------------------------------------------------------ */

static void
DragDrop(XdndState *st)
{
    SendDragDrop(st);

    /* Set timeout in case target doesn't reply */
    st->core.finished_timer = IswAppAddTimeOut(
        IswWidgetToApplicationContext(st->core.shell),
        FINISHED_TIMEOUT, DragFinishedTimeout, (IswPointer) st);

    /* Release capture and remove drag event handler so normal input
     * resumes immediately. XdndFinished arrives as a ClientMessage
     * through HandleXdndEvent (the non-maskable handler), not through
     * HandleDragEvent, so we don't need it anymore. Keep st->core.dragging
     * True so HandleXdndEvent still processes XdndFinished/XdndStatus. */
    IswUngrabPointer(st->core.shell, ISW_CURRENT_TIME);
    IswUngrabKeyboard(st->core.shell, ISW_CURRENT_TIME);
    DestroyDragIcon(st);

    IswRemoveEventHandler(st->core.shell,
                         XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                         XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                         XCB_EVENT_MASK_KEY_RELEASE,
                         TRUE, HandleDragEvent, (IswPointer) st);

    _IswPlatformFlush(IswDisplayOf(st->core.shell));
}

static void
DragCancel(XdndState *st)
{
    if (st->drag_target_win != XCB_NONE)
        SendDragLeave(st);

    if (st->core.drag_desc.finished) {
        st->core.drag_desc.finished(st->core.drag_source, ISW_DND_ACTION_NONE,
                               False, st->core.drag_desc.client_data);
    }

    DragCleanup(st);
}

static void
DragCleanup(XdndState *st)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));

    /* Release pointer and keyboard capture */
    IswUngrabPointer(st->core.shell, ISW_CURRENT_TIME);
    IswUngrabKeyboard(st->core.shell, ISW_CURRENT_TIME);

    /* Drag-source grab released; restore the windowless implicit grab. */
    {
        IswPerDisplayInput pdi = _IswGetPerDisplayInput(IswDisplayOf(st->core.shell));
        if (pdi)
            pdi->xdndDragActive = False;
    }

    /* Remove drag event handler */
    IswRemoveEventHandler(st->core.shell,
                         XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                         XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                         XCB_EVENT_MASK_KEY_RELEASE,
                         TRUE, HandleDragEvent, (IswPointer) st);

    /* Disown selection */
    IswDisownSelection(st->core.shell, st->XdndSelection, st->drag_timestamp);

    /* Clean up icon */
    DestroyDragIcon(st);
    if (st->drag_icon_owned && st->core.drag_desc.icon_pixmap != 0) {
        xcb_free_pixmap(conn, (xcb_pixmap_t) st->core.drag_desc.icon_pixmap);
        st->core.drag_desc.icon_pixmap = 0;
        st->drag_icon_owned = False;
    }
    if (st->drag_icon_cmap != XCB_NONE) {
        xcb_free_colormap(conn, st->drag_icon_cmap);
        st->drag_icon_cmap = XCB_NONE;
    }

    /* Remove timeout */
    if (st->core.finished_timer) {
        IswRemoveTimeOut(st->core.finished_timer);
        st->core.finished_timer = 0;
    }

    if (st->core.drag_desc.types) {
        for (int i = 0; i < st->core.drag_desc.num_types; i++)
            IswFree((char *) st->core.drag_desc.types[i]);
        IswFree((char *) st->core.drag_desc.types);
        st->core.drag_desc.types = NULL;
    }

    /* Reset state */
    st->core.dragging = False;
    st->core.drag_started = False;
    st->core.drag_source = NULL;
    st->drag_target_win = XCB_NONE;
    st->core.drag_status_pending = False;
    st->core.drag_position_deferred = False;

    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* Drag source selection convert proc                                 */
/* ------------------------------------------------------------------ */

static Boolean
DragConvertSelection(Widget w, xcb_atom_t *selection, xcb_atom_t *target,
                     xcb_atom_t *type_return, IswPointer *value_return,
                     unsigned long *length_return, int *format_return)
{
    XdndState *st = GetXdndState(w);
    if (!st || !st->core.dragging)
        return False;

    (void) selection;

    if (*target == st->targets_atom) {
        xcb_atom_t *targets = (xcb_atom_t *) IswMalloc(
            (st->core.drag_desc.num_types + 1) * sizeof(xcb_atom_t));
        targets[0] = st->targets_atom;
        for (int i = 0; i < st->core.drag_desc.num_types; i++)
            targets[i + 1] = XdndStringToAtom(st, st->core.drag_desc.types[i]);

        *type_return = XCB_ATOM_ATOM;
        *value_return = (IswPointer) targets;
        *length_return = st->core.drag_desc.num_types + 1;
        *format_return = 32;
        return True;
    }

    if (st->core.drag_desc.convert) {
        const char *target_str = XdndAtomToString(st, *target);
        Boolean ok = st->core.drag_desc.convert(st->core.drag_source, target_str,
                                           value_return, length_return,
                                           format_return,
                                           st->core.drag_desc.client_data);
        IswFree((char *) target_str);
        if (ok)
            *type_return = *target;

        return ok;
    }

    return False;
}

static void
DragLoseSelection(Widget w, xcb_atom_t *selection)
{
    /* Another app took our selection — unusual during drag but handle it */
    (void) w;
    (void) selection;
}

/* ------------------------------------------------------------------ */
/* Drag icon                                                          */
/* ------------------------------------------------------------------ */

static void
CreateDragIconFromRaster(XdndState *st, const unsigned char *rgba,
                         unsigned int w, unsigned int h)
{
    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    xcb_screen_t *screen = _IswXcbScreen(IswScreenOf(st->core.shell));

    /* Find a 32-bit visual for alpha transparency */
    xcb_visualtype_t *visual32 = _IswXcbFindVisual(screen, 32);
    if (!visual32)
        return;

    /* Create pixmap at depth 32 so alpha is preserved */
    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, 32, pixmap, screen->root, w, h);

    /* Convert RGBA to premultiplied ARGB32 for Cairo */
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, (int)w);
    unsigned char *argb = (unsigned char *)malloc((size_t)stride * h);
    if (!argb) {
        xcb_free_pixmap(conn, pixmap);
        return;
    }

    for (unsigned int i = 0; i < w * h; i++) {
        unsigned char r = rgba[i * 4 + 0];
        unsigned char g = rgba[i * 4 + 1];
        unsigned char b = rgba[i * 4 + 2];
        unsigned char a = rgba[i * 4 + 3];
        unsigned int row = i / w;
        unsigned int col = i % w;
        uint32_t *pixel = (uint32_t *)(argb + row * stride + col * 4);
        if (a == 255)
            *pixel = (255u << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        else if (a == 0)
            *pixel = 0;
        else
            *pixel = ((uint32_t)a << 24) |
                     ((uint32_t)((r * a + 127) / 255) << 16) |
                     ((uint32_t)((g * a + 127) / 255) << 8) |
                     (uint32_t)((b * a + 127) / 255);
    }

    /* Paint onto the 32-bit pixmap via Cairo */
    cairo_surface_t *target = cairo_xcb_surface_create(
        conn, pixmap, visual32, w, h);
    cairo_surface_t *source = cairo_image_surface_create_for_data(
        argb, CAIRO_FORMAT_ARGB32, (int)w, (int)h, stride);
    cairo_t *cr = cairo_create(target);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, source, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(source);
    cairo_surface_flush(target);
    cairo_surface_destroy(target);
    free(argb);

    /* Create a colormap for the 32-bit visual */
    xcb_colormap_t cmap = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE,
                        cmap, screen->root, visual32->visual_id);

    st->core.drag_desc.icon_pixmap = pixmap;
    st->core.drag_desc.icon_width = (int)w;
    st->core.drag_desc.icon_height = (int)h;
    st->core.drag_desc.icon_hotspot_x = (int)w / 2;
    st->core.drag_desc.icon_hotspot_y = (int)h / 2;
    st->drag_icon_owned = True;
    st->drag_icon_cmap = cmap;
    st->drag_icon_visual = visual32->visual_id;
}

static void
CreateDragIcon(XdndState *st)
{
    /* Auto-generate icon from IconView item raster */
    if (st->core.drag_desc.icon_pixmap == 0 &&
        IswIsSubclass(st->core.drag_source, iconViewWidgetClass)) {
        int idx = IswIconViewHitTest(st->core.drag_source,
                                     st->core.drag_press_x, st->core.drag_press_y);
        if (idx >= 0) {
            unsigned int rw, rh;
            const unsigned char *raster =
                IswIconViewGetItemRaster(st->core.drag_source, idx, &rw, &rh);
            if (raster)
                CreateDragIconFromRaster(st, raster, rw, rh);
        }
    }

    if (st->core.drag_desc.icon_pixmap == 0)
        return;

    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    xcb_screen_t *screen = _IswXcbScreen(IswScreenOf(st->core.shell));

    st->drag_icon_win = xcb_generate_id(conn);

    if (st->drag_icon_owned && st->drag_icon_visual) {
        /* 32-bit ARGB window for transparent drag icon */
        uint32_t vals[4];
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL |
                        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP;
        vals[0] = (uint32_t) st->core.drag_desc.icon_pixmap;
        vals[1] = 0;
        vals[2] = True;
        vals[3] = st->drag_icon_cmap;

        xcb_create_window(conn, 32,
                          st->drag_icon_win, screen->root,
                          (int16_t)(st->core.drag_start_x - st->core.drag_desc.icon_hotspot_x),
                          (int16_t)(st->core.drag_start_y - st->core.drag_desc.icon_hotspot_y),
                          st->core.drag_desc.icon_width,
                          st->core.drag_desc.icon_height,
                          0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          st->drag_icon_visual, mask, vals);
    } else {
        uint32_t vals[2];
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_OVERRIDE_REDIRECT;
        vals[0] = (uint32_t) st->core.drag_desc.icon_pixmap;
        vals[1] = True;

        xcb_create_window(conn, XCB_COPY_FROM_PARENT,
                          st->drag_icon_win, screen->root,
                          (int16_t)(st->core.drag_start_x - st->core.drag_desc.icon_hotspot_x),
                          (int16_t)(st->core.drag_start_y - st->core.drag_desc.icon_hotspot_y),
                          st->core.drag_desc.icon_width,
                          st->core.drag_desc.icon_height,
                          0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_COPY_FROM_PARENT, mask, vals);
    }

    /* _NET_WM_WINDOW_TYPE */
    {
        xcb_intern_atom_cookie_t wt_cookie = xcb_intern_atom(conn, FALSE, 19, "_NET_WM_WINDOW_TYPE");
        xcb_intern_atom_cookie_t type_cookie = xcb_intern_atom(conn, FALSE, 22, "_NET_WM_WINDOW_TYPE_DND");
        xcb_intern_atom_reply_t *wt_reply = xcb_intern_atom_reply(conn, wt_cookie, NULL);
        xcb_intern_atom_reply_t *type_reply = xcb_intern_atom_reply(conn, type_cookie, NULL);
        if (wt_reply && type_reply) {
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, st->drag_icon_win,
                                wt_reply->atom, XCB_ATOM_ATOM, 32,
                                1, &type_reply->atom);
        }
        free(wt_reply);
        free(type_reply);
    }

    xcb_map_window(conn, st->drag_icon_win);
    xcb_flush(conn);
}

static void
MoveDragIcon(XdndState *st, int root_x, int root_y)
{
    if (st->drag_icon_win == XCB_NONE)
        return;

    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    /* HiDPI: scale logical to physical for the X server */
    double _sf = _IswGetScaleFactor(IswDisplayOf(st->core.shell));
    uint32_t values[2];
    values[0] = (uint32_t)(int32_t)((root_x - st->core.drag_desc.icon_hotspot_x) * _sf + 0.5);
    values[1] = (uint32_t)(int32_t)((root_y - st->core.drag_desc.icon_hotspot_y) * _sf + 0.5);

    xcb_configure_window(conn, st->drag_icon_win,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                         values);
    xcb_flush(conn);
}

static void
DestroyDragIcon(XdndState *st)
{
    if (st->drag_icon_win == XCB_NONE)
        return;

    xcb_connection_t *conn = _IswXcbConn(IswDisplayOf(st->core.shell));
    xcb_destroy_window(conn, st->drag_icon_win);
    st->drag_icon_win = XCB_NONE;
    xcb_flush(conn);
}

/* ================================================================== */
/*                                                                    */
/* Backend vtable                                                     */
/*                                                                    */
/* ================================================================== */

const IswPlatformDndOps isw_platform_xcb_dnd_ops = {
    .enable                  = xcb_dnd_enable,
    .widget_accept_drops     = xcb_dnd_widget_accept_drops,
    .start_drag              = xcb_dnd_start_drag,
    .set_accepted_types      = xcb_dnd_set_accepted_types,
    .set_accepted_actions    = xcb_dnd_set_accepted_actions,
    .set_drop_callback       = xcb_dnd_set_drop_callback,
    .set_drag_motion_callback = xcb_dnd_set_drag_motion_callback,
    .set_drag_leave_callback = xcb_dnd_set_drag_leave_callback,
    .is_dragging             = xcb_dnd_is_dragging,
};

/* ================================================================== */
/*                                                                    */
/* GENERIC SERVICE — transport-neutral IswDnd* public entry points    */
/*                                                                    */
/* Thin dispatchers over the platform DnD ops (via the _IswPlatformDnd* */
/* wrappers).  Application/widget code calls these; the active backend  */
/* supplies the implementation.                                         */
/* ================================================================== */

void
IswDndEnable(Widget shell)
{
    _IswPlatformDndEnable(shell);
}

void
IswDndWidgetAcceptDrops(Widget w)
{
    _IswPlatformDndWidgetAcceptDrops(w);
}

void
IswDndStartDrag(Widget source_widget, IswEvent *trigger_event,
                IswDragSourceDesc *desc)
{
    _IswPlatformDndStartDrag(source_widget, trigger_event, desc);
}

void
IswDndSetAcceptedTypes(Widget w, const char **types, int num_types)
{
    _IswPlatformDndSetAcceptedTypes(w, types, num_types);
}

void
IswDndSetAcceptedActions(Widget w, IswDndAction actions)
{
    _IswPlatformDndSetAcceptedActions(w, actions);
}

void
IswDndSetDropCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    _IswPlatformDndSetDropCallback(w, proc, closure);
}

void
IswDndSetDragMotionCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    _IswPlatformDndSetDragMotionCallback(w, proc, closure);
}

void
IswDndSetDragLeaveCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    _IswPlatformDndSetDragLeaveCallback(w, proc, closure);
}

Boolean
IswDndIsDragging(Widget w)
{
    return _IswPlatformDndIsDragging(w);
}
