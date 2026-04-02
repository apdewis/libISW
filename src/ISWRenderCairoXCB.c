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

#include "ISWRenderPrivate.h"
#include <X11/IntrinsicP.h> /* For Xt private types */
#include <X11/CoreP.h>       /* For accessing widget->core fields */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cairo/cairo-ft.h>
#include <xcb/present.h>

/* Defined in Initialize.c */
extern double _XtGetScaleFactor(xcb_connection_t *dpy);

/* Defined in ISWRender.c — TTF font resolution via fontconfig/FreeType/cairo-ft */
extern void _ISWSetCairoFontFromXFont(cairo_t *cr, XFontStruct *font, double scale);

/*
 * Cairo-XCB Backend Data
 */
typedef struct {
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

    /* State for save/restore */
    int save_count;

    /* Present extension for vsync'd blits */
    Boolean present_ok;            /* Present extension usable for this window */
    xcb_present_event_t present_eid; /* event context id */
    uint32_t present_serial;       /* monotonic serial for present_pixmap */

    /* Deferred initialization: surface created on first begin() if widget
     * had zero dimensions at ISWRenderCreate time. */
    Boolean deferred;
} ISWRenderCairoXCBData;

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
static Boolean
_cairo_xcb_create_surface(ISWRenderContext *ctx, ISWRenderCairoXCBData *data)
{
    Dimension w = ctx->widget->core.width;
    Dimension h = ctx->widget->core.height;

    /* Clamp oversized dimensions — Cairo's XCB surface limit */
    if (w > 32767) w = 32767;
    if (h > 32767) h = 32767;

    data->surface = cairo_xcb_surface_create(
        ctx->connection,
        ctx->window,
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
                xcb_present_query_version(ctx->connection, 1, 0);
            xcb_present_query_version_reply_t *vr =
                xcb_present_query_version_reply(ctx->connection, vc, NULL);
            if (vr) {
                present_available = 1;
                free(vr);
            }
        }

        if (present_available) {
            data->present_eid = xcb_generate_id(ctx->connection);
            xcb_present_select_input(ctx->connection,
                                     data->present_eid, ctx->window,
                                     XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);
            data->present_ok = True;
            data->present_serial = 0;
        }
    }

    return True;
}

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

    /* Find visual for depth, fall back to root depth visual */
    data->visual = ISWRenderFindVisual(ctx->screen, depth);
    if (!data->visual && depth != ctx->screen->root_depth) {
        data->visual = ISWRenderFindVisual(ctx->screen, ctx->screen->root_depth);
    }
    if (!data->visual) {
        fprintf(stderr, "ISWRenderCairoXCB: No usable visual found\n");
        free(data);
        return False;
    }

    /* Set capabilities early — available even while deferred */
    ctx->capabilities = ISW_RENDER_CAP_BASIC |
                       ISW_RENDER_CAP_ANTIALIASING |
                       ISW_RENDER_CAP_GRADIENTS |
                       ISW_RENDER_CAP_ALPHA |
                       ISW_RENDER_CAP_TRANSFORMS |
                       ISW_RENDER_CAP_TEXT_ADVANCED;

    data->save_count = 0;

    /* Defer surface creation if widget has no dimensions yet */
    if (ctx->widget->core.width == 0 || ctx->widget->core.height == 0 ||
        ctx->window == 0) {
        data->deferred = True;
        data->surface = NULL;
        data->cairo_ctx = NULL;
        ctx->backend_data = data;
        return True;
    }

    data->deferred = False;
    ctx->backend_data = data;

    if (!_cairo_xcb_create_surface(ctx, data)) {
        ctx->backend_data = NULL;
        free(data);
        return False;
    }

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
    
    /* Destroy back buffer */
    if (data->back_ctx) {
        cairo_destroy(data->back_ctx);
    }
    if (data->back_surface) {
        cairo_surface_destroy(data->back_surface);
    }
    if (data->back_pixmap && ctx->connection) {
        xcb_free_pixmap(ctx->connection, data->back_pixmap);
    }

    /* Release Present event context */
    if (data->present_ok && ctx->connection) {
        xcb_present_select_input(ctx->connection,
                                 data->present_eid, ctx->window,
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

    /* Complete deferred initialization now that the widget has a window */
    if (data->deferred) {
        if (ctx->widget->core.width == 0 || ctx->widget->core.height == 0 ||
            ctx->window == 0) {
            return;  /* Still not ready */
        }
        /* Pick up the window that wasn't available at init time */
        if (ctx->window != 0 && ctx->window != (xcb_window_t)-1) {
            ctx->connection = (xcb_connection_t*)XtDisplay(ctx->widget);
            ctx->window = XtWindow(ctx->widget);
        }
        if (!_cairo_xcb_create_surface(ctx, data)) {
            return;  /* Surface creation failed — skip this frame */
        }
        data->deferred = False;
    }

    /* Update window surface size */
    if (ctx->widget && data->surface) {
        Dimension w = ctx->widget->core.width;
        Dimension h = ctx->widget->core.height;
        cairo_xcb_surface_set_size(data->surface, w, h);

        /* Ensure back buffer can hold widget dimensions.
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
                xcb_free_pixmap(ctx->connection, data->back_pixmap);
                data->back_pixmap = 0;
            }

            /* Over-allocate by 25% to absorb subsequent small resizes */
            Dimension aw = w + w / 4;
            Dimension ah = h + h / 4;
            if (aw < 1) aw = 1;
            if (ah < 1) ah = 1;

            /* Create new back buffer */
            uint8_t depth = (ctx->widget->core.depth != 0)
                          ? ctx->widget->core.depth
                          : ctx->screen->root_depth;
            data->back_pixmap = xcb_generate_id(ctx->connection);
            xcb_create_pixmap(ctx->connection, depth, data->back_pixmap,
                              ctx->window, aw, ah);
            data->back_surface = cairo_xcb_surface_create(
                ctx->connection, data->back_pixmap, data->visual, aw, ah);
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
                uint32_t bg = ctx->widget->core.background_pixel;
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

        /* Seed the back buffer so partial repaints preserve context.
         *
         * Without Present: copy the window surface (X server keeps it
         * current via background fills and the previous cairo blit).
         *
         * With Present: the back buffer already holds the previous
         * frame (Present copies from it, never into it).  Copying
         * from the window surface would race with the async present.
         * Skip the copy — the back buffer is already authoritative. */
        if (!data->present_ok) {
            cairo_surface_flush(data->surface);
            cairo_set_source_surface(data->back_ctx, data->surface, 0, 0);
            cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_SOURCE);
            cairo_paint(data->back_ctx);
            cairo_set_operator(data->back_ctx, CAIRO_OPERATOR_OVER);
        }

        data->cairo_ctx = data->back_ctx;
        cairo_save(data->cairo_ctx);
    }
}

static void
cairo_xcb_end(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx)
        return;

    cairo_restore(data->cairo_ctx);

    /* Blit back buffer to window */
    if (data->back_surface) {
        cairo_surface_flush(data->back_surface);

        if (data->present_ok && data->back_pixmap) {
            /* Vsync'd present: the X server will schedule the
             * pixmap copy at the next vblank, eliminating tearing.
             * target_msc=0 means "next vblank". */
            xcb_present_pixmap(ctx->connection,
                               ctx->window,
                               data->back_pixmap,
                               ++data->present_serial,
                               XCB_NONE,       /* valid region (whole) */
                               XCB_NONE,       /* update region (whole) */
                               0, 0,           /* x/y offset */
                               XCB_NONE,       /* target_crtc (auto) */
                               XCB_NONE,       /* wait_fence */
                               XCB_NONE,       /* idle_fence */
                               XCB_PRESENT_OPTION_COPY, /* copy, don't flip */
                               0,              /* target_msc: next vblank */
                               1,              /* divisor */
                               0,              /* remainder */
                               0, NULL);       /* notifies */
        } else if (data->window_ctx) {
            /* Fallback: immediate cairo blit (no vsync) */
            cairo_set_source_surface(data->window_ctx, data->back_surface, 0, 0);
            cairo_set_operator(data->window_ctx, CAIRO_OPERATOR_SOURCE);
            cairo_paint(data->window_ctx);
            cairo_set_operator(data->window_ctx, CAIRO_OPERATOR_OVER);
        }
    }

    if (data->surface)
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

    if (!data->cairo_ctx) return;
    cairo_save(data->cairo_ctx);
    data->save_count++;
}

static void
cairo_xcb_restore(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    double r, g, b;

    ctx->current_color = pixel;
    if (!data->cairo_ctx) return;
    ISWRenderPixelToRGB(ctx, pixel, &r, &g, &b);
    cairo_set_source_rgb(data->cairo_ctx, r, g, b);
}

static void
cairo_xcb_set_color_rgba(ISWRenderContext *ctx, double r, double g, double b, double a)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx) return;
    cairo_set_source_rgba(data->cairo_ctx, r, g, b, a);
}

static void
cairo_xcb_set_line_width(ISWRenderContext *ctx, double width)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx) return;
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_fill(data->cairo_ctx);
}

static void
cairo_xcb_stroke_rectangle(ISWRenderContext *ctx, int x, int y, int w, int h)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

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
cairo_xcb_fill_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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
cairo_xcb_stroke_polygon(ISWRenderContext *ctx, xcb_point_t *points, int num)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx) return;
    cairo_move_to(data->cairo_ctx, x1, y1);
    cairo_line_to(data->cairo_ctx, x2, y2);
    cairo_stroke(data->cairo_ctx);
}

static void
cairo_xcb_draw_arc(ISWRenderContext *ctx, int x, int y, int w, int h,
                  double angle1, double angle2)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    cairo_text_extents_t extents;
    char *null_term;
    int width;

    if (!data->cairo_ctx) return len * 8;
    /* Cairo expects null-terminated string */
    null_term = (char*)malloc(len + 1);
    if (!null_term) {
        return len * 8;  /* Estimate */
    }
    
    memcpy(null_term, text, len);
    null_term[len] = '\0';
    
    cairo_text_extents(data->cairo_ctx, null_term, &extents);
    /* Use x_advance (pen advance), not width (ink bounding box).
     * width can be smaller than x_advance due to side bearings,
     * causing cumulative positioning drift in multi-chunk text. */
    width = (int)ceil(extents.x_advance);

    free(null_term);

    return width;
}

static int
cairo_xcb_text_height(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
    cairo_font_extents_t extents;

    if (!data->cairo_ctx) return 12;
    cairo_font_extents(data->cairo_ctx, &extents);

    return (int)(extents.ascent + extents.descent);
}

static void
cairo_xcb_set_font(ISWRenderContext *ctx, XFontStruct *font)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx) return;

    double scale = _XtGetScaleFactor(ctx->connection);
    _ISWSetCairoFontFromXFont(data->cairo_ctx, font, scale);

    /* Keep both contexts in sync so text queries work outside begin/end */
    if (data->back_ctx && data->cairo_ctx != data->back_ctx)
        _ISWSetCairoFontFromXFont(data->back_ctx, font, scale);
    if (data->window_ctx && data->cairo_ctx != data->window_ctx)
        _ISWSetCairoFontFromXFont(data->window_ctx, font, scale);
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

    if (!data->cairo_ctx) return;
    cairo_reset_clip(data->cairo_ctx);
    cairo_rectangle(data->cairo_ctx, x, y, w, h);
    cairo_clip(data->cairo_ctx);
}

static void
cairo_xcb_clear_clip(ISWRenderContext *ctx)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->surface) return;
    /*
     * For within-window scrolling, flush the Cairo surface so all pending
     * drawing is committed to the X window, then use XCB copy_area for the
     * server-side pixel copy, then mark the surface dirty so Cairo re-reads.
     */
    cairo_surface_flush(data->surface);
    xcb_flush(ctx->connection);

    /* Create a temporary xcb_gcontext_t for the copy */
    xcb_gcontext_t gc = xcb_generate_id(ctx->connection);
    uint32_t gc_mask = XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t gc_vals[] = { 0 };
    xcb_create_gc(ctx->connection, gc, ctx->window, gc_mask, gc_vals);

    xcb_copy_area(ctx->connection, ctx->window, ctx->window,
                  gc, src_x, src_y, dst_x, dst_y, width, height);
    xcb_flush(ctx->connection);

    xcb_free_gc(ctx->connection, gc);

    /* Tell Cairo the surface contents changed underneath it */
    cairo_surface_mark_dirty(data->surface);
}

static void
cairo_xcb_draw_pixmap(ISWRenderContext *ctx,
                      xcb_pixmap_t pixmap,
                      int src_x, int src_y,
                      int dst_x, int dst_y,
                      unsigned int width, unsigned int height,
                      unsigned int depth)
{
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;

    if (!data->cairo_ctx) return;
    if (depth == 1) {
        /*
         * 1-bit bitmap: create a Cairo surface for the bitmap and use it
         * as a mask with the current source color (foreground).
         */
        cairo_surface_t *bitmap_surface;

        bitmap_surface = cairo_xcb_surface_create_for_bitmap(
            ctx->connection, ctx->screen, pixmap, width, height);

        if (cairo_surface_status(bitmap_surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(bitmap_surface);
            /* Fallback to XCB */
            cairo_surface_flush(data->surface);
            xcb_gcontext_t gc = xcb_generate_id(ctx->connection);
            uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
            uint32_t vals[] = { (uint32_t)ctx->current_color, 0 };
            xcb_create_gc(ctx->connection, gc, ctx->window, mask, vals);
            xcb_copy_plane(ctx->connection, pixmap, ctx->window, gc,
                           src_x, src_y, dst_x, dst_y, width, height, 1);
            xcb_free_gc(ctx->connection, gc);
            xcb_flush(ctx->connection);
            cairo_surface_mark_dirty(data->surface);
            return;
        }

        /* The current source color is already set — use bitmap as mask */
        cairo_mask_surface(data->cairo_ctx, bitmap_surface,
                           dst_x - src_x, dst_y - src_y);

        cairo_surface_destroy(bitmap_surface);
    } else {
        /*
         * Multi-plane pixmap: create a Cairo surface and paint it
         * onto the window surface.
         */
        cairo_surface_t *pixmap_surface;

        pixmap_surface = cairo_xcb_surface_create(
            ctx->connection, pixmap, data->visual, width, height);

        if (cairo_surface_status(pixmap_surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(pixmap_surface);
            /* Fallback to XCB */
            cairo_surface_flush(data->surface);
            xcb_gcontext_t gc = xcb_generate_id(ctx->connection);
            uint32_t mask = XCB_GC_GRAPHICS_EXPOSURES;
            uint32_t vals[] = { 0 };
            xcb_create_gc(ctx->connection, gc, ctx->window, mask, vals);
            xcb_copy_area(ctx->connection, pixmap, ctx->window, gc,
                          src_x, src_y, dst_x, dst_y, width, height);
            xcb_free_gc(ctx->connection, gc);
            xcb_flush(ctx->connection);
            cairo_surface_mark_dirty(data->surface);
            return;
        }

        cairo_set_source_surface(data->cairo_ctx, pixmap_surface,
                                 dst_x - src_x, dst_y - src_y);
        cairo_rectangle(data->cairo_ctx, dst_x, dst_y, width, height);
        cairo_fill(data->cairo_ctx);

        cairo_surface_destroy(pixmap_surface);

        /* Restore the previous source color */
        cairo_xcb_set_color(ctx, ctx->current_color);
    }
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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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

    /* Scale if destination size differs from image size */
    if (dst_w != img_w || dst_h != img_h) {
        cairo_translate(data->cairo_ctx, dst_x, dst_y);
        cairo_scale(data->cairo_ctx,
                    (double)dst_w / (double)img_w,
                    (double)dst_h / (double)img_h);
        cairo_set_source_surface(data->cairo_ctx, img_surface, 0, 0);
    } else {
        cairo_set_source_surface(data->cairo_ctx, img_surface, dst_x, dst_y);
    }

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
    ISWRenderCairoXCBData *data = (ISWRenderCairoXCBData*)ctx->backend_data;
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
    .set_clip_rectangle = cairo_xcb_set_clip_rectangle,
    .clear_clip = cairo_xcb_clear_clip,
    .copy_area = cairo_xcb_copy_area,
    .draw_pixmap = cairo_xcb_draw_pixmap,
    .draw_image_rgba = cairo_xcb_draw_image_rgba,
    .set_gradient = cairo_xcb_set_gradient,
    .get_cairo_context = cairo_xcb_get_cairo_context
};
