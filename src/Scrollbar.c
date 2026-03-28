
/* MODIFIED ATHENA SCROLLBAR (USING ARROWHEADS AT ENDS OF TRAVEL) */
/* Modifications Copyright 1992 by Mitch Trachtenberg             */
/* Rights, permissions, and disclaimer of warranty are as in the  */
/* DEC and MIT notice below.                                      */

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

/* ScrollBar.c */
/* created by weissman, Mon Jul  7 13:20:03 1986 */
/* converted by swick, Thu Aug 27 1987 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ScrollbarP.h>
#include <ISW/ScrollWheel.h>

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "ISWXcbDraw.h"

/* Private definitions. */

static char defaultTranslations[] =
    "<Btn1Down>:   NotifyScroll()\n\
     <Btn2Down>:   MoveThumb() NotifyThumb() \n\
     <Btn3Down>:   NotifyScroll()\n\
     <Btn1Motion>: HandleThumb() \n\
     <Btn3Motion>: HandleThumb() \n\
     <Btn2Motion>: MoveThumb() NotifyThumb() \n\
     <BtnUp>:      EndScroll()";

static float floatZero = 0.0;

#define Offset(field) XtOffsetOf(ScrollbarRec, field)

static XtResource resources[] = {
/*  {XtNscrollCursor, XtCCursor, XtRCursor, sizeof(Cursor),
       Offset(scrollbar.cursor), XtRString, "crosshair"},*/
  {XtNlength, XtCLength, XtRDimension, sizeof(Dimension),
       Offset(scrollbar.length), XtRImmediate, (XtPointer) 1},
  {XtNthickness, XtCThickness, XtRDimension, sizeof(Dimension),
       Offset(scrollbar.thickness), XtRImmediate, (XtPointer) 14},
  {XtNorientation, XtCOrientation, XtROrientation, sizeof(XtOrientation),
      Offset(scrollbar.orientation), XtRImmediate, (XtPointer) XtorientVertical},
  {XtNscrollProc, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(scrollbar.scrollProc), XtRCallback, NULL},
  {XtNthumbProc, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(scrollbar.thumbProc), XtRCallback, NULL},
  {XtNjumpProc, XtCCallback, XtRCallback, sizeof(XtPointer),
       Offset(scrollbar.jumpProc), XtRCallback, NULL},
  {XtNthumb, XtCThumb, XtRBitmap, sizeof(Pixmap),
       Offset(scrollbar.thumb), XtRImmediate, (XtPointer) XtUnspecifiedPixmap},
  {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
       Offset(scrollbar.foreground), XtRString, XtDefaultForeground},
  {XtNshown, XtCShown, XtRFloat, sizeof(float),
       Offset(scrollbar.shown), XtRFloat, (XtPointer)&floatZero},
  {XtNtopOfThumb, XtCTopOfThumb, XtRFloat, sizeof(float),
       Offset(scrollbar.top), XtRFloat, (XtPointer)&floatZero},
  {XtNpickTop, XtCPickTop, XtRBoolean, sizeof(Boolean),
       Offset(scrollbar.pick_top), XtRBoolean, (XtPointer) False},
  {XtNminimumThumb, XtCMinimumThumb, XtRDimension, sizeof(Dimension),
       Offset(scrollbar.min_thumb), XtRImmediate, (XtPointer) 7},
  {XtNscrollWheelIncrement, XtCScrollWheelIncrement, XtRDimension, sizeof(Dimension),
       Offset(scrollbar.scroll_wheel_increment), XtRImmediate,
       (XtPointer) ISW_SCROLL_WHEEL_DEFAULT_INCREMENT},
};
#undef Offset

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void HandleThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MoveThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void NotifyThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void NotifyScroll(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void EndScroll(Widget, xcb_generic_event_t *, String *, Cardinal *);

static XtActionsRec actions[] = {
    {"HandleThumb",	HandleThumb},
    {"MoveThumb",	MoveThumb},
    {"NotifyThumb",	NotifyThumb},
    {"NotifyScroll",	NotifyScroll},
    {"EndScroll",	EndScroll}
};


ScrollbarClassRec scrollbarClassRec = {
  { /* core fields */
    /* superclass       */	(WidgetClass) &simpleClassRec,
    /* class_name       */	"Scrollbar",
    /* size             */	sizeof(ScrollbarRec),
    /* class_initialize	*/	ClassInitialize,
    /* class_part_init  */	NULL,
    /* class_inited	*/	FALSE,
    /* initialize       */	Initialize,
    /* initialize_hook  */	NULL,
    /* realize          */	Realize,
    /* actions          */	actions,
    /* num_actions	*/	XtNumber(actions),
    /* resources        */	resources,
    /* num_resources    */	XtNumber(resources),
    /* xrm_class        */	NULLQUARK,
    /* compress_motion	*/	TRUE,
    /* compress_exposure*/	TRUE,
    /* compress_enterleave*/	TRUE,
    /* visible_interest */	FALSE,
    /* destroy          */	Destroy,
    /* resize           */	Resize,
    /* expose           */	Redisplay,
    /* set_values       */	SetValues,
    /* set_values_hook  */	NULL,
    /* set_values_almost */	XtInheritSetValuesAlmost,
    /* get_values_hook  */	NULL,
    /* accept_focus     */	NULL,
    /* version          */	XtVersion,
    /* callback_private */	NULL,
    /* tm_table         */	defaultTranslations,
    /* query_geometry	*/	XtInheritQueryGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension        */	NULL
  },
  { /* simple fields */
    /* change_sensitive	*/	XtInheritChangeSensitive
  },
  { /* scrollbar fields */
    /* ignore		*/	0
  }

};

WidgetClass scrollbarWidgetClass = (WidgetClass)&scrollbarClassRec;

#define NoButton -1
#define PICKLENGTH(widget, x, y) \
    ((widget->scrollbar.orientation == XtorientHorizontal) ? x : y)
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#define MAX(x,y)	((x) > (y) ? (x) : (y))

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    XtSetTypeConverter( XtRString, XtROrientation, ISWCvtStringToOrientation,
		    (XtConvertArgList)NULL, 0, XtCacheNone, (XtDestructor)NULL );
}

/* CHECKIT #define MARGIN(sbw) (sbw)->scrollbar.thickness */
#define MARGIN(sbw) (sbw)->scrollbar.thickness

/*
 The original Isw Scrollbar's FillArea *really* relied on the fact that the
 server was going to clip at the window boundaries; so the logic was really
 rather sloppy.  To avoid drawing over the shadows and the arrows requires
 some extra care...  Hope I didn't make any mistakes.
*/
/* Padding between the scrollbar edges and the trough/thumb */
#define SCROLLBAR_PAD 2
/* Inset of the thumb relative to the trough */
#define THUMB_INSET 3

static void
FillArea (ScrollbarWidget sbw, Position top, Position bottom, int fill)
{
    int tlen = bottom - top;	/* length of thumb in pixels */
    int sw, margin, floor;
    int lx, ly, lw, lh;

    if (bottom <= 0 || bottom <= top)
	return;
    sw = 0;
    margin = MARGIN (sbw);
    floor = sbw->scrollbar.length - margin;

    /* Inset the thumb so it's narrower than the channel */
    int inset = fill ? (SCROLLBAR_PAD + THUMB_INSET) : SCROLLBAR_PAD;

    if (sbw->scrollbar.orientation == XtorientHorizontal) {
	lx = ((top < margin) ? margin : top);
	ly = sw + inset;
	lw = ((bottom > floor) ? floor - top : tlen);
	lh = sbw->core.height - 2 * sw - 2 * inset;
    } else {
	lx = sw + inset;
	ly = ((top < margin) ? margin : top);
	lw = sbw->core.width - 2 * sw - 2 * inset;
	lh = ((bottom > floor) ? floor - top : tlen);
    }
    if (lh <= 0 || lw <= 0) return;

    ISWRenderContext *ctx = sbw->scrollbar.render_ctx;

    if (fill) {
        /* Draw thumb with rounded corners in foreground color */
        double radius = (sbw->scrollbar.orientation == XtorientHorizontal)
                        ? lh / 2.0 : lw / 2.0;
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, sbw->scrollbar.foreground);
        ISWRenderFillRoundedRectangle(ctx, lx, ly, lw, lh, radius);
        ISWRenderEnd(ctx);
    } else {
        /* Erase thumb area by restoring trough (background) color */
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, sbw->core.background_pixel);
        ISWRenderFillRectangle(ctx, lx, ly, lw, lh);
        ISWRenderEnd(ctx);
    }
}

/* Paint the thumb in the area specified by sbw->top and
   sbw->shown.  The old area is erased.  The painting and
   erasing is done cleverly so that no flickering will occur. */

static void
PaintThumb (ScrollbarWidget sbw, xcb_generic_event_t *event)
{
    Dimension s                   = 0;
    Position  oldtop              = sbw->scrollbar.topLoc;
    Position  oldbot              = oldtop + sbw->scrollbar.shownLength;
    Dimension margin              = MARGIN (sbw);
    Dimension tzl                 = sbw->scrollbar.length - margin - margin;
    Position newtop, newbot;
    Position  floor               = sbw->scrollbar.length - margin;

    newtop = margin + (int)(tzl * sbw->scrollbar.top);
    newbot = newtop + (int)(tzl * sbw->scrollbar.shown);
    if (sbw->scrollbar.shown < 1.) newbot++;
    if (newbot < newtop + (int)sbw->scrollbar.min_thumb +
                        2 * (int)0)
      newbot = newtop + sbw->scrollbar.min_thumb +
                        2 * 0;
    if ( newbot >= floor ) {
	newtop = floor-(newbot-newtop)+1;
	newbot = floor;
    }

    sbw->scrollbar.topLoc = newtop;
    sbw->scrollbar.shownLength = newbot - newtop;
    if (XtIsRealized ((Widget) sbw)) {
      /*  3D thumb wanted ?
       */
      if (s)
   {
          /* Clear entire old thumb area then draw new one.
           * Differential clearing leaves border remnants. */
          if (oldtop != oldbot)
              FillArea(sbw, oldtop, oldbot, 0);
          FillArea(sbw, newtop, newbot, 1);
   }
      else
   {
   /*
     Note to Mitch: FillArea is (now) correctly implemented to
     not draw over shadows or the arrows. Therefore setting clipmasks
     doesn't seem to be necessary.  Correct me if I'm wrong!
   */
          /* Clear entire old thumb area then draw new one. */
          if (oldtop != oldbot)
              FillArea(sbw, oldtop, oldbot, 0);
          FillArea(sbw, newtop, newbot, 1);
   }
    }
}

static void
PaintArrows (ScrollbarWidget sbw)
{
    xcb_point_t    pt[20];
    Dimension s                   = 0;
    Dimension t   = sbw->scrollbar.thickness;
    Dimension l   = sbw->scrollbar.length;
    Dimension tms = t - s, lms = l - s;
    Dimension tm1 = t - 1;
    Dimension lmt = l - t;
    Dimension lp1 = lmt + 1;
    Dimension sm1 = s - 1;
    Dimension t2  = t / 2;
    Dimension sa30 = (Dimension)(1.732 * s );  /* cotangent of 30 deg */


    if (XtIsRealized ((Widget) sbw)) {
	/* 3D arrows?
         */
	if (s) {
	    /* upper/right arrow */
	    pt[0].x = sm1;         pt[0].y = tm1;
	    pt[1].x = t2;          pt[1].y = sm1;
	    pt[2].x = t2;          pt[2].y = s + sa30;
	    pt[3].x = sm1 + sa30;  pt[3].y = tms - 1;

	    pt[4].x = sm1;         pt[4].y = tm1;
	    pt[5].x = tms;         pt[5].y = tm1;
	    pt[6].x = t2;          pt[6].y = sm1;
	    pt[7].x = t2;          pt[7].y = s + sa30;
	    pt[8].x = tms - sa30;  pt[8].y = tms - 1;
	    pt[9].x = sm1 + sa30;  pt[9].y = tms - 1;

	    /* lower/left arrow */
	    pt[10].x = tms;        pt[10].y = lp1;
	    pt[11].x = s;          pt[11].y = lp1;
	    pt[12].x = t2;         pt[12].y = lms;
	    pt[13].x = t2;         pt[13].y = lms - sa30;
	    pt[14].x = s + sa30;   pt[14].y = lmt + s + 1;
	    pt[15].x = tms - sa30; pt[15].y = lmt + s + 1;

	    pt[16].x = tms;        pt[16].y = lp1;
	    pt[17].x = t2;         pt[17].y = lms;
	    pt[18].x = t2;         pt[18].y = lms - sa30;
	    pt[19].x = tms - sa30; pt[19].y = lmt + s + 1;

	    /* horizontal arrows require that x and y coordinates be swapped */
	    if (sbw->scrollbar.orientation == XtorientHorizontal) {
		int n;
		int swap;
		for (n = 0; n < 20; n++) {
		    swap = pt[n].x;
		    pt[n].x = pt[n].y;
		    pt[n].y = swap;
		}
	    }
	           ISWRenderContext *ctx = sbw->scrollbar.render_ctx;
	           
	           /* Use Cairo for polygon rendering */
	           ISWRenderBegin(ctx);
	           ISWRenderSetColor(ctx, sbw->scrollbar.foreground);
	           ISWRenderFillPolygon(ctx, (xcb_point_t *)pt, 4);
	           ISWRenderFillPolygon(ctx, (xcb_point_t *)(pt + 4), 6);
	           ISWRenderFillPolygon(ctx, (xcb_point_t *)(pt + 10), 6);
	           ISWRenderFillPolygon(ctx, (xcb_point_t *)(pt + 16), 4);
	           ISWRenderEnd(ctx);

	} else {
	    /* Arrow base matches trough width; tips are inset along length */
	    Dimension bp = SCROLLBAR_PAD + THUMB_INSET;  /* base matches thumb width */
	    Dimension tp = t / 4;          /* tip inset along length axis */

	    pt[0].x = bp;          pt[0].y = tm1 - tp;
	    pt[1].x = t - bp;      pt[1].y = tm1 - tp;
	    pt[2].x = t2;          pt[2].y = tp;

	    pt[3].x = bp;          pt[3].y = lp1 + tp;
	    pt[4].x = t - bp;      pt[4].y = lp1 + tp;
	    pt[5].x = t2;          pt[5].y = l - tp;

	    /* horizontal arrows require that x and y coordinates be swapped */
	    if (sbw->scrollbar.orientation == XtorientHorizontal) {
		int n;
		int swap;
		for (n = 0; n < 6; n++) {
		    swap = pt[n].x;
		    pt[n].x = pt[n].y;
		    pt[n].y = swap;
		}
	    }
	    ISWRenderContext *ctx = sbw->scrollbar.render_ctx;
	    ISWRenderBegin(ctx);
	    ISWRenderSetColor(ctx, sbw->scrollbar.foreground);
	    ISWRenderFillPolygon(ctx, (xcb_point_t *)pt, 3);
	    ISWRenderFillPolygon(ctx, (xcb_point_t *)(pt+3), 3);
	    ISWRenderEnd(ctx);
	}
    }
}

/*	Function Name: Destroy
 *	Description: Called as the scrollbar is going away...
 *	Arguments: w - the scrollbar.
 *	Returns: nonw
 */
static void
Destroy (Widget w)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    if(sbw->scrollbar.timer_id != (XtIntervalId) 0)
	XtRemoveTimeOut (sbw->scrollbar.timer_id);
    XtReleaseGC (w, sbw->scrollbar.gc);

    /* Destroy Cairo rendering context */
    if (sbw->scrollbar.render_ctx)
        ISWRenderDestroy(sbw->scrollbar.render_ctx);
}

/*	Function Name: CreateGC
 *	Description: Creates the xcb_gcontext_t.
 *	Arguments: w - the scrollbar widget.
 *	Returns: none.
 */

static void
CreateGC (Widget w)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    xcb_create_gc_value_list_t gcValues;
    XtGCMask mask;
    unsigned int depth = 1;

    if (sbw->scrollbar.thumb == XtUnspecifiedPixmap) {
        sbw->scrollbar.thumb = IswCreateStippledPixmap (XtDisplay(w), XtWindow(w),
    	(Pixel) 1, (Pixel) 0, depth);
    } else if (sbw->scrollbar.thumb != None) {
	xcb_connection_t *conn = XtDisplay(w);
	xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, sbw->scrollbar.thumb);
	xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(conn, cookie, NULL);
	if (reply) {
	    depth = reply->depth;
	    free(reply);
	} else {
	    XtAppError (XtWidgetToApplicationContext (w),
	       "Scrollbar Widget: Could not get geometry of thumb pixmap.");
	}
    }

    gcValues.foreground = sbw->scrollbar.foreground;
    gcValues.background = sbw->core.background_pixel;
    mask = GCForeground | GCBackground;

    if (sbw->scrollbar.thumb != None) {
	if (depth == 1) {
	    gcValues.fill_style = XCB_FILL_STYLE_OPAQUE_STIPPLED;
	    gcValues.stipple = sbw->scrollbar.thumb;
	    mask |= GCFillStyle | GCStipple;
	}
	else {
	    gcValues.fill_style = XCB_FILL_STYLE_TILED;
	    gcValues.tile = sbw->scrollbar.thumb;
	    mask |= GCFillStyle | GCTile;
	}
    }
    /* the creation should be non-caching, because */
    /* we now set and clear clip masks on the gc returned */
    sbw->scrollbar.gc = XtGetGC (w, mask, (xcb_create_gc_value_list_t*)&gcValues);
}

static void
SetDimensions (ScrollbarWidget sbw)
{
    if (sbw->scrollbar.orientation == XtorientVertical) {
	sbw->scrollbar.length = sbw->core.height;
	sbw->scrollbar.thickness = sbw->core.width;
    } else {
	sbw->scrollbar.length = sbw->core.width;
	sbw->scrollbar.thickness = sbw->core.height;
    }
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ScrollbarWidget sbw = (ScrollbarWidget) new;
    /* Install scroll wheel event dispatcher (once per connection) */
    ISWScrollWheelInit(XtDisplay(new));

    /* HiDPI: scale dimension resources */
    sbw->scrollbar.thickness = ISWScaleDim(new, sbw->scrollbar.thickness);
    sbw->scrollbar.length = ISWScaleDim(new, sbw->scrollbar.length);
    sbw->scrollbar.min_thumb = ISWScaleDim(new, sbw->scrollbar.min_thumb);
    sbw->scrollbar.scroll_wheel_increment = ISWScaleDim(new, sbw->scrollbar.scroll_wheel_increment);
    CreateGC (new);

    if (sbw->core.width == 0)
	sbw->core.width = (sbw->scrollbar.orientation == XtorientVertical)
	    ? sbw->scrollbar.thickness : sbw->scrollbar.length;

    if (sbw->core.height == 0)
	sbw->core.height = (sbw->scrollbar.orientation == XtorientHorizontal)
	    ? sbw->scrollbar.thickness : sbw->scrollbar.length;

    SetDimensions (sbw);
    sbw->scrollbar.scroll_mode = 0;
    sbw->scrollbar.timer_id = (XtIntervalId)0;
    sbw->scrollbar.topLoc = 0;
    sbw->scrollbar.shownLength = sbw->scrollbar.min_thumb;

    /* Defer render_ctx creation to Realize — Cairo needs a window */
    sbw->scrollbar.render_ctx = NULL;
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    if(sbw->simple.cursor_name == NULL)
	XtVaSetValues(w, XtNcursorName, "crosshair", NULL);
    /* dont set the cursor of the window to anything */
    *valueMask &= ~CWCursor;
    /*
     * The Simple widget actually stuffs the value in the valuemask.
     */

    (*scrollbarWidgetClass->core_class.superclass->core_class.realize)
	(dpy, w, valueMask, attributes);

    /* Create Cairo rendering context now that we have a window */
    sbw->scrollbar.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget desired, ArgList args, Cardinal *num_args)
{
    ScrollbarWidget sbw = (ScrollbarWidget) current;
    ScrollbarWidget dsbw = (ScrollbarWidget) desired;
    Boolean redraw = FALSE;

/*
 * If these values are outside the acceptable range ignore them...
 */

    if (dsbw->scrollbar.top < 0.0 || dsbw->scrollbar.top > 1.0)
        dsbw->scrollbar.top = sbw->scrollbar.top;

    if (dsbw->scrollbar.shown < 0.0 || dsbw->scrollbar.shown > 1.0)
        dsbw->scrollbar.shown = sbw->scrollbar.shown;

/*
 * Change colors and stuff...
 */
    if (XtIsRealized (desired)) {
	if (sbw->scrollbar.foreground != dsbw->scrollbar.foreground ||
	    sbw->core.background_pixel != dsbw->core.background_pixel ||
	    sbw->scrollbar.thumb != dsbw->scrollbar.thumb) {
	    XtReleaseGC (desired, sbw->scrollbar.gc);
	    CreateGC (desired);
	    redraw = TRUE;
	}
	if (sbw->scrollbar.top != dsbw->scrollbar.top ||
	    sbw->scrollbar.shown != dsbw->scrollbar.shown)
	    redraw = TRUE;
    }
    return redraw;
}

static void
Resize (Widget w)
{
    /* ForgetGravity has taken care of background, but thumb may
     * have to move as a result of the new size. */
    SetDimensions ((ScrollbarWidget) w);
    Redisplay (w, NULL, 0);
}


/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;

    /* Draw the trough (channel) in background color, padded from the edges */
    {
        Dimension s = 0;
        Dimension margin = MARGIN(sbw);
        int tx, ty, tw, th;

        if (sbw->scrollbar.orientation == XtorientHorizontal) {
            tx = margin;
            ty = s + SCROLLBAR_PAD;
            tw = sbw->scrollbar.length - 2 * margin;
            th = sbw->core.height - 2 * s - 2 * SCROLLBAR_PAD;
        } else {
            tx = s + SCROLLBAR_PAD;
            ty = margin;
            tw = sbw->core.width - 2 * s - 2 * SCROLLBAR_PAD;
            th = sbw->scrollbar.length - 2 * margin;
        }

        ISWRenderContext *ctx = sbw->scrollbar.render_ctx;
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, sbw->core.background_pixel);
        ISWRenderFillRectangle(ctx, tx, ty, tw, th);
        ISWRenderEnd(ctx);
    }

    /* Forces entire thumb to be painted. */
    sbw->scrollbar.topLoc = -(sbw->scrollbar.length + 1);
    PaintThumb (sbw, event);
    PaintArrows (sbw);

}


static Boolean
CompareEvents(xcb_generic_event_t *oldEvent, xcb_generic_event_t *newEvent)
{
    uint8_t oldType = oldEvent->response_type & ~0x80;
    uint8_t newType = newEvent->response_type & ~0x80;
    
    if (newType != oldType)
	return False;

    switch (newType) {
    case XCB_MOTION_NOTIFY: {
	xcb_motion_notify_event_t *newMotion = (xcb_motion_notify_event_t*)newEvent;
	xcb_motion_notify_event_t *oldMotion = (xcb_motion_notify_event_t*)oldEvent;
	if (newMotion->state != oldMotion->state) return False;
	if (newMotion->event != oldMotion->event) return False;
	break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
	xcb_button_press_event_t *newButton = (xcb_button_press_event_t*)newEvent;
	xcb_button_press_event_t *oldButton = (xcb_button_press_event_t*)oldEvent;
	if (newButton->state != oldButton->state) return False;
	if (newButton->detail != oldButton->detail) return False;
	if (newButton->event != oldButton->event) return False;
	break;
    }
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
	xcb_key_press_event_t *newKey = (xcb_key_press_event_t*)newEvent;
	xcb_key_press_event_t *oldKey = (xcb_key_press_event_t*)oldEvent;
	if (newKey->state != oldKey->state) return False;
	if (newKey->detail != oldKey->detail) return False;
	if (newKey->event != oldKey->event) return False;
	break;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
	xcb_enter_notify_event_t *newCross = (xcb_enter_notify_event_t*)newEvent;
	xcb_enter_notify_event_t *oldCross = (xcb_enter_notify_event_t*)oldEvent;
	if (newCross->mode != oldCross->mode) return False;
	if (newCross->detail != oldCross->detail) return False;
	if (newCross->state != oldCross->state) return False;
	if (newCross->event != oldCross->event) return False;
	break;
    }
    }

    return True;
}

/* Unused - LookAhead is stubbed out for XCB compatibility
struct EventData {
    xcb_generic_event_t *oldEvent;
    int count;
};

static Bool
PeekNotifyEvent(xcb_connection_t *dpy, xcb_generic_event_t *event, char *args)
{
    struct EventData *eventData = (struct EventData*)args;

    return ((++eventData->count == QLength(dpy))
	    || CompareEvents(event, eventData->oldEvent));
}
*/

static Boolean
LookAhead (Widget w, xcb_generic_event_t *event)
{
    /* TODO: XCB doesn't have direct QLength/XPeekIfEvent equivalents
     * This function was used to look ahead in the event queue and skip
     * redundant motion events. For now, stub it to always return False
     * (don't skip). This is conservative and correct, just potentially
     * less efficient.
     *
     * Full implementation would require:
     * - Maintaining our own event queue or using xcb_poll_for_event
     * - Implementing event comparison and filtering
     */
    (void)w;
    (void)event;
    return False;
}


static void
ExtractPosition(xcb_generic_event_t *event, Position *x, Position *y)
{
    uint8_t type = event->response_type & ~0x80;
    
    switch(type) {
    case XCB_MOTION_NOTIFY: {
	xcb_motion_notify_event_t *mev = (xcb_motion_notify_event_t *)event;
	*x = mev->event_x;
	*y = mev->event_y;
	break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
	xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
	*x = bev->event_x;
	*y = bev->event_y;
	break;
    }
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
	xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
	*x = kev->event_x;
	*y = kev->event_y;
	break;
    }
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY: {
	xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;
	*x = cev->event_x;
	*y = cev->event_y;
	break;
    }
    default:
	*x = 0; *y = 0;
    }
}

/* ARGSUSED */
static void
HandleThumb(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    Position x,y;
    ScrollbarWidget sbw = (ScrollbarWidget) w;

    ExtractPosition( event, &x, &y );
    /* if the motion event puts the pointer in thumb, call Move and Notify */
    /* also call Move and Notify if we're already in continuous scroll mode */
    if (sbw->scrollbar.scroll_mode == 2 ||
	(PICKLENGTH (sbw,x,y) >= sbw->scrollbar.topLoc &&
	PICKLENGTH (sbw,x,y) <= sbw->scrollbar.topLoc + sbw->scrollbar.shownLength)){
	XtCallActionProc(w, "MoveThumb", event, params, *num_params);
	XtCallActionProc(w, "NotifyThumb", event, params, *num_params);
    }
}

static void
RepeatNotify(XtPointer client_data, XtIntervalId *idp)
{
#define A_FEW_PIXELS 5
    ScrollbarWidget sbw = (ScrollbarWidget) client_data;
    intptr_t call_data;
    if (sbw->scrollbar.scroll_mode != 1 && sbw->scrollbar.scroll_mode != 3) {
	sbw->scrollbar.timer_id = (XtIntervalId) 0;
	return;
    }
    call_data = MAX (A_FEW_PIXELS, sbw->scrollbar.length / 20);
    if (sbw->scrollbar.scroll_mode == 1)
	call_data = -call_data;
    XtCallCallbacks((Widget)sbw, XtNscrollProc, (XtPointer) call_data);
    sbw->scrollbar.timer_id =
    XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)sbw),
		    (unsigned long) 150,
		    RepeatNotify,
		    client_data);
}

/*
 * Make sure the first number is within the range specified by the other
 * two numbers.
 */

/*
 * Same as above, but for floating numbers.
 */

static float
FloatInRange(float num, float small, float big)
{
    return (num < small) ? small : ((num > big) ? big : num);
}


static void
NotifyScroll (Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    intptr_t call_data;
    Position x, y;

    if (sbw->scrollbar.scroll_mode == 2  /* if scroll continuous */
	|| LookAhead (w, event))
	return;

    ExtractPosition (event, &x, &y);

    if (PICKLENGTH (sbw,x,y) < sbw->scrollbar.thickness) {
 /* handle first arrow zone */
 call_data = -MAX (A_FEW_PIXELS, sbw->scrollbar.length / 20);
	XtCallCallbacks (w, XtNscrollProc, (XtPointer)(call_data));
	/* establish autoscroll */
	sbw->scrollbar.timer_id =
	    XtAppAddTimeOut (XtWidgetToApplicationContext (w),
				(unsigned long) 300, RepeatNotify, (XtPointer)w);
	sbw->scrollbar.scroll_mode = 1;
    } else if (PICKLENGTH (sbw,x,y) > sbw->scrollbar.length - sbw->scrollbar.thickness) {
 /* handle last arrow zone */
 call_data = MAX (A_FEW_PIXELS, sbw->scrollbar.length / 20);
 XtCallCallbacks (w, XtNscrollProc, (XtPointer)(call_data));
	/* establish autoscroll */
	sbw->scrollbar.timer_id =
	    XtAppAddTimeOut (XtWidgetToApplicationContext (w),
				(unsigned long) 300, RepeatNotify, (XtPointer)w);
	sbw->scrollbar.scroll_mode = 3;
    } else if (PICKLENGTH (sbw, x, y) < sbw->scrollbar.topLoc) {
 /* handle zone "above" the thumb */
 call_data = - sbw->scrollbar.length;
 XtCallCallbacks (w, XtNscrollProc, (XtPointer)(call_data));
    } else if (PICKLENGTH (sbw, x, y) > sbw->scrollbar.topLoc + sbw->scrollbar.shownLength) {
 /* handle zone "below" the thumb */
 call_data = sbw->scrollbar.length;
 XtCallCallbacks (w, XtNscrollProc, (XtPointer)(call_data));
    } else
	{
	/* handle the thumb in the motion notify action */
	}
    return;
}

/* ARGSUSED */
static void
EndScroll(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;

    sbw->scrollbar.scroll_mode = 0;
    /* no need to remove any autoscroll timeout; it will no-op */
    /* because the scroll_mode is 0 */
    /* but be sure to remove timeout in destroy proc */
}

static float
FractionLoc (ScrollbarWidget sbw, int x, int y)
{
    float   result;
    int margin;
    float   height, width;

    margin = MARGIN (sbw);
    x -= margin;
    y -= margin;
    height = sbw->core.height - 2 * margin;
    width = sbw->core.width - 2 * margin;
    result = PICKLENGTH (sbw, x / width, y / height);
    return FloatInRange(result, 0.0, 1.0);
}


static void
MoveThumb (Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    Position x, y;
    float loc, s;
    float t;

    if (LookAhead (w, event)) return;

    /* Check if event is on same screen - XCB motion events have same_screen field */
    if (event) {
 xcb_motion_notify_event_t *mev = (xcb_motion_notify_event_t *)event;
 if (!mev->same_screen) return;
    }

    ExtractPosition (event, &x, &y);
    loc = FractionLoc (sbw, x, y);
    s = sbw->scrollbar.shown;
    t = sbw->scrollbar.top;
    if (sbw->scrollbar.scroll_mode != 2 )
      /* initialize picked position */
      sbw->scrollbar.picked = (FloatInRange( loc, t, t + s ) - t);
    if (sbw->scrollbar.pick_top)
      sbw->scrollbar.top = loc;
    else {
      sbw->scrollbar.top = loc - sbw->scrollbar.picked;
      if (sbw->scrollbar.top < 0.0) sbw->scrollbar.top = 0.0;
    }

    if (sbw->scrollbar.top + sbw->scrollbar.shown > 1.0)
      sbw->scrollbar.top = 1.0 - sbw->scrollbar.shown;
    sbw->scrollbar.scroll_mode = 2; /* indicate continuous scroll */
    PaintThumb (sbw, event);
    xcb_flush(XtDisplay(w));	/* re-draw it before Notifying */
}


/* ARGSUSED */
static void
NotifyThumb (Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    register ScrollbarWidget sbw = (ScrollbarWidget) w;
    union {
        XtPointer xtp;
        float xtf;
    } xtpf;

    if (LookAhead (w, event)) return;

    /* thumbProc is not pretty, but is necessary for backwards
       compatibility on those architectures for which it work{s,ed};
       the intent is to pass a (truncated) float by value. */
    xtpf.xtf = sbw->scrollbar.top;

/* #ifdef ISW_ARROW_SCROLLBARS */
    /* This corrects for rounding errors: If the thumb is moved to the end of
       the scrollable area sometimes the last line/column is not displayed.
       This can happen when the integer number of the top line or leftmost
       column to be be displayed is calculated from the float value
       sbw->scrollbar.top. The numerical error of this rounding problem is
       very small. We therefore add a small value which then forces the
       next line/column (the correct one) to be used. Since we can expect
       that the resolution of display screens will not be higher then
       10000 text lines/columns we add 1/10000 to the top position. The
       intermediate variable `top' is used to avoid erroneous summing up
       corrections (can this happen at all?). If the arrows are not displayed
       there is no problem since in this case there is always a constant
       integer number of pixels the thumb must be moved in order to scroll
       to the next line/column. */
    /* Removed the dependancy on scrollbar arrows. Xterm as distributed in
       X11R6.6 by The XFree86 Project wants this correction, with or without
       the arrows. */
    xtpf.xtf += 0.0001;
/* #endif */

    XtCallCallbacks (w, XtNthumbProc, xtpf.xtp);
    XtCallCallbacks (w, XtNjumpProc, (XtPointer)&sbw->scrollbar.top);
}



/************************************************************
 *
 *  Public routines.
 *
 ************************************************************/

/* Set the scroll bar to the given location. */

void ISWScrollbarSetThumb (Widget w,
#if NeedWidePrototypes
			double top, double shown)
#else
			float top, float shown)
#endif
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;

    if (sbw->scrollbar.scroll_mode == (char) 2) return; /* if still thumbing */

    sbw->scrollbar.top = (top > 1.0) ? 1.0 :
				(top >= 0.0) ? top : sbw->scrollbar.top;

    sbw->scrollbar.shown = (shown > 1.0) ? 1.0 :
				(shown >= 0.0) ? shown : sbw->scrollbar.shown;

    PaintThumb (sbw, NULL);
}

