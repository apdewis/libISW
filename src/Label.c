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
#include <ISW/ISWSVG.h>
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

/* Shadow resource name definitions (previously from ThreeD.h) */
#define XtNshadowWidth "shadowWidth"
#define XtCShadowWidth "ShadowWidth"
#define XtNtopShadowPixel "topShadowPixel"
#define XtCTopShadowPixel "TopShadowPixel"
#define XtNbottomShadowPixel "bottomShadowPixel"
#define XtCBottomShadowPixel "BottomShadowPixel"
#define XtNtopShadowContrast "topShadowContrast"
#define XtCTopShadowContrast "TopShadowContrast"
#define XtNbottomShadowContrast "bottomShadowContrast"
#define XtCBottomShadowContrast "BottomShadowContrast"
#define XtNrelief "relief"
#define XtCRelief "Relief"
#define XtRRelief "Relief"

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
    {XtNleftBitmap, XtCLeftBitmap, XtRBitmap, sizeof(Pixmap),
       offset(label.left_bitmap), XtRImmediate, (XtPointer) None},
    {XtNbitmap, XtCPixmap, XtRBitmap, sizeof(Pixmap),
	offset(label.pixmap), XtRImmediate, (XtPointer)None},
    {XtNresize, XtCResize, XtRBoolean, sizeof(Boolean),
	offset(label.resize), XtRImmediate, (XtPointer)True},
    {XtNshadowWidth, XtCShadowWidth, XtRDimension, sizeof(Dimension),
	offset(label.shadow_width), XtRImmediate, (XtPointer) 0},
    {XtNtopShadowPixel, XtCTopShadowPixel, XtRPixel, sizeof(Pixel),
	offset(label.top_shadow_pixel), XtRString, XtDefaultForeground},
    {XtNbottomShadowPixel, XtCBottomShadowPixel, XtRPixel, sizeof(Pixel),
	offset(label.bot_shadow_pixel), XtRString, XtDefaultForeground},
    {XtNrelief, XtCRelief, XtRRelief, sizeof(XtRelief),
	offset(label.relief), XtRImmediate, (XtPointer) XtReliefRaised},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
         XtOffsetOf(RectObjRec,rectangle.border_width), XtRImmediate,
         (XtPointer)1},
    {XtNsvgData, XtCSvgData, XtRString, sizeof(String),
	offset(label.svg_data), XtRImmediate, (XtPointer)NULL},
    {XtNsvgFile, XtCSvgFile, XtRString, sizeof(String),
	offset(label.svg_file), XtRImmediate, (XtPointer)NULL},
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
 * Parse SVG from svgFile or svgData resources.
 * Frees any previous SVG state first.
 */
static void
_LabelParseSVG(LabelWidget lw)
{
    float dpi = (float)(96.0 * ISWScaleFactor((Widget)lw));

    /* Free previous state */
    if (lw->label.svg_raster) {
	free(lw->label.svg_raster);
	lw->label.svg_raster = NULL;
	lw->label.svg_raster_w = 0;
	lw->label.svg_raster_h = 0;
    }
    if (lw->label.svg_image) {
	ISWSVGDestroy(lw->label.svg_image);
	lw->label.svg_image = NULL;
    }

    /* Try file first, then inline data */
    if (lw->label.svg_file && lw->label.svg_file[0]) {
	lw->label.svg_image = ISWSVGLoadFile(lw->label.svg_file, "px", dpi);
    } else if (lw->label.svg_data && lw->label.svg_data[0]) {
	lw->label.svg_image = ISWSVGLoadData(lw->label.svg_data, "px", dpi);
    }
}

/*
 * Rasterize the parsed SVG at the label's current dimensions.
 * Called after SetTextWidthAndHeight has set label_width/label_height.
 */
static void
_LabelRasterizeSVG(LabelWidget lw)
{
    unsigned int w, h;

    if (!lw->label.svg_image)
	return;

    w = lw->label.label_width;
    h = lw->label.label_height;
    if (w == 0 || h == 0)
	return;

    /* Skip if cache is still valid */
    if (lw->label.svg_raster &&
	lw->label.svg_raster_w == w && lw->label.svg_raster_h == h)
	return;

    if (lw->label.svg_raster)
	free(lw->label.svg_raster);

    lw->label.svg_raster = ISWSVGRasterize(lw->label.svg_image, w, h);
    lw->label.svg_raster_w = w;
    lw->label.svg_raster_h = h;
}

/*
 * Calculate width and height of displayed text in pixels
 */

static void
SetTextWidthAndHeight(LabelWidget lw)
{
    XFontStruct	*fs = lw->label.font;

    char *nl;

    /* SVG takes priority over pixmap and text */
    if (lw->label.svg_image) {
	float svg_w = ISWSVGGetWidth(lw->label.svg_image);
	float svg_h = ISWSVGGetHeight(lw->label.svg_image);
	double scale = ISWScaleFactor((Widget)lw);
	lw->label.label_width = (Dimension)(svg_w * scale + 0.5);
	lw->label.label_height = (Dimension)(svg_h * scale + 0.5);
	lw->label.label_len = 0;
	lw->label.depth = 32;
	return;
    }

    if (lw->label.pixmap != None) {
 xcb_connection_t *conn = ((Widget)lw)->core.display;
 xcb_get_geometry_cookie_t cookie;
 xcb_get_geometry_reply_t *reply;
 xcb_generic_error_t *error = NULL;

 cookie = xcb_get_geometry(conn, lw->label.pixmap);
 reply = xcb_get_geometry_reply(conn, cookie, &error);
 
 if (reply != NULL) {
     lw->label.label_height = reply->height;
     lw->label.label_width = reply->width;
     lw->label.depth = reply->depth;
     free(reply);
     return;
 }
 if (error) {
     free(error);
 }
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
GetnormalGC(LabelWidget lw)
{
    xcb_create_gc_value_list_t values;
    memset(&values, 0, sizeof(values));

    values.foreground = lw->label.foreground;
    values.background = lw->core.background_pixel;
    values.graphics_exposures = 0;  /* False */

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gc_mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    if (lw->label.font != NULL) {
	values.font = lw->label.font->fid;
	gc_mask |= XCB_GC_FONT;
    }

#ifdef ISW_INTERNATIONALIZATION
    if ( lw->simple.international == True )
        /* Since Xmb/wcDrawString eats the font, I must use XtAllocateGC. */
        lw->label.normal_GC = XtAllocateGC(
                (Widget)lw, 0,
	gc_mask & ~XCB_GC_FONT,  /* Don't include font in mask for international mode */
	&values, XCB_GC_FONT, 0 );
    else
#endif
        lw->label.normal_GC = XtGetGC((Widget)lw, gc_mask, &values);
}

static void
GetgrayGC(LabelWidget lw)
{
    xcb_create_gc_value_list_t values;
    memset(&values, 0, sizeof(values));

    values.foreground = lw->label.foreground;
    values.background = lw->core.background_pixel;
    values.fill_style = XCB_FILL_STYLE_TILED;
    values.tile = IswCreateStippledPixmap(XtDisplay((Widget)lw),
    	  XtWindow((Widget)lw),
    	  lw->label.foreground,
    	  lw->core.background_pixel,
    	  lw->core.depth);
    values.graphics_exposures = 0;  /* False */

    /* XCB Fix: Add NULL check for font before accessing fid */
    XtGCMask gc_mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                       XCB_GC_TILE | XCB_GC_FILL_STYLE | XCB_GC_GRAPHICS_EXPOSURES;
    if (lw->label.font != NULL) {
	values.font = lw->label.font->fid;
	gc_mask |= XCB_GC_FONT;
    }

    lw->label.stipple = values.tile;
#ifdef ISW_INTERNATIONALIZATION
    if ( lw->simple.international == True )
        /* Since Xmb/wcDrawString eats the font, I must use XtAllocateGC. */
        lw->label.gray_GC = XtAllocateGC((Widget)lw,  0,
			gc_mask & ~XCB_GC_FONT,  /* Don't include font in mask for international mode */
			&values, XCB_GC_FONT, 0);
    else
#endif
        lw->label.gray_GC = XtGetGC((Widget)lw, gc_mask, &values);
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
set_bitmap_info (LabelWidget lw)
{
    xcb_connection_t *conn;
    xcb_get_geometry_cookie_t cookie;
    xcb_get_geometry_reply_t *reply;
    xcb_generic_error_t *error = NULL;
    int success = 0;

    if (lw->label.pixmap || !lw->label.left_bitmap) {
	lw->label.lbm_width = lw->label.lbm_height = 0;
    } else {
	conn = ((Widget)lw)->core.display;
	cookie = xcb_get_geometry(conn, lw->label.left_bitmap);
	reply = xcb_get_geometry_reply(conn, cookie, &error);
	
	if (reply != NULL) {
	    lw->label.lbm_width = reply->width;
	    lw->label.lbm_height = reply->height;
	    lw->label.depth = reply->depth;
	    free(reply);
	    success = 1;
	}
	if (error) {
	    free(error);
	}
	if (!success) {
	    lw->label.lbm_width = lw->label.lbm_height = 0;
	}
    }
    compute_bitmap_offsets (lw);
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    LabelWidget lw = (LabelWidget) new;

    /* HiDPI: scale dimension resources */
    lw->label.internal_width = ISWScaleDim(new, lw->label.internal_width);
    lw->label.internal_height = ISWScaleDim(new, lw->label.internal_height);
    if (lw->label.shadow_width > 0)
        lw->label.shadow_width = ISWScaleDim(new, lw->label.shadow_width);

    /* disable shadows if we're not a subclass of Command */
    if (!XtIsSubclass(new, commandWidgetClass))
	lw->label.shadow_width = 0;

    if (lw->label.label == NULL)
        lw->label.label = XtNewString(lw->core.name);
    else
        lw->label.label = XtNewString(lw->label.label);

    /* Copy SVG resource strings and parse */
    if (lw->label.svg_data)
	lw->label.svg_data = XtNewString(lw->label.svg_data);
    if (lw->label.svg_file)
	lw->label.svg_file = XtNewString(lw->label.svg_file);
    lw->label.svg_image = NULL;
    lw->label.svg_raster = NULL;
    lw->label.svg_raster_w = 0;
    lw->label.svg_raster_h = 0;
    _LabelParseSVG(lw);

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

    GetnormalGC(lw);
    GetgrayGC(lw);

    /* Allocate shadow GCs for 3D appearance (if shadow_width > 0) */
    if (lw->label.shadow_width > 0) {
	xcb_create_gc_value_list_t gcv;
	XtGCMask valuemask;
	
	/* Top shadow GC */
	valuemask = GCForeground;
	gcv.foreground = lw->label.top_shadow_pixel;
	lw->label.top_shadow_GC = XtGetGC((Widget)lw, valuemask, &gcv);
	
	/* Bottom shadow GC */
	gcv.foreground = lw->label.bot_shadow_pixel;
	lw->label.bot_shadow_GC = XtGetGC((Widget)lw, valuemask, &gcv);
    } else {
	lw->label.top_shadow_GC = None;
	lw->label.bot_shadow_GC = None;
    }

    /* Initialize render context to NULL (will be created on first use) */
    lw->label.render_ctx = NULL;

    SetTextWidthAndHeight(lw);  /* label.label or label.pixmap */

    if (lw->core.height == 0)
	lw->core.height = lw->label.label_height +
				2 * lw->label.internal_height;

    set_bitmap_info(lw);  /* req's core.height, sets label.lbm_* */

    if (lw->label.lbm_height > lw->label.label_height)
	lw->core.height = lw->label.lbm_height +
				2 * lw->label.internal_height;

    if (lw->core.width == 0)
        lw->core.width = lw->label.label_width +
				2 * lw->label.internal_width +
				LEFT_OFFSET(lw);  /* req's label.lbm_width */

    lw->label.label_x = lw->label.label_y = 0;
    (*XtClass(new)->core_class.resize) ((Widget)lw);

    lw->label.stippled = lw->label.left_stippled = None;
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
    xcb_pixmap_t pm;
    xcb_gcontext_t gc;
    xcb_connection_t *conn = gw->core.display;
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

    gc = XtIsSensitive(gw) ? w->label.normal_GC : w->label.gray_GC;
#ifdef notdef
    if (region != NULL)
	XSetRegion(gw->display, gc, region);
#endif /*notdef*/

    /* SVG rendering path — takes priority over pixmap and text */
    if (w->label.svg_image) {
	_LabelRasterizeSVG(w);
	if (w->label.svg_raster && ctx) {
	    ISWRenderBegin(ctx);
	    ISWRenderDrawImageRGBA(ctx, w->label.svg_raster,
				   w->label.svg_raster_w,
				   w->label.svg_raster_h,
				   w->label.label_x, w->label.label_y,
				   w->label.svg_raster_w,
				   w->label.svg_raster_h);
	    ISWRenderEnd(ctx);
	}
	xcb_flush(conn);
	return;
    }

    if (w->label.pixmap == None) {
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

	/* display left bitmap */
	if (w->label.left_bitmap && w->label.lbm_width != 0) {
	    pm = w->label.left_bitmap;
#ifdef ISW_MULTIPLANE_PIXMAPS
	    if (!XtIsSensitive(gw)) {
		if (w->label.left_stippled == None)
		    w->label.left_stippled = stipplePixmap(gw,
				w->label.left_bitmap, w->core.colormap,
				w->core.background_pixel, w->label.depth);
		if (w->label.left_stippled != None)
		    pm = w->label.left_stippled;
	    }
#endif

	    if (ctx) {
		ISWRenderBegin(ctx);
		ISWRenderSetColor(ctx, w->label.foreground);
		ISWRenderDrawPixmap(ctx, pm, 0, 0,
				    (int) w->label.internal_width,
				    (int) w->label.lbm_y,
				    w->label.lbm_width, w->label.lbm_height,
				    w->label.depth);
		ISWRenderEnd(ctx);
	    }
	}

#ifdef ISW_INTERNATIONALIZATION
        if ( w->simple.international == True ) {

	    ksy += w->label.fontset->ascent;

            /* Use Cairo rendering if available */
            if (ctx) {
                ISWRenderBegin(ctx);
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

    } else {
	pm = w->label.pixmap;
#ifdef ISW_MULTIPLANE_PIXMAPS
	if (!XtIsSensitive(gw)) {
	    if (w->label.stippled == None)
		w->label.stippled = stipplePixmap(gw,
				w->label.pixmap, w->core.colormap,
				w->core.background_pixel, w->label.depth);
	    if (w->label.stippled != None)
		pm = w->label.stippled;
	}
#endif

	if (ctx) {
	    ISWRenderBegin(ctx);
	    ISWRenderSetColor(ctx, w->label.foreground);
	    ISWRenderDrawPixmap(ctx, pm, 0, 0,
				w->label.label_x, w->label.label_y,
				w->label.label_width, w->label.label_height,
				w->label.depth);
	    ISWRenderEnd(ctx);
	}
    }

#ifdef notdef
    if (region != 0)
	/* FIXME: XCB - XSetClipMask needs xcb_change_gc with clip mask */
	/* XSetClipMask(conn, gc, (xcb_pixmap_t)None); */
	(void)gc; /* suppress warning */
#endif /* notdef */
    
    /* Flush XCB commands to server */
    xcb_flush(conn);
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

#define PIXMAP		0
#define WIDTH		1
#define HEIGHT		2
#define NUM_CHECKS	3

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
	if (streq(XtNbitmap, args[i].name))
	    checks[PIXMAP] = TRUE;
	if (streq(XtNwidth, args[i].name))
	    checks[WIDTH] = TRUE;
	if (streq(XtNheight, args[i].name))
	    checks[HEIGHT] = TRUE;
    }

    /* Handle SVG resource changes */
    if (curlw->label.svg_data != newlw->label.svg_data) {
	if (curlw->label.svg_data)
	    XtFree(curlw->label.svg_data);
	newlw->label.svg_data = newlw->label.svg_data ?
	    XtNewString(newlw->label.svg_data) : NULL;
	was_resized = True;
    }
    if (curlw->label.svg_file != newlw->label.svg_file) {
	if (curlw->label.svg_file)
	    XtFree(curlw->label.svg_file);
	newlw->label.svg_file = newlw->label.svg_file ?
	    XtNewString(newlw->label.svg_file) : NULL;
	was_resized = True;
    }
    if (curlw->label.svg_data != newlw->label.svg_data ||
	curlw->label.svg_file != newlw->label.svg_file) {
	/* Invalidate cached raster */
	if (newlw->label.svg_raster) {
	    free(newlw->label.svg_raster);
	    newlw->label.svg_raster = NULL;
	    newlw->label.svg_raster_w = 0;
	    newlw->label.svg_raster_h = 0;
	}
	_LabelParseSVG(newlw);
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

    if (was_resized || checks[PIXMAP] ||
		curlw->label.font != newlw->label.font ||
#ifdef ISW_INTERNATIONALIZATION
		(curlw->simple.international &&
			curlw->label.fontset != newlw->label.fontset) ||
#endif
		curlw->label.encoding != newlw->label.encoding ||
		curlw->label.justify != newlw->label.justify) {
	SetTextWidthAndHeight(newlw);   /* label.label or label.pixmap */
	was_resized = True;
    }

    if (curlw->label.left_bitmap != newlw->label.left_bitmap ||
		curlw->label.internal_width != newlw->label.internal_width ||
		curlw->label.internal_height != newlw->label.internal_height)
	was_resized = True;

    /* recalculate the window size if something has changed. */
    if (newlw->label.resize && was_resized) {
	if (curlw->core.height == reqlw->core.height && !checks[HEIGHT])
	    newlw->core.height = newlw->label.label_height +
				2 * newlw->label.internal_height;

	set_bitmap_info (newlw);  /* req's core.height, sets label.lbm_* */

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
        /* the fontset is not in the GC - no new GC if fontset changes */
 XtReleaseGC(new, curlw->label.normal_GC);
 XtReleaseGC(new, curlw->label.gray_GC);
 ISWReleaseStippledPixmap( XtScreen(current), curlw->label.stipple );
 GetnormalGC(newlw);
 GetgrayGC(newlw);
 redisplay = True;
    }

#ifdef ISW_MULTIPLANE_PIXMAPS
    if (curlw->label.pixmap != newlw->label.pixmap)
    {
	newlw->label.stippled = None;
	if (curlw->label.stippled != None) {
	    xcb_connection_t *conn = current->core.display;
	    xcb_free_pixmap(conn, curlw->label.stippled);
	    xcb_flush(conn);
	}
    }
    if (curlw->label.left_bitmap != newlw->label.left_bitmap)
    {
	newlw->label.left_stippled = None;
	if (curlw->label.left_stippled != None) {
	    xcb_connection_t *conn = current->core.display;
	    xcb_free_pixmap(conn, curlw->label.left_stippled);
	    xcb_flush(conn);
	}
    }
#endif

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
    if (lw->label.svg_raster) {
	free(lw->label.svg_raster);
	lw->label.svg_raster = NULL;
    }
    if (lw->label.svg_image) {
	ISWSVGDestroy(lw->label.svg_image);
	lw->label.svg_image = NULL;
    }
    if (lw->label.svg_data)
	XtFree(lw->label.svg_data);
    if (lw->label.svg_file)
	XtFree(lw->label.svg_file);
    XtReleaseGC( w, lw->label.normal_GC );
    XtReleaseGC( w, lw->label.gray_GC);
    
    /* Release shadow GCs */
    if (lw->label.top_shadow_GC != None)
	XtReleaseGC( w, lw->label.top_shadow_GC );
    if (lw->label.bot_shadow_GC != None)
	XtReleaseGC( w, lw->label.bot_shadow_GC );
    
    /* Free Cairo render context if allocated */
    if (lw->label.render_ctx != NULL) {
	ISWRenderDestroy(lw->label.render_ctx);
	lw->label.render_ctx = NULL;
    }
#ifdef ISW_MULTIPLANE_PIXMAPS
    {
	xcb_connection_t *conn = w->core.display;
	if (lw->label.stippled != None) {
	    xcb_free_pixmap(conn, lw->label.stippled);
	}
	if (lw->label.left_stippled != None) {
	    xcb_free_pixmap(conn, lw->label.left_stippled);
	}
	xcb_flush(conn);
    }
#endif
    ISWReleaseStippledPixmap( XtScreen(w), lw->label.stipple );
}


static XtGeometryResult
QueryGeometry(Widget w, XtWidgetGeometry *intended, XtWidgetGeometry *preferred)
{
    LabelWidget lw = (LabelWidget)w;

    preferred->request_mode = CWWidth | CWHeight;
    preferred->width = (lw->label.label_width +
			    2 * lw->label.internal_width +
			    LEFT_OFFSET(lw));
    preferred->height = lw->label.label_height +
			    2 * lw->label.internal_height;
    if (  ((intended->request_mode & (CWWidth | CWHeight))
	   	== (CWWidth | CWHeight)) &&
	  intended->width == preferred->width &&
	  intended->height == preferred->height)
	return XtGeometryYes;
    else if (preferred->width == w->core.width &&
	     preferred->height == w->core.height)
	return XtGeometryNo;
    else
	return XtGeometryAlmost;
}
