/*
 * IswXcbDraw.c - XCB drawing compatibility implementation
 *
 * Provides XCB-native implementations of drawing, font, and xcb_gcontext_t operations
 * that were previously handled by Xlib.
 *
 * Copyright (c) 2026 Isw3d Project
 */

#include "ISWXcbDraw.h"
#include <ISW/Form.h>  /* For IswEdgeType definition */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


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
 * ISWQueryColor - Query RGB values for a pixel
 */
int
ISWQueryColor(xcb_connection_t *conn, xcb_colormap_t cmap, IswColor *color)
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
 * =================================================================
 * XCB CORE FONT HELPERS (Phase 2: Non-Xft implementations)
 * =================================================================
 */


/*
 * ISWXcbQueryFontMetrics - Query font ascent, descent, and max width
 *
 * Replacement for accessing IswFontStruct->max_bounds directly.
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
 * =================================================================
 * FONT FALLBACK HANDLING
 * =================================================================
 */

/*
 * ISWLoadFallbackFont - Load a fallback font when resource converters fail
 *
 * This function loads a hardcoded default font using XCB when the
 * IswRFontStruct and IswRFontSet resource converters fail in the custom libXt.
 *
 * Returns: A minimal IswFontStruct with a valid font ID, or NULL on failure
 */
IswFontStruct *
ISWLoadFallbackFont(xcb_connection_t *conn)
{
    IswFontStruct *font;
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
            
            /* Create minimal IswFontStruct */
            font = (IswFontStruct *)calloc(1, sizeof(IswFontStruct));
            if (!font) {
                xcb_close_font(conn, fid);
                return NULL;
            }
            
            font->fid = fid;
            font->min_char_or_byte2 = 0;
            font->max_char_or_byte2 = 255;

            /* Query actual font metrics from X server so that
             * Cairo text rendering gets a valid (non-zero) font size. */
            {
                ISWFontMetrics metrics;
                ISWXcbQueryFontMetrics(conn, fid, &metrics);
                font->ascent  = metrics.ascent;
                font->descent = metrics.descent;
            }

            return font;
        }
        free(error);
        /* Try next fallback */
    }
    
    fprintf(stderr, "ISWLoadFallbackFont: All fallback fonts failed\n");
    return NULL;
}
