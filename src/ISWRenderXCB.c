/*
 * ISWRenderXCB.c - Pure XCB rendering backend
 *
 * Copyright (c) 2026 ISW Project
 *
 * This backend provides direct XCB rendering - always available as fallback.
 * Wraps existing XCB primitives from the widget library.
 *
 * CRITICAL: Pure XCB only - NO XLIB DEPENDENCIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ISWRenderPrivate.h"
#include "ISWXcbDraw.h"
#include <stdlib.h>
#include <string.h>

/*
 * XCB Backend Data
 */
typedef struct {
    xcb_gcontext_t gc;
    xcb_create_gc_value_list_t gc_values;
    uint32_t current_fg;
    uint32_t current_bg;
    int saved_count;  /* For save/restore stack */
} ISWRenderXCBData;

/*
 * =================================================================
 * Lifecycle
 * =================================================================
 */

Boolean
ISWRenderXCBInit(ISWRenderContext *ctx)
{
    ISWRenderXCBData *data;
    
    /* Allocate backend data */
    data = (ISWRenderXCBData*)calloc(1, sizeof(*data));
    if (!data) {
        return False;
    }
    
    /* Create graphics context */
    data->gc = xcb_generate_id(ctx->connection);
    ISWInitGCValues(&data->gc_values);
    
    /* Set initial GC values */
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                    XCB_GC_GRAPHICS_EXPOSURES;
    
    data->current_fg = ctx->screen->black_pixel;
    data->current_bg = ctx->screen->white_pixel;
    data->gc_values.foreground = data->current_fg;
    data->gc_values.background = data->current_bg;
    data->gc_values.graphics_exposures = 0;
    
    /* Create GC */
    xcb_create_gc(ctx->connection, data->gc, ctx->window,
                  mask, &data->gc_values);
    
    /* Set context capabilities - XCB supports only basic drawing */
    ctx->capabilities = ISW_RENDER_CAP_BASIC;
    ctx->backend_data = data;
    
    data->saved_count = 0;
    
    return True;
}

void
ISWRenderXCBDestroy(ISWRenderContext *ctx)
{
    ISWRenderXCBData *data;
    
    if (!ctx || !ctx->backend_data) {
        return;
    }
    
    data = (ISWRenderXCBData*)ctx->backend_data;
    
    /* Free GC */
    if (data->gc) {
        xcb_free_gc(ctx->connection, data->gc);
    }
    
    /* Free backend data */
    free(data);
    ctx->backend_data = NULL;
}

/*
 * =================================================================
 * Frame Operations
 * =================================================================
 */

static void
xcb_begin(ISWRenderContext *ctx)
{
    /* Nothing special needed for XCB - operations immediate */
    (void)ctx;
}

static void
xcb_end(ISWRenderContext *ctx)
{
    /* Flush XCB connection */
    if (ctx && ctx->connection) {
        xcb_flush(ctx->connection);
    }
}

/*
 * =================================================================
 * State Management
 * =================================================================
 */

static void
xcb_save(ISWRenderContext *ctx)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    /* XCB doesn't have state stack - just track that save was called */
    data->saved_count++;
}

static void
xcb_restore(ISWRenderContext *ctx)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    if (data->saved_count > 0) {
        data->saved_count--;
    }
    
    /* Would need to implement state stack for full save/restore */
    /* For now, just track nesting level */
}

/*
 * =================================================================
 * Color Management
 * =================================================================
 */

static void
xcb_set_color(ISWRenderContext *ctx, Pixel pixel)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    if (data->current_fg != pixel) {
        data->current_fg = pixel;
        uint32_t values[] = { pixel };
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, values);
    }
    
    ctx->current_color = pixel;
}

static void
xcb_set_color_rgba(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    /* XCB doesn't support alpha - ignore it and just set solid color */
    /* Convert RGB to pixel value (this is a simplification) */
    uint32_t pixel = ((uint32_t)(r * 255) << 16) |
                     ((uint32_t)(g * 255) << 8) |
                     ((uint32_t)(b * 255));
    
    (void)a;  /* Ignore alpha */
    xcb_set_color(ctx, pixel);
}

static void
xcb_set_line_width(ISWRenderContext *ctx, double width)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    uint32_t line_width = (uint32_t)width;
    
    xcb_change_gc(ctx->connection, data->gc,
                  XCB_GC_LINE_WIDTH, &line_width);
    
    ctx->line_width = width;
}

/*
 * =================================================================
 * Drawing Primitives
 * =================================================================
 */

static void
xcb_fill_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_rectangle_t rect = { x, y, w, h };
    
    xcb_poly_fill_rectangle(ctx->connection, ctx->window,
                           data->gc, 1, &rect);
}

static void
xcb_stroke_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_rectangle_t rect = { x, y, w, h };
    
    xcb_poly_rectangle(ctx->connection, ctx->window,
                      data->gc, 1, &rect);
}

static void
xcb_fill_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    if (num < 3) {
        return;  /* Need at least 3 points for a polygon */
    }
    
    xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                  XCB_POLY_SHAPE_COMPLEX,
                  XCB_COORD_MODE_ORIGIN, num, points);
}

static void
xcb_stroke_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    if (num < 2) {
        return;
    }
    
    xcb_poly_line(ctx->connection, XCB_COORD_MODE_ORIGIN,
                  ctx->window, data->gc, num, points);
}

static void
xcb_draw_line(ISWRenderContext *ctx, int x1, int y1, int x2, int y2)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_point_t points[2];
    
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    
    xcb_poly_line(ctx->connection, XCB_COORD_MODE_ORIGIN,
                  ctx->window, data->gc, 2, points);
}

static void
xcb_draw_arc(ISWRenderContext *ctx, int x, int y, int w, int h,
            double angle1, double angle2)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_arc_t arc;
    
    /* Convert angles from radians to 1/64th degrees */
    int16_t start_angle = (int16_t)((angle1 * 180.0 / 3.14159265) * 64);
    int16_t delta_angle = (int16_t)(((angle2 - angle1) * 180.0 / 3.14159265) * 64);
    
    arc.x = x;
    arc.y = y;
    arc.width = w;
    arc.height = h;
    arc.angle1 = start_angle;
    arc.angle2 = delta_angle;
    
    xcb_poly_arc(ctx->connection, ctx->window, data->gc, 1, &arc);
}

/*
 * =================================================================
 * Widget-Specific Drawing
 * =================================================================
 */

static void
xcb_draw_shadow(ISWRenderContext *ctx, int x, int y, int w, int h,
               int shadow_width, XtRelief relief,
               Pixel top_color, Pixel bottom_color)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_point_t pt[6];
    Pixel light, dark;
    
    /* Determine light and dark colors based on relief */
    if (relief == XtReliefRaised) {
        light = top_color;
        dark = bottom_color;
    } else if (relief == XtReliefSunken) {
        light = bottom_color;
        dark = top_color;
    } else {
        /* Ridge/Groove handled below */
        light = top_color;
        dark = bottom_color;
    }
    
    if (relief == XtReliefRaised || relief == XtReliefSunken) {
        /* Top-left shadow polygon */
        pt[0].x = x;
        pt[0].y = y + h;
        pt[1].x = x;
        pt[1].y = y;
        pt[2].x = x + w;
        pt[2].y = y;
        pt[3].x = x + w - shadow_width;
        pt[3].y = y + shadow_width;
        pt[4].x = x + shadow_width;
        pt[4].y = y + shadow_width;
        pt[5].x = x + shadow_width;
        pt[5].y = y + h - shadow_width;
        
        /* Set light color and draw */
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &light);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
        
        /* Bottom-right shadow polygon */
        pt[0].x = x;
        pt[0].y = y + h;
        pt[1].x = x + w;
        pt[1].y = y + h;
        pt[2].x = x + w;
        pt[2].y = y;
        pt[3].x = x + w - shadow_width;
        pt[3].y = y + shadow_width;
        pt[4].x = x + w - shadow_width;
        pt[4].y = y + h - shadow_width;
        pt[5].x = x + shadow_width;
        pt[5].y = y + h - shadow_width;
        
        /* Set dark color and draw */
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &dark);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
                      
    } else if (relief == XtReliefRidge || relief == XtReliefGroove) {
        /* Ridge/Groove: split shadow width and draw twice */
        int sw = shadow_width / 2;
        
        /* Outer layer */
        Pixel outer_light = (relief == XtReliefRidge) ? top_color : bottom_color;
        Pixel outer_dark = (relief == XtReliefRidge) ? bottom_color : top_color;
        
        /* Draw outer top-left */
        pt[0].x = x;
        pt[0].y = y + h;
        pt[1].x = x;
        pt[1].y = y;
        pt[2].x = x + w;
        pt[2].y = y;
        pt[3].x = x + w - sw;
        pt[3].y = y + sw;
        pt[4].x = x + sw;
        pt[4].y = y + sw;
        pt[5].x = x + sw;
        pt[5].y = y + h - sw;
        
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &outer_light);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
        
        /* Draw outer bottom-right */
        pt[0].x = x;
        pt[0].y = y + h;
        pt[1].x = x + w;
        pt[1].y = y + h;
        pt[2].x = x + w;
        pt[2].y = y;
        pt[3].x = x + w - sw;
        pt[3].y = y + sw;
        pt[4].x = x + w - sw;
        pt[4].y = y + h - sw;
        pt[5].x = x + sw;
        pt[5].y = y + h - sw;
        
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &outer_dark);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
        
        /* Inner layer (inverted) */
        Pixel inner_light = (relief == XtReliefRidge) ? bottom_color : top_color;
        Pixel inner_dark = (relief == XtReliefRidge) ? top_color : bottom_color;
        
        int sw2 = shadow_width - sw;
        int x2 = x + sw;
        int y2 = y + sw;
        int w2 = w - 2 * sw;
        int h2 = h - 2 * sw;
        
        /* Draw inner top-left */
        pt[0].x = x2;
        pt[0].y = y2 + h2;
        pt[1].x = x2;
        pt[1].y = y2;
        pt[2].x = x2 + w2;
        pt[2].y = y2;
        pt[3].x = x2 + w2 - sw2;
        pt[3].y = y2 + sw2;
        pt[4].x = x2 + sw2;
        pt[4].y = y2 + sw2;
        pt[5].x = x2 + sw2;
        pt[5].y = y2 + h2 - sw2;
        
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &inner_light);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
        
        /* Draw inner bottom-right */
        pt[0].x = x2;
        pt[0].y = y2 + h2;
        pt[1].x = x2 + w2;
        pt[1].y = y2 + h2;
        pt[2].x = x2 + w2;
        pt[2].y = y2;
        pt[3].x = x2 + w2 - sw2;
        pt[3].y = y2 + sw2;
        pt[4].x = x2 + w2 - sw2;
        pt[4].y = y2 + h2 - sw2;
        pt[5].x = x2 + sw2;
        pt[5].y = y2 + h2 - sw2;
        
        xcb_change_gc(ctx->connection, data->gc,
                      XCB_GC_FOREGROUND, &inner_dark);
        xcb_fill_poly(ctx->connection, ctx->window, data->gc,
                      XCB_POLY_SHAPE_COMPLEX,
                      XCB_COORD_MODE_ORIGIN, 6, pt);
    }
    
    /* Restore current color */
    xcb_change_gc(ctx->connection, data->gc,
                  XCB_GC_FOREGROUND, &data->current_fg);
}

static void
xcb_draw_bevel(ISWRenderContext *ctx, int x, int y, int w, int h,
              int bevel_width, Boolean raised)
{
    /* Use shadow drawing with appropriate relief */
    XtRelief relief = raised ? XtReliefRaised : XtReliefSunken;
    
    xcb_draw_shadow(ctx, x, y, w, h, bevel_width, relief,
                   ctx->screen->white_pixel,
                   ctx->screen->black_pixel);
}

/*
 * =================================================================
 * Text Rendering
 * =================================================================
 */

static void
xcb_draw_string(ISWRenderContext *ctx, const char *text, int len,
               int x, int y)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    /* Use ISWXcbDrawString from ISWXcbDraw.h */
    ISWXcbDrawString(ctx->connection, ctx->window, data->gc,
                     x, y, text, len);
}

static int
xcb_text_width(ISWRenderContext *ctx, const char *text, int len)
{
    /* Use font if set, otherwise estimate */
    if (ctx->current_font) {
        return ISWFontStructTextWidth(ctx->current_font, text, len);
    }
    
    /* Rough estimate: 8 pixels per character */
    return len * 8;
}

static int
xcb_text_height(ISWRenderContext *ctx)
{
    /* Use font if set */
    if (ctx->current_font) {
        return ctx->current_font->ascent + ctx->current_font->descent;
    }
    
    /* Rough estimate */
    return 12;
}

static void
xcb_set_font(ISWRenderContext *ctx, XFontStruct *font)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    
    if (!font) {
        return;
    }
    
    /* Set font in GC */
    xcb_change_gc(ctx->connection, data->gc,
                  XCB_GC_FONT, &font->fid);
}

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

static void
xcb_set_clip_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    xcb_rectangle_t clip_rect;
    
    clip_rect.x = x;
    clip_rect.y = y;
    clip_rect.width = w;
    clip_rect.height = h;
    
    xcb_set_clip_rectangles(ctx->connection, XCB_CLIP_ORDERING_YX_BANDED,
                            data->gc, 0, 0, 1, &clip_rect);
}

static void
xcb_clear_clip(ISWRenderContext *ctx)
{
    ISWRenderXCBData *data = (ISWRenderXCBData*)ctx->backend_data;
    uint32_t clip_mask = XCB_NONE;
    
    xcb_change_gc(ctx->connection, data->gc,
                  XCB_GC_CLIP_MASK, &clip_mask);
}

/*
 * =================================================================
 * Advanced Features (Not Supported)
 * =================================================================
 */

static Boolean
xcb_set_gradient(ISWRenderContext *ctx, double x1, double y1, double x2, double y2,
                Pixel c1, Pixel c2)
{
    /* XCB doesn't support gradients */
    (void)ctx;
    (void)x1; (void)y1; (void)x2; (void)y2;
    (void)c1; (void)c2;
    return False;
}

static void*
xcb_get_cairo_context(ISWRenderContext *ctx)
{
    /* XCB backend has no Cairo context */
    (void)ctx;
    return NULL;
}

/*
 * =================================================================
 * Operations Vtable
 * =================================================================
 */

const ISWRenderOps isw_render_xcb_ops = {
    .init = ISWRenderXCBInit,
    .destroy = ISWRenderXCBDestroy,
    .begin = xcb_begin,
    .end = xcb_end,
    .save = xcb_save,
    .restore = xcb_restore,
    .set_color = xcb_set_color,
    .set_color_rgba = xcb_set_color_rgba,
    .set_line_width = xcb_set_line_width,
    .fill_rectangle = xcb_fill_rectangle,
    .stroke_rectangle = xcb_stroke_rectangle,
    .fill_polygon = xcb_fill_polygon,
    .stroke_polygon = xcb_stroke_polygon,
    .draw_line = xcb_draw_line,
    .draw_arc = xcb_draw_arc,
    .draw_string = xcb_draw_string,
    .text_width = xcb_text_width,
    .text_height = xcb_text_height,
    .set_font = xcb_set_font,
    .draw_shadow = xcb_draw_shadow,
    .draw_bevel = xcb_draw_bevel,
    .set_clip_rectangle = xcb_set_clip_rectangle,
    .clear_clip = xcb_clear_clip,
    .set_gradient = xcb_set_gradient,
    .get_cairo_context = xcb_get_cairo_context
};
