/*
 * ISWXdnd.c - XDND version 5 drop target implementation
 *
 * Handles the XDND protocol for receiving drag-and-drop operations.
 * Widgets register XtNdropCallback to accept drops. The shell window
 * advertises XdndAware and handles the protocol messages, then routes
 * the drop data to the widget under the cursor.
 *
 * Pure XCB — no Xlib dependencies.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWXdnd.h>
#include "ISWXcbDraw.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* XDND protocol version we support */
#define XDND_VERSION 5

/* Per-shell XDND state */
typedef struct _XdndState {
    /* Atoms */
    xcb_atom_t XdndAware;
    xcb_atom_t XdndEnter;
    xcb_atom_t XdndPosition;
    xcb_atom_t XdndStatus;
    xcb_atom_t XdndLeave;
    xcb_atom_t XdndDrop;
    xcb_atom_t XdndFinished;
    xcb_atom_t XdndSelection;
    xcb_atom_t XdndActionCopy;
    xcb_atom_t XdndTypeList;
    xcb_atom_t text_uri_list;
    xcb_atom_t text_plain;

    /* Current drag session */
    xcb_window_t source_window;
    Boolean       have_uri_list;    /* source offers text/uri-list */
    int           drop_x, drop_y;   /* last position (root coords) */
    Widget        shell;
} XdndState;

static void InternAtoms(XdndState *st, xcb_connection_t *conn);
static void HandleXdndEvent(Widget w, XtPointer closure,
                            xcb_generic_event_t *event, Boolean *cont);
static void HandleXdndEnter(XdndState *st, xcb_client_message_event_t *cm);
static void HandleXdndPosition(XdndState *st, xcb_client_message_event_t *cm);
static void HandleXdndDrop(XdndState *st, xcb_client_message_event_t *cm);
static void HandleXdndLeave(XdndState *st);
static void HandleSelectionData(Widget w, XtPointer closure,
                                xcb_atom_t *selection, xcb_atom_t *type,
                                XtPointer value, unsigned long *length,
                                int *format);
static Widget FindDropTarget(Widget shell, int root_x, int root_y);
static Widget FindDropChild(Widget composite, int wx, int wy);

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
    st->XdndActionCopy = IswXcbInternAtom(conn, "XdndActionCopy", False);
    st->XdndTypeList   = IswXcbInternAtom(conn, "XdndTypeList", False);
    st->text_uri_list  = IswXcbInternAtom(conn, "text/uri-list", False);
    st->text_plain     = IswXcbInternAtom(conn, "text/plain", False);
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

    conn = XtDisplay(shell);
    st = (XdndState *) XtCalloc(1, sizeof(XdndState));
    st->shell = shell;
    InternAtoms(st, conn);

    /* Advertise XdndAware on the shell window */
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
    /* Currently a no-op marker — the XDND handler walks the widget tree
     * to find widgets with XtNdropCallback set. In future this could
     * register the widget in a lookup table for faster dispatch. */
    (void) w;
}

/* ------------------------------------------------------------------ */

static void
HandleXdndEvent(Widget w, XtPointer closure, xcb_generic_event_t *event,
                Boolean *cont)
{
    XdndState *st = (XdndState *) closure;
    uint8_t type = event->response_type & ~0x80;

    if (type == XCB_CLIENT_MESSAGE) {
        xcb_client_message_event_t *cm = (xcb_client_message_event_t *) event;

        if (cm->type == st->XdndEnter) {
            HandleXdndEnter(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndPosition) {
            HandleXdndPosition(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndDrop) {
            HandleXdndDrop(st, cm);
            *cont = FALSE;
        } else if (cm->type == st->XdndLeave) {
            HandleXdndLeave(st);
            *cont = FALSE;
        }
    } else if (type == XCB_SELECTION_NOTIFY) {
        xcb_selection_notify_event_t *sn = (xcb_selection_notify_event_t *) event;
        if (sn->selection == st->XdndSelection) {
            HandleSelectionData(w, closure, &sn->selection,
                                &sn->target, NULL, NULL, NULL);
            *cont = FALSE;
        }
    }

    (void) w;
}

/* ------------------------------------------------------------------ */

static void
HandleXdndEnter(XdndState *st, xcb_client_message_event_t *cm)
{
    st->source_window = cm->data.data32[0];
    st->have_uri_list = False;

    int version = (cm->data.data32[1] >> 24) & 0xFF;
    if (version > XDND_VERSION)
        return;

    Boolean use_type_list = (cm->data.data32[1] & 1);

    if (use_type_list) {
        /* More than 3 types — read XdndTypeList property from source */
        xcb_connection_t *conn = XtDisplay(st->shell);
        xcb_get_property_cookie_t cookie =
            xcb_get_property(conn, False, st->source_window,
                             st->XdndTypeList, XCB_ATOM_ATOM, 0, 256);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(conn, cookie, NULL);
        if (reply) {
            xcb_atom_t *atoms = (xcb_atom_t *) xcb_get_property_value(reply);
            int count = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
            for (int i = 0; i < count; i++) {
                if (atoms[i] == st->text_uri_list) {
                    st->have_uri_list = True;
                    break;
                }
            }
            free(reply);
        }
    } else {
        /* Up to 3 types in data32[2..4] */
        for (int i = 2; i <= 4; i++) {
            if (cm->data.data32[i] == st->text_uri_list) {
                st->have_uri_list = True;
                break;
            }
        }
    }
}

static void
HandleXdndPosition(XdndState *st, xcb_client_message_event_t *cm)
{
    xcb_connection_t *conn = XtDisplay(st->shell);

    st->drop_x = (int)(cm->data.data32[2] >> 16);
    st->drop_y = (int)(cm->data.data32[2] & 0xFFFF);

    /* Check if a drop target widget exists under the cursor */
    Widget target = FindDropTarget(st->shell, st->drop_x, st->drop_y);
    Boolean accept = (target != NULL && st->have_uri_list);

    /* Send XdndStatus reply */
    xcb_client_message_event_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.response_type = XCB_CLIENT_MESSAGE;
    reply.window = st->source_window;
    reply.type = st->XdndStatus;
    reply.format = 32;
    reply.data.data32[0] = XtWindow(st->shell);       /* target window */
    reply.data.data32[1] = accept ? 1 : 0;            /* bit 0: accept */
    reply.data.data32[2] = 0;                          /* empty rectangle */
    reply.data.data32[3] = 0;
    reply.data.data32[4] = accept ? st->XdndActionCopy : 0;

    xcb_send_event(conn, False, st->source_window,
                   0, (const char *) &reply);
    xcb_flush(conn);
}

static void
HandleXdndDrop(XdndState *st, xcb_client_message_event_t *cm)
{
    xcb_connection_t *conn = XtDisplay(st->shell);
    xcb_timestamp_t timestamp = cm->data.data32[2];

    if (!st->have_uri_list) {
        /* Reject — send XdndFinished with accept=0 */
        xcb_client_message_event_t reply;
        memset(&reply, 0, sizeof(reply));
        reply.response_type = XCB_CLIENT_MESSAGE;
        reply.window = st->source_window;
        reply.type = st->XdndFinished;
        reply.format = 32;
        reply.data.data32[0] = XtWindow(st->shell);
        reply.data.data32[1] = 0;  /* not accepted */
        xcb_send_event(conn, False, st->source_window,
                       0, (const char *) &reply);
        xcb_flush(conn);
        return;
    }

    /* Request the selection data as text/uri-list */
    xcb_convert_selection(conn, XtWindow(st->shell),
                          st->XdndSelection, st->text_uri_list,
                          st->XdndSelection, timestamp);
    xcb_flush(conn);
}

static void
HandleXdndLeave(XdndState *st)
{
    st->source_window = 0;
    st->have_uri_list = False;
}

/* ------------------------------------------------------------------ */
/* Selection data arrival — parse URIs and dispatch to widget          */
/* ------------------------------------------------------------------ */

static char **
ParseUriList(const char *data, int len, int *out_count)
{
    /* text/uri-list: lines separated by \r\n, # lines are comments */
    char **uris = NULL;
    int count = 0;
    int capacity = 0;
    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        /* Find end of line */
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        int line_len = eol - p;
        if (line_len > 0 && *p != '#') {
            /* Strip file:// prefix */
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
            uris[count] = entry;
            count++;
        }

        /* Skip line ending */
        p = eol;
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }

    if (uris)
        uris[count] = NULL;

    *out_count = count;
    return uris;
}

static void
HandleSelectionData(Widget w, XtPointer closure,
                    xcb_atom_t *selection, xcb_atom_t *type,
                    XtPointer value, unsigned long *length, int *format)
{
    XdndState *st = (XdndState *) closure;
    xcb_connection_t *conn = XtDisplay(st->shell);

    (void) selection;
    (void) type;
    (void) format;

    /* Read the property data from the selection transfer */
    xcb_get_property_cookie_t cookie =
        xcb_get_property(conn, True, XtWindow(st->shell),
                         st->XdndSelection, XCB_ATOM_ANY, 0, 65536);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(conn, cookie, NULL);

    if (reply) {
        char *data = (char *) xcb_get_property_value(reply);
        int data_len = xcb_get_property_value_length(reply);

        if (data && data_len > 0) {
            int num_uris = 0;
            char **uris = ParseUriList(data, data_len, &num_uris);

            if (uris && num_uris > 0) {
                /* Find the widget under the drop position */
                Widget target = FindDropTarget(st->shell,
                                               st->drop_x, st->drop_y);
                if (target) {
                    /* Translate root coords to widget-local */
                    xcb_translate_coordinates_cookie_t tc =
                        xcb_translate_coordinates(conn,
                            XtScreen(st->shell)->root,
                            XtWindow(target),
                            (int16_t) st->drop_x, (int16_t) st->drop_y);
                    xcb_translate_coordinates_reply_t *tr =
                        xcb_translate_coordinates_reply(conn, tc, NULL);

                    IswDropCallbackData cb;
                    cb.uris = uris;
                    cb.num_uris = num_uris;
                    cb.x = tr ? tr->dst_x : 0;
                    cb.y = tr ? tr->dst_y : 0;
                    free(tr);

                    XtCallCallbacks(target, XtNdropCallback, (XtPointer) &cb);
                }

                /* Free URI strings */
                for (int i = 0; i < num_uris; i++)
                    XtFree(uris[i]);
                XtFree((char *) uris);
            }
        }
        free(reply);
    }

    /* Send XdndFinished */
    xcb_client_message_event_t finished;
    memset(&finished, 0, sizeof(finished));
    finished.response_type = XCB_CLIENT_MESSAGE;
    finished.window = st->source_window;
    finished.type = st->XdndFinished;
    finished.format = 32;
    finished.data.data32[0] = XtWindow(st->shell);
    finished.data.data32[1] = 1;  /* accepted */
    finished.data.data32[2] = st->XdndActionCopy;

    xcb_send_event(conn, False, st->source_window,
                   0, (const char *) &finished);
    xcb_flush(conn);

    /* Reset state */
    st->source_window = 0;
    st->have_uri_list = False;

    (void) w;
    (void) value;
    (void) length;
}

/* ------------------------------------------------------------------ */
/* Widget tree walk to find the drop target                           */
/* ------------------------------------------------------------------ */

static Widget
FindDropChild(Widget composite, int wx, int wy)
{
    /* Walk children in reverse order (top of stacking order first) */
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
            /* Check if this child has a dropCallback */
            if (XtHasCallbacks(child, XtNdropCallback) == XtCallbackHasSome)
                return child;

            /* Recurse into composite children */
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
FindDropTarget(Widget shell, int root_x, int root_y)
{
    xcb_connection_t *conn = XtDisplay(shell);

    /* Translate root coordinates to shell-local */
    xcb_translate_coordinates_cookie_t cookie =
        xcb_translate_coordinates(conn,
            XtScreen(shell)->root, XtWindow(shell),
            (int16_t) root_x, (int16_t) root_y);
    xcb_translate_coordinates_reply_t *reply =
        xcb_translate_coordinates_reply(conn, cookie, NULL);

    if (!reply)
        return NULL;

    int wx = reply->dst_x;
    int wy = reply->dst_y;
    free(reply);

    /* Check if the shell itself accepts drops */
    if (XtHasCallbacks(shell, XtNdropCallback) == XtCallbackHasSome)
        return shell;

    /* Walk the widget tree to find a drop target */
    return FindDropChild(shell, wx, wy);
}
