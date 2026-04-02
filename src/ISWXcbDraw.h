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
#include <X11/Intrinsic.h>

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

/*
 * =================================================================
 * WIDGET ORIENTATION TYPE (Missing from modified libXt)
 * =================================================================
 *
 * The XCB-based libXt doesn't define XtOrientation type, only the
 * string constants XtCOrientation, XtEvertical, XtEhorizontal.
 * We need to define the enumeration type for widget structures.
 */
#ifndef _IswXtOrientation_defined
#define _IswXtOrientation_defined
typedef enum {
    XtorientHorizontal = 0,
    XtorientVertical = 1
} XtOrientation;
#endif

/*
 * =================================================================
 * TEXT JUSTIFICATION TYPE (Missing from modified libXt)
 * =================================================================
 *
 * The XCB-based libXt doesn't define XtJustify type.
 * Used by Label, SmeBSB, and other text widgets.
 */
#ifndef _IswXtJustify_defined
#define _IswXtJustify_defined
typedef enum {
    XtJustifyLeft,
    XtJustifyCenter,
    XtJustifyRight
} XtJustify;
#endif

/*
 * =================================================================
 * WIDGET EDGE TYPE  
 * =================================================================
 *
 * IswEdgeType is defined in Form.h with IswChain* values.
 * Form.h also provides XtEdgeType as an alias via #define.
 * The converter uses IswEdgeType directly.
 */

/*
 * =================================================================
 * WIDGET GRAVITY TYPE (Missing from modified libXt)
 * =================================================================
 *
 * The XCB-based libXt doesn't define XtGravity type.
 * Used by Tree widget and other layout widgets.
 */
#ifndef _IswXtGravity_defined
#define _IswXtGravity_defined
typedef unsigned int XtGravity;
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
 *   color - XColor structure with pixel set, RGB values filled in
 *
 * Returns: 1 on success, 0 on failure
 */
int ISWQueryColor(xcb_connection_t *conn, xcb_colormap_t cmap, XColor *color);

/*
 * ISWAllocColor - Allocate a color cell
 *
 * XCB replacement for XAllocColor. Allocates a read-only color cell
 * with the closest available RGB values.
 *
 * Parameters:
 *   conn  - XCB connection
 *   cmap  - Colormap to allocate from
 *   color - XColor structure with RGB values, pixel filled in on success
 *
 * Returns: 1 on success, 0 on failure
 */
int ISWAllocColor(xcb_connection_t *conn, xcb_colormap_t cmap, XColor *color);

/*
 * IswCreateStippledPixmap - Create a stippled pixmap for grayed-out effects
 *
 * Replacement for XmuCreateStippledPixmap from libXmu.
 * Creates a 2x2 checkerboard pattern pixmap.
 *
 * Parameters:
 *   conn  - XCB connection
 *   d     - Drawable for depth reference
 *   fg    - Foreground pixel value
 *   bg    - Background pixel value
 *   depth - Depth of pixmap to create
 *
 * Returns: xcb_pixmap_t (0 on failure)
 */
xcb_pixmap_t IswCreateStippledPixmap(xcb_connection_t *conn,
                                     xcb_drawable_t d,
                                     unsigned long fg,
                                     unsigned long bg,
                                     unsigned int depth);

/*
 * ISWReleaseStippledPixmap - Free a stippled pixmap
 *
 * Releases the server resources for a pixmap created by IswCreateStippledPixmap.
 *
 * Parameters:
 *   screen - XCB screen pointer
 *   pixmap - Pixmap to free
 */
void ISWReleaseStippledPixmap(xcb_screen_t *screen, xcb_pixmap_t pixmap);

/*
 * ISWFontStructTextWidth - Calculate text width using XFontStruct
 *
 * Replacement for XTextWidth when using XFontStruct (legacy fonts).
 *
 * Parameters:
 *   font - XFontStruct pointer
 *   text - Text string
 *   len  - Length of text
 *
 * Returns: Width in pixels
 */
int ISWFontStructTextWidth(XFontStruct *font, const char *text, int len);

/*
 * ISWFontSetTextWidth - Calculate text width using XFontSet (void*)
 *
 * For internationalized text - currently a stub that returns an estimate.
 *
 * Parameters:
 *   fontset - XFontSet (void*) pointer
 *   text    - Text string
 *   len     - Length of text
 *
 * Returns: Width in pixels (estimated)
 */
int ISWFontSetTextWidth(void *fontset, const char *text, int len);

/* Generic XTextWidth that handles both types via _Generic in C11 or macro */
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 201112L
/* C11 _Generic selection */
#define XTextWidth(font, text, len) _Generic((font), \
    XFontStruct*: ISWFontStructTextWidth, \
    default: ISWFontSetTextWidth \
)((font), (text), (len))
#else
/* Pre-C11: use XFontStruct version by default */
#define XTextWidth(font, text, len) ISWFontStructTextWidth((XFontStruct*)(font), (text), (len))
#endif
#else
/* Pre-C11: use XFontStruct version by default */
#define XTextWidth(font, text, len) ISWFontStructTextWidth((XFontStruct*)(font), (text), (len))
#endif

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
 * FONT METRICS (XFontStruct compatibility)
 * =================================================================
 *
 * The XCB-based XFontStruct lacks per-character metrics.
 * These functions provide alternatives using XCB font queries.
 */

/*
 * ISWFontTextWidth - Calculate text width using XCB font queries
 *
 * Replacement for XTextWidth - queries server for text extent
 *
 * Parameters:
 *   conn - XCB connection
 *   font - XCB font ID (from XFontStruct->fid)
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
 *   font   - XFontStruct pointer (for font ID)
 *   atom   - Atom to query
 *   value  - Output value
 *
 * Returns: True if property found, False otherwise
 */
Bool IswGetFontProperty(xcb_connection_t *conn, XFontStruct *font,
                        xcb_atom_t atom, unsigned long *value);

/*
 * =================================================================
 * ATOM OPERATIONS
 * =================================================================
 */

/*
 * IswXcbInternAtom - Intern an atom using XCB
 *
 * Replacement for XInternAtom
 *
 * Parameters:
 *   conn           - XCB connection
 *   name           - Atom name string
 *   only_if_exists - If true, return None for non-existent atoms
 *
 * Returns: Atom (xcb_atom_t), or None (0) if not found
 */
xcb_atom_t IswXcbInternAtom(xcb_connection_t *conn, const char *name,
                            Bool only_if_exists);

/*
 * Standard X Selection Atoms - Runtime interning macros
 *
 * In Xlib, these were predefined constants (XCB_ATOM_STRING, XA_TEXT, etc.).
 * In XCB, we must intern them at runtime.
 */
#define XCB_ATOM_TARGETS(d)             IswXcbInternAtom((d), "TARGETS", False)
#define XCB_ATOM_TEXT(d)                IswXcbInternAtom((d), "TEXT", False)
#define XCB_ATOM_COMPOUND_TEXT(d)       IswXcbInternAtom((d), "COMPOUND_TEXT", False)
#define XCB_ATOM_LENGTH(d)              IswXcbInternAtom((d), "LENGTH", False)
#define XCB_ATOM_LIST_LENGTH(d)         IswXcbInternAtom((d), "LIST_LENGTH", False)
#define XCB_ATOM_CHARACTER_POSITION(d)  IswXcbInternAtom((d), "CHARACTER_POSITION", False)
#define XCB_ATOM_DELETE(d)              IswXcbInternAtom((d), "DELETE", False)
#define XCB_ATOM_SPAN(d)                IswXcbInternAtom((d), "SPAN", False)
#define XCB_ATOM_NULL(d)                IswXcbInternAtom((d), "NULL", False)
#define XCB_ATOM_CLIPBOARD(d)           IswXcbInternAtom((d), "CLIPBOARD", False)

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
 *   font - XCB font ID (from XFontStruct->fid)
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
 * Replacement for accessing XFontStruct->max_bounds
 *
 * Parameters:
 *   conn    - XCB connection
 *   font    - XCB font ID
 *   metrics - Output metrics structure
 */
void ISWXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                            ISWFontMetrics *metrics);

/*
 * ISWXcbDrawText - Draw text using xcb_image_text_8
 *
 * Replacement for XDrawString (draws text with background)
 *
 * Parameters:
 *   conn - XCB connection
 *   d    - Drawable (window or pixmap)
 *   gc   - Graphics context (must have font set)
 *   x, y - Text position (baseline)
 *   text - Text string
 *   len  - Length of text (max 255)
 */
void ISWXcbDrawText(xcb_connection_t *conn, xcb_drawable_t d,
                    xcb_gcontext_t gc, int16_t x, int16_t y,
                    const char *text, uint8_t len);

/*
 * =================================================================
 * REGION OPERATIONS
 * =================================================================
 *
 * These functions replace Xlib region operations.
 * Region in this implementation is a pointer to an IswRegion structure
 * containing an array of rectangles.
 */

/* Forward declare Region type - opaque pointer */
#ifndef _XAWREGION_DEFINED
#define _XAWREGION_DEFINED
typedef struct _IswRegion *ISWRegionPtr;
/* Use the Region type from Isw3dP.h if available */
#endif

/*
 * ISWCreateRegion - Create an empty region
 *
 * Returns: Newly allocated region (caller must free with ISWDestroyRegion)
 */
ISWRegionPtr ISWCreateRegion(void);

/*
 * ISWDestroyRegion - Free a region
 *
 * Parameters:
 *   region - Region to destroy
 */
void ISWDestroyRegion(ISWRegionPtr region);

/*
 * ISWUnionRectWithRegion - Add a rectangle to a region
 *
 * Parameters:
 *   rect   - Rectangle to add (xcb_rectangle_t*)
 *   source - Source region
 *   dest   - Destination region (may be same as source)
 */
void ISWUnionRectWithRegion(xcb_rectangle_t *rect, ISWRegionPtr source, ISWRegionPtr dest);

/*
 * ISWSubtractRegion - Subtract one region from another
 *
 * Parameters:
 *   regM   - Region to subtract from
 *   regS   - Region to subtract
 *   regD   - Destination region for result
 */
void ISWSubtractRegion(ISWRegionPtr regM, ISWRegionPtr regS, ISWRegionPtr regD);

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
 * ISWCopyISOLatin1Lowered - Copy string converting to lowercase
 *
 * Replacement for XmuCopyISOLatin1Lowered from libXmu
 * Copies source to dest, converting ISO Latin-1 characters to lowercase.
 *
 * Parameters:
 *   dst - Destination buffer (must be large enough)
 *   src - Source string
 */
void ISWCopyISOLatin1Lowered(char *dst, const char *src);

/*
 * =================================================================
 * TYPE CONVERTERS (libXmu replacements)
 * =================================================================
 */

/*
 * ISWCvtStringToOrientation - Convert string to XtOrientation
 *
 * Replacement for the converter that was in libXmu/Converters.c
 * Converts "horizontal" or "vertical" to XtOrientation values.
 *
 * This is an XtTypeConverter for use with XtSetTypeConverter().
 * Note: In XCB-based libXt, Display* is actually xcb_connection_t*
 */
Boolean ISWCvtStringToOrientation(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data
);

/*
 * ISWCvtStringToJustify - Convert string to XtJustify
 *
 * Replacement for the converter that was in libXmu/Converters.c
 * Converts "left", "center", or "right" to XtJustify values.
 *
 * This is an XtTypeConverter for use with XtSetTypeConverter().
 * Note: In XCB-based libXt, Display* is actually xcb_connection_t*
 */
Boolean ISWCvtStringToJustify(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data
);

/*
 * ISWCvtStringToEdgeType - Convert string to XtEdgeType
 *
 * Converts "ChainTop", "ChainBottom", "ChainLeft", "ChainRight", "Rubber"
 * to XtEdgeType values for Form widget constraints.
 *
 * This is an XtTypeConverter for use with XtSetTypeConverter().
 * Note: In XCB-based libXt, Display* is actually xcb_connection_t*
 */
Boolean ISWCvtStringToEdgeType(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data
);

/*
 * ISWCvtStringToWidget - Convert string to Widget
 *
 * Replacement for XmuCvtStringToWidget from libXmu.
 * Converts a widget name string to a Widget reference by searching
 * the widget tree starting from the parent widget.
 *
 * This is an XtTypeConverter for use with XtSetTypeConverter().
 * Requires parentCvtArgs to provide the parent widget.
 * Note: In XCB-based libXt, Display* is actually xcb_connection_t*
 */
Boolean ISWCvtStringToWidget(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data
);

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
 * ISWCompareISOLatin1 - Case-insensitive string comparison for ISO Latin-1
 *
 * Parameters:
 *   first  - First string
 *   second - Second string
 *
 * Returns:
 *   0 if strings match (case-insensitive)
 *   non-zero if strings differ
 */
int ISWCompareISOLatin1(const char *first, const char *second);

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

/* Map XUnmapWindow to XCB version */
#ifndef XUnmapWindow
#define XUnmapWindow(dpy, win) xcb_unmap_window((dpy), (win))
#endif

/* Map XMapRaised to XCB version - just use xcb_map_window (XCB doesn't have direct MapRaised) */
#ifndef XMapRaised
#define XMapRaised(dpy, win) xcb_map_window((dpy), (win))
#endif

/* XMoveResizeWindow - use xcb_configure_window */
#ifndef XMoveResizeWindow
#define XMoveResizeWindow(dpy, win, x, y, w, h) \
    do { \
        uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | \
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT; \
        uint32_t values[] = { (uint32_t)(x), (uint32_t)(y), (uint32_t)(w), (uint32_t)(h) }; \
        xcb_configure_window((dpy), (win), mask, values); \
    } while(0)
#endif

/* XLowerWindow - use xcb_configure_window with stack mode */
#ifndef XLowerWindow
#define XLowerWindow(dpy, win) \
    do { \
        uint16_t mask = XCB_CONFIG_WINDOW_STACK_MODE; \
        uint32_t values[] = { XCB_STACK_MODE_BELOW }; \
        xcb_configure_window((dpy), (win), mask, values); \
    } while(0)
#endif

/* XRaiseWindow - use xcb_configure_window with stack mode */
#ifndef XRaiseWindow
#define XRaiseWindow(dpy, win) \
    do { \
        uint16_t mask = XCB_CONFIG_WINDOW_STACK_MODE; \
        uint32_t values[] = { XCB_STACK_MODE_ABOVE }; \
        xcb_configure_window((dpy), (win), mask, values); \
    } while(0)
#endif

/* XQueryPointer - simple wrapper function declared once */
int ISWQueryPointer(xcb_connection_t *dpy, xcb_window_t win,
                    xcb_window_t *root_ret, xcb_window_t *child_ret,
                    int *root_x, int *root_y, int *win_x, int *win_y,
                    unsigned *mask);

#ifndef XQueryPointer
#define XQueryPointer ISWQueryPointer
#endif

/*
 * =================================================================
 * FONT FALLBACK HANDLING
 * =================================================================
 */

/* Load a fallback font when resource converters fail */
XFontStruct *ISWLoadFallbackFont(xcb_connection_t *conn);

/* Free a fallback font created by ISWLoadFallbackFont */
void ISWFreeFallbackFont(xcb_connection_t *conn, XFontStruct *font);

#endif /* _IswXcbDraw_h */
