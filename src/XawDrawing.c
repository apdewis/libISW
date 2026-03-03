/*
 * XawDrawing.c - XCB-based drawing utilities for Xaw3d
 * 
 * Implements drawing utilities to replace libXmu Drawing.h functions.
 * Uses XCB for X protocol communication instead of Xlib.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Intrinsic.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdlib.h>
#include <string.h>


/* 
 * FIXME: These helper functions need proper implementation as part of the
 * broader XCB port. In XCB, Screen (xcb_screen_t) doesn't have a Display member.
 * These need to be implemented using XawContext or a global Screen->Display mapping.
 */
static xcb_connection_t* GetDisplayFromScreen(xcb_screen_t *screen);
static xcb_window_t GetRootFromScreen(xcb_screen_t *screen);

/*
 * XawCreateStippledxcb_bitmap_t- Create a 50% gray stippled pixmap
 * 
 * Creates a 2x2 pixmap with a checkerboard pattern (50% gray stipple).
 * The pattern alternates between foreground and background pixels.
 */
Pixmap
XawCreateStippledPixmap(xcb_screen_t *screen, Pixel fg, Pixel bg, unsigned int depth)
{
    xcb_displ *dpy;
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    xcb_window_t root;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;
    
    /* 2x2 stipple pattern - checkerboard (50% gray)
     * Pattern:  Bit layout:
     *   X .     10
     *   . X     01
     * Bytes are ordered LSB first, so {0x02, 0x01} gives us the pattern above */
    static const uint8_t stipple_data[2] = { 0x02, 0x01 };
    
    if (!screen) {
        return None;
    }
    
    dpy = GetDisplayFromScreen(screen);
    if (!dpy) {
        return None;
    }
    
    root = GetRootFromScreen(screen);
    
    /* Generate pixmap ID */
    pixmap = xcb_generate_id(dpy);
    
    /* Create the pixmap */
    cookie = xcb_create_pixmap_checked(dpy, depth, pixmap, root, 2, 2);
    error = xcb_request_check(dpy, cookie);
    if (error) {
        free(error);
        return None;
    }
    
    /* Create a graphics context for drawing */
    gc = xcb_generate_id(dpy);
    
    uint32_t gc_values[2];
    gc_values[0] = fg;  /* foreground */
    gc_values[1] = bg;  /* background */
    
    cookie = xcb_create_gc_checked(dpy, gc, pixmap,
                                    XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                                    gc_values);
    error = xcb_request_check(dpy, cookie);
    if (error) {
        free(error);
        xcb_free_pixmap(dpy, pixmap);
        return None;
    }
    
    /* Draw the stipple pattern - use XY format for bitmap data */
    cookie = xcb_put_image_checked(dpy,
                                    XCB_IMAGE_FORMAT_XY_BITMAP,  /* format */
                                    pixmap,                       /* drawable */
                                    gc,                          /* gc */
                                    2,                           /* width */
                                    2,                           /* height */
                                    0,                           /* dst_x */
                                    0,                           /* dst_y */
                                    0,                           /* left_pad */
                                    1,                           /* depth for bitmap */
                                    sizeof(stipple_data),        /* data length */
                                    stipple_data);               /* data */
    
    error = xcb_request_check(dpy, cookie);
    if (error) {
        free(error);
        xcb_free_gc(dpy, gc);
        xcb_free_pixmap(dpy, pixmap);
        return None;
    }
    
    /* Free the GC - we don't need it anymore */
    xcb_free_gc(dpy, gc);
    
    /* Flush to ensure pixmap is created */
    xcb_flush(dpy);
    
    return (Pixmap)pixmap;
}

/*
 * XawReleaseStippledxcb_bitmap_t- Free a stippled pixmap
 * 
 * Releases the server resources for a pixmap created by XawCreateStippledPixmap.
 */
void
XawReleaseStippledPixmap(xcb_screen_t *screen, xcb_bitmap_tpixmap)
{
    xcb_connection_t *dpy;
    
    if (!screen || pixmap == None) {
        return;
    }
    
    dpy = GetDisplayFromScreen(screen);
    if (!dpy) {
        return;
    }
    
    /* Free the pixmap resource */
    xcb_free_pixmap(dpy, (xcb_pixmap_t)pixmap);
    xcb_flush(dpy);
}

/*
 * XawDistinguishablePixels - Determine if pixels are visually distinct
 * 
 * Queries the RGB values for the given pixels and determines if they
 * have sufficient color difference to be distinguishable.
 * 
 * The threshold for distinguishability is set to 10000 per color component
 * difference (on a 0-65535 scale), which provides reasonable visual distinction.
 */
Bool
XawDistinguishablePixels(xcb_connection_t *dpy, Colormap cmap, 
                        unsigned long *pixels, int count)
{
    xcb_query_colors_cookie_t cookie;
    xcb_query_colors_reply_t *reply;
    xcb_generic_error_t *error;
    xcb_rgb_t *colors;
    int i, j;
    Bool distinguishable = True;
    
    /* Need at least 2 pixels to compare */
    if (!dpy || !pixels || count < 2) {
        return True;  /* Assume distinguishable if invalid input */
    }
    
    /* Convert unsigned long pixels to uint32_t for XCB */
    uint32_t *xcb_pixels = malloc(count * sizeof(uint32_t));
    if (!xcb_pixels) {
        return True;
    }
    
    for (i = 0; i < count; i++) {
        xcb_pixels[i] = (uint32_t)pixels[i];
    }
    
    /* Query the RGB values for all pixels */
    cookie = xcb_query_colors(dpy, (xcb_colormap_t)cmap, count, xcb_pixels);
    reply = xcb_query_colors_reply(dpy, cookie, &error);
    
    free(xcb_pixels);
    
    if (error) {
        free(error);
        return True;  /* Assume distinguishable on error */
    }
    
    if (!reply) {
        return True;
    }
    
    colors = xcb_query_colors_colors(reply);
    
    /* Compare each pair of colors
     * Colors are distinguishable if any RGB component differs by more than
     * our threshold (10000 on a 0-65535 scale, which is about 15% difference) */
    #define COLOR_THRESHOLD 10000
    
    for (i = 0; i < count - 1 && distinguishable; i++) {
        for (j = i + 1; j < count && distinguishable; j++) {
            int dr = abs(colors[i].red - colors[j].red);
            int dg = abs(colors[i].green - colors[j].green);
            int db = abs(colors[i].blue - colors[j].blue);
            
            /* If all color components are too similar, pixels are not distinguishable */
            if (dr < COLOR_THRESHOLD && dg < COLOR_THRESHOLD && db < COLOR_THRESHOLD) {
                distinguishable = False;
            }
        }
    }
    
    free(reply);
    
    return distinguishable;
}

/*
 * XmuCreateStippledPixmap - Compatibility alias for XawCreateStippledPixmap
 *
 * Provides backward compatibility with code using XmuCreateStippledPixmap.
 */
xcb_bitmap_t
XmuCreateStippledPixmap(xcb_screen_t*screen, Pixel fg, Pixel bg, unsigned int depth)
{
    return XawCreateStippledPixmap(screen, fg, bg, depth);
}

/*
 * Helper function stubs - to be properly implemented as part of XCB port
 */

/*
 * GetDisplayFromScreen - Get Display (connection) from Screen
 * 
 * FIXME: In Xlib, Screen has a Display* member. In XCB, xcb_screen_t doesn't.
 * This needs to be implemented using one of:
 * 1. XawContext to map Screen -> Display
 * 2. Global Display variable (if single display assumption holds)
 * 3. Extended Screen structure with Display member
 */
static xcb_connection_t*
GetDisplayFromScreen(xcb_screen_t *screen)
{
    /* STUB: Return NULL for now - needs proper implementation */
    (void)screen;  /* Suppress unused parameter warning */
    /* TODO: Implement Screen->Display mapping */
    return NULL;
}

/*
 * GetRootFromxcb_screen_t- Get root window from Screen
 * 
 * In XCB, xcb_screen_t has a root member, so this is straightforward.
 */
static xcb_window_t
GetRootFromScreen(xcb_screen_t *screen)
{
    if (!screen) {
        return 0;
    }

    return screen->root;
}
