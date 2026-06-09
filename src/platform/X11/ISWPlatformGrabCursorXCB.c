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

/* ---- selection ops ------------------------------------------------------- */

static void
xcb_sel_set_owner(IswDisplay dpy, IswWindow owner, xcb_atom_t selection,
                  IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_set_selection_owner(conn, _IswXcbWindow(owner), selection,
                            (xcb_timestamp_t) time);
}

static IswWindow
xcb_sel_get_owner(IswDisplay dpy, xcb_atom_t selection)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;
    xcb_window_t owner = XCB_NONE;

    if (!conn)
        return _IswXcbWindowWrap(XCB_NONE);
    cookie = xcb_get_selection_owner(conn, selection);
    reply = xcb_get_selection_owner_reply(conn, cookie, NULL);
    if (reply) {
        owner = reply->owner;
        free(reply);
    }
    return _IswXcbWindowWrap(owner);
}

static void
xcb_sel_convert(IswDisplay dpy, IswWindow requestor, xcb_atom_t selection,
                xcb_atom_t target, xcb_atom_t property, IswTime time)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return;
    xcb_convert_selection(conn, _IswXcbWindow(requestor), selection, target,
                          property, (xcb_timestamp_t) time);
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
};

const IswPlatformSelectionOps isw_platform_xcb_selection_ops = {
    .set_owner = xcb_sel_set_owner,
    .get_owner = xcb_sel_get_owner,
    .convert   = xcb_sel_convert,
};
