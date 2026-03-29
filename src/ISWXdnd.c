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

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWXdnd.h>
#include <ISW/ISWContext.h>
#include "ISWXcbDraw.h"

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

    /* Cursors */
    xcb_cursor_t        cursor_default;
    xcb_cursor_t        cursor_copy;
    xcb_cursor_t        cursor_move;
    xcb_cursor_t        cursor_link;
    xcb_cursor_t        cursor_reject;

    /* Finished timeout */
    XtIntervalId        finished_timer;

    /* Per-widget drop configs */
    DropConfig         *drop_configs;
} XdndState;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void InternAtoms(XdndState *st, xcb_connection_t *conn);
static void CreateCursors(XdndState *st, xcb_connection_t *conn);

/* Drop target handlers */
static void HandleXdndEvent(Widget w, XtPointer closure,
                            xcb_generic_event_t *event, Boolean *cont);
static void HandleTargetEnter(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetPosition(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetDrop(XdndState *st, xcb_client_message_event_t *cm);
static void HandleTargetLeave(XdndState *st);
static void TargetSelectionCallback(Widget w, XtPointer closure,
                                    xcb_atom_t *selection, xcb_atom_t *type,
                                    XtPointer value, unsigned long *length,
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
static Widget FindDropChild(Widget composite, int wx, int wy);

/* Drag source */
static void HandleDragEvent(Widget w, XtPointer closure,
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
static void DragFinishedTimeout(XtPointer closure, XtIntervalId *id);

/* Drag source selection */
static Boolean DragConvertSelection(Widget w, xcb_atom_t *selection,
                                    xcb_atom_t *target, xcb_atom_t *type_return,
                                    XtPointer *value_return,
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

static void
CreateCursors(XdndState *st, xcb_connection_t *conn)
{
    st->cursor_default = CreateGlyphCursor(conn, XC_left_ptr);
    st->cursor_copy    = CreateGlyphCursor(conn, XC_hand2);
    st->cursor_move    = CreateGlyphCursor(conn, XC_fleur);
    st->cursor_link    = CreateGlyphCursor(conn, XC_hand1);
    st->cursor_reject  = CreateGlyphCursor(conn, XC_X_cursor);
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
    Boolean shift = (state & ShiftMask) != 0;
    Boolean ctrl  = (state & ControlMask) != 0;

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
        dc = (DropConfig *) XtCalloc(1, sizeof(DropConfig));
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
FindDropChild(Widget composite, int wx, int wy)
{
    if (!XtIsComposite(composite))
        return NULL;

    CompositeWidget cw = (CompositeWidget) composite;
    for (int i = cw->composite.num_children - 1; i >= 0; i--) {
        Widget child = cw->composite.children[i];
        if (!XtIsManaged(child) || !XtIsRealized(child))
            continue;

        int cx = child->core.x;
        int cy = child->core.y;
        int cw2 = child->core.width;
        int ch = child->core.height;

        if (wx >= cx && wx < cx + cw2 && wy >= cy && wy < cy + ch) {
            if (XtHasCallbacks(child, XtNdropCallback) == XtCallbackHasSome)
                return child;

            if (XtIsComposite(child)) {
                Widget deeper = FindDropChild(child, wx - cx, wy - cy);
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_translate_coordinates_cookie_t cookie =
        xcb_translate_coordinates(conn,
            XtScreen(st->shell)->root, XtWindow(st->shell),
            (int16_t) root_x, (int16_t) root_y);
    xcb_translate_coordinates_reply_t *reply =
        xcb_translate_coordinates_reply(conn, cookie, NULL);

    if (!reply)
        return NULL;

    int wx = reply->dst_x;
    int wy = reply->dst_y;
    free(reply);

    if (XtHasCallbacks(st->shell, XtNdropCallback) == XtCallbackHasSome)
        return st->shell;

    return FindDropChild(st->shell, wx, wy);
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
                uris = (char **) XtRealloc((char *) uris,
                                           (capacity + 1) * sizeof(char *));
            }
            char *entry = XtMalloc(uri_len + 1);
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
    if (IswFindContext(XtDisplay(shell), XtWindow(shell),
                       xdnd_context, (void **) &st) != 0)
        return NULL;
    return st;
}

static XdndState *
GetXdndStateForWidget(Widget w)
{
    /* Walk up to the shell */
    Widget shell = w;
    while (shell && !XtIsShell(shell))
        shell = XtParent(shell);
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

    if (!XtIsRealized(shell))
        return;

    if (xdnd_context == 0)
        xdnd_context = IswUniqueContext();

    /* Check if already enabled */
    if (GetXdndState(shell))
        return;

    conn = XtDisplay(shell);
    st = (XdndState *) XtCalloc(1, sizeof(XdndState));
    st->shell = shell;
    InternAtoms(st, conn);
    CreateCursors(st, conn);

    /* Store state on the shell window */
    IswSaveContext(XtDisplay(shell), XtWindow(shell),
                   xdnd_context, (void *) st);

    /* Advertise XdndAware */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, XtWindow(shell),
                        st->XdndAware, XCB_ATOM_ATOM, 32, 1, &version);

    /* Register non-maskable event handler for ClientMessage and SelectionNotify */
    XtAddEventHandler(shell, (EventMask) 0, TRUE,
                      HandleXdndEvent, (XtPointer) st);

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
        XtFree((char *) dc->accepted_types);

    if (types && num_types > 0) {
        dc->accepted_types = (xcb_atom_t *) XtMalloc(num_types * sizeof(xcb_atom_t));
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

xcb_atom_t
ISWXdndInternType(Widget w, const char *mime_type)
{
    return IswXcbInternAtom(XtDisplay(w), mime_type, False);
}

/* ================================================================== */
/*                                                                    */
/* DROP TARGET — incoming drag handling                                */
/*                                                                    */
/* ================================================================== */

static void
HandleXdndEvent(Widget w, XtPointer closure, xcb_generic_event_t *event,
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
        XtFree((char *) st->src_types);
        st->src_types = NULL;
        st->src_num_types = 0;
    }

    Boolean use_type_list = (cm->data.data32[1] & 1);

    if (use_type_list) {
        /* More than 3 types — read XdndTypeList property */
        xcb_connection_t *conn = XtDisplay(st->shell);
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->src_window,
                             st->XdndTypeList, XCB_ATOM_ATOM, 0, 256);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);
        if (reply) {
            xcb_atom_t *atoms = (xcb_atom_t *) xcb_get_property_value(reply);
            int count = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
            if (count > 0) {
                st->src_types = (xcb_atom_t *) XtMalloc(count * sizeof(xcb_atom_t));
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
            st->src_types = (xcb_atom_t *) XtMalloc(count * sizeof(xcb_atom_t));
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
            XtHasCallbacks(st->hover_widget, XtNdragLeaveCallback) == XtCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            XtCallCallbacks(st->hover_widget, XtNdragLeaveCallback, (XtPointer) &cbd);
        }

        st->hover_widget = target;

        /* Enter new widget */
        if (target &&
            XtHasCallbacks(target, XtNdragEnterCallback) == XtCallbackHasSome) {
            IswDragOverCallbackData cbd = {0};
            cbd.offered_types = st->src_types;
            cbd.num_offered_types = st->src_num_types;
            cbd.offered_actions = st->src_actions;
            cbd.proposed_action = proposed;
            XtCallCallbacks(target, XtNdragEnterCallback, (XtPointer) &cbd);
        }
    }

    /* Negotiate type/action */
    Boolean accept = False;
    xcb_atom_t accepted_type = XCB_ATOM_NONE;
    IswDndAction accepted_action = ISW_DND_ACTION_NONE;

    if (target) {
        /* First, let the widget's dragMotion callback override */
        if (XtHasCallbacks(target, XtNdragMotionCallback) == XtCallbackHasSome) {
            xcb_connection_t *conn = XtDisplay(st->shell);
            xcb_translate_coordinates_cookie_t tc =
                xcb_translate_coordinates(conn,
                    XtScreen(st->shell)->root, XtWindow(target),
                    (int16_t) st->drop_x, (int16_t) st->drop_y);
            xcb_translate_coordinates_reply_t *tr =
                xcb_translate_coordinates_reply(conn, tc, NULL);

            IswDragOverCallbackData cbd = {0};
            cbd.x = tr ? tr->dst_x : 0;
            cbd.y = tr ? tr->dst_y : 0;
            free(tr);
            cbd.offered_types = st->src_types;
            cbd.num_offered_types = st->src_num_types;
            cbd.offered_actions = st->src_actions;
            cbd.proposed_action = proposed;

            XtCallCallbacks(target, XtNdragMotionCallback, (XtPointer) &cbd);

            if (cbd.accepted_type != XCB_ATOM_NONE) {
                accepted_type = cbd.accepted_type;
                accepted_action = cbd.accepted_action;
                accept = True;
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
    XtGetSelectionValue(st->shell, st->XdndSelection,
                        st->negotiated_type,
                        TargetSelectionCallback,
                        (XtPointer) st,
                        st->drop_timestamp);
}

/* ------------------------------------------------------------------ */
/* Selection data arrival for drop target                             */
/* ------------------------------------------------------------------ */

static void
TargetSelectionCallback(Widget w, XtPointer closure,
                        xcb_atom_t *selection, xcb_atom_t *type,
                        XtPointer value, unsigned long *length,
                        int *format)
{
    XdndState *st = (XdndState *) closure;

    (void) w;
    (void) selection;

    if (!value || !length || *length == 0) {
        /* Selection transfer failed — try manual property read as fallback */
        xcb_connection_t *conn = XtDisplay(st->shell);
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, True, XtWindow(st->shell),
                             st->XdndSelection, XCB_ATOM_ANY, 0, 65536);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);

        if (reply) {
            char *data = (char *) xcb_get_property_value(reply);
            int data_len = xcb_get_property_value_length(reply);

            if (data && data_len > 0) {
                /* Make a copy since we need it after freeing the reply */
                char *data_copy = XtMalloc(data_len);
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
        xcb_connection_t *conn = XtDisplay(st->shell);
        xcb_translate_coordinates_cookie_t tc =
            xcb_translate_coordinates(conn,
                XtScreen(st->shell)->root, XtWindow(target),
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

        XtCallCallbacks(target, XtNdropCallback, (XtPointer) &cb);

        /* Free URI strings */
        if (cb.uris) {
            for (int i = 0; i < cb.num_uris; i++)
                XtFree(cb.uris[i]);
            XtFree((char *) cb.uris);
        }
    }

    XtFree(value);

    SendXdndFinished(st, True, ActionToAtom(st, st->negotiated_action));

    /* Fire dragLeave on the hover widget */
    if (st->hover_widget &&
        XtHasCallbacks(st->hover_widget, XtNdragLeaveCallback) == XtCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        XtCallCallbacks(st->hover_widget, XtNdragLeaveCallback, (XtPointer) &cbd);
    }

    /* Reset drop target state */
    st->src_window = 0;
    st->hover_widget = NULL;
    if (st->src_types) {
        XtFree((char *) st->src_types);
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
        XtHasCallbacks(st->hover_widget, XtNdragLeaveCallback) == XtCallbackHasSome) {
        IswDragOverCallbackData cbd = {0};
        XtCallCallbacks(st->hover_widget, XtNdragLeaveCallback, (XtPointer) &cbd);
    }

    st->src_window = 0;
    st->hover_widget = NULL;
    if (st->src_types) {
        XtFree((char *) st->src_types);
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndStatus;
    reply.format = 32;
    reply.data.data32[0] = XtWindow(st->shell);
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->src_window;
    reply.type = st->XdndFinished;
    reply.format = 32;
    reply.data.data32[0] = XtWindow(st->shell);
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

    xcb_connection_t *conn = XtDisplay(st->shell);

    st->dragging = True;
    st->drag_desc = *desc;
    st->drag_source = source_widget;
    st->drag_timestamp = trigger_event->time;
    st->drag_start_x = trigger_event->root_x;
    st->drag_start_y = trigger_event->root_y;
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
    st->finished_timer = 0;

    /* Copy the type list (caller's array may be transient) */
    if (desc->num_types > 0) {
        st->drag_desc.types = (xcb_atom_t *) XtMalloc(
            desc->num_types * sizeof(xcb_atom_t));
        memcpy(st->drag_desc.types, desc->types,
               desc->num_types * sizeof(xcb_atom_t));
    }

    /* Own XdndSelection */
    XtOwnSelection(st->shell, st->XdndSelection, st->drag_timestamp,
                    DragConvertSelection, DragLoseSelection, NULL);

    /* Set XdndTypeList property on our window if >3 types */
    if (desc->num_types > 3) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, XtWindow(st->shell),
                            st->XdndTypeList, XCB_ATOM_ATOM, 32,
                            desc->num_types, st->drag_desc.types);
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
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, XtWindow(st->shell),
                                st->XdndActionList, XCB_ATOM_ATOM, 32, n, actions);
        }
    }

    /* Grab the pointer on the shell window. During an active grab all
     * pointer events report on the grab window regardless of where the
     * cursor is, so we still track movement over foreign apps.  Using
     * the shell (not root) ensures Xt dispatches events to our handler. */
    xcb_grab_pointer_cookie_t gc =
        xcb_grab_pointer(conn, False, XtWindow(st->shell),
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

    /* Install raw event handler for drag tracking */
    XtAddEventHandler(st->shell,
                      ButtonReleaseMask | PointerMotionMask |
                      ButtonMotionMask | KeyPressMask,
                      TRUE,  /* non-maskable too, for ClientMessage */
                      HandleDragEvent, (XtPointer) st);

    xcb_flush(conn);
}

/* ------------------------------------------------------------------ */
/* Drag event handler                                                 */
/* ------------------------------------------------------------------ */

static void
HandleDragEvent(Widget w, XtPointer closure, xcb_generic_event_t *event,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;
    uint8_t type = event->response_type & ~0x80;

    (void) w;

    if (!st->dragging)
        return;

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

    case XCB_KEY_PRESS: {
        xcb_key_press_event_t *ke = (xcb_key_press_event_t *) event;
        /* Check for Escape key (keycode 9 on most systems, but use keysym) */
        xcb_connection_t *conn = XtDisplay(st->shell);
        xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
        if (syms) {
            xcb_keysym_t sym = xcb_key_symbols_get_keysym(syms, ke->detail, 0);
            xcb_key_symbols_free(syms);
            if (sym == 0xff1b) {  /* XK_Escape */
                DragCancel(st);
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

static xcb_window_t
FindXdndAwareWindow(XdndState *st, xcb_connection_t *conn,
                    xcb_window_t start, int *version_out)
{
    xcb_window_t win = start;
    *version_out = 0;

    /* Walk up the window tree looking for XdndAware */
    while (win != XCB_NONE && win != XtScreen(st->shell)->root) {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, win,
                             st->XdndAware, XCB_ATOM_ATOM, 0, 1);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);

        if (reply && reply->type != XCB_ATOM_NONE &&
            xcb_get_property_value_length(reply) >= (int) sizeof(uint32_t)) {
            uint32_t ver = *(uint32_t *) xcb_get_property_value(reply);
            *version_out = (int) ver;
            free(reply);
            return win;
        }
        free(reply);

        /* Go to parent */
        xcb_query_tree_cookie_t tc = xcb_query_tree(conn, win);
        xcb_query_tree_reply_t *tr = xcb_query_tree_reply(conn, tc, NULL);
        if (!tr)
            return XCB_NONE;
        xcb_window_t parent = tr->parent;
        free(tr);

        if (parent == win)
            break;
        win = parent;
    }

    return XCB_NONE;
}

static void
DragMotion(XdndState *st, int root_x, int root_y)
{
    xcb_connection_t *conn = XtDisplay(st->shell);

    MoveDragIcon(st, root_x, root_y);

    /* Find the window under the cursor */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         XtScreen(st->shell)->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);

    xcb_window_t child_win = XCB_NONE;
    unsigned int modifiers = 0;
    if (qpr) {
        child_win = qpr->child;
        modifiers = qpr->mask;
        free(qpr);
    }

    /* Don't target our own shell window */
    if (child_win == XtWindow(st->shell))
        child_win = XCB_NONE;

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
                                   st->drag_timestamp,
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = target;
    cm.type = st->XdndEnter;
    cm.format = 32;
    cm.data.data32[0] = XtWindow(st->shell);
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    /* Determine action from keyboard modifiers */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn,
                                         XtScreen(st->shell)->root);
    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);
    unsigned int modifiers = 0;
    if (qpr) {
        modifiers = qpr->mask;
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
    cm.data.data32[0] = XtWindow(st->shell);
    cm.data.data32[1] = 0;  /* reserved */
    cm.data.data32[2] = ((uint32_t) root_x << 16) | ((uint32_t) root_y & 0xFFFF);
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
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndLeave;
    cm.format = 32;
    cm.data.data32[0] = XtWindow(st->shell);

    xcb_send_event(conn, False, st->drag_target_win, 0, (const char *) &cm);
    xcb_flush(conn);
}

static void
SendDragDrop(XdndState *st)
{
    xcb_connection_t *conn = XtDisplay(st->shell);

    xcb_client_message_event_t cm;
    memset(&cm, 0, sizeof(cm));
    cm.response_type = XCB_CLIENT_MESSAGE;
    cm.window = st->drag_target_win;
    cm.type = st->XdndDrop;
    cm.format = 32;
    cm.data.data32[0] = XtWindow(st->shell);
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
        XtRemoveTimeOut(st->finished_timer);
        st->finished_timer = 0;
    }

    if (st->drag_desc.finished) {
        st->drag_desc.finished(st->drag_source, performed,
                               accepted, st->drag_desc.client_data);
    }

    DragCleanup(st);
}

static void
DragFinishedTimeout(XtPointer closure, XtIntervalId *id)
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
    st->finished_timer = XtAppAddTimeOut(
        XtWidgetToApplicationContext(st->shell),
        FINISHED_TIMEOUT, DragFinishedTimeout, (XtPointer) st);

    /* Ungrab pointer and remove drag event handler so normal input
     * resumes immediately. XdndFinished arrives as a ClientMessage
     * through HandleXdndEvent (the non-maskable handler), not through
     * HandleDragEvent, so we don't need it anymore. Keep st->dragging
     * True so HandleXdndEvent still processes XdndFinished/XdndStatus. */
    xcb_connection_t *conn = XtDisplay(st->shell);
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    DestroyDragIcon(st);

    XtRemoveEventHandler(st->shell,
                         ButtonReleaseMask | PointerMotionMask |
                         ButtonMotionMask | KeyPressMask,
                         TRUE, HandleDragEvent, (XtPointer) st);

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
    xcb_connection_t *conn = XtDisplay(st->shell);

    /* Ungrab pointer */
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);

    /* Remove drag event handler */
    XtRemoveEventHandler(st->shell,
                         ButtonReleaseMask | PointerMotionMask |
                         ButtonMotionMask | KeyPressMask,
                         TRUE, HandleDragEvent, (XtPointer) st);

    /* Disown selection */
    XtDisownSelection(st->shell, st->XdndSelection, st->drag_timestamp);

    /* Clean up icon */
    DestroyDragIcon(st);

    /* Remove timeout */
    if (st->finished_timer) {
        XtRemoveTimeOut(st->finished_timer);
        st->finished_timer = 0;
    }

    /* Free copied type list */
    if (st->drag_desc.types) {
        XtFree((char *) st->drag_desc.types);
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
                     xcb_atom_t *type_return, XtPointer *value_return,
                     unsigned long *length_return, int *format_return)
{
    XdndState *st = GetXdndState(w);
    if (!st || !st->dragging)
        return False;

    (void) selection;

    /* Handle TARGETS request */
    if (*target == st->targets_atom) {
        xcb_atom_t *targets = (xcb_atom_t *) XtMalloc(
            (st->drag_desc.num_types + 1) * sizeof(xcb_atom_t));
        targets[0] = st->targets_atom;
        for (int i = 0; i < st->drag_desc.num_types; i++)
            targets[i + 1] = st->drag_desc.types[i];

        *type_return = XCB_ATOM_ATOM;
        *value_return = (XtPointer) targets;
        *length_return = st->drag_desc.num_types + 1;
        *format_return = 32;
        return True;
    }

    /* Delegate to app's convert proc */
    if (st->drag_desc.convert) {
        return st->drag_desc.convert(st->drag_source, *target,
                                     value_return, length_return,
                                     format_return,
                                     st->drag_desc.client_data);
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
CreateDragIcon(XdndState *st)
{
    if (st->drag_desc.icon_pixmap == 0)
        return;

    xcb_connection_t *conn = XtDisplay(st->shell);
    xcb_screen_t *screen = XtScreen(st->shell);

    st->drag_icon_win = xcb_generate_id(conn);

    uint32_t values[2];
    uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_BACK_PIXMAP;
    values[0] = st->drag_desc.icon_pixmap;
    values[1] = True;

    xcb_create_window(conn, XCB_COPY_FROM_PARENT,
                      st->drag_icon_win, screen->root,
                      (int16_t)(st->drag_start_x - st->drag_desc.icon_hotspot_x),
                      (int16_t)(st->drag_start_y - st->drag_desc.icon_hotspot_y),
                      st->drag_desc.icon_width,
                      st->drag_desc.icon_height,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT, mask, values);

    xcb_map_window(conn, st->drag_icon_win);
    xcb_flush(conn);
}

static void
MoveDragIcon(XdndState *st, int root_x, int root_y)
{
    if (st->drag_icon_win == XCB_NONE)
        return;

    xcb_connection_t *conn = XtDisplay(st->shell);
    uint32_t values[2];
    values[0] = root_x - st->drag_desc.icon_hotspot_x;
    values[1] = root_y - st->drag_desc.icon_hotspot_y;

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

    xcb_connection_t *conn = XtDisplay(st->shell);
    xcb_destroy_window(conn, st->drag_icon_win);
    st->drag_icon_win = XCB_NONE;
    xcb_flush(conn);
}
