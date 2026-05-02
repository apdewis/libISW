
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

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ScrollbarP.h>
#include <ISW/ScrollWheel.h>

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "ISWXcbDraw.h"
#include <ISW/FocusMgrI.h>

/* Private definitions. */

static char defaultTranslations[] =
    "<Btn1Down>:   NotifyScroll()\n\
     <Btn2Down>:   MoveThumb() NotifyThumb() \n\
     <Btn3Down>:   NotifyScroll()\n\
     <Btn1Motion>: HandleThumb() \n\
     <Btn3Motion>: HandleThumb() \n\
     <Btn2Motion>: MoveThumb() NotifyThumb() \n\
     <BtnUp>:      EndScroll()\n\
     <Key>Up:      ScrollLineBackward()\n\
     <Key>Down:    ScrollLineForward()\n\
     <Key>Left:    ScrollLineBackward()\n\
     <Key>Right:   ScrollLineForward()\n\
     <Key>Page_Up:   ScrollPageBackward()\n\
     <Key>Page_Down: ScrollPageForward()\n\
     <Key>Home:    ScrollToStart()\n\
     <Key>End:     ScrollToEnd()";

static float floatZero = 0.0;

#define Offset(field) IswOffsetOf(ScrollbarRec, field)

static IswResource resources[] = {
/*  {IswNscrollCursor, IswCCursor, IswRCursor, sizeof(xcb_cursor_t),
       Offset(scrollbar.cursor), IswRString, "crosshair"},*/
  {IswNlength, IswCLength, IswRDimension, sizeof(Dimension),
       Offset(scrollbar.length), IswRImmediate, (IswPointer) 1},
  {IswNthickness, IswCThickness, IswRDimension, sizeof(Dimension),
       Offset(scrollbar.thickness), IswRImmediate, (IswPointer) 14},
  {IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
      Offset(scrollbar.orientation), IswRImmediate, (IswPointer) IswOrientVertical},
  {IswNscrollProc, IswCCallback, IswRCallback, sizeof(IswPointer),
       Offset(scrollbar.scrollProc), IswRCallback, NULL},
  {IswNthumbProc, IswCCallback, IswRCallback, sizeof(IswPointer),
       Offset(scrollbar.thumbProc), IswRCallback, NULL},
  {IswNjumpProc, IswCCallback, IswRCallback, sizeof(IswPointer),
       Offset(scrollbar.jumpProc), IswRCallback, NULL},
  {IswNthumb, IswCThumb, IswRBitmap, sizeof(xcb_pixmap_t),
       Offset(scrollbar.thumb), IswRImmediate, (IswPointer) IswUnspecifiedPixmap},
  {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
       Offset(scrollbar.foreground), IswRString, IswDefaultForeground},
  {IswNshown, IswCShown, IswRFloat, sizeof(float),
       Offset(scrollbar.shown), IswRFloat, (IswPointer)&floatZero},
  {IswNtopOfThumb, IswCTopOfThumb, IswRFloat, sizeof(float),
       Offset(scrollbar.top), IswRFloat, (IswPointer)&floatZero},
  {IswNpickTop, IswCPickTop, IswRBoolean, sizeof(Boolean),
       Offset(scrollbar.pick_top), IswRBoolean, (IswPointer) False},
  {IswNminimumThumb, IswCMinimumThumb, IswRDimension, sizeof(Dimension),
       Offset(scrollbar.min_thumb), IswRImmediate, (IswPointer) 7},
  {IswNscrollWheelIncrement, IswCScrollWheelIncrement, IswRDimension, sizeof(Dimension),
       Offset(scrollbar.scroll_wheel_increment), IswRImmediate,
       (IswPointer) ISW_SCROLL_WHEEL_DEFAULT_INCREMENT},
};
#undef Offset

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void HandleThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MoveThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void NotifyThumb(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void NotifyScroll(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void EndScroll(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollLineForward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollLineBackward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollPageForward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollPageBackward(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollToStart(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ScrollToEnd(Widget, xcb_generic_event_t *, String *, Cardinal *);

static IswActionsRec actions[] = {
    {"HandleThumb",         HandleThumb},
    {"MoveThumb",           MoveThumb},
    {"NotifyThumb",         NotifyThumb},
    {"NotifyScroll",        NotifyScroll},
    {"EndScroll",           EndScroll},
    {"ScrollLineForward",   ScrollLineForward},
    {"ScrollLineBackward",  ScrollLineBackward},
    {"ScrollPageForward",   ScrollPageForward},
    {"ScrollPageBackward",  ScrollPageBackward},
    {"ScrollToStart",       ScrollToStart},
    {"ScrollToEnd",         ScrollToEnd},
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
    /* num_actions	*/	IswNumber(actions),
    /* resources        */	resources,
    /* num_resources    */	IswNumber(resources),
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
    /* set_values_almost */	IswInheritSetValuesAlmost,
    /* get_values_hook  */	NULL,
    /* accept_focus     */	NULL,
    /* version          */	IswVersion,
    /* callback_private */	NULL,
    /* tm_table         */	defaultTranslations,
    /* query_geometry	*/	IswInheritQueryGeometry,
    /* display_accelerator*/	IswInheritDisplayAccelerator,
    /* extension        */	NULL
  },
  { /* simple fields */
    /* change_sensitive	*/	IswInheritChangeSensitive
  },
  { /* scrollbar fields */
    /* ignore		*/	0
  }

};

WidgetClass scrollbarWidgetClass = (WidgetClass)&scrollbarClassRec;

#define NoButton -1
#define PICKLENGTH(widget, x, y) \
    ((widget->scrollbar.orientation == IswOrientHorizontal) ? x : y)
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#define MAX(x,y)	((x) > (y) ? (x) : (y))

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter( IswRString, IswROrientation, ISWCvtStringToOrientation,
		    (IswConvertArgList)NULL, 0, IswCacheNone, (IswDestructor)NULL );
}

/* CHECKIT #define MARGIN(sbw) (sbw)->scrollbar.thickness */
#define MARGIN(sbw) (sbw)->scrollbar.thickness

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
    int inset = fill ? THUMB_INSET : 0;

    if (sbw->scrollbar.orientation == IswOrientHorizontal) {
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
    double radius = (sbw->scrollbar.orientation == IswOrientHorizontal)
                        ? lh / 2.0 : lw / 2.0;
    if (fill) {
        /* Draw thumb with rounded corners in foreground color */
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, sbw->scrollbar.foreground);
        ISWRenderFillStrokeRoundedRectangle(ctx, lx, ly, lw, lh, radius, 0.2, 1);
        ISWRenderEnd(ctx);
    } else {
        /* Erase thumb area by restoring trough (background) color */
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, sbw->core.background_pixel);
        ISWRenderFillStrokeRoundedRectangle(ctx, lx, ly, lw, lh, radius, 1, 1.5);
        ISWRenderEnd(ctx);
    }
}

/* Paint the thumb in the area specified by sbw->top and
   sbw->shown.  The old area is erased.  The painting and
   erasing is done cleverly so that no flickering will occur. */

static void
PaintThumb (ScrollbarWidget sbw, xcb_generic_event_t *event)
{
    Position  oldtop              = sbw->scrollbar.topLoc;
    Position  oldbot              = oldtop + sbw->scrollbar.shownLength;
    Dimension margin              = MARGIN (sbw);
    Dimension tzl                 = sbw->scrollbar.length - margin - margin;
    Position newtop, newbot;
    Position  floor               = sbw->scrollbar.length - margin;

    newtop = margin + (int)(tzl * sbw->scrollbar.top);
    newbot = newtop + (int)(tzl * sbw->scrollbar.shown);
    if (sbw->scrollbar.shown < 1.) newbot++;
    if (newbot < newtop + (int)sbw->scrollbar.min_thumb)
      newbot = newtop + sbw->scrollbar.min_thumb;

    if ( newbot >= floor ) {
	    newtop = floor-(newbot-newtop)+1;
	    newbot = floor;
    }

    sbw->scrollbar.topLoc = newtop;
    sbw->scrollbar.shownLength = newbot - newtop;
    if (IswIsRealized ((Widget) sbw)) {
        /* Clear entire old thumb area then draw new one. */
        if (oldtop != oldbot)
            FillArea(sbw, oldtop, oldbot, 0);
        FillArea(sbw, newtop, newbot, 1);
    }
}

static void
PaintArrows (ScrollbarWidget sbw)
{
    xcb_point_t    pt[20];
    Dimension t   = sbw->scrollbar.thickness;
    Dimension l   = sbw->scrollbar.length;
    Dimension tms = t , lms = l;
    Dimension tm1 = t - 1;
    Dimension lmt = l - t;
    Dimension lp1 = lmt + 1;
    Dimension t2  = t / 2;
    Dimension sa30 = (Dimension)(1.732);  /* cotangent of 30 deg */

    if (IswIsRealized ((Widget) sbw)) {
	    /* Arrow base matches trough width; tips are inset along length */
	    Dimension bp = THUMB_INSET;  /* base matches thumb width */
	    Dimension tp = t / 4;          /* tip inset along length axis */

	    pt[0].x = bp;          pt[0].y = tm1 - tp;
	    pt[1].x = t - bp;      pt[1].y = tm1 - tp;
	    pt[2].x = t2;          pt[2].y = tp;

	    pt[3].x = bp;          pt[3].y = lp1 + tp;
	    pt[4].x = t - bp;      pt[4].y = lp1 + tp;
	    pt[5].x = t2;          pt[5].y = l - tp;

	    /* horizontal arrows require that x and y coordinates be swapped */
	    if (sbw->scrollbar.orientation == IswOrientHorizontal) {
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

/*	Function Name: Destroy
 *	Description: Called as the scrollbar is going away...
 *	Arguments: w - the scrollbar.
 *	Returns: nonw
 */
static void
Destroy (Widget w)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    if(sbw->scrollbar.timer_id != (IswIntervalId) 0)
	IswRemoveTimeOut (sbw->scrollbar.timer_id);
    /* Destroy Cairo rendering context */
    if (sbw->scrollbar.render_ctx)
        ISWRenderDestroy(sbw->scrollbar.render_ctx);
}


static void
SetDimensions (ScrollbarWidget sbw)
{
    if (sbw->scrollbar.orientation == IswOrientVertical) {
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
    ISWScrollWheelInit(IswDisplay(new));

    /* Scrollbars are not Tab stops — users drive them indirectly via the
     * widget they scroll (keyboard nav on the focused List/Text etc.). */

    /* HiDPI: dimensions stay in logical pixels; scaled at X boundary */

    if (sbw->core.width == 0)
	sbw->core.width = (sbw->scrollbar.orientation == IswOrientVertical)
	    ? sbw->scrollbar.thickness : sbw->scrollbar.length;

    if (sbw->core.height == 0)
	sbw->core.height = (sbw->scrollbar.orientation == IswOrientHorizontal)
	    ? sbw->scrollbar.thickness : sbw->scrollbar.length;

    SetDimensions (sbw);
    sbw->scrollbar.scroll_mode = 0;
    sbw->scrollbar.timer_id = (IswIntervalId)0;
    sbw->scrollbar.topLoc = 0;
    sbw->scrollbar.shownLength = sbw->scrollbar.min_thumb;

    /* Defer render_ctx creation to Realize — Cairo needs a window */
    sbw->scrollbar.render_ctx = NULL;
}

static void
Realize(xcb_connection_t *dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    if(sbw->simple.cursor_name == NULL)
	IswVaSetValues(w, IswNcursorName, "crosshair", NULL);
    /* dont set the cursor of the window to anything */
    *valueMask &= ~XCB_CW_CURSOR;
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
    if (IswIsRealized (desired)) {
	if (sbw->scrollbar.foreground != dsbw->scrollbar.foreground ||
	    sbw->core.background_pixel != dsbw->core.background_pixel ||
	    sbw->scrollbar.thumb != dsbw->scrollbar.thumb) {
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
    /* Thumb may have to move as a result of the new size. */
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

        Dimension margin = MARGIN(sbw);
        int tx, ty, tw, th;

        if (sbw->scrollbar.orientation == IswOrientHorizontal) {
            tx = margin;
            ty = 0;
            tw = sbw->scrollbar.length - 2 * margin;
            th = sbw->core.height - 2;
        } else {
            tx = 0;
            ty = margin;
            tw = sbw->core.width - 2;
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

    if (sbw->scrollbar.render_ctx) {
        ISWRenderContext *ctx = sbw->scrollbar.render_ctx;
        ISWRenderBegin(ctx);
        _IswFocusMgrDrawRing((Widget) sbw, ctx, sbw->scrollbar.foreground, 1.0);
        ISWRenderEnd(ctx);
    }
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
	IswCallActionProc(w, "MoveThumb", event, params, *num_params);
	IswCallActionProc(w, "NotifyThumb", event, params, *num_params);
    }
}

static void
RepeatNotify(IswPointer client_data, IswIntervalId *idp)
{
#define A_FEW_PIXELS 5
    ScrollbarWidget sbw = (ScrollbarWidget) client_data;
    intptr_t call_data;
    if (sbw->scrollbar.scroll_mode != 1 && sbw->scrollbar.scroll_mode != 3) {
	sbw->scrollbar.timer_id = (IswIntervalId) 0;
	return;
    }
    call_data = MAX (A_FEW_PIXELS, sbw->scrollbar.length / 20);
    if (sbw->scrollbar.scroll_mode == 1)
	call_data = -call_data;
    IswCallCallbacks((Widget)sbw, IswNscrollProc, (IswPointer) call_data);
    sbw->scrollbar.timer_id =
    IswAppAddTimeOut(IswWidgetToApplicationContext((Widget)sbw),
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
	IswCallCallbacks (w, IswNscrollProc, (IswPointer)(call_data));
	/* establish autoscroll */
	sbw->scrollbar.timer_id =
	    IswAppAddTimeOut (IswWidgetToApplicationContext (w),
				(unsigned long) 300, RepeatNotify, (IswPointer)w);
	sbw->scrollbar.scroll_mode = 1;
    } else if (PICKLENGTH (sbw,x,y) > sbw->scrollbar.length - sbw->scrollbar.thickness) {
 /* handle last arrow zone */
 call_data = MAX (A_FEW_PIXELS, sbw->scrollbar.length / 20);
 IswCallCallbacks (w, IswNscrollProc, (IswPointer)(call_data));
	/* establish autoscroll */
	sbw->scrollbar.timer_id =
	    IswAppAddTimeOut (IswWidgetToApplicationContext (w),
				(unsigned long) 300, RepeatNotify, (IswPointer)w);
	sbw->scrollbar.scroll_mode = 3;
    } else if (PICKLENGTH (sbw, x, y) < sbw->scrollbar.topLoc) {
 /* handle zone "above" the thumb */
 call_data = - sbw->scrollbar.length;
 IswCallCallbacks (w, IswNscrollProc, (IswPointer)(call_data));
    } else if (PICKLENGTH (sbw, x, y) > sbw->scrollbar.topLoc + sbw->scrollbar.shownLength) {
 /* handle zone "below" the thumb */
 call_data = sbw->scrollbar.length;
 IswCallCallbacks (w, IswNscrollProc, (IswPointer)(call_data));
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

static int
LineDelta(ScrollbarWidget sbw)
{
    int d = MAX(A_FEW_PIXELS, sbw->scrollbar.length / 20);
    return d > 0 ? d : 1;
}

static int
PageDelta(ScrollbarWidget sbw)
{
    int d = sbw->scrollbar.length;
    return d > 0 ? d : 1;
}

static void
ScrollLineForward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    intptr_t cd = LineDelta(sbw);
    (void)e; (void)p; (void)np;
    IswCallCallbacks(w, IswNscrollProc, (IswPointer)cd);
}

static void
ScrollLineBackward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    intptr_t cd = -LineDelta(sbw);
    (void)e; (void)p; (void)np;
    IswCallCallbacks(w, IswNscrollProc, (IswPointer)cd);
}

static void
ScrollPageForward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    intptr_t cd = PageDelta(sbw);
    (void)e; (void)p; (void)np;
    IswCallCallbacks(w, IswNscrollProc, (IswPointer)cd);
}

static void
ScrollPageBackward(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    intptr_t cd = -PageDelta(sbw);
    (void)e; (void)p; (void)np;
    IswCallCallbacks(w, IswNscrollProc, (IswPointer)cd);
}

static void
ScrollToStart(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    float top = 0.0;
    union { IswPointer xtp; float xtf; } xtpf;
    (void)e; (void)p; (void)np;
    sbw->scrollbar.top = top;
    xtpf.xtf = top + 0.0001f;
    IswCallCallbacks(w, IswNthumbProc, xtpf.xtp);
    IswCallCallbacks(w, IswNjumpProc, (IswPointer)&sbw->scrollbar.top);
}

static void
ScrollToEnd(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    ScrollbarWidget sbw = (ScrollbarWidget) w;
    float top = 1.0f - sbw->scrollbar.shown;
    union { IswPointer xtp; float xtf; } xtpf;
    (void)e; (void)p; (void)np;
    if (top < 0.0f) top = 0.0f;
    sbw->scrollbar.top = top;
    xtpf.xtf = top + 0.0001f;
    IswCallCallbacks(w, IswNthumbProc, xtpf.xtp);
    IswCallCallbacks(w, IswNjumpProc, (IswPointer)&sbw->scrollbar.top);
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
    xcb_flush(IswDisplay(w));	/* re-draw it before Notifying */
}


/* ARGSUSED */
static void
NotifyThumb (Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    register ScrollbarWidget sbw = (ScrollbarWidget) w;
    union {
        IswPointer xtp;
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

    IswCallCallbacks (w, IswNthumbProc, xtpf.xtp);
    IswCallCallbacks (w, IswNjumpProc, (IswPointer)&sbw->scrollbar.top);
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

