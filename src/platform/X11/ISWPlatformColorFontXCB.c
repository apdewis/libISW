/*
 * ISWPlatformColorFontXCB.c - XCB backend for the color + font platform ops
 *
 * Copyright (c) 2026 ISW Project
 *
 * Implements IswPlatformColorOps and IswPlatformFontOps over XCB: colormap
 * pixel<->RGB allocation, named-color allocation/lookup, pixel release, visual
 * matching, and core server-font open/close.  Colormap/font/visual handles are
 * value handles (each IS the native id/pointer reinterpreted), so the seam
 * conversions below are plain casts.  Pixel values stay the server's numeric
 * pixel; only the colormap/visual/font TYPES are neutral toolkit-side.
 *
 * Phase 4 of the ISWPlatform vtable (docs/ISWPLATFORM_PLAN.md).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"

/* ---- color/font/visual value handles (the internal seam) ----------------- */

xcb_colormap_t
_IswXcbColormap(IswColormap cmap)
{
    return (xcb_colormap_t) cmap;
}

IswColormap
_IswXcbColormapWrap(xcb_colormap_t cmap)
{
    return (IswColormap) cmap;
}

xcb_font_t
_IswXcbFontId(IswFontId fid)
{
    return (xcb_font_t) fid;
}

IswFontId
_IswXcbFontIdWrap(xcb_font_t fid)
{
    return (IswFontId) fid;
}

xcb_visualtype_t *
_IswXcbVisual(IswVisual vis)
{
    return (xcb_visualtype_t *) vis;
}

IswVisual
_IswXcbVisualWrap(xcb_visualtype_t *vis)
{
    return (IswVisual) vis;
}

/* ---- color ops ----------------------------------------------------------- */

static Boolean
xcb_col_query_color(IswDisplay dpy, IswColormap cmap,
                    unsigned long pixel, IswColor *out)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_query_colors_cookie_t cookie;
    xcb_query_colors_reply_t *reply;
    xcb_rgb_t *rgb;
    uint32_t px = (uint32_t)(pixel & 0xFFFFFF);

    if (!conn)
        return False;
    cookie = xcb_query_colors(conn, _IswXcbColormap(cmap), 1, &px);
    reply = xcb_query_colors_reply(conn, cookie, NULL);
    if (reply == NULL)
        return False;
    rgb = xcb_query_colors_colors(reply);
    out->pixel = pixel;
    out->red   = rgb[0].red;
    out->green = rgb[0].green;
    out->blue  = rgb[0].blue;
    out->flags = DoRed | DoGreen | DoBlue;
    free(reply);
    return True;
}

static Boolean
xcb_col_alloc_color(IswDisplay dpy, IswColormap cmap,
                    unsigned short red, unsigned short green,
                    unsigned short blue, unsigned long *pixel_out)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_alloc_color_cookie_t cookie;
    xcb_alloc_color_reply_t *reply;

    if (!conn)
        return False;
    cookie = xcb_alloc_color(conn, _IswXcbColormap(cmap),
                             (uint16_t) red, (uint16_t) green, (uint16_t) blue);
    reply = xcb_alloc_color_reply(conn, cookie, NULL);
    if (reply == NULL)
        return False;
    if (pixel_out)
        *pixel_out = reply->pixel;
    free(reply);
    return True;
}

static Boolean
xcb_col_alloc_named_color(IswDisplay dpy, IswColormap cmap,
                          const char *name, unsigned long *pixel_out)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_alloc_named_color_cookie_t cookie;
    xcb_alloc_named_color_reply_t *reply;

    if (!conn || !name)
        return False;
    cookie = xcb_alloc_named_color(conn, _IswXcbColormap(cmap),
                                   (uint16_t) strlen(name), name);
    reply = xcb_alloc_named_color_reply(conn, cookie, NULL);
    if (reply == NULL)
        return False;
    if (pixel_out)
        *pixel_out = reply->pixel;
    free(reply);
    return True;
}

static Boolean
xcb_col_lookup_color(IswDisplay dpy, IswColormap cmap, const char *name)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_lookup_color_cookie_t cookie;
    xcb_lookup_color_reply_t *reply;

    if (!conn || !name)
        return False;
    cookie = xcb_lookup_color(conn, _IswXcbColormap(cmap),
                              (uint16_t) strlen(name), name);
    reply = xcb_lookup_color_reply(conn, cookie, NULL);
    if (reply == NULL)
        return False;
    free(reply);
    return True;
}

static void
xcb_col_free_colors(IswDisplay dpy, IswColormap cmap, unsigned long pixel)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    uint32_t px = (uint32_t)(pixel & 0xFFFFFF);

    if (!conn || xcb_connection_has_error(conn) != 0)
        return;
    xcb_free_colors(conn, _IswXcbColormap(cmap), 0, 1, &px);
}

static Boolean
xcb_col_match_visual_info(IswDisplay dpy _X_UNUSED, IswScreen screen,
                          int depth, int visual_class, IswVisualInfo *out)
{
    xcb_screen_t *scr = _IswXcbScreen(screen);
    xcb_depth_iterator_t depth_iter;

    if (!scr)
        return False;
    depth_iter = xcb_screen_allowed_depths_iterator(scr);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t vis_iter;
        if (depth_iter.data->depth != depth)
            continue;
        vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
        for (; vis_iter.rem; xcb_visualtype_next(&vis_iter)) {
            if (vis_iter.data->_class == visual_class) {
                out->visual        = _IswXcbVisualWrap(vis_iter.data);
                out->visualid      = vis_iter.data->visual_id;
                out->depth         = depth;
                out->class         = visual_class;
                out->red_mask      = vis_iter.data->red_mask;
                out->green_mask    = vis_iter.data->green_mask;
                out->blue_mask     = vis_iter.data->blue_mask;
                out->colormap_size = vis_iter.data->colormap_entries;
                out->bits_per_rgb  = vis_iter.data->bits_per_rgb_value;
                return True;
            }
        }
    }
    return False;
}

/* ---- font ops ------------------------------------------------------------ */

static IswFontId
xcb_font_load_font(IswDisplay dpy, const char *name)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    xcb_font_t fid;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *err;

    if (!conn || !name)
        return 0;
    fid = xcb_generate_id(conn);
    cookie = xcb_open_font_checked(conn, fid, (uint16_t) strlen(name), name);
    err = xcb_request_check(conn, cookie);
    if (err != NULL) {
        free(err);
        return 0;               /* font not found */
    }
    return _IswXcbFontIdWrap(fid);
}

static void
xcb_font_free_font(IswDisplay dpy, IswFontId fid)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);

    if (!conn || fid == 0)
        return;
    xcb_close_font(conn, _IswXcbFontId(fid));
}

/* Last-resort font: open a hardcoded core font and populate an IswFontStruct
   with real server metrics, so text rendering gets a valid font size.
   Used when the IswRFontStruct/IswRFontSet resource converters yield no font. */
static IswFontStruct *
xcb_font_load_fallback_font(IswDisplay dpy)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    static const char *const fallback_names[] = { "fixed", "6x13", "cursor", NULL };
    int i;

    if (!conn)
        return NULL;

    for (i = 0; fallback_names[i] != NULL; i++) {
        xcb_font_t fid = xcb_generate_id(conn);
        xcb_void_cookie_t cookie =
            xcb_open_font_checked(conn, fid,
                                  (uint16_t) strlen(fallback_names[i]),
                                  fallback_names[i]);
        xcb_generic_error_t *error = xcb_request_check(conn, cookie);

        if (!error) {
            IswFontStruct *font = (IswFontStruct *) calloc(1, sizeof(IswFontStruct));
            xcb_query_font_reply_t *reply;

            if (!font) {
                xcb_close_font(conn, fid);
                return NULL;
            }

            font->fid = _IswXcbFontIdWrap(fid);
            font->min_char_or_byte2 = 0;
            font->max_char_or_byte2 = 255;

            /* Default metrics in case the query fails. */
            font->ascent  = 10;
            font->descent = 2;

            reply = xcb_query_font_reply(conn, xcb_query_font(conn, fid), NULL);
            if (reply) {
                font->ascent  = reply->font_ascent;
                font->descent = reply->font_descent;
                free(reply);
            }

            return font;
        }
        free(error);
    }

    return NULL;
}

/* ---- vtables ------------------------------------------------------------- */

const IswPlatformColorOps isw_platform_xcb_color_ops = {
    .query_color       = xcb_col_query_color,
    .alloc_color       = xcb_col_alloc_color,
    .alloc_named_color = xcb_col_alloc_named_color,
    .lookup_color      = xcb_col_lookup_color,
    .free_colors       = xcb_col_free_colors,
    .match_visual_info = xcb_col_match_visual_info,
};

const IswPlatformFontOps isw_platform_xcb_font_ops = {
    .load_font          = xcb_font_load_font,
    .free_font          = xcb_font_free_font,
    .load_fallback_font = xcb_font_load_fallback_font,
};
