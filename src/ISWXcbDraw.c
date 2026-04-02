/*
 * IswXcbDraw.c - XCB drawing compatibility implementation
 *
 * Provides XCB-native implementations of drawing, font, and xcb_gcontext_t operations
 * that were previously handled by Xlib.
 *
 * Copyright (c) 2026 Isw3d Project
 */

#include "ISWXcbDraw.h"
#include "../include/ISW/ISWXftCompat.h"  /* For ISWFontSet definition */
#include "../include/ISW/Form.h"  /* For IswEdgeType definition */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * =================================================================
 * WINDOW MANAGEMENT HELPERS
 * =================================================================
 */

/*
 * ISWQueryPointer - XCB wrapper for querying pointer position
 */
int
ISWQueryPointer(xcb_connection_t *dpy, xcb_window_t win,
                xcb_window_t *root_ret, xcb_window_t *child_ret,
                int *root_x, int *root_y, int *win_x, int *win_y,
                unsigned *mask)
{
    xcb_query_pointer_cookie_t cookie = xcb_query_pointer(dpy, win);
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(dpy, cookie, NULL);
    if (reply) {
        if (root_ret) *root_ret = reply->root;
        if (child_ret) *child_ret = reply->child;
        if (root_x) *root_x = reply->root_x;
        if (root_y) *root_y = reply->root_y;
        if (win_x) *win_x = reply->win_x;
        if (win_y) *win_y = reply->win_y;
        if (mask) *mask = reply->mask;
        free(reply);
        return 1;
    }
    return 0;
}

/*
 * =================================================================
 * BITMAP AND PIXMAP CREATION
 * =================================================================
 */

/*
 * IswCreateBitmapFromData - Create a depth-1 pixmap from bitmap data
 *
 * This is the XCB replacement for XCreateBitmapFromData.
 * The data is expected to be in XCB_IMAGE_ORDER_LSB_FIRST bit order (standard X bitmap format).
 */
xcb_pixmap_t
IswCreateBitmapFromData(xcb_connection_t *conn,
                        xcb_drawable_t drawable,
                        const char *data,
                        unsigned int width,
                        unsigned int height)
{
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;
    int bytes_per_row;
    int data_len;
    
    if (!conn || !data || width == 0 || height == 0)
        return 0;
    
    /* Calculate data size (each row padded to byte boundary) */
    bytes_per_row = (width + 7) / 8;
    data_len = bytes_per_row * height;
    
    /* Generate IDs for pixmap and temporary xcb_gcontext_t */
    pixmap = xcb_generate_id(conn);
    gc = xcb_generate_id(conn);
    
    /* Create a depth-1 pixmap (bitmap) */
    cookie = xcb_create_pixmap_checked(conn, 1, pixmap, drawable, width, height);
    error = xcb_request_check(conn, cookie);
    if (error) {
        free(error);
        return 0;
    }
    
    /* Create a temporary xcb_gcontext_t for the pixmap */
    {
        uint32_t gc_values[2] = { 1, 0 };  /* foreground=1, background=0 */
        cookie = xcb_create_gc_checked(conn, gc, pixmap,
                                        XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                                        gc_values);
        error = xcb_request_check(conn, cookie);
        if (error) {
            free(error);
            xcb_free_pixmap(conn, pixmap);
            return 0;
        }
    }
    
    /* Put the bitmap data into the pixmap using XCB_IMAGE_FORMAT_XY_BITMAP */
    xcb_put_image(conn,
                  XCB_IMAGE_FORMAT_XY_BITMAP,  /* format */
                  pixmap,                       /* drawable */
                  gc,                           /* gc */
                  width,                        /* width */
                  height,                       /* height */
                  0,                            /* dst_x */
                  0,                            /* dst_y */
                  0,                            /* left_pad */
                  1,                            /* depth */
                  data_len,                     /* data_len */
                  (const uint8_t *)data);       /* data */
    
    /* Free temporary xcb_gcontext_t */
    xcb_free_gc(conn, gc);
    
    /* Flush to ensure pixmap is created */
    xcb_flush(conn);
    
    return pixmap;
}

/*
 * ISWFreePixmap - Free a pixmap
 */
void
ISWFreePixmap(xcb_connection_t *conn, xcb_pixmap_t pixmap)
{
    if (conn && pixmap)
        xcb_free_pixmap(conn, pixmap);
}

/*
 * IswCreatePixmapFromBitmapData - Create a colored pixmap from bitmap data
 *
 * This is the XCB replacement for XCreatePixmapFromBitmapData.
 * Creates a pixmap at the specified depth and renders the bitmap data
 * using the provided foreground and background colors.
 */
xcb_pixmap_t
IswCreatePixmapFromBitmapData(xcb_connection_t *conn,
                              xcb_drawable_t drawable,
                              const char *data,
                              unsigned int width,
                              unsigned int height,
                              unsigned long fg,
                              unsigned long bg,
                              unsigned int depth)
{
    xcb_pixmap_t pixmap, bitmap;
    xcb_gcontext_t gc;
    
    if (!conn || !data || width == 0 || height == 0 || depth == 0)
        return 0;
    
    /* First create the depth-1 bitmap from the data */
    bitmap = IswCreateBitmapFromData(conn, drawable, data, width, height);
    if (bitmap == 0)
        return 0;
    
    /* Create the output pixmap at the specified depth */
    pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth, pixmap, drawable, width, height);
    
    /* Create a xcb_gcontext_t with foreground and background colors */
    gc = xcb_generate_id(conn);
    {
        uint32_t values[2];
        uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
        values[0] = fg;
        values[1] = bg;
        xcb_create_gc(conn, gc, pixmap, mask, values);
    }
    
    /* Copy the bitmap to the pixmap with color conversion */
    xcb_copy_plane(conn, bitmap, pixmap, gc, 0, 0, 0, 0, width, height, 1);
    
    /* Clean up */
    xcb_free_gc(conn, gc);
    ISWFreePixmap(conn, bitmap);
    xcb_flush(conn);
    
    return pixmap;
}

/*
 * ISWQueryColor - Query RGB values for a pixel
 */
int
ISWQueryColor(xcb_connection_t *conn, xcb_colormap_t cmap, XColor *color)
{
    xcb_query_colors_cookie_t cookie;
    xcb_query_colors_reply_t *reply;
    xcb_rgb_t *rgb;
    uint32_t pixel;
    
    if (!conn || !color)
        return 0;
    
    pixel = color->pixel;
    cookie = xcb_query_colors(conn, cmap, 1, &pixel);
    reply = xcb_query_colors_reply(conn, cookie, NULL);
    
    if (!reply)
        return 0;
    
    rgb = xcb_query_colors_colors(reply);
    if (rgb) {
        color->red = rgb->red;
        color->green = rgb->green;
        color->blue = rgb->blue;
    }
    
    free(reply);
    return 1;
}

/*
 * ISWAllocColor - Allocate a read-only color cell
 */
int
ISWAllocColor(xcb_connection_t *conn, xcb_colormap_t cmap, XColor *color)
{
    xcb_alloc_color_cookie_t cookie;
    xcb_alloc_color_reply_t *reply;
    
    if (!conn || !color)
        return 0;
    
    cookie = xcb_alloc_color(conn, cmap, color->red, color->green, color->blue);
    reply = xcb_alloc_color_reply(conn, cookie, NULL);
    
    if (!reply)
        return 0;
    
    color->pixel = reply->pixel;
    color->red = reply->red;
    color->green = reply->green;
    color->blue = reply->blue;
    
    free(reply);
    return 1;
}

/*
 * IswCreateStippledPixmap - Create a stippled pixmap for grayed-out effects
 *
 * Creates a 2x2 checkerboard pattern pixmap for use as a tile in GCs.
 */
xcb_pixmap_t
IswCreateStippledPixmap(xcb_connection_t *conn, xcb_drawable_t d,
                        unsigned long fg, unsigned long bg,
                        unsigned int depth)
{
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    xcb_rectangle_t rects[2];
    
    if (!conn)
        return 0;
    
    /* Create a 2x2 pixmap */
    pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, depth, pixmap, d, 2, 2);
    
    /* Create a temporary xcb_gcontext_t */
    gc = xcb_generate_id(conn);
    {
        uint32_t values[1];
        values[0] = bg;
        xcb_create_gc(conn, gc, pixmap, XCB_GC_FOREGROUND, values);
    }
    
    /* Fill with background */
    rects[0].x = 0; rects[0].y = 0; rects[0].width = 2; rects[0].height = 2;
    xcb_poly_fill_rectangle(conn, pixmap, gc, 1, rects);
    
    /* Draw foreground pixels in checkerboard pattern */
    {
        uint32_t values[1];
        values[0] = fg;
        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, values);
    }
    
    /* Draw two points for checkerboard: (0,0) and (1,1) */
    {
        xcb_point_t points[2];
        points[0].x = 0; points[0].y = 0;
        points[1].x = 1; points[1].y = 1;
        xcb_poly_point(conn, XCB_COORD_MODE_ORIGIN, pixmap, gc, 2, points);
    }
    
    xcb_free_gc(conn, gc);
    xcb_flush(conn);
    
    return pixmap;
}

/*
 * ISWFontStructTextWidth - Convenience wrapper that works with XFontStruct
 *
 * This function is provided for backward compatibility with code that
 * used XTextWidth. It requires a connection context, so it gets it from
 * a global or per-widget state.
 */
int
ISWFontStructTextWidth(XFontStruct *font, const char *text, int len)
{
    /* Note: This is a stub - in actual use, the widget should call
     * ISWXcbTextWidth directly with the connection.
     * For now, return an estimate based on character count.
     */
    if (!font || !text || len <= 0)
        return 0;
    /* Rough estimate - should use actual font metrics */
    return len * 8;  /* Assume 8 pixels per character as fallback */
}

/*
 * ISWFontSetTextWidth - Text width for XFontSet (internationalized text)
 *
 * This is a stub for internationalized text support.
 */
int
ISWFontSetTextWidth(void *fontset, const char *text, int len)
{
    (void)fontset;
    if (!text || len <= 0)
        return 0;
    /* Rough estimate for internationalized text */
    return len * 10;  /* Assume 10 pixels per character as fallback */
}

/*
 * =================================================================
 * xcb_gcontext_t VALUE HELPERS
 * =================================================================
 */

void
ISWInitGCValues(xcb_create_gc_value_list_t *values)
{
    if (values)
        memset(values, 0, sizeof(*values));
}

void
ISWSetGCFont(xcb_create_gc_value_list_t *values, xcb_font_t font)
{
    if (values)
        values->font = font;
}

void
ISWSetGCForeground(xcb_create_gc_value_list_t *values, uint32_t pixel)
{
    if (values)
        values->foreground = pixel;
}

void
ISWSetGCBackground(xcb_create_gc_value_list_t *values, uint32_t pixel)
{
    if (values)
        values->background = pixel;
}

void
ISWSetGCGraphicsExposures(xcb_create_gc_value_list_t *values, int exposures)
{
    if (values)
        values->graphics_exposures = exposures ? 1 : 0;
}

void
ISWSetGCFunction(xcb_create_gc_value_list_t *values, uint32_t function)
{
    if (values)
        values->function = function;
}

/*
 * =================================================================
 * FONT METRICS
 * =================================================================
 */

/*
 * ISWFontTextWidth - Calculate text width using XCB
 *
 * Uses xcb_query_text_extents to get the width of a text string.
 */
int
ISWFontTextWidth(xcb_connection_t *conn, xcb_font_t font,
                 const char *text, int len)
{
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t *reply;
    xcb_char2b_t *chars;
    int i, width = 0;
    
    if (!conn || !text || len <= 0)
        return 0;
    
    /* Convert text to char2b format (required by XCB) */
    chars = (xcb_char2b_t *)malloc(len * sizeof(xcb_char2b_t));
    if (!chars)
        return 0;
    
    for (i = 0; i < len; i++) {
        chars[i].byte1 = 0;
        chars[i].byte2 = (uint8_t)text[i];
    }
    
    /* Query text extents */
    cookie = xcb_query_text_extents(conn, font, len, chars);
    reply = xcb_query_text_extents_reply(conn, cookie, NULL);
    
    free(chars);
    
    if (reply) {
        width = reply->overall_width;
        free(reply);
    }
    
    return width;
}

/*
 * ISWFontCharWidth - Get width of a single character
 */
int
ISWFontCharWidth(xcb_connection_t *conn, xcb_font_t font, unsigned char c)
{
    char text[1];
    text[0] = (char)c;
    return ISWFontTextWidth(conn, font, text, 1);
}

/*
 * IswGetFontProperty - Get a font property
 *
 * Queries the font for a specific property atom.
 */
Bool
IswGetFontProperty(xcb_connection_t *conn, XFontStruct *font,
                   xcb_atom_t atom, unsigned long *value)
{
    xcb_query_font_cookie_t cookie;
    xcb_query_font_reply_t *reply;
    xcb_fontprop_t *props;
    int num_props, i;
    Bool found = False;
    
    if (!conn || !font || !value)
        return False;
    
    /* Query font properties */
    cookie = xcb_query_font(conn, font->fid);
    reply = xcb_query_font_reply(conn, cookie, NULL);
    
    if (!reply)
        return False;
    
    /* Search for the property */
    props = xcb_query_font_properties(reply);
    num_props = xcb_query_font_properties_length(reply);
    
    for (i = 0; i < num_props; i++) {
        if (props[i].name == (xcb_atom_t)atom) {
            *value = props[i].value;
            found = True;
            break;
        }
    }
    
    free(reply);
    return found;
}

/*
 * =================================================================
 * ATOM OPERATIONS
 * =================================================================
 */

xcb_atom_t
IswXcbInternAtom(xcb_connection_t *conn, const char *name, Bool only_if_exists)
{
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = 0;
    
    if (!conn || !name)
        return 0;
    
    cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0,
                             strlen(name), name);
    reply = xcb_intern_atom_reply(conn, cookie, NULL);
    
    if (reply) {
        atom = reply->atom;
        free(reply);
    }
    
    return atom;
}

/*
 * =================================================================
 * TEXT DRAWING
 * =================================================================
 */

/*
 * ISWXcbDrawImageString - Draw text with background
 *
 * Uses xcb_image_text_8 for 8-bit text drawing.
 */
void
ISWXcbDrawImageString(xcb_connection_t *conn, xcb_drawable_t d,
                      xcb_gcontext_t gc, int x, int y,
                      const char *text, int len)
{
    if (!conn || !text || len <= 0)
        return;
    
    /* XCB limits text to 255 characters per request */
    while (len > 0) {
        int chunk = (len > 255) ? 255 : len;
        xcb_image_text_8(conn, chunk, d, gc, x, y, text);
        text += chunk;
        len -= chunk;
        /* Note: for multiple chunks, x position would need adjustment */
        /* This is a simplified implementation for typical use cases */
    }
    
    xcb_flush(conn);
}

/*
 * ISWXcbDrawString - Draw text without background
 *
 * Note: XCB doesn't have a direct equivalent to XDrawString that
 * draws only foreground without background. We use poly_text_8
 * which draws text over existing background.
 */
void
ISWXcbDrawString(xcb_connection_t *conn, xcb_drawable_t d,
                 xcb_gcontext_t gc, int x, int y,
                 const char *text, int len)
{
    if (!conn || !text || len <= 0)
        return;
    
    /* Use poly_text_8 for transparent background text */
    /* Build text item with delta=0 */
    while (len > 0) {
        int chunk = (len > 254) ? 254 : len;  /* poly_text allows 254 chars max */
        
        /* For simplicity, use image_text but caller should set
         * xcb_gcontext_t fill style appropriately for transparent effect.
         * A full implementation would use poly_text_8 with proper items. */
        xcb_image_text_8(conn, chunk, d, gc, x, y, text);
        text += chunk;
        len -= chunk;
    }
    
    xcb_flush(conn);
}

/*
 * =================================================================
 * XAWFONTSET TEXT RENDERING (IswXftCompat.h implementations)
 * =================================================================
 */

/*
 * IswTextWidth - Calculate text width using ISWFontSet
 *
 * Wrapper around ISWFontTextWidth that uses the connection stored in fontset.
 * This provides a simplified API for widgets that have a fontset pointer.
 */
int
IswTextWidth(ISWFontSet *fontset, const char *text, int len)
{
    if (!fontset || !fontset->conn || !text || len <= 0)
        return 0;
    
    /* Use the fontset's connection and font_id to query text width */
    return ISWFontTextWidth(fontset->conn, fontset->font_id, text, len);
}

/*
 * IswDrawString - Draw text using ISWFontSet
 *
 * Draws text string using the font specified in fontset.
 * Uses xcb_image_text_8 which draws with background fill.
 */
void
IswDrawString(xcb_connection_t *conn, xcb_drawable_t d,
              ISWFontSet *fontset, xcb_gcontext_t gc,
              int x, int y, const char *text, int len)
{
    if (!conn || !fontset || !text || len <= 0)
        return;
    
    /* Draw text in chunks (XCB limits to 255 chars per request) */
    while (len > 0) {
        int chunk = (len > 255) ? 255 : len;
        xcb_image_text_8(conn, chunk, d, gc, x, y, text);
        
        /* For multi-chunk text, advance x position */
        if (len > 255 && fontset->conn) {
            int chunk_width = ISWFontTextWidth(fontset->conn, fontset->font_id, text, chunk);
            x += chunk_width;
        }
        
        text += chunk;
        len -= chunk;
    }
    
    xcb_flush(conn);
}

/*
 * =================================================================
 * XCB CORE FONT HELPERS (Phase 2: Non-Xft implementations)
 * =================================================================
 */

/*
 * ISWXcbTextWidth - Calculate text width
 *
 * This is an alias/wrapper for ISWFontTextWidth for consistency
 * with the function naming in the header.
 */
int
ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                const char *text, int len)
{
    return ISWFontTextWidth(conn, font, text, len);
}

/*
 * ISWXcbQueryFontMetrics - Query font ascent, descent, and max width
 *
 * Replacement for accessing XFontStruct->max_bounds directly.
 */
void
ISWXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                       ISWFontMetrics *metrics)
{
    xcb_query_font_cookie_t cookie;
    xcb_query_font_reply_t *reply;
    
    if (!conn || !metrics)
        return;
    
    /* Set fallback values in case query fails */
    metrics->ascent = 10;
    metrics->descent = 2;
    metrics->max_char_width = 8;
    
    /* Query the font from server */
    cookie = xcb_query_font(conn, font);
    reply = xcb_query_font_reply(conn, cookie, NULL);
    
    if (reply) {
        metrics->ascent = reply->font_ascent;
        metrics->descent = reply->font_descent;
        metrics->max_char_width = reply->max_bounds.character_width;
        free(reply);
    }
}

/*
 * ISWXcbDrawText - Draw text using xcb_image_text_8
 *
 * Replacement for XDrawString. Draws text with background fill.
 */
void
ISWXcbDrawText(xcb_connection_t *conn, xcb_drawable_t d,
               xcb_gcontext_t gc, int16_t x, int16_t y,
               const char *text, uint8_t len)
{
    if (!conn || !text || len == 0)
        return;
    
    /* XCB image_text_8 draws text with background */
    xcb_image_text_8(conn, len, d, gc, x, y, text);
    xcb_flush(conn);
}

/*
 * =================================================================
 * STRING UTILITIES (libXmu replacements)
 * =================================================================
 */

/*
 * ISWCopyISOLatin1Lowered - Copy string converting to lowercase
 *
 * This is a replacement for XmuCopyISOLatin1Lowered from libXmu.
 * Copies source to dest, converting ISO Latin-1 characters to lowercase.
 * The ISO Latin-1 character set has uppercase letters at:
 *   0x41-0x5A (A-Z) -> 0x61-0x7A (a-z)
 *   0xC0-0xDE (uppercase accented) -> 0xE0-0xFE (lowercase accented)
 *   Exception: 0xD7 (multiplication sign) has no lowercase
 */
void
ISWCopyISOLatin1Lowered(char *dst, const char *src)
{
    unsigned char c;
    
    if (!dst || !src)
        return;
    
    while ((c = (unsigned char)*src++) != '\0') {
        /* ASCII uppercase A-Z */
        if (c >= 0x41 && c <= 0x5A) {
            c += 0x20;  /* Convert to lowercase */
        }
        /* ISO Latin-1 uppercase accented (except multiplication sign at 0xD7) */
        else if (c >= 0xC0 && c <= 0xDE && c != 0xD7) {
            c += 0x20;  /* Convert to lowercase */
        }
        *dst++ = (char)c;
    }
    *dst = '\0';
}

/*
 * =================================================================
 * TYPE CONVERTERS (libXmu replacements)
 * =================================================================
 */

/*
 * ISWCvtStringToOrientation - Convert string to XtOrientation
 *
 * Converts "horizontal" or "vertical" (case-insensitive) to XtOrientation.
 */
Boolean
ISWCvtStringToOrientation(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data)
{
    static XtOrientation orientation;
    char lowerName[64];
    const char *str = (const char *)from->addr;
    
    (void)display;     /* unused */
    (void)args;        /* unused */
    (void)num_args;    /* unused */
    (void)converter_data;  /* unused */
    
    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;
    
    ISWCopyISOLatin1Lowered(lowerName, str);
    
    if (strcmp(lowerName, "horizontal") == 0) {
        orientation = XtorientHorizontal;
    } else if (strcmp(lowerName, "vertical") == 0) {
        orientation = XtorientVertical;
    } else {
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (XtPointer)&orientation;
    } else if (to->size < sizeof(XtOrientation)) {
        to->size = sizeof(XtOrientation);
        return False;
    } else {
        *(XtOrientation *)to->addr = orientation;
    }
    to->size = sizeof(XtOrientation);
    
    return True;
}

/*
 * ISWCvtStringToJustify - Convert string to XtJustify
 *
 * Converts "left", "center", or "right" (case-insensitive) to XtJustify.
 */
Boolean
ISWCvtStringToJustify(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data)
{
    static XtJustify justify;
    char lowerName[64];
    const char *str = (const char *)from->addr;
    
    (void)display;     /* unused */
    (void)args;        /* unused */
    (void)num_args;    /* unused */
    (void)converter_data;  /* unused */
    
    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;
    
    ISWCopyISOLatin1Lowered(lowerName, str);
    
    if (strcmp(lowerName, "left") == 0) {
        justify = XtJustifyLeft;
    } else if (strcmp(lowerName, "center") == 0) {
        justify = XtJustifyCenter;
    } else if (strcmp(lowerName, "right") == 0) {
        justify = XtJustifyRight;
    } else {
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (XtPointer)&justify;
    } else if (to->size < sizeof(XtJustify)) {
        to->size = sizeof(XtJustify);
        return False;
    } else {
        *(XtJustify *)to->addr = justify;
    }
    to->size = sizeof(XtJustify);
    
    return True;
}

/*
 * ISWCvtStringToEdgeType - Convert string to XtEdgeType
 *
 * Converts "ChainTop", "ChainBottom", "ChainLeft", "ChainRight", "Rubber"
 * (case-insensitive) to XtEdgeType values for Form widget constraints.
 */
Boolean
ISWCvtStringToEdgeType(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data)
{
    static IswEdgeType edge;  /* Use IswEdgeType from Form.h */
    char lowerName[64];
    const char *str = (const char *)from->addr;
    
    (void)display;     /* unused */
    (void)args;        /* unused */
    (void)num_args;    /* unused */
    (void)converter_data;  /* unused */
    
    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;
    
    ISWCopyISOLatin1Lowered(lowerName, str);
    
    /* Use IswChain* values from Form.h (Form.h maps XtChain* to IswChain* via defines) */
    if (strcmp(lowerName, "chaintop") == 0) {
        edge = IswChainTop;
    } else if (strcmp(lowerName, "chainbottom") == 0) {
        edge = IswChainBottom;
    } else if (strcmp(lowerName, "chainleft") == 0) {
        edge = IswChainLeft;
    } else if (strcmp(lowerName, "chainright") == 0) {
        edge = IswChainRight;
    } else if (strcmp(lowerName, "rubber") == 0) {
        edge = IswRubber;
    } else {
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (XtPointer)&edge;
    } else if (to->size < sizeof(IswEdgeType)) {
        to->size = sizeof(IswEdgeType);
        return False;
    } else {
        *(IswEdgeType *)to->addr = edge;
    }
    to->size = sizeof(IswEdgeType);
    
    return True;
}

/*
 * ISWCvtStringToWidget - Convert string to Widget
 *
 * Replacement for XmuCvtStringToWidget from libXmu.
 * Converts a widget name string to a Widget reference by searching
 * the widget tree starting from the parent widget (provided via args).
 */
Boolean
ISWCvtStringToWidget(
    xcb_connection_t *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data)
{
    static Widget widget;
    Widget parent;
    const char *name;
    
    (void)display;         /* unused */
    (void)converter_data;  /* unused */
    
    /* Need exactly one argument: the parent widget */
    if (*num_args != 1) {
        XtAppWarningMsg(
            XtWidgetToApplicationContext(*((Widget *)args[0].addr)),
            "wrongParameters", "cvtStringToWidget", "XtToolkitError",
            "String to Widget conversion requires parent argument",
            (String *)NULL, (Cardinal *)NULL);
        return False;
    }
    
    parent = *((Widget *)args[0].addr);
    name = (const char *)from->addr;
    
    if (name == NULL || *name == '\0') {
        return False;
    }
    
    /* Look up widget by name from the parent */
    widget = XtNameToWidget(parent, (String)name);
    
    if (widget == (Widget)NULL) {
        /* Widget not found - not an error, may be created later */
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (XtPointer)&widget;
    } else if (to->size < sizeof(Widget)) {
        to->size = sizeof(Widget);
        return False;
    } else {
        *(Widget *)to->addr = widget;
    }
    to->size = sizeof(Widget);
    
    return True;
}

/*
 * =================================================================
 * REGION OPERATIONS
 * =================================================================
 */

/* Maximum number of rectangles in a region (can be expanded if needed) */
#define XAWREGION_MAXRECTS 64

/* Internal region structure */
typedef struct _IswRegion {
    int numRects;
    xcb_rectangle_t rects[XAWREGION_MAXRECTS];
    xcb_rectangle_t extents;  /* Bounding box */
} IswRegion;

/*
 * ISWCreateRegion - Create an empty region
 */
ISWRegionPtr
ISWCreateRegion(void)
{
    ISWRegionPtr region = (ISWRegionPtr)calloc(1, sizeof(IswRegion));
    return region;
}

/*
 * ISWDestroyRegion - Free a region
 */
void
ISWDestroyRegion(ISWRegionPtr region)
{
    if (region)
        free(region);
}

/* Helper to update region extents */
static void
UpdateRegionExtents(ISWRegionPtr region)
{
    int i;
    int16_t minx, miny, maxx, maxy;
    
    if (region->numRects == 0) {
        region->extents.x = 0;
        region->extents.y = 0;
        region->extents.width = 0;
        region->extents.height = 0;
        return;
    }
    
    minx = region->rects[0].x;
    miny = region->rects[0].y;
    maxx = region->rects[0].x + region->rects[0].width;
    maxy = region->rects[0].y + region->rects[0].height;
    
    for (i = 1; i < region->numRects; i++) {
        if (region->rects[i].x < minx)
            minx = region->rects[i].x;
        if (region->rects[i].y < miny)
            miny = region->rects[i].y;
        if (region->rects[i].x + region->rects[i].width > maxx)
            maxx = region->rects[i].x + region->rects[i].width;
        if (region->rects[i].y + region->rects[i].height > maxy)
            maxy = region->rects[i].y + region->rects[i].height;
    }
    
    region->extents.x = minx;
    region->extents.y = miny;
    region->extents.width = maxx - minx;
    region->extents.height = maxy - miny;
}

/*
 * ISWUnionRectWithRegion - Add a rectangle to a region
 *
 * Simplified implementation: just adds the rectangle to the list.
 * A full implementation would merge overlapping rectangles.
 */
void
ISWUnionRectWithRegion(xcb_rectangle_t *rect, ISWRegionPtr source, ISWRegionPtr dest)
{
    int i;
    
    if (!rect || !source || !dest)
        return;
    
    /* Copy source to dest if different */
    if (source != dest) {
        dest->numRects = source->numRects;
        for (i = 0; i < source->numRects; i++)
            dest->rects[i] = source->rects[i];
    }
    
    /* Add the new rectangle if there's room */
    if (dest->numRects < XAWREGION_MAXRECTS) {
        dest->rects[dest->numRects] = *rect;
        dest->numRects++;
    }
    
    UpdateRegionExtents(dest);
}

/*
 * ISWSubtractRegion - Subtract one region from another
 *
 * Simplified implementation: for the specific use case in Command.c,
 * this creates a "frame" region (outer - inner).
 * A full implementation would handle complex polygon subtraction.
 */
void
ISWSubtractRegion(ISWRegionPtr regM, ISWRegionPtr regS, ISWRegionPtr regD)
{
    /* 
     * Simplified subtraction for frame regions:
     * If regM has 1 rect (outer) and regS has 1 rect (inner),
     * create 4 rectangles for the frame.
     */
    if (!regM || !regS || !regD)
        return;
    
    if (regM->numRects == 1 && regS->numRects == 1) {
        xcb_rectangle_t *outer = &regM->rects[0];
        xcb_rectangle_t *inner = &regS->rects[0];
        
        regD->numRects = 0;
        
        /* Top rectangle */
        if (inner->y > outer->y) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = outer->y;
            regD->rects[regD->numRects].width = outer->width;
            regD->rects[regD->numRects].height = inner->y - outer->y;
            regD->numRects++;
        }
        
        /* Bottom rectangle */
        if ((inner->y + inner->height) < (outer->y + outer->height)) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = inner->y + inner->height;
            regD->rects[regD->numRects].width = outer->width;
            regD->rects[regD->numRects].height = (outer->y + outer->height) - (inner->y + inner->height);
            regD->numRects++;
        }
        
        /* Left rectangle */
        if (inner->x > outer->x) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = inner->y;
            regD->rects[regD->numRects].width = inner->x - outer->x;
            regD->rects[regD->numRects].height = inner->height;
            regD->numRects++;
        }
        
        /* Right rectangle */
        if ((inner->x + inner->width) < (outer->x + outer->width)) {
            regD->rects[regD->numRects].x = inner->x + inner->width;
            regD->rects[regD->numRects].y = inner->y;
            regD->rects[regD->numRects].width = (outer->x + outer->width) - (inner->x + inner->width);
            regD->rects[regD->numRects].height = inner->height;
            regD->numRects++;
        }
        
        UpdateRegionExtents(regD);
    } else {
        /* For complex cases, just copy regM */
        int i;
        regD->numRects = regM->numRects;
        for (i = 0; i < regM->numRects; i++)
            regD->rects[i] = regM->rects[i];
        regD->extents = regM->extents;
    }
}

/*
 * ISWReshapeWidget - Shape a widget using the X Shape extension
 *
 * This is a stub implementation. Full shape support requires:
 * - xcb-shape library
 * - Proper shape pixmap creation
 * - For now, just returns True (rectangular shape)
 */
Boolean
ISWReshapeWidget(Widget w, int shape_style, int corner_width, int corner_height)
{
    (void)w;
    (void)shape_style;
    (void)corner_width;
    (void)corner_height;
    
    /*
     * TODO: Implement proper shape extension support using xcb-shape.
     * For now, return True indicating rectangular (default) shape.
     * This allows the code to compile and run, but shaped buttons
     * will appear rectangular.
     */
    return True;
}

/*
 * =================================================================
 * COLOR UTILITIES
 * =================================================================
 */

/*
 * ISWDistinguishablePixels - Check if pixels are visually distinguishable
 *
 * Simplified implementation for XCB. A full implementation would use
 * xcb_query_colors to get the RGB values and compare them with a threshold.
 * For now, we use a simple comparison - if pixels are identical, they're
 * not distinguishable; otherwise assume they are.
 */
Boolean
ISWDistinguishablePixels(
    xcb_connection_t *conn,
    xcb_colormap_t colormap,
    unsigned long *pixels,
    int count)
{
    int i, j;
    
    (void)conn;       /* Unused for simplified version */
    (void)colormap;   /* Unused for simplified version */
    
    /* Check if all pixels are different from each other */
    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (pixels[i] == pixels[j]) {
                /* Two pixels are identical - not distinguishable */
                return False;
            }
        }
    }
    
    /*
     * All pixels have different values, so assume they're distinguishable.
     *
     * TODO: Full implementation should use xcb_query_colors to get actual
     * RGB values and compare with a perceptual threshold, like:
     *
     * xcb_query_colors_cookie_t cookie;
     * xcb_query_colors_reply_t *reply;
     * cookie = xcb_query_colors(conn, colormap, count, pixels);
     * reply = xcb_query_colors_reply(conn, cookie, NULL);
     * if (reply) {
     *     xcb_rgb_iterator_t iter = xcb_query_colors_colors_iterator(reply);
     *     // Compare RGB values with threshold
     *     free(reply);
     * }
     */
    return True;
}

/*
 * ISWCompareISOLatin1 - Case-insensitive string comparison for ISO Latin-1
 */
int ISWCompareISOLatin1(const char *first, const char *second)
{
    const unsigned char *p1 = (const unsigned char *)first;
    const unsigned char *p2 = (const unsigned char *)second;
    unsigned char c1, c2;
    
    while (*p1 && *p2) {
        c1 = *p1;
        c2 = *p2;
        
        /* Convert to lowercase if uppercase */
        if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 += 'a' - 'A';
            
        if (c1 != c2)
            return c1 - c2;
            
        p1++;
        p2++;
    }
    
    return *p1 - *p2;
}

/*
 * IswLocatePixmapFile - Stub implementation
 *
 * This is a stub that always returns None. Full implementation would require
 * XPM library support or other image format handling.
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
    int *yhotp)
{
    /* Suppress unused parameter warnings */
    (void)screen;
    (void)name;
    (void)fore;
    (void)back;
    (void)depth;
    (void)srcname;
    (void)srcnamelen;
    (void)widthp;
    (void)heightp;
    (void)xhotp;
    (void)yhotp;
    
    /* TODO: Implement pixmap file loading
     * This would require:
     * - XPM library integration for XPM files
     * - XBM parsing for bitmap files
     * - File path resolution
     * - Pixmap creation from file data
     */
    
    return None;
}

/*
 * =================================================================
 * FONT FALLBACK HANDLING
 * =================================================================
 */

/*
 * ISWLoadFallbackFont - Load a fallback font when resource converters fail
 *
 * This function loads a hardcoded default font using XCB when the
 * XtRFontStruct and XtRFontSet resource converters fail in the custom libXt.
 *
 * Returns: A minimal XFontStruct with a valid font ID, or NULL on failure
 */
XFontStruct *
ISWLoadFallbackFont(xcb_connection_t *conn)
{
    XFontStruct *font;
    xcb_font_t fid;
    const char *fallback_names[] = {
        "fixed",
        "6x13",
        "cursor",
        NULL
    };
    int i;
    
    if (!conn) {
        fprintf(stderr, "ISWLoadFallbackFont: NULL connection\n");
        return NULL;
    }
    
    /* Try each fallback font name in order */
    for (i = 0; fallback_names[i] != NULL; i++) {
        fid = xcb_generate_id(conn);
        xcb_void_cookie_t cookie = xcb_open_font_checked(
            conn, fid, strlen(fallback_names[i]), fallback_names[i]);
        
        xcb_generic_error_t *error = xcb_request_check(conn, cookie);
        if (!error) {
            /* Font loaded successfully */
            fprintf(stderr, "ISWLoadFallbackFont: Loaded font '%s' with fid=%lu\n",
                    fallback_names[i], (unsigned long)fid);
            
            /* Create minimal XFontStruct */
            font = (XFontStruct *)calloc(1, sizeof(XFontStruct));
            if (!font) {
                xcb_close_font(conn, fid);
                return NULL;
            }
            
            font->fid = fid;
            font->min_char_or_byte2 = 0;
            font->max_char_or_byte2 = 255;
            
            /* Note: We don't query font properties here to avoid additional
             * round-trips. The widgets will use reasonable defaults. */
            
            return font;
        }
        free(error);
        /* Try next fallback */
    }
    
    fprintf(stderr, "ISWLoadFallbackFont: All fallback fonts failed\n");
    return NULL;
}

/*
 * ISWFreeFallbackFont - Free a fallback font created by ISWLoadFallbackFont
 */
void
ISWFreeFallbackFont(xcb_connection_t *conn, XFontStruct *font)
{
    if (font) {
        if (conn && font->fid) {
            xcb_close_font(conn, font->fid);
        }
        free(font);
    }
}
