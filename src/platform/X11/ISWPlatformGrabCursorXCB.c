/*
 * ISWPlatformGrabCursorXCB.c - XCB backend for cursor, grab, and selection ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformCursorOps (glyph/themed cursor create, set-on-window,
 * free), IswPlatformGrabOps (passive/active pointer/keyboard/button/key grabs),
 * and the three pure-selection protocol verbs of IswPlatformSelectionOps
 * (set/get owner, convert).  Cursor handles are value handles (each IS the
 * native xcb_cursor_t), so the seam conversions below are plain casts.  The
 * selection/target/property atoms stay xcb_atom_t until Phase 6 abstracts atoms;
 * the property-exchange machinery the convert drives stays in Selection.c.
 *
 * Phase 5 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).  The only TU
 * (besides the other backend TUs) that issues xcb grab / cursor / selection-verb
 * requests for these paths.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#include "ISWContextI.h"
#include "uthash.h"

/* ---- cursor value handle (the internal seam) ----------------------------- */

xcb_cursor_t
_IswXcbCursor(IswCursor cursor)
{
    return (xcb_cursor_t) cursor;
}

IswCursor
_IswXcbCursorWrap(xcb_cursor_t cursor)
{
    return (IswCursor) cursor;
}

/* ---- cursor ops ---------------------------------------------------------- */

/* Glyph cursor from the standard "cursor" font (fallback when no theme). */
static IswCursor
xcb_cur_create_glyph(IswDisplay dpy, unsigned int shape)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    static xcb_font_t cursor_font = XCB_NONE;
    xcb_cursor_t cursor;

    if (!conn)
        return 0;
    if (cursor_font == XCB_NONE) {
        cursor_font = xcb_generate_id(conn);
        xcb_open_font(conn, cursor_font, strlen("cursor"), "cursor");
    }
    cursor = xcb_generate_id(conn);
    xcb_create_glyph_cursor(conn, cursor,
                            cursor_font, cursor_font,
                            shape,          /* source char */
                            shape + 1,      /* mask char */
                            0, 0, 0,        /* foreground RGB (black) */
                            65535, 65535, 65535); /* background RGB (white) */
    return _IswXcbCursorWrap(cursor);
}

/* Theme-aware named cursor via xcb-cursor; glyph fallback. */
static IswCursor
xcb_cur_load_named(IswDisplay dpy, IswScreen screen,
                   const char *name, unsigned int fallback_shape)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_screen_t *scr = _IswXcbScreen(screen);
    xcb_cursor_context_t *ctx;
    xcb_cursor_t cursor;

    if (!conn || !scr)
        return 0;
    if (xcb_cursor_context_new(conn, scr, &ctx) < 0)
        return xcb_cur_create_glyph(dpy, fallback_shape);

    cursor = xcb_cursor_load_cursor(ctx, name);
    xcb_cursor_context_free(ctx);

    if (cursor == XCB_CURSOR_NONE)
        return xcb_cur_create_glyph(dpy, fallback_shape);

    return _IswXcbCursorWrap(cursor);
}

static void
xcb_cur_set_window_cursor(IswDisplay dpy, IswWindow win, IswCursor cursor)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    uint32_t value = _IswXcbCursor(cursor);

    if (!conn || _IswXcbWindow(win) == XCB_NONE)
        return;
    xcb_change_window_attributes(conn, _IswXcbWindow(win),
                                 XCB_CW_CURSOR, &value);
}

static void
xcb_cur_free_cursor(IswDisplay dpy, IswCursor cursor)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);

    if (!conn || cursor == 0 || _IswXcbCursor(cursor) == XCB_CURSOR_NONE)
        return;
    xcb_free_cursor(conn, _IswXcbCursor(cursor));
}

/* ---- grab ops ------------------------------------------------------------ */

static int
xcb_grb_grab_pointer(IswDisplay dpy, IswWindow grab_window,
                     Boolean owner_events, unsigned int event_mask,
                     int pointer_mode, int keyboard_mode,
                     IswWindow confine_to, IswCursor cursor, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_grab_pointer_cookie_t cookie;
    xcb_grab_pointer_reply_t *reply;
    int status;

    if (!conn)
        return -1;
    cookie = xcb_grab_pointer(conn, (uint8_t) owner_events,
                              _IswXcbWindow(grab_window),
                              (uint16_t) event_mask,
                              (uint8_t) pointer_mode, (uint8_t) keyboard_mode,
                              _IswXcbWindow(confine_to), _IswXcbCursor(cursor),
                              (xcb_timestamp_t) time);
    reply = xcb_grab_pointer_reply(conn, cookie, NULL);
    if (!reply)
        return -1;
    status = reply->status;
    free(reply);
    return status;
}

static void
xcb_grb_ungrab_pointer(IswDisplay dpy, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_ungrab_pointer(conn, (xcb_timestamp_t) time);
}

static int
xcb_grb_grab_keyboard(IswDisplay dpy, IswWindow grab_window,
                      Boolean owner_events, int pointer_mode,
                      int keyboard_mode, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_grab_keyboard_cookie_t cookie;
    xcb_grab_keyboard_reply_t *reply;
    int status;

    if (!conn)
        return -1;
    cookie = xcb_grab_keyboard(conn, (uint8_t) owner_events,
                               _IswXcbWindow(grab_window),
                               (xcb_timestamp_t) time,
                               (uint8_t) pointer_mode, (uint8_t) keyboard_mode);
    reply = xcb_grab_keyboard_reply(conn, cookie, NULL);
    if (!reply)
        return -1;
    status = reply->status;
    free(reply);
    return status;
}

static void
xcb_grb_ungrab_keyboard(IswDisplay dpy, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_ungrab_keyboard(conn, (xcb_timestamp_t) time);
}

static void
xcb_grb_grab_button(IswDisplay dpy, IswWindow grab_window, int button,
                    unsigned int modifiers, Boolean owner_events,
                    unsigned int event_mask, int pointer_mode,
                    int keyboard_mode, IswWindow confine_to, IswCursor cursor)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_grab_button(conn, (uint8_t) owner_events, _IswXcbWindow(grab_window),
                    (uint16_t) event_mask,
                    (uint8_t) pointer_mode, (uint8_t) keyboard_mode,
                    _IswXcbWindow(confine_to), _IswXcbCursor(cursor),
                    (uint8_t) button, (uint16_t) modifiers);
}

static void
xcb_grb_ungrab_button(IswDisplay dpy, IswWindow grab_window, int button,
                      unsigned int modifiers)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_ungrab_button(conn, (uint8_t) button, _IswXcbWindow(grab_window),
                      (uint16_t) modifiers);
}

static void
xcb_grb_grab_key(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                 unsigned int modifiers, Boolean owner_events,
                 int pointer_mode, int keyboard_mode)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_grab_key(conn, (uint8_t) owner_events, _IswXcbWindow(grab_window),
                 (uint16_t) modifiers, (xcb_keycode_t) keycode,
                 (uint8_t) pointer_mode, (uint8_t) keyboard_mode);
}

static void
xcb_grb_ungrab_key(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                   unsigned int modifiers)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_ungrab_key(conn, (xcb_keycode_t) keycode, _IswXcbWindow(grab_window),
                   (uint16_t) modifiers);
}

static void
xcb_grb_allow_events(IswDisplay dpy, int mode, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_allow_events(conn, (uint8_t) mode, (xcb_timestamp_t) time);
}

static void
xcb_grb_change_active_pointer_grab(IswDisplay dpy, IswCursor cursor,
                                   IswTime time, unsigned int event_mask)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_change_active_pointer_grab(conn, _IswXcbCursor(cursor),
                                   (xcb_timestamp_t) time,
                                   (uint16_t) event_mask);
}

/* ---- selection ops ------------------------------------------------------- */

/* On X11 an IswSelectionId is numerically an interned atom. */

static IswSelectionId
xcb_sel_intern_name(IswDisplay dpy, const char *name, Boolean only_if_exists)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    IswSelectionId id = ISW_SELECTION_NONE;

    if (!conn || !name)
        return ISW_SELECTION_NONE;
    cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0,
                             (uint16_t) strlen(name), name);
    reply = xcb_intern_atom_reply(conn, cookie, NULL);
    if (reply) {
        id = (IswSelectionId) reply->atom;
        free(reply);
    }
    return id;
}

static Boolean
xcb_sel_name_of(IswDisplay dpy, IswSelectionId id, char *buf, size_t buflen)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_atom_name_cookie_t cookie;
    xcb_get_atom_name_reply_t *reply;
    int len;

    if (!conn || id == ISW_SELECTION_NONE || !buf || buflen == 0)
        return False;
    cookie = xcb_get_atom_name(conn, (xcb_atom_t) id);
    reply = xcb_get_atom_name_reply(conn, cookie, NULL);
    if (!reply)
        return False;
    len = xcb_get_atom_name_name_length(reply);
    if ((size_t) len >= buflen)
        len = (int) buflen - 1;
    memcpy(buf, xcb_get_atom_name_name(reply), (size_t) len);
    buf[len] = '\0';
    free(reply);
    return True;
}

static void
xcb_sel_set_owner(IswDisplay dpy, IswWindow owner, IswSelectionId selection,
                  IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_set_selection_owner(conn, _IswXcbWindow(owner), (xcb_atom_t) selection,
                            (xcb_timestamp_t) time);
}

static IswWindow
xcb_sel_get_owner(IswDisplay dpy, IswSelectionId selection)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;
    xcb_window_t owner = XCB_NONE;

    if (!conn)
        return _IswXcbWindowWrap(XCB_NONE);
    cookie = xcb_get_selection_owner(conn, (xcb_atom_t) selection);
    reply = xcb_get_selection_owner_reply(conn, cookie, NULL);
    if (reply) {
        owner = reply->owner;
        free(reply);
    }
    return _IswXcbWindowWrap(owner);
}

static void
xcb_sel_convert(IswDisplay dpy, IswWindow requestor, IswSelectionId selection,
                IswSelectionId target, IswSelectionId property, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_convert_selection(conn, _IswXcbWindow(requestor),
                          (xcb_atom_t) selection, (xcb_atom_t) target,
                          (xcb_atom_t) property, (xcb_timestamp_t) time);
}

static Boolean
xcb_sel_decode_event(IswDisplay dpy, const void *native, IswSelectionEvent *out)
{
    const xcb_generic_event_t *ev = (const xcb_generic_event_t *) native;

    (void) dpy;
    if (!out)
        return False;
    memset(out, 0, sizeof(*out));
    out->kind = ISW_SEL_EVENT_OTHER;
    if (!ev)
        return False;

    switch (ev->response_type & ~0x80) {
    case XCB_SELECTION_CLEAR: {
        const xcb_selection_clear_event_t *e =
            (const xcb_selection_clear_event_t *) ev;
        out->kind      = ISW_SEL_EVENT_CLEAR;
        out->selection = (IswSelectionId) e->selection;
        out->time      = (IswTime) e->time;
        out->serial    = e->sequence;
        return True;
    }
    case XCB_SELECTION_REQUEST: {
        const xcb_selection_request_event_t *e =
            (const xcb_selection_request_event_t *) ev;
        out->kind             = ISW_SEL_EVENT_REQUEST;
        out->selection        = (IswSelectionId) e->selection;
        out->target           = (IswSelectionId) e->target;
        out->property         = (IswSelectionId) e->property;
        out->requestor        = _IswXcbWindowWrap(e->requestor);
        out->time             = (IswTime) e->time;
        out->request.requestor = _IswXcbWindowWrap(e->requestor);
        out->request.owner     = _IswXcbWindowWrap(e->owner);
        out->request.selection = (IswSelectionId) e->selection;
        out->request.target    = (IswSelectionId) e->target;
        out->request.property  = (IswSelectionId) e->property;
        out->request.time      = (IswTime) e->time;
        return True;
    }
    case XCB_SELECTION_NOTIFY: {
        const xcb_selection_notify_event_t *e =
            (const xcb_selection_notify_event_t *) ev;
        out->kind      = ISW_SEL_EVENT_NOTIFY;
        out->selection = (IswSelectionId) e->selection;
        out->target    = (IswSelectionId) e->target;
        out->property  = (IswSelectionId) e->property;
        out->requestor = _IswXcbWindowWrap(e->requestor);
        out->time      = (IswTime) e->time;
        return True;
    }
    case XCB_PROPERTY_NOTIFY: {
        const xcb_property_notify_event_t *e =
            (const xcb_property_notify_event_t *) ev;
        out->kind      = (e->state == XCB_PROPERTY_DELETE)
                             ? ISW_SEL_EVENT_PROP_DELETE
                             : ISW_SEL_EVENT_PROP_NEW;
        out->property  = (IswSelectionId) e->atom;
        out->requestor = _IswXcbWindowWrap(e->window);
        out->time      = (IswTime) e->time;
        return True;
    }
    default:
        return False;
    }
}

static void
xcb_sel_send_notify(IswDisplay dpy, const IswSelectionRequest *req,
                    IswSelectionId property)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_selection_notify_event_t ev;

    if (!conn || !req)
        return;
    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_SELECTION_NOTIFY;
    ev.requestor     = _IswXcbWindow(req->requestor);
    ev.selection     = (xcb_atom_t) req->selection;
    ev.target        = (xcb_atom_t) req->target;
    ev.property      = (xcb_atom_t) property;
    ev.time          = (xcb_timestamp_t) req->time;
    xcb_send_event(conn, 0, _IswXcbWindow(req->requestor), 0, (const char *) &ev);
}

static unsigned long
xcb_sel_max_transfer_bytes(IswDisplay dpy)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    unsigned long maxreq;

    if (!conn)
        return 0;
    maxreq = (unsigned long) xcb_get_maximum_request_length(conn);
    if (65536 < maxreq)
        maxreq = 65536;
    return (maxreq << 2) - 100;
}

/* ---- vtables ------------------------------------------------------------- */

const IswPlatformCursorOps isw_platform_xcb_cursor_ops = {
    .create_glyph      = xcb_cur_create_glyph,
    .load_named        = xcb_cur_load_named,
    .set_window_cursor = xcb_cur_set_window_cursor,
    .free_cursor       = xcb_cur_free_cursor,
};

const IswPlatformGrabOps isw_platform_xcb_grab_ops = {
    .grab_pointer    = xcb_grb_grab_pointer,
    .ungrab_pointer  = xcb_grb_ungrab_pointer,
    .grab_keyboard   = xcb_grb_grab_keyboard,
    .ungrab_keyboard = xcb_grb_ungrab_keyboard,
    .grab_button     = xcb_grb_grab_button,
    .ungrab_button   = xcb_grb_ungrab_button,
    .grab_key        = xcb_grb_grab_key,
    .ungrab_key      = xcb_grb_ungrab_key,
    .allow_events    = xcb_grb_allow_events,
    .change_active_pointer_grab = xcb_grb_change_active_pointer_grab,
};

const IswPlatformSelectionOps isw_platform_xcb_selection_ops = {
    .intern_name        = xcb_sel_intern_name,
    .name_of            = xcb_sel_name_of,
    .set_owner          = xcb_sel_set_owner,
    .get_owner          = xcb_sel_get_owner,
    .convert            = xcb_sel_convert,
    .decode_event       = xcb_sel_decode_event,
    .send_notify        = xcb_sel_send_notify,
    .max_transfer_bytes = xcb_sel_max_transfer_bytes,
};

/* ---- resource-id → data context table ------------------------------------ *
 *
 * XCB replacement for Xlib's XSaveContext/XFindContext/XDeleteContext: a
 * generic {resource-id, context} → data side table (uthash).  It lives here
 * because its only users are the X11 selection-transfer bookkeeping in
 * Selection.c (state attached to foreign requestor windows/atoms the toolkit
 * does not own) and the X11 XDND backend — both X11-protocol concerns.  The
 * IswDisplay parameter is carried only for Xlib API shape; the table is keyed
 * purely on {id, context}.
 */

typedef struct _IswContextKey {
    XID      id;         /* window or resource id */
    XContext context;    /* context identifier    */
} IswContextKey;

typedef struct _IswContextEntry {
    IswContextKey  key;  /* composite key (must be first for HASH_FIND) */
    IswPointer     data;
    UT_hash_handle hh;
} IswContextEntry;

static IswContextEntry *context_table = NULL;
static XContext         next_context_id = 1;

XContext
IswUniqueContext(void)
{
    return next_context_id++;
}

int
IswSaveContext(IswDisplay dpy _X_UNUSED, XID id, XContext context, IswPointer data)
{
    IswContextEntry *entry;
    IswContextKey key;

    key.id = id;
    key.context = context;

    HASH_FIND(hh, context_table, &key, sizeof(IswContextKey), entry);
    if (entry != NULL) {
        entry->data = data;
        return 0;
    }

    entry = (IswContextEntry *) malloc(sizeof(IswContextEntry));
    if (entry == NULL)
        return 1;

    entry->key = key;
    entry->data = data;
    HASH_ADD(hh, context_table, key, sizeof(IswContextKey), entry);
    return 0;
}

int
IswFindContext(IswDisplay dpy _X_UNUSED, XID id, XContext context,
               IswPointer *data_return)
{
    IswContextEntry *entry;
    IswContextKey key;

    key.id = id;
    key.context = context;

    HASH_FIND(hh, context_table, &key, sizeof(IswContextKey), entry);
    if (entry != NULL) {
        *data_return = entry->data;
        return 0;
    }
    return 1;
}

int
IswDeleteContext(IswDisplay dpy _X_UNUSED, XID id, XContext context)
{
    IswContextEntry *entry;
    IswContextKey key;

    key.id = id;
    key.context = context;

    HASH_FIND(hh, context_table, &key, sizeof(IswContextKey), entry);
    if (entry != NULL) {
        HASH_DEL(context_table, entry);
        free(entry);
        return 0;
    }
    return 1;
}
