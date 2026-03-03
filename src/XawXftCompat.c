/*
 * XawXftCompat.c - Xft compatibility layer implementation
 *
 * This file implements the Xft compatibility layer for Xaw3d,
 * replacing XFontSet with modern Xft font rendering.
 *
 * Implementation Status: FOUNDATION PHASE
 *   - Skeleton functions with stubs
 *   - TODO: Implement actual font loading and rendering
 *   - TODO: Implement character encoding conversion
 *
 * Copyright (c) 2026 Xaw3d Project
 * Licensed under the same terms as the rest of Xaw3d
 */

#include "XawXftCompat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>

/*
 * ==================================================================
 * FONT MANAGEMENT FUNCTIONS
 * ==================================================================
 */

/*
 * InitializeFontconfig - Ensure fontconfig is initialized
 *
 * Fontconfig requires FcInit() to be called before any other operations.
 * This function ensures it's initialized exactly once.
 */
static void
InitializeFontconfig(void)
{
    static int initialized = 0;
    
    if (!initialized) {
        if (!FcInit()) {
            fprintf(stderr, "InitializeFontconfig: FcInit() failed\n");
        }
        initialized = 1;
    }
}

/*
 * ParseFontName - Parse XLFD or simple font name into Xft pattern
 *
 * Handles three types of font specifications:
 *   1. XLFD format: "-foundry-family-weight-slant-..."
 *   2. Xft patterns: "Sans-12", "mono:size=10"
 *   3. Simple names: "fixed", "helvetica"
 *
 * Returns: FcPattern ready for matching, or NULL on failure
 */
static FcPattern* 
ParseFontName(const char *fontname)
{
    FcPattern *pattern;
    
    if (!fontname || !*fontname) {
        /* Default to fixed font */
        return FcNameParse((const FcChar8 *)"mono:size=12");
    }
    
    /* Check if it's an XLFD (starts with -) */
    if (fontname[0] == '-') {
        /* Parse XLFD format using XftXlfdParse */
        pattern = XftXlfdParse(fontname, True, True);
        if (pattern) {
            return pattern;
        }
        
        /* FIXME: XftXlfdParse may fail on complex XLFD patterns
         * Fall back to simple name parse if XLFD parse fails */
        fprintf(stderr, "ParseFontName: XLFD parse failed for '%s', trying fallback\n", 
                fontname);
    }
    
    /* Try as Xft pattern name (e.g., "Sans-12", "mono:size=10") */
    pattern = FcNameParse((const FcChar8 *)fontname);
    if (pattern) {
        return pattern;
    }
    
    /* Final fallback to mono font */
    fprintf(stderr, "ParseFontName: All parsing failed for '%s', using mono:size=12\n",
            fontname);
    return FcNameParse((const FcChar8 *)"mono:size=12");
}

/*
 * XawCreateFontSet - Create XawFontSet from font specification
 *
 * Phase 3.2 Implementation:
 *   - Parse XLFD vs Xft pattern
 *   - Load XftFont with pattern matching
 *   - Cache font metrics (ascent, descent, height, max_advance)
 *   - Handle error cases and fallback fonts
 */
XawFontSet* 
XawCreateFontSet(Display *dpy, const char *fontname)
{
    XawFontSet *fs;
    FcPattern *pattern, *match;
    FcResult result;
    XftFont *xft_font;
    
    if (!dpy) {
        fprintf(stderr, "XawCreateFontSet: NULL display\n");
        return NULL;
    }
    
    /* Ensure Fontconfig is initialized */
    InitializeFontconfig();
    
    /* Allocate structure */
    fs = (XawFontSet *) malloc(sizeof(XawFontSet));
    if (!fs) {
        fprintf(stderr, "XawCreateFontSet: malloc failed\n");
        return NULL;
    }
    
    /* Initialize structure */
    memset(fs, 0, sizeof(XawFontSet));
    
    /* Parse font name into pattern */
    pattern = ParseFontName(fontname);
    if (!pattern) {
        free(fs);
        fprintf(stderr, "XawCreateFontSet: ParseFontName failed\n");
        return NULL;
    }
    
    /* Configure pattern for matching */
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    
    /* Find best matching font */
    match = FcFontMatch(NULL, pattern, &result);
    FcPatternDestroy(pattern);
    
    if (!match) {
        free(fs);
        fprintf(stderr, "XawCreateFontSet: FcFontMatch failed\n");
        return NULL;
    }
    
    /* Open the Xft font */
    xft_font = XftFontOpenPattern(dpy, match);
    if (!xft_font) {
        /* Try fallback fonts if pattern match failed */
        fprintf(stderr, "XawCreateFontSet: XftFontOpenPattern failed, trying fallback\n");
        xft_font = XftFontOpen(dpy, DefaultScreen(dpy),
                               XFT_FAMILY, XftTypeString, "mono",
                               XFT_SIZE, XftTypeDouble, 12.0,
                               NULL);
    }
    
    if (!xft_font) {
        free(fs);
        fprintf(stderr, "XawCreateFontSet: All font loading failed\n");
        return NULL;
    }
    
    /* Store display connection */
    fs->dpy = dpy;
    
    /* Store font and extract metrics */
    fs->xft_font = xft_font;
    fs->ascent = xft_font->ascent;
    fs->descent = xft_font->descent;
    fs->height = xft_font->height;
    fs->max_advance = xft_font->max_advance_width;
    
    /* Store font name for debugging */
    fs->font_name = strdup(fontname ? fontname : "default");
    
    return fs;
}

/*
 * XawFreeFontSet - Free XawFontSet and associated resources
 */
void 
XawFreeFontSet(Display *dpy, XawFontSet *fs)
{
    if (!fs)
        return;
    
    /* Free Xft font if loaded */
    if (fs->xft_font && dpy) {
        XftFontClose(dpy, fs->xft_font);
    }
    
    /* Free font name */
    if (fs->font_name) {
        free(fs->font_name);
    }
    
    /* Free structure */
    free(fs);
}

/*
 * ==================================================================
 * TEXT MEASUREMENT FUNCTIONS
 * ==================================================================
 */

/*
 * XawTextWidth - Calculate text width
 *
 * Phase 3.2 Implementation:
 *   - Use XftTextExtentsUtf8 to measure text
 *   - Return xglyphinfo.xOff for logical width (advance)
 *   - Handle empty strings and NULL fontset
 */
int 
XawTextWidth(XawFontSet *fs, const char *text, int len)
{
    XGlyphInfo extents;
    
    if (!fs || !fs->xft_font || !fs->dpy || !text || len <= 0)
        return 0;
    
    /* Measure text using Xft */
    XftTextExtentsUtf8(fs->dpy, fs->xft_font, 
                       (const FcChar8 *)text, len, &extents);
    
    /* Return advance width (xOff) for proper text layout */
    return extents.xOff;
}

/*
 * XawTextExtents - Calculate text bounding boxes
 *
 * Phase 3.2 Implementation:
 *   - Use XftTextExtentsUtf8 to get XGlyphInfo
 *   - Convert XGlyphInfo to XRectangle for ink/logical extents
 *   - Match XFontSet behavior for negative y values
 *
 * XGlyphInfo fields:
 *   width, height: ink bounding box dimensions
 *   x, y: ink bounding box offset from drawing origin
 *   xOff, yOff: cursor advance (logical width)
 */
int 
XawTextExtents(XawFontSet *fs, const char *text, int len,
               XRectangle *overall_ink, XRectangle *overall_logical)
{
    XGlyphInfo extents;
    
    if (!fs || !fs->xft_font || !fs->dpy || !text || len <= 0) {
        if (overall_ink) {
            overall_ink->x = overall_ink->y = 0;
            overall_ink->width = overall_ink->height = 0;
        }
        if (overall_logical) {
            overall_logical->x = overall_logical->y = 0;
            overall_logical->width = overall_logical->height = 0;
        }
        return 0;
    }
    
    /* Measure text using Xft */
    XftTextExtentsUtf8(fs->dpy, fs->xft_font,
                       (const FcChar8 *)text, len, &extents);
    
    if (overall_ink) {
        /* Ink extent: actual pixels drawn
         * XGlyphInfo uses drawing origin as reference
         * XFontSet uses top-left, so we negate extents.x and extents.y */
        overall_ink->x = -extents.x;
        overall_ink->y = -extents.y;
        overall_ink->width = extents.width;
        overall_ink->height = extents.height;
    }
    
    if (overall_logical) {
        /* Logical extent: layout spacing
         * Y position relative to baseline (negative = above baseline) */
        overall_logical->x = 0;
        overall_logical->y = -fs->ascent;
        overall_logical->width = extents.xOff;  /* Advance width */
        overall_logical->height = fs->height;   /* Total font height */
    }
    
    /* Return advance width */
    return extents.xOff;
}

/*
 * ==================================================================
 * TEXT RENDERING FUNCTIONS (Phase 3.3/3.4)
 * ==================================================================
 */

/* Phase 3.3: XftDraw caching */

/* Simple cache: store last used drawable and its XftDraw */
static Drawable cached_drawable = None;
static XftDraw *cached_xft_draw = NULL;
static Display *cached_display = NULL;

/* Helper: Get or create XftDraw for a drawable */
static XftDraw*
GetXftDraw(Display *dpy, Drawable d)
{
    Visual *visual;
    Colormap colormap;
    
    /* Return cached if same drawable */
    if (cached_xft_draw && cached_drawable == d && cached_display == dpy) {
        return cached_xft_draw;
    }
    
    /* Destroy old cached XftDraw */
    if (cached_xft_draw) {
        XftDrawDestroy(cached_xft_draw);
        cached_xft_draw = NULL;
        cached_drawable = None;
    }
    
    /* Get drawable visual info */
    visual = DefaultVisual(dpy, DefaultScreen(dpy));
    colormap = DefaultColormap(dpy, DefaultScreen(dpy));
    
    /* Create new XftDraw */
    cached_xft_draw = XftDrawCreate(dpy, d, visual, colormap);
    if (cached_xft_draw) {
        cached_drawable = d;
        cached_display = dpy;
    }
    
    return cached_xft_draw;
}

/* Phase 3.3: GC to XftColor conversion */

/* Helper: Convert GC foreground to XftColor (internal use only) */
static int
GCFgToXftColor(Display *dpy, GC gc, XftColor *xft_color)
{
    XGCValues gc_values;
    XColor xcolor;
    
    /* Get GC foreground pixel value */
    if (!XGetGCValues(dpy, gc, GCForeground, &gc_values)) {
        /* Fallback to black */
        xft_color->color.red = 0;
        xft_color->color.green = 0;
        xft_color->color.blue = 0;
        xft_color->color.alpha = 0xffff;
        xft_color->pixel = 0;
        return 0;
    }
    
    /* Query RGB values for the pixel */
    xcolor.pixel = gc_values.foreground;
    if (XQueryColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &xcolor)) {
        xft_color->color.red = xcolor.red;
        xft_color->color.green = xcolor.green;
        xft_color->color.blue = xcolor.blue;
        xft_color->color.alpha = 0xffff;  /* Opaque */
        xft_color->pixel = xcolor.pixel;
        return 1;
    }
    
    /* Fallback */
    xft_color->color.red = 0;
    xft_color->color.green = 0;
    xft_color->color.blue = 0;
    xft_color->color.alpha = 0xffff;
    xft_color->pixel = gc_values.foreground;
    return 0;
}

/* Helper: Convert GC background to XftColor (internal use only) */
static int
GCBgToXftColor(Display *dpy, GC gc, XftColor *xft_color)
{
    XGCValues gc_values;
    XColor xcolor;
    
    /* Get GC background pixel value */
    if (!XGetGCValues(dpy, gc, GCBackground, &gc_values)) {
        /* Fallback to white */
        xft_color->color.red = 0xffff;
        xft_color->color.green = 0xffff;
        xft_color->color.blue = 0xffff;
        xft_color->color.alpha = 0xffff;
        xft_color->pixel = 1;
        return 0;
    }
    
    /* Query RGB values for the pixel */
    xcolor.pixel = gc_values.background;
    if (XQueryColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &xcolor)) {
        xft_color->color.red = xcolor.red;
        xft_color->color.green = xcolor.green;
        xft_color->color.blue = xcolor.blue;
        xft_color->color.alpha = 0xffff;  /* Opaque */
        xft_color->pixel = xcolor.pixel;
        return 1;
    }
    
    /* Fallback */
    xft_color->color.red = 0xffff;
    xft_color->color.green = 0xffff;
    xft_color->color.blue = 0xffff;
    xft_color->color.alpha = 0xffff;
    xft_color->pixel = gc_values.background;
    return 0;
}

/* Phase 3.4: Text rendering */

/*
 * XawDrawString - Draw text with transparent background
 *
 * Replaces: XmbDrawString / XwcDrawString
 *
 * Implementation:
 *   - Get/create cached XftDraw for drawable
 *   - Convert GC foreground to XftColor
 *   - Draw text using XftDrawStringUtf8
 *   - No background fill (transparent)
 */
void
XawDrawString(Display *dpy, Drawable d, XawFontSet *fs, GC gc,
              int x, int y, const char *text, int len)
{
    XftDraw *xft_draw;
    XftColor xft_color;
    
    if (!fs || !fs->xft_font || !text || len <= 0)
        return;
    
    /* Get XftDraw for this drawable */
    xft_draw = GetXftDraw(dpy, d);
    if (!xft_draw)
        return;
    
    /* Convert GC foreground to XftColor */
    if (!GCFgToXftColor(dpy, gc, &xft_color))
        return;
    
    /* Draw the text
     * Note: XFontSet coordinate system has y at baseline
     * Xft also uses baseline, so no adjustment needed
     */
    XftDrawStringUtf8(xft_draw, &xft_color, fs->xft_font,
                      x, y, (const FcChar8 *)text, len);
}

/*
 * XawDrawImageString - Draw text with background fill
 *
 * Replaces: XmbDrawImageString / XwcDrawImageString
 *
 * Implementation:
 *   - Get/create cached XftDraw for drawable
 *   - Convert GC foreground and background to XftColors
 *   - Measure text to get background rectangle size
 *   - Draw background rectangle
 *   - Draw text on top
 */
void
XawDrawImageString(Display *dpy, Drawable d, XawFontSet *fs, GC gc,
                   int x, int y, const char *text, int len)
{
    XftDraw *xft_draw;
    XftColor xft_fg, xft_bg;
    XGlyphInfo extents;
    
    if (!fs || !fs->xft_font || !text || len <= 0)
        return;
    
    /* Get XftDraw for this drawable */
    xft_draw = GetXftDraw(dpy, d);
    if (!xft_draw)
        return;
    
    /* Convert GC foreground to XftColor */
    if (!GCFgToXftColor(dpy, gc, &xft_fg))
        return;
    
    /* Convert GC background to XftColor */
    if (!GCBgToXftColor(dpy, gc, &xft_bg))
        return;
    
    /* Measure text to get background rectangle size */
    XftTextExtentsUtf8(dpy, fs->xft_font,
                       (const FcChar8 *)text, len, &extents);
    
    /* Draw background rectangle
     * y coordinate is at baseline, so background starts at (y - ascent)
     */
    XftDrawRect(xft_draw, &xft_bg,
                x, y - fs->ascent,
                extents.xOff, fs->height);
    
    /* Draw text on top */
    XftDrawStringUtf8(xft_draw, &xft_fg, fs->xft_font,
                      x, y, (const FcChar8 *)text, len);
}

/*
 * ==================================================================
 * CHARACTER ENCODING CONVERSION (Phase 3.4)
 * ==================================================================
 */

/*
 * XawMbToUtf8 - Convert multi-byte to UTF-8
 *
 * Phase 3.4 Implementation:
 *   - Allocates and returns UTF-8 string
 *   - Currently assumes ISO-8859-1/ASCII input (direct copy)
 *   - TODO: Proper locale-based conversion using iconv
 *
 * Caller must free() returned string.
 */
char*
XawMbToUtf8(const char *mb_text, int mb_len, int *utf8_len)
{
    char *utf8_text;
    
    /* For now, assume ISO-8859-1/ASCII input = direct copy
     * TODO: Proper locale-based conversion using iconv or similar
     */
    utf8_text = malloc(mb_len + 1);
    if (!utf8_text) {
        if (utf8_len) *utf8_len = 0;
        return NULL;
    }
    
    memcpy(utf8_text, mb_text, mb_len);
    utf8_text[mb_len] = '\0';
    
    if (utf8_len) *utf8_len = mb_len;
    return utf8_text;
}

/*
 * XawWcToUtf8 - Convert wide-char string to UTF-8
 *
 * Phase 3.4 Implementation:
 *   - Uses wcstombs for conversion
 *   - Allocates and returns UTF-8 string
 *   - Handles conversion errors
 *
 * Caller must free() returned string.
 */
char*
XawWcToUtf8(const wchar_t *wc_text, int wc_len, int *utf8_len)
{
    size_t result;
    char *utf8_text;
    size_t max_size = wc_len * 4 + 1;  /* Max UTF-8 bytes per wchar */
    
    utf8_text = malloc(max_size);
    if (!utf8_text) {
        if (utf8_len) *utf8_len = 0;
        return NULL;
    }
    
    /* Use wcstombs for conversion */
    result = wcstombs(utf8_text, wc_text, max_size - 1);
    if (result == (size_t)-1) {
        free(utf8_text);
        if (utf8_len) *utf8_len = 0;
        return NULL;
    }
    
    utf8_text[result] = '\0';
    if (utf8_len) *utf8_len = result;
    return utf8_text;
}

/*
 * ==================================================================
 * FONT METRICS ACCESS
 * ==================================================================
 */

int 
XawFontSetAscent(XawFontSet *fs)
{
    return fs ? fs->ascent : 0;
}

int 
XawFontSetDescent(XawFontSet *fs)
{
    return fs ? fs->descent : 0;
}

int 
XawFontSetHeight(XawFontSet *fs)
{
    return fs ? fs->height : 0;
}

int 
XawFontSetMaxWidth(XawFontSet *fs)
{
    return fs ? fs->max_advance : 0;
}

/*
 * ==================================================================
 * XftDraw CONTEXT MANAGEMENT
 * ==================================================================
 */

/*
 * XawCreateXftDraw - Create XftDraw context
 *
 * TODO Phase 4:
 *   - Wrapper around XftDrawCreate
 *   - Add error handling
 */
XftDraw* 
XawCreateXftDraw(Display *dpy, Drawable d, Visual *visual, Colormap cmap)
{
    if (!dpy || !d || !visual)
        return NULL;
    
    return XftDrawCreate(dpy, d, visual, cmap);
}

/*
 * XawDestroyXftDraw - Destroy XftDraw context
 */
void 
XawDestroyXftDraw(XftDraw *draw)
{
    if (draw)
        XftDrawDestroy(draw);
}

/*
 * XawGCToXftColor - Convert GC foreground to XftColor (Public API)
 *
 * Phase 3.3 Implementation:
 *   - Extract GC foreground pixel value
 *   - Query colormap to get RGB values
 *   - Convert to XftColor structure
 *
 * This is the public API version that takes explicit Visual and Colormap.
 * For internal use, the simpler GCFgToXftColor helper is preferred.
 */
Bool 
XawGCToXftColor(Display *dpy, Visual *visual, Colormap cmap,
                GC gc, XftColor *xft_color)
{
    XGCValues gc_values;
    XColor xcolor;
    
    if (!dpy || !gc || !xft_color)
        return False;
    
    /* Get GC foreground pixel value */
    if (!XGetGCValues(dpy, gc, GCForeground, &gc_values)) {
        /* Fallback to black */
        xft_color->color.red = 0;
        xft_color->color.green = 0;
        xft_color->color.blue = 0;
        xft_color->color.alpha = 0xffff;
        xft_color->pixel = 0;
        return False;
    }
    
    /* Query RGB values for the pixel */
    xcolor.pixel = gc_values.foreground;
    if (XQueryColor(dpy, cmap, &xcolor)) {
        xft_color->color.red = xcolor.red;
        xft_color->color.green = xcolor.green;
        xft_color->color.blue = xcolor.blue;
        xft_color->color.alpha = 0xffff;  /* Opaque */
        xft_color->pixel = xcolor.pixel;
        return True;
    }
    
    /* Fallback */
    xft_color->color.red = 0;
    xft_color->color.green = 0;
    xft_color->color.blue = 0;
    xft_color->color.alpha = 0xffff;
    xft_color->pixel = gc_values.foreground;
    return False;
}
