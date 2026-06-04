/*
 * ISWXdnd.c - XDND version 5 drag-and-drop implementation
 *
 * Full drag source and drop target support implementing the XDND v5
 * protocol. Interoperates with any XDND-aware application (GTK, Qt,
 * Firefox, etc.).
 *
 * Drop target: shell windows advertise XdndAware and handle the
 * protocol messages, routing drop data to the widget under the cursor.
 * Widgets register callbacks for drop, dragEnter, dragMotion, dragLeave.
 *
 * Drag source: widgets initiate drags via ISWXdndStartDrag. The library
 * grabs the pointer, tracks motion, sends XDND protocol messages to
 * foreign windows, and owns XdndSelection to provide data.
 *
 * Pure XCB — no Xlib dependencies.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/IntrinsicI.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWXdnd.h>
#include <ISW/ISWContext.h>
#include <ISW/IconView.h>
#include <ISW/ViewportP.h>
#include <ISW/ISWRender.h>
#include "ISWRenderPrivate.h"
#include "ISWXcbDraw.h"
#include <xcb/xcb_cursor.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

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

/* ------------------------------------------------------------------ */
/* Per-widget drop configuration                                      */
/* ------------------------------------------------------------------ */

typedef struct _DropConfig {
    Widget              widget;
    xcb_atom_t         *accepted_types;
    int                 num_accepted_types;
    IswDndAction        accepted_actions;
    /* Direct callback — used when the widget class doesn't declare
     * IswNdropCallback as a resource (e.g. widgets inheriting from
     * Core/Composite rather than Simple). */
    IswCallbackProc      drop_proc;
    IswPointer           drop_closure;
    IswCallbackProc      motion_proc;
    IswPointer           motion_closure;
    IswCallbackProc      leave_proc;
    IswPointer           leave_closure;
    struct _DropConfig *next;
} DropConfig;

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

    Widget shell;

    /* --- Drop target state --- */
    xcb_window_t    src_window;         /* source window of incoming drag */
    xcb_atom_t     *src_types;          /* types offered by source */
    int             src_num_types;
    IswDndAction    src_actions;        /* actions offered by source */
    int             src_version;        /* source XDND version */
    int             drop_x, drop_y;     /* last position (root coords) */
    Widget          hover_widget;       /* widget currently under cursor */
    xcb_atom_t      negotiated_type;    /* type accepted for current drop */
    IswDndAction    negotiated_action;  /* action accepted for current drop */
    xcb_timestamp_t drop_timestamp;     /* timestamp from XdndDrop */

    /* --- Drag source state --- */
    Boolean             dragging;
    IswDragSourceDesc   drag_desc;
    Widget              drag_source;
    xcb_timestamp_t     drag_timestamp;
    int                 drag_start_x;   /* root coords of initial press */
    int                 drag_start_y;
    int                 drag_press_x;   /* widget-local press coords */
    int                 drag_press_y;
    Boolean             drag_started;   /* past threshold? */

    /* Target tracking during drag */
    xcb_window_t        drag_target_win;    /* foreign window under cursor */
    int                 drag_target_ver;    /* its XDND version */
    Boolean             drag_status_pending;/* waiting for XdndStatus */
    Boolean             drag_target_accepted;
    IswDndAction        drag_target_action;
    int                 drag_last_x;        /* last sent position */
    int                 drag_last_y;
    Boolean             drag_position_deferred;/* moved while status pending */

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

    /* Finished timeout */
    IswIntervalId        finished_timer;

    /* Per-widget drop configs */
    DropConfig         *drop_configs;
} XdndState;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void InternAtoms(XdndState *st, xcb_connection_t *conn);
static void CreateCursors(XdndState *st, xcb_connection_t *conn,
                          xcb_screen_t *screen);

/* Drop target handlers */
static void HandleXdndEvent(Widget w, IswPointer closure,
                            xcb_generic_event_t *event, Boolean *cont);
static void HandleTargetEnter(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetPosition(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetDrop(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetLeave(XdndState *st);
static void TargetSelectionCallback(Widget w, IswPointer closure,
                                    xcb_atom_t *selection, xcb_atom_t *type,
                                    IswPointer value, unsigned long *length,
                                    int *format);
static void SendXdndStatus(XdndState *st, Boolean accept,
                           xcb_atom_t action_atom);
static void SendXdndFinished(XdndState *st, Boolean accept,
                             xcb_atom_t action_atom);

/* Drop target type negotiation */
static DropConfig *FindDropConfig(XdndState *st, Widget w);
static Boolean NegotiateType(XdndState *st, Widget target,
                             xcb_atom_t *type_out, IswDndAction *action_out);
static IswDndAction AtomToAction(XdndState *st, xcb_atom_t atom);
static xcb_atom_t ActionToAtom(XdndState *st, IswDndAction action);

/* Widget tree walk */
static Widget FindDropTarget(XdndState *st, int root_x, int root_y);
static Widget FindDropChild(XdndState *st, Widget composite, int wx, int wy);

/* Drag source */
static void HandleDragEvent(Widget w, IswPointer closure,
                            xcb_generic_event_t *event, Boolean *cont);
static void DragMotion(XdndState *st, int root_x, int root_y);
static void DragDrop(XdndState *st);
static void DragCancel(XdndState *st);
static void DragCleanup(XdndState *st);
static xcb_window_t FindXdndAwareWindow(XdndState *st, xcb_connection_t *conn,
                                        xcb_window_t child, int *version_out);
static void SendDragEnter(XdndState *st, xcb_window_t target);
static void SendDragPosition(XdndState *st, int root_x, int root_y);
static void SendDragLeave(XdndState *st);
static void SendDragDrop(XdndState *st);
static void HandleDragStatus(XdndState *st, xcb_client_message_event_t *cm);
static void HandleDragFinished(XdndState *st, xcb_client_message_event_t *cm);
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

/* URI parsing */
static char **ParseUriList(const char *data, int len, int *out_count);

/* Keyboard modifier → action mapping */
static IswDndAction ModifiersToAction(XdndState *st, unsigned int state);

/* ------------------------------------------------------------------ */
/* Atom internment                                                    */
/* ------------------------------------------------------------------ */

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
/* Action ↔ atom conversion                                           */
/* ------------------------------------------------------------------ */

static IswDndAction
AtomToAction(XdndState *st, xcb_atom_t atom)
{
    if (atom == st->action_copy)    return ISW_DND_ACTION_COPY;
    if (atom == st->action_move)    return ISW_DND_ACTION_MOVE;
    if (atom == st->action_link)    return ISW_DND_ACTION_LINK;
    if (atom == st->action_ask)     return ISW_DND_ACTION_ASK;
    if (atom == st->action_private) return ISW_DND_ACTION_PRIVATE;
    return ISW_DND_ACTION_NONE;
}

static xcb_atom_t
ActionToAtom(XdndState *st, IswDndAction action)
{
    if (action & ISW_DND_ACTION_COPY)    return st->action_copy;
    if (action & ISW_DND_ACTION_MOVE)    return st->action_move;
    if (action & ISW_DND_ACTION_LINK)    return st->action_link;
    if (action & ISW_DND_ACTION_ASK)     return st->action_ask;
    if (action & ISW_DND_ACTION_PRIVATE) return st->action_private;
    return XCB_ATOM_NONE;
}

static IswDndAction
ModifiersToAction(XdndState *st, unsigned int state)
{
    /* Shift = Move, Ctrl = Copy, Ctrl+Shift = Link */
    Boolean shift = (state & XCB_MOD_MASK_SHIFT) != 0;
    Boolean ctrl  = (state & XCB_MOD_MASK_CONTROL) != 0;

    (void) st;

    if (ctrl && shift) return ISW_DND_ACTION_LINK;
    if (ctrl)          return ISW_DND_ACTION_COPY;
    if (shift)         return ISW_DND_ACTION_MOVE;
    return ISW_DND_ACTION_COPY; /* default */
}

/* ------------------------------------------------------------------ */
/* Per-widget drop config management                                  */
/* ------------------------------------------------------------------ */

static DropConfig *
FindDropConfig(XdndState *st, Widget w)
{
    DropConfig *dc;
    for (dc = st->drop_configs; dc; dc = dc->next) {
        if (dc->widget == w)
            return dc;
    }
    return NULL;
}

static DropConfig *
GetOrCreateDropConfig(XdndState *st, Widget w)
{
    DropConfig *dc = FindDropConfig(st, w);
    if (!dc) {
        dc = (DropConfig *) IswCalloc(1, sizeof(DropConfig));
        dc->widget = w;
        dc->next = st->drop_configs;
        st->drop_configs = dc;
    }
    return dc;
}

/* ------------------------------------------------------------------ */
/* Type/action negotiation                                            */
/* ------------------------------------------------------------------ */

static Boolean
NegotiateType(XdndState *st, Widget target,
              xcb_atom_t *type_out, IswDndAction *action_out)
{
    DropConfig *dc = FindDropConfig(st, target);

    /* Find best matching type */
    xcb_atom_t best_type = XCB_ATOM_NONE;

    if (dc && dc->accepted_types && dc->num_accepted_types > 0) {
        /* Intersect source types with target's accepted types */
        for (int i = 0; i < dc->num_accepted_types && best_type == XCB_ATOM_NONE; i++) {
            for (int j = 0; j < st->src_num_types; j++) {
                if (dc->accepted_types[i] == st->src_types[j]) {
                    best_type = dc->accepted_types[i];
                    break;
                }
            }
        }
    } else {
        /* No type filter — accept first offered type */
        if (st->src_num_types > 0)
            best_type = st->src_types[0];
    }

    if (best_type == XCB_ATOM_NONE)
        return False;

    /* Find best matching action */
    IswDndAction target_actions = (dc && dc->accepted_actions)
                                  ? dc->accepted_actions
                                  : (ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE |
                                     ISW_DND_ACTION_LINK);
    IswDndAction common = st->src_actions & target_actions;

    if (common == ISW_DND_ACTION_NONE)
        return False;

    /* Prefer copy > move > link > ask > private */
    IswDndAction best_action = ISW_DND_ACTION_NONE;
    if (common & ISW_DND_ACTION_COPY)         best_action = ISW_DND_ACTION_COPY;
    else if (common & ISW_DND_ACTION_MOVE)    best_action = ISW_DND_ACTION_MOVE;
    else if (common & ISW_DND_ACTION_LINK)    best_action = ISW_DND_ACTION_LINK;
    else if (common & ISW_DND_ACTION_ASK)     best_action = ISW_DND_ACTION_ASK;
    else if (common & ISW_DND_ACTION_PRIVATE) best_action = ISW_DND_ACTION_PRIVATE;

    *type_out = best_type;
    *action_out = best_action;
    return True;
}

/* ------------------------------------------------------------------ */
/* Widget tree walk to find drop target                               */
/* ------------------------------------------------------------------ */

static Widget
FindDropChild(XdndState *st, Widget composite, int wx, int wy)
{
    if (!IswIsComposite(composite))
        return NULL;

    CompositeWidget cw = (CompositeWidget) composite;
    for (int i = cw->composite.num_children - 1; i >= 0; i--) {
        Widget child = cw->composite.children[i];
        if (!IswIsManaged(child) || !IswIsRealized(child))
            continue;

        int cx = child->core.x;
        int cy = child->core.y;
        int cw2 = child->core.width;
        int ch = child->core.height;

        if (wx >= cx && wx < cx + cw2 && wy >= cy && wy < cy + ch) {
            /* Check DropConfig first (works for any widget class),
             * then fall back to IswHasCallbacks (for widgets that
             * declare IswNdropCallback as a resource). */
            if (FindDropConfig(st, child) ||
                IswHasCallbacks(child, IswNdropCallback) == IswCallbackHasSome)
                return child;

            if (IswIsComposite(child)) {
                Widget deeper = FindDropChild(st, child, wx - cx, wy - cy);
                if (deeper)
                    return deeper;
            }
        }
    }
    return NULL;
}

static Widget
FindDropTarget(XdndState *st, int root_x, int root_y)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    xcb_translate_coordinates_cookie_t cookie =
        xcb_translate_coordinates(conn,
            IswScreen(st->shell)->root, IswWindow(st->shell),
            (int16_t) root_x, (int16_t) root_y);
    xcb_translate_coordinates_reply_t *reply =
        xcb_translate_coordinates_reply(conn, cookie, NULL);

    if (!reply) {
        return NULL;
    }

    double sf = ISWScaleFactor(st->shell);
    int wx = (int)(reply->dst_x / sf + 0.5);
    int wy = (int)(reply->dst_y / sf + 0.5);
    free(reply);

    if (FindDropConfig(st, st->shell)) {
        return st->shell;
    }
    if (IswHasCallbacks(st->shell, IswNdropCallback) == IswCallbackHasSome) {
        return st->shell;
    }

    Widget result = FindDropChild(st, st->shell, wx, wy);

    /* Fallback: iterate registered DropConfigs directly.  This handles
       widgets inside Viewports whose clip windows hide them from the
       normal widget-tree walk. */
    if (!result) {
        DropConfig *dc;
        for (dc = st->drop_configs; dc; dc = dc->next) {
            if (dc->widget == st->shell || !IswIsRealized(dc->widget))
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
                    IswWindow(bounds_widget), IswScreen(st->shell)->root,
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

/* ------------------------------------------------------------------ */
/* URI list parser                                                    */
/* ------------------------------------------------------------------ */

static char **
ParseUriList(const char *data, int len, int *out_count)
{
    char **uris = NULL;
    int count = 0;
    int capacity = 0;
    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        int line_len = eol - p;
        if (line_len > 0 && *p != '#') {
            const char *uri = p;
            int uri_len = line_len;
            if (uri_len >= 7 && strncmp(uri, "file://", 7) == 0) {
                uri += 7;
                uri_len -= 7;
            }

            if (count >= capacity) {
                capacity = capacity ? capacity * 2 : 8;
                uris = (char **) IswRealloc((char *) uris,
                                           (capacity + 1) * sizeof(char *));
            }
            char *entry = IswMalloc(uri_len + 1);
            memcpy(entry, uri, uri_len);
            entry[uri_len] = '\0';
            uris[count++] = entry;
        }

        p = eol;
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }

    if (uris)
        uris[count] = NULL;

    *out_count = count;
    return uris;
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
    if (IswFindContext(IswDisplay(shell), IswWindow(shell),
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

/* ------------------------------------------------------------------ */

void
ISWXdndEnable(Widget shell)
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

    conn = IswDisplay(shell);
    st = (XdndState *) IswCalloc(1, sizeof(XdndState));
    st->shell = shell;
    InternAtoms(st, conn);
    CreateCursors(st, conn, IswScreen(shell));

    /* Store state on the shell window */
    IswSaveContext(IswDisplay(shell), IswWindow(shell),
                   xdnd_context, (void *) st);

    /* Advertise XdndAware */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, IswWindow(shell),
                        st->XdndAware, XCB_ATOM_ATOM, 32, 1, &version);

    /* Register non-maskable event handler for ClientMessage and SelectionNotify */
    IswAddEventHandler(shell, (EventMask) 0, TRUE,
                      HandleXdndEvent, (IswPointer) st);

    xcb_flush(conn);
}

void
ISWXdndWidgetAcceptDrops(Widget w)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;
    /* Ensure a DropConfig exists so the widget is registered */
    GetOrCreateDropConfig(st, w);
}

void
ISWXdndSetAcceptedTypes(Widget w, xcb_atom_t *types, int num_types)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = GetOrCreateDropConfig(st, w);

    if (dc->accepted_types)
        IswFree((char *) dc->accepted_types);

    if (types && num_types > 0) {
        dc->accepted_types = (xcb_atom_t *) IswMalloc(num_types * sizeof(xcb_atom_t));
        memcpy(dc->accepted_types, types, num_types * sizeof(xcb_atom_t));
        dc->num_accepted_types = num_types;
    } else {
        dc->accepted_types = NULL;
        dc->num_accepted_types = 0;
    }
}

void
ISWXdndSetAcceptedActions(Widget w, IswDndAction actions)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = GetOrCreateDropConfig(st, w);
    dc->accepted_actions = actions;
}

void
ISWXdndSetDropCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = GetOrCreateDropConfig(st, w);
    dc->drop_proc = proc;
    dc->drop_closure = closure;
}

void
ISWXdndSetDragMotionCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = GetOrCreateDropConfig(st, w);
    dc->motion_proc = proc;
    dc->motion_closure = closure;
}

void
ISWXdndSetDragLeaveCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    XdndState *st = GetXdndStateForWidget(w);
    if (!st)
        return;

    DropConfig *dc = GetOrCreateDropConfig(st, w);
    dc->leave_proc = proc;
    dc->leave_closure = closure;
}

xcb_atom_t
ISWXdndInternType(Widget w, const char *mime_type)
{
    return IswXcbInternAtom(IswDisplay(w), mime_type, False);
}

Boolean
ISWXdndIsDragging(Widget w)
{
    XdndState *st = GetXdndStateForWidget(w);
    return st && st->dragging;
}

/* ================================================================== */
/*                                                                    */
/* DROP TARGET — incoming drag handling                                */
/*                                                                    */
/* ================================================================== */

static void
HandleXdndEvent(Widget w, IswPointer closure, xcb_generic_event_t *event,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;
    uint8_t type = event->response_type & ~0x80;

    if (type == XCB_CLIENT_MESSAGE) {
        xcb_client_message_event_t *cm = (xcb_client_message_event_t *) event;

        /* Drop target messages (we are the target) */
        if (cm->type == st->XdndEnter) {
            HandleTargetEnter(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndPosition) {
            HandleTargetPosition(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndDrop) {
            HandleTargetDrop(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndLeave) {
            HandleTargetLeave(st);
            *cont = FALSE;
        }

        /* Drag source messages (we are the source) */
        if (st->dragging) {
            if (cm->type == st->XdndStatus) {
                HandleDragStatus(st, cm);
                *cont = FALSE;
            } else if (cm->type == st->XdndFinished) {
                HandleDragFinished(st, cm);
                *cont = FALSE;
            }
        }
    }

    (void) w;
}

/* ------------------------------------------------------------------ */
/* XdndEnter — source entered our window                              */
/* ------------------------------------------------------------------ */

static void
HandleTargetEnter(XdndState *st, xcb_client_message_event_t *cm)
{
    st->src_window = cm->data.data32[0];
    st->src_version = (cm->data.data32[1] >> 24) & 0xFF;

    if (st->src_version > XDND_VERSION)
        return;

    /* Free previous type list */
    if (st->src_types) {
        IswFree((char *) st->src_types);
        st->src_types = NULL;
        st->src_num_types = 0;
    }

    Boolean use_type_list = (cm->data.data32[1] & 1);

    if (use_type_list) {
        /* More than 3 types — read XdndTypeList property */
        xcb_connection_t *conn = IswDisplay(st->shell);
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->src_window,
                             st->XdndTypeList, XCB_ATOM_ATOM, 0, 256);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);
        if (reply) {
            xcb_atom_t *atoms = (xcb_atom_t *) xcb_get_property_value(reply);
            int count = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
            if (count > 0) {
                st->src_types = (xcb_atom_t *) IswMalloc(count * sizeof(xcb_atom_t));
                memcpy(st->src_types, atoms, count * sizeof(xcb_atom_t));
                st->src_num_types = count;
            }
            free(reply);
        }
    } else {
        /* Up to 3 types in data32[2..4] */
        int count = 0;
        xcb_atom_t types[3];
        for (int i = 2; i <= 4; i++) {
            if (cm->data.data32[i] != XCB_ATOM_NONE)
                types[count++] = cm->data.data32[i];
        }
        if (count > 0) {
            st->src_types = (xcb_atom_t *) IswMalloc(count * sizeof(xcb_atom_t));
            memcpy(st->src_types, types, count * sizeof(xcb_atom_t));
            st->src_num_types = count;
        }
    }

    /* Default: assume copy is offered (many sources don't advertise actions) */
    st->src_actions = ISW_DND_ACTION_COPY;

    st->hover_widget = NULL;
    st->negotiated_type = XCB_ATOM_NONE;
    st->negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndPosition — source moved over our window                        */
/* ------------------------------------------------------------------ */

static void
HandleTargetPosition(XdndState *st, xcb_client_message_event_t *cm)
{
    st->drop_x = (int)(cm->data.data32[2] >> 16);
    st->drop_y = (int)(cm->data.data32[2] & 0xFFFF);
    /* Extract proposed action from source */
    xcb_atom_t proposed_atom = cm->data.data32[4];
    IswDndAction proposed = AtomToAction(st, proposed_atom);
    if (proposed == ISW_DND_ACTION_NONE)
        proposed = ISW_DND_ACTION_COPY;

    /* Find the widget under the cursor */
    Widget target = FindDropTarget(st, st->drop_x, st->drop_y);

    /* Handle enter/leave transitions */
    if (target != st->hover_widget) {
        /* Leave old widget */
        if (st->hover_widget &&
            IswHasCallbacks(st->hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            IswCallCallbacks(st->hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
        } else if (st->hover_widget) {
            DropConfig *dc = FindDropConfig(st, st->hover_widget);
            if (dc && dc->leave_proc) {
                IswDragOverCallbackData cbd = {0};
                dc->leave_proc(st->hover_widget, dc->leave_closure, (IswPointer) &cbd);
            }
        }

        st->hover_widget = target;

        /* Enter new widget */
        if (target &&
            IswHasCallbacks(target, IswNdragEnterCallback) == IswCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            cbd.offered_types = st->src_types;
            cbd.num_offered_types = st->src_num_types;
            cbd.offered_actions = st->src_actions;
            cbd.proposed_action = proposed;
            IswCallCallbacks(target, IswNdragEnterCallback, (IswPointer) &cbd);
        }
    }

    /* Negotiate type/action */
    Boolean accept = False;
    xcb_atom_t accepted_type = XCB_ATOM_NONE;
    IswDndAction accepted_action = ISW_DND_ACTION_NONE;

    if (target) {
        /* First, let the widget's dragMotion callback override */
        if (IswHasCallbacks(target, IswNdragMotionCallback) == IswCallbackHasSome) {
            xcb_connection_t *conn = IswDisplay(st->shell);
            double sf = ISWScaleFactor(st->shell);
            xcb_translate_coordinates_cookie_t tc =
                xcb_translate_coordinates(conn,
                    IswScreen(st->shell)->root, IswWindow(target),
                    (int16_t) st->drop_x, (int16_t) st->drop_y);
            xcb_translate_coordinates_reply_t *tr =
                xcb_translate_coordinates_reply(conn, tc, NULL);

            IswDragOverCallbackData cbd = {0};
            cbd.x = tr ? (int)(tr->dst_x / sf + 0.5) : 0;
            cbd.y = tr ? (int)(tr->dst_y / sf + 0.5) : 0;
            free(tr);
            cbd.offered_types = st->src_types;
            cbd.num_offered_types = st->src_num_types;
            cbd.offered_actions = st->src_actions;
            cbd.proposed_action = proposed;

            IswCallCallbacks(target, IswNdragMotionCallback, (IswPointer) &cbd);

            if (cbd.accepted_type != XCB_ATOM_NONE) {
                accepted_type = cbd.accepted_type;
                accepted_action = cbd.accepted_action;
                accept = True;
            }
        } else {
            DropConfig *dc = FindDropConfig(st, target);
            if (dc && dc->motion_proc) {
                xcb_connection_t *conn = IswDisplay(st->shell);
                double sf = ISWScaleFactor(st->shell);
                xcb_translate_coordinates_cookie_t tc =
                    xcb_translate_coordinates(conn,
                        IswScreen(st->shell)->root, IswWindow(target),
                        (int16_t) st->drop_x, (int16_t) st->drop_y);
                xcb_translate_coordinates_reply_t *tr =
                    xcb_translate_coordinates_reply(conn, tc, NULL);

                IswDragOverCallbackData cbd = {0};
                cbd.x = tr ? (int)(tr->dst_x / sf + 0.5) : 0;
                cbd.y = tr ? (int)(tr->dst_y / sf + 0.5) : 0;
                free(tr);
                cbd.offered_types = st->src_types;
                cbd.num_offered_types = st->src_num_types;
                cbd.offered_actions = st->src_actions;
                cbd.proposed_action = proposed;

                dc->motion_proc(target, dc->motion_closure, (IswPointer) &cbd);

                if (cbd.accepted_type != XCB_ATOM_NONE) {
                    accepted_type = cbd.accepted_type;
                    accepted_action = cbd.accepted_action;
                    accept = True;
                }
            }
        }

        /* If callback didn't accept, try automatic negotiation */
        if (!accept) {
            accept = NegotiateType(st, target, &accepted_type, &accepted_action);
        }
    }

    st->negotiated_type = accepted_type;
    st->negotiated_action = accepted_action;

    SendXdndStatus(st, accept,
                   accept ? ActionToAtom(st, accepted_action) : XCB_ATOM_NONE);
}

/* ------------------------------------------------------------------ */
/* XdndDrop — source released over our window                         */
/* ------------------------------------------------------------------ */

static void
HandleTargetDrop(XdndState *st, xcb_client_message_event_t *cm)
{
    st->drop_timestamp = cm->data.data32[2];
    if (st->negotiated_type == XCB_ATOM_NONE || !st->hover_widget) {
        SendXdndFinished(st, False, XCB_ATOM_NONE);
        HandleTargetLeave(st);
        return;
    }

    /* Request the data via Xt selection mechanism — gets INCR for free */
    IswGetSelectionValue(st->shell, st->XdndSelection,
                        st->negotiated_type,
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
        /* Selection transfer failed — read the data directly from the
         * source window property (set eagerly by ISWXdndStartDrag).
         * This bypasses the Xt selection mechanism which is unreliable
         * for cross-client transfers in XCB-based Xt. */
        xcb_connection_t *conn = IswDisplay(st->shell);
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->src_window,
                             st->negotiated_type, XCB_ATOM_ANY, 0, 65536);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);

        if (reply) {
            char *data = (char *) xcb_get_property_value(reply);
            int data_len = xcb_get_property_value_length(reply);

            if (data && data_len > 0) {
                /* Make a copy since we need it after freeing the reply */
                char *data_copy = IswMalloc(data_len);
                memcpy(data_copy, data, data_len);

                unsigned long len = data_len;
                int fmt = 8;
                xcb_atom_t tp = st->negotiated_type;
                /* Recurse with the data we got */
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

    Widget target = st->hover_widget;
    if (!target)
        target = FindDropTarget(st, st->drop_x, st->drop_y);

    if (target) {
        xcb_connection_t *conn = IswDisplay(st->shell);
        xcb_translate_coordinates_cookie_t tc =
            xcb_translate_coordinates(conn,
                IswScreen(st->shell)->root, IswWindow(target),
                (int16_t) st->drop_x, (int16_t) st->drop_y);
        xcb_translate_coordinates_reply_t *tr =
            xcb_translate_coordinates_reply(conn, tc, NULL);

        IswDropCallbackData cb;
        memset(&cb, 0, sizeof(cb));
        cb.x = tr ? tr->dst_x : 0;
        cb.y = tr ? tr->dst_y : 0;
        free(tr);

        cb.data = value;
        cb.data_length = *length;
        cb.data_type = *type;
        cb.data_format = format ? *format : 8;
        cb.action = st->negotiated_action;

        /* Populate legacy URI fields if type is text/uri-list */
        if (*type == st->text_uri_list) {
            cb.uris = ParseUriList((const char *) value, (int) *length,
                                   &cb.num_uris);
        }

        /* Deliver via Xt callback list if available, otherwise use
         * the direct callback stored in the DropConfig. */
        if (IswHasCallbacks(target, IswNdropCallback) == IswCallbackHasSome) {
            IswCallCallbacks(target, IswNdropCallback, (IswPointer) &cb);
        } else {
            DropConfig *dc = FindDropConfig(st, target);
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

    SendXdndFinished(st, True, ActionToAtom(st, st->negotiated_action));

    /* Fire dragLeave on the hover widget */
    if (st->hover_widget &&
        IswHasCallbacks(st->hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        IswCallCallbacks(st->hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
    } else if (st->hover_widget) {
        DropConfig *dc = FindDropConfig(st, st->hover_widget);
        if (dc && dc->leave_proc) {
            IswDragOverCallbackData cbd = {0};
            dc->leave_proc(st->hover_widget, dc->leave_closure, (IswPointer) &cbd);
        }
    }

    /* Reset drop target state */
    st->src_window = 0;
    st->hover_widget = NULL;
    if (st->src_types) {
        IswFree((char *) st->src_types);
        st->src_types = NULL;
        st->src_num_types = 0;
    }
    st->negotiated_type = XCB_ATOM_NONE;
    st->negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndLeave — source left our window                                 */
/* ------------------------------------------------------------------ */

static void
HandleTargetLeave(XdndState *st)
{
    if (st->hover_widget &&
        IswHasCallbacks(st->hover_widget, IswNdragLeaveCallback) == IswCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        IswCallCallbacks(st->hover_widget, IswNdragLeaveCallback, (IswPointer) &cbd);
    } else if (st->hover_widget) {
        DropConfig *dc = FindDropConfig(st, st->hover_widget);
        if (dc && dc->leave_proc) {
            IswDragOverCallbackData cbd = {0};
            dc->leave_proc(st->hover_widget, dc->leave_closure, (IswPointer) &cbd);
        }
    }

    st->src_window = 0;
    st->hover_widget = NULL;
    if (st->src_types) {
        IswFree((char *) st->src_types);
        st->src_types = NULL;
        st->src_num_types = 0;
    }
    st->negotiated_type = XCB_ATOM_NONE;
    st->negotiated_action = ISW_DND_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* XdndStatus / XdndFinished replies                                  */
/* ------------------------------------------------------------------ */

static void
SendXdndStatus(XdndState *st, Boolean accept, xcb_atom_t action_atom)
{
    xcb_connection_t *conn = IswDisplay(st->shell);
    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndStatus;
    reply.format = 32;
    reply.data.data32[0] = IswWindow(st->shell);
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
    xcb_connection_t *conn = IswDisplay(st->shell);

    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndFinished;
    reply.format = 32;
    reply.data.data32[0] = IswWindow(st->shell);
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

void
ISWXdndStartDrag(Widget source_widget,
                 xcb_button_press_event_t *trigger_event,
                 IswDragSourceDesc *desc)
{
    XdndState *st = GetXdndStateForWidget(source_widget);
    if (!st || st->dragging)
        return;

    /* Don't start a drag if the source widget has an active rubber band */
    if (IswIsSubclass(source_widget, iconViewWidgetClass) &&
        IswIconViewBandActive(source_widget))
        return;

    xcb_connection_t *conn = IswDisplay(st->shell);

    st->dragging = True;
    st->drag_desc = *desc;
    st->drag_source = source_widget;
    st->drag_timestamp = trigger_event->time;
    st->drag_start_x = trigger_event->root_x;
    st->drag_start_y = trigger_event->root_y;
    st->drag_press_x = trigger_event->event_x;
    st->drag_press_y = trigger_event->event_y;
    st->drag_started = False;
    st->drag_target_win = XCB_NONE;
    st->drag_target_ver = 0;
    st->drag_status_pending = False;
    st->drag_target_accepted = False;
    st->drag_target_action = ISW_DND_ACTION_NONE;
    st->drag_last_x = trigger_event->root_x;
    st->drag_last_y = trigger_event->root_y;
    st->drag_position_deferred = False;
    st->drag_icon_win = XCB_NONE;
    st->drag_icon_owned = False;
    st->drag_icon_cmap = XCB_NONE;
    st->drag_icon_visual = 0;
    st->finished_timer = 0;

    /* Copy the type list (caller's array may be transient) */
    if (desc->num_types > 0) {
        st->drag_desc.types = (xcb_atom_t *) IswMalloc(
            desc->num_types * sizeof(xcb_atom_t));
        memcpy(st->drag_desc.types, desc->types,
               desc->num_types * sizeof(xcb_atom_t));
    }

    /* Own XdndSelection */
    (void) IswOwnSelection(st->shell, st->XdndSelection, st->drag_timestamp,
                    DragConvertSelection, DragLoseSelection, NULL);
    /* Set XdndTypeList property on our window if >3 types */
    if (desc->num_types > 3) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, IswWindow(st->shell),
                            st->XdndTypeList, XCB_ATOM_ATOM, 32,
                            desc->num_types, st->drag_desc.types);
    }

    /* Eagerly convert and store drag data as a property on the source
     * window.  This allows the target to read it via xcb_get_property
     * as a fallback when the Xt selection transfer fails (cross-client
     * selection dispatch is unreliable in XCB-based Xt). */
    if (desc->convert) {
        for (int i = 0; i < desc->num_types; i++) {
            IswPointer data = NULL;
            unsigned long length = 0;
            int format = 8;
            if (desc->convert(st->drag_source, desc->types[i],
                              &data, &length, &format, desc->client_data)) {
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE,
                                    IswWindow(st->shell),
                                    desc->types[i], desc->types[i],
                                    format, length, data);
                IswFree(data);
            }
        }
        xcb_flush(conn);
    }

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
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, IswWindow(st->shell),
                                st->XdndActionList, XCB_ATOM_ATOM, 32, n, actions);
        }
    }

    /* Grab the pointer on the shell window. During an active grab all
     * pointer events report on the grab window regardless of where the
     * cursor is, so we still track movement over foreign apps.  Using
     * the shell (not root) ensures Xt dispatches events to our handler. */
    xcb_grab_pointer_cookie_t gc =
        xcb_grab_pointer(conn, False, IswWindow(st->shell),
                         XCB_EVENT_MASK_BUTTON_RELEASE |
                         XCB_EVENT_MASK_POINTER_MOTION |
                         XCB_EVENT_MASK_BUTTON_MOTION,
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                         XCB_NONE, st->cursor_default,
                         st->drag_timestamp);
    xcb_grab_pointer_reply_t *gr = xcb_grab_pointer_reply(conn, gc, NULL);
    if (!gr || gr->status != XCB_GRAB_STATUS_SUCCESS) {
        free(gr);
        DragCleanup(st);
        return;
    }
    free(gr);

    /* Grab the keyboard through Xt so key events dispatch to the shell,
     * where HandleDragEvent is registered.  Raw xcb_grab_keyboard does
     * the X grab but skips Xt's input dispatch bookkeeping, so key
     * events get remapped to the focused child and never reach us. */
    IswGrabKeyboard(st->shell, False,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                    st->drag_timestamp);

    /* Install raw event handler for drag tracking */
    IswAddEventHandler(st->shell,
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
HandleDragEvent(Widget w, IswPointer closure, xcb_generic_event_t *event,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;
    uint8_t type = event->response_type & ~0x80;

    (void) w;

    if (!st->dragging) {
        return;
    }

    switch (type) {
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *me = (xcb_motion_notify_event_t *) event;
        int root_x = me->root_x;
        int root_y = me->root_y;

        if (!st->drag_started) {
            int dx = root_x - st->drag_start_x;
            int dy = root_y - st->drag_start_y;
            if (dx * dx + dy * dy < DRAG_THRESHOLD * DRAG_THRESHOLD)
                return;
            st->drag_started = True;
            CreateDragIcon(st);
        }

        DragMotion(st, root_x, root_y);
        *cont = FALSE;
        break;
    }

    case XCB_BUTTON_RELEASE: {
        if (!st->drag_started) {
            DragCleanup(st);
        } else if (st->drag_target_win != XCB_NONE && st->drag_target_accepted) {
            DragDrop(st);
        } else {
            DragCancel(st);
        }
        *cont = FALSE;
        break;
    }

    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
        xcb_key_press_event_t *ke = (xcb_key_press_event_t *) event;
        xcb_connection_t *conn = IswDisplay(st->shell);
        xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
        if (syms) {
            xcb_keysym_t sym = xcb_key_symbols_get_keysym(syms, ke->detail, 0);
            xcb_key_symbols_free(syms);
            if (type == XCB_KEY_PRESS && sym == 0xff1b) {  /* XK_Escape */
                DragCancel(st);
                *cont = FALSE;
            } else if (sym == 0xffe1 || sym == 0xffe2 ||  /* Shift_L/R */
                       sym == 0xffe3 || sym == 0xffe4) {  /* Control_L/R */
                /* Modifier changed — re-evaluate action and cursor */
                if (st->drag_started)
                    DragMotion(st, st->drag_last_x, st->drag_last_y);
                *cont = FALSE;
            }
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

    if (start == XCB_NONE || start == IswScreen(st->shell)->root)
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
DragMotion(XdndState *st, int root_x, int root_y)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    MoveDragIcon(st, root_x, root_y);

    /* Find the window under the cursor */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         IswScreen(st->shell)->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);

    xcb_window_t child_win = XCB_NONE;
    unsigned int modifiers = 0;
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
        st->drag_target_ver = target_version;
        st->drag_target_accepted = False;
        st->drag_target_action = ISW_DND_ACTION_NONE;
        st->drag_status_pending = False;

        /* Enter new target */
        if (target_win != XCB_NONE)
            SendDragEnter(st, target_win);
    }

    /* Send position */
    if (st->drag_target_win != XCB_NONE) {
        /* Check if modifier keys changed the desired action */
        IswDndAction mod_action = ModifiersToAction(st, modifiers);
        (void) mod_action; /* used in SendDragPosition via drag_desc.actions */

        if (st->drag_status_pending) {
            /* Defer — will resend when XdndStatus arrives */
            st->drag_last_x = root_x;
            st->drag_last_y = root_y;
            st->drag_position_deferred = True;
        } else {
            SendDragPosition(st, root_x, root_y);
        }
    }

    /* Update cursor based on acceptance state */
    xcb_cursor_t cursor;
    if (st->drag_target_win == XCB_NONE) {
        cursor = st->cursor_default;
    } else if (!st->drag_target_accepted) {
        cursor = st->cursor_reject;
    } else {
        switch (st->drag_target_action) {
        case ISW_DND_ACTION_MOVE: cursor = st->cursor_move; break;
        case ISW_DND_ACTION_LINK: cursor = st->cursor_link; break;
        default:                  cursor = st->cursor_copy; break;
        }
    }
    xcb_change_active_pointer_grab(conn, cursor,
                                   XCB_CURRENT_TIME,
                                   XCB_EVENT_MASK_BUTTON_RELEASE |
                                   XCB_EVENT_MASK_POINTER_MOTION |
                                   XCB_EVENT_MASK_BUTTON_MOTION);
    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* Send XDND messages to foreign target                               */
/* ------------------------------------------------------------------ */

static void
SendDragEnter(XdndState *st, xcb_window_t target)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = target;
    cm.type = st->XdndEnter;
    cm.format = 32;
    cm.data.data32[0] = IswWindow(st->shell);
    cm.data.data32[1] = (XDND_VERSION << 24);

    if (st->drag_desc.num_types > 3) {
        cm.data.data32[1] |= 1;  /* use XdndTypeList property */
    }

    /* Fill in up to 3 types directly */
    for (int i = 0; i < 3 && i < st->drag_desc.num_types; i++)
        cm.data.data32[2 + i] = st->drag_desc.types[i];


    xcb_send_event(conn, False, target, 0, (const char *) &cm);
    xcb_flush(conn);
}

static void
SendDragPosition(XdndState *st, int root_x, int root_y)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    /* Determine action from keyboard modifiers */
    /* Query pointer for modifiers and physical root coordinates.
     * root_x/root_y from the motion event have been descaled to logical
     * pixels by the dispatcher — XdndPosition must use the X server's
     * native physical coordinates so external apps map them correctly. */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         IswScreen(st->shell)->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);
    unsigned int modifiers = 0;
    int phys_x = root_x, phys_y = root_y;
    if (qpr) {
        modifiers = qpr->mask;
        phys_x = qpr->root_x;
        phys_y = qpr->root_y;
        free(qpr);
    }

    IswDndAction desired = ModifiersToAction(st, modifiers);
    /* Constrain to offered actions */
    if (!(st->drag_desc.actions & desired))
        desired = ISW_DND_ACTION_COPY;  /* fallback */

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndPosition;
    cm.format = 32;
    cm.data.data32[0] = IswWindow(st->shell);
    cm.data.data32[1] = 0;  /* reserved */
    cm.data.data32[2] = ((uint32_t) phys_x << 16) | ((uint32_t) phys_y & 0xFFFF);
    cm.data.data32[3] = st->drag_timestamp;
    cm.data.data32[4] = ActionToAtom(st, desired);


    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);

    st->drag_status_pending = True;
    st->drag_last_x = root_x;
    st->drag_last_y = root_y;
}

static void
SendDragLeave(XdndState *st)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndLeave;
    cm.format = 32;
    cm.data.data32[0] = IswWindow(st->shell);

    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);
}

static void
SendDragDrop(XdndState *st)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndDrop;
    cm.format = 32;
    cm.data.data32[0] = IswWindow(st->shell);
    cm.data.data32[1] = 0;  /* reserved */
    cm.data.data32[2] = st->drag_timestamp;

    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* XdndStatus handler (drag source receiving target's reply)          */
/* ------------------------------------------------------------------ */

static void
HandleDragStatus(XdndState *st, xcb_client_message_event_t *cm)
{
    st->drag_status_pending = False;
    st->drag_target_accepted = (cm->data.data32[1] & 1) != 0;
    st->drag_target_action = AtomToAction(st, cm->data.data32[4]);

    /* If we deferred a position update, send it now */
    if (st->drag_position_deferred) {
        st->drag_position_deferred = False;
        SendDragPosition(st, st->drag_last_x, st->drag_last_y);
    }
}

/* ------------------------------------------------------------------ */
/* XdndFinished handler (drag source receiving completion)            */
/* ------------------------------------------------------------------ */

static void
HandleDragFinished(XdndState *st, xcb_client_message_event_t *cm)
{
    Boolean accepted = (cm->data.data32[1] & 1) != 0;
    IswDndAction performed = AtomToAction(st, cm->data.data32[2]);

    if (st->finished_timer) {
        IswRemoveTimeOut(st->finished_timer);
        st->finished_timer = 0;
    }

    if (st->drag_desc.finished) {
        st->drag_desc.finished(st->drag_source, performed,
                               accepted, st->drag_desc.client_data);
    }

    DragCleanup(st);
}

static void
DragFinishedTimeout(IswPointer closure, IswIntervalId *id)
{
    XdndState *st = (XdndState *) closure;
    (void) id;

    st->finished_timer = 0;

    if (st->drag_desc.finished) {
        st->drag_desc.finished(st->drag_source, ISW_DND_ACTION_NONE,
                               False, st->drag_desc.client_data);
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
    st->finished_timer = IswAppAddTimeOut(
        IswWidgetToApplicationContext(st->shell),
        FINISHED_TIMEOUT, DragFinishedTimeout, (IswPointer) st);

    /* Ungrab pointer and remove drag event handler so normal input
     * resumes immediately. XdndFinished arrives as a ClientMessage
     * through HandleXdndEvent (the non-maskable handler), not through
     * HandleDragEvent, so we don't need it anymore. Keep st->dragging
     * True so HandleXdndEvent still processes XdndFinished/XdndStatus. */
    xcb_connection_t *conn = IswDisplay(st->shell);
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    IswUngrabKeyboard(st->shell, XCB_CURRENT_TIME);
    DestroyDragIcon(st);

    IswRemoveEventHandler(st->shell,
                         XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                         XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                         XCB_EVENT_MASK_KEY_RELEASE,
                         TRUE, HandleDragEvent, (IswPointer) st);

    xcb_flush(conn);
}

static void
DragCancel(XdndState *st)
{
    if (st->drag_target_win != XCB_NONE)
        SendDragLeave(st);

    if (st->drag_desc.finished) {
        st->drag_desc.finished(st->drag_source, ISW_DND_ACTION_NONE,
                               False, st->drag_desc.client_data);
    }

    DragCleanup(st);
}

static void
DragCleanup(XdndState *st)
{
    xcb_connection_t *conn = IswDisplay(st->shell);

    /* Ungrab pointer and keyboard */
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    IswUngrabKeyboard(st->shell, XCB_CURRENT_TIME);

    /* Remove drag event handler */
    IswRemoveEventHandler(st->shell,
                         XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                         XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_KEY_PRESS |
                         XCB_EVENT_MASK_KEY_RELEASE,
                         TRUE, HandleDragEvent, (IswPointer) st);

    /* Disown selection */
    IswDisownSelection(st->shell, st->XdndSelection, st->drag_timestamp);

    /* Clean up icon */
    DestroyDragIcon(st);
    if (st->drag_icon_owned && st->drag_desc.icon_pixmap != 0) {
        xcb_free_pixmap(conn, st->drag_desc.icon_pixmap);
        st->drag_desc.icon_pixmap = 0;
        st->drag_icon_owned = False;
    }
    if (st->drag_icon_cmap != XCB_NONE) {
        xcb_free_colormap(conn, st->drag_icon_cmap);
        st->drag_icon_cmap = XCB_NONE;
    }

    /* Remove timeout */
    if (st->finished_timer) {
        IswRemoveTimeOut(st->finished_timer);
        st->finished_timer = 0;
    }

    /* Free copied type list */
    if (st->drag_desc.types) {
        IswFree((char *) st->drag_desc.types);
        st->drag_desc.types = NULL;
    }

    /* Reset state */
    st->dragging = False;
    st->drag_started = False;
    st->drag_source = NULL;
    st->drag_target_win = XCB_NONE;
    st->drag_status_pending = False;
    st->drag_position_deferred = False;

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
    if (!st || !st->dragging)
        return False;

    (void) selection;

    /* Handle TARGETS request */
    if (*target == st->targets_atom) {
        xcb_atom_t *targets = (xcb_atom_t *) IswMalloc(
            (st->drag_desc.num_types + 1) * sizeof(xcb_atom_t));
        targets[0] = st->targets_atom;
        for (int i = 0; i < st->drag_desc.num_types; i++)
            targets[i + 1] = st->drag_desc.types[i];

        *type_return = XCB_ATOM_ATOM;
        *value_return = (IswPointer) targets;
        *length_return = st->drag_desc.num_types + 1;
        *format_return = 32;
        return True;
    }

    /* Delegate to app's convert proc */
    if (st->drag_desc.convert) {
        Boolean ok = st->drag_desc.convert(st->drag_source, *target,
                                           value_return, length_return,
                                           format_return,
                                           st->drag_desc.client_data);
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
    xcb_connection_t *conn = IswDisplay(st->shell);
    xcb_screen_t *screen = IswScreen(st->shell);

    /* Find a 32-bit visual for alpha transparency */
    xcb_visualtype_t *visual32 = ISWRenderFindVisual(screen, 32);
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

    st->drag_desc.icon_pixmap = pixmap;
    st->drag_desc.icon_width = (int)w;
    st->drag_desc.icon_height = (int)h;
    st->drag_desc.icon_hotspot_x = (int)w / 2;
    st->drag_desc.icon_hotspot_y = (int)h / 2;
    st->drag_icon_owned = True;
    st->drag_icon_cmap = cmap;
    st->drag_icon_visual = visual32->visual_id;
}

static void
CreateDragIcon(XdndState *st)
{
    /* Auto-generate icon from IconView item raster */
    if (st->drag_desc.icon_pixmap == 0 &&
        IswIsSubclass(st->drag_source, iconViewWidgetClass)) {
        int idx = IswIconViewHitTest(st->drag_source,
                                     st->drag_press_x, st->drag_press_y);
        if (idx >= 0) {
            unsigned int rw, rh;
            const unsigned char *raster =
                IswIconViewGetItemRaster(st->drag_source, idx, &rw, &rh);
            if (raster)
                CreateDragIconFromRaster(st, raster, rw, rh);
        }
    }

    if (st->drag_desc.icon_pixmap == 0)
        return;

    xcb_connection_t *conn = IswDisplay(st->shell);
    xcb_screen_t *screen = IswScreen(st->shell);

    st->drag_icon_win = xcb_generate_id(conn);

    if (st->drag_icon_owned && st->drag_icon_visual) {
        /* 32-bit ARGB window for transparent drag icon */
        uint32_t vals[4];
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL |
                        XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP;
        vals[0] = st->drag_desc.icon_pixmap;
        vals[1] = 0;
        vals[2] = True;
        vals[3] = st->drag_icon_cmap;

        xcb_create_window(conn, 32,
                          st->drag_icon_win, screen->root,
                          (int16_t)(st->drag_start_x - st->drag_desc.icon_hotspot_x),
                          (int16_t)(st->drag_start_y - st->drag_desc.icon_hotspot_y),
                          st->drag_desc.icon_width,
                          st->drag_desc.icon_height,
                          0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          st->drag_icon_visual, mask, vals);
    } else {
        uint32_t vals[2];
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_OVERRIDE_REDIRECT;
        vals[0] = st->drag_desc.icon_pixmap;
        vals[1] = True;

        xcb_create_window(conn, XCB_COPY_FROM_PARENT,
                          st->drag_icon_win, screen->root,
                          (int16_t)(st->drag_start_x - st->drag_desc.icon_hotspot_x),
                          (int16_t)(st->drag_start_y - st->drag_desc.icon_hotspot_y),
                          st->drag_desc.icon_width,
                          st->drag_desc.icon_height,
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

    xcb_connection_t *conn = IswDisplay(st->shell);
    /* HiDPI: scale logical to physical for the X server */
    double _sf = _IswGetScaleFactor(conn);
    uint32_t values[2];
    values[0] = (uint32_t)(int32_t)((root_x - st->drag_desc.icon_hotspot_x) * _sf + 0.5);
    values[1] = (uint32_t)(int32_t)((root_y - st->drag_desc.icon_hotspot_y) * _sf + 0.5);

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

    xcb_connection_t *conn = IswDisplay(st->shell);
    xcb_destroy_window(conn, st->drag_icon_win);
    st->drag_icon_win = XCB_NONE;
    xcb_flush(conn);
}
