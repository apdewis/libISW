/*
 * ISWRenderCairoXCB.c - Cairo-XCB rendering backend
 *
 * Copyright (c) 2026 ISW Project
 *
 * This backend provides high-quality software rendering using Cairo
 * with an XCB surface. Features anti-aliasing, gradients, and alpha blending.
 *
 * CRITICAL: Uses Cairo-XCB surface - pure XCB, NO XLIB DEPENDENCIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ISWRenderCairoXCB.h"
#include "ISWPlatformPrivate.h"
#include <ISW/IntrinsicP.h> /* For Xt private types */
#include <ISW/CoreP.h>       /* For accessing widget->core fields */
#include <ISW/CompositeP.h>  /* For clipping out windowless children */
#include <ISW/SimpleP.h>     /* For simple.self_border (own-border widgets) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <xcb/present.h>

/* Defined in Initialize.c */
extern double _IswGetScaleFactor(IswDisplay dpy);

/* _ISWSetCairoFontFromXFont declared in ISWRenderPrivate.h */

/*
 * Cairo-XCB surface — the concrete struct _IswSurface for this backend.
 *
 * Holds the per-widget back buffer, its window-target surface, and the Present
 * state.  Self-contained: it carries the connection / screen / window / visual
 * it needs so the surface ops operate on the handle alone, without a render
 * context.  `cairo_ctx` is the surface's current drawing target (window ctx
 * outside a frame, back ctx during one); the render context mirrors it in
 * ctx->draw for the drawing primitives.
 */
struct _IswSurface {
    xcb_connection_t *connection;  /* server connection (from the widget) */
    xcb_screen_t *screen;          /* screen (for bitmap surfaces) */
    xcb_window_t window;           /* target window (windowed widget / ancestor) */

    cairo_surface_t *surface;      /* window surface (blit target) */
    cairo_t *cairo_ctx;            /* active drawing context (swapped in begin/end) */
    xcb_visualtype_t *visual;

    /* Double buffering */
    xcb_pixmap_t back_pixmap;      /* server-side back buffer */
    cairo_surface_t *back_surface; /* cairo surface on back_pixmap */
    cairo_t *back_ctx;             /* persistent context on back buffer */
    cairo_t *window_ctx;           /* context on window surface (for queries) */
    Dimension back_w, back_h;      /* current back buffer dimensions */
    Dimension alloc_w, alloc_h;    /* allocated pixmap dimensions (>= back_w/h) */
    Boolean back_is_image;         /* back buffer is a client-side ARGB32 image
                                      surface (windowless) vs server pixmap */
    Boolean back_needs_clear;      /* back surface freshly (re)allocated; clear
                                      to transparent on next begin (once) */

    /* State for save/restore */
    int save_count;

    /* Present extension for vsync'd blits */
    Boolean present_ok;            /* Present extension usable for this window */
    xcb_present_event_t present_eid; /* event context id */
    uint32_t present_serial;       /* monotonic serial for present_pixmap */

    /* Deferred initialization: surface created on first begin() if widget
     * had zero dimensions at ISWRenderCreate time. */
    Boolean deferred;

    /* Nested begin/end: when > 0, begin() and end() are no-ops so that
     * a parent widget can wrap the entire paint in one frame while child
     * widgets keep their own begin/end calls without causing extra blits. */
    int frame_depth;
};

/* The backend's data IS its IswSurface; keep the historical name as an alias so
   the body below reads unchanged. */
typedef struct _IswSurface ISWRenderCairoXCBData;

/*
 * =================================================================
 * Lifecycle
 * =================================================================
 */

/*
 * _cairo_xcb_create_surface - Create the window-target Cairo surface and
 * a cairo context on it. The context is always available for queries
 * (text measurement, etc.) even outside begin/end.
 * Returns True on success, False on failure.
 */
/* Nearest windowed ancestor of a (possibly windowless) widget.  Returns the
   widget itself if it is windowed. */
static Widget
_cairo_xcb_windowed_widget(Widget w)
{
    while (w != NULL && IswIsWidget(w) && !IswIsShell(w) &&
           w->core.parent != NULL)
        w = w->core.parent;
    return w;
}

static Boolean
_cairo_xcb_create_surface(IswSurface data, Widget widget)
{
    /* Windowless widgets share their windowed ancestor's window; the surface
       must cover the whole window, not just the child's rectangle. */
    Widget surf_w = !IswIsShell(widget)
                  ? _cairo_xcb_windowed_widget(widget) : widget;
    Dimension w = surf_w->core.width;
    Dimension h = surf_w->core.height;

    /* Clamp oversized dimensions — Cairo's XCB surface limit */
    if (w > 32767) w = 32767;
    if (h > 32767) h = 32767;

    data->surface = cairo_xcb_surface_create(
        data->connection,
        data->window,
        data->visual,
        w, h
    );

    if (cairo_surface_status(data->surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "ISWRenderCairoXCB: Failed to create Cairo surface: %s\n",
               cairo_status_to_string(cairo_surface_status(data->surface)));
        cairo_surface_destroy(data->surface);
        data->surface = NULL;
        return False;
    }

    /* HiDPI: set device scale so Cairo maps logical drawing coordinates
     * to physical surface pixels transparently. */
    {
        double sf = _IswGetScaleFactor(IswDisplayOf(widget));
        if (sf > 1.0)
            cairo_surface_set_device_scale(data->surface, sf, sf);
    }

    /* Create context on window surface — always available for queries */
    data->window_ctx = cairo_create(data->surface);
    if (cairo_status(data->window_ctx) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(data->window_ctx);
        data->window_ctx = NULL;
        cairo_surface_destroy(data->surface);
        data->surface = NULL;
        return False;
    }
    cairo_set_antialias(data->window_ctx, CAIRO_ANTIALIAS_GOOD);
    cairo_set_line_width(data->window_ctx, 1.0);
    cairo_set_operator(data->window_ctx, CAIRO_OPERATOR_OVER);

    /* cairo_ctx defaults to window context; begin() swaps to back buffer */
    data->cairo_ctx = data->window_ctx;

    /* Probe the Present extension (once) and set up an event context
     * for vsync'd blits.  Failure is non-fatal — we fall back to
     * immediate cairo_paint. */
    {
        static int present_probed = 0;
        static int present_available = 0;

        if (!present_probed) {
            present_probed = 1;
            xcb_present_query_version_cookie_t vc =
                xcb_present_query_version(data->connection, 1, 0);
            xcb_present_query_version_reply_t *vr =
                xcb_present_query_version_reply(data->connection, vc, NULL);
            if (vr) {
                present_available = 1;
                free(vr);
            }
        }

        if (present_available) {
            data->present_eid = xcb_generate_id(data->connection);
            xcb_present_select_input(data->connection,
                                     data->present_eid, data->window,
                                     XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);

            /* Present blits are deferred and race with the next frame's
             * begin() overwriting the back buffer.  Disable until we have
             * proper completion synchronization — the immediate cairo
             * blit path is flicker-free with our double buffer. */
            data->present_ok = False;
            data->present_serial = 0;
        }
    }

    return True;
}

static IswSurface
cairo_xcb_surface_init(Widget widget)
{
    IswSurface data;
    xcb_screen_t *screen;
    xcb_window_t window;
    uint8_t depth;

    screen = _IswXcbScreen(IswScreenOf(widget));
    window = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));

    /* Allocate surface state */
    data = (IswSurface)calloc(1, sizeof(*data));
    if (!data) {
        return NULL;
    }

    data->connection = _IswXcbConn(IswDisplayOf(widget));
    data->screen = screen;
    data->window = window;

    /* Use widget depth if set, otherwise use screen's root depth */
    depth = (widget->core.depth != 0) ? widget->core.depth : screen->root_depth;

    /* Find visual for depth, fall back to root depth visual */
    data->visual = _IswXcbFindVisual(screen, depth);
    if (!data->visual && depth != screen->root_depth) {
        data->visual = _IswXcbFindVisual(screen, screen->root_depth);
    }
    if (!data->visual) {
        fprintf(stderr, "ISWRenderCairoXCB: No usable visual found\n");
        free(data);
        return NULL;
    }

    data->save_count = 0;

    /* Defer surface creation if widget has no dimensions yet */
    if (widget->core.width == 0 || widget->core.height == 0 || window == 0) {
        data->deferred = True;
        data->surface = NULL;
        data->cairo_ctx = NULL;
        return data;
    }

    data->deferred = False;

    if (!_cairo_xcb_create_surface(data, widget)) {
        free(data);
        return NULL;
    }

    return data;
}

static void
cairo_xcb_surface_destroy(IswSurface data)
{
    if (!data) {
        return;
    }

    /* Destroy back buffer */
    if (data->back_ctx) {
        cairo_destroy(data->back_ctx);
    }
    if (data->back_surface) {
        cairo_surface_destroy(data->back_surface);
    }
    if (data->back_pixmap && data->connection) {
        xcb_free_pixmap(data->connection, data->back_pixmap);
    }

    /* Release Present event context */
    if (data->present_ok && data->connection) {
        xcb_present_select_input(data->connection,
                                 data->present_eid, data->window,
                                 XCB_PRESENT_EVENT_MASK_NO_EVENT);
    }

    /* Destroy window context and surface */
    if (data->window_ctx) {
        cairo_destroy(data->window_ctx);
    }
    if (data->surface) {
        cairo_surface_destroy(data->surface);
    }

    free(data);
}

/*
 * =================================================================
 * Frame Operations
 * =================================================================
 */

/*
 * Ensure data->back_{pixmap,surface,ctx} exist and can hold a (w x h)
 * physical-pixel region.  Reallocates with 25% slack, matching the windowed
 * path's hysteresis so interactive resize doesn't thrash the server pixmap.
 * sf is the HiDPI scale factor for the back surface's device scale.
 * Returns True if a usable back_ctx is available.
 */
static Boolean
_cairo_xcb_ensure_back(IswSurface data, Widget widget,
                       Dimension w, Dimension h, double sf)
{
    /* Windowless widgets use a client-side ARGB32 IMAGE surface so transparent
     * margins composite correctly onto the parent (a server pixmap at root
     * depth 24 has no alpha).  Windowed widgets use a server pixmap so the
     * Present extension can blit it. */
    Boolean want_image = (widget && !IswIsShell(widget));

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (data->back_surface == NULL ||
        data->back_is_image != want_image ||
        w > data->alloc_w || h > data->alloc_h ||
        w < data->alloc_w / 2 || h < data->alloc_h / 2) {
        if (data->back_ctx) {
            cairo_destroy(data->back_ctx);
            data->back_ctx = NULL;
        }
        if (data->back_surface) {
            cairo_surface_destroy(data->back_surface);
            data->back_surface = NULL;
        }
        if (data->back_pixmap) {
            xcb_free_pixmap(data->connection, data->back_pixmap);
            data->back_pixmap = 0;
        }

        Dimension aw = w + w / 4;
        Dimension ah = h + h / 4;
        if (aw < 1) aw = 1;
        if (ah < 1) ah = 1;

        if (want_image) {
            data->back_surface =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, aw, ah);
        } else {
            uint8_t depth = (widget->core.depth != 0)
                          ? widget->core.depth
                          : data->screen->root_depth;
            data->back_pixmap = xcb_generate_id(data->connection);
            xcb_create_pixmap(data->connection, depth, data->back_pixmap,
                              data->window, aw, ah);
            data->back_surface = cairo_xcb_surface_create(
                data->connection, data->back_pixmap, data->visual, aw, ah);
        }
        if (sf > 1.0)
            cairo_surface_set_device_scale(data->back_surface, sf, sf);
        data->back_ctx = cairo_create(data->back_surface);
        cairo_set_antialias(data->back_ctx, CAIRO_ANTIALIAS_GOOD);
        cairo_set_line_width(data->back_ctx, 1.0);
        cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_OVER);

        data->back_is_image = want_image;
        data->back_needs_clear = True;  /* fresh pixmap has undefined contents */
        data->alloc_w = aw;
        data->alloc_h = ah;
    } else if (!want_image && data->back_surface &&
               (data->back_w != w || data->back_h != h)) {
        /* XCB surfaces track a drawable; resize the view.  Image surfaces are
         * fixed-size — the alloc slack covers small changes, so leave as is. */
        cairo_xcb_surface_set_size(data->back_surface, w, h);
    }
    data->back_w = w;
    data->back_h = h;
    return data->back_ctx != NULL;
}

static void *
cairo_xcb_surface_begin(IswSurface data, Widget widget)
{
    /* Windowless widgets (surface-per-widget model): each owns a back surface
     * sized to its own footprint (content + border ring).  It draws at local
     * (0,0); the border ring is painted at the surface edges and content is
     * offset by the border width.  The composite pass later folds this surface
     * into the parent's surface.  No origin translate, no clip-out-children,
     * no ancestor-sized surface — the surface boundary IS the clip. */
    if (widget && !IswIsShell(widget)) {
        double sf;
        int bw;
        Dimension pw, ph;

        if (data->frame_depth > 0) {
            data->frame_depth++;
            return data->cairo_ctx;
        }
        if (data->deferred) {
            data->connection = _IswXcbConn(IswDisplayOf(widget));
            data->window = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
            if (widget->core.width == 0 || widget->core.height == 0 ||
                data->window == 0)
                return NULL;  /* not ready */
            if (!_cairo_xcb_create_surface(data, widget))
                return NULL;
            data->deferred = False;
        }
        if (!data->window_ctx)
            return NULL;

        sf = _IswGetScaleFactor(IswDisplayOf(widget));
        bw = (int) widget->core.border_width;
        /* Footprint = content + border ring, in physical pixels. */
        pw = (Dimension)((widget->core.width + 2 * bw) * sf + 0.5);
        ph = (Dimension)((widget->core.height + 2 * bw) * sf + 0.5);

        if (!_cairo_xcb_ensure_back(data, widget, pw, ph, sf)) {
            data->cairo_ctx = data->window_ctx;  /* degraded: no buffer */
            cairo_save(data->cairo_ctx);
            data->frame_depth = 1;
            return data->cairo_ctx;
        }

        /* Font state from the always-present window ctx. */
        {
            cairo_font_face_t *face = cairo_get_font_face(data->window_ctx);
            cairo_matrix_t font_matrix;
            cairo_get_font_matrix(data->window_ctx, &font_matrix);
            cairo_set_font_face(data->back_ctx, face);
            cairo_set_font_matrix(data->back_ctx, &font_matrix);
        }

        data->cairo_ctx = data->back_ctx;
        cairo_save(data->cairo_ctx);

        /* Clear the footprint to transparent ONLY on a freshly (re)allocated
         * surface — so unpainted margins composite correctly.  Do NOT clear on
         * every frame: widgets like Command paint in two separate begin/end
         * passes (Label content, then pressed-border) into the same surface,
         * and clearing each pass would wipe the first pass's content. */
        if (data->back_needs_clear) {
            cairo_save(data->cairo_ctx);
            cairo_set_operator(data->cairo_ctx, CAIRO_OPERATOR_CLEAR);
            cairo_paint(data->cairo_ctx);
            cairo_restore(data->cairo_ctx);
            data->back_needs_clear = False;
        }

        /* Border ring: footprint rect minus content rect, even-odd filled.
         * Skipped for widgets that paint their own border (Command and its
         * subclasses draw a rounded Cairo stroke from core.border_width) so
         * the border is not rendered twice. */
        if (bw > 0 &&
            !(IswIsSubclass(widget, simpleWidgetClass) &&
              ((SimpleWidget) widget)->simple.self_border)) {
            Pixel bp = widget->core.border_pixel;
            int cw_ = widget->core.width;
            int ch = widget->core.height;
            cairo_save(data->cairo_ctx);
            cairo_set_source_rgb(data->cairo_ctx,
                ((bp >> 16) & 0xff) / 255.0,
                ((bp >>  8) & 0xff) / 255.0,
                ((bp      ) & 0xff) / 255.0);
            cairo_set_fill_rule(data->cairo_ctx, CAIRO_FILL_RULE_EVEN_ODD);
            cairo_rectangle(data->cairo_ctx, 0, 0, cw_ + 2 * bw, ch + 2 * bw);
            cairo_rectangle(data->cairo_ctx, bw, bw, cw_, ch);
            cairo_fill(data->cairo_ctx);
            cairo_restore(data->cairo_ctx);
        }

        /* Content draws at local (0,0) = inside the border ring. */
        cairo_translate(data->cairo_ctx, bw, bw);
        cairo_rectangle(data->cairo_ctx, 0, 0,
                        widget->core.width, widget->core.height);
        cairo_clip(data->cairo_ctx);
        data->frame_depth = 1;
        return data->cairo_ctx;
    }

    /* Nested begin: a parent widget already started the frame — just
     * keep drawing into the same back buffer without blitting. */
    if (data->frame_depth > 0) {
        data->frame_depth++;
        return data->cairo_ctx;
    }

    /* Complete deferred initialization now that the widget has a window */
    if (data->deferred) {
        data->window = _IswXcbWindow(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
        if (widget->core.width == 0 || widget->core.height == 0 ||
            data->window == 0) {
            return NULL;  /* Still not ready */
        }
        /* Pick up the window that wasn't available at init time */
        if (data->window != 0 && data->window != (xcb_window_t)-1) {
            data->connection = _IswXcbConn(IswDisplayOf(widget));
        }
        if (!_cairo_xcb_create_surface(data, widget)) {
            return NULL;  /* Surface creation failed — skip this frame */
        }
        data->deferred = False;
    }

    /* Update window surface size — use physical pixels for surfaces
     * since the X window is at physical size. */
    if (widget && data->surface) {
        double sf = _IswGetScaleFactor(IswDisplayOf(widget));
        Dimension w = (Dimension)(widget->core.width * sf + 0.5);
        Dimension h = (Dimension)(widget->core.height * sf + 0.5);
        cairo_xcb_surface_set_size(data->surface, w, h);

        /* Ensure back buffer can hold widget dimensions (physical).
         *
         * To avoid destroying and recreating the server-side pixmap on
         * every ConfigureNotify during interactive resize, we over-
         * allocate by 25% and only reallocate when the widget outgrows
         * the allocation or shrinks below half of it. */
        if (data->back_pixmap == 0 ||
            w > data->alloc_w || h > data->alloc_h ||
            w < data->alloc_w / 2 || h < data->alloc_h / 2) {
            /* Tear down old back buffer */
            if (data->back_ctx) {
                cairo_destroy(data->back_ctx);
                data->back_ctx = NULL;
            }
            if (data->back_surface) {
                cairo_surface_destroy(data->back_surface);
                data->back_surface = NULL;
            }
            if (data->back_pixmap) {
                xcb_free_pixmap(data->connection, data->back_pixmap);
                data->back_pixmap = 0;
            }

            /* Over-allocate by 25% to absorb subsequent small resizes */
            Dimension aw = w + w / 4;
            Dimension ah = h + h / 4;
            if (aw < 1) aw = 1;
            if (ah < 1) ah = 1;

            /* Create new back buffer */
            uint8_t depth = (widget->core.depth != 0)
                          ? widget->core.depth
                          : data->screen->root_depth;
            data->back_pixmap = xcb_generate_id(data->connection);
            xcb_create_pixmap(data->connection, depth, data->back_pixmap,
                              data->window, aw, ah);
            data->back_surface = cairo_xcb_surface_create(
                data->connection, data->back_pixmap, data->visual, aw, ah);
            /* HiDPI: device scale for logical→physical mapping */
            if (sf > 1.0)
                cairo_surface_set_device_scale(data->back_surface, sf, sf);
            data->back_ctx = cairo_create(data->back_surface);
            cairo_set_antialias(data->back_ctx, CAIRO_ANTIALIAS_GOOD);
            cairo_set_line_width(data->back_ctx, 1.0);
            cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_OVER);

            /* Fill the new pixmap with the widget's background color.
             * The pixmap has undefined contents after creation.  The
             * non-Present path copies from the window surface which
             * the X server keeps filled with the background pixel, but
             * the Present path uses the back buffer as authoritative,
             * so we must initialize it ourselves. */
            {
                uint32_t bg = widget->core.background_pixel;
                cairo_save(data->back_ctx);
                cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_SOURCE);
                cairo_set_source_rgb(data->back_ctx,
                    ((bg >> 16) & 0xff) / 255.0,
                    ((bg >>  8) & 0xff) / 255.0,
                    ((bg      ) & 0xff) / 255.0);
                cairo_paint(data->back_ctx);
                cairo_restore(data->back_ctx);
            }

            data->alloc_w = aw;
            data->alloc_h = ah;
        } else if (data->back_surface &&
                   (data->back_w != w || data->back_h != h)) {
            /* Pixmap is large enough — just resize the Cairo surface
             * view so drawing is clipped to the widget area. */
            cairo_xcb_surface_set_size(data->back_surface, w, h);
        }
        data->back_w = w;
        data->back_h = h;
    }

    /* Swap cairo_ctx to target the back buffer for this frame */
    if (data->back_ctx) {
        /* Copy font state from window context to back buffer context */
        cairo_font_face_t *face = cairo_get_font_face(data->window_ctx);
        cairo_matrix_t font_matrix;
        cairo_get_font_matrix(data->window_ctx, &font_matrix);
        cairo_set_font_face(data->back_ctx, face);
        cairo_set_font_matrix(data->back_ctx, &font_matrix);

        /* Seed the back buffer so partial repaints preserve context. */
        if (!data->present_ok) {
            cairo_surface_flush(data->surface);
            cairo_set_source_surface(data->back_ctx, data->surface, 0, 0);
            cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_SOURCE);
            cairo_paint(data->back_ctx);
            cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_OVER);
        }

        data->cairo_ctx = data->back_ctx;
        cairo_save(data->cairo_ctx);

    } else {
        /* No back buffer — draw directly to window surface */
        cairo_save(data->cairo_ctx);
    }

    data->frame_depth = 1;
    return data->cairo_ctx;
}

/* Present-source accessor: hand the platform present_root the back buffer it
 * needs to blit, so the window blit itself lives in the platform layer (the
 * render backend no longer names a window).  Flushes the back surface and bumps
 * the present serial.  See ISWRenderPrivate.h. */
Boolean
_ISWRenderSurfacePresentSource(IswSurface data,
                               cairo_surface_t **back_cairo,
                               void **window_cr,
                               xcb_pixmap_t *back_pixmap,
                               uint32_t *present_serial)
{
    if (!data || !data->back_surface)
        return False;
    cairo_surface_flush(data->back_surface);
    if (data->surface)
        cairo_surface_flush(data->surface);
    if (back_cairo)     *back_cairo = data->back_surface;
    if (window_cr)      *window_cr = data->window_ctx;
    /* Present path only when the back buffer is a server pixmap and Present is
       usable; otherwise 0 tells the caller to use the cairo source path. */
    if (back_pixmap)    *back_pixmap = (data->present_ok ? data->back_pixmap : 0);
    if (present_serial) *present_serial = ++data->present_serial;
    return True;
}

static void
cairo_xcb_surface_end(IswSurface data, Widget widget, IswWindow window)
{
    /* Nested end: parent frame still active — don't present yet. */
    if (data->frame_depth > 1) {
        data->frame_depth--;
        return;
    }

    /* Windowless (surface-per-widget): the widget painted into its own back
     * surface.  Just undo the begin() save and flush — the composite pass
     * (ISWRenderCompositeSubtree) folds this surface up the tree and presents
     * the windowed root once.  No present here. */
    if (widget && !IswIsShell(widget)) {
        data->frame_depth = 0;
        if (data->cairo_ctx)
            cairo_restore(data->cairo_ctx);
        if (data->back_surface)
            cairo_surface_flush(data->back_surface);
        return;
    }

    data->frame_depth = 0;

    if (!data->cairo_ctx)
        return;

    cairo_restore(data->cairo_ctx);

    /* Present this windowed widget's back surface to its window via the platform
       root-present op (the blit lives in the platform layer now). */
    if (data->back_surface) {
        cairo_surface_flush(data->back_surface);
        if (data->surface)
            cairo_surface_flush(data->surface);
        _IswPlatformPresentRoot(IswDisplayOf(widget), window,
                                (IswSurface) data,
                                (int) data->back_w, (int) data->back_h);
    }
}

/* fill_background: paint the composite target's surface with the widget's
 * background pixel.  Used on a windowed composite root before folding children
 * so uncovered gaps show the background rather than a bare (black) window. */
static void
cairo_xcb_fill_background(IswSurface data, Widget widget)
{
    cairo_t *dctx;
    Pixel bg;
    double sf;

    if (!data) return;
    if (widget == NULL) return;

    sf = _IswGetScaleFactor(IswDisplayOf(widget));

    /* The window surface was sized when the surface was created — possibly
       before the widget reached its final laid-out size.  Track the current
       window size so children composite onto a full-size surface and are not
       clipped to a stale (too-small) extent. */
    if (data->surface) {
        Dimension pw = (Dimension)(widget->core.width * sf + 0.5);
        Dimension ph = (Dimension)(widget->core.height * sf + 0.5);
        if (pw < 1) pw = 1;
        if (ph < 1) ph = 1;
        cairo_xcb_surface_set_size(data->surface, pw, ph);
    }

    /* Composite into a server-pixmap BACK buffer, never straight into the
       window: the whole pass (background fill + folding every child surface)
       must be invisible until the finished frame is blitted once by present().
       Painting directly to the window shows the cleared background and each
       child popping in — the flicker.  Ensure the back buffer here so the
       lazy composite root (which never gets a begin()/end()) is buffered.
       Re-ensure EVERY pass at the current window size: the window can grow
       after the buffer's first allocation, and a too-small buffer would clip
       away (leave transparent) widgets near the right/bottom edges — e.g. the
       full-width status bar at the bottom. */
    {
        Dimension pw = (Dimension)(widget->core.width * sf + 0.5);
        Dimension ph = (Dimension)(widget->core.height * sf + 0.5);
        _cairo_xcb_ensure_back(data, widget, pw, ph, sf);
    }
    dctx = data->back_ctx ? data->back_ctx : data->window_ctx;
    if (dctx == NULL) return;

    bg = widget->core.background_pixel;
    cairo_save(dctx);
    cairo_set_operator(dctx, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(dctx,
        ((bg >> 16) & 0xff) / 255.0,
        ((bg >>  8) & 0xff) / 255.0,
        ((bg      ) & 0xff) / 255.0);
    cairo_paint(dctx);
    cairo_restore(dctx);
}

/* composite_onto: paint src's (windowless) back surface onto dst's back surface
 * at the child's position within dst's content area.  dst may be the windowed
 * root (content at 0,0) or a windowless parent (content offset by its border).
 * src's surface footprint already includes src's own border ring at (0,0). */
static void
cairo_xcb_composite_onto(IswSurface dd, Widget dst_widget,
                         IswSurface sd, Widget src_widget, int x, int y)
{
    cairo_t *dctx;
    int dst_content_off;

    if (!dd || !sd || sd->back_surface == NULL)
        return;

    /* Destination drawing context: a windowless parent composites into its own
     * back_ctx; the windowed root composites into its back_ctx (or window). */
    dctx = dd->back_ctx ? dd->back_ctx : dd->window_ctx;
    if (dctx == NULL)
        return;

    /* dst content origin within dst's surface: windowless parents reserve a
     * border ring at the top-left of their surface; windowed roots do not. */
    dst_content_off = (dst_widget && !IswIsShell(dst_widget))
                    ? (int) dst_widget->core.border_width : 0;

    cairo_surface_flush(sd->back_surface);
    cairo_save(dctx);
    /* Clip to dst's content rectangle so a child larger than its parent (e.g. a
       Viewport's scrolled content) does not overflow the parent's bounds — this
       is the clipping the X server used to enforce via child windows. */
    if (dst_widget != NULL) {
        cairo_rectangle(dctx, dst_content_off, dst_content_off,
                        (double) dst_widget->core.width,
                        (double) dst_widget->core.height);
        cairo_clip(dctx);
    }
    /* Additional composite clip the parent imposed on this child (Viewport
       confining its scrolled content to the clip region).  The clip is given in
       the PARENT's content coordinate frame.  The child composites at dst
       position (x,y) = parent_origin + child.x/y, so the parent's content frame
       sits at (x - child.x, y - child.y) within dst.  Offsetting the clip by
       that frame origin is essential when the parent is context-less and folds
       directly into the root (the clip would otherwise be applied at the root
       origin instead of the parent's on-screen position, letting the child
       overflow — e.g. scrolled Viewport content bleeding over the status bar). */
    if (IswIsWidget(src_widget) && src_widget->core.composite_clip &&
        src_widget->core.composite_clip_w > 0) {
        int frame_x = x - (int) src_widget->core.x;
        int frame_y = y - (int) src_widget->core.y;
        cairo_rectangle(dctx,
                        dst_content_off + frame_x + src_widget->core.composite_clip_x,
                        dst_content_off + frame_y + src_widget->core.composite_clip_y,
                        (double) src_widget->core.composite_clip_w,
                        (double) src_widget->core.composite_clip_h);
        cairo_clip(dctx);
    }

    /* Confine the source to ITS OWN widget footprint at its composited
       position.  A widget's back surface is allocated with slack and may hold
       pixels (scrollbars, scrolled content, AA bleed) beyond the widget's
       logical rectangle; without this clip those pixels overflow into adjacent
       siblings depending on composite (z) order — e.g. the content Viewport,
       folded after the bottom status bar, bleeding over it.  Clipping each
       source to its footprint makes adjacent widgets non-overlapping regardless
       of fold order. */
    if (IswIsWidget(src_widget)) {
        int bw2 = (int) src_widget->core.border_width * 2;
        cairo_rectangle(dctx,
                        dst_content_off + x,
                        dst_content_off + y,
                        (double) (src_widget->core.width + bw2),
                        (double) (src_widget->core.height + bw2));
        cairo_clip(dctx);
    }

    cairo_set_source_surface(dctx, sd->back_surface,
                             dst_content_off + x, dst_content_off + y);
    /* OVER reads the destination and blends per pixel — the dominant CPU cost of
       the composite pass.  Most widget surfaces are opaque across their footprint
       (background-filled, then painted over), so the blend is wasted: SOURCE is a
       straight copy (no destination read, often a blit) and produces an identical
       result.  Keep OVER only where the source genuinely carries alpha inside its
       footprint: self_border widgets (Command et al.) draw rounded corners with
       transparent gaps, and a composite clip narrower than the footprint means we
       are folding a sub-region that must let the destination show through. */
    {
        Boolean has_alpha =
            (IswIsSubclass(src_widget, simpleWidgetClass) &&
             ((SimpleWidget) src_widget)->simple.self_border) ||
            (IswIsWidget(src_widget) && src_widget->core.composite_clip &&
             src_widget->core.composite_clip_w > 0);
        cairo_set_operator(dctx,
                           has_alpha ? CAIRO_OPERATOR_OVER
                                     : CAIRO_OPERATOR_SOURCE);
    }
    cairo_paint(dctx);
    cairo_restore(dctx);
}


/*
 * =================================================================
 * State Management
 * =================================================================
 */

static void
cairo_xcb_save(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_save(data->cairo_ctx);
    data->save_count++;
}

static void
cairo_xcb_restore(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    if (data->save_count > 0) {
        cairo_restore(data->cairo_ctx);
        data->save_count--;
    }
}

/*
 * =================================================================
 * Color Management
 * =================================================================
 */

static void
cairo_xcb_set_color(ISWRenderContext *ctx, Pixel pixel)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    double r, g, b;

    ctx->current_color = pixel;
    if (!data->cairo_ctx) return;
    ISWRenderPixelToRGB(ctx, pixel, &r, &g, &b);
    cairo_set_source_rgb(data->cairo_ctx, r, g, b);
}

static void
cairo_xcb_set_color_rgba(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_set_source_rgba(data->cairo_ctx, r, g, b, a);
}

static void
cairo_xcb_set_line_width(ISWRenderContext *ctx, double width)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    ctx->line_width = width;
    if (!data->cairo_ctx) return;
    cairo_set_line_width(data->cairo_ctx, width);
}

/*
 * =================================================================
 * Drawing Primitives
 * =================================================================
 */

static void
cairo_xcb_fill_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    /* Draw four individual lines to form the rectangle.
     * Using separate stroke calls to ensure rendering.
     */
    /* Top */
    cairo_move_to(data->cairo_ctx, x, y);
    cairo_line_to(data->cairo_ctx, x + w, y);
    cairo_stroke(data->cairo_ctx);
    
    /* Right */
    cairo_move_to(data->cairo_ctx, x + w, y);
    cairo_line_to(data->cairo_ctx, x + w, y + h);
    cairo_stroke(data->cairo_ctx);
    
    /* Bottom */
    cairo_move_to(data->cairo_ctx, x + w, y + h);
    cairo_line_to(data->cairo_ctx, x, y + h);
    cairo_stroke(data->cairo_ctx);
    
    /* Left */
    cairo_move_to(data->cairo_ctx, x, y + h);
    cairo_line_to(data->cairo_ctx, x, y);
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_fill_polygon(ISWRenderContext *ctx, IswPoint *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    int i;

    if (!data->cairo_ctx || num < 3) {
        return;
    }
    
    cairo_move_to(data->cairo_ctx, points[0].x, points[0].y);
    for (i = 1; i < num; i++) {
        cairo_line_to(data->cairo_ctx, points[i].x, points[i].y);
    }
    cairo_close_path(data->cairo_ctx);
    cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_polygon(ISWRenderContext *ctx, IswPoint *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    int i;

    if (!data->cairo_ctx || num < 2) {
        return;
    }
    
    cairo_move_to(data->cairo_ctx, points[0].x, points[0].y);
    for (i = 1; i < num; i++) {
        cairo_line_to(data->cairo_ctx, points[i].x, points[i].y);
    }
    cairo_close_path(data->cairo_ctx);  /* Close the polygon back to start */
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_draw_line(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_move_to(data->cairo_ctx, x1, y1);
    cairo_line_to(data->cairo_ctx, x2, y2);
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_draw_arc(ISWRenderContext *ctx, int x, int y, int w, int h,
                  double angle1, double angle2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    double cx = x + w / 2.0;
    double cy = y + h / 2.0;
    double rx = w / 2.0;
    double ry = h / 2.0;
    
    /* Save state for transformation */
    cairo_save(data->cairo_ctx);
    
    /* Transform for ellipse */
    cairo_translate(data->cairo_ctx, cx, cy);
    cairo_scale(data->cairo_ctx, rx, ry);
    
    /* Draw arc (Cairo uses radians) */
    cairo_arc(data->cairo_ctx, 0, 0, 1.0, angle1, angle2);
    
    /* Restore and stroke */
    cairo_restore(data->cairo_ctx);
    cairo_stroke(data->cairo_ctx);
}

/* Append a rounded-rectangle path to cr. */
static void
cairo_xcb_rounded_path(cairo_t *cr, int x, int y, int w, int h, double radius)
{
    double max_r = (w < h ? w : h) / 2.0;
    double x0 = x, y0 = y, r = radius;

    if (r > max_r) r = max_r;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x0 + w - r, y0 + r,     r, -M_PI/2, 0);
    cairo_arc(cr, x0 + w - r, y0 + h - r, r, 0,        M_PI/2);
    cairo_arc(cr, x0 + r,     y0 + h - r, r, M_PI/2,   M_PI);
    cairo_arc(cr, x0 + r,     y0 + r,     r, M_PI,      3*M_PI/2);
    cairo_close_path(cr);
}

static void
cairo_xcb_fill_rounded_rect(ISWRenderContext *ctx,
                            int x, int y, int w, int h, double radius)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_xcb_rounded_path(data->cairo_ctx, x, y, w, h, radius);
    cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_rounded_rect(ISWRenderContext *ctx,
                              int x, int y, int w, int h, double radius,
                              double stroke_width)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_xcb_rounded_path(data->cairo_ctx, x, y, w, h, radius);
    cairo_set_line_width(data->cairo_ctx, stroke_width);
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_fill_stroke_rounded_rect(ISWRenderContext *ctx,
                                   int x, int y, int w, int h, double radius,
                                   double fill_alpha, double stroke_width)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    double r, g, b;

    if (!data->cairo_ctx) return;
    ISWRenderPixelToRGB(ctx, ctx->current_color, &r, &g, &b);

    cairo_xcb_rounded_path(data->cairo_ctx, x, y, w, h, radius);
    cairo_set_source_rgba(data->cairo_ctx, r, g, b, fill_alpha);
    cairo_fill_preserve(data->cairo_ctx);

    cairo_set_source_rgb(data->cairo_ctx, r, g, b);
    cairo_set_line_width(data->cairo_ctx, stroke_width);
    cairo_stroke(data->cairo_ctx);
}

/* Paint an RGBA image's alpha as a mask in the current foreground colour. */
static void
cairo_xcb_draw_image_masked(ISWRenderContext *ctx, Pixel foreground,
                            const unsigned char *rgba,
                            unsigned int img_w, unsigned int img_h,
                            int dst_x, int dst_y,
                            unsigned int dst_w, unsigned int dst_h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    cairo_t *cr;
    unsigned int stride, i;
    unsigned char *a8_buf;
    cairo_surface_t *mask_surface;

    if (!data->cairo_ctx || !rgba || img_w == 0 || img_h == 0)
        return;
    cr = data->cairo_ctx;

    stride = cairo_format_stride_for_width(CAIRO_FORMAT_A8, (int)img_w);
    a8_buf = (unsigned char *)calloc(stride * img_h, 1);
    if (!a8_buf)
        return;

    for (i = 0; i < img_w * img_h; i++) {
        unsigned int row = i / img_w;
        unsigned int col = i % img_w;
        a8_buf[row * stride + col] = rgba[i * 4 + 3];
    }

    mask_surface = cairo_image_surface_create_for_data(
        a8_buf, CAIRO_FORMAT_A8, (int)img_w, (int)img_h, (int)stride);

    if (cairo_surface_status(mask_surface) == CAIRO_STATUS_SUCCESS) {
        cairo_xcb_set_color(ctx, foreground);
        cairo_save(cr);
        if (dst_w != img_w || dst_h != img_h) {
            cairo_translate(cr, dst_x, dst_y);
            cairo_scale(cr,
                        (double)dst_w / (double)img_w,
                        (double)dst_h / (double)img_h);
            cairo_mask_surface(cr, mask_surface, 0, 0);
        } else {
            cairo_mask_surface(cr, mask_surface, dst_x, dst_y);
        }
        cairo_restore(cr);
    }

    cairo_surface_destroy(mask_surface);
    free(a8_buf);
}

/*
 * =================================================================
 * Text Rendering
 * =================================================================
 */

static void
cairo_xcb_draw_string(ISWRenderContext *ctx, const char *text, int len,
                     int x, int y)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    char *null_term;

    if (!data->cairo_ctx) return;
    /* Cairo expects null-terminated string */
    null_term = (char*)malloc(len + 1);
    if (!null_term) {
        return;
    }
    
    memcpy(null_term, text, len);
    null_term[len] = '\0';
    
    /* Draw text */
    cairo_move_to(data->cairo_ctx, x, y);
    cairo_show_text(data->cairo_ctx, null_term);

    free(null_term);
}

static int
cairo_xcb_text_width(ISWRenderContext *ctx, const char *text, int len)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    cairo_text_extents_t extents;
    char *null_term;
    int width;

    if (!data->cairo_ctx) return len * 8;
    null_term = (char*)malloc(len + 1);
    if (!null_term) {
        return len * 8;
    }

    memcpy(null_term, text, len);
    null_term[len] = '\0';

    /* With cairo_surface_set_device_scale, cairo_text_extents returns
     * logical pixel values automatically. */
    cairo_text_extents(data->cairo_ctx, null_term, &extents);
    double adv = ceil(extents.x_advance);
    width = (int)adv;

    free(null_term);

    return width;
}

static int
cairo_xcb_text_height(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    cairo_font_extents_t extents;

    if (!data->cairo_ctx) return 12;
    cairo_font_extents(data->cairo_ctx, &extents);

    return (int)(extents.ascent + extents.descent);
}

static void
cairo_xcb_set_font(ISWRenderContext *ctx, IswFontStruct *font)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;

    /* HiDPI: set font at logical size — the Cairo scale transform on
     * the render surface handles physical magnification. */
    _ISWSetCairoFontFromXFont(data->cairo_ctx, font, 1.0);

    /* Keep both contexts in sync so text queries work outside begin/end */
    if (data->back_ctx && data->cairo_ctx != data->back_ctx)
        _ISWSetCairoFontFromXFont(data->back_ctx, font, 1.0);
    if (data->window_ctx && data->cairo_ctx != data->window_ctx)
        _ISWSetCairoFontFromXFont(data->window_ctx, font, 1.0);
}

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

static void
cairo_xcb_set_clip_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_reset_clip(data->cairo_ctx);
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_clip(data->cairo_ctx);
}

static void
cairo_xcb_clear_clip(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->cairo_ctx) return;
    cairo_reset_clip(data->cairo_ctx);
}

/*
 * =================================================================
 * Pixmap/Bitmap Rendering
 * =================================================================
 */

static void
cairo_xcb_copy_area(ISWRenderContext *ctx,
                    int src_x, int src_y,
                    int dst_x, int dst_y,
                    unsigned int width, unsigned int height)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;

    if (!data->surface) return;
    /*
     * For within-window scrolling, flush the Cairo surface so all pending
     * drawing is committed to the X window, then use XCB copy_area for the
     * server-side pixel copy, then mark the surface dirty so Cairo re-reads.
     */
    cairo_surface_flush(data->surface);
    xcb_flush(data->connection);

    /* Create a temporary xcb_gcontext_t for the copy */
    xcb_gcontext_t gc = xcb_generate_id(data->connection);
    uint32_t gc_mask = XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t gc_vals[] = { 0 };
    xcb_create_gc(data->connection, gc, data->window, gc_mask, gc_vals);

    xcb_copy_area(data->connection, data->window, data->window,
                  gc, src_x, src_y, dst_x, dst_y, width, height);
    xcb_flush(data->connection);

    xcb_free_gc(data->connection, gc);

    /* Tell Cairo the surface contents changed underneath it */
    cairo_surface_mark_dirty(data->surface);
}

/*
 * =================================================================
 * RGBA Image Rendering
 * =================================================================
 */

static void
cairo_xcb_draw_image_rgba(ISWRenderContext *ctx,
                          const unsigned char *rgba,
                          unsigned int img_w, unsigned int img_h,
                          int dst_x, int dst_y,
                          unsigned int dst_w, unsigned int dst_h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    cairo_surface_t *img_surface;
    unsigned char *cairo_buf;
    unsigned int stride, i;

    if (!data->cairo_ctx || !rgba || img_w == 0 || img_h == 0)
        return;

    /*
     * Cairo ARGB32 format is native-endian 32-bit: 0xAARRGGBB (premultiplied).
     * nanosvg produces straight RGBA (R,G,B,A bytes). Convert in-place copy.
     */
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, (int)img_w);
    cairo_buf = (unsigned char *)malloc(stride * img_h);
    if (!cairo_buf)
        return;

    for (i = 0; i < img_w * img_h; i++) {
        unsigned char r = rgba[i * 4 + 0];
        unsigned char g = rgba[i * 4 + 1];
        unsigned char b = rgba[i * 4 + 2];
        unsigned char a = rgba[i * 4 + 3];
        unsigned int row = i / img_w;
        unsigned int col = i % img_w;
        uint32_t *pixel = (uint32_t *)(cairo_buf + row * stride + col * 4);

        /* Premultiply alpha */
        if (a == 255) {
            *pixel = (255u << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        } else if (a == 0) {
            *pixel = 0;
        } else {
            *pixel = ((uint32_t)a << 24) |
                     ((uint32_t)((r * a + 127) / 255) << 16) |
                     ((uint32_t)((g * a + 127) / 255) << 8) |
                     (uint32_t)((b * a + 127) / 255);
        }
    }

    img_surface = cairo_image_surface_create_for_data(
        cairo_buf, CAIRO_FORMAT_ARGB32, (int)img_w, (int)img_h, (int)stride);

    if (cairo_surface_status(img_surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(img_surface);
        free(cairo_buf);
        return;
    }

    cairo_save(data->cairo_ctx);

    /* HiDPI: if the image was rasterized at physical resolution, set
     * device scale on the image surface so Cairo maps it 1:1 to the
     * device-scaled destination without resampling. */
    if (dst_w != img_w || dst_h != img_h) {
        double sx = (double)img_w / (double)dst_w;
        double sy = (double)img_h / (double)dst_h;
        cairo_surface_set_device_scale(img_surface, sx, sy);
    }
    cairo_set_source_surface(data->cairo_ctx, img_surface, dst_x, dst_y);
    cairo_paint(data->cairo_ctx);
    cairo_restore(data->cairo_ctx);

    cairo_surface_destroy(img_surface);
    free(cairo_buf);

    /* Restore the previous source color */
    cairo_xcb_set_color(ctx, ctx->current_color);
}

/*
 * =================================================================
 * Advanced Features
 * =================================================================
 */

static Boolean
cairo_xcb_set_gradient(ISWRenderContext *ctx, double x1, double y1, double x2, double y2,
                      Pixel color1, Pixel color2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    cairo_pattern_t *gradient;
    if (!data->cairo_ctx) return False;
    double r1, g1, b1, r2, g2, b2;
    
    /* Convert pixels to RGB */
    ISWRenderPixelToRGB(ctx, color1, &r1, &g1, &b1);
    ISWRenderPixelToRGB(ctx, color2, &r2, &g2, &b2);
    
    /* Create linear gradient */
    gradient = cairo_pattern_create_linear(x1, y1, x2, y2);
    cairo_pattern_add_color_stop_rgb(gradient, 0.0, r1, g1, b1);
    cairo_pattern_add_color_stop_rgb(gradient, 1.0, r2, g2, b2);
    
    /* Set as source */
    cairo_set_source(data->cairo_ctx, gradient);
    cairo_pattern_destroy(gradient);
    
    return True;
}

static void
cairo_xcb_push_group(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data;

    if (!ctx || !ctx->surface)
        return;

    data = (ISWRenderCairoXCBData*)ctx->surface;
    if (data->cairo_ctx)
        cairo_push_group(data->cairo_ctx);
}

static void
cairo_xcb_pop_group_alpha(ISWRenderContext *ctx, double alpha)
{
    ISWRenderCairoXCBData *data;

    if (!ctx || !ctx->surface)
        return;

    data = (ISWRenderCairoXCBData*)ctx->surface;
    if (data->cairo_ctx) {
        cairo_pop_group_to_source(data->cairo_ctx);
        cairo_paint_with_alpha(data->cairo_ctx, alpha);
    }
}

/* ---- Path construction / painting ---- */

static void
cairo_xcb_path_begin(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_new_path(data->cairo_ctx);
}

static void
cairo_xcb_path_new_sub_path(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_new_sub_path(data->cairo_ctx);
}

static void
cairo_xcb_path_move_to(ISWRenderContext *ctx, double x, double y)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_move_to(data->cairo_ctx, x, y);
}

static void
cairo_xcb_path_line_to(ISWRenderContext *ctx, double x, double y)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_line_to(data->cairo_ctx, x, y);
}

static void
cairo_xcb_path_arc(ISWRenderContext *ctx, double cx, double cy, double r,
                   double angle1, double angle2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_arc(data->cairo_ctx, cx, cy, r, angle1, angle2);
}

static void
cairo_xcb_path_rectangle(ISWRenderContext *ctx,
                         double x, double y, double w, double h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
}

static void
cairo_xcb_path_close(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_close_path(data->cairo_ctx);
}

static void
cairo_xcb_fill_path(ISWRenderContext *ctx, Boolean preserve)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    if (preserve)
        cairo_fill_preserve(data->cairo_ctx);
    else
        cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_path(ISWRenderContext *ctx, Boolean preserve)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    if (preserve)
        cairo_stroke_preserve(data->cairo_ctx);
    else
        cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_clip_path(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_clip(data->cairo_ctx);
}

static void
cairo_xcb_paint(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_paint(data->cairo_ctx);
}

/* ---- Path / draw state ---- */

static void
cairo_xcb_set_fill_rule(ISWRenderContext *ctx, ISWFillRule rule)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_set_fill_rule(data->cairo_ctx,
                        rule == ISW_FILL_RULE_EVEN_ODD
                            ? CAIRO_FILL_RULE_EVEN_ODD
                            : CAIRO_FILL_RULE_WINDING);
}

static void
cairo_xcb_set_dash(ISWRenderContext *ctx, const double *dashes,
                   int num_dashes, double offset)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_set_dash(data->cairo_ctx, dashes, num_dashes, offset);
}

static void
cairo_xcb_set_operator(ISWRenderContext *ctx, ISWOperator op)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_set_operator(data->cairo_ctx,
                       op == ISW_OPERATOR_DIFFERENCE
                           ? CAIRO_OPERATOR_DIFFERENCE
                           : CAIRO_OPERATOR_OVER);
}

/* ---- Affine transform ---- */

static void
cairo_xcb_translate(ISWRenderContext *ctx, double tx, double ty)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_translate(data->cairo_ctx, tx, ty);
}

static void
cairo_xcb_scale(ISWRenderContext *ctx, double sx, double sy)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_scale(data->cairo_ctx, sx, sy);
}

static void
cairo_xcb_rotate(ISWRenderContext *ctx, double radians)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx) return;
    cairo_rotate(data->cairo_ctx, radians);
}

static void
cairo_xcb_show_text(ISWRenderContext *ctx, const char *text)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->surface;
    if (!data->cairo_ctx || !text) return;
    cairo_show_text(data->cairo_ctx, text);
}

/*
 * =================================================================
 * Operations Vtables
 * =================================================================
 */

/*
 * =================================================================
 * Visual / Pixel -> RGB  (backend-private; was in the neutral dispatcher)
 * =================================================================
 *
 * The neutral ISWRenderContext no longer carries any native display handle.
 * The backend owns the connection/screen/visual in its IswSurface and decodes
 * pixels here; ISWRenderPixelToRGB forwards to the .pixel_to_rgb op below.
 */

/* Find the XCB visual for a given depth on a screen (first visual at depth). */
xcb_visualtype_t *
_IswXcbFindVisual(xcb_screen_t *screen, uint8_t depth)
{
    xcb_depth_iterator_t depth_iter;
    xcb_visualtype_iterator_t visual_iter;

    if (!screen)
        return NULL;

    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter)) {
        if (depth_iter.data->depth == depth) {
            visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
            if (visual_iter.rem)
                return visual_iter.data;
        }
    }
    return NULL;
}

/* Decode one channel: extract the masked bits and scale to 0.0-1.0. */
static double
cairo_xcb_channel(Pixel pixel, uint32_t mask)
{
    if (mask == 0)
        return 0.0;
    while (!(mask & 1)) {
        pixel >>= 1;
        mask >>= 1;
    }
    return (pixel & mask) / (double)mask;
}

/* Synchronous server query for a palette (non-TrueColor) pixel's RGB. */
static int
cairo_xcb_query_color(xcb_connection_t *conn, xcb_colormap_t cmap,
                      IswColor *color)
{
    xcb_query_colors_cookie_t cookie;
    xcb_query_colors_reply_t *reply;
    xcb_rgb_t *rgb;
    uint32_t pixel;

    if (!conn || !color)
        return 0;

    pixel = color->pixel;
    cookie = xcb_query_colors(conn, cmap, 1, &pixel);
    reply = xcb_query_colors_reply(conn, cookie, NULL);
    if (!reply)
        return 0;

    rgb = xcb_query_colors_colors(reply);
    if (rgb) {
        color->red = rgb->red;
        color->green = rgb->green;
        color->blue = rgb->blue;
    }
    free(reply);
    return 1;
}

/* .pixel_to_rgb op: decode a pixel using the surface's own visual. */
static void
cairo_xcb_pixel_to_rgb(ISWRenderContext *ctx, Pixel pixel,
                       double *r, double *g, double *b)
{
    ISWRenderCairoXCBData *data;
    xcb_visualtype_t *visual;

    if (!ctx || !r || !g || !b)
        return;

    data = (ISWRenderCairoXCBData *) ctx->surface;
    visual = data ? data->visual : NULL;

    /* Fast path: TrueColor/DirectColor pixels encode RGB in the channel masks,
       decoded locally with no server traffic. */
    if (visual &&
        (visual->_class == XCB_VISUAL_CLASS_TRUE_COLOR ||
         visual->_class == XCB_VISUAL_CLASS_DIRECT_COLOR)) {
        *r = cairo_xcb_channel(pixel, visual->red_mask);
        *g = cairo_xcb_channel(pixel, visual->green_mask);
        *b = cairo_xcb_channel(pixel, visual->blue_mask);
        return;
    }

    /* Palette visual: ask the server for the colormap entry's RGB. */
    if (data && data->connection && data->screen) {
        IswColor color;
        color.pixel = pixel;
        if (cairo_xcb_query_color(data->connection,
                                  data->screen->default_colormap, &color)) {
            *r = color.red / 65535.0;
            *g = color.green / 65535.0;
            *b = color.blue / 65535.0;
            return;
        }
    }

    /* Last-resort fallback: assume packed 0xRRGGBB. */
    *r = ((pixel >> 16) & 0xFF) / 255.0;
    *g = ((pixel >> 8) & 0xFF) / 255.0;
    *b = (pixel & 0xFF) / 255.0;
}

/*
 * =================================================================
 * Fonts (fontconfig + FreeType + cairo-ft) — backend-private.
 * Discovery via fontconfig, loading via FreeType, Cairo via cairo-ft.
 * The widget-keyed measurement entry points are published on the platform
 * render ops; the neutral ISWScaled* wrappers forward to them.
 * =================================================================
 */

static FT_Library _ft_library = NULL;

typedef struct _ISWFontCacheEntry {
    struct _ISWFontCacheEntry *next;
    char *pattern_key;          /* "family:weight:slant" */
    cairo_font_face_t *cr_face;
    FT_Face ft_face;
} _ISWFontCacheEntry;

static _ISWFontCacheEntry *_font_cache = NULL;

static void
_ISWInitFreeType(void)
{
    if (!_ft_library)
        FT_Init_FreeType(&_ft_library);
}

/* Resolve a font description to a (cached) Cairo font face. */
static cairo_font_face_t *
_ISWResolveFontFace(const char *family, int weight, int slant)
{
    char key[256];
    _ISWFontCacheEntry *entry;
    FcPattern *pattern = NULL, *match = NULL;
    FcResult result;
    FcChar8 *font_file = NULL;
    FT_Face ft_face = NULL;
    cairo_font_face_t *cr_face;

    snprintf(key, sizeof(key), "%s:%d:%d", family ? family : "Sans",
             weight, slant);

    for (entry = _font_cache; entry; entry = entry->next)
        if (strcmp(entry->pattern_key, key) == 0)
            return entry->cr_face;

    _ISWInitFreeType();

    pattern = FcPatternCreate();
    FcPatternAddString(pattern, FC_FAMILY,
                       (const FcChar8 *)(family ? family : "Sans"));
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcPatternAddInteger(pattern, FC_SLANT, slant);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    match = FcFontMatch(NULL, pattern, &result);
    if (!match) {
        FcPatternDestroy(pattern);
        return NULL;
    }
    if (FcPatternGetString(match, FC_FILE, 0, &font_file) != FcResultMatch) {
        FcPatternDestroy(match);
        FcPatternDestroy(pattern);
        return NULL;
    }
    if (FT_New_Face(_ft_library, (const char *)font_file, 0, &ft_face) != 0) {
        FcPatternDestroy(match);
        FcPatternDestroy(pattern);
        return NULL;
    }

    cr_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);

    entry = (_ISWFontCacheEntry *)malloc(sizeof(_ISWFontCacheEntry));
    entry->pattern_key = strdup(key);
    entry->cr_face = cr_face;
    entry->ft_face = ft_face;  /* kept alive — Cairo references it */
    entry->next = _font_cache;
    _font_cache = entry;

    FcPatternDestroy(match);
    FcPatternDestroy(pattern);
    return cr_face;
}

/* Configure a Cairo context with the TTF face + size from an IswFontStruct.
   Declared in ISWRenderPrivate.h; used by the backend's draw/text path too. */
void
_ISWSetCairoFontFromXFont(cairo_t *cr, IswFontStruct *font, double scale)
{
    cairo_font_face_t *face;
    double size;

    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;

    face = _ISWResolveFontFace(family, weight, slant);
    if (face)
        cairo_set_font_face(cr, face);

    if (font && font->pt_size > 0)
        size = font->pt_size * (96.0 / 72.0) * scale;
    else if (font)
        size = (double)(font->ascent + font->descent) * scale;
    else
        size = 12.0 * scale;

    if (size < 1.0)
        size = 12.0 * scale;

    cairo_set_font_size(cr, size);
}

/* Persistent process-wide measurement context (avoids per-call surface churn). */
static cairo_surface_t *_measure_surf = NULL;
static cairo_t *_measure_cr = NULL;
static double _cached_font_size = -1.0;
static const char *_cached_font_family = NULL;
static int _cached_font_weight = -1;
static int _cached_font_slant = -1;
static cairo_font_extents_t _cached_font_extents;
static double _measure_device_scale = 0.0;

static cairo_t *
_ISWGetMeasureCR(double device_scale)
{
    if (!_measure_cr) {
        cairo_font_face_t *face;

        _measure_surf = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
        if (device_scale > 1.0)
            cairo_surface_set_device_scale(_measure_surf, device_scale, device_scale);
        _measure_device_scale = device_scale;
        _measure_cr = cairo_create(_measure_surf);

        face = _ISWResolveFontFace("Sans", FC_WEIGHT_NORMAL, FC_SLANT_ROMAN);
        if (face)
            cairo_set_font_face(_measure_cr, face);
        else
            cairo_select_font_face(_measure_cr, "Sans",
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_NORMAL);
    } else if (device_scale != _measure_device_scale) {
        cairo_surface_set_device_scale(_measure_surf,
            device_scale > 1.0 ? device_scale : 1.0,
            device_scale > 1.0 ? device_scale : 1.0);
        _measure_device_scale = device_scale;
        _cached_font_size = -1.0;
        _cached_font_family = NULL;
    }
    return _measure_cr;
}

static double
_ISWComputeFontSize(Widget widget, IswFontStruct *font)
{
    (void)widget;
    if (font) {
        if (font->pt_size > 0)
            return font->pt_size * (96.0 / 72.0);
        double s = (double)(font->ascent + font->descent);
        return s >= 1.0 ? s : 10.0;
    }
    return 10.0;
}

static int
_ISWMeasureFontChanged(IswFontStruct *font)
{
    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    int weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
    int slant  = font ? font->font_slant  : FC_SLANT_ROMAN;

    if (weight != _cached_font_weight || slant != _cached_font_slant)
        return 1;
    if (!_cached_font_family || strcmp(family, _cached_font_family) != 0)
        return 1;
    return 0;
}

static void
_ISWSyncMeasureFont(cairo_t *cr, Widget widget, IswFontStruct *font)
{
    double size = _ISWComputeFontSize(widget, font);

    if (_ISWMeasureFontChanged(font)) {
        _ISWSetCairoFontFromXFont(cr, font, 1.0);
        _cached_font_family = (font && font->font_family) ? font->font_family : "Sans";
        _cached_font_weight = font ? font->font_weight : FC_WEIGHT_NORMAL;
        _cached_font_slant  = font ? font->font_slant  : FC_SLANT_ROMAN;
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    } else if (size != _cached_font_size) {
        cairo_set_font_size(cr, size);
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    }
}

static void
_ISWGetCairoFontExtents(Widget widget, IswFontStruct *font,
                        cairo_font_extents_t *extents)
{
    cairo_t *cr = _ISWGetMeasureCR(ISWScaleFactor(widget));
    _ISWSyncMeasureFont(cr, widget, font);
    *extents = _cached_font_extents;
}

/* .scaled_text_width op */
int
cairo_xcb_scaled_text_width(Widget widget, IswFontStruct *font,
                            const char *text, int len)
{
    cairo_text_extents_t extents;
    char *null_term;
    int width;
    cairo_t *cr;

    if (!text || len <= 0)
        return 0;

    cr = _ISWGetMeasureCR(ISWScaleFactor(widget));
    _ISWSyncMeasureFont(cr, widget, font);

    null_term = (char *)malloc(len + 1);
    if (!null_term)
        return len * 8;
    memcpy(null_term, text, len);
    null_term[len] = '\0';

    cairo_text_extents(cr, null_term, &extents);
    width = (int)ceil(extents.x_advance);

    free(null_term);
    return width;
}

/* .scaled_font_height op */
int
cairo_xcb_scaled_font_height(Widget widget, IswFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    return (int)ceil(extents.ascent + extents.descent);
}

/* .scaled_font_ascent op */
int
cairo_xcb_scaled_font_ascent(Widget widget, IswFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    return (int)ceil(extents.ascent);
}

/* .scaled_font_cap_height op */
int
cairo_xcb_scaled_font_cap_height(Widget widget, IswFontStruct *font)
{
    cairo_t *cr = _ISWGetMeasureCR(ISWScaleFactor(widget));
    cairo_text_extents_t text_ext;

    _ISWSyncMeasureFont(cr, widget, font);
    cairo_text_extents(cr, "X", &text_ext);
    return (int)ceil(-text_ext.y_bearing);
}

/* Drawing primitives — operate on the active draw target of ctx->surface. */
const ISWRenderOps isw_render_cairo_xcb_ops = {
    .save = cairo_xcb_save,
    .restore = cairo_xcb_restore,
    .set_color = cairo_xcb_set_color,
    .set_color_rgba = cairo_xcb_set_color_rgba,
    .set_line_width = cairo_xcb_set_line_width,
    .fill_rectangle = cairo_xcb_fill_rectangle,
    .stroke_rectangle = cairo_xcb_stroke_rectangle,
    .fill_polygon = cairo_xcb_fill_polygon,
    .stroke_polygon = cairo_xcb_stroke_polygon,
    .draw_line = cairo_xcb_draw_line,
    .draw_arc = cairo_xcb_draw_arc,
    .fill_rounded_rect = cairo_xcb_fill_rounded_rect,
    .stroke_rounded_rect = cairo_xcb_stroke_rounded_rect,
    .fill_stroke_rounded_rect = cairo_xcb_fill_stroke_rounded_rect,
    .draw_string = cairo_xcb_draw_string,
    .text_width = cairo_xcb_text_width,
    .text_height = cairo_xcb_text_height,
    .set_font = cairo_xcb_set_font,
    .set_clip_rectangle = cairo_xcb_set_clip_rectangle,
    .clear_clip = cairo_xcb_clear_clip,
    .copy_area = cairo_xcb_copy_area,
    .draw_image_rgba = cairo_xcb_draw_image_rgba,
    .draw_image_masked = cairo_xcb_draw_image_masked,
    .set_gradient = cairo_xcb_set_gradient,
    .push_group = cairo_xcb_push_group,
    .pop_group_alpha = cairo_xcb_pop_group_alpha,
    .path_begin = cairo_xcb_path_begin,
    .path_new_sub_path = cairo_xcb_path_new_sub_path,
    .path_move_to = cairo_xcb_path_move_to,
    .path_line_to = cairo_xcb_path_line_to,
    .path_arc = cairo_xcb_path_arc,
    .path_rectangle = cairo_xcb_path_rectangle,
    .path_close = cairo_xcb_path_close,
    .fill_path = cairo_xcb_fill_path,
    .stroke_path = cairo_xcb_stroke_path,
    .clip_path = cairo_xcb_clip_path,
    .paint = cairo_xcb_paint,
    .set_fill_rule = cairo_xcb_set_fill_rule,
    .set_dash = cairo_xcb_set_dash,
    .set_operator = cairo_xcb_set_operator,
    .translate = cairo_xcb_translate,
    .scale = cairo_xcb_scale,
    .rotate = cairo_xcb_rotate,
    .show_text = cairo_xcb_show_text,
    .pixel_to_rgb = cairo_xcb_pixel_to_rgb
};

/* Surface backend — per-widget surface lifecycle, frame, and compositing. */
const IswSurfaceOps isw_surface_cairo_xcb_ops = {
    .create = cairo_xcb_surface_init,
    .destroy = cairo_xcb_surface_destroy,
    .begin = cairo_xcb_surface_begin,
    .end = cairo_xcb_surface_end,
    .composite_onto = cairo_xcb_composite_onto,
    .fill_background = cairo_xcb_fill_background
};

/*
 * =================================================================
 * Backend Availability / Detection / Selection
 * =================================================================
 *
 * The render system's entry in the platform ops table.  Selection of the
 * concrete drawing/surface sub-vtables and backend detection live here, in the
 * X11 backend — the neutral dispatcher reaches them only via
 * _IswPlatformRenderOpsActive() and never names a backend vtable.
 */

Boolean
ISWRenderBackendAvailable(ISWRenderBackend backend)
{
    switch (backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return True;

#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return ISWRenderEGLAvailable();
#endif

        case ISW_RENDER_BACKEND_AUTO:
        default:
            return False;
    }
}

static ISWRenderBackend
ISWRenderDetectBackend(ISWRenderBackend preferred)
{
    /* Honor explicit request if available */
    if (preferred != ISW_RENDER_BACKEND_AUTO) {
        if (ISWRenderBackendAvailable(preferred))
            return preferred;
        /* Fall through to auto-detection if unavailable */
    }

    /* Check environment variable */
    const char *env = getenv("ISW_RENDER_BACKEND");
    if (env) {
        if (strcmp(env, "cairo-egl") == 0) {
#ifdef HAVE_CAIRO_EGL
            if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL))
                return ISW_RENDER_BACKEND_CAIRO_EGL;
#endif
        } else if (strcmp(env, "cairo") == 0) {
            return ISW_RENDER_BACKEND_CAIRO_XCB;
        }
    }

    /* Auto-detect: prefer best available */
#ifdef HAVE_CAIRO_EGL
    if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL))
        return ISW_RENDER_BACKEND_CAIRO_EGL;
#endif

    return ISW_RENDER_BACKEND_CAIRO_XCB;
}

static const ISWRenderOps *
isw_render_draw_ops(ISWRenderBackend backend)
{
    switch (backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return &isw_render_cairo_xcb_ops;
#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return &isw_render_cairo_egl_ops;
#endif
        default:
            return NULL;
    }
}

static const IswSurfaceOps *
isw_render_surface_ops(ISWRenderBackend backend)
{
    switch (backend) {
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return &isw_surface_cairo_xcb_ops;
#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return &isw_surface_cairo_egl_ops;
#endif
        default:
            return NULL;
    }
}

const struct _IswPlatformRenderOps isw_platform_xcb_render_ops = {
    .available   = ISWRenderBackendAvailable,
    .detect      = ISWRenderDetectBackend,
    .draw_ops    = isw_render_draw_ops,
    .surface_ops = isw_render_surface_ops,
    .scaled_text_width      = cairo_xcb_scaled_text_width,
    .scaled_font_height     = cairo_xcb_scaled_font_height,
    .scaled_font_ascent     = cairo_xcb_scaled_font_ascent,
    .scaled_font_cap_height = cairo_xcb_scaled_font_cap_height,
};



