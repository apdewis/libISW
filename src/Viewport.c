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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/Scrollbar.h>
#include <ISW/ViewportP.h>
#include <ISW/IswArgMacros.h>

/* Utility macro */
#define AssignMax(x, y) ((x) = ((x) > (y) ? (x) : (y)))

#include <stdint.h>
#include <xcb/xcb.h>
#include "ISWXcbDraw.h"

static void ScrollUpDownProc(Widget, IswPointer, IswPointer);
static void ThumbProc(Widget, IswPointer, IswPointer);
static void ScrollWheelSink(Widget, IswPointer, xcb_generic_event_t *, Boolean *);
static Boolean GetGeometry(Widget, Dimension, Dimension);
static void ComputeWithForceBars(Widget, Boolean, IswWidgetGeometry *, int *, int *);

#define offset(field) IswOffsetOf(ViewportRec, viewport.field)
static IswResource resources[] = {
    {IswNforceBars, IswCBoolean, IswRBoolean, sizeof(Boolean),
	 offset(forcebars), IswRImmediate, (IswPointer)False},
    {IswNallowHoriz, IswCBoolean, IswRBoolean, sizeof(Boolean),
	 offset(allowhoriz), IswRImmediate, (IswPointer)False},
    {IswNallowVert, IswCBoolean, IswRBoolean, sizeof(Boolean),
	 offset(allowvert), IswRImmediate, (IswPointer)False},
    {IswNuseBottom, IswCBoolean, IswRBoolean, sizeof(Boolean),
	 offset(usebottom), IswRImmediate, (IswPointer)False},
    {IswNuseRight, IswCBoolean, IswRBoolean, sizeof(Boolean),
	 offset(useright), IswRImmediate, (IswPointer)False},
    {IswNreportCallback, IswCReportCallback, IswRCallback, sizeof(IswPointer),
	 offset(report_callbacks), IswRImmediate, (IswPointer) NULL},
};
#undef offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void ConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void ChangeManaged(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean Layout(FormWidget, Dimension, Dimension, Boolean);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static IswGeometryResult PreferredGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

#define superclass	(&formClassRec)
ViewportClassRec viewportClassRec = {
  { /* core_class fields */
    /* superclass	  */	(WidgetClass) superclass,
    /* class_name	  */	"Viewport",
    /* widget_size	  */	sizeof(ViewportRec),
    /* class_initialize	  */	IswInitializeWidgetSet,
    /* class_part_init    */    NULL,
    /* class_inited	  */	FALSE,
    /* initialize	  */	Initialize,
    /* initialize_hook    */    NULL,
    /* realize		  */	Realize,
    /* actions		  */	NULL,
    /* num_actions	  */	0,
    /* resources	  */	resources,
    /* num_resources	  */	IswNumber(resources),
    /* xrm_class	  */	NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/    TRUE,
    /* visible_interest	  */	FALSE,
    /* destroy		  */	NULL,
    /* resize		  */	Resize,
    /* expose		  */	IswInheritExpose,
    /* set_values	  */	SetValues,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    IswInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus	  */	NULL,
    /* version            */    IswVersion,
    /* callback_private	  */	NULL,
    /* tm_table    	  */	NULL,
    /* query_geometry     */    PreferredGeometry,
    /* display_accelerator*/	IswInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager	  */	GeometryManager,
    /* change_managed	  */	ChangeManaged,
    /* insert_child	  */	IswInheritInsertChild,
    /* delete_child	  */	IswInheritDeleteChild,
    /* extension          */	NULL
  },
  { /* constraint_class fields */
    /* subresourses	  */	NULL,
    /* subresource_count  */	0,
    /* constraint_size	  */	sizeof(ViewportConstraintsRec),
    /* initialize	  */	ConstraintInitialize,
    /* destroy		  */	NULL,
    /* set_values	  */	NULL,
    /* extension          */	NULL
  },
  { /* form_class fields */
    /* layout		  */	Layout
  },
  { /* viewport_class fields */
    /* empty		  */	0
  }
};


WidgetClass viewportWidgetClass = (WidgetClass)&viewportClassRec;

static Widget
CreateScrollbar(ViewportWidget w, Boolean horizontal)
{
    Widget clip = w->viewport.clip;
    ViewportConstraints constraints =
	(ViewportConstraints)clip->core.constraints;
    IswArgBuilder ab = IswArgBuilderInit();
    Widget bar;

    IswArgOrientation(&ab,
       horizontal ? IswOrientHorizontal : IswOrientVertical );
    IswArgLength(&ab,
	     horizontal ? clip->core.width : clip->core.height);
    IswArgLeft(&ab,
	     (!horizontal && w->viewport.useright) ? IswChainRight : IswChainLeft);
    IswArgRight(&ab,
	     (!horizontal && !w->viewport.useright) ? IswChainLeft : IswChainRight);
    IswArgTop(&ab,
	     (horizontal && w->viewport.usebottom) ? IswChainBottom : IswChainTop);
    IswArgBottom(&ab,
	     (horizontal && !w->viewport.usebottom) ? IswChainTop : IswChainBottom);
    IswArgMappedWhenManaged(&ab, False);

    bar = IswCreateWidget((horizontal ? "horizontal" : "vertical"),
			  scrollbarWidgetClass, (Widget)w,
			  ab.args, ab.count );
    IswAddCallback( bar, IswNscrollProc, ScrollUpDownProc, (IswPointer)w );
    IswAddCallback( bar, IswNjumpProc, ThumbProc, (IswPointer)w );

    if (horizontal) {
	w->viewport.horiz_bar = bar;
	constraints->form.vert_base = bar;
    }
    else {
	w->viewport.vert_bar = bar;
	constraints->form.horiz_base = bar;
    }

    IswManageChild( bar );

    return bar;
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ViewportWidget w = (ViewportWidget)new;
    IswArgBuilder ab = IswArgBuilderInit();
    Widget h_bar, v_bar;
    Dimension clip_height, clip_width;
    Dimension pad = 0, sw = 0;

    w->form.default_spacing = 0;  /* Reset the default spacing to 0 pixels. */

/*
 * Initialize all widget pointers to NULL.
 */

    w->viewport.child = (Widget) NULL;
    w->viewport.horiz_bar = w->viewport.vert_bar = (Widget)NULL;

/*3D Widget creation removed - ThreeD eliminated.
 * Viewport will function without 3D border effects.
 */
    /* w->viewport.threeD = NULL; -- ThreeD removed */

/*
 * Create Clip Widget.
 */

    IswArgBackgroundPixmap(&ab, None);
    IswArgBorderWidth(&ab, 0);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainRight);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainBottom);
    IswArgWidth(&ab, w->core.width - 2 * sw);
    IswArgHeight(&ab, w->core.height - 2 * sw);

    w->viewport.clip = IswCreateManagedWidget("clip", widgetClass, new,
					     ab.args, ab.count);

    /*
     * Select XCB_EVENT_MASK_BUTTON_PRESS on the clip widget so that scroll wheel events
     * (buttons 4-7) propagate from non-interactive children (Labels, Boxes)
     * to the clip window instead of being discarded by the X server.
     * The actual scroll handling is done by the ScrollWheel event dispatcher.
     */
    IswAddEventHandler(w->viewport.clip,
                      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                      False, ScrollWheelSink, NULL);

    if (!w->viewport.forcebars)
        return;		 /* If we are not forcing the bars then we are done. */

    if (w->viewport.allowhoriz)
      (void) CreateScrollbar(w, True);
    if (w->viewport.allowvert)
      (void) CreateScrollbar(w, False);

    h_bar = w->viewport.horiz_bar;
    v_bar = w->viewport.vert_bar;

/*
 * Set the clip widget to the correct height.
 */

    clip_width = w->core.width - 2 * sw;
    clip_height = w->core.height - 2 * sw;

    if ( (h_bar != NULL) &&
	 ((int)w->core.width >
	  (int)(h_bar->core.width + h_bar->core.border_width + pad)) )
        clip_width -= h_bar->core.width + h_bar->core.border_width + pad;

    if ( (v_bar != NULL) &&
	 ((int)w->core.height >
	  (int)(v_bar->core.height + v_bar->core.border_width + pad)) )
        clip_height -= v_bar->core.height + v_bar->core.border_width + pad;

    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, clip_width);
    IswArgHeight(&ab, clip_height);
    IswSetValues(w->viewport.clip, ab.args, ab.count);
}

/* ARGSUSED */
static void
ConstraintInitialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ((ViewportConstraints)new->core.constraints)->viewport.reparented = False;
}

static void
Realize(xcb_connection_t *conn, Widget widget, IswValueMask *value_mask, uint32_t *values)
{
    ViewportWidget w = (ViewportWidget)widget;
    Widget child = w->viewport.child;
    Widget clip = w->viewport.clip;
    Widget threeD = NULL; /* (Widget)w->viewport.threeD; */

    if (!(*value_mask & XCB_CW_BIT_GRAVITY)) {
        int insert_idx = 0;
        int total_values = 0;
        int i;
        uint32_t bit;

        /* Count values for bits below XCB_CW_BIT_GRAVITY (bits 0-3) */
        for (bit = 1; bit < XCB_CW_BIT_GRAVITY; bit <<= 1) {
            if (*value_mask & bit)
                insert_idx++;
        }
        /* Count total values in current attributes array */
        for (bit = 1; bit <= XCB_CW_CURSOR; bit <<= 1) {
            if (*value_mask & bit)
                total_values++;
        }
        /* Shift values from insert_idx onward to make room */
        for (i = total_values; i > insert_idx; i--)
            values[i] = values[i - 1];

        values[insert_idx] = XCB_GRAVITY_NORTH_WEST;
        *value_mask |= XCB_CW_BIT_GRAVITY;
    }
    (*superclass->core_class.realize)(conn, widget, value_mask, values);

    (*w->core.widget_class->core_class.resize)(widget);	/* turn on bars */

    if (child != (Widget)NULL) {
	IswMoveWidget( child, (Position)0, (Position)0 );
	IswRealizeWidget( clip );
	IswRealizeWidget( child );
	/* IswRealizeWidget( threeD ); */
	
	/* Lower threeD window */
	uint32_t lower_values[] = { XCB_STACK_MODE_BELOW };
	/* xcb_configure_window(conn, IswWindow(threeD), XCB_CONFIG_WINDOW_STACK_MODE, lower_values);
	
	/* Reparent child to clip */
	xcb_reparent_window(conn, IswWindow(child), IswWindow(clip), 0, 0);
	
	IswMapWidget( child );
    }
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ViewportWidget w = (ViewportWidget)new;
    ViewportWidget cw = (ViewportWidget)current;

    if ( (w->viewport.forcebars != cw->viewport.forcebars) ||
	 (w->viewport.allowvert != cw->viewport.allowvert) ||
	 (w->viewport.allowhoriz != cw->viewport.allowhoriz) ||
	 (w->viewport.useright != cw->viewport.useright) ||
	 (w->viewport.usebottom != cw->viewport.usebottom) )
    {
	(*w->core.widget_class->core_class.resize)(new); /* Recompute layout.*/
    }

    return False;
}


static void
ChangeManaged(Widget widget)
{
    ViewportWidget w = (ViewportWidget)widget;
    int num_children = w->composite.num_children;
    Widget child, *childP;
    int i;

    child = (Widget)NULL;
    for (childP=w->composite.children, i=0; i < num_children; childP++, i++) {
	if (IswIsManaged(*childP)
	    && *childP != w->viewport.clip
	    && *childP != w->viewport.horiz_bar
	    && *childP != w->viewport.vert_bar
	    /* 	    && *childP != (Widget)w->viewport.threeD)	    && *childP != (Widget)w->viewport.threeD) *childP != (Widget)w->viewport.threeD */ )
	{
	    child = *childP;
	    break;
	}
    }

    if (child != w->viewport.child) {
	w->viewport.child = child;
	if (child != (Widget)NULL) {
	    IswResizeWidget( child, child->core.width,
			    child->core.height, (Dimension)0 );
	    if (IswIsRealized(widget)) {
		ViewportConstraints constraints =
		    (ViewportConstraints)child->core.constraints;
		if (!IswIsRealized(child)) {
		    xcb_window_t window = IswWindow(w);
		    IswMoveWidget( child, (Position)0, (Position)0 );
#ifdef notdef
		    /* this is dirty, but it saves the following code: */
		    IswRealizeWidget( child );
		    xcb_connection_t *conn = IswDisplay(w);
		    xcb_reparent_window(conn, IswWindow(child),
				     IswWindow(w->viewport.clip), 0, 0);
		    if (child->core.mapped_when_managed)
			IswMapWidget( child );
#else
		    w->core.window = IswWindow(w->viewport.clip);
		    IswRealizeWidget( child );
		    w->core.window = window;
#endif /* notdef */
		    constraints->viewport.reparented = True;
		}
		else if (!constraints->viewport.reparented) {
		    xcb_connection_t *conn = IswDisplay(w);
		    xcb_reparent_window(conn, IswWindow(child),
				     IswWindow(w->viewport.clip), 0, 0);
		    constraints->viewport.reparented = True;
		    if (child->core.mapped_when_managed)
			IswMapWidget( child );
		}
	    }
	    GetGeometry( widget, child->core.width, child->core.height );
	    (*((ViewportWidgetClass)w->core.widget_class)->form_class.layout)
		( (FormWidget)w, w->core.width, w->core.height, FALSE );
	    /* %%% do we need to hide this child from Form?  */
	}
    }

#ifdef notdef
    (*superclass->composite_class.change_managed)( widget );
#endif
}


static void
SetBar(Widget w, Position top, Dimension length, Dimension total)
{
    ISWScrollbarSetThumb(w, (float)top/(float)total,
			 (float)length/(float)total);
}

static void
RedrawThumbs(ViewportWidget w)
{
    Widget child = w->viewport.child;
    Widget clip = w->viewport.clip;

    if (w->viewport.horiz_bar != (Widget)NULL)
	SetBar( w->viewport.horiz_bar, -(child->core.x),
	        clip->core.width, child->core.width );

    if (w->viewport.vert_bar != (Widget)NULL)
	SetBar( w->viewport.vert_bar, -(child->core.y),
	        clip->core.height, child->core.height );
}



static void
SendReport (ViewportWidget w, unsigned int changed)
{
    IswPannerReport rep;

    if (w->viewport.report_callbacks) {
	Widget child = w->viewport.child;
	Widget clip = w->viewport.clip;

	rep.changed = changed;
	rep.slider_x = -child->core.x;	/* child is canvas */
	rep.slider_y = -child->core.y;	/* clip is slider */
	rep.slider_width = clip->core.width;
	rep.slider_height = clip->core.height;
	rep.canvas_width = child->core.width;
	rep.canvas_height = child->core.height;
	IswCallCallbackList ((Widget) w, w->viewport.report_callbacks,
			    (IswPointer) &rep);
    }
}


static void
MoveChild(ViewportWidget w, Position x, Position y)
{
    Widget child = w->viewport.child;
    Widget clip = w->viewport.clip;

    /* make sure we never move past right/bottom borders */
    if (-x + (int)clip->core.width > (int)child->core.width)
	x = -(child->core.width - clip->core.width);

    if (-y + (int)clip->core.height > (int)child->core.height)
	y = -(child->core.height - clip->core.height);

    /* make sure we never move past left/top borders */
    if (x >= 0) x = 0;
    if (y >= 0) y = 0;

    IswMoveWidget(child, x, y);
    SendReport (w, (IswPRSliderX | IswPRSliderY));

    RedrawThumbs(w);
}


static void
ComputeLayout(Widget widget, Boolean query, Boolean destroy_scrollbars)
{
    ViewportWidget w = (ViewportWidget)widget;
    Widget child = w->viewport.child;
    Widget clip = w->viewport.clip;
    Widget threeD = NULL; /* (Widget)w->viewport.threeD; */
    ViewportConstraints constraints
	= (ViewportConstraints)clip->core.constraints;
    Boolean needshoriz, needsvert;
    int clip_width, clip_height;
    int bar_width, bar_height;
    IswWidgetGeometry intended;
    Dimension pad = 0, sw = 0;

    /*
     * I've made two optimizations here. The first does away with the
     * loop, and the second defers setting the child dimensions to the
     * clip if smaller until after adjusting for possible scrollbars.
     * If you find that these go too far, define the identifiers here
     * as required.  -- djhjr
     */
#define NEED_LAYOUT_LOOP
#undef PREP_CHILD_TO_CLIP

    if (child == (Widget) NULL) return;

    /* IswVaGetValues(threeD, IswNshadowWidth, &sw, NULL);
       if (sw) pad = 2; */ sw = 0; pad = 0;

    clip_width = w->core.width - 2 * sw;
    clip_height = w->core.height - 2 * sw;
    intended.request_mode = XCB_CONFIG_WINDOW_BORDER_WIDTH;
    intended.border_width = 0;

    if (w->viewport.forcebars) {
        needsvert = w->viewport.allowvert;
        needshoriz = w->viewport.allowhoriz;
        ComputeWithForceBars(widget, query, &intended,
			     &clip_width, &clip_height);
    }
    else {
#ifdef NEED_LAYOUT_LOOP
        Dimension prev_width, prev_height;
	IswGeometryMask prev_mode;
#endif
	IswWidgetGeometry preferred;

	needshoriz = needsvert = False;

	/*
	 * intended.{width,height} caches the eventual child dimensions,
	 * but we don't set the mode bits until after we decide that the
	 * child's preferences are not acceptable.
	 */

	intended.width = clip_width;
	intended.height = clip_height;

	if (!w->viewport.allowhoriz)
	    intended.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	if (!w->viewport.allowvert)
	    intended.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;

	if (!query) {
	    preferred.width = child->core.width;
	    preferred.height = child->core.height;
	}

#ifdef NEED_LAYOUT_LOOP
	do { /* while intended != prev */
#endif
	    if (query) {
		/* Query the child's preferred size.  Constrain each axis
		 * only when scrolling is not allowed on that axis, so the
		 * child reports its natural size on scrollable axes. */
		(void) IswQueryGeometry( child, &intended, &preferred );
		if ( !(preferred.request_mode & XCB_CONFIG_WINDOW_WIDTH) )
		    preferred.width = intended.width;
		if ( !(preferred.request_mode & XCB_CONFIG_WINDOW_HEIGHT) )
		    preferred.height = intended.height;
	    }

#ifdef NEED_LAYOUT_LOOP
	    prev_width = intended.width;
	    prev_height = intended.height;
	    prev_mode = intended.request_mode;
#endif

	    /*
	     * Note that having once decided to turn on either bar
	     * we'll not change our mind until we're next resized,
	     * thus avoiding potential oscillations.
	     */

#define CheckHoriz()							\
	    if (w->viewport.allowhoriz &&				\
		    (int)preferred.width > clip_width + 2 * sw) {	\
		if (!needshoriz) {					\
		    Widget horiz_bar = w->viewport.horiz_bar;		\
		    needshoriz = True;					\
		    if (horiz_bar == (Widget)NULL)			\
			horiz_bar = CreateScrollbar(w, True);		\
		    clip_height -= horiz_bar->core.height +		\
				   horiz_bar->core.border_width + pad;	\
		    if (clip_height < 1) clip_height = 1;		\
		}							\
		intended.width = preferred.width;			\
	    }
/* enddef */
	    CheckHoriz();
	    if (w->viewport.allowvert &&
		    (int)preferred.height > clip_height + 2 * sw) {
		if (!needsvert) {
		    Widget vert_bar = w->viewport.vert_bar;
		    needsvert = True;
		    if (vert_bar == (Widget)NULL)
			vert_bar = CreateScrollbar(w, False);
		    clip_width -= vert_bar->core.width +
				  vert_bar->core.border_width + pad;
		    if (clip_width < 1) clip_width = 1;
		    if (!needshoriz) CheckHoriz();
		}
		intended.height = preferred.height;
	    }

#ifdef PREP_CHILD_TO_CLIP
	    if (!w->viewport.allowhoriz ||
		    (int)preferred.width < clip_width) {
	        intended.width = clip_width;
		intended.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	    }
	    if (!w->viewport.allowvert ||
		    (int)preferred.height < clip_height) {
	        intended.height = clip_height;
		intended.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
	    }
#endif
#ifdef NEED_LAYOUT_LOOP
	} while ( intended.request_mode != prev_mode ||
		  (intended.request_mode & XCB_CONFIG_WINDOW_WIDTH &&
			intended.width != prev_width) ||
		  (intended.request_mode & XCB_CONFIG_WINDOW_HEIGHT &&
			intended.height != prev_height) );
#endif

#ifndef PREP_CHILD_TO_CLIP
	if (!w->viewport.allowhoriz ||
		(int)preferred.width < clip_width) {
	    intended.width = clip_width;
	    intended.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
	}
	if (!w->viewport.allowvert ||
		(int)preferred.height < clip_height) {
	    intended.height = clip_height;
	    intended.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
	}
#endif
    }

    bar_width = bar_height = 0;
    if (needsvert)
	bar_width = w->viewport.vert_bar->core.width +
		    w->viewport.vert_bar->core.border_width + pad;
    if (needshoriz)
	bar_height = w->viewport.horiz_bar->core.height +
		     w->viewport.horiz_bar->core.border_width + pad;

    if (0 /* IswIsRealized(threeD) */ )
	/* XLowerWindow( IswDisplay(threeD), IswWindow(threeD) ); */

    /* IswMoveWidget( threeD,
		  (Position)(!needsvert ? 0 :
			     (w->viewport.useright ? 0 : bar_width)),
		  (Position)(!needshoriz ? 0 :
			     (w->viewport.usebottom ? 0 : bar_height)) ); */
    /* IswResizeWidget( threeD, (Dimension)(w->core.width - bar_width),
		    (Dimension)(w->core.height - bar_height), (Dimension)0 ); */

    if (IswIsRealized(clip))
	xcb_configure_window(IswDisplay(clip), IswWindow(clip),
	    XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){XCB_STACK_MODE_ABOVE});

    IswMoveWidget( clip,
		  (Position)(!needsvert ? sw :
			     (w->viewport.useright ? sw : bar_width + sw)),
		  (Position)(!needshoriz ? sw :
			     (w->viewport.usebottom ? sw : bar_height + sw)) );
    IswResizeWidget( clip, (Dimension)clip_width, (Dimension)clip_height,
		    (Dimension)0 );

    if (w->viewport.horiz_bar != (Widget)NULL) {
	Widget bar = w->viewport.horiz_bar;
	if (!needshoriz) {
	    constraints->form.vert_base = (Widget)NULL;
	    if (destroy_scrollbars) {
		IswDestroyWidget( bar );
		w->viewport.horiz_bar = (Widget)NULL;
	    }
	}
	else {
	    int bw = bar->core.border_width;
	    IswResizeWidget( bar,
			    (Dimension)(clip_width + 2 * sw), bar->core.height,
			    (Dimension)bw );
	    IswMoveWidget( bar,
			  (Position)((needsvert && !w->viewport.useright)
			   ? w->viewport.vert_bar->core.width + pad
			   : -bw),
			  (Position)(w->viewport.usebottom
			    ? w->core.height - bar->core.height - bw
			    : -bw) );
	    IswSetMappedWhenManaged( bar, True );
	}
    }

    if (w->viewport.vert_bar != (Widget)NULL) {
	Widget bar = w->viewport.vert_bar;
	if (!needsvert) {
	    constraints->form.horiz_base = (Widget)NULL;
	    if (destroy_scrollbars) {
		IswDestroyWidget( bar );
		w->viewport.vert_bar = (Widget)NULL;
	    }
	}
	else {
	    int bw = bar->core.border_width;
	    IswResizeWidget( bar,
			    bar->core.width, (Dimension)(clip_height + 2 * sw),
			    (Dimension)bw );
	    IswMoveWidget( bar,
			  (Position)(w->viewport.useright
			   ? w->core.width - bar->core.width - bw
			   : -bw),
			  (Position)((needshoriz && !w->viewport.usebottom)
			    ? w->viewport.horiz_bar->core.height + pad
			    : -bw) );
	    IswSetMappedWhenManaged( bar, True );
	}
    }

    if (child != (Widget)NULL) {
	IswResizeWidget( child, (Dimension)intended.width,
		        (Dimension)intended.height, (Dimension)0 );
	MoveChild(w,
		  needshoriz ? child->core.x : 0,
		  needsvert ? child->core.y : 0);
    }

    SendReport (w, IswPRAll);
}

/*      Function Name: ComputeWithForceBars
 *      Description: Computes the layout give forcebars is set.
 *      Arguments: widget - the viewport widget.
 *                 query - whether or not to query the child.
 *                 intended - the cache of the childs height is
 *                            stored here ( USED AND RETURNED ).
 *                 clip_width, clip_height - size of clip window.
 *                                           (USED AND RETURNED ).
 *      Returns: none.
 */

static void
ComputeWithForceBars(Widget widget, Boolean query, IswWidgetGeometry *intended,
                     int *clip_width, int *clip_height)
{
    ViewportWidget w = (ViewportWidget)widget;
    Widget child = w->viewport.child;
    IswWidgetGeometry preferred;
    Dimension pad = 0, sw = 0;

/*
 * If forcebars then needs = allows = has.
 * Thus if needsvert is set it MUST have a scrollbar.
 */

    /* IswVaGetValues((Widget)(w->viewport.threeD), IswNshadowWidth, &sw, NULL);
       if (sw) pad = 2; */ sw = 0; pad = 0;

    if (w->viewport.allowvert) {
	if (w->viewport.vert_bar == NULL)
	    w->viewport.vert_bar = CreateScrollbar(w, False);

	*clip_width -= w->viewport.vert_bar->core.width +
		       w->viewport.vert_bar->core.border_width + pad;
    }

    if (w->viewport.allowhoriz) {
	if (w->viewport.horiz_bar == NULL)
	    w->viewport.horiz_bar = CreateScrollbar(w, True);

        *clip_height -= w->viewport.horiz_bar->core.height +
		       w->viewport.horiz_bar->core.border_width + pad;
    }

    AssignMax( *clip_width, 1 );
    AssignMax( *clip_height, 1 );

    if (!w->viewport.allowvert) {
        intended->height = *clip_height;
        intended->request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    if (!w->viewport.allowhoriz) {
        intended->width = *clip_width;
        intended->request_mode |= XCB_CONFIG_WINDOW_WIDTH;
    }

    if ( query ) {
        if ( (w->viewport.allowvert || w->viewport.allowhoriz) ) {
	    IswQueryGeometry( child, intended, &preferred );

	    if ( !(intended->request_mode & XCB_CONFIG_WINDOW_WIDTH) ) {
	        if ( preferred.request_mode & XCB_CONFIG_WINDOW_WIDTH )
		    intended->width = preferred.width;
		else
		    intended->width = child->core.width;
	    }

	    if ( !(intended->request_mode & XCB_CONFIG_WINDOW_HEIGHT) ) {
	        if ( preferred.request_mode & XCB_CONFIG_WINDOW_HEIGHT )
		    intended->height = preferred.height;
		else
		    intended->height = child->core.height;
	    }
	}
    }
    else {
        if (w->viewport.allowvert)
	    intended->height = child->core.height;
	if (w->viewport.allowhoriz)
	    intended->width = child->core.width;
    }

    if (*clip_width > (int)intended->width)
	intended->width = *clip_width;
    if (*clip_height > (int)intended->height)
	intended->height = *clip_height;
}

static void
Resize(Widget widget)
{
    ComputeLayout( widget, /*query=*/True, /*destroy=*/True );
}


/* ARGSUSED */
static Boolean
Layout(FormWidget w, Dimension width, Dimension height, Boolean junk)
{
    ComputeLayout( (Widget)w, /*query=*/True, /*destroy=*/True );
    w->form.preferred_width = w->core.width;
    w->form.preferred_height = w->core.height;
    return False;
}


/*
 * No-op event handler registered on the clip widget to ensure
 * XCB_EVENT_MASK_BUTTON_PRESS is set on the clip window.  This causes X11 to
 * propagate button events (including scroll wheel) from children
 * that don't select XCB_EVENT_MASK_BUTTON_PRESS.  The actual scroll wheel
 * handling is done by the ScrollWheel event dispatcher.
 */
/* ARGSUSED */
static void
ScrollWheelSink(Widget w, IswPointer closure, xcb_generic_event_t *event, Boolean *continue_to_dispatch)
{
    /* Intentionally empty — the ScrollWheel event dispatcher handles
       scroll wheel events before they reach this handler. */
}

static void
ScrollUpDownProc(Widget widget, IswPointer closure, IswPointer call_data)
{
    ViewportWidget w = (ViewportWidget)closure;
    Widget child = w->viewport.child;
    int pix = (intptr_t) call_data;
    Position x, y;

    if (child == NULL) return;	/* no child to scroll. */

    x = child->core.x - ((widget == w->viewport.horiz_bar) ? pix : 0);
    y = child->core.y - ((widget == w->viewport.vert_bar) ? pix : 0);
    MoveChild(w, x, y);
}


/* ARGSUSED */
static void
ThumbProc(Widget widget, IswPointer closure, IswPointer call_data)
{
    ViewportWidget w = (ViewportWidget)closure;
    Widget child = w->viewport.child;
    float *percent = (float *) call_data;
    Position x, y;

    if (child == NULL) return;	/* no child to scroll. */

    if (widget == w->viewport.horiz_bar)
#ifdef macII				/* bug in the macII A/UX 1.0 cc */
	x = (int)(-*percent * child->core.width);
#else /* else not macII */
	x = -(int)(*percent * child->core.width);
#endif /* macII */
    else
	x = child->core.x;

    if (widget == w->viewport.vert_bar)
#ifdef macII				/* bug in the macII A/UX 1.0 cc */
	y = (int)(-*percent * child->core.height);
#else /* else not macII */
	y = -(int)(*percent * child->core.height);
#endif /* macII */
    else
	y = child->core.y;

    MoveChild(w, x, y);
}

static IswGeometryResult
TestSmaller(ViewportWidget w, IswWidgetGeometry *request, IswWidgetGeometry *reply_return)
{
  if (request->width < w->core.width || request->height < w->core.height)
    return IswMakeGeometryRequest((Widget)w, request, reply_return);
  else
    return IswGeometryYes;
}

static IswGeometryResult
GeometryRequestPlusScrollbar(ViewportWidget w, Boolean horizontal,
                             IswWidgetGeometry *request, IswWidgetGeometry *reply_return)
{
  Widget bar;
  IswWidgetGeometry plusScrollbars;
  Dimension pad = 0, sw = 0;

  /* IswVaGetValues((Widget)(w->viewport.threeD), IswNshadowWidth, &sw, NULL);
     if (sw) pad = 2; */ sw = 0; pad = 0;

  plusScrollbars = *request;
  if ((bar = w->viewport.horiz_bar) == (Widget)NULL)
    bar = CreateScrollbar(w, horizontal);
  request->width += bar->core.width + pad;
  request->height += bar->core.height + pad;
  IswDestroyWidget(bar);
  return IswMakeGeometryRequest((Widget) w, &plusScrollbars, reply_return);
 }

#define WidthChange() (request->width != w->core.width)
#define HeightChange() (request->height != w->core.height)

static IswGeometryResult
QueryGeometry(ViewportWidget w, IswWidgetGeometry *request, IswWidgetGeometry *reply_return)
{
  if (w->viewport.allowhoriz && w->viewport.allowvert)
    return TestSmaller(w, request, reply_return);

  else if (w->viewport.allowhoriz && !w->viewport.allowvert) {
    if (WidthChange() && !HeightChange())
      return TestSmaller(w, request, reply_return);
    else if (!WidthChange() && HeightChange())
      return IswMakeGeometryRequest((Widget) w, request, reply_return);
    else if (WidthChange() && HeightChange()) /* hard part */
      return GeometryRequestPlusScrollbar(w, True, request, reply_return);
    else /* !WidthChange() && !HeightChange() */
      return IswGeometryYes;
  }
  else if (!w->viewport.allowhoriz && w->viewport.allowvert) {
    if (!WidthChange() && HeightChange())
      return TestSmaller(w, request, reply_return);
    else if (WidthChange() && !HeightChange())
      return IswMakeGeometryRequest((Widget)w, request, reply_return);
    else if (WidthChange() && HeightChange()) /* hard part */
      return GeometryRequestPlusScrollbar(w, False, request, reply_return);
    else /* !WidthChange() && !HeightChange() */
      return IswGeometryYes;
  }
  else /* (!w->viewport.allowhoriz && !w->viewport.allowvert) */
    return IswMakeGeometryRequest((Widget) w, request, reply_return);
}

#undef WidthChange
#undef HeightChange

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    ViewportWidget w = (ViewportWidget)child->core.parent;
    Boolean rWidth = (Boolean)(request->request_mode & XCB_CONFIG_WINDOW_WIDTH);
    Boolean rHeight = (Boolean)(request->request_mode & XCB_CONFIG_WINDOW_HEIGHT);
    IswWidgetGeometry allowed;
    IswGeometryResult result;
    Boolean reconfigured;
    Boolean child_changed_size;
    Dimension height_remaining;
    Dimension pad = 0, sw = 0;

    if (request->request_mode & IswCWQueryOnly)
      return QueryGeometry(w, request, reply);

    if (child != w->viewport.child
        || request->request_mode & ~(XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
				     | XCB_CONFIG_WINDOW_BORDER_WIDTH)
	|| ((request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH)
	    && request->border_width > 0))
	return IswGeometryNo;

    /* IswVaGetValues((Widget)(w->viewport.threeD), IswNshadowWidth, &sw, NULL);
       if (sw) pad = 2; */ sw = 0; pad = 0;

    allowed = *request;

    reconfigured = GetGeometry( (Widget)w,
			        (rWidth ? request->width : w->core.width),
			        (rHeight ? request->height : w->core.height)
			      );

    child_changed_size = ((rWidth && child->core.width != request->width) ||
			  (rHeight && child->core.height != request->height));

    height_remaining = w->core.height;
    if (rWidth && w->core.width != request->width) {
	if (w->viewport.allowhoriz && request->width > w->core.width) {
	    /* horizontal scrollbar will be needed so possibly reduce height */
	    Widget bar = w->viewport.horiz_bar;
	    if (bar == (Widget)NULL) {
		bar = CreateScrollbar( w, True );
		height_remaining -= bar->core.height +
				    bar->core.border_width + pad;
	    }
	    reconfigured = True;
	}
	else {
	    allowed.width = w->core.width;
	}
    }
    if (rHeight && height_remaining != request->height) {
	if (w->viewport.allowvert && request->height > height_remaining) {
	    /* vertical scrollbar will be needed, so possibly reduce width */
	    if (!w->viewport.allowhoriz || request->width < w->core.width) {
		if (!rWidth) {
		    allowed.width = w->core.width;
		    allowed.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
		}
		if (w->viewport.vert_bar == (Widget)NULL) {
		    Widget bar = CreateScrollbar( w, False );
		    if ( (int)allowed.width >
			 (int)(bar->core.width + bar->core.border_width + pad) )
			allowed.width -= bar->core.width +
					 bar->core.border_width + pad;
		    else
			allowed.width = 1;
		}
		reconfigured = True;
	    }
	}
	else {
	    allowed.height = height_remaining;
	}
    }

    if (allowed.width != request->width || allowed.height != request->height) {
	*reply = allowed;
	result = IswGeometryAlmost;
    }
    else {
	if (rWidth)  child->core.width = request->width;
	if (rHeight) child->core.height = request->height;
	result = IswGeometryYes;
    }

    if (reconfigured || child_changed_size)
	ComputeLayout( (Widget)w,
		       /*query=*/ False,
		       /*destroy=*/ True );

    return result;
  }


static Boolean
GetGeometry(Widget w, Dimension width, Dimension height)
{
    IswWidgetGeometry geometry, return_geom;
    IswGeometryResult result;

    if (width == w->core.width && height == w->core.height)
	return False;

    geometry.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    geometry.width = width;
    geometry.height = height;

    if (IswIsRealized(w)) {
	/* Post-realize, the viewport should not request resizes from its
	   parent.  Its size is determined by its parent; it scrolls to
	   accommodate content that doesn't fit. */
	return False;
    } else {
	/* This is the Realize call; we'll inherit a w&h iff none currently */
	if (w->core.width != 0) {
	    if (w->core.height != 0) return False;
	    geometry.width = w->core.width;
	}
	if (w->core.height != 0) geometry.height = w->core.height;
    }

    result = IswMakeGeometryRequest(w, &geometry, &return_geom);
    if (result == IswGeometryAlmost)
	result = IswMakeGeometryRequest(w, &return_geom, (IswWidgetGeometry *)NULL);

    return (result == IswGeometryYes);
}

static IswGeometryResult
PreferredGeometry(Widget w, IswWidgetGeometry *constraints, IswWidgetGeometry *reply)
{
    /* If the viewport has been given an explicit size, report that as
       preferred rather than delegating to the child.  Delegating to the
       child causes the parent to expand the viewport to fit all content,
       which defeats the purpose of having a scrollable viewport. */
    if (w->core.width != 0 && w->core.height != 0) {
	reply->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	reply->width = w->core.width;
	reply->height = w->core.height;
	if (constraints != NULL &&
	    (constraints->request_mode & XCB_CONFIG_WINDOW_WIDTH) &&
	    constraints->width == w->core.width &&
	    (constraints->request_mode & XCB_CONFIG_WINDOW_HEIGHT) &&
	    constraints->height == w->core.height)
	    return IswGeometryYes;
	return IswGeometryAlmost;
    }
    if (((ViewportWidget)w)->viewport.child != NULL)
	return IswQueryGeometry( ((ViewportWidget)w)->viewport.child,
			       constraints, reply );
    else
	return IswGeometryYes;
}


void
IswViewportSetLocation (Widget gw,
#if NeedWidePrototypes
			double xoff, double yoff)
#else
			float xoff, float yoff)
#endif
{
    ViewportWidget w = (ViewportWidget) gw;
    Widget child = w->viewport.child;
    Position x, y;

    if (xoff > 1.0)			/* scroll to right */
       x = child->core.width;
    else if (xoff < 0.0)		/* if the offset is < 0.0 nothing */
       x = child->core.x;
    else
       x = (Position) (((float) child->core.width) * xoff);

    if (yoff > 1.0)
       y = child->core.height;
    else if (yoff < 0.0)
       y = child->core.y;
    else
       y = (Position) (((float) child->core.height) * yoff);

    MoveChild (w, -x, -y);
}

void
IswViewportSetCoordinates (Widget gw,
#if NeedWidePrototypes
			   int x, int y)
#else
			   Position x, Position y)
#endif
{
    ViewportWidget w = (ViewportWidget) gw;
    Widget child = w->viewport.child;

    if (x > (int)child->core.width)
      x = child->core.width;
    else if (x < 0)
      x = child->core.x;

    if (y > (int)child->core.height)
      y = child->core.height;
    else if (y < 0)
      y = child->core.y;

    MoveChild (w, -x, -y);
}

