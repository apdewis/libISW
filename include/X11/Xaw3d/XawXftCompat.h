/*
 * XawXftCompat.h - XCB/Xft compatibility layer for Xaw3d
 *
 * This header provides XawFontSet typedef and font-related definitions
 * for the XCB migration. Full Xft support to be added in Phase 3.
 *
 * Copyright (c) 2026 Xaw3d Project
 */

#ifndef _XawXftCompat_h
#define _XawXftCompat_h

#include <xcb/xcb.h>
#include <X11/Intrinsic.h>

/*
 * XawFontSet - Font set wrapper for XCB-based text rendering
 *
 * This structure wraps XCB font information to provide a simple
 * interface for internationalized text rendering. In the future,
 * this will be extended to support Xft/FreeType fonts.
 */
typedef struct _XawFontSet {
    xcb_font_t font_id;       /* Primary XCB font ID */
    int ascent;               /* Font ascent */
    int descent;              /* Font descent */
    int height;               /* Total line height (ascent + descent) */
    /* Future: Xft font support fields */
} XawFontSet;

/*
 * XawTextWidth - Calculate text width using XawFontSet
 *
 * Parameters:
 *   fontset - XawFontSet pointer
 *   text    - Text string
 *   len     - Length of text
 *
 * Returns: Width in pixels
 *
 * Note: This is a function declaration. Implementation in XawXcbDraw.c
 */
int XawTextWidth(XawFontSet *fontset, const char *text, int len);

/*
 * XawDrawString - Draw text using XawFontSet
 *
 * Parameters:
 *   conn    - XCB connection
 *   d       - Drawable (window or pixmap)
 *   fontset - XawFontSet pointer
 *   gc      - Graphics context
 *   x, y    - Text position (baseline)
 *   text    - Text string
 *   len     - Length of text
 *
 * Note: This is a function declaration. Implementation in XawXcbDraw.c
 */
void XawDrawString(xcb_connection_t *conn, xcb_drawable_t d,
                   XawFontSet *fontset, xcb_gcontext_t gc,
                   int x, int y, const char *text, int len);

#endif /* _XawXftCompat_h */
