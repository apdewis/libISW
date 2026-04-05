/***********************************************************

Copyright (c) 1987, 1988, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/*
 * Label.c - Label widget
 *
 */

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
/* REMOVED: Xmu headers include Xlib.h which conflicts with XCB-only approach */
/* #include <X11/Xmu/Converters.h> */
/* #include <X11/Xmu/Drawing.h> */
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>
#include <ISW/Command.h>
#include <ISW/LabelP.h>
/* NO XFT - using pure XCB rendering */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>
#include "ISWXcbDraw.h"


/* Forward declarations for Xmu functions that need XCB replacements */
/* XmuCvtStringToJustify signature must match XtConverter */
//extern void XmuCvtStringToJustify(XrmValue*, Cardinal*, XrmValue*, XrmValue*);
//extern xcb_pixmap_t XmuCreateStippledPixmap(xcb_screen_t*, Pixel, Pixel, unsigned int);
//extern void XmuReleaseStippledPixmap(Screen*, xcb_pixmap_t);

/* needed for abs() */
#ifndef X_NOT_STDC_ENV
#include <stdlib.h>
#else
int abs();
#endif

#define streq(a,b) (strcmp( (a), (b) ) == 0)

#define MULTI_LINE_LABEL 32767

#ifdef CRAY
#define WORD64
#endif

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define offset(field) XtOffsetOf(LabelRec, field)
static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(label.foreground), XtRString, XtDefaultForeground},
    {XtNfont,  XtCFont, XtRFontStruct, sizeof(XFontStruct *),
	offset(label.font),XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
    {XtNfontSet,  XtCFontSet, XtRFontSet, sizeof(XFontSet ),
        offset(label.fontset),XtRString, XtDefaultFontSet},
#endif
    {XtNlabel,  XtCLabel, XtRString, sizeof(String),
	offset(label.label), XtRString, NULL},
    {XtNencoding, XtCEncoding, XtRUnsignedChar, sizeof(unsigned char),
	offset(label.encoding), XtRImmediate, (XtPointer)IswTextEncoding8bit},
    {XtNjustify, XtCJustify, XtRJustify, sizeof(XtJustify),
	offset(label.justify), XtRImmediate, (XtPointer)XtJustifyCenter},
    {XtNinternalWidth, XtCWidth, XtRDimension,  sizeof(Dimension),
	offset(label.internal_width), XtRImmediate, (XtPointer)4},
    {XtNinternalHeight, XtCHeight, XtRDimension, sizeof(Dimension),
	offset(label.internal_height), XtRImmediate, (XtPointer)2},
    {XtNleftImage, XtCLeftImage, XtRString, sizeof(String),
       offset(label.left_image_source), XtRImmediate, (XtPointer)NULL},
    {XtNimage, XtCImage, XtRString, sizeof(String),
	offset(label.image_source), XtRImmediate, (XtPointer)NULL},
    {XtNresize, XtCResize, XtRBoolean, sizeof(Boolean),
	offset(label.resize), XtRImmediate, (XtPointer)True},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
         XtOffsetOf(RectObjRec,rectangle.border_width), XtRImmediate,
         (XtPointer)1},
};
#undef offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void ClassInitialize(void);
static void Destroy(Widget);
static XtGeometryResult QueryGeometry(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

LabelClassRec labelClassRec = {
  {
/* core_class fields */
    /* superclass	  	*/	(WidgetClass) &simpleClassRec,
    /* class_name	  	*/	"Label",
    /* widget_size	  	*/	sizeof(LabelRec),
    /* class_initialize   	*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited       	*/	FALSE,
    /* initialize	  	*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize		  	*/	XtInheritRealize,
    /* actions		  	*/	NULL,
    /* num_actions	  	*/	0,
    /* resources	  	*/	resources,
    /* num_resources	  	*/	XtNumber(resources),
    /* xrm_class	  	*/	NULLQUARK,
    /* compress_motion	  	*/	TRUE,
    /* compress_exposure  	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest	  	*/	FALSE,
    /* destroy		  	*/	Destroy,
    /* resize		  	*/	Resize,
    /* expose		  	*/	Redisplay,
    /* set_values	  	*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus	 	*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private   	*/	NULL,
    /* tm_table		   	*/	NULL,
    /* query_geometry		*/	QueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
/* Simple class fields initialization */
  {
    /* change_sensitive		*/	XtInheritChangeSensitive
  },
/* Label class fields initialization */
  {
    /* ignore 			*/	0
  }
};

WidgetClass labelWidgetClass = (WidgetClass)&labelClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    //XtAddConverter( XtRString, XtRJustify, ISWCvtStringToJustify,
	//	    (XtConvertArgList)NULL, 0 );
}

/*
 * 16-bit text support (XChar2b/XTextWidth16) is disabled for XCB migration.
 * The encoding resource should not be set to anything other than 8-bit.
 * Future: Re-implement using Xft/FreeType for proper Unicode support.
 */
#undef WORD64  /* Disable WORD64 16-bit text handling */

/*
 * Convert the widget's foreground pixel to a "#RRGGBB" hex string.
 * Returns True on success, False if the color could not be queried.
 */
static Boolean
_LabelForegroundHex(LabelWidget lw, char *hex, size_t hex_size)
{
    xcb_connection_t *conn = ((Widget)lw)->core.display;
    xcb_colormap_t cmap = ((Widget)lw)->core.colormap;
    uint32_t pixel = (uint32_t)lw->label.foreground;
    xcb_query_colors_cookie_t cookie;
    xcb_query_colors_reply_t *reply;

    hex[0] = '\0';
    cookie = xcb_query_colors(conn, cmap, 1, &pixel);
    reply = xcb_query_colors_reply(conn, cookie, NULL);
    if (reply) {
	xcb_rgb_t *colors = xcb_query_colors_colors(reply);
	if (xcb_query_colors_colors_length(reply) > 0) {
	    snprintf(hex, hex_size, "#%02X%02X%02X",
		     colors[0].red >> 8,
		     colors[0].green >> 8,
		     colors[0].blue >> 8);
	}
	free(reply);
    }
    return hex[0] != '\0';
}

/*
 * Load an ISWImage from a source string using the widget's DPI and foreground.
 */
static ISWImage *
_LabelLoadImage(LabelWidget lw, const char *source)
{
    float dpi = (float)(96.0 * ISWScaleFactor((Widget)lw));
    char fg_hex[8];
    const char *color = _LabelForegroundHex(lw, fg_hex, sizeof(fg_hex)) ? fg_hex : NULL;
    return ISWImageLoad(source, (double)dpi, color);
}

/*
 * Calculate width and height of displayed text in pixels
 */

static void
SetTextWidthAndHeight(LabelWidget lw)
{
    XFontStruct	*fs = lw->label.font;

    char *nl;

    /* image resource takes priority over text */
    if (lw->label.image) {
        double scale = ISWImageIsVector(lw->label.image)
                       ? ISWScaleFactor((Widget)lw) : 1.0;
        lw->label.label_width  = (Dimension)(ISWImageGetWidth(lw->label.image)  * scale + 0.5);
        lw->label.label_height = (Dimension)(ISWImageGetHeight(lw->label.image) * scale + 0.5);
        lw->label.label_len = 0;
        return;
    }
    /*
     * Use ISWScaledTextWidth/ISWScaledFontHeight to measure text the way
     * Cairo will actually render it, so layout matches rendering on HiDPI.
     */
    {
	int line_height = ISWScaledFontHeight((Widget)lw, fs);
	lw->label.label_height = line_height;
	if (lw->label.label == NULL) {
	    lw->label.label_len = 0;
	    lw->label.label_width = 0;
	}
	else if ((nl = index(lw->label.label, '\n')) != NULL) {
	    char *label;
	    lw->label.label_len = MULTI_LINE_LABEL;
	    lw->label.label_width = 0;
	    for (label = lw->label.label; nl != NULL; nl = index(label, '\n')) {
		int width = ISWScaledTextWidth((Widget)lw, fs, label, (int)(nl - label));
		if (width > (int)lw->label.label_width)
		    lw->label.label_width = width;
		label = nl + 1;
		if (*label)
		    lw->label.label_height += line_height;
	    }
	    if (*label) {
		int width = ISWScaledTextWidth((Widget)lw, fs, label, strlen(label));
		if (width > (int)lw->label.label_width)
		    lw->label.label_width = width;
	    }
	} else {
	    lw->label.label_len = strlen(lw->label.label);
	    lw->label.label_width =
		ISWScaledTextWidth((Widget)lw, fs, lw->label.label, (int)lw->label.label_len);
	}
    }
}

static void
compute_bitmap_offsets (LabelWidget lw)
{
    if (lw->label.lbm_height != 0)
	lw->label.lbm_y = (lw->core.height - lw->label.lbm_height) / 2;
    else
	lw->label.lbm_y = 0;
}

static void
set_left_image_info(LabelWidget lw)
{
    if (lw->label.left_image) {
        double scale = ISWImageIsVector(lw->label.left_image)
                       ? ISWScaleFactor((Widget)lw) : 1.0;
        lw->label.lbm_width  = (unsigned int)(ISWImageGetWidth(lw->label.left_image)  * scale + 0.5);
        lw->label.lbm_height = (unsigned int)(ISWImageGetHeight(lw->label.left_image) * scale + 0.5);
    } else {
        lw->label.lbm_width = lw->label.lbm_height = 0;
    }
    compute_bitmap_offsets(lw);
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    LabelWidget lw = (LabelWidget) new;

    /* HiDPI: scale dimension resources */
    lw->label.internal_width = ISWScaleDim(new, lw->label.internal_width);
    lw->label.internal_height = ISWScaleDim(new, lw->label.internal_height);

    if (lw->label.label == NULL)
        lw->label.label = XtNewString(lw->core.name);
    else
        lw->label.label = XtNewString(lw->label.label);

    /* Load images from string resources */
    lw->label.image_source = lw->label.image_source
        ? XtNewString(lw->label.image_source) : NULL;
    lw->label.left_image_source = lw->label.left_image_source
        ? XtNewString(lw->label.left_image_source) : NULL;
    lw->label.image = lw->label.image_source
        ? _LabelLoadImage(lw, lw->label.image_source) : NULL;
    lw->label.left_image = lw->label.left_image_source
        ? _LabelLoadImage(lw, lw->label.left_image_source) : NULL;

    /* XCB Fix: XtRFontStruct converter may fail in XCB mode, leaving font NULL.
     * If font is NULL but fontset is available, create a minimal XFontStruct
     * using the fontset's font_id (similar to MultiSink.c approach). */
    if (lw->label.font == NULL) {
#ifdef ISW_INTERNATIONALIZATION
 if (lw->label.fontset != NULL) {
     /* Allocate and initialize a minimal XFontStruct from fontset */
     lw->label.font = (XFontStruct *)XtMalloc(sizeof(XFontStruct));
     memset(lw->label.font, 0, sizeof(XFontStruct));
     lw->label.font->fid = lw->label.fontset->font_id;
     lw->label.font->min_char_or_byte2 = 0;
     lw->label.font->max_char_or_byte2 = 255;
 } else
#endif
 {
     /* Both font and fontset are NULL - load fallback font */
     fprintf(stderr, "WARNING Label.c: Both font and fontset are NULL for widget '%s'\n",
             XtName(new));
     fprintf(stderr, "         Attempting to load fallback font...\n");
     
     lw->label.font = ISWLoadFallbackFont(XtDisplay(new));
     
     if (lw->label.font == NULL) {
         fprintf(stderr, "FATAL Label.c: Fallback font loading failed for widget '%s'\n",
                 XtName(new));
         XtAppError(XtWidgetToApplicationContext(new),
                    "Label widget: Both font resource converters failed AND fallback font loading failed");
     } else {
         fprintf(stderr, "SUCCESS Label.c: Fallback font loaded with fid=%lu\n",
                 (unsigned long)lw->label.font->fid);
     }
 }
    } else {
    }

    /* Initialize render context to NULL (will be created on first use) */
    lw->label.render_ctx = NULL;

    SetTextWidthAndHeight(lw);  /* label.label or label.pixmap */

    if (lw->core.height == 0)
	lw->core.height = lw->label.label_height +
				2 * lw->label.internal_height;

    set_left_image_info(lw);  /* req's core.height, sets label.lbm_* */

    if (lw->label.lbm_height > lw->label.label_height)
	lw->core.height = lw->label.lbm_height +
				2 * lw->label.internal_height;

    if (lw->core.width == 0)
        lw->core.width = lw->label.label_width +
				2 * lw->label.internal_width +
				LEFT_OFFSET(lw);  /* req's label.lbm_width */

    lw->label.label_x = lw->label.label_y = 0;
    (*XtClass(new)->core_class.resize) ((Widget)lw);

} /* Initialize */

/*
 * Repaint the widget window
 */

/* ARGSUSED */
static void
Redisplay(Widget gw, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    LabelWidget w = (LabelWidget) gw;
    LabelWidgetClass lwclass = (LabelWidgetClass) XtClass (gw);
    ISWRenderContext *ctx = w->label.render_ctx;  /* Cairo rendering context */
    
    /* Create render context on first use (lazy initialization) */
    if (!ctx && gw->core.width > 0 && gw->core.height > 0) {
        ctx = w->label.render_ctx = ISWRenderCreate(gw, ISW_RENDER_BACKEND_AUTO);
    }
    
    /* Note: event and region use XCB types per the migration plan */
    (void)event; /* May be used in future for expose event handling */

    /*
     * Don't draw shadows if Command is going to redraw them.
     * The shadow draw method is region aware, but since 99% of
     * all labels don't have shadows, we'll check for a shadow
     * before we incur the function call overhead.
     *
     * Shadow drawing removed - ThreeD eliminated.
     */

    /*
     * now we'll see if we need to draw the rest of the label
     */
    /* Region handling: xcb_xfixes_region_t is a scalar (0 = no region) */
    if (region != 0) {
	int x = w->label.label_x;
	unsigned int width = w->label.label_width;
	if (w->label.lbm_width) {
	    if (w->label.label_x > (x = w->label.internal_width))
		width += w->label.label_x - x;
	}
	/* XRectInRegion not available in XCB - would need xcb_xfixes_fetch_region
	 * For now, always redraw (no early return optimization) */
	(void)x; (void)width; (void)region; /* suppress warnings */
    }


    /* Image rendering path (replaces text/pixmap) */
    if (w->label.image) {
        unsigned int disp_w = w->label.label_width;
        unsigned int disp_h = w->label.label_height;

        /* Clamp to available widget space */
        unsigned int avail_w = w->core.width > 2 * w->label.internal_width
            ? w->core.width - 2 * w->label.internal_width : 0;
        unsigned int avail_h = w->core.height > 2 * w->label.internal_height
            ? w->core.height - 2 * w->label.internal_height : 0;
        if (avail_w > 0 && avail_h > 0 && (disp_w != avail_w || disp_h != avail_h)) {
            float sw = (float)avail_w / disp_w;
            float sh = (float)avail_h / disp_h;
            float s  = sw < sh ? sw : sh;
            disp_w = (unsigned int)(disp_w * s + 0.5f);
            disp_h = (unsigned int)(disp_h * s + 0.5f);
            if (disp_w == 0) disp_w = 1;
            if (disp_h == 0) disp_h = 1;
        }

        unsigned int rw, rh;
        const unsigned char *pixels = ISWImageRasterize(w->label.image,
            disp_w, disp_h, &rw, &rh);
        if (pixels && ctx) {
            int draw_x = (int)(w->core.width  - disp_w) / 2;
            int draw_y = (int)(w->core.height - disp_h) / 2;
            if (draw_x < 0) draw_x = 0;
            if (draw_y < 0) draw_y = 0;
            ISWRenderBegin(ctx);
            ISWRenderSetColor(ctx, w->core.background_pixel);
            ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
            ISWRenderDrawImageRGBA(ctx, pixels, rw, rh,
                                   draw_x, draw_y, disp_w, disp_h);
            ISWRenderEnd(ctx);
        }
        return;
    }

    {
 int len = w->label.label_len;
 char *label = w->label.label;
 Position y = w->label.label_y;
 XFontStruct *fs = w->label.font;

 if (fs == NULL) {
     /* No font available - skip text rendering */
     return;
 }

 /* Use Cairo's actual font metrics for baseline and line height */
 int line_height = ISWScaledFontHeight((Widget)w, fs);
 y += (Position)ISWScaledFontAscent((Widget)w, fs);

#ifdef ISW_INTERNATIONALIZATION
 Position ksy = w->label.label_y;
 if (w->simple.international == True)
     ksy += (Position)ISWScaledFontAscent((Widget)w, fs);
#endif

	/* display left image */
	if (w->label.left_image && w->label.lbm_width != 0) {
	    unsigned int rw, rh;
	    const unsigned char *pixels = ISWImageRasterize(w->label.left_image,
	        w->label.lbm_width, w->label.lbm_height, &rw, &rh);
	    if (pixels && ctx) {
		ISWRenderBegin(ctx);
		ISWRenderDrawImageRGBA(ctx, pixels, rw, rh,
				       (int)w->label.internal_width,
				       (int)w->label.lbm_y,
				       w->label.lbm_width,
				       w->label.lbm_height);
		ISWRenderEnd(ctx);
	    }
	}

#ifdef ISW_INTERNATIONALIZATION
        if ( w->simple.international == True ) {

	    ksy += w->label.fontset->ascent;

            /* Use Cairo rendering if available */
            if (ctx) {
                ISWRenderBegin(ctx);
                /* Clear the widget area before drawing so stale text
                 * from a previous label value doesn't show through. */
                ISWRenderSetColor(ctx, w->core.background_pixel);
                ISWRenderFillRectangle(ctx, 0, 0,
                                       w->core.width, w->core.height);
                ISWRenderSetFont(ctx, w->label.font);
                ISWRenderSetColor(ctx, w->label.foreground);

                if (len == MULTI_LINE_LABEL) {
                    char *nl;
                    while ((nl = index(label, '\n')) != NULL) {
                        ISWRenderDrawString(ctx, label, (int)(nl - label),
                                          w->label.label_x, ksy);
                        ksy += line_height;
                        label = nl + 1;
                    }
                    len = strlen(label);
                }
                if (len)
                    ISWRenderDrawString(ctx, label, len,
                                      w->label.label_x, ksy);

                ISWRenderEnd(ctx);
            }

        } else
#endif
        { /* international false, so use XCB core font rendering */

            /* Use Cairo rendering if available */
            if (ctx) {
                ISWRenderBegin(ctx);
                /* Clear the widget area before drawing so stale text
                 * from a previous label value doesn't show through. */
                ISWRenderSetColor(ctx, w->core.background_pixel);
                ISWRenderFillRectangle(ctx, 0, 0,
                                       w->core.width, w->core.height);
                ISWRenderSetFont(ctx, w->label.font);
                ISWRenderSetColor(ctx, w->label.foreground);

                if (len == MULTI_LINE_LABEL) {
                    char *nl;
                    while ((nl = index(label, '\n')) != NULL) {
                        int segment_len = (int)(nl - label);
                        if (segment_len > 0) {
                            ISWRenderDrawString(ctx, label, segment_len,
                                              w->label.label_x, y);
                        }
                        y += line_height;
                        label = nl + 1;
                    }
                    len = strlen(label);
                }
                if (len) {
                    ISWRenderDrawString(ctx, label, len,
                                      w->label.label_x, y);
                }

                ISWRenderEnd(ctx);
            }

        } /* endif international */

    }

    /* Draw per-edge borders (on top of content) */
    if (ctx &&
        (gw->core.border_width_top != 0 || gw->core.border_width_right != 0 ||
         gw->core.border_width_bottom != 0 || gw->core.border_width_left != 0)) {
        ISWRenderBegin(ctx);
        ISWRenderDrawBorder(ctx,
            gw->core.border_width_top, gw->core.border_width_right,
            gw->core.border_width_bottom, gw->core.border_width_left,
            gw->core.border_pixel_top, gw->core.border_pixel_right,
            gw->core.border_pixel_bottom, gw->core.border_pixel_left,
            gw->core.width, gw->core.height);
        ISWRenderEnd(ctx);
    }
}

static void
_Reposition(LabelWidget lw, Dimension width, Dimension height,
            Position *dx, Position *dy)
{
    Position newPos;
    Position leftedge = lw->label.internal_width + LEFT_OFFSET(lw);

    switch (lw->label.justify) {
	case XtJustifyLeft:
	    newPos = leftedge;
	    break;
	case XtJustifyRight:
	    newPos = width - lw->label.label_width - lw->label.internal_width;
	    break;
	case XtJustifyCenter:
	default:
	    newPos = (int)(width - lw->label.label_width) / 2;
	    break;
    }

    if (newPos < (Position)leftedge)
	newPos = leftedge;
    *dx = newPos - lw->label.label_x;
    lw->label.label_x = newPos;

    *dy = (newPos = (int)(height - lw->label.label_height) / 2)
	  - lw->label.label_y;
    lw->label.label_y = newPos;

    lw->label.lbm_y = (height - lw->label.lbm_height) / 2;

    return;
}

static void
Resize(Widget w)
{
    LabelWidget lw = (LabelWidget)w;
    Position dx, dy;

    _Reposition(lw, w->core.width, w->core.height, &dx, &dy);
    compute_bitmap_offsets (lw);
}

/*
 * Set specified arguments into widget
 */

#define WIDTH		0
#define HEIGHT		1
#define NUM_CHECKS	2

static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    LabelWidget curlw = (LabelWidget) current;
    LabelWidget reqlw = (LabelWidget) request;
    LabelWidget newlw = (LabelWidget) new;
    int i;
    Boolean was_resized = False, redisplay = False, checks[NUM_CHECKS];

    for (i = 0; i < NUM_CHECKS; i++)
	checks[i] = FALSE;
    for (i = 0; i < *num_args; i++) {
	if (streq(XtNwidth, args[i].name))
	    checks[WIDTH] = TRUE;
	if (streq(XtNheight, args[i].name))
	    checks[HEIGHT] = TRUE;
    }

    /* Handle image resource changes */
    if (curlw->label.image_source != newlw->label.image_source) {
	if (curlw->label.image_source)
	    XtFree(curlw->label.image_source);
	newlw->label.image_source = newlw->label.image_source
	    ? XtNewString(newlw->label.image_source) : NULL;
	if (newlw->label.image) {
	    ISWImageDestroy(newlw->label.image);
	    newlw->label.image = NULL;
	}
	if (newlw->label.image_source)
	    newlw->label.image = _LabelLoadImage(newlw, newlw->label.image_source);
	was_resized = True;
    }
    if (curlw->label.left_image_source != newlw->label.left_image_source) {
	if (curlw->label.left_image_source)
	    XtFree(curlw->label.left_image_source);
	newlw->label.left_image_source = newlw->label.left_image_source
	    ? XtNewString(newlw->label.left_image_source) : NULL;
	if (newlw->label.left_image) {
	    ISWImageDestroy(newlw->label.left_image);
	    newlw->label.left_image = NULL;
	}
	if (newlw->label.left_image_source)
	    newlw->label.left_image = _LabelLoadImage(newlw, newlw->label.left_image_source);
	was_resized = True;
    }

    if (newlw->label.label == NULL)
	newlw->label.label = newlw->core.name;
    if (curlw->label.label != newlw->label.label) {
        if (curlw->label.label != curlw->core.name)
	    XtFree((char *)curlw->label.label);
	if (newlw->label.label != newlw->core.name)
	    newlw->label.label = XtNewString(newlw->label.label);
	was_resized = True;
    }

    if (was_resized ||
		curlw->label.font != newlw->label.font ||
#ifdef ISW_INTERNATIONALIZATION
		(curlw->simple.international &&
			curlw->label.fontset != newlw->label.fontset) ||
#endif
		curlw->label.encoding != newlw->label.encoding ||
		curlw->label.justify != newlw->label.justify) {
	SetTextWidthAndHeight(newlw);
	was_resized = True;
    }

    if (curlw->label.left_image != newlw->label.left_image ||
		curlw->label.internal_width != newlw->label.internal_width ||
		curlw->label.internal_height != newlw->label.internal_height)
	was_resized = True;

    /* recalculate the window size if something has changed. */
    if (newlw->label.resize && was_resized) {
	if (curlw->core.height == reqlw->core.height && !checks[HEIGHT])
	    newlw->core.height = newlw->label.label_height +
				2 * newlw->label.internal_height;

	set_left_image_info(newlw);  /* req's core.height, sets label.lbm_* */

	if (newlw->label.lbm_height > newlw->label.label_height)
	    newlw->core.height = newlw->label.lbm_height +
					2 * newlw->label.internal_height;

	if (curlw->core.width == reqlw->core.width && !checks[WIDTH])
	    newlw->core.width = newlw->label.label_width +
				2 * newlw->label.internal_width +
				LEFT_OFFSET(newlw);  /* req's label.lbm_width */
    }

    /* enforce minimum dimensions */
    if (newlw->label.resize) {
	if (checks[HEIGHT]) {
	    if (newlw->label.label_height > newlw->label.lbm_height)
		i = newlw->label.label_height +
			2 * newlw->label.internal_height;
	    else
		i = newlw->label.lbm_height + 2 * newlw->label.internal_height;
	    if (i > newlw->core.height)
		newlw->core.height = i;
	}
	if (checks[WIDTH]) {
	    i = newlw->label.label_width + 2 * newlw->label.internal_width +
			LEFT_OFFSET(newlw);  /* req's label.lbm_width */
	    if (i > newlw->core.width)
		newlw->core.width = i;
	}
    }

    /* XCB Fix: Add NULL checks before comparing font->fid */
    Bool font_changed = False;
    if (curlw->label.font != NULL && newlw->label.font != NULL) {
 font_changed = (curlw->label.font->fid != newlw->label.font->fid);
    } else if (curlw->label.font != newlw->label.font) {
 /* One is NULL and the other isn't */
 font_changed = True;
    }

    if (curlw->core.background_pixel != newlw->core.background_pixel ||
  curlw->label.foreground != newlw->label.foreground ||
  font_changed) {
 redisplay = True;
 /* Recolor SVG images if foreground changed */
 if (curlw->label.foreground != newlw->label.foreground) {
     char fg_hex[8];
     const char *color = _LabelForegroundHex(newlw, fg_hex, sizeof(fg_hex))
                         ? fg_hex : NULL;
     if (newlw->label.image)
         ISWImageRecolor(newlw->label.image, color);
     if (newlw->label.left_image)
         ISWImageRecolor(newlw->label.left_image, color);
 }
    }


    if (was_resized) {
	Position dx, dy;

	/* Resize() will be called if geometry changes succeed */
	_Reposition(newlw, curlw->core.width, curlw->core.height, &dx, &dy);
    }

    return was_resized || redisplay ||
	   XtIsSensitive(current) != XtIsSensitive(new);
}

static void
Destroy(Widget w)
{
    LabelWidget lw = (LabelWidget)w;

    if ( lw->label.label != lw->core.name )
	XtFree( lw->label.label );
    if (lw->label.image_source)
	XtFree(lw->label.image_source);
    if (lw->label.left_image_source)
	XtFree(lw->label.left_image_source);
    if (lw->label.image) {
	ISWImageDestroy(lw->label.image);
	lw->label.image = NULL;
    }
    if (lw->label.left_image) {
	ISWImageDestroy(lw->label.left_image);
	lw->label.left_image = NULL;
    }
    /* Free Cairo render context if allocated */
    if (lw->label.render_ctx != NULL) {
	ISWRenderDestroy(lw->label.render_ctx);
	lw->label.render_ctx = NULL;
    }
}


static XtGeometryResult
QueryGeometry(Widget w, XtWidgetGeometry *intended, XtWidgetGeometry *preferred)
{
    LabelWidget lw = (LabelWidget)w;

    preferred->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    preferred->width = (lw->label.label_width +
			    2 * lw->label.internal_width +
			    LEFT_OFFSET(lw));
    preferred->height = lw->label.label_height +
			    2 * lw->label.internal_height;
    if (  ((intended->request_mode & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
	   	== (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)) &&
	  intended->width == preferred->width &&
	  intended->height == preferred->height)
	return XtGeometryYes;
    else if (preferred->width == w->core.width &&
	     preferred->height == w->core.height)
	return XtGeometryNo;
    else
	return XtGeometryAlmost;
}
