/*
 * IswXftCompat.h - XCB/Xft compatibility layer for Isw3d
 *
 * This header provides ISWFontSet typedef and font-related definitions
 * for the XCB migration. Full Xft support to be added in Phase 3.
 *
 * Copyright (c) 2026 Isw3d Project
 */

#ifndef _ISW_IswXftCompat_h
#define _ISW_IswXftCompat_h

#include <xcb/xcb.h>
#include <X11/Intrinsic.h>

/*
 * ISWFontSet - Font set wrapper for XCB-based text rendering
 *
 * This structure wraps XCB font information to provide a simple
 * interface for internationalized text rendering. In the future,
 * this will be extended to support Xft/FreeType fonts.
 */
typedef struct _IswFontSet {
    xcb_connection_t *conn;   /* XCB connection (for text width queries) */
    xcb_font_t font_id;       /* Primary XCB font ID */
    int ascent;               /* Font ascent */
    int descent;              /* Font descent */
    int height;               /* Total line height (ascent + descent) */
    /* Future: Xft font support fields */
} ISWFontSet;

/*
 * IswTextWidth - Calculate text width using ISWFontSet
 *
 * Parameters:
 *   fontset - ISWFontSet pointer
 *   text    - Text string
 *   len     - Length of text
 *
 * Returns: Width in pixels
 *
 * Note: This is a function declaration. Implementation in IswXcbDraw.c
 */
int IswTextWidth(ISWFontSet *fontset, const char *text, int len);

/*
 * IswDrawString - Draw text using ISWFontSet
 *
 * Parameters:
 *   conn    - XCB connection
 *   d       - Drawable (window or pixmap)
 *   fontset - ISWFontSet pointer
 *   gc      - Graphics context
 *   x, y    - Text position (baseline)
 *   text    - Text string
 *   len     - Length of text
 *
 * Note: This is a function declaration. Implementation in IswXcbDraw.c
 */
void IswDrawString(xcb_connection_t *conn, xcb_drawable_t d,
                   ISWFontSet *fontset, xcb_gcontext_t gc,
                   int x, int y, const char *text, int len);

#endif /* _ISW_IswXftCompat_h */
