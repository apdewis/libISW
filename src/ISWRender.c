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
#include <math.h>
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* Defined in Initialize.c — avoids pulling in InitialI.h */
extern double _XtGetScaleFactor(xcb_connection_t *dpy);

/*
 * =================================================================
 * Backend Availability Checks
 * =================================================================
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
            return ISW_RENDER_BACKEND_CAIRO_XCB;
        }
    }

    /* Auto-detect: prefer best available */
#ifdef HAVE_CAIRO_EGL
    if (ISWRenderBackendAvailable(ISW_RENDER_BACKEND_CAIRO_EGL)) {
        return ISW_RENDER_BACKEND_CAIRO_EGL;
    }
#endif

    return ISW_RENDER_BACKEND_CAIRO_XCB;
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
        case ISW_RENDER_BACKEND_CAIRO_XCB:
            ctx->ops = &isw_render_cairo_xcb_ops;
            break;

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
        fprintf(stderr, "ISWRender: Backend %d init failed\n", ctx->backend);
        free(ctx);
        return NULL;
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
        return ISW_RENDER_BACKEND_CAIRO_XCB;  /* Safe default */
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
 * RGBA Image Rendering
 * =================================================================
 */

void
ISWRenderDrawImageRGBA(ISWRenderContext *ctx,
                       const unsigned char *rgba,
                       unsigned int img_width, unsigned int img_height,
                       int dst_x, int dst_y,
                       unsigned int dst_w, unsigned int dst_h)
{
    if (!ctx || !ctx->ops || !ctx->ops->draw_image_rgba || !rgba)
        return;

    ctx->ops->draw_image_rgba(ctx, rgba, img_width, img_height,
                              dst_x, dst_y, dst_w, dst_h);
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

/*
 * =================================================================
 * HiDPI Scaling
 * =================================================================
 */

double
ISWScaleFactor(Widget widget)
{
    if (!widget)
        return 1.0;
    return _XtGetScaleFactor(XtDisplayOfObject(widget));
}

Dimension
ISWScaleDim(Widget widget, int value)
{
    double scale = ISWScaleFactor(widget);
    int result = (int)(value * scale + 0.5);
    return (Dimension)(result > 0 ? result : 1);
}

#include <cairo.h>
#include <math.h>

/*
 * =================================================================
 * FreeType / Fontconfig Font Resolution
 *
 * Provides TTF/OTF font support via fontconfig (font discovery) +
 * FreeType (font loading) + cairo-ft (Cairo integration).
 * No Xlib dependencies in this path.
 * =================================================================
 */

static FT_Library _ft_library = NULL;

/* Cache for resolved font faces — avoids repeated fontconfig lookups */
typedef struct _ISWFontCacheEntry {
    struct _ISWFontCacheEntry *next;
    char *pattern_key;          /* "family:size:weight:slant" */
    cairo_font_face_t *cr_face;
    FT_Face ft_face;
} _ISWFontCacheEntry;

static _ISWFontCacheEntry *_font_cache = NULL;

static void
_ISWInitFreeType(void)
{
    if (!_ft_library) {
        FT_Init_FreeType(&_ft_library);
    }
}

/*
 * _ISWResolveFontFace - Resolve a font description to a Cairo font face.
 *
 * Uses fontconfig to find a matching font file, FreeType to load it,
 * and cairo-ft to create a Cairo font face. Results are cached.
 *
 * Parameters:
 *   family - font family name (e.g., "Sans", "Monospace", "Serif")
 *   weight - FC_WEIGHT_NORMAL, FC_WEIGHT_BOLD, etc.
 *   slant  - FC_SLANT_ROMAN, FC_SLANT_ITALIC, etc.
 *
 * Returns a cairo_font_face_t* (cached, do NOT destroy).
 */
cairo_font_face_t *
_ISWResolveFontFace(const char *family, int weight, int slant)
{
    char key[256];
    _ISWFontCacheEntry *entry;
    FcPattern *pattern = NULL, *match = NULL;
    FcResult result;
    FcChar8 *font_file = NULL;
    FT_Face ft_face = NULL;

    snprintf(key, sizeof(key), "%s:%d:%d", family ? family : "Sans",
             weight, slant);

    /* Check cache */
    for (entry = _font_cache; entry; entry = entry->next) {
        if (strcmp(entry->pattern_key, key) == 0)
            return entry->cr_face;
    }

    _ISWInitFreeType();

    /* Use fontconfig to find a matching font file */
    pattern = FcPatternCreate();
    FcPatternAddString(pattern, FC_FAMILY,
                       (const FcChar8 *)(family ? family : "Sans"));
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcPatternAddInteger(pattern, FC_SLANT, slant);
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

    /* Load with FreeType */
    if (FT_New_Face(_ft_library, (const char *)font_file, 0, &ft_face) != 0) {
        FcPatternDestroy(match);
        FcPatternDestroy(pattern);
        return NULL;
    }

    /* Create Cairo font face */
    cairo_font_face_t *cr_face =
        cairo_ft_font_face_create_for_ft_face(ft_face, 0);

    /* Cache it */
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

/*
 * _ISWSetCairoFontFromXFont - Configure a Cairo context with a proper
 * TTF font face resolved via fontconfig, sized from XFontStruct metrics.
 */
void
_ISWSetCairoFontFromXFont(cairo_t *cr, XFontStruct *font, double scale)
{
    cairo_font_face_t *face;
    double size;

    const char *family = (font && font->font_family) ? font->font_family : "Sans";
    face = _ISWResolveFontFace(family, FC_WEIGHT_NORMAL, FC_SLANT_ROMAN);
    if (face)
        cairo_set_font_face(cr, face);

    if (font)
        size = (font->ascent + font->descent) * scale;
    else
        size = 12.0 * scale;

    cairo_set_font_size(cr, size);
}

/*
 * Persistent measurement context — avoids creating/destroying a Cairo
 * surface + context on every text measurement or font extents query.
 * Lazily created on first use, lives for the process lifetime.
 */
static cairo_surface_t *_measure_surf = NULL;
static cairo_t *_measure_cr = NULL;

/* Cached font extents — re-queried only when font size changes. */
static double _cached_font_size = -1.0;
static cairo_font_extents_t _cached_font_extents;

static cairo_t *
_ISWGetMeasureCR(void)
{
    if (!_measure_cr) {
        cairo_font_face_t *face;

        _measure_surf = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
        _measure_cr = cairo_create(_measure_surf);

        face = _ISWResolveFontFace("Sans", FC_WEIGHT_NORMAL, FC_SLANT_ROMAN);
        if (face)
            cairo_set_font_face(_measure_cr, face);
        else
            cairo_select_font_face(_measure_cr, "Sans",
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_NORMAL);
    }
    return _measure_cr;
}

/* Compute the scaled font size from a font + widget scale factor */
static double
_ISWComputeFontSize(Widget widget, XFontStruct *font)
{
    double scale = ISWScaleFactor(widget);
    if (font)
        return (font->ascent + font->descent) * scale;
    return 12.0 * scale;
}

/*
 * Get cached Cairo font extents. Only re-queries Cairo when the
 * effective font size changes (different font or different scale).
 */
static void
_ISWGetCairoFontExtents(Widget widget, XFontStruct *font, cairo_font_extents_t *extents)
{
    double size = _ISWComputeFontSize(widget, font);
    cairo_t *cr = _ISWGetMeasureCR();

    if (size != _cached_font_size) {
        cairo_set_font_size(cr, size);
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    }
    *extents = _cached_font_extents;
}

/*
 * Measure text using the persistent Cairo context with the same font face
 * and size that the render path uses. This ensures layout matches rendering.
 */
int
ISWScaledTextWidth(Widget widget, XFontStruct *font, const char *text, int len)
{
    cairo_text_extents_t extents;
    char *null_term;
    double size;
    int width;
    cairo_t *cr;

    if (!text || len <= 0)
        return 0;

    size = _ISWComputeFontSize(widget, font);
    cr = _ISWGetMeasureCR();

    if (size != _cached_font_size) {
        cairo_set_font_size(cr, size);
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    }

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

int
ISWScaledFontHeight(Widget widget, XFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    return (int)ceil(extents.ascent + extents.descent);
}

int
ISWScaledFontAscent(Widget widget, XFontStruct *font)
{
    cairo_font_extents_t extents;
    _ISWGetCairoFontExtents(widget, font, &extents);
    return (int)ceil(extents.ascent);
}

int
ISWScaledFontCapHeight(Widget widget, XFontStruct *font)
{
    double size = _ISWComputeFontSize(widget, font);
    cairo_t *cr = _ISWGetMeasureCR();
    cairo_text_extents_t text_ext;

    if (size != _cached_font_size) {
        cairo_set_font_size(cr, size);
        cairo_font_extents(cr, &_cached_font_extents);
        _cached_font_size = size;
    }

    cairo_text_extents(cr, "X", &text_ext);
    return (int)ceil(-text_ext.y_bearing);
}

/* Cairo is now a mandatory dependency — no non-Cairo fallback needed */
