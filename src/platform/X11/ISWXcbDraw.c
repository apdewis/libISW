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
