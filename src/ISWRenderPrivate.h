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

/* Conditional backend support */
#ifdef HAVE_CAIRO
#include <cairo.h>
#include <cairo-xcb.h>
#endif

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
    void (*set_font)(struct _ISWRenderContext *ctx, XFontStruct *font);
    
    /* Widget-specific */
    void (*draw_shadow)(struct _ISWRenderContext *ctx,
                       int x, int y, int w, int h, int sw,
                       XtRelief relief, Pixel tc, Pixel bc);
    void (*draw_bevel)(struct _ISWRenderContext *ctx,
                      int x, int y, int w, int h, int bw,
                      Boolean raised);
    
    /* Clipping */
    void (*set_clip_rectangle)(struct _ISWRenderContext *ctx,
                              int x, int y, int w, int h);
    void (*clear_clip)(struct _ISWRenderContext *ctx);
    
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
    const ISWRenderOps *ops;
    
    /* Current state */
    Pixel current_color;
    double line_width;
    XFontStruct *current_font;
    
    /* Backend-specific data */
    void *backend_data;
    
} ISWRenderContext;

/*
 * =================================================================
 * Helper Functions
 * =================================================================
 */

/*
 * ISWRenderPixelToRGB - Convert pixel value to RGB components
 *
 * Parameters:
 *   ctx   - Rendering context
 *   pixel - Pixel value
 *   r, g, b - Output RGB components (0.0-1.0)
 */
void ISWRenderPixelToRGB(ISWRenderContext *ctx, Pixel pixel,
                        double *r, double *g, double *b);

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
 * XCB Backend
 * =================================================================
 *
 * Pure XCB implementation - always available as fallback.
 */

extern const ISWRenderOps isw_render_xcb_ops;

Boolean ISWRenderXCBInit(ISWRenderContext *ctx);
void ISWRenderXCBDestroy(ISWRenderContext *ctx);

/*
 * =================================================================
 * Cairo-XCB Backend
 * =================================================================
 *
 * Cairo with XCB surface - NO XLIB.
 * Provides anti-aliasing, gradients, alpha blending.
 */

#ifdef HAVE_CAIRO
extern const ISWRenderOps isw_render_cairo_xcb_ops;

Boolean ISWRenderCairoXCBInit(ISWRenderContext *ctx);
void ISWRenderCairoXCBDestroy(ISWRenderContext *ctx);

/* Resize handler for window size changes */
void ISWRenderCairoXCBResize(ISWRenderContext *ctx, int width, int height);
#endif

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

#endif /* _ISWRenderPrivate_h */
