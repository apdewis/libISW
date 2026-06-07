/*
 * ISWPlatformDisplayXCB.c - XCB backend for the display + window platform ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformDisplayOps + IswPlatformWindowOps over XCB, and the
 * INTERNAL backend seam (_IswXcbConn / _IswXcbScreen / _IswXcbWindow / ...)
 * that not-yet-abstracted toolkit categories use to reach the native
 * connection while they await their own phase (atoms→6, color/font→4,
 * selection/cursor/grab→5, input→3).  The seam is declared only in the
 * src/-internal ISWPlatformPrivate.h — never in a public include/ISW/ header —
 * so the public API is fully decoupled from XCB even though the implementation
 * still is.  The seam shrinks phase by phase and is deleted after Phase 6.
 *
 * Handle representation (XCB backend only) — every handle is a native value
 * reinterpreted, never a separately-allocated wrapper, so the seam conversions
 * are plain casts and the handles stay one word:
 *   IswDisplay = xcb_connection_t* reinterpreted as the opaque IswDisplay.
 *                (core.display and the conn the dispatch/per-display layers
 *                carry are thus the same value — no lookup, no divergence.)
 *   IswScreen  = xcb_screen_t* reinterpreted as the opaque IswScreen.
 *   IswWindow  = xcb_window_t (a 32-bit id) reinterpreted as the opaque
 *                IswWindow pointer.  Never dereferenced.
 *
 * Phase 2 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"

/* ---- handle representation ------------------------------------------------
 *
 * Like IswWindow (the window id reinterpreted) and IswScreen (an xcb_screen_t*
 * reinterpreted), IswDisplay IS the xcb_connection_t* reinterpreted — there is
 * no separate wrapper struct.  This keeps the handle a single word, makes
 * core.display interchangeable with the connection the dispatch/per-display
 * layers carry, and turns the seam conversions into plain casts (no
 * dereference, so a stale/mis-sourced value can never crash).  A non-XCB
 * backend would map IswDisplay to its own connection object the same way.
 */

/* ---- handle <-> native conversions (the internal seam) ------------------- */

xcb_connection_t *
_IswXcbConn(IswDisplay dpy)
{
    return (xcb_connection_t *) dpy;
}

xcb_screen_t *
_IswXcbScreen(IswScreen screen)
{
    return (xcb_screen_t *) screen;
}

/* Default screen of a display, as a native xcb_screen_t.  The authoritative
   default-screen INDEX lives in the per-display table (set from xcb_connect's
   screen-number output); the toolkit's _IswGetDefaultScreen consults it.  This
   backend helper, used only to pick a root_visual at window-create, returns the
   first root, which is correct for the common single-screen case. */
xcb_screen_t *
_IswXcbDefaultScreen(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (!conn)
        return NULL;
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

xcb_window_t
_IswXcbWindow(IswWindow win)
{
    return (xcb_window_t) (uintptr_t) win;
}

IswWindow
_IswXcbWindowWrap(xcb_window_t id)
{
    return (IswWindow) (uintptr_t) id;
}

/* ---- display ops --------------------------------------------------------- */

static IswDisplay
xcb_disp_open(const char *display_name, int *default_screen)
{
    int scr = 0;
    xcb_connection_t *conn;

    conn = xcb_connect(display_name, &scr);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        if (conn)
            xcb_disconnect(conn);
        return NULL;
    }
    if (default_screen)
        *default_screen = scr;
    return (IswDisplay) conn;
}

static void
xcb_disp_close(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn) {
        xcb_flush(conn);
        xcb_disconnect(conn);
    }
}

static Boolean
xcb_disp_has_error(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? (xcb_connection_has_error(conn) != 0) : True;
}

static void
xcb_disp_flush(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn)
        xcb_flush(conn);
}

static int
xcb_disp_connection_fd(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? xcb_get_file_descriptor(conn) : -1;
}

static int
xcb_disp_screen_count(IswDisplay dpy)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    return conn ? xcb_setup_roots_length(xcb_get_setup(conn)) : 0;
}

static IswScreen
xcb_disp_screen(IswDisplay dpy, int index)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    xcb_screen_iterator_t it;
    int i;
    if (!conn || index < 0)
        return NULL;
    it = xcb_setup_roots_iterator(xcb_get_setup(conn));
    for (i = 0; i < index && it.rem; i++)
        xcb_screen_next(&it);
    return it.rem ? (IswScreen) it.data : NULL;
}

static IswWindow
xcb_disp_root_window(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? _IswXcbWindowWrap(s->root) : _IswXcbWindowWrap(0);
}

static uint32_t
xcb_disp_screen_width(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? s->width_in_pixels : 0;
}

static uint32_t
xcb_disp_screen_height(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? s->height_in_pixels : 0;
}

static void
xcb_disp_bell(IswDisplay dpy, int percent)
{
    xcb_connection_t *conn = (xcb_connection_t *) dpy;
    if (conn) {
        xcb_bell(conn, (int8_t) percent);
        xcb_flush(conn);
    }
}

static const IswPlatformDisplayOps xcb_display_ops = {
    .open           = xcb_disp_open,
    .close          = xcb_disp_close,
    .has_error      = xcb_disp_has_error,
    .flush          = xcb_disp_flush,
    .connection_fd  = xcb_disp_connection_fd,
    .screen_count   = xcb_disp_screen_count,
    .screen         = xcb_disp_screen,
    .root_window    = xcb_disp_root_window,
    .screen_width   = xcb_disp_screen_width,
    .screen_height  = xcb_disp_screen_height,
    .bell           = xcb_disp_bell,
};

/* ---- window ops ---------------------------------------------------------- */

/* Translate the neutral attribute subset into an XCB value-mask/value-list,
   in the canonical XCB ordering.  Returns the number of values written. */
static uint32_t
attrs_to_values(const IswWindowAttributes *a, unsigned int mask,
                uint32_t *out_value_mask, uint32_t values[8])
{
    uint32_t vm = 0;
    int n = 0;
    /* XCB requires values in increasing CW_* bit order. */
    if (mask & ISW_ATTR_BACK_PIXEL) {
        vm |= XCB_CW_BACK_PIXEL;        values[n++] = a->background_pixel;
    }
    if (mask & ISW_ATTR_BORDER_PIXEL) {
        vm |= XCB_CW_BORDER_PIXEL;      values[n++] = a->border_pixel;
    }
    if (mask & ISW_ATTR_OVERRIDE) {
        vm |= XCB_CW_OVERRIDE_REDIRECT; values[n++] = a->override_redirect ? 1 : 0;
    }
    if (mask & ISW_ATTR_SAVE_UNDER) {
        vm |= XCB_CW_SAVE_UNDER;        values[n++] = a->save_under ? 1 : 0;
    }
    if (mask & ISW_ATTR_EVENT_MASK) {
        vm |= XCB_CW_EVENT_MASK;        values[n++] = a->event_mask;
    }
    *out_value_mask = vm;
    return (uint32_t) n;
}

static IswWindow
xcb_win_alloc_id(IswDisplay dpy)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    if (!conn)
        return _IswXcbWindowWrap(0);
    return _IswXcbWindowWrap(xcb_generate_id(conn));
}

static IswWindow
xcb_win_create(IswDisplay dpy, IswWindow parent,
               const IswWindowGeometry *geom,
               const IswWindowAttributes *attrs)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    xcb_screen_t *s = _IswXcbDefaultScreen(dpy);
    xcb_window_t id;
    uint32_t value_mask = 0, values[8];
    uint32_t nv;

    if (!c || !s)
        return _IswXcbWindowWrap(0);

    id = xcb_generate_id(c);
    nv = attrs ? attrs_to_values(attrs, ISW_ATTR_BACK_PIXEL |
                                 ISW_ATTR_BORDER_PIXEL | ISW_ATTR_OVERRIDE |
                                 ISW_ATTR_SAVE_UNDER | ISW_ATTR_EVENT_MASK,
                                 &value_mask, values)
                : 0;
    (void) nv;
    xcb_create_window(c, XCB_COPY_FROM_PARENT, id, _IswXcbWindow(parent),
                      (int16_t) geom->x, (int16_t) geom->y,
                      (uint16_t) geom->width, (uint16_t) geom->height,
                      (uint16_t) geom->border_width,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      s->root_visual, value_mask, values);
    return _IswXcbWindowWrap(id);
}

static void
xcb_win_destroy(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_destroy_window(c, _IswXcbWindow(win));
}

static void
xcb_win_map(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_map_window(c, _IswXcbWindow(win));
}

static void
xcb_win_unmap(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_unmap_window(c, _IswXcbWindow(win));
}

static void
xcb_win_reparent(IswDisplay dpy, IswWindow win, IswWindow new_parent,
                 int32_t x, int32_t y)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_reparent_window(c, _IswXcbWindow(win), _IswXcbWindow(new_parent),
                            (int16_t) x, (int16_t) y);
}

static void
xcb_win_configure(IswDisplay dpy, IswWindow win,
                  const IswWindowGeometry *geom, unsigned int mask,
                  IswStackMode stack, IswWindow sibling)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    uint32_t cm = 0, values[7];
    int n = 0;

    if (!c)
        return;
    /* XCB requires values in increasing CONFIG_WINDOW_* bit order. */
    if (mask & ISW_CONFIG_X)      { cm |= XCB_CONFIG_WINDOW_X;
                                    values[n++] = (uint32_t)(int32_t) geom->x; }
    if (mask & ISW_CONFIG_Y)      { cm |= XCB_CONFIG_WINDOW_Y;
                                    values[n++] = (uint32_t)(int32_t) geom->y; }
    if (mask & ISW_CONFIG_WIDTH)  { cm |= XCB_CONFIG_WINDOW_WIDTH;
                                    values[n++] = geom->width; }
    if (mask & ISW_CONFIG_HEIGHT) { cm |= XCB_CONFIG_WINDOW_HEIGHT;
                                    values[n++] = geom->height; }
    if (mask & ISW_CONFIG_BORDER) { cm |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
                                    values[n++] = geom->border_width; }
    if ((mask & ISW_CONFIG_STACK) && sibling != NULL) {
        cm |= XCB_CONFIG_WINDOW_SIBLING; values[n++] = _IswXcbWindow(sibling);
    }
    if ((mask & ISW_CONFIG_STACK) && stack != ISW_STACK_NONE) {
        cm |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[n++] = (stack == ISW_STACK_ABOVE) ? XCB_STACK_MODE_ABOVE
                                                  : XCB_STACK_MODE_BELOW;
    }
    if (cm)
        xcb_configure_window(c, _IswXcbWindow(win), (uint16_t) cm, values);
}

static void
xcb_win_change_attributes(IswDisplay dpy, IswWindow win,
                          const IswWindowAttributes *attrs, unsigned int mask)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    uint32_t value_mask = 0, values[8];
    if (!c || !attrs)
        return;
    attrs_to_values(attrs, mask, &value_mask, values);
    if (value_mask)
        xcb_change_window_attributes(c, _IswXcbWindow(win), value_mask, values);
}

static void
xcb_win_clear_area(IswDisplay dpy, IswWindow win,
                   int16_t x, int16_t y, uint16_t w, uint16_t h,
                   Boolean generate_expose)
{
    xcb_connection_t *c = _IswXcbConn(dpy);
    if (c)
        xcb_clear_area(c, generate_expose ? 1 : 0, _IswXcbWindow(win),
                       x, y, w, h);
}

static IswWindowId
xcb_win_window_id(IswWindow win)
{
    return (IswWindowId) _IswXcbWindow(win);
}

static IswWindow
xcb_win_window_from_id(IswWindowId id)
{
    return _IswXcbWindowWrap((xcb_window_t) id);
}

static const IswPlatformWindowOps xcb_window_ops = {
    .alloc_id           = xcb_win_alloc_id,
    .create             = xcb_win_create,
    .destroy            = xcb_win_destroy,
    .map                = xcb_win_map,
    .unmap              = xcb_win_unmap,
    .reparent           = xcb_win_reparent,
    .configure          = xcb_win_configure,
    .change_attributes  = xcb_win_change_attributes,
    .clear_area         = xcb_win_clear_area,
    .window_id          = xcb_win_window_id,
    .window_from_id     = xcb_win_window_from_id,
};

/* ---- backend vtable + dispatcher ----------------------------------------- */

const IswPlatformOps isw_platform_xcb_ops = {
    .display   = &xcb_display_ops,
    .window    = &xcb_window_ops,
    .event     = NULL,   /* Phase 1 translator is standalone for now */
    .input     = NULL,
    .selection = NULL,
    .color     = NULL,
    .font      = NULL,
    .cursor    = NULL,
};

const IswPlatformOps *
_IswPlatformGetOps(void)
{
    return &isw_platform_xcb_ops;
}

/* Neutral event-loop fd accessor (replaces the ConnectionNumber XCB macro),
   dispatched through the active backend's display vtable. */
int
_IswPlatformConnectionFd(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswPlatformGetOps();
    if (ops && ops->display && ops->display->connection_fd)
        return ops->display->connection_fd(dpy);
    return -1;
}
