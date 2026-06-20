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
#include <xcb/present.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#include "IntrinsicI.h"
#include <ISW/InitialI.h>
#include "ISWPlatformPrivate.h"
#include "ISWPlatformDisplayXCB.h"
#include "ISWRenderCairoXCB.h" 

xcb_connection_t *
_IswXcbConn(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy; //convert opaque handle type to backend specific
    return idx->conn;
}

xcb_screen_t *
_IswXcbScreen(IswScreen screen)
{
    return (xcb_screen_t *) screen;
}

xcb_screen_t *
_IswXcbDefaultScreen(IswDisplay dpy)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    if (!idx->conn)
        return NULL;
    return xcb_setup_roots_iterator(xcb_get_setup(idx->conn)).data;
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
    IswDisplayXCB *xcbDisplay;
    xcb_connection_t *conn;

    conn = xcb_connect(display_name, &scr);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        if (conn)
            xcb_disconnect(conn);
        return NULL;
    }
    if (default_screen)
        *default_screen = scr;

    xcbDisplay = (IswDisplayXCB *)IswMalloc(sizeof(IswDisplayXCB));
    if(xcbDisplay != NULL) {
        memset(xcbDisplay, 0, sizeof(*xcbDisplay));
        xcbDisplay->conn = conn;
        _IswXcbAllocWWTable((IswDisplay) xcbDisplay);
        return (IswDisplay) xcbDisplay;
    } else {
        return (IswDisplay) NULL;
    }
}

static void
xcb_disp_close(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    _IswXcbFreeWWTable(dpy);
    if (priv->conn) {
        if (priv->blit_gc)
            xcb_free_gc(priv->conn, priv->blit_gc);
        xcb_flush(priv->conn);
        xcb_disconnect(priv->conn);
    }
}

static Boolean
xcb_disp_has_error(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    return priv->conn ? (xcb_connection_has_error(priv->conn) != 0) : True;
}

static void
xcb_disp_flush(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn)
        xcb_flush(priv->conn);
}

static void
xcb_disp_sync(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn) {
        /* A round-trip request blocks until the server has processed all
           prior requests. */
        xcb_get_input_focus_reply_t *r =
            xcb_get_input_focus_reply(priv->conn,
                                      xcb_get_input_focus(priv->conn), NULL);
        free(r);
    }
}

static int
xcb_disp_connection_fd(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    return priv->conn ? xcb_get_file_descriptor(priv->conn) : -1;
}

static int
xcb_disp_screen_count(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    return priv->conn ? xcb_setup_roots_length(xcb_get_setup(priv->conn)) : 0;
}

static IswScreen
xcb_disp_screen(IswDisplay dpy, int index)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    xcb_screen_iterator_t it;
    int i;
    if (!priv->conn || index < 0)
        return NULL;
    it = xcb_setup_roots_iterator(xcb_get_setup(priv->conn));
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

static IswColormap
xcb_disp_screen_default_colormap(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return _IswXcbColormapWrap(s ? s->default_colormap : 0);
}

static int
xcb_disp_screen_depth(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? (int) s->root_depth : 0;
}

static unsigned long
xcb_disp_screen_black_pixel(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? (unsigned long) s->black_pixel : 0;
}

static unsigned long
xcb_disp_screen_white_pixel(IswScreen screen)
{
    xcb_screen_t *s = (xcb_screen_t *) screen;
    return s ? (unsigned long) s->white_pixel : 0;
}

static void
xcb_disp_bell(IswDisplay dpy, int percent)
{
   IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn) {
        //#TODO non bell error indication
    }
}

static const char *
xcb_disp_vendor(IswDisplay dpy)
{
    static char buf[256];
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    const xcb_setup_t *setup;
    int len;
    const char *v;

    if (!priv || !priv->conn)
        return "";
    setup = xcb_get_setup(priv->conn);
    if (!setup)
        return "";
    /* xcb_setup_vendor is length-prefixed, not NUL-terminated. */
    len = xcb_setup_vendor_length(setup);
    v = xcb_setup_vendor(setup);
    if (len >= (int) sizeof(buf))
        len = (int) sizeof(buf) - 1;
    memcpy(buf, v, (size_t) len);
    buf[len] = '\0';
    return buf;
}

static void *
xcb_disp_native_display(IswDisplay dpy)
{
    return (void *) _IswXcbConn(dpy);
}

static void *
xcb_disp_native_screen(IswScreen screen)
{
    return (void *) _IswXcbScreen(screen);
}

static void *
xcb_disp_native_window(IswWindow win)
{
    return (void *) (uintptr_t) _IswXcbWindow(win);
}

static const IswPlatformDisplayOps xcb_display_ops = {
    .open           = xcb_disp_open,
    .close          = xcb_disp_close,
    .has_error      = xcb_disp_has_error,
    .flush          = xcb_disp_flush,
    .sync           = xcb_disp_sync,
    .connection_fd  = xcb_disp_connection_fd,
    .screen_count   = xcb_disp_screen_count,
    .screen         = xcb_disp_screen,
    .root_window    = xcb_disp_root_window,
    .screen_width   = xcb_disp_screen_width,
    .screen_height  = xcb_disp_screen_height,
    .screen_default_colormap = xcb_disp_screen_default_colormap,
    .screen_depth   = xcb_disp_screen_depth,
    .screen_black_pixel = xcb_disp_screen_black_pixel,
    .screen_white_pixel = xcb_disp_screen_white_pixel,
    .bell           = xcb_disp_bell,
    .vendor         = xcb_disp_vendor,
    .native_display = xcb_disp_native_display,
    .native_screen  = xcb_disp_native_screen,
    .native_window  = xcb_disp_native_window,
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
    /* XCB requires values in increasing CW_* bit order:
       BACK_PIXEL(1) < BORDER_PIXEL(3) < BIT_GRAVITY(4) < OVERRIDE_REDIRECT(9)
       < SAVE_UNDER(10) < EVENT_MASK(11) < COLORMAP(13). */
    if (mask & ISW_ATTR_BACK_PIXEL) {
        vm |= XCB_CW_BACK_PIXEL;        values[n++] = a->background_pixel;
    }
    if (mask & ISW_ATTR_BORDER_PIXEL) {
        vm |= XCB_CW_BORDER_PIXEL;      values[n++] = a->border_pixel;
    }
    if (mask & ISW_ATTR_BIT_GRAVITY) {
        vm |= XCB_CW_BIT_GRAVITY;       values[n++] = a->bit_gravity_nw
                                                     ? XCB_GRAVITY_NORTH_WEST
                                                     : XCB_GRAVITY_BIT_FORGET;
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
    if (mask & ISW_ATTR_COLORMAP) {
        vm |= XCB_CW_COLORMAP;          values[n++] = (uint32_t) _IswXcbColormap(a->colormap);
    }
    *out_value_mask = vm;
    return (uint32_t) n;
}

static IswWindow
xcb_win_alloc_id(IswDisplay dpy)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (!priv->conn)
        return _IswXcbWindowWrap(0);
    return _IswXcbWindowWrap(xcb_generate_id(priv->conn));
}

/* Shared window create: honors visual/colormap/depth/bit-gravity from attrs.
   OVERRIDE_REDIRECT and SAVE_UNDER are applied here too (no post-create change
   needed).  parent_win is the native parent. */
static xcb_window_t
xcb_create_window_full(xcb_connection_t *conn, xcb_screen_t *s,
                       xcb_window_t parent_win,
                       const IswWindowGeometry *geom,
                       const IswWindowAttributes *attrs,
                       unsigned int window_class)
{
    xcb_window_t id = xcb_generate_id(conn);
    uint32_t value_mask = 0, values[8];
    uint8_t depth = XCB_COPY_FROM_PARENT;
    xcb_visualid_t visual = s ? s->root_visual : XCB_COPY_FROM_PARENT;

    if (attrs) {
        unsigned int amask = ISW_ATTR_BACK_PIXEL | ISW_ATTR_BORDER_PIXEL |
                             ISW_ATTR_OVERRIDE | ISW_ATTR_SAVE_UNDER |
                             ISW_ATTR_EVENT_MASK | ISW_ATTR_BIT_GRAVITY;
        if (attrs->colormap) amask |= ISW_ATTR_COLORMAP;
        attrs_to_values(attrs, amask, &value_mask, values);
        if (attrs->depth) depth = (uint8_t) attrs->depth;
        if (attrs->visual) visual = (xcb_visualid_t) attrs->visual;
    }
    xcb_create_window(conn, depth, id, parent_win,
                      (int16_t) geom->x, (int16_t) geom->y,
                      (uint16_t) geom->width, (uint16_t) geom->height,
                      (uint16_t) geom->border_width,
                      (uint16_t) window_class, visual, value_mask, values);
    return id;
}

static IswWindow
xcb_win_create(IswDisplay dpy, IswWindow parent,
               const IswWindowGeometry *geom,
               const IswWindowAttributes *attrs,
               unsigned int window_class)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    xcb_screen_t *s = _IswXcbDefaultScreen(dpy);

    if (!priv->conn || !s)
        return _IswXcbWindowWrap(0);

    return _IswXcbWindowWrap(
        xcb_create_window_full(priv->conn, s, _IswXcbWindow(parent),
                               geom, attrs, window_class));
}

static void
xcb_win_destroy(IswDisplay dpy, IswWindow win)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;

    /* Drop the window->widget association before destroying the server window.
       Otherwise the entry lingers pointing at a widget about to be freed, so
       event translation (IswWindowToWidget) and the queued-event liveness check
       can hand back a dangling widget.  Must run while the widget is still
       alive: IswUnregisterDrawable dereferences it to locate the table slot. */
    IswUnregisterDrawable(dpy, win);

    if (priv->conn)
        xcb_destroy_window(priv->conn, _IswXcbWindow(win));
}

static void
xcb_win_map(IswDisplay dpy, IswWindow win)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn)
        xcb_map_window(priv->conn, _IswXcbWindow(win));
}

static void
xcb_win_unmap(IswDisplay dpy, IswWindow win)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn)
        xcb_unmap_window(priv->conn, _IswXcbWindow(win));
}

static void
xcb_win_reparent(IswDisplay dpy, IswWindow win, IswWindow new_parent,
                 int32_t x, int32_t y)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn)
        xcb_reparent_window(priv->conn, _IswXcbWindow(win), _IswXcbWindow(new_parent),
                            (int16_t) x, (int16_t) y);
}

static void
xcb_win_configure(IswDisplay dpy, IswWindow win,
                  const IswWindowGeometry *geom, unsigned int mask,
                  IswStackMode stack, IswWindow sibling)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    uint32_t cm = 0, values[7];
    int n = 0;

    if (!priv->conn)
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
        xcb_configure_window(priv->conn, _IswXcbWindow(win), (uint16_t) cm, values);
}

static void
xcb_win_change_attributes(IswDisplay dpy, IswWindow win,
                          const IswWindowAttributes *attrs, unsigned int mask)
{
    IswDisplayXCB *idx = (IswDisplayXCB *) dpy;
    uint32_t value_mask = 0, values[8];
    if (!idx->conn || !attrs)
        return;
    attrs_to_values(attrs, mask, &value_mask, values);
    if (value_mask)
        xcb_change_window_attributes(idx->conn, _IswXcbWindow(win), value_mask, values);
}

static void
xcb_win_clear_area(IswDisplay dpy, IswWindow win,
                   int16_t x, int16_t y, uint16_t w, uint16_t h,
                   Boolean generate_expose)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    if (priv->conn)
        xcb_clear_area(priv->conn, generate_expose ? 1 : 0, _IswXcbWindow(win),
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

static Boolean
xcb_win_window_viewable(IswDisplay dpy, IswWindow win)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_get_window_attributes_cookie_t c;
    xcb_get_window_attributes_reply_t *r;
    Boolean viewable;
    if (!conn)
        return False;
    c = xcb_get_window_attributes(conn, _IswXcbWindow(win));
    r = xcb_get_window_attributes_reply(conn, c, NULL);
    viewable = (r && r->map_state == XCB_MAP_STATE_VIEWABLE);
    free(r);
    return viewable;
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
    .window_viewable    = xcb_win_window_viewable,
};

/* ---- root surface ops ---------------------------------------------------- */

/* Create the WM-managed top-level window for a (windowless) shell: a child of
   the screen root with the shell's visual/colormap/depth/event-mask. */
static IswWindow
xcb_root_create(IswDisplay dpy, IswScreen screen,
                const IswWindowGeometry *geom, const IswWindowAttributes *attrs)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    xcb_screen_t *s = _IswXcbScreen(screen);
    xcb_window_t win;
    if (!priv->conn || !s)
        return _IswXcbWindowWrap(0);
    win = xcb_create_window_full(priv->conn, s, s->root, geom, attrs,
                                 XCB_WINDOW_CLASS_INPUT_OUTPUT);
    /* The platform owns this window; remember it as the display's top-level so
       boundary code can resolve any widget to it without the toolkit holding a
       window handle. */
    priv->root_window = win;
    return _IswXcbWindowWrap(win);
}

/* Resolve the platform-owned window backing a widget.  Most widgets have none
   and map to the display's shell root; widgets that back a distinct window
   (tooltip popups) are registered via _IswPlatformSetWidgetWindow.  The toolkit
   itself stores no window handle. */
IswWindow
_IswPlatformWidgetWindow(IswDisplay dpy, Widget w)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    int i;
    if (!priv)
        return _IswXcbWindowWrap(0);
    for (i = 0; i < priv->wmap_count; i++)
        if (priv->wmap[i].widget == (void *) w)
            return _IswXcbWindowWrap(priv->wmap[i].window);
    return _IswXcbWindowWrap(priv->root_window);
}

/* Reverse of _IswPlatformWidgetWindow: the widget that owns a given native
   window, or the shell root's widget for the display's top-level window.  Used
   by the backend's event translation to stamp an event's dispatch target with
   the root widget of the window that received it — the toolkit core never does
   this resolution itself. */
Widget
_IswXcbWidgetForWindow(IswDisplay dpy, xcb_window_t window)
{
    /* The window→widget table (ISWPlatformWWTableXCB.c) resolves both a
       widget's own window and the foreign/extra windows registered onto it
       (selection requestor, tray screen-root); event target resolution must
       see all of them, so go through it rather than scanning wmap. */
    if (window == 0)
        return NULL;
    return IswWindowToWidget(dpy, _IswXcbWindowWrap(window));
}

/* Neutral reverse lookup (declared in ISW/ISWPlatform.h). */
Widget
_IswPlatformWidgetForWindow(IswDisplay dpy, IswWindow win)
{
    return IswWindowToWidget(dpy, win);
}

/* Neutral widget-liveness test (declared in ISW/ISWPlatform.h). */
Boolean
_IswPlatformWidgetIsLive(IswDisplay dpy, Widget widget)
{
    return _IswXcbWidgetRegistered(dpy, widget);
}

/* Register (or clear, with win==0) the window backing a specific widget. */
void
_IswPlatformSetWidgetWindow(IswDisplay dpy, Widget w, IswWindow win)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    xcb_window_t xw = _IswXcbWindow(win);
    int i;
    if (!priv || !w)
        return;
    for (i = 0; i < priv->wmap_count; i++) {
        if (priv->wmap[i].widget == (void *) w) {
            if (xw == 0) {       /* remove */
                priv->wmap[i] = priv->wmap[--priv->wmap_count];
            } else {
                priv->wmap[i].window = xw;
            }
            return;
        }
    }
    if (xw == 0)
        return;
    if (priv->wmap_count == priv->wmap_cap) {
        int ncap = priv->wmap_cap ? priv->wmap_cap * 2 : 4;
        IswWidgetWindowMap *n = (IswWidgetWindowMap *)
            realloc(priv->wmap, (size_t) ncap * sizeof(*n));
        if (!n)
            return;
        priv->wmap = n;
        priv->wmap_cap = ncap;
    }
    priv->wmap[priv->wmap_count].widget = (void *) w;
    priv->wmap[priv->wmap_count].window = xw;
    priv->wmap_count++;
}

/* Blit a finished composite surface to the root window.  The window blit lives
   here (the render backend hands us its back buffer through the accessor and no
   longer names a window): Present path when usable, else cairo source-paint
   onto the surface's cached window context. */
static void
xcb_root_present(IswDisplay dpy, IswWindow win, IswSurface surface,
                 int width, int height)
{
    IswDisplayXCB *priv = (IswDisplayXCB*)dpy;
    cairo_surface_t *back = NULL;
    void *window_cr = NULL;
    xcb_pixmap_t back_pixmap = 0;
    xcb_pixmap_t copy_pixmap = 0;
    unsigned int copy_w = 0, copy_h = 0;
    uint32_t serial = 0;
    (void) width; (void) height;

    if (!priv->conn)
        return;
    if (!_ISWRenderSurfacePresentSource(surface, &back, &window_cr,
                                        &back_pixmap, &serial,
                                        &copy_pixmap, &copy_w, &copy_h))
        return;

    if (back_pixmap) {
        xcb_present_pixmap(priv->conn, _IswXcbWindow(win), back_pixmap, serial,
                           XCB_NONE,  /* valid region (whole) */
                           XCB_NONE,  /* update region (whole) */
                           0, 0,      /* x/y offset */
                           XCB_NONE,  /* target_crtc (auto) */
                           XCB_NONE,  /* wait_fence */
                           XCB_NONE,  /* idle_fence */
                           XCB_PRESENT_OPTION_COPY,
                           0, 0, 0,   /* target_msc / divisor / remainder */
                           0, NULL);  /* notifies */
    } else if (copy_pixmap && copy_w > 0 && copy_h > 0) {
        /* Straight server-side blit: pixmap→window CopyArea moves the final
           full-window copy off the CPU entirely (no destination read, no Cairo
           per-pixel pass).  The opaque root composite needs no blend. */
        if (priv->blit_gc == 0) {
            priv->blit_gc = xcb_generate_id(priv->conn);
            xcb_create_gc(priv->conn, priv->blit_gc, _IswXcbWindow(win), 0, NULL);
        }
        xcb_copy_area(priv->conn, copy_pixmap, _IswXcbWindow(win), priv->blit_gc,
                      0, 0, 0, 0, (uint16_t) copy_w, (uint16_t) copy_h);
    } else if (window_cr && back) {
        cairo_t *cr = (cairo_t *) window_cr;
        cairo_set_source_surface(cr, back, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    }
    xcb_flush(priv->conn);
}

static const IswPlatformRootOps xcb_root_ops = {
    .create_root  = xcb_root_create,
    .present_root = xcb_root_present,
};

/* ---- backend vtable + dispatcher ----------------------------------------- */

extern const IswPlatformEventOps isw_platform_xcb_event_ops; /* ISWPlatformEventXCB.c */
extern const IswPlatformInputOps isw_platform_xcb_input_ops; /* ISWPlatformInputXCB.c */
extern const IswPlatformColorOps isw_platform_xcb_color_ops; /* ISWPlatformColorFontXCB.c */
extern const IswPlatformFontOps  isw_platform_xcb_font_ops;  /* ISWPlatformColorFontXCB.c */
extern const IswPlatformCursorOps    isw_platform_xcb_cursor_ops;    /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformGrabOps      isw_platform_xcb_grab_ops;      /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformSelectionOps     isw_platform_xcb_selection_ops;      /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformSelectionHighOps isw_platform_xcb_selection_high_ops; /* ISWPlatformGrabCursorXCB.c */
extern const IswPlatformAtomOps      isw_platform_xcb_atom_ops;      /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformPropertyOps  isw_platform_xcb_property_ops;  /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformHintOps      isw_platform_xcb_hint_ops;      /* ISWPlatformAtomPropXCB.c */
extern const IswPlatformDndOps       isw_platform_xcb_dnd_ops;       /* ISWPlatformDndXCB.c */

const IswPlatformOps isw_platform_xcb_ops = {
    .display   = &xcb_display_ops,
    .window    = &xcb_window_ops,
    .root      = &xcb_root_ops,
    .event     = &isw_platform_xcb_event_ops,   /* Phase 11a */
    .input     = &isw_platform_xcb_input_ops,
    .selection      = &isw_platform_xcb_selection_ops,
    .selection_high = &isw_platform_xcb_selection_high_ops,
    .color     = &isw_platform_xcb_color_ops,
    .font      = &isw_platform_xcb_font_ops,
    .cursor    = &isw_platform_xcb_cursor_ops,
    .grab      = &isw_platform_xcb_grab_ops,
    .atom      = &isw_platform_xcb_atom_ops,
    .property  = &isw_platform_xcb_property_ops,
    .hint      = &isw_platform_xcb_hint_ops,
    .dnd       = &isw_platform_xcb_dnd_ops,
    .resource  = &isw_platform_xcb_resource_ops,   /* Phase 15 */
    .render    = &isw_platform_xcb_render_ops,
};

/* Backend selection (Phase 9): chosen as the first act of init, before any
   connection exists, so connection setup goes through the vtable.  Single
   backend today → always the XCB ops; a build/env selector slots in here. */
const IswPlatformOps *
_IswPlatformSelectBackend(void)
{
    return &isw_platform_xcb_ops;
}

const IswPlatformRenderOps *
_IswPlatformRenderOpsActive(void)
{
    return _IswPlatformSelectBackend()->render;
}

/* Neutral event-loop fd accessor (replaces the ConnectionNumber XCB macro),
   dispatched through the active backend's display vtable.

   Uses the selected backend rather than the per-display record's ops: this is
   called during display setup (AddToAppContext) BEFORE the per-display record
   is registered, so _IswGetPerDisplay(dpy) would miss.  Connection-fd access is
   connection-setup-adjacent (same category as open/close), so the selected
   backend is the right source — consistent with Phase 9. */
int
_IswPlatformConnectionFd(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->display && ops->display->connection_fd)
        return ops->display->connection_fd(dpy);
    return -1;
}

IswDisplay
_IswPlatformOpenDisplay(const char *display_name, int *default_screen)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->display && ops->display->open)
        return ops->display->open(display_name, default_screen);
    return NULL;
}

void
_IswPlatformCloseDisplay(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->display && ops->display->close)
        ops->display->close(dpy);
}

const char *
_IswPlatformDisplayVendor(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->vendor)
        return ops->display->vendor(dpy);
    return "";
}

void *
IswDisplayNativeHandle(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->native_display)
        return ops->display->native_display(dpy);
    return NULL;
}

void *
IswScreenNativeHandle(IswScreen screen)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->display && ops->display->native_screen)
        return ops->display->native_screen(screen);
    return NULL;
}

void *
IswWindowNativeHandle(IswWindow win)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->display && ops->display->native_window)
        return ops->display->native_window(win);
    return NULL;
}

IswScreen
_IswDefaultScreenOf(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    const IswPlatformOps *ops = pd ? pd->ops : _IswPlatformSelectBackend();
    int index = pd ? pd->defaultScreen : 0;
    if (ops && ops->display && ops->display->screen)
        return ops->display->screen(dpy, index);
    return NULL;
}

IswWindow
_IswDefaultRootWindow(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    const IswPlatformOps *ops = pd ? pd->ops : _IswPlatformSelectBackend();
    IswScreen screen = _IswDefaultScreenOf(dpy);
    if (ops && ops->display && ops->display->root_window)
        return ops->display->root_window(screen);
    return _IswXcbWindowWrap(0);
}

uint32_t
_IswPlatformScreenWidth(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_width)
        return ops->display->screen_width(screen);
    return 0;
}

uint32_t
_IswPlatformScreenHeight(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_height)
        return ops->display->screen_height(screen);
    return 0;
}

IswColormap
_IswPlatformScreenDefaultColormap(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_default_colormap)
        return ops->display->screen_default_colormap(screen);
    return (IswColormap) 0;
}

int
_IswPlatformScreenDepth(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_depth)
        return ops->display->screen_depth(screen);
    return 0;
}

unsigned long
_IswPlatformScreenBlackPixel(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_black_pixel)
        return ops->display->screen_black_pixel(screen);
    return 0;
}

unsigned long
_IswPlatformScreenWhitePixel(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->screen_white_pixel)
        return ops->display->screen_white_pixel(screen);
    return 0;
}

/* Connection health + flush wrappers (Phase 11a) — used by the event loop.
   Recover ops from the per-display record (loop runs post-registration). */
Boolean
_IswPlatformDisplayHasError(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->has_error)
        return ops->display->has_error(dpy);
    return True;
}

void
_IswPlatformFlush(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->flush)
        ops->display->flush(dpy);
}

void
_IswPlatformSync(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->display && ops->display->sync)
        ops->display->sync(dpy);
}

/* Event-loop poll (Phase 11a).  Returns the next native event (caller frees),
   or NULL.  Recovers ops from the per-display record — the loop runs after the
   display is registered. */
IswEvent *
_IswPlatformPollEvent(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->event && ops->event->poll)
        return ops->event->poll(dpy);
    return NULL;
}

IswEvent *
_IswPlatformPollQueuedEvent(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->event && ops->event->poll_queued)
        return ops->event->poll_queued(dpy);
    return NULL;
}

/* Window attribute change (Phase 13a) — used by the selection code to toggle
   a requestor window's event mask during a transfer. */
void
_IswPlatformChangeAttributes(IswDisplay dpy, IswWindow win,
                             const IswWindowAttributes *attrs, unsigned int mask)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->change_attributes)
        ops->window->change_attributes(dpy, win, attrs, mask);
}

/* Window lifecycle dispatchers (Phase 13c) — the toolkit calls these instead of
   xcb_* window functions; each recovers the injected ops and null-guards. */
IswWindow
_IswPlatformAllocWindowId(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->alloc_id)
        return ops->window->alloc_id(dpy);
    return _IswXcbWindowWrap(0);
}

IswWindow
_IswPlatformCreateWindow(IswDisplay dpy, IswWindow parent,
                         const IswWindowGeometry *geom,
                         const IswWindowAttributes *attrs,
                         unsigned int window_class)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->create)
        return ops->window->create(dpy, parent, geom, attrs, window_class);
    return _IswXcbWindowWrap(0);
}

void
_IswPlatformDestroyWindow(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->destroy)
        ops->window->destroy(dpy, win);
}

void
_IswPlatformMapWindow(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->map)
        ops->window->map(dpy, win);
}

void
_IswPlatformUnmapWindow(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->unmap)
        ops->window->unmap(dpy, win);
}

void
_IswPlatformReparentWindow(IswDisplay dpy, IswWindow win, IswWindow new_parent,
                           int32_t x, int32_t y)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->reparent)
        ops->window->reparent(dpy, win, new_parent, x, y);
}

void
_IswPlatformConfigureWindow(IswDisplay dpy, IswWindow win,
                            const IswWindowGeometry *geom, unsigned int mask,
                            IswStackMode stack, IswWindow sibling)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->configure)
        ops->window->configure(dpy, win, geom, mask, stack, sibling);
}

void
_IswPlatformClearArea(IswDisplay dpy, IswWindow win,
                      int16_t x, int16_t y, uint16_t w, uint16_t h,
                      Boolean generate_expose)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->clear_area)
        ops->window->clear_area(dpy, win, x, y, w, h, generate_expose);
}

IswWindowId
_IswPlatformWindowId(IswWindow win)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->window && ops->window->window_id)
        return ops->window->window_id(win);
    return 0;
}

Boolean
_IswPlatformWindowViewable(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->window_viewable)
        return ops->window->window_viewable(dpy, win);
    return False;
}

IswWindow
_IswPlatformWindowFromId(IswDisplay dpy, IswWindowId id)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->window && ops->window->window_from_id)
        return ops->window->window_from_id(id);
    return _IswXcbWindowWrap((xcb_window_t) id);
}

/* Root surface dispatchers (Phase 13c). */
IswWindow
_IswPlatformCreateRoot(IswDisplay dpy, IswScreen screen,
                       const IswWindowGeometry *geom,
                       const IswWindowAttributes *attrs)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->root && ops->root->create_root)
        return ops->root->create_root(dpy, screen, geom, attrs);
    return _IswXcbWindowWrap(0);
}

void
_IswPlatformPresentRoot(IswDisplay dpy, IswWindow win, IswSurface surface,
                        int width, int height)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->root && ops->root->present_root)
        ops->root->present_root(dpy, win, surface, width, height);
}

/* ---- resource-resolution wrappers (Phase 15) -----------------------------
   Toolkit resource code (Initialize.c / Resources.c / Intrinsic.c / Error.c /
   Display.c) calls these instead of any xcb_xrm_* function, so no
   toolkit TU names Xrm.  Resource-database ops are NOT keyed on a connection
   (the database is a free-standing store; several calls happen before a
   per-display record exists), so ops come from the selected backend — same
   rationale as _IswPlatformConnectionFd, not the per-display record. */
IswDatabaseHandle
_IswPlatformResourceFromString(const char *str)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->from_string)
        return ops->resource->from_string(str);
    return NULL;
}

IswDatabaseHandle
_IswPlatformResourceFromFile(const char *filename)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->from_file)
        return ops->resource->from_file(filename);
    return NULL;
}

IswDatabaseHandle
_IswPlatformResourceBuildUserDb(IswDisplay dpy, IswScreen screen)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->build_user_db)
        return ops->resource->build_user_db(dpy, screen);
    return NULL;
}

void
_IswPlatformResourceCombine(IswDatabaseHandle source, IswDatabaseHandle *target,
                            Boolean override)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->combine)
        ops->resource->combine(source, target, override);
}

void
_IswPlatformResourcePut(IswDatabaseHandle *db, const char *resource,
                        const char *value)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->put_resource)
        ops->resource->put_resource(db, resource, value);
}

void
_IswPlatformResourcePutLine(IswDatabaseHandle *db, const char *line)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->put_resource_line)
        ops->resource->put_resource_line(db, line);
}

char *
_IswPlatformResourceToString(IswDatabaseHandle db)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->to_string)
        return ops->resource->to_string(db);
    return NULL;
}

void
_IswPlatformResourceFree(IswDatabaseHandle db)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->free)
        ops->resource->free(db);
}

int
_IswPlatformResourceGetString(IswDatabaseHandle db, const char *res_name,
                              const char *res_class, char **out)
{
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->resource && ops->resource->get_string)
        return ops->resource->get_string(db, res_name, res_class, out);
    return -1;
}

/* ---- thin dispatch wrappers (color / font / cursor / grab / selection) ----
   Toolkit/widget code calls these instead of walking the ops vtable.  Each
   null-guards the sub-vtable + op (a backend that hasn't filled it degrades to
   no-op / failure).  Same convention as _IswPlatformConnectionFd. */

/* Color (Phase 4) */
Boolean
_IswPlatformQueryColor(IswDisplay dpy, IswColormap cmap,
                       unsigned long pixel, IswColor *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->query_color)
        return ops->color->query_color(dpy, cmap, pixel, out);
    return False;
}

Boolean
_IswPlatformAllocColor(IswDisplay dpy, IswColormap cmap,
                       unsigned short red, unsigned short green,
                       unsigned short blue, unsigned long *pixel_out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->alloc_color)
        return ops->color->alloc_color(dpy, cmap, red, green, blue, pixel_out);
    return False;
}

Boolean
_IswPlatformAllocNamedColor(IswDisplay dpy, IswColormap cmap,
                            const char *name, unsigned long *pixel_out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->alloc_named_color)
        return ops->color->alloc_named_color(dpy, cmap, name, pixel_out);
    return False;
}

Boolean
_IswPlatformLookupColor(IswDisplay dpy, IswColormap cmap, const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->lookup_color)
        return ops->color->lookup_color(dpy, cmap, name);
    return False;
}

void
_IswPlatformFreeColors(IswDisplay dpy, IswColormap cmap, unsigned long pixel)
{
    if(!dpy) return;
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->free_colors)
        ops->color->free_colors(dpy, cmap, pixel);
}

Boolean
_IswPlatformMatchVisualInfo(IswDisplay dpy, IswScreen screen,
                            int depth, int visual_class, IswVisualInfo *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->color && ops->color->match_visual_info)
        return ops->color->match_visual_info(dpy, screen, depth,
                                             visual_class, out);
    return False;
}

/* Font (Phase 4) */
IswFontId
_IswPlatformLoadFont(IswDisplay dpy, const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->font && ops->font->load_font)
        return ops->font->load_font(dpy, name);
    return 0;
}

void
_IswPlatformFreeFont(IswDisplay dpy, IswFontId fid)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->font && ops->font->free_font)
        ops->font->free_font(dpy, fid);
}

IswFontStruct *
_IswPlatformLoadFallbackFont(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->font && ops->font->load_fallback_font)
        return ops->font->load_fallback_font(dpy);
    return NULL;
}

/* Cursor (Phase 5) */
IswCursor
_IswPlatformLoadNamedCursor(IswDisplay dpy, IswScreen screen,
                            const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->load_named)
        return ops->cursor->load_named(dpy, screen, name);
    return 0;
}

void
_IswPlatformSetWindowCursor(IswDisplay dpy, IswWindow win, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->set_window_cursor)
        ops->cursor->set_window_cursor(dpy, win, cursor);
}

void
_IswPlatformFreeCursor(IswDisplay dpy, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->cursor && ops->cursor->free_cursor)
        ops->cursor->free_cursor(dpy, cursor);
}

/* Grabs (Phase 5) */
int
_IswPlatformGrabPointer(IswDisplay dpy, IswWindow grab_window,
                        Boolean owner_events, unsigned int event_mask,
                        int pointer_mode, int keyboard_mode,
                        IswWindow confine_to, IswCursor cursor, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_pointer)
        return ops->grab->grab_pointer(dpy, grab_window, owner_events,
                                       event_mask, pointer_mode, keyboard_mode,
                                       confine_to, cursor, time);
    return -1;
}

void
_IswPlatformUngrabPointer(IswDisplay dpy, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_pointer)
        ops->grab->ungrab_pointer(dpy, time);
}

int
_IswPlatformGrabKeyboard(IswDisplay dpy, IswWindow grab_window,
                         Boolean owner_events, int pointer_mode,
                         int keyboard_mode, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_keyboard)
        return ops->grab->grab_keyboard(dpy, grab_window, owner_events,
                                        pointer_mode, keyboard_mode, time);
    return -1;
}

void
_IswPlatformUngrabKeyboard(IswDisplay dpy, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_keyboard)
        ops->grab->ungrab_keyboard(dpy, time);
}

void
_IswPlatformGrabButton(IswDisplay dpy, IswWindow grab_window, int button,
                       unsigned int modifiers, Boolean owner_events,
                       unsigned int event_mask, int pointer_mode,
                       int keyboard_mode, IswWindow confine_to, IswCursor cursor)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_button)
        ops->grab->grab_button(dpy, grab_window, button, modifiers,
                               owner_events, event_mask, pointer_mode,
                               keyboard_mode, confine_to, cursor);
}

void
_IswPlatformGrabKey(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                    unsigned int modifiers, Boolean owner_events,
                    int pointer_mode, int keyboard_mode)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->grab_key)
        ops->grab->grab_key(dpy, grab_window, keycode, modifiers,
                            owner_events, pointer_mode, keyboard_mode);
}

void
_IswPlatformUngrabKey(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                      unsigned int modifiers)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_key)
        ops->grab->ungrab_key(dpy, grab_window, keycode, modifiers);
}

void
_IswPlatformUngrabButton(IswDisplay dpy, IswWindow grab_window, int button,
                         unsigned int modifiers)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->ungrab_button)
        ops->grab->ungrab_button(dpy, grab_window, button, modifiers);
}

void
_IswPlatformRefreshMapping(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->refresh_mapping)
        ops->input->refresh_mapping(dpy);
}

void
_IswPlatformWarpPointer(IswDisplay dpy, IswWindow dst_win, int x, int y)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->warp_pointer)
        ops->input->warp_pointer(dpy, dst_win, x, y);
}

IswKeySym
_IswPlatformKeycodeToKeysym(IswDisplay dpy, IswKeyCode kc, int col)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->keycode_to_keysym)
        return ops->input->keycode_to_keysym(dpy, kc, col);
    return IswNoSymbol;
}

void
_IswPlatformKeysymToKeycodes(IswDisplay dpy, IswKeySym ks,
                             IswKeyCode **out, int *count)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (out) *out = NULL;
    if (count) *count = 0;
    if (ops && ops->input && ops->input->keysym_to_keycodes)
        ops->input->keysym_to_keycodes(dpy, ks, out, count);
}

void
_IswPlatformConvertCase(IswKeySym ks, IswKeySym *lower, IswKeySym *upper)
{
    /* convert_case takes no display; reach the active backend's input ops
       directly.  All displays share the backend ops table. */
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    if (ops && ops->input && ops->input->convert_case) {
        ops->input->convert_case(ks, lower, upper);
        return;
    }
    if (lower) *lower = ks;
    if (upper) *upper = ks;
}

void
_IswPlatformTranslateKeycode(IswDisplay dpy, IswKeyCode kc, IswModMask state,
                             IswModMask *mods_return, IswKeySym *keysym_return)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->translate_keycode) {
        ops->input->translate_keycode(dpy, kc, state, mods_return, keysym_return);
        return;
    }
    if (mods_return) *mods_return = 0;
    if (keysym_return) *keysym_return = IswNoSymbol;
}

void
_IswPlatformBuildModMap(IswDisplay dpy, IswModKeysymEntry *mods_return,
                        IswKeySym **keysyms_return, int *count_return)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (keysyms_return) *keysyms_return = NULL;
    if (count_return) *count_return = 0;
    if (ops && ops->input && ops->input->build_mod_map)
        ops->input->build_mod_map(dpy, mods_return, keysyms_return, count_return);
}

void
_IswPlatformFreeKeysyms(IswDisplay dpy)
{
    if(!dpy) return;
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->free_keysyms)
        ops->input->free_keysyms(dpy);
}

void
_IswPlatformChangeActivePointerGrab(IswDisplay dpy, IswCursor cursor,
                                    IswTime time, unsigned int event_mask)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->grab && ops->grab->change_active_pointer_grab)
        ops->grab->change_active_pointer_grab(dpy, cursor, time, event_mask);
}

/* Selection */
IswSelectionId
_IswPlatformSelectionInternName(IswDisplay dpy, const char *name,
                                Boolean only_if_exists)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->intern_name)
        return ops->selection->intern_name(dpy, name, only_if_exists);
    return ISW_SELECTION_NONE;
}

Boolean
_IswPlatformSelectionName(IswDisplay dpy, IswSelectionId id,
                          char *buf, size_t buflen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->name_of)
        return ops->selection->name_of(dpy, id, buf, buflen);
    return False;
}

void
_IswPlatformSetSelectionOwner(IswDisplay dpy, IswWindow owner,
                              IswSelectionId selection, IswTime time)
{
    if(!dpy) return;
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->set_owner)
        ops->selection->set_owner(dpy, owner, selection, time);
}

IswWindow
_IswPlatformGetSelectionOwner(IswDisplay dpy, IswSelectionId selection)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->get_owner)
        return ops->selection->get_owner(dpy, selection);
    return _IswXcbWindowWrap(XCB_NONE);
}

void
_IswPlatformConvertSelection(IswDisplay dpy, IswWindow requestor,
                             IswSelectionId selection, IswSelectionId target,
                             IswSelectionId property, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->convert)
        ops->selection->convert(dpy, requestor, selection, target,
                                property, time);
}

Boolean
_IswPlatformSelectionDecodeEvent(IswDisplay dpy, const void *native,
                                 IswSelectionEvent *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->decode_event)
        return ops->selection->decode_event(dpy, native, out);
    if (out)
        out->kind = ISW_SEL_EVENT_OTHER;
    return False;
}

void
_IswPlatformSelectionSendNotify(IswDisplay dpy, const IswSelectionRequest *req,
                                IswSelectionId property)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->send_notify)
        ops->selection->send_notify(dpy, req, property);
}

IswSelectionId
_IswPlatformSelectionStdType(IswDisplay dpy, IswSelectionStdType which)
{
    const char *name;

    switch (which) {
    case ISW_SEL_STDTYPE_STRING:  name = "STRING";  break;
    case ISW_SEL_STDTYPE_INTEGER: name = "INTEGER"; break;
    case ISW_SEL_STDTYPE_ID_LIST:
    default:                      name = "ATOM";    break;
    }
    return _IswPlatformSelectionInternName(dpy, name, False);
}

unsigned long
_IswPlatformSelectionMaxTransfer(IswDisplay dpy)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection && ops->selection->max_transfer_bytes)
        return ops->selection->max_transfer_bytes(dpy);
    return 0;
}

/* High-level selection — offer/request/disown */
Boolean
_IswPlatformSelectionOffer(IswDisplay dpy, Widget widget, IswTime time,
                           IswSelectionOfferProc offer_proc,
                           IswSelectionLoseProc lose_proc)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection_high && ops->selection_high->offer)
        return ops->selection_high->offer(dpy, widget, time,
                                          offer_proc, lose_proc);
    return False;
}

void
_IswPlatformSelectionDisown(IswDisplay dpy, Widget widget, IswTime time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection_high && ops->selection_high->disown)
        ops->selection_high->disown(dpy, widget, time);
}

void
_IswPlatformSelectionRequestText(IswDisplay dpy, Widget widget, IswTime time,
                                 IswSelectionReceiveProc receive,
                                 IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->selection_high && ops->selection_high->request)
        ops->selection_high->request(dpy, widget, time, receive, closure);
    else if (receive)
        receive(widget, closure, NULL, 0);
}

/* Input — query the pointer relative to `win`, via the input vtable. */
Boolean
_IswPlatformQueryPointer(IswDisplay dpy, IswWindow win,
                         int *root_x, int *root_y, int *win_x, int *win_y,
                         IswModMask *mods, IswWindow *child)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->input && ops->input->query_pointer)
        return ops->input->query_pointer(dpy, win, root_x, root_y,
                                         win_x, win_y, mods, child);
    return False;
}

/* Atom (Phase 6) */
Atom
_IswPlatformInternAtomOp(IswDisplay dpy, const char *name, Boolean only_if_exists)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->atom && ops->atom->intern)
        return ops->atom->intern(dpy, name, only_if_exists);
    return ISW_ATOM_NONE;
}

Boolean
_IswPlatformGetAtomName(IswDisplay dpy, Atom atom, char *buf, size_t buflen)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->atom && ops->atom->get_name)
        return ops->atom->get_name(dpy, atom, buf, buflen);
    return False;
}

/* Property (Phase 6) */
void
_IswPlatformChangeProperty(IswDisplay dpy, IswWindow win, Atom property,
                           Atom type, int format, IswPropMode mode,
                           const void *data, uint32_t num_elements)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->change)
        ops->property->change(dpy, win, property, type, format, mode,
                              data, num_elements);
}

Boolean
_IswPlatformGetProperty(IswDisplay dpy, IswWindow win, Atom property, Atom type,
                        uint32_t long_offset, uint32_t long_length,
                        IswProperty *out)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->get)
        return ops->property->get(dpy, win, property, type,
                                  long_offset, long_length, out);
    if (out) memset(out, 0, sizeof(*out));
    return False;
}

void
_IswPlatformDeleteProperty(IswDisplay dpy, IswWindow win, Atom property)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->property && ops->property->delete_)
        ops->property->delete_(dpy, win, property);
}

/* WM hints (Phase 6) */
void
_IswPlatformSetWindowTitle(IswDisplay dpy, IswWindow win, const char *utf8)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_window_title)
        ops->hint->set_window_title(dpy, win, utf8);
}

void
_IswPlatformSetIconTitle(IswDisplay dpy, IswWindow win, const char *utf8)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_icon_title)
        ops->hint->set_icon_title(dpy, win, utf8);
}

void
_IswPlatformSetWmClass(IswDisplay dpy, IswWindow win,
                       const char *name, const char *class_name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_class)
        ops->hint->set_wm_class(dpy, win, name, class_name);
}

void
_IswPlatformSetWmProtocols(IswDisplay dpy, IswWindow win,
                           const char **protocol_names, int num_protocols)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_protocols)
        ops->hint->set_wm_protocols(dpy, win, protocol_names, num_protocols);
}

void
_IswPlatformSetTransientFor(IswDisplay dpy, IswWindow win, IswWindow leader)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_transient_for)
        ops->hint->set_transient_for(dpy, win, leader);
}

void
_IswPlatformSetWindowType(IswDisplay dpy, IswWindow win, IswWindowType type)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_window_type)
        ops->hint->set_window_type(dpy, win, type);
}

void
_IswPlatformSetPid(IswDisplay dpy, IswWindow win, uint32_t pid)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_pid)
        ops->hint->set_pid(dpy, win, pid);
}

void
_IswPlatformSetNormalHints(IswDisplay dpy, IswWindow win, uint32_t flags,
                           int x, int y, int width, int height,
                           int min_width, int min_height,
                           int max_width, int max_height,
                           int width_inc, int height_inc,
                           int min_aspect_num, int min_aspect_den,
                           int max_aspect_num, int max_aspect_den,
                           int base_width, int base_height, int win_gravity)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_normal_hints)
        ops->hint->set_normal_hints(dpy, win, flags, x, y, width, height,
                                    min_width, min_height, max_width, max_height,
                                    width_inc, height_inc,
                                    min_aspect_num, min_aspect_den,
                                    max_aspect_num, max_aspect_den,
                                    base_width, base_height, win_gravity);
}

void
_IswPlatformSetWmHints(IswDisplay dpy, IswWindow win, const IswWmHints *hints)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_hints)
        ops->hint->set_wm_hints(dpy, win, hints);
}

void
_IswPlatformSetStrutPartial(IswDisplay dpy, IswWindow win,
                            const IswStrutPartial *strut)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_strut_partial)
        ops->hint->set_strut_partial(dpy, win, strut);
}

void
_IswPlatformDeleteTransientFor(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->delete_transient_for)
        ops->hint->delete_transient_for(dpy, win);
}

void
_IswPlatformSetWmCommand(IswDisplay dpy, IswWindow win,
                         const char *const *argv, int argc)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_wm_command)
        ops->hint->set_wm_command(dpy, win, argv, argc);
}

void
_IswPlatformDeleteWmCommand(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->delete_wm_command)
        ops->hint->delete_wm_command(dpy, win);
}

void
_IswPlatformSetClientLeader(IswDisplay dpy, IswWindow win, IswWindow leader)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_client_leader)
        ops->hint->set_client_leader(dpy, win, leader);
}

void
_IswPlatformSetWindowRole(IswDisplay dpy, IswWindow win, const char *role)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_window_role)
        ops->hint->set_window_role(dpy, win, role);
}

void
_IswPlatformDeleteWindowRole(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->delete_window_role)
        ops->hint->delete_window_role(dpy, win);
}

void
_IswPlatformSetLocaleName(IswDisplay dpy, IswWindow win, const char *locale)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_locale_name)
        ops->hint->set_locale_name(dpy, win, locale);
}

void
_IswPlatformSetUserTime(IswDisplay dpy, IswWindow win,
                        IswWindow time_win, uint32_t time)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_user_time)
        ops->hint->set_user_time(dpy, win, time_win, time);
}

void
_IswPlatformSetStartupId(IswDisplay dpy, IswWindow win, IswWindow root,
                         const char *startup_id)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_startup_id)
        ops->hint->set_startup_id(dpy, win, root, startup_id);
}

void
_IswPlatformSetIconData(IswDisplay dpy, IswWindow win,
                        const uint32_t *data, uint32_t num_elements)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_icon_data)
        ops->hint->set_icon_data(dpy, win, data, num_elements);
}

void
_IswPlatformDeleteIconData(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->delete_icon_data)
        ops->hint->delete_icon_data(dpy, win);
}

void
_IswPlatformSetIconic(IswDisplay dpy, IswWindow win)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->set_iconic)
        ops->hint->set_iconic(dpy, win);
}

void
_IswPlatformToggleWmState(IswDisplay dpy, IswWindow win,
                          const char *state_name, Boolean set)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->hint && ops->hint->toggle_wm_state)
        ops->hint->toggle_wm_state(dpy, win, state_name, set);
}

Boolean
_IswPlatformIsProtocol(IswDisplay dpy, IswProtocolId id, const char *name)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(dpy)->ops;
    if (ops && ops->atom && ops->atom->intern) {
        Atom expected = ops->atom->intern(dpy, name, True);
        return (expected != ISW_ATOM_NONE && (Atom) id == expected);
    }
    return False;
}

/* ---- WM-protocol primitives (Shell) -------------------------------------- */

void
_IswPlatformSendMessage(IswDisplay dpy, IswWindow target, IswWindow win,
                        Atom type, int format, const void *data,
                        Boolean propagate, unsigned int event_mask)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_client_message_event_t ev;

    if (!conn)
        return;
    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.format        = (uint8_t) format;
    ev.window        = _IswXcbWindow(win);
    ev.type          = (xcb_atom_t) type;
    if (data)
        memcpy(ev.data.data8, data, 20);   /* 5x32 / 10x16 / 20x8 union */
    xcb_send_event(conn, propagate ? 1 : 0, _IswXcbWindow(target),
                   event_mask, (const char *) &ev);
}

Boolean
_IswPlatformTranslateToRoot(IswDisplay dpy, IswWindow src, int x, int y,
                            int *root_x, int *root_y)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_screen_t *screen;
    xcb_translate_coordinates_cookie_t cookie;
    xcb_translate_coordinates_reply_t *reply;

    if (!conn)
        return False;
    screen = _IswXcbDefaultScreen(dpy);
    if (!screen)
        return False;
    cookie = xcb_translate_coordinates(conn, _IswXcbWindow(src), screen->root,
                                       (int16_t) x, (int16_t) y);
    reply = xcb_translate_coordinates_reply(conn, cookie, NULL);
    if (!reply)
        return False;
    if (root_x) *root_x = reply->dst_x;
    if (root_y) *root_y = reply->dst_y;
    free(reply);
    return True;
}

IswWindow
_IswPlatformCreateInputOnly(IswDisplay dpy, IswWindow parent)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_window_t id;

    if (!conn)
        return (IswWindow) 0;
    id = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, id, _IswXcbWindow(parent),
                      0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
                      XCB_COPY_FROM_PARENT, 0, NULL);
    return _IswXcbWindowWrap(id);
}

Boolean
_IswPlatformWaitForConfigure(IswDisplay dpy, IswWindow win,
                             unsigned long timeout_ms,
                             int *new_x, int *new_y, int *new_w, int *new_h,
                             int *new_border, Boolean *reparented)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_window_t w = _IswXcbWindow(win);
    xcb_screen_t *screen = _IswXcbDefaultScreen(dpy);
    xcb_window_t root = screen ? screen->root : 0;
    struct timespec start, now;

    if (!conn)
        return False;
    xcb_flush(conn);
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (;;) {
        xcb_generic_event_t *ev = xcb_poll_for_event(conn);
        if (ev) {
            uint8_t type = ev->response_type & ~0x80;
            if (type == XCB_CONFIGURE_NOTIFY) {
                xcb_configure_notify_event_t *cne =
                    (xcb_configure_notify_event_t *) ev;
                if (cne->window == w) {
                    if (new_x) *new_x = cne->x;
                    if (new_y) *new_y = cne->y;
                    if (new_w) *new_w = cne->width;
                    if (new_h) *new_h = cne->height;
                    if (new_border) *new_border = cne->border_width;
                    free(ev);
                    return True;
                }
            }
            if (type == XCB_REPARENT_NOTIFY && reparented) {
                xcb_reparent_notify_event_t *rne =
                    (xcb_reparent_notify_event_t *) ev;
                if (rne->window == w)
                    *reparented = (rne->parent != root);
            }
            free(ev);
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        {
            unsigned long elapsed_ms =
                (unsigned long)(now.tv_sec - start.tv_sec) * 1000 +
                (unsigned long)(now.tv_nsec - start.tv_nsec) / 1000000;
            if (elapsed_ms >= timeout_ms)
                return False;
        }
        {
            struct timespec sleep_ts = { 0, 1000000 }; /* 1ms */
            nanosleep(&sleep_ts, NULL);
        }
    }
}

/* Drag-and-drop (Phase 7) */
void
_IswPlatformDndEnable(Widget shell)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(shell))->ops;
    if (ops && ops->dnd && ops->dnd->enable)
        ops->dnd->enable(shell);
}

void
_IswPlatformDndWidgetAcceptDrops(Widget w)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->widget_accept_drops)
        ops->dnd->widget_accept_drops(w);
}

void
_IswPlatformDndStartDrag(Widget source, IswEvent *trigger,
                         IswDragSourceDesc *desc)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(source))->ops;
    if (ops && ops->dnd && ops->dnd->start_drag)
        ops->dnd->start_drag(source, trigger, desc);
}

void
_IswPlatformDndSetAcceptedTypes(Widget w, const char **types, int num_types)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_accepted_types)
        ops->dnd->set_accepted_types(w, types, num_types);
}

void
_IswPlatformDndSetAcceptedActions(Widget w, IswDndAction actions)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_accepted_actions)
        ops->dnd->set_accepted_actions(w, actions);
}

void
_IswPlatformDndSetDropCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drop_callback)
        ops->dnd->set_drop_callback(w, proc, closure);
}

void
_IswPlatformDndSetDragMotionCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drag_motion_callback)
        ops->dnd->set_drag_motion_callback(w, proc, closure);
}

void
_IswPlatformDndSetDragLeaveCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->set_drag_leave_callback)
        ops->dnd->set_drag_leave_callback(w, proc, closure);
}


Boolean
_IswPlatformDndIsDragging(Widget w)
{
    const IswPlatformOps *ops = _IswGetPerDisplay(IswDisplayOfObject(w))->ops;
    if (ops && ops->dnd && ops->dnd->is_dragging)
        return ops->dnd->is_dragging(w);
    return False;
}
