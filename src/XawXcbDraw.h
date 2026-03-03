/*
 * XawXcbDraw.h - XCB drawing compatibility functions for Xaw3d
 * 
 * Provides XCB-native implementations of drawing, font, and GC operations
 * that were previously handled by Xlib. This is part of the complete
 * Xlib-to-XCB migration.
 *
 * Copyright (c) 2026 Xaw3d Project
 */

#ifndef _XawXcbDraw_h
#define _XawXcbDraw_h

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <X11/Intrinsic.h>

/*
 * =================================================================
 * BITMAP AND PIXMAP CREATION
 * =================================================================
 */

/*
 * XawCreateBitmapFromData - Create a pixmap from static bitmap data
 *
 * XCB replacement for XCreateBitmapFromData
 *
 * Parameters:
 *   conn     - XCB connection
 *   drawable - Drawable to determine depth (root window usually)
 *   data     - Pointer to bitmap data (LSBFirst bit order)
 *   width    - Width of bitmap
 *   height   - Height of bitmap
 *
 * Returns: xcb_pixmap_t (0 on failure)
 *
 * Note: Creates a depth-1 pixmap (bitmap) suitable for use as
 * cursor shapes, stipple patterns, or clip masks.
 */
xcb_pixmap_t XawCreateBitmapFromData(xcb_connection_t *conn,
                                     xcb_drawable_t drawable,
                                     const char *data,
                                     unsigned int width,
                                     unsigned int height);

/*
 * XawFreePixmap - Free a pixmap created by XawCreateBitmapFromData
 *
 * XCB replacement for XFreePixmap
 *
 * Parameters:
 *   conn   - XCB connection
 *   pixmap - Pixmap to free
 */
void XawFreePixmap(xcb_connection_t *conn, xcb_pixmap_t pixmap);

/*
 * =================================================================
 * GC VALUE HELPERS
 * =================================================================
 *
 * XCB uses xcb_create_gc_value_list_t instead of XGCValues.
 * These helpers make it easier to set up GC values.
 */

/*
 * XawGCValueMask - Mask values for GC creation
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

/* GXxor for function value */
#ifndef GXxor
#define GXxor XCB_GX_XOR
#endif

/*
 * XawInitGCValues - Initialize an xcb_create_gc_value_list_t structure
 *
 * Parameters:
 *   values - Pointer to value list to initialize
 *
 * Sets all fields to safe defaults.
 */
void XawInitGCValues(xcb_create_gc_value_list_t *values);

/*
 * XawSetGCFont - Set font in GC value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   font   - XCB font ID
 */
void XawSetGCFont(xcb_create_gc_value_list_t *values, xcb_font_t font);

/*
 * XawSetGCForeground - Set foreground in GC value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   pixel  - Foreground pixel value
 */
void XawSetGCForeground(xcb_create_gc_value_list_t *values, uint32_t pixel);

/*
 * XawSetGCBackground - Set background in GC value list
 *
 * Parameters:
 *   values - Pointer to value list
 *   pixel  - Background pixel value
 */
void XawSetGCBackground(xcb_create_gc_value_list_t *values, uint32_t pixel);

/*
 * XawSetGCGraphicsExposures - Set graphics_exposures in GC value list
 *
 * Parameters:
 *   values   - Pointer to value list
 *   exposures - Boolean (0 = off, 1 = on)
 */
void XawSetGCGraphicsExposures(xcb_create_gc_value_list_t *values, int exposures);

/*
 * XawSetGCFunction - Set function in GC value list
 *
 * Parameters:
 *   values   - Pointer to value list
 *   function - GC function (e.g., GXxor)
 */
void XawSetGCFunction(xcb_create_gc_value_list_t *values, uint32_t function);

/*
 * =================================================================
 * FONT METRICS (XFontStruct compatibility)
 * =================================================================
 *
 * The XCB-based XFontStruct lacks per-character metrics.
 * These functions provide alternatives using XCB font queries.
 */

/*
 * XawFontTextWidth - Calculate text width using XCB font queries
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
int XawFontTextWidth(xcb_connection_t *conn, xcb_font_t font,
                     const char *text, int len);

/*
 * XawFontCharWidth - Get width of a single character
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
int XawFontCharWidth(xcb_connection_t *conn, xcb_font_t font, unsigned char c);

/*
 * XawGetFontProperty - Get a font property by atom
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
Bool XawGetFontProperty(xcb_connection_t *conn, XFontStruct *font,
                        Atom atom, unsigned long *value);

/*
 * =================================================================
 * ATOM OPERATIONS
 * =================================================================
 */

/*
 * XawXcbInternAtom - Intern an atom using XCB
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
xcb_atom_t XawXcbInternAtom(xcb_connection_t *conn, const char *name,
                            Bool only_if_exists);

/*
 * =================================================================
 * TEXT DRAWING (Server fonts - for compatibility)
 * =================================================================
 *
 * Note: For modern text rendering, use the Xft layer in XawXftCompat.h
 * These functions provide XCB-based server font drawing for legacy code.
 */

/*
 * XawXcbDrawImageString - Draw text with background using XCB
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
void XawXcbDrawImageString(xcb_connection_t *conn, xcb_drawable_t d,
                           xcb_gcontext_t gc, int x, int y,
                           const char *text, int len);

/*
 * XawXcbDrawString - Draw text without background using XCB
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
void XawXcbDrawString(xcb_connection_t *conn, xcb_drawable_t d,
                      xcb_gcontext_t gc, int x, int y,
                      const char *text, int len);

/*
 * =================================================================
 * XCB CORE FONT HELPERS (Phase 2: Non-Xft text rendering)
 * =================================================================
 */

/*
 * XawXcbTextWidth - Calculate text width using xcb_query_text_extents
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
int XawXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len);

/*
 * XawFontMetrics - Font metrics structure
 */
typedef struct {
    int ascent;
    int descent;
    int max_char_width;
} XawFontMetrics;

/*
 * XawXcbQueryFontMetrics - Query font metrics using xcb_query_font
 *
 * Replacement for accessing XFontStruct->max_bounds
 *
 * Parameters:
 *   conn    - XCB connection
 *   font    - XCB font ID
 *   metrics - Output metrics structure
 */
void XawXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                            XawFontMetrics *metrics);

/*
 * XawXcbDrawText - Draw text using xcb_image_text_8
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
void XawXcbDrawText(xcb_connection_t *conn, xcb_drawable_t d,
                    xcb_gcontext_t gc, int16_t x, int16_t y,
                    const char *text, uint8_t len);

#endif /* _XawXcbDraw_h */
