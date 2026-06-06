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
 * Backend Operations Vtable
 * =================================================================
 *
 * Each backend implements this interface. The core renderer
 * dispatches to the appropriate backend via function pointers.
 */

typedef struct _ISWRenderOps {
    /* Lifecycle */
    Boolean (*init)(struct _ISWRenderContext *ctx);
    void (*destroy)(struct _ISWRenderContext *ctx);
    
    /* Frame operations */
    void (*begin)(struct _ISWRenderContext *ctx);
    void (*end)(struct _ISWRenderContext *ctx);
    
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

    /* Surface-tree compositing.
     * composite_onto: paint src's back surface onto dst's back surface at
     *   logical (x,y), clipped to dst's bounds.  Used by the composite pass to
     *   fold a windowless child's surface into its parent's surface.
     * present: blit a (windowed root) widget's back surface to its X window
     *   once, after all descendants have been composited into it. */
    void (*composite_onto)(struct _ISWRenderContext *dst,
                           struct _ISWRenderContext *src, int x, int y);
    void (*present)(struct _ISWRenderContext *ctx);
    /* Fill the composite target's whole surface with the widget's background
     * pixel.  Called on a windowed composite root before folding children, so
     * gaps the children don't cover show the background, not a bare window. */
    void (*fill_background)(struct _ISWRenderContext *ctx);

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
    const ISWRenderOps *ops;
    
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

    /* Composite clip: when set, this widget is clipped to (clip_x,clip_y,
       clip_w,clip_h) — in the PARENT's content coordinates — as it is folded
       into its parent.  Used by scrolling containers (Viewport) to confine a
       scrolled child to the clip region (excluding scrollbars) regardless of
       the child's own size.  Width 0 means no composite clip. */
    int clip_x, clip_y, clip_w, clip_h;

    /* Backend-specific data */
    void *backend_data;

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

Boolean ISWRenderCairoXCBInit(ISWRenderContext *ctx);
void ISWRenderCairoXCBDestroy(ISWRenderContext *ctx);

/* Resize handler for window size changes */
void ISWRenderCairoXCBResize(ISWRenderContext *ctx, int width, int height);

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

Boolean ISWRenderEGLInit(ISWRenderContext *ctx);
void ISWRenderEGLDestroy(ISWRenderContext *ctx);

/* Check if EGL platform XCB is available */
Boolean ISWRenderEGLAvailable(void);

/* Resize handler for window size changes */
void ISWRenderEGLResize(ISWRenderContext *ctx, int width, int height);
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
