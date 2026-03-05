/***********************************************************
Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.
Copyright 1992, 1993 by Kaleb Keithley

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Digital, MIT, or Kaleb
Keithley not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

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
#include <X11/Xaw3d/Xaw3dP.h>
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/Xaw3d/XawInit.h>
#include <X11/Xaw3d/ThreeDP.h>
#include <X11/Xaw3d/SimpleMenP.h>
#include <X11/Xaw3d/SmeThreeDP.h>
#include <X11/Xosdefs.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "XawXcbDraw.h"

/* Initialization of defaults */

#define XtNtopShadowPixmap "topShadowPixmap"
#define XtCTopShadowPixmap "TopShadowPixmap"
#define XtNbottomShadowPixmap "bottomShadowPixmap"
#define XtCBottomShadowPixmap "BottomShadowPixmap"
#define XtNshadowed "shadowed"
#define XtCShadowed "Shadowed"

#define offset(field) XtOffsetOf(SmeThreeDRec, field)

static XtResource resources[] = {
    {XtNshadowWidth, XtCShadowWidth, XtRDimension, sizeof(Dimension),
	offset(sme_threeD.shadow_width), XtRImmediate, (XtPointer) 2},
    {XtNtopShadowPixel, XtCTopShadowPixel, XtRPixel, sizeof(Pixel),
	offset(sme_threeD.top_shadow_pixel), XtRString, XtDefaultForeground},
    {XtNbottomShadowPixel, XtCBottomShadowPixel, XtRPixel, sizeof(Pixel),
	offset(sme_threeD.bot_shadow_pixel), XtRString, XtDefaultForeground},
    {XtNtopShadowPixmap, XtCTopShadowPixmap, XtRPixmap, sizeof(Pixmap),
	offset(sme_threeD.top_shadow_pxmap), XtRImmediate, (XtPointer) NULL},
    {XtNbottomShadowPixmap, XtCBottomShadowPixmap, XtRPixmap, sizeof(Pixmap),
	offset(sme_threeD.bot_shadow_pxmap), XtRImmediate, (XtPointer) NULL},
    {XtNtopShadowContrast, XtCTopShadowContrast, XtRInt, sizeof(int),
	offset(sme_threeD.top_shadow_contrast), XtRImmediate, (XtPointer) 20},
    {XtNbottomShadowContrast, XtCBottomShadowContrast, XtRInt, sizeof(int),
	offset(sme_threeD.bot_shadow_contrast), XtRImmediate, (XtPointer) 40},
    {XtNuserData, XtCUserData, XtRPointer, sizeof(XtPointer),
	offset(sme_threeD.user_data), XtRPointer, (XtPointer) NULL},
    {XtNbeNiceToColormap, XtCBeNiceToColormap, XtRBoolean, sizeof(Boolean),
	offset(sme_threeD.be_nice_to_cmap), XtRImmediate, (XtPointer) True},
    {XtNshadowed, XtCShadowed, XtRBoolean, sizeof(Boolean),
	offset(sme_threeD.shadowed), XtRImmediate, (XtPointer) False},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
	XtOffsetOf(RectObjRec,rectangle.border_width), XtRImmediate,
	(XtPointer)0}
};

#undef offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void ClassPartInitialize(WidgetClass);
static void _XawSme3dDrawShadows(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

SmeThreeDClassRec smeThreeDClassRec = {
    { /* core fields */
    /* superclass		*/	(WidgetClass) &smeClassRec,
    /* class_name		*/	"SmeThreeD",
    /* widget_size		*/	sizeof(SmeThreeDRec),
    /* class_initialize		*/	NULL,
    /* class_part_initialize	*/	ClassPartInitialize,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	NULL,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* resource_count		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	XtInheritResize,
    /* expose			*/	NULL,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry           */	XtInheritQueryGeometry,
    /* display_accelerator      */	NULL,
    /* extension                */	NULL
    },
    { /* Menu Entry fields */
    /* highlight                */      XtInheritHighlight,
    /* unhighlight              */      XtInheritUnhighlight,
    /* notify                   */      XtInheritNotify,
    /* extension                */      NULL
    },
    { /* threeD fields */
    /* shadow draw              */      _XawSme3dDrawShadows
    }
};

WidgetClass smeThreeDObjectClass = (WidgetClass) &smeThreeDClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/


#define shadowpm_width 8
#define shadowpm_height 8
static char shadowpm_bits[] = {
    0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55};

static char mtshadowpm_bits[] = {
    0x92, 0x24, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24};

static char mbshadowpm_bits[] = {
    0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0xdb};

/* ARGSUSED */
static void
AllocTopShadowGC (Widget w)
{
    SmeThreeDObject	tdo = (SmeThreeDObject) w;
    xcb_screen_t	*scn = XtScreenOfObject (w);
    XtGCMask		valuemask;
    xcb_create_gc_value_list_t	myXGCV;

    memset(&myXGCV, 0, sizeof(myXGCV));
    if (tdo->sme_threeD.be_nice_to_cmap || scn->root_depth == 1) {
 valuemask = XCB_GC_TILE | XCB_GC_FILL_STYLE;
 myXGCV.tile = tdo->sme_threeD.top_shadow_pxmap;
 myXGCV.fill_style = XCB_FILL_STYLE_TILED;
    } else {
 valuemask = XCB_GC_FOREGROUND;
 myXGCV.foreground = tdo->sme_threeD.top_shadow_pixel;
    }
    tdo->sme_threeD.top_shadow_GC = XtGetGC(w, valuemask, (xcb_create_gc_value_list_t*)&myXGCV);
}

/* ARGSUSED */
static void
AllocBotShadowGC (Widget w)
{
    SmeThreeDObject	tdo = (SmeThreeDObject) w;
    xcb_screen_t	*scn = XtScreenOfObject (w);
    XtGCMask		valuemask;
    xcb_create_gc_value_list_t	myXGCV;

    memset(&myXGCV, 0, sizeof(myXGCV));
    if (tdo->sme_threeD.be_nice_to_cmap || scn->root_depth == 1) {
	valuemask = XCB_GC_TILE | XCB_GC_FILL_STYLE;
	myXGCV.tile = tdo->sme_threeD.bot_shadow_pxmap;
	myXGCV.fill_style = XCB_FILL_STYLE_TILED;
    } else {
	valuemask = XCB_GC_FOREGROUND;
	myXGCV.foreground = tdo->sme_threeD.bot_shadow_pixel;
    }
    tdo->sme_threeD.bot_shadow_GC = XtGetGC(w, valuemask, (xcb_create_gc_value_list_t*)&myXGCV);
}

/* ARGSUSED */
static void
AllocEraseGC (Widget w)
{
    Widget		parent = XtParent (w);
    SmeThreeDObject	tdo = (SmeThreeDObject) w;
    XtGCMask		valuemask;
    xcb_create_gc_value_list_t	myXGCV;

    memset(&myXGCV, 0, sizeof(myXGCV));
    valuemask = XCB_GC_FOREGROUND;
    myXGCV.foreground = parent->core.background_pixel;
    tdo->sme_threeD.erase_GC = XtGetGC(w, valuemask, (xcb_create_gc_value_list_t*)&myXGCV);
}

/* ARGSUSED */
static void
AllocTopShadowPixmap (Widget new)
{
    SmeThreeDObject	tdo = (SmeThreeDObject) new;
    Widget		parent = XtParent (new);
    xcb_connection_t	*dpy = XtDisplayOfObject (new);
    xcb_screen_t	*scn = XtScreenOfObject (new);
    unsigned long	top_fg_pixel = 0, top_bg_pixel = 0;
    char		*pm_data;
    Boolean		create_pixmap = FALSE;

    /*
     * I know, we're going to create two pixmaps for each and every
     * shadow'd widget.  Yeuck.  I'm semi-relying on server side
     * pixmap cacheing.
     */

    if (scn->root_depth == 1) {
	top_fg_pixel = scn->black_pixel;
	top_bg_pixel = scn->white_pixel;
	pm_data = mtshadowpm_bits;
	create_pixmap = TRUE;
    } else if (tdo->sme_threeD.be_nice_to_cmap) {
	if (parent->core.background_pixel == scn->white_pixel) {
	    top_fg_pixel = scn->white_pixel;
	    top_bg_pixel = grayPixel( scn->black_pixel, dpy, scn);
	} else if (parent->core.background_pixel == scn->black_pixel) {
	    top_fg_pixel = grayPixel( scn->black_pixel, dpy, scn);
	    top_bg_pixel = scn->white_pixel;
	} else {
	    top_fg_pixel = parent->core.background_pixel;
	    top_bg_pixel = scn->white_pixel;
	}
#ifndef XAW_GRAY_BLKWHT_STIPPLES
	if (parent->core.background_pixel == scn->white_pixel ||
	    parent->core.background_pixel == scn->black_pixel) {
	    pm_data = mtshadowpm_bits;
       } else
#endif
	    pm_data = shadowpm_bits;

	create_pixmap = TRUE;
    }

    if (create_pixmap)
	tdo->sme_threeD.top_shadow_pxmap = XawCreateStippledPixmap(dpy,
			scn->root,
			top_fg_pixel,
			top_bg_pixel,
			scn->root_depth);
}

/* ARGSUSED */
static void
AllocBotShadowPixmap (Widget new)
{
    SmeThreeDObject	tdo = (SmeThreeDObject) new;
    Widget		parent = XtParent (new);
    xcb_connection_t	*dpy = XtDisplayOfObject (new);
    xcb_screen_t	*scn = XtScreenOfObject (new);
    unsigned long	bot_fg_pixel = 0, bot_bg_pixel = 0;
    char		*pm_data;
    Boolean		create_pixmap = FALSE;

    if (scn->root_depth == 1) {
	bot_fg_pixel = scn->black_pixel;
	bot_bg_pixel = scn->white_pixel;
	pm_data = mbshadowpm_bits;
	create_pixmap = TRUE;
    } else if (tdo->sme_threeD.be_nice_to_cmap) {
	if (parent->core.background_pixel == scn->white_pixel) {
	    bot_fg_pixel = grayPixel( scn->white_pixel, dpy, scn);
	    bot_bg_pixel = scn->black_pixel;
	} else if (parent->core.background_pixel == scn->black_pixel) {
	    bot_fg_pixel = scn->black_pixel;
	    bot_bg_pixel = grayPixel( scn->black_pixel, dpy, scn);
	} else {
	    bot_fg_pixel = parent->core.background_pixel;
	    bot_bg_pixel = scn->black_pixel;
	}
#ifndef XAW_GRAY_BLKWHT_STIPPLES
	if (parent->core.background_pixel == scn->white_pixel ||
	    parent->core.background_pixel == scn->black_pixel) {
	    pm_data = mbshadowpm_bits;
	} else
#endif
	    pm_data = shadowpm_bits;

	create_pixmap = TRUE;
    }

    if (create_pixmap)
	tdo->sme_threeD.bot_shadow_pxmap = XawCreateStippledPixmap(dpy,
			scn->root,
			bot_fg_pixel,
			bot_bg_pixel,
			scn->root_depth);
}

/* ARGSUSED */
void
XawSme3dComputeTopShadowRGB (Widget new, XColor *xcol_out)
{
    if (XtIsSubclass (new, smeThreeDObjectClass)) {
	SmeThreeDObject tdo = (SmeThreeDObject) new;
	Widget w = XtParent (new);
	double contrast;
	xcb_connection_t *dpy = XtDisplayOfObject (new);
	xcb_screen_t *scn = XtScreenOfObject (new);
	Colormap cmap = w->core.colormap;
	uint32_t pixel = w->core.background_pixel;

	if (pixel == scn->white_pixel || pixel == scn->black_pixel) {
	    contrast = (100 - tdo->sme_threeD.top_shadow_contrast) / 100.0;
	    xcol_out->red   = contrast * 65535.0;
	    xcol_out->green = contrast * 65535.0;
	    xcol_out->blue  = contrast * 65535.0;
	} else {
	    xcb_query_colors_cookie_t cookie = xcb_query_colors(dpy, cmap, 1, &pixel);
	    xcb_query_colors_reply_t *reply = xcb_query_colors_reply(dpy, cookie, NULL);
	    if (reply) {
		xcb_rgb_t *rgb = xcb_query_colors_colors(reply);
		contrast = 1.0 + tdo->sme_threeD.top_shadow_contrast / 100.0;
#define MIN(x,y) (unsigned short) (x < y) ? x : y
		xcol_out->red   = MIN (65535, (int) (contrast * (double) rgb->red));
		xcol_out->green = MIN (65535, (int) (contrast * (double) rgb->green));
		xcol_out->blue  = MIN (65535, (int) (contrast * (double) rgb->blue));
#undef MIN
		free(reply);
	    } else {
		xcol_out->red = xcol_out->green = xcol_out->blue = 0;
	    }
	}
    } else
	xcol_out->red = xcol_out->green = xcol_out->blue = 0;
}

/* ARGSUSED */
static void
AllocTopShadowPixel (Widget new)
{
    XColor set_c;
    SmeThreeDObject tdo = (SmeThreeDObject) new;
    Widget w = XtParent (new);
    xcb_connection_t *dpy = XtDisplayOfObject (new);
    Colormap cmap = w->core.colormap;

    XawSme3dComputeTopShadowRGB (new, &set_c);
    xcb_alloc_color_cookie_t cookie = xcb_alloc_color(dpy, cmap, set_c.red, set_c.green, set_c.blue);
    xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(dpy, cookie, NULL);
    if (reply) {
	tdo->sme_threeD.top_shadow_pixel = reply->pixel;
	free(reply);
    } else {
	tdo->sme_threeD.top_shadow_pixel = w->core.background_pixel;
    }
}


/* ARGSUSED */
void
XawSme3dComputeBottomShadowRGB (Widget new, XColor *xcol_out)
{
    if (XtIsSubclass (new, smeThreeDObjectClass)) {
	SmeThreeDObject tdo = (SmeThreeDObject) new;
	Widget w = XtParent (new);
	double contrast;
	xcb_connection_t *dpy = XtDisplayOfObject (new);
	xcb_screen_t *scn = XtScreenOfObject (new);
	Colormap cmap = w->core.colormap;
	uint32_t pixel = w->core.background_pixel;

	if (pixel == scn->white_pixel || pixel == scn->black_pixel) {
	    contrast = tdo->sme_threeD.bot_shadow_contrast / 100.0;
	    xcol_out->red   = contrast * 65535.0;
	    xcol_out->green = contrast * 65535.0;
	    xcol_out->blue  = contrast * 65535.0;
	} else {
	    xcb_query_colors_cookie_t cookie = xcb_query_colors(dpy, cmap, 1, &pixel);
	    xcb_query_colors_reply_t *reply = xcb_query_colors_reply(dpy, cookie, NULL);
	    if (reply) {
		xcb_rgb_t *rgb = xcb_query_colors_colors(reply);
		contrast = (100 - tdo->sme_threeD.bot_shadow_contrast) / 100.0;
		xcol_out->red   = contrast * rgb->red;
		xcol_out->green = contrast * rgb->green;
		xcol_out->blue  = contrast * rgb->blue;
		free(reply);
	    } else {
		xcol_out->red = xcol_out->green = xcol_out->blue = 0;
	    }
	}
    } else
	xcol_out->red = xcol_out->green = xcol_out->blue = 0;
}

/* ARGSUSED */
static void
AllocBotShadowPixel (Widget new)
{
    XColor set_c;
    SmeThreeDObject tdo = (SmeThreeDObject) new;
    Widget w = XtParent (new);
    xcb_connection_t *dpy = XtDisplayOfObject (new);
    Colormap cmap = w->core.colormap;

    XawSme3dComputeBottomShadowRGB (new, &set_c);
    xcb_alloc_color_cookie_t cookie = xcb_alloc_color(dpy, cmap, set_c.red, set_c.green, set_c.blue);
    xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(dpy, cookie, NULL);
    if (reply) {
	tdo->sme_threeD.bot_shadow_pixel = reply->pixel;
	free(reply);
    } else {
	tdo->sme_threeD.bot_shadow_pixel = w->core.background_pixel;
    }
}


/* ARGSUSED */
static void
ClassPartInitialize (WidgetClass wc)
{
    SmeThreeDClassRec *tdwc = (SmeThreeDClassRec *) wc;
    SmeThreeDClassRec *super =
	(SmeThreeDClassRec *) tdwc->rect_class.superclass;

    if (tdwc->sme_threeD_class.shadowdraw == XtInheritXawSme3dShadowDraw)
	tdwc->sme_threeD_class.shadowdraw = super->sme_threeD_class.shadowdraw;
}

/* ARGSUSED */
static void
Initialize (Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SmeThreeDObject 	w = (SmeThreeDObject) new;
    xcb_screen_t		*scr = XtScreenOfObject (new);

    if (w->sme_threeD.be_nice_to_cmap || scr->root_depth == 1) {
	AllocTopShadowPixmap (new);
	AllocBotShadowPixmap (new);
    } else {
	if (w->sme_threeD.top_shadow_pixel == w->sme_threeD.bot_shadow_pixel) {
	    AllocTopShadowPixel (new);
	    AllocBotShadowPixel (new);
	}
	w->sme_threeD.top_shadow_pxmap = w->sme_threeD.bot_shadow_pxmap = XCB_NONE;
    }
    AllocTopShadowGC (new);
    AllocBotShadowGC (new);
    AllocEraseGC (new);
}

static void
Destroy (Widget gw)
{
    SmeThreeDObject w = (SmeThreeDObject) gw;
    xcb_connection_t *dpy = XtDisplayOfObject (gw);
    XtReleaseGC (gw, w->sme_threeD.top_shadow_GC);
    XtReleaseGC (gw, w->sme_threeD.bot_shadow_GC);
    XtReleaseGC (gw, w->sme_threeD.erase_GC);
    if (w->sme_threeD.top_shadow_pxmap)
	xcb_free_pixmap (dpy, w->sme_threeD.top_shadow_pxmap);
    if (w->sme_threeD.bot_shadow_pxmap)
	xcb_free_pixmap (dpy, w->sme_threeD.bot_shadow_pxmap);
}

/* ARGSUSED */
static Boolean
SetValues (Widget gcurrent, Widget grequest, Widget gnew, ArgList args, Cardinal *num_args)
{
    SmeThreeDObject current = (SmeThreeDObject) gcurrent;
    SmeThreeDObject new = (SmeThreeDObject) gnew;
    Boolean redisplay = FALSE;
    Boolean alloc_top_pixel = FALSE;
    Boolean alloc_bot_pixel = FALSE;
    Boolean alloc_top_pixmap = FALSE;
    Boolean alloc_bot_pixmap = FALSE;

#if 0
    (*threeDWidgetClass->core_class.superclass->core_class.set_values)
	(gcurrent, grequest, gnew, NULL, 0);
#endif
    if (new->sme_threeD.shadow_width != current->sme_threeD.shadow_width)
	redisplay = TRUE;
    if (new->sme_threeD.be_nice_to_cmap != current->sme_threeD.be_nice_to_cmap) {
	if (new->sme_threeD.be_nice_to_cmap) {
	    alloc_top_pixmap = TRUE;
	    alloc_bot_pixmap = TRUE;
	} else {
	    alloc_top_pixel = TRUE;
	    alloc_bot_pixel = TRUE;
	}
	redisplay = TRUE;
    }
    if (!new->sme_threeD.be_nice_to_cmap &&
	new->sme_threeD.top_shadow_contrast != current->sme_threeD.top_shadow_contrast)
	alloc_top_pixel = TRUE;
    if (!new->sme_threeD.be_nice_to_cmap &&
	new->sme_threeD.bot_shadow_contrast != current->sme_threeD.bot_shadow_contrast)
	alloc_bot_pixel = TRUE;
    if (alloc_top_pixel)
	AllocTopShadowPixel (gnew);
    if (alloc_bot_pixel)
	AllocBotShadowPixel (gnew);
    if (alloc_top_pixmap)
	AllocTopShadowPixmap (gnew);
    if (alloc_bot_pixmap)
	AllocBotShadowPixmap (gnew);
    if (!new->sme_threeD.be_nice_to_cmap &&
	new->sme_threeD.top_shadow_pixel != current->sme_threeD.top_shadow_pixel)
	alloc_top_pixel = TRUE;
    if (!new->sme_threeD.be_nice_to_cmap &&
	new->sme_threeD.bot_shadow_pixel != current->sme_threeD.bot_shadow_pixel)
	alloc_bot_pixel = TRUE;
    if (new->sme_threeD.be_nice_to_cmap) {
	if (alloc_top_pixmap) {
	    XtReleaseGC (gcurrent, current->sme_threeD.top_shadow_GC);
	    AllocTopShadowGC (gnew);
	    redisplay = True;
	}
	if (alloc_bot_pixmap) {
	    XtReleaseGC (gcurrent, current->sme_threeD.bot_shadow_GC);
	    AllocBotShadowGC (gnew);
	    redisplay = True;
	}
    } else {
 xcb_connection_t *dpy = XtDisplayOfObject (gnew);
 if (alloc_top_pixel) {
     if (new->sme_threeD.top_shadow_pxmap) {
  xcb_free_pixmap (dpy, new->sme_threeD.top_shadow_pxmap);
  new->sme_threeD.top_shadow_pxmap = XCB_NONE;
     }
     XtReleaseGC (gcurrent, current->sme_threeD.top_shadow_GC);
     AllocTopShadowGC (gnew);
     redisplay = True;
 }
 if (alloc_bot_pixel) {
     if (new->sme_threeD.bot_shadow_pxmap) {
  xcb_free_pixmap (dpy, new->sme_threeD.bot_shadow_pxmap);
  new->sme_threeD.bot_shadow_pxmap = XCB_NONE;
     }
     XtReleaseGC (gcurrent, current->sme_threeD.bot_shadow_GC);
     AllocBotShadowGC (gnew);
     redisplay = True;
 }
    }
    return (redisplay);
}

/* ARGSUSED */
static void
_XawSme3dDrawShadows(Widget gw)
{
    SmeThreeDObject tdo = (SmeThreeDObject) gw;
    SimpleMenuWidget smw = (SimpleMenuWidget) XtParent(gw);
    ThreeDWidget tdw = (ThreeDWidget) smw->simple_menu.threeD;
    Dimension s = tdo->sme_threeD.shadow_width;
    Dimension ps = tdw->threeD.shadow_width;
    xcb_point_t pt[6];

    /*
     * draw the shadows using the core part width and height,
     * and the threeD part shadow_width.
     *
     * no point to do anything if the shadow_width is 0 or the
     * widget has not been realized.
     */
    if (s > 0 && XtIsRealized(gw))
    {
	Dimension	h = tdo->rectangle.height;
	Dimension	w = tdo->rectangle.width - ps;
	Dimension	x = tdo->rectangle.x + ps;
	Dimension	y = tdo->rectangle.y;
	xcb_connection_t	*dpy = XtDisplayOfObject(gw);
	xcb_window_t	win = XtWindowOfObject(gw);
	xcb_gcontext_t		top, bot;

	if (tdo->sme_threeD.shadowed)
	{
	    top = tdo->sme_threeD.top_shadow_GC;
	    bot = tdo->sme_threeD.bot_shadow_GC;
	}
	else
	    top = bot = tdo->sme_threeD.erase_GC;

	/* top-left shadow */
	pt[0].x = x;		pt[0].y = y + h;
	pt[1].x = x;		pt[1].y = y;
	pt[2].x = w;		pt[2].y = y;
	pt[3].x = w - s;	pt[3].y = y + s;
	pt[4].x = ps + s;       pt[4].y = y + s;
	pt[5].x = ps + s;       pt[5].y = y + h - s;
	xcb_fill_poly(dpy, win, top, XCB_POLY_SHAPE_COMPLEX, XCB_COORD_MODE_ORIGIN, 6, pt);

	/* bottom-right shadow */
/*	pt[0].x = x;		pt[0].y = y + h;	*/
	pt[1].x = w;		pt[1].y = y + h;
/*	pt[2].x = w;		pt[2].y = y;		*/
/*	pt[3].x = w - s;	pt[3].y = y + s;	*/
	pt[4].x = w - s;	pt[4].y = y + h - s;
/*	pt[5].x = ps + s;	pt[5].y = y + h - s;	*/
	xcb_fill_poly(dpy, win, bot, XCB_POLY_SHAPE_COMPLEX, XCB_COORD_MODE_ORIGIN, 6, pt);
    }
}

