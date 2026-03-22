/*
 * ISWRender.c - Core rendering abstraction implementation
 *
 * Copyright (c) 2026 ISW Project
 *
 * This file implements the backend-agnostic rendering API.
 * Handles backend detection, context lifecycle, and dispatching
 * to backend-specific implementations.
 *
 * CRITICAL: All backends use pure XCB - NO XLIB DEPENDENCIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ISWRenderPrivate.h"
#include "ISWXcbDraw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * =================================================================
 * Backend Availability Checks
 * =================================================================
 */

Boolean
ISWRenderBackendAvailable(ISWRenderBackend backend)
{
    switch (backend) {
        case ISW_RENDER_BACKEND_XCB:
            /* XCB backend always available */
            return True;
            
#ifdef HAVE_CAIRO
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            /* Cairo-XCB available if Cairo compiled in */
            return True;
#endif
            
#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            /* EGL backend available if Cairo-GL and EGL compiled in */
            return ISWRenderEGLAvailable();
#endif
            
        case ISW_RENDER_BACKEND_AUTO:
            /* Auto is not a real backend */
            return False;
            
        default:
            return False;
    }
    
    return False;
}

/*
 * =================================================================
 * Backend Detection and Selection
 * =================================================================
 */

static ISWRenderBackend
ISWRenderDetectBackend(ISWRenderBackend preferred)
{
    /* Honor explicit request if available */
    if (preferred != ISW_RENDER_BACKEND_AUTO) {
        if (ISWRenderBackendAvailable(preferred)) {
            return preferred;
        }
        /* Fall through to auto-detection if unavailable */
    }
    
    /* Check environment variable */
    const char *env = getenv("ISW_RENDER_BACKEND");
    if (env) {
        if (strcmp(env, "cairo-egl") == 0) {
#ifdef HAVE_CAIRO_EGL
            if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL)) {
                return ISW_RENDER_BACKEND_CAIRO_EGL;
            }
#endif
        } else if (strcmp(env, "cairo") == 0) {
#ifdef HAVE_CAIRO
            return ISW_RENDER_BACKEND_CAIRO_XCB;
#endif
        } else if (strcmp(env, "xcb") == 0) {
            return ISW_RENDER_BACKEND_XCB;
        }
    }
    
    /* Auto-detect: prefer best available */
#ifdef HAVE_CAIRO_EGL
    if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL)) {
        return ISW_RENDER_BACKEND_CAIRO_EGL;
    }
#endif
    
#ifdef HAVE_CAIRO
    /* Cairo-XCB is recommended default if available */
    return ISW_RENDER_BACKEND_CAIRO_XCB;
#endif
    
    /* Fallback to pure XCB */
    return ISW_RENDER_BACKEND_XCB;
}

/*
 * =================================================================
 * Context Lifecycle
 * =================================================================
 */

ISWRenderContext*
ISWRenderCreate(Widget widget, ISWRenderBackend preferred)
{
    ISWRenderContext *ctx;
    static Boolean backend_logged = False;  /* Log backend info once */
    
    if (!widget) {
        fprintf(stderr, "ISWRender: NULL widget passed to ISWRenderCreate\n");
        return NULL;
    }
    
    /* Allocate context */
    ctx = (ISWRenderContext*)calloc(1, sizeof(ISWRenderContext));
    if (!ctx) {
        fprintf(stderr, "ISWRender: Failed to allocate context\n");
        return NULL;
    }
    
    /* Get widget display and window info */
    ctx->widget = widget;
    ctx->connection = (xcb_connection_t*)XtDisplay(widget);
    ctx->window = XtWindow(widget);
    ctx->screen = (xcb_screen_t*)XtScreen(widget);
    
    /* Get colormap from screen - we'll use the screen's default colormap */
    ctx->colormap = ctx->screen ? ctx->screen->default_colormap : 0;
    
    /* Detect and select backend */
    ctx->backend = ISWRenderDetectBackend(preferred);
    
    /* Initialize backend */
    switch (ctx->backend) {
        case ISW_RENDER_BACKEND_XCB:
            ctx->ops = &isw_render_xcb_ops;
            break;
            
#ifdef HAVE_CAIRO
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            ctx->ops = &isw_render_cairo_xcb_ops;
            break;
#endif
            
#ifdef HAVE_CAIRO_EGL
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            ctx->ops = &isw_render_cairo_egl_ops;
            break;
#endif
            
        default:
            fprintf(stderr, "ISWRender: Unknown backend %d\n", ctx->backend);
            free(ctx);
            return NULL;
    }
    
    /* Call backend init */
    if (!ctx->ops->init(ctx)) {
        /* Backend initialization failed - try fallback to XCB */
        if (ctx->backend != ISW_RENDER_BACKEND_XCB) {
            fprintf(stderr, "ISWRender: Backend %d init failed, falling back to XCB\n",
                   ctx->backend);
            ctx->backend = ISW_RENDER_BACKEND_XCB;
            ctx->ops = &isw_render_xcb_ops;
            
            if (!ctx->ops->init(ctx)) {
                fprintf(stderr, "ISWRender: XCB backend init failed\n");
                free(ctx);
                return NULL;
            }
        } else {
            fprintf(stderr, "ISWRender: XCB backend init failed\n");
            free(ctx);
            return NULL;
        }
    }
    
    /* Initialize state */
    ctx->current_color = 0;
    ctx->line_width = 1.0;
    ctx->current_font = NULL;
    
    /* Log backend selection once */
    if (!backend_logged) {
        unsigned int caps = ctx->capabilities;
        fprintf(stderr, "ISWRender: Using '%s' backend", 
               ISWRenderGetBackendName(ctx));
        
        /* Show capabilities */
        if (caps != ISW_RENDER_CAP_BASIC) {
            fprintf(stderr, " (");
            if (caps & ISW_RENDER_CAP_ANTIALIASING) fprintf(stderr, "antialiasing ");
            if (caps & ISW_RENDER_CAP_GRADIENTS) fprintf(stderr, "gradients ");
            if (caps & ISW_RENDER_CAP_ALPHA) fprintf(stderr, "alpha ");
            if (caps & ISW_RENDER_CAP_TRANSFORMS) fprintf(stderr, "transforms ");
            if (caps & ISW_RENDER_CAP_HW_ACCEL) fprintf(stderr, "hw-accel ");
            fprintf(stderr, ")");
        }
        fprintf(stderr, "\n");
        backend_logged = True;
    }
    
    return ctx;
}

void
ISWRenderDestroy(ISWRenderContext *ctx)
{
    if (!ctx) {
        return;
    }
    
    /* Call backend destroy */
    if (ctx->ops && ctx->ops->destroy) {
        ctx->ops->destroy(ctx);
    }
    
    /* Free context */
    free(ctx);
}

ISWRenderBackend
ISWRenderGetBackend(ISWRenderContext *ctx)
{
    if (!ctx) {
        return ISW_RENDER_BACKEND_XCB;  /* Safe default */
    }
    
    return ctx->backend;
}

ISWRenderCaps
ISWRenderGetCapabilities(ISWRenderContext *ctx)
{
    if (!ctx) {
        return ISW_RENDER_CAP_BASIC;  /* Safe default */
    }
    
    return ctx->capabilities;
}

const char*
ISWRenderGetBackendName(ISWRenderContext *ctx)
{
    if (!ctx) {
        return "None";
    }
    
    switch (ctx->backend) {
        case ISW_RENDER_BACKEND_XCB:
            return "XCB Fallback";
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            return "Cairo-XCB";
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            return "Cairo-EGL (Hardware Accelerated)";
        case ISW_RENDER_BACKEND_AUTO:
            return "Auto-detect";
        default:
            return "Unknown";
    }
}

void
ISWRenderPrintBackendInfo(void)
{
    ISWRenderBackend backend = ISWRenderDetectBackend(ISW_RENDER_BACKEND_AUTO);
    ISWRenderCaps caps;
    const char *backend_name;
    
    /* Get backend name */
    switch (backend) {
        case ISW_RENDER_BACKEND_XCB:
            backend_name = "XCB Fallback";
            caps = ISW_RENDER_CAP_BASIC;
            break;
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            backend_name = "Cairo-XCB";
            caps = ISW_RENDER_CAP_BASIC | ISW_RENDER_CAP_ANTIALIASING |
                   ISW_RENDER_CAP_GRADIENTS | ISW_RENDER_CAP_ALPHA |
                   ISW_RENDER_CAP_TRANSFORMS | ISW_RENDER_CAP_TEXT_ADVANCED;
            break;
        case ISW_RENDER_BACKEND_CAIRO_EGL:
            backend_name = "Cairo-EGL (Hardware Accelerated)";
            caps = ISW_RENDER_CAP_BASIC | ISW_RENDER_CAP_ANTIALIASING |
                   ISW_RENDER_CAP_GRADIENTS | ISW_RENDER_CAP_ALPHA |
                   ISW_RENDER_CAP_TRANSFORMS | ISW_RENDER_CAP_TEXT_ADVANCED |
                   ISW_RENDER_CAP_HW_ACCEL;
            break;
        default:
            backend_name = "Unknown";
            caps = ISW_RENDER_CAP_BASIC;
            break;
    }
    
    fprintf(stderr, "ISWRender: Using '%s' backend", backend_name);
    
    /* Show capabilities */
    if (caps != ISW_RENDER_CAP_BASIC) {
        fprintf(stderr, " (");
        if (caps & ISW_RENDER_CAP_ANTIALIASING) fprintf(stderr, "antialiasing ");
        if (caps & ISW_RENDER_CAP_GRADIENTS) fprintf(stderr, "gradients ");
        if (caps & ISW_RENDER_CAP_ALPHA) fprintf(stderr, "alpha ");
        if (caps & ISW_RENDER_CAP_TRANSFORMS) fprintf(stderr, "transforms ");
        if (caps & ISW_RENDER_CAP_HW_ACCEL) fprintf(stderr, "hw-accel ");
        fprintf(stderr, "\b)");  /* Remove trailing space */
    }
    fprintf(stderr, "\n");
}

/*
 * =================================================================
 * State Management
 * =================================================================
 */

void
ISWRenderBegin(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->begin) {
        return;
    }
    
    ctx->ops->begin(ctx);
}

void
ISWRenderEnd(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->end) {
        return;
    }
    
    ctx->ops->end(ctx);
}

void
ISWRenderSave(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->save) {
        return;
    }
    
    ctx->ops->save(ctx);
}

void
ISWRenderRestore(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->restore) {
        return;
    }
    
    ctx->ops->restore(ctx);
}

/*
 * =================================================================
 * Color and Line Management
 * =================================================================
 */

void
ISWRenderSetColor(ISWRenderContext *ctx, Pixel pixel)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_color) {
        return;
    }
    
    ctx->ops->set_color(ctx, pixel);
}

void
ISWRenderSetColorRGBA(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_color_rgba) {
        /* Fallback to solid color if RGBA not supported */
        if (ctx->ops && ctx->ops->set_color) {
            /* Best effort: just use pixel color, ignore alpha */
            return;
        }
        return;
    }
    
    ctx->ops->set_color_rgba(ctx, r, g, b, a);
}

void
ISWRenderSetLineWidth(ISWRenderContext *ctx, double width)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_line_width) {
        return;
    }
    
    ctx->ops->set_line_width(ctx, width);
}

/*
 * =================================================================
 * Shape Drawing Primitives
 * =================================================================
 */

void
ISWRenderStrokeRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_rectangle) {
        return;
    }
    
    ctx->ops->stroke_rectangle(ctx, x, y, width, height);
}

void
ISWRenderFillRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_rectangle) {
        return;
    }
    
    ctx->ops->fill_rectangle(ctx, x, y, width, height);
}

void
ISWRenderStrokePolygon(ISWRenderContext *ctx, xcb_point_t *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->stroke_polygon) {
        return;
    }
    
    ctx->ops->stroke_polygon(ctx, points, num_points);
}

void
ISWRenderFillPolygon(ISWRenderContext *ctx, xcb_point_t *points, int num_points)
{
    if (!ctx || !ctx->ops || !ctx->ops->fill_polygon) {
        return;
    }
    
    ctx->ops->fill_polygon(ctx, points, num_points);
}

void
ISWRenderDrawLine(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_line) {
        return;
    }
    
    ctx->ops->draw_line(ctx, x1, y1, x2, y2);
}

void
ISWRenderDrawArc(ISWRenderContext *ctx, int x, int y, int width, int height,
                double angle1, double angle2)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_arc) {
        return;
    }
    
    ctx->ops->draw_arc(ctx, x, y, width, height, angle1, angle2);
}

/*
 * =================================================================
 * Widget-Specific Drawing
 * =================================================================
 */

void
ISWRenderDrawShadow(ISWRenderContext *ctx, int x, int y, int width, int height,
                   int shadow_width, XtRelief relief,
                   Pixel top_color, Pixel bottom_color)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_shadow) {
        return;
    }
    
    ctx->ops->draw_shadow(ctx, x, y, width, height, shadow_width,
                         relief, top_color, bottom_color);
}

void
ISWRenderDrawBevel(ISWRenderContext *ctx, int x, int y, int width, int height,
                  int bevel_width, Boolean raised)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_bevel) {
        /* Fallback: use shadow drawing */
        if (ctx->ops && ctx->ops->draw_shadow) {
            XtRelief relief = raised ? XtReliefRaised : XtReliefSunken;
            /* Use widget's colors - this is a simplification */
            ctx->ops->draw_shadow(ctx, x, y, width, height, bevel_width,
                                relief, ctx->screen->white_pixel,
                                ctx->screen->black_pixel);
        }
        return;
    }
    
    ctx->ops->draw_bevel(ctx, x, y, width, height, bevel_width, raised);
}

/*
 * =================================================================
 * Text Rendering
 * =================================================================
 */

void
ISWRenderDrawString(ISWRenderContext *ctx, const char *text, int length,
                   int x, int y)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_string) {
        return;
    }
    
    ctx->ops->draw_string(ctx, text, length, x, y);
}

int
ISWRenderTextWidth(ISWRenderContext *ctx, const char *text, int length)
{
    if (!ctx || !ctx->ops || !ctx->ops->text_width) {
        /* Rough estimate: 8 pixels per character */
        return length * 8;
    }
    
    return ctx->ops->text_width(ctx, text, length);
}

int
ISWRenderTextHeight(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->text_height) {
        /* Rough estimate */
        return 12;
    }
    
    return ctx->ops->text_height(ctx);
}

void
ISWRenderSetFont(ISWRenderContext *ctx, XFontStruct *font)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_font) {
        return;
    }
    
    ctx->current_font = font;
    ctx->ops->set_font(ctx, font);
}

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

void
ISWRenderSetClipRectangle(ISWRenderContext *ctx, int x, int y, int width, int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_clip_rectangle) {
        return;
    }
    
    ctx->ops->set_clip_rectangle(ctx, x, y, width, height);
}

void
ISWRenderClearClip(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->clear_clip) {
        return;
    }
    
    ctx->ops->clear_clip(ctx);
}

/*
 * =================================================================
 * Pixmap/Bitmap Rendering
 * =================================================================
 */

void
ISWRenderCopyArea(ISWRenderContext *ctx,
                  int src_x, int src_y,
                  int dst_x, int dst_y,
                  unsigned int width, unsigned int height)
{
    if (!ctx || !ctx->ops || !ctx->ops->copy_area) {
        return;
    }

    ctx->ops->copy_area(ctx, src_x, src_y, dst_x, dst_y, width, height);
}

void
ISWRenderDrawPixmap(ISWRenderContext *ctx,
                    xcb_pixmap_t pixmap,
                    int src_x, int src_y,
                    int dst_x, int dst_y,
                    unsigned int width, unsigned int height,
                    unsigned int depth)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_pixmap || !pixmap) {
        return;
    }

    ctx->ops->draw_pixmap(ctx, pixmap, src_x, src_y, dst_x, dst_y,
                          width, height, depth);
}

/*
 * =================================================================
 * Advanced Features (Cairo-only)
 * =================================================================
 */

Boolean
ISWRenderSetGradient(ISWRenderContext *ctx, double x1, double y1, double x2, double y2,
                    Pixel color1, Pixel color2)
{
    if (!ctx || !ctx->ops || !ctx->ops->set_gradient) {
        return False;
    }
    
    return ctx->ops->set_gradient(ctx, x1, y1, x2, y2, color1, color2);
}

void*
ISWRenderGetCairoContext(ISWRenderContext *ctx)
{
    if (!ctx || !ctx->ops || !ctx->ops->get_cairo_context) {
        return NULL;
    }
    
    return ctx->ops->get_cairo_context(ctx);
}

/*
 * =================================================================
 * Helper Functions
 * =================================================================
 */

void
ISWRenderPixelToRGB(ISWRenderContext *ctx, Pixel pixel,
                   double *r, double *g, double *b)
{
    XColor color;
    
    if (!ctx || !r || !g || !b) {
        return;
    }
    
    /* Query color from X server using the context's colormap */
    color.pixel = pixel;
    
    if (ctx->colormap && ISWQueryColor(ctx->connection, ctx->colormap, &color)) {
        /* Convert from 16-bit to 0.0-1.0 range */
        *r = color.red / 65535.0;
        *g = color.green / 65535.0;
        *b = color.blue / 65535.0;
    } else {
        /* Fallback: extract RGB from pixel assuming TrueColor format */
        *r = ((pixel >> 16) & 0xFF) / 255.0;
        *g = ((pixel >> 8) & 0xFF) / 255.0;
        *b = (pixel & 0xFF) / 255.0;
    }
}

xcb_visualtype_t*
ISWRenderFindVisual(xcb_screen_t *screen, uint8_t depth)
{
    xcb_depth_iterator_t depth_iter;
    xcb_visualtype_iterator_t visual_iter;
    
    if (!screen) {
        return NULL;
    }
    
    /* Iterate through screen depths */
    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter)) {
        
        if (depth_iter.data->depth == depth) {
            /* Found matching depth, return first visual */
            visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
            if (visual_iter.rem) {
                return visual_iter.data;
            }
        }
    }
    
    return NULL;
}
