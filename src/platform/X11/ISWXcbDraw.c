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
 * TYPE CONVERTERS (libXmu replacements)
 * =================================================================
 */

/*
 * ISWCvtStringToOrientation - Convert string to IswOrientation
 *
 * Converts "horizontal" or "vertical" (case-insensitive) to IswOrientation.
 */
Boolean
ISWCvtStringToOrientation(
    IswDisplay display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    IswPointer *converter_data)
{
    static IswOrientation orientation;
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
        orientation = IswOrientHorizontal;
    } else if (strcmp(lowerName, "vertical") == 0) {
        orientation = IswOrientVertical;
    } else {
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (IswPointer)&orientation;
    } else if (to->size < sizeof(IswOrientation)) {
        to->size = sizeof(IswOrientation);
        return False;
    } else {
        *(IswOrientation *)to->addr = orientation;
    }
    to->size = sizeof(IswOrientation);
    
    return True;
}

/*
 * ISWCvtStringToJustify - Convert string to IswJustify
 *
 * Converts "left", "center", or "right" (case-insensitive) to IswJustify.
 */
Boolean
ISWCvtStringToJustify(
    IswDisplay display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    IswPointer *converter_data)
{
    static IswJustify justify;
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
        justify = IswJustifyLeft;
    } else if (strcmp(lowerName, "center") == 0) {
        justify = IswJustifyCenter;
    } else if (strcmp(lowerName, "right") == 0) {
        justify = IswJustifyRight;
    } else {
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (IswPointer)&justify;
    } else if (to->size < sizeof(IswJustify)) {
        to->size = sizeof(IswJustify);
        return False;
    } else {
        *(IswJustify *)to->addr = justify;
    }
    to->size = sizeof(IswJustify);
    
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
    IswDisplay display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    IswPointer *converter_data)
{
    static Widget widget;
    Widget parent;
    const char *name;
    
    (void)display;         /* unused */
    (void)converter_data;  /* unused */
    
    /* Need exactly one argument: the parent widget */
    if (*num_args != 1) {
        IswAppWarningMsg(
            IswWidgetToApplicationContext(*((Widget *)args[0].addr)),
            "wrongParameters", "cvtStringToWidget", "IswToolkitError",
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
    widget = IswNameToWidget(parent, (String)name);
    
    if (widget == (Widget)NULL) {
        /* Widget not found - not an error, may be created later */
        return False;
    }
    
    if (to->addr == NULL) {
        to->addr = (IswPointer)&widget;
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

/* Internal region structure.  Its pointer form is the neutral IswRegion handle
   (ISW/IswTypes.h) and the ISWRegionPtr / Region aliases; the struct tag stays
   _IswRegion.  No value typedef named IswRegion here, so it does not collide
   with the IswRegion handle typedef. */
struct _IswRegion {
    int numRects;
    xcb_rectangle_t rects[XAWREGION_MAXRECTS];
    xcb_rectangle_t extents;  /* Bounding box */
};

/*
 * ISWCreateRegion - Create an empty region
 */
ISWRegionPtr
ISWCreateRegion(void)
{
    ISWRegionPtr region = (ISWRegionPtr)calloc(1, sizeof(struct _IswRegion));
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
ISWUnionRectWithRegion(IswRectangle *rect, ISWRegionPtr source, ISWRegionPtr dest)
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

    /* Add the new rectangle if there's room.  The region stores rectangles in
       its own (backend) representation; copy field-by-field from the neutral
       IswRectangle the caller passed. */
    if (dest->numRects < XAWREGION_MAXRECTS) {
        dest->rects[dest->numRects].x      = rect->x;
        dest->rects[dest->numRects].y      = rect->y;
        dest->rects[dest->numRects].width  = rect->width;
        dest->rects[dest->numRects].height = rect->height;
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
 * =================================================================
 * COLOR UTILITIES
 * =================================================================
 */

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
