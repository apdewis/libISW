/*
 * ISWRenderPrivate.h - Internal structures for ISW rendering backends
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file contains internal structures and function declarations
 * for the ISWRender backend implementations. Not part of public API.
 *
 * CRITICAL: All backends use pure XCB - NO XLIB DEPENDENCIES.
 */

#ifndef _ISWRenderPrivate_h
#define _ISWRenderPrivate_h

#include "../include/ISW/ISWRender.h"
#include <xcb/xcb.h>

/* Cairo is a mandatory dependency */
#include <cairo.h>
#include <cairo-xcb.h>

#ifdef HAVE_CAIRO_EGL
#include <cairo-gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

/*
 * =================================================================
 * Surface Backend Operations Vtable
 * =================================================================
 *
 * An IswSurface (declared opaque in ISW/Intrinsic.h) is the per-widget render
 * target — the surface-tree analogue of a window.  Every widget owns one
 * (core.surface); a windowless widget paints into its own surface at local
 * (0,0), and the composite pass folds child surfaces into parent surfaces up to
 * the windowed root, which presents to its window.
 *
 * This table abstracts the surface itself: its lifecycle, the per-frame target
 * switch, and the surface-to-surface compositing/presentation.  It is keyed on
 * IswSurface handles (and the owning Widget for geometry/background), NOT on a
 * render context, so the composite walk operates on surfaces reached generically
 * via IswSurfaceOf — no Widget->context registry.  The drawing primitives that
 * mark a surface stay in ISWRenderOps; a render context draws into the surface
 * its widget owns.
 *
 * The concrete struct _IswSurface is defined by the backend (the cairo-xcb
 * backend's holds the cairo back buffer / pixmap / present state).
 */
typedef struct _IswSurfaceOps {
    /* Create the per-widget surface for `widget` (windowed or windowless),
       sized from the widget's current geometry.  Returns NULL on failure or if
       the widget is not yet sized (caller retries at first paint). */
    IswSurface (*create)(Widget widget);
    /* Destroy a surface and free its buffers. */
    void (*destroy)(IswSurface surface);

    /* Begin a frame: ensure the back buffer matches the widget's current size,
       target it, and return the render-layer drawing handle (cairo_t*) the
       ISWRenderOps primitives draw with — or NULL if not ready this frame. */
    void *(*begin)(IswSurface surface, Widget widget);
    /* End a frame: flush the back buffer.  For a windowed widget, blit it to
       `window`; for a windowless widget this is just a flush (the composite pass
       presents).  `window` is the widget's own window (windowed) or 0. */
    void (*end)(IswSurface surface, Widget widget, IswWindow window);

    /* Fold src's surface onto dst's surface at logical (x,y) in dst's content
       frame, clipped to dst's bounds.  `dst_widget`/`src_widget` supply
       geometry/border/clip for the fold. */
    void (*composite_onto)(IswSurface dst, Widget dst_widget,
                           IswSurface src, Widget src_widget, int x, int y);
    /* Fill `surface` with `widget`'s background pixel, sizing it to the widget's
       current window size.  Called on a windowed composite root before folding
       children so uncovered gaps show the background, not a bare window. */
    void (*fill_background)(IswSurface surface, Widget widget);
} IswSurfaceOps;

/*
 * =================================================================
 * Backend Operations Vtable
 * =================================================================
 *
 * Each backend implements this interface. The core renderer
 * dispatches to the appropriate backend via function pointers.
 */

typedef struct _ISWRenderOps {
    /* State management */
    void (*save)(struct _ISWRenderContext *ctx);
    void (*restore)(struct _ISWRenderContext *ctx);
    
    /* Color */
    void (*set_color)(struct _ISWRenderContext *ctx, Pixel pixel);
    void (*set_color_rgba)(struct _ISWRenderContext *ctx,
                          double r, double g, double b, double a);
    void (*set_line_width)(struct _ISWRenderContext *ctx, double width);
    
    /* Drawing primitives */
    void (*fill_rectangle)(struct _ISWRenderContext *ctx,
                          int x, int y, int w, int h);
    void (*stroke_rectangle)(struct _ISWRenderContext *ctx,
                            int x, int y, int w, int h);
    void (*fill_polygon)(struct _ISWRenderContext *ctx,
                        xcb_point_t *pts, int num);
    void (*stroke_polygon)(struct _ISWRenderContext *ctx,
                          xcb_point_t *pts, int num);
    void (*draw_line)(struct _ISWRenderContext *ctx,
                     int x1, int y1, int x2, int y2);
    void (*draw_arc)(struct _ISWRenderContext *ctx,
                    int x, int y, int w, int h,
                    double angle1, double angle2);
    
    /* Text */
    void (*draw_string)(struct _ISWRenderContext *ctx,
                       const char *text, int len, int x, int y);
    int (*text_width)(struct _ISWRenderContext *ctx,
                     const char *text, int len);
    int (*text_height)(struct _ISWRenderContext *ctx);
    void (*set_font)(struct _ISWRenderContext *ctx, IswFontStruct *font);
    
    /* Clipping */
    void (*set_clip_rectangle)(struct _ISWRenderContext *ctx,
                              int x, int y, int w, int h);
    void (*clear_clip)(struct _ISWRenderContext *ctx);
    
    /* Pixmap/Bitmap rendering */
    void (*copy_area)(struct _ISWRenderContext *ctx,
                      int src_x, int src_y,
                      int dst_x, int dst_y,
                      unsigned int width, unsigned int height);
    void (*draw_pixmap)(struct _ISWRenderContext *ctx,
                        xcb_pixmap_t pixmap,
                        int src_x, int src_y,
                        int dst_x, int dst_y,
                        unsigned int width, unsigned int height,
                        unsigned int depth);

    /* RGBA image rendering */
    void (*draw_image_rgba)(struct _ISWRenderContext *ctx,
                            const unsigned char *rgba,
                            unsigned int img_w, unsigned int img_h,
                            int dst_x, int dst_y,
                            unsigned int dst_w, unsigned int dst_h);

    /* Advanced (Cairo only) */
    Boolean (*set_gradient)(struct _ISWRenderContext *ctx,
                           double x1, double y1, double x2, double y2,
                           Pixel c1, Pixel c2);
    void* (*get_cairo_context)(struct _ISWRenderContext *ctx);

} ISWRenderOps;

/*
 * =================================================================
 * Base Rendering Context
 * =================================================================
 *
 * Shared by all backends. Backend-specific data stored in
 * backend_data pointer.
 */

typedef struct _ISWRenderContext {
    /* Widget and display info */
    Widget widget;
    xcb_connection_t *connection;  /* Pure XCB connection, NOT Display* */
    xcb_window_t window;
    xcb_screen_t *screen;
    xcb_colormap_t colormap;
    
    /* Backend information */
    ISWRenderBackend backend;
    ISWRenderCaps capabilities;
    const ISWRenderOps *ops;            /* stateless drawing primitives */
    const IswSurfaceOps *surface_ops;   /* surface lifecycle + compositing */

    /* The surface this context draws into — the widget's own (core.surface).
       Cached here so the drawing primitives reach it without a widget lookup.
       It also holds the active draw target (set by surface_ops->begin), which
       the ISWRenderOps primitives use. */
    IswSurface surface;

    /* Current state */
    Pixel current_color;
    double line_width;
    IswFontStruct *current_font;

    /* Windowless rendering: drawing origin within the target window.
       A windowless widget shares its windowed ancestor's window; this is
       the widget's position relative to that window, applied as a
       coordinate translation at frame Begin so the widget draws in its own
       local (0,0)-based coordinates.  Zero for windowed widgets. */
    int origin_x, origin_y;

    /* Composite root that the composite pass created itself, because the
       windowed root has no expose proc that paints its own content (bare
       Box/Form/Shell).  Such a root's window must be background-filled before
       folding children.  A root owned by a widget that paints its own content
       (SimpleMenu, IconView, ...) must NOT be filled — that would wipe it. */
    Boolean lazy_composite_root;

    /* Set once this root has presented a pass that actually folded child
       content.  Until then, a pass that would fold nothing is skipped entirely
       (no fill, no blit): a lazy root with no mapped children yet has nothing to
       show, and re-filling+blitting the full-window background is pure overhead.
       Once content has appeared, later empty passes still present so that
       un-mapping the last child correctly clears it. */
    Boolean presented_content;

    /* Composite re-expose gate.  A composite container's surface persists
       between composite passes and accumulates its folded children.  The fold
       (ISWRenderCompositeSubtree) used to re-run every composite container's
       expose proc each pass to erase pixels vacated by an unmapped/moved child
       — but for the common case (one widget repainted, scroll, hover) nothing
       in most containers changed, so regenerating their identical background
       was the dominant compositing cost.  This flag is set whenever the
       container (or something under it) actually changed since the last fold —
       a paint into its surface (ISWRenderEnd), or a child un/map/geometry change
       — and the fold re-runs the container's expose proc ONLY when it is set,
       then clears it.  Starts True so the first composite always paints. */
    Boolean composite_dirty;

    /* Pixel->RGB decode cache.  ISWRenderPixelToRGB is called once per
       colour set on the draw path; resolving a pixel to RGB must not
       round-trip to the server (a synchronous QueryColors per draw was a
       major source of latency under a busy X server).  visual is the
       colormap's visual, resolved lazily and cached; for TrueColor/
       DirectColor it lets us decode pixels directly from the channel masks
       with zero server traffic.  visual_resolved guards the one-time lookup
       (a NULL visual is a valid "not found, use fallback" result). */
    xcb_visualtype_t *visual;
    Boolean visual_resolved;

} ISWRenderContext;

/*
 * =================================================================
 * Helper Functions
 * =================================================================
 */

/*
 * ISWRenderFindVisual - Find XCB visual for given depth
 *
 * Parameters:
 *   screen - XCB screen
 *   depth  - Depth to find visual for
 *
 * Returns: xcb_visualtype_t* or NULL if not found
 */
xcb_visualtype_t* ISWRenderFindVisual(xcb_screen_t *screen, uint8_t depth);

/*
 * =================================================================
 * Cairo-XCB Backend
 * =================================================================
 *
 * Cairo with XCB surface - NO XLIB.
 * Provides anti-aliasing, gradients, alpha blending.
 */

extern const ISWRenderOps isw_render_cairo_xcb_ops;
extern const IswSurfaceOps isw_surface_cairo_xcb_ops;

/*
 * Present source accessor — the inputs a platform present_root needs to blit a
 * finished composite surface to its window, without reaching into the private
 * struct _IswSurface.  Fills the out params from `surface`'s back buffer and
 * (for the Present path) bumps the present serial.  Returns False if the
 * surface has no back buffer yet (nothing to present).
 *
 *   back_cairo  — the back buffer as a cairo surface (the blit source).
 *   window_cr   — the surface's cached cairo context on the window-target
 *                 surface (the cairo blit destination); NULL if unavailable.
 *   back_pixmap — the server pixmap for the Present path, or 0 when Present is
 *                 unusable / the back buffer is a client image (use the cairo
 *                 path instead).
 *   present_serial — next Present serial (only meaningful when back_pixmap != 0).
 *
 * Defined in ISWRenderCairoXCB.c.
 */
Boolean _ISWRenderSurfacePresentSource(IswSurface surface,
                                       cairo_surface_t **back_cairo,
                                       void **window_cr,
                                       xcb_pixmap_t *back_pixmap,
                                       uint32_t *present_serial);

/*
 * =================================================================
 * Cairo-EGL Backend (Hardware Accelerated)
 * =================================================================
 *
 * Cairo with EGL - NO XLIB, NO GLX!
 * Uses EGL platform XCB extension for pure XCB compatibility.
 * Provides hardware-accelerated rendering via OpenGL.
 *
 * CRITICAL: Uses EGL, NOT GLX, to avoid Xlib dependency.
 */

#ifdef HAVE_CAIRO_EGL
extern const ISWRenderOps isw_render_cairo_egl_ops;
extern const IswSurfaceOps isw_surface_cairo_egl_ops;

/* Check if EGL platform XCB is available */
Boolean ISWRenderEGLAvailable(void);
#endif

/*
 * =================================================================
 * Backend Availability Checks
 * =================================================================
 */

/*
 * ISWRenderBackendAvailable - Check if backend is available
 *
 * Parameters:
 *   backend - Backend to check
 *
 * Returns: True if backend can be used, False otherwise
 */
Boolean ISWRenderBackendAvailable(ISWRenderBackend backend);

/*
 * Font resolution — ISWRender.c
 */
cairo_font_face_t *_ISWResolveFontFace(const char *family, int weight, int slant);
void _ISWSetCairoFontFromXFont(cairo_t *cr, IswFontStruct *font, double scale);

#endif /* _ISWRenderPrivate_h */
