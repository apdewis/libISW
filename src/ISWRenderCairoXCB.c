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

#ifdef HAVE_CAIRO

#include "ISWRenderPrivate.h"
#include <X11/IntrinsicP.h> /* For Xt private types */
#include <X11/CoreP.h>       /* For accessing widget->core fields */
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Cairo-XCB Backend Data
 */
typedef struct {
    cairo_surface_t *surface;
    cairo_t *cairo_ctx;
    xcb_visualtype_t *visual;
    
    /* State for save/restore */
    int save_count;
} ISWRenderCairoXCBData;

/*
 * =================================================================
 * Lifecycle
 * =================================================================
 */

Boolean
ISWRenderCairoXCBInit(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data;
    uint8_t depth;
    
    /* Allocate backend data */
    data = (ISWRenderCairoXCBData*)calloc(1, sizeof(*data));
    if (!data) {
        return False;
    }
    
    /* Use widget depth if set, otherwise use screen's root depth */
    depth = (ctx->widget->core.depth != 0) ? ctx->widget->core.depth : ctx->screen->root_depth;
    
    /* Find visual for depth */
    data->visual = ISWRenderFindVisual(ctx->screen, depth);
    if (!data->visual) {
        fprintf(stderr, "ISWRenderCairoXCB: Failed to find visual for depth %d\n", depth);
        free(data);
        return False;
    }
    
    /* Validate widget has non-zero dimensions */
    if (ctx->widget->core.width == 0 || ctx->widget->core.height == 0) {
        /* Widget not sized yet - defer to XCB backend or wait for resize */
        free(data);
        return False;
    }
    
    /* Validate dimensions are reasonable (Cairo has limits) */
    if (ctx->widget->core.width > 32767 || ctx->widget->core.height > 32767) {
        fprintf(stderr, "ISWRenderCairoXCB: Widget '%s' (class %s) dimensions too large (%dx%d)\n",
                XtName(ctx->widget),
                ctx->widget->core.widget_class->core_class.class_name,
                ctx->widget->core.width, ctx->widget->core.height);
        free(data);
        return False;
    }
    
    /* Create Cairo XCB surface - PURE XCB, NO XLIB */
    data->surface = cairo_xcb_surface_create(
        ctx->connection,           /* xcb_connection_t*, NOT Display* */
        ctx->window,              /* xcb_window_t, NOT Window */
        data->visual,             /* xcb_visualtype_t*, NOT Visual* */
        ctx->widget->core.width,
        ctx->widget->core.height
    );
    
    if (cairo_surface_status(data->surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "ISWRenderCairoXCB: Failed to create Cairo surface: %s\n",
               cairo_status_to_string(cairo_surface_status(data->surface)));
        cairo_surface_destroy(data->surface);
        free(data);
        return False;
    }
    
    /* Create Cairo context */
    data->cairo_ctx = cairo_create(data->surface);
    if (cairo_status(data->cairo_ctx) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "ISWRenderCairoXCB: Failed to create Cairo context: %s\n",
               cairo_status_to_string(cairo_status(data->cairo_ctx)));
        cairo_destroy(data->cairo_ctx);
        cairo_surface_destroy(data->surface);
        free(data);
        return False;
    }
    
    /* Set capabilities */
    ctx->capabilities = ISW_RENDER_CAP_BASIC |
                       ISW_RENDER_CAP_ANTIALIASING |
                       ISW_RENDER_CAP_GRADIENTS |
                       ISW_RENDER_CAP_ALPHA |
                       ISW_RENDER_CAP_TRANSFORMS |
                       ISW_RENDER_CAP_TEXT_ADVANCED;
    
    ctx->backend_data = data;
    data->save_count = 0;
    
    /* Set default rendering quality */
    cairo_set_antialias(data->cairo_ctx, CAIRO_ANTIALIAS_GOOD);
    cairo_set_line_width(data->cairo_ctx, 1.0);
    cairo_set_operator(data->cairo_ctx, CAIRO_OPERATOR_OVER);
    
    return True;
}

void
ISWRenderCairoXCBDestroy(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data;
    
    if (!ctx || !ctx->backend_data) {
        return;
    }
    
    data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    /* Destroy Cairo context */
    if (data->cairo_ctx) {
        cairo_destroy(data->cairo_ctx);
    }
    
    /* Destroy Cairo surface */
    if (data->surface) {
        cairo_surface_destroy(data->surface);
    }
    
    /* Free backend data */
    free(data);
    ctx->backend_data = NULL;
}

void
ISWRenderCairoXCBResize(ISWRenderContext *ctx, int width, int height)
{
    ISWRenderCairoXCBData *data;
    
    if (!ctx || !ctx->backend_data) {
        return;
    }
    
    data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    /* Update surface size */
    cairo_xcb_surface_set_size(data->surface, width, height);
}

/*
 * =================================================================
 * Frame Operations
 * =================================================================
 */

static void
cairo_xcb_begin(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    /* Save initial state */
    cairo_save(data->cairo_ctx);
}

static void
cairo_xcb_end(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    /* Restore initial state */
    cairo_restore(data->cairo_ctx);
    
    /* Flush to XCB */
    cairo_surface_flush(data->surface);
    xcb_flush(ctx->connection);
}

/*
 * =================================================================
 * State Management
 * =================================================================
 */

static void
cairo_xcb_save(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_save(data->cairo_ctx);
    data->save_count++;
}

static void
cairo_xcb_restore(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    double r, g, b;
    
    ISWRenderPixelToRGB(ctx, pixel, &r, &g, &b);
    cairo_set_source_rgb(data->cairo_ctx, r, g, b);
    
    ctx->current_color = pixel;
}

static void
cairo_xcb_set_color_rgba(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_set_source_rgba(data->cairo_ctx, r, g, b, a);
}

static void
cairo_xcb_set_line_width(ISWRenderContext *ctx, double width)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_set_line_width(data->cairo_ctx, width);
    ctx->line_width = width;
}

/*
 * =================================================================
 * Drawing Primitives
 * =================================================================
 */

static void
cairo_xcb_fill_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
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
cairo_xcb_fill_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    int i;
    
    if (num < 3) {
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
cairo_xcb_stroke_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    int i;
    
    if (num < 2) {
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_move_to(data->cairo_ctx, x1, y1);
    cairo_line_to(data->cairo_ctx, x2, y2);
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_draw_arc(ISWRenderContext *ctx, int x, int y, int w, int h,
                  double angle1, double angle2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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

/*
 * =================================================================
 * Widget-Specific Drawing - Anti-aliased Shadows
 * =================================================================
 */

static void
cairo_xcb_draw_shadow(ISWRenderContext *ctx, int x, int y, int w, int h,
                     int shadow_width, XtRelief relief,
                     Pixel top_color, Pixel bottom_color)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    double tr, tg, tb, br, bg, bb;
    Pixel light, dark;
    
    /* Convert pixel colors to RGB */
    ISWRenderPixelToRGB(ctx, top_color, &tr, &tg, &tb);
    ISWRenderPixelToRGB(ctx, bottom_color, &br, &bg, &bb);
    
    /* Determine light and dark colors based on relief */
    if (relief == XtReliefRaised) {
        light = top_color;
        dark = bottom_color;
    } else if (relief == XtReliefSunken) {
        light = bottom_color;
        dark = top_color;
    } else {
        light = top_color;
        dark = bottom_color;
    }
    
    if (relief == XtReliefRaised || relief == XtReliefSunken) {
        /* Top-left shadow with anti-aliasing */
        ISWRenderPixelToRGB(ctx, light, &tr, &tg, &tb);
        cairo_set_source_rgb(data->cairo_ctx, tr, tg, tb);
        
        cairo_move_to(data->cairo_ctx, x, y + h);
        cairo_line_to(data->cairo_ctx, x, y);
        cairo_line_to(data->cairo_ctx, x + w, y);
        cairo_line_to(data->cairo_ctx, x + w - shadow_width, y + shadow_width);
        cairo_line_to(data->cairo_ctx, x + shadow_width, y + shadow_width);
        cairo_line_to(data->cairo_ctx, x + shadow_width, y + h - shadow_width);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
        
        /* Bottom-right shadow with anti-aliasing */
        ISWRenderPixelToRGB(ctx, dark, &br, &bg, &bb);
        cairo_set_source_rgb(data->cairo_ctx, br, bg, bb);
        
        cairo_move_to(data->cairo_ctx, x, y + h);
        cairo_line_to(data->cairo_ctx, x + w, y + h);
        cairo_line_to(data->cairo_ctx, x + w, y);
        cairo_line_to(data->cairo_ctx, x + w - shadow_width, y + shadow_width);
        cairo_line_to(data->cairo_ctx, x + w - shadow_width, y + h - shadow_width);
        cairo_line_to(data->cairo_ctx, x + shadow_width, y + h - shadow_width);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
        
    } else if (relief == XtReliefRidge || relief == XtReliefGroove) {
        /* Ridge/Groove: split shadow and draw layered */
        int sw = shadow_width / 2;
        Pixel outer_light = (relief == XtReliefRidge) ? top_color : bottom_color;
        Pixel outer_dark = (relief == XtReliefRidge) ? bottom_color : top_color;
        Pixel inner_light = (relief == XtReliefRidge) ? bottom_color : top_color;
        Pixel inner_dark = (relief == XtReliefRidge) ? top_color : bottom_color;
        
        /* Outer top-left */
        ISWRenderPixelToRGB(ctx, outer_light, &tr, &tg, &tb);
        cairo_set_source_rgb(data->cairo_ctx, tr, tg, tb);
        cairo_move_to(data->cairo_ctx, x, y + h);
        cairo_line_to(data->cairo_ctx, x, y);
        cairo_line_to(data->cairo_ctx, x + w, y);
        cairo_line_to(data->cairo_ctx, x + w - sw, y + sw);
        cairo_line_to(data->cairo_ctx, x + sw, y + sw);
        cairo_line_to(data->cairo_ctx, x + sw, y + h - sw);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
        
        /* Outer bottom-right */
        ISWRenderPixelToRGB(ctx, outer_dark, &br, &bg, &bb);
        cairo_set_source_rgb(data->cairo_ctx, br, bg, bb);
        cairo_move_to(data->cairo_ctx, x, y + h);
        cairo_line_to(data->cairo_ctx, x + w, y + h);
        cairo_line_to(data->cairo_ctx, x + w, y);
        cairo_line_to(data->cairo_ctx, x + w - sw, y + sw);
        cairo_line_to(data->cairo_ctx, x + w - sw, y + h - sw);
        cairo_line_to(data->cairo_ctx, x + sw, y + h - sw);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
        
        /* Inner layer */
        int sw2 = shadow_width - sw;
        int x2 = x + sw;
        int y2 = y + sw;
        int w2 = w - 2 * sw;
        int h2 = h - 2 * sw;
        
        /* Inner top-left */
        ISWRenderPixelToRGB(ctx, inner_light, &tr, &tg, &tb);
        cairo_set_source_rgb(data->cairo_ctx, tr, tg, tb);
        cairo_move_to(data->cairo_ctx, x2, y2 + h2);
        cairo_line_to(data->cairo_ctx, x2, y2);
        cairo_line_to(data->cairo_ctx, x2 + w2, y2);
        cairo_line_to(data->cairo_ctx, x2 + w2 - sw2, y2 + sw2);
        cairo_line_to(data->cairo_ctx, x2 + sw2, y2 + sw2);
        cairo_line_to(data->cairo_ctx, x2 + sw2, y2 + h2 - sw2);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
        
        /* Inner bottom-right */
        ISWRenderPixelToRGB(ctx, inner_dark, &br, &bg, &bb);
        cairo_set_source_rgb(data->cairo_ctx, br, bg, bb);
        cairo_move_to(data->cairo_ctx, x2, y2 + h2);
        cairo_line_to(data->cairo_ctx, x2 + w2, y2 + h2);
        cairo_line_to(data->cairo_ctx, x2 + w2, y2);
        cairo_line_to(data->cairo_ctx, x2 + w2 - sw2, y2 + sw2);
        cairo_line_to(data->cairo_ctx, x2 + w2 - sw2, y2 + h2 - sw2);
        cairo_line_to(data->cairo_ctx, x2 + sw2, y2 + h2 - sw2);
        cairo_close_path(data->cairo_ctx);
        cairo_fill(data->cairo_ctx);
    }
    
    /* Restore current color */
    cairo_xcb_set_color(ctx, ctx->current_color);
}

static void
cairo_xcb_draw_bevel(ISWRenderContext *ctx, int x, int y, int w, int h,
                    int bevel_width, Boolean raised)
{
    XtRelief relief = raised ? XtReliefRaised : XtReliefSunken;
    
    cairo_xcb_draw_shadow(ctx, x, y, w, h, bevel_width, relief,
                         ctx->screen->white_pixel,
                         ctx->screen->black_pixel);
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    char *null_term;
    
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    cairo_text_extents_t extents;
    char *null_term;
    int width;
    
    /* Cairo expects null-terminated string */
    null_term = (char*)malloc(len + 1);
    if (!null_term) {
        return len * 8;  /* Estimate */
    }
    
    memcpy(null_term, text, len);
    null_term[len] = '\0';
    
    cairo_text_extents(data->cairo_ctx, null_term, &extents);
    width = (int)extents.width;
    
    free(null_term);
    
    return width;
}

static int
cairo_xcb_text_height(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    cairo_font_extents_t extents;
    
    cairo_font_extents(data->cairo_ctx, &extents);
    
    return (int)(extents.ascent + extents.descent);
}

static void
cairo_xcb_set_font(ISWRenderContext *ctx, XFontStruct *font)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    /* For now, use Cairo's toy font API */
    /* TODO: Convert XFontStruct to Cairo font for better matching */
    
    /* Set a reasonable default font */
    cairo_select_font_face(data->cairo_ctx, "Sans",
                          CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_NORMAL);
    
    /* Use font size from XFontStruct if available */
    if (font) {
        double size = font->ascent + font->descent;
        cairo_set_font_size(data->cairo_ctx, size);
    } else {
        cairo_set_font_size(data->cairo_ctx, 12.0);
    }
}

/*
 * =================================================================
 * Clipping
 * =================================================================
 */

static void
cairo_xcb_set_clip_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_reset_clip(data->cairo_ctx);
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_clip(data->cairo_ctx);
}

static void
cairo_xcb_clear_clip(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    
    cairo_reset_clip(data->cairo_ctx);
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    cairo_pattern_t *gradient;
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

static void*
cairo_xcb_get_cairo_context(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data;
    
    if (!ctx || !ctx->backend_data) {
        return NULL;
    }
    
    data = (ISWRenderCairoXCBData*)ctx->backend_data;
    return (void*)data->cairo_ctx;
}

/*
 * =================================================================
 * Operations Vtable
 * =================================================================
 */

const ISWRenderOps isw_render_cairo_xcb_ops = {
    .init = ISWRenderCairoXCBInit,
    .destroy = ISWRenderCairoXCBDestroy,
    .begin = cairo_xcb_begin,
    .end = cairo_xcb_end,
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
    .draw_string = cairo_xcb_draw_string,
    .text_width = cairo_xcb_text_width,
    .text_height = cairo_xcb_text_height,
    .set_font = cairo_xcb_set_font,
    .draw_shadow = cairo_xcb_draw_shadow,
    .draw_bevel = cairo_xcb_draw_bevel,
    .set_clip_rectangle = cairo_xcb_set_clip_rectangle,
    .clear_clip = cairo_xcb_clear_clip,
    .set_gradient = cairo_xcb_set_gradient,
    .get_cairo_context = cairo_xcb_get_cairo_context
};

#endif /* HAVE_CAIRO */
