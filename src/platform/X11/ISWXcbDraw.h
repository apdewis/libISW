/*
 * IswXcbDraw.h - XCB drawing compatibility functions for Isw3d
 * 
 * Provides XCB-native implementations of drawing, font, and xcb_gcontext_t operations
 * that were previously handled by Xlib. This is part of the complete
 * Xlib-to-XCB migration.
 *
 * Copyright (c) 2026 Isw3d Project
 */

#ifndef _IswXcbDraw_h
#define _IswXcbDraw_h

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <ISW/Intrinsic.h>
#include <ISW/ISWPlatform.h>
#include <ISW/ISWP.h>

/*
 * =================================================================
 * SCREEN DIMENSION MACROS (Xlib compatibility)
 * =================================================================
 *
 * These macros provide Xlib-compatible screen dimension access.
 * In XCB, xcb_screen_t has width_in_pixels and height_in_pixels.
 */
#ifndef WidthOfScreen
#define WidthOfScreen(s)  ((s)->width_in_pixels)
#endif

#ifndef HeightOfScreen
#define HeightOfScreen(s) ((s)->height_in_pixels)
#endif

#ifndef _IswJustify_defined
#define _IswJustify_defined
typedef enum {
    IswJustifyLeft,
    IswJustifyCenter,
    IswJustifyRight
} IswJustify;
#endif

#ifndef _IswGravity_defined
#define _IswGravity_defined
typedef unsigned int IswGravity;
#endif

/*
 * =================================================================
 * BITMAP AND PIXMAP CREATION
 * =================================================================
 */

/*
 * IswCreateBitmapFromData - Create a pixmap from static bitmap data
 *
 * XCB replacement for XCreateBitmapFromData
 *
 * Parameters:
 *   conn     - XCB connection
 *   drawable - Drawable to determine depth (root window usually)
 *   data     - Pointer to bitmap data (XCB_IMAGE_ORDER_LSB_FIRST bit order)
 *   width    - Width of bitmap
 *   height   - Height of bitmap
 *
 * Returns: xcb_pixmap_t (0 on failure)
 *
 * Note: Creates a depth-1 pixmap (bitmap) suitable for use as
 * cursor shapes, stipple patterns, or clip masks.
 */
xcb_pixmap_t IswCreateBitmapFromData(xcb_connection_t *conn,
                                     xcb_drawable_t drawable,
                                     const char *data,
                                     unsigned int width,
                                     unsigned int height);

/*
 * ISWFreePixmap - Free a pixmap created by IswCreateBitmapFromData
 *
 * XCB replacement for XFreePixmap
 *
 * Parameters:
 *   conn   - XCB connection
 *   pixmap - Pixmap to free
 */
void ISWFreePixmap(xcb_connection_t *conn, xcb_pixmap_t pixmap);

/*
 * IswCreatePixmapFromBitmapData - Create a colored pixmap from bitmap data
 *
 * XCB replacement for XCreatePixmapFromBitmapData
 *
 * Parameters:
 *   conn     - XCB connection
 *   drawable - Drawable to use for screen reference  
 *   data     - Pointer to bitmap data (XCB_IMAGE_ORDER_LSB_FIRST bit order)
 *   width    - Width of bitmap
 *   height   - Height of bitmap
 *   fg       - Foreground pixel value
 *   bg       - Background pixel value
 *   depth    - Depth of pixmap to create
 *
 * Returns: xcb_pixmap_t (0 on failure)
 *
 * Note: Creates a pixmap at the specified depth with bitmap data
 * rendered using foreground and background colors.
 */
xcb_pixmap_t IswCreatePixmapFromBitmapData(xcb_connection_t *conn,
                                           xcb_drawable_t drawable,
                                           const char *data,
                                           unsigned int width,
                                           unsigned int height,
                                           unsigned long fg,
                                           unsigned long bg,
                                           unsigned int depth);

/*
 * ISWQueryColor - Query RGB values for a pixel
 *
 * XCB replacement for XQueryColor. Fills in the RGB values
 * for the pixel value in color->pixel.
 *
 * Parameters:
 *   conn  - XCB connection
 *   cmap  - Colormap to query
 *   color - IswColor structure with pixel set, RGB values filled in
 *
 * Returns: 1 on success, 0 on failure
 */
int ISWQueryColor(xcb_connection_t *conn, xcb_colormap_t cmap, IswColor *color);



/*
 * ISWFontStructTextWidth - Calculate text width using IswFontStruct
 *
 * Replacement for XTextWidth when using IswFontStruct (legacy fonts).
 *
 * Parameters:
 *   font - IswFontStruct pointer
 *   text - Text string
 *   len  - Length of text
 *
 * Returns: Width in pixels
 */
int ISWFontStructTextWidth(IswFontStruct *font, const char *text, int len);

#define XTextWidth(font, text, len) ISWFontStructTextWidth((IswFontStruct*)(font), (text), (len))

/* XTextWidth16 stub - XCB doesn't support 16-bit text well, returns estimated width */
#define XTextWidth16(font, text, len) \
    (XTextWidth((font), (const char*)(text), (len) * 2))

/*
 * =================================================================
 * xcb_gcontext_t VALUE HELPERS
 * =================================================================
 *
 * XCB uses xcb_create_gc_value_list_t instead of XGCValues.
 * These helpers make it easier to set up xcb_gcontext_t values.
 */

/*
 * IswGCValueMask - Mask values for xcb_gcontext_t creation
 * These map to XCB_GC_* constants
 */
#define XAW_GC_FUNCTION           XCB_GC_FUNCTION
#define XAW_GC_PLANE_MASK         XCB_GC_PLANE_MASK
#define XAW_GC_FOREGROUND         XCB_GC_FOREGROUND
#define XAW_GC_BACKGROUND         XCB_GC_BACKGROUND
#define XAW_GC_LINE_WIDTH         XCB_GC_LINE_WIDTH
#define XAW_GC_LINE_STYLE         XCB_GC_LINE_STYLE
#define XAW_GC_CAP_STYLE          XCB_GC_CAP_STYLE
#define XAW_GC_JOIN_STYLE         XCB_GC_JOIN_STYLE
#define XAW_GC_FILL_STYLE         XCB_GC_FILL_STYLE
#define XAW_GC_FILL_RULE          XCB_GC_FILL_RULE
#define XAW_GC_TILE               XCB_GC_TILE
#define XAW_GC_STIPPLE            XCB_GC_STIPPLE
#define XAW_GC_TILE_STIPPLE_X     XCB_GC_TILE_STIPPLE_ORIGIN_X
#define XAW_GC_TILE_STIPPLE_Y     XCB_GC_TILE_STIPPLE_ORIGIN_Y
#define XAW_GC_FONT               XCB_GC_FONT
#define XAW_GC_SUBWINDOW_MODE     XCB_GC_SUBWINDOW_MODE
#define XAW_GC_GRAPHICS_EXPOSURES XCB_GC_GRAPHICS_EXPOSURES
#define XAW_GC_CLIP_X             XCB_GC_CLIP_ORIGIN_X
#define XAW_GC_CLIP_Y             XCB_GC_CLIP_ORIGIN_Y
#define XAW_GC_CLIP_MASK          XCB_GC_CLIP_MASK
#define XAW_GC_DASH_OFFSET        XCB_GC_DASH_OFFSET
#define XAW_GC_DASH_LIST          XCB_GC_DASH_LIST
#define XAW_GC_ARC_MODE           XCB_GC_ARC_MODE

/* XCB_GX_XOR for function value (use XCB_GX_* constants directly) */

/*
 * ISWInitGCValues - Initialize an xcb_create_gc_value_list_t structure
 *
 * Parameters:
 *   values - Pointer to value list to initialize
 *
 * Sets all fields to safe defaults.
 */
void ISWInitGCValues(xcb_create_gc_value_list_t *values);

/*
 * ISWSetGCFont - Set font in xcb_gcontext_t value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   font   - XCB font ID
 */
void ISWSetGCFont(xcb_create_gc_value_list_t *values, xcb_font_t font);

/*
 * ISWSetGCForeground - Set foreground in xcb_gcontext_t value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   pixel  - Foreground pixel value
 */
void ISWSetGCForeground(xcb_create_gc_value_list_t *values, uint32_t pixel);

/*
 * ISWSetGCBackground - Set background in xcb_gcontext_t value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   pixel  - Background pixel value
 */
void ISWSetGCBackground(xcb_create_gc_value_list_t *values, uint32_t pixel);

/*
 * ISWSetGCGraphicsExposures - Set graphics_exposures in xcb_gcontext_t value list
 *
 * Parameters:
 *   values   - Pointer to value list
 *   exposures - Boolean (0 = off, 1 = on)
 */
void ISWSetGCGraphicsExposures(xcb_create_gc_value_list_t *values, int exposures);

/*
 * ISWSetGCFunction - Set function in xcb_gcontext_t value list
 *
 * Parameters:
 *   values   - Pointer to value list
 *   function - xcb_gcontext_t function (e.g., XCB_GX_XOR)
 */
void ISWSetGCFunction(xcb_create_gc_value_list_t *values, uint32_t function);

/*
 * =================================================================
 * FONT METRICS (IswFontStruct compatibility)
 * =================================================================
 *
 * The XCB-based IswFontStruct lacks per-character metrics.
 * These functions provide alternatives using XCB font queries.
 */

/*
 * ISWFontTextWidth - Calculate text width using XCB font queries
 *
 * Replacement for XTextWidth - queries server for text extent
 *
 * Parameters:
 *   conn - XCB connection
 *   font - XCB font ID (from IswFontStruct->fid)
 *   text - Text string
 *   len  - Length of text in bytes
 *
 * Returns: Width in pixels
 */
int ISWFontTextWidth(xcb_connection_t *conn, xcb_font_t font,
                     const char *text, int len);

/*
 * ISWFontCharWidth - Get width of a single character
 *
 * Replacement for accessing font->per_char[c].width
 *
 * Parameters:
 *   conn - XCB connection
 *   font - XCB font ID
 *   c    - Character to measure
 *
 * Returns: Width in pixels
 */
int ISWFontCharWidth(xcb_connection_t *conn, xcb_font_t font, unsigned char c);

/*
 * IswGetFontProperty - Get a font property by atom
 *
 * Replacement for XGetFontProperty
 *
 * Parameters:
 *   conn   - XCB connection
 *   font   - IswFontStruct pointer (for font ID)
 *   atom   - Atom to query
 *   value  - Output value
 *
 * Returns: True if property found, False otherwise
 */
Bool IswGetFontProperty(xcb_connection_t *conn, IswFontStruct *font,
                        xcb_atom_t atom, unsigned long *value);

/*
 * =================================================================
 * TEXT DRAWING (Server fonts - for compatibility)
 * =================================================================
 *
 * Note: For modern text rendering, use the Xft layer in IswXftCompat.h
 * These functions provide XCB-based server font drawing for legacy code.
 */

/*
 * ISWXcbDrawImageString - Draw text with background using XCB
 *
 * Replacement for XDrawImageString
 *
 * Parameters:
 *   conn - XCB connection
 *   d    - Drawable (window or pixmap)
 *   gc   - Graphics context (must have font set)
 *   x, y - Text position (baseline)
 *   text - Text string
 *   len  - Length of text
 */
void ISWXcbDrawImageString(xcb_connection_t *conn, xcb_drawable_t d,
                           xcb_gcontext_t gc, int x, int y,
                           const char *text, int len);

/*
 * ISWXcbDrawString - Draw text without background using XCB
 *
 * Replacement for XDrawString
 *
 * Parameters:
 *   conn - XCB connection
 *   d    - Drawable (window or pixmap)
 *   gc   - Graphics context (must have font set)
 *   x, y - Text position (baseline)
 *   text - Text string
 *   len  - Length of text
 */
void ISWXcbDrawString(xcb_connection_t *conn, xcb_drawable_t d,
                      xcb_gcontext_t gc, int x, int y,
                      const char *text, int len);

/*
 * =================================================================
 * XCB CORE FONT HELPERS (Phase 2: Non-Xft text rendering)
 * =================================================================
 */

/*
 * ISWXcbTextWidth - Calculate text width using xcb_query_text_extents
 *
 * Replacement for XTextWidth - queries server for text extent
 *
 * Parameters:
 *   conn - XCB connection
 *   font - XCB font ID (from IswFontStruct->fid)
 *   text - Text string
 *   len  - Length of text in bytes
 *
 * Returns: Width in pixels
 */
int ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len);

/*
 * ISWFontMetrics - Font metrics structure
 */
typedef struct {
    int ascent;
    int descent;
    int max_char_width;
} ISWFontMetrics;

/*
 * ISWXcbQueryFontMetrics - Query font metrics using xcb_query_font
 *
 * Replacement for accessing IswFontStruct->max_bounds
 *
 * Parameters:
 *   conn    - XCB connection
 *   font    - XCB font ID
 *   metrics - Output metrics structure
 */
void ISWXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                            ISWFontMetrics *metrics);

/*
 * ISWReshapeWidget - Shape a widget using the X Shape extension
 *
 * Parameters:
 *   w           - Widget to reshape
 *   shape_style - Shape style (IswShapeRectangle, IswShapeOval, IswShapeEllipse, IswShapeRoundedRectangle)
 *   corner_width  - Corner width for rounded shapes
 *   corner_height - Corner height for rounded shapes
 *
 * Returns: True if successful, False otherwise
 */
Boolean ISWReshapeWidget(Widget w, int shape_style, int corner_width, int corner_height);

/*
 * =================================================================
 * STRING UTILITIES (libXmu replacements)
 * =================================================================
 */


/*
 * =================================================================
 * COLOR UTILITIES
 * =================================================================
 */

/*
 * ISWDistinguishablePixels - Check if pixels are visually distinguishable
 *
 * XCB replacement for color comparison utility.
 * Queries the RGB values of each pixel and determines if they are
 * sufficiently different to be visually distinguishable.
 *
 * Parameters:
 *   conn      - XCB connection (Display* in Xlib)
 *   colormap  - Colormap to query
 *   pixels    - Array of pixel values to compare
 *   count     - Number of pixels in array
 *
 * Returns:
 *   True if all pixels are distinguishable from each other
 *   False if any two pixels are too similar
 */
Boolean ISWDistinguishablePixels(
    xcb_connection_t *conn,
    xcb_colormap_t colormap,
    unsigned long *pixels,
    int count
);

/*
 * IswLocatePixmapFile - Locate and load a pixmap file
 *
 * This is a stub implementation that always returns None.
 * Full implementation would require XPM or other image format support.
 *
 * Parameters:
 *   screen     - Screen to create pixmap on
 *   name       - Name of pixmap file
 *   fore       - Foreground pixel
 *   back       - Background pixel
 *   depth      - Depth of pixmap
 *   srcname    - Return source name (if non-NULL)
 *   srcnamelen - Length of srcname buffer
 *   widthp     - Return width (if non-NULL)
 *   heightp    - Return height (if non-NULL)
 *   xhotp      - Return X hotspot (if non-NULL)
 *   yhotp      - Return Y hotspot (if non-NULL)
 *
 * Returns:
 *   Pixmap (currently always returns None)
 */
xcb_pixmap_t IswLocatePixmapFile(
    xcb_screen_t *screen,
    const char *name,
    unsigned long fore,
    unsigned long back,
    unsigned int depth,
    char *srcname,
    size_t srcnamelen,
    int *widthp,
    int *heightp,
    int *xhotp,
    int *yhotp
);

/*
 * =================================================================
 * XLIB COMPATIBILITY MACROS FOR TEXT DRAWING
 * =================================================================
 */

/* Map XDrawString to XCB version - simple text drawing */
#ifndef XDrawString
#define XDrawString(dpy, win, gc, x, y, str, len) \
    xcb_image_text_8((dpy), (len), (win), (gc), (x), (y), (str))
#endif

/* Map XDrawString16 to XCB version - 16-bit text drawing */
#ifndef XDrawString16
#define XDrawString16(dpy, win, gc, x, y, str, len) \
    xcb_image_text_16((dpy), (len), (win), (gc), (x), (y), (const xcb_char2b_t*)(str))
#endif

/*
 * =================================================================
 * XLIB COMPATIBILITY MACROS FOR WINDOW MANIPULATION
 * =================================================================
 */

/* Xlib-named window operation macros removed — use xcb_* calls directly. */

/*
 * =================================================================
 * FONT FALLBACK HANDLING
 * =================================================================
 */

/* Load a fallback font when resource converters fail */
IswFontStruct *ISWLoadFallbackFont(xcb_connection_t *conn);

/* Free a fallback font created by ISWLoadFallbackFont */
void ISWFreeFallbackFont(xcb_connection_t *conn, IswFontStruct *font);

#endif /* _IswXcbDraw_h */
