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
 * Box.c - Box composite widget
 *
 */

#include	<ISW/IntrinsicP.h>
#include	<ISW/StringDefs.h>
#include	<ISW/ISWInit.h>
#include	<ISW/BoxP.h>
#include	<ISW/ISWRender.h>
#include	"ISWXcbDraw.h"

/****************************************************************
 *
 * Box Resources
 *
 ****************************************************************/

static IswResource resources[] = {
    { IswNhSpace, IswCHSpace, IswRDimension, sizeof(Dimension),
		IswOffsetOf(BoxRec, box.h_space),
		IswRImmediate, (IswPointer)4 },
    { IswNvSpace, IswCVSpace, IswRDimension, sizeof(Dimension),
		IswOffsetOf(BoxRec, box.v_space),
		IswRImmediate, (IswPointer)4 },
    { IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
		IswOffsetOf(BoxRec, box.orientation),
		IswRImmediate, (IswPointer)IswOrientVertical },
};

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void ChangeManaged(Widget);
static IswGeometryResult PreferredSize(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

BoxClassRec boxClassRec = {
  {
/* core_class fields      */
    /* superclass         */    (WidgetClass) &compositeClassRec,
    /* class_name         */    "Box",
    /* widget_size        */    sizeof(BoxRec),
    /* class_initialize   */    ClassInitialize,
    /* class_part_init    */	NULL,
    /* class_inited       */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    IswInheritRealize,
    /* actions            */    NULL,
    /* num_actions	  */	0,
    /* resources          */    resources,
    /* num_resources      */    IswNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave*/	TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    Destroy,
    /* resize             */    Resize,
    /* expose             */    Redisplay,
    /* set_values         */    SetValues,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */    IswInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus       */    NULL,
    /* version            */	IswVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	PreferredSize,
    /* display_accelerator*/	IswInheritDisplayAccelerator,
    /* extension          */	NULL
  },{
/* composite_class fields */
    /* geometry_manager   */    GeometryManager,
    /* change_managed     */    ChangeManaged,
    /* insert_child	  */	IswInheritInsertChild,
    /* delete_child	  */	IswInheritDeleteChild,
    /* extension          */	NULL
  },{
/* Box class fields */
    /* empty		  */	0,
  }
};

WidgetClass boxWidgetClass = (WidgetClass)&boxClassRec;


/****************************************************************
 *
 * Private Routines
 *
 ****************************************************************/

/*
 *
 * Do a layout, either actually assigning positions, or just calculating size.
 * Returns minimum width and height that will preserve the same layout.
 *
 */

static void
DoLayout(BoxWidget bbw, Dimension width, Dimension height,
         Dimension *reply_width, Dimension *reply_height, Boolean position)
{
    Boolean vbox = (bbw->box.orientation == IswOrientVertical);
    Cardinal  i;
    Dimension w, h;	/* Width and height needed for box 		*/
    Dimension lw, lh;	/* Width and height needed for current line 	*/
    Dimension bw, bh;	/* Width and height needed for current widget 	*/
    Dimension h_space;  /* Local copy of bbw->box.h_space 		*/
    Widget widget;	/* Current widget	 			*/
    int num_mapped_children = 0;

    /* Box width and height */
    h_space = bbw->box.h_space;

    w = 0;
    for (i = 0; i < bbw->composite.num_children; i++) {
	if ( bbw->composite.children[i]->core.width > w )
            w = bbw->composite.children[i]->core.width;
    }
    w += h_space;
    if ( w > width ) width = w;
    h = bbw->box.v_space;

    /* Line width and height */
    lh = 0;
    lw = h_space;

    for (i = 0; i < bbw->composite.num_children; i++) {
	widget = bbw->composite.children[i];
	if (widget->core.managed) {
	    if (widget->core.mapped_when_managed) num_mapped_children++;
	    /* Compute widget width. Advance by one border width so adjacent
	     * children's borders overlap into a single shared pixel column
	     * (X11 places windows at the outer corner of the border). */
	    bw = widget->core.width + widget->core.border_width + h_space;
	    if ((Dimension)(lw + bw) > width) {
		if (lw > h_space) {
		    /* At least one widget on this line, and
		     * can't fit any more.  Start new line if vbox.
		     */
		    AssignMax(w, lw);
		    if (vbox) {
			h += lh + bbw->box.v_space;
			lh = 0;
			lw = h_space;
		    }
		}
		else if (!position) {
		    /* too narrow for this widget; we'll assume we can grow */
		    DoLayout(bbw, lw + bw, height, reply_width,
			     reply_height, position);
		    return;
		}
	    }
	    if (position && (lw != (Dimension)widget->core.x || h != (Dimension)widget->core.y)) {
		/* It would be nice to use window gravity, but there isn't
		 * sufficient fine-grain control to nicely handle all
		 * situations (e.g. when only the height changes --
		 * a common case).  Explicit unmapping is a cheap hack
		 * to speed things up & avoid the visual jitter as
		 * things slide around.
		 *
		 * %%% perhaps there should be a client resource to
		 * control this.  If so, we'll have to optimize to
		 * perform the moves from the correct end so we don't
		 * force extra exposures as children occlude each other.
		 */
		
		IswMoveWidget(widget, (int)lw, (int)h);
	    }
	    lw += bw;
	    bh = widget->core.height + 2*widget->core.border_width;
	    AssignMax(lh, bh);
	} /* if managed */
    } /* for */

    if (!vbox && width && lw > width && lh < height) {
	/* reduce width if too wide and height not filled */
	Dimension sw = lw, sh = lh;
	Dimension width_needed = 0;
	IswOrientation orientation = bbw->box.orientation;
	bbw->box.orientation = IswOrientVertical;
	while (sh < height && sw > width) {
	    width_needed = sw;
	    DoLayout(bbw, sw-1, height, &sw, &sh, False);
	}
	if (sh < height) width_needed = sw;
	if (width_needed != lw) {
	    DoLayout(bbw,width_needed,height,reply_width,reply_height,position);
	    bbw->box.orientation = orientation;
	    return;
	}
	bbw->box.orientation = orientation;
    }
   if ( vbox && ( ( width < w ) || ( width < lw ) ) ) {
        AssignMax(w, lw);
        DoLayout( bbw, w, height, reply_width, reply_height, position );
        return;
    }
    if (position && IswIsRealized((Widget)bbw)) {
	    int j = bbw->composite.num_children;
	    Widget *childP = bbw->composite.children;
	    for (; j > 0; childP++, j--)
		if (IswIsRealized(*childP) && IswIsManaged(*childP) &&
		    (*childP)->core.mapped_when_managed)
		    IswMapWidget(*childP);
    }

    /* Finish off last line */
    if (lw > h_space) {
	AssignMax(w, lw);
        h += lh + bbw->box.v_space;
    }

    *reply_width = IswMax(w, 1);
    *reply_height = IswMax(h, 1);
}

/*
 *
 * Calculate preferred size, given constraining box, caching it in the widget.
 *
 */

static IswGeometryResult
PreferredSize(Widget widget, IswWidgetGeometry *constraint, IswWidgetGeometry *preferred)
{
    BoxWidget w = (BoxWidget)widget;
    Dimension width /*, height */;
    Dimension preferred_width = w->box.preferred_width;
    Dimension preferred_height = w->box.preferred_height;

    constraint->request_mode &= XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    if (constraint->request_mode == 0)
	/* parent isn't going to change w or h, so nothing to re-compute */
	return IswGeometryYes;

    if (constraint->request_mode == w->box.last_query_mode &&
	(!(constraint->request_mode & XCB_CONFIG_WINDOW_WIDTH) ||
	 constraint->width == w->box.last_query_width) &&
	(!(constraint->request_mode & XCB_CONFIG_WINDOW_HEIGHT) ||
	 constraint->height == w->box.last_query_height)) {
	/* same query; current preferences are still valid */
	preferred->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	preferred->width = preferred_width;
	preferred->height = preferred_height;
	if (constraint->request_mode == (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT) &&
	    constraint->width == preferred_width &&
	    constraint->height == preferred_height)
	    return IswGeometryYes;
	else
	    return IswGeometryAlmost;
    }

    /* else gotta do it the long way...
       I have a preference for tall and narrow, so if my width is
       constrained, I'll accept it; otherwise, I'll compute the minimum
       width that will fit me within the height constraint */

    w->box.last_query_mode = constraint->request_mode;
    w->box.last_query_width = constraint->width;
    w->box.last_query_height= constraint->height;

    if (constraint->request_mode & XCB_CONFIG_WINDOW_WIDTH)
	width = constraint->width;
    else /* if (constraint->request_mode & XCB_CONFIG_WINDOW_HEIGHT) */ {
	 /* let's see if I can become any narrower */
	width = 0;
	constraint->width = 65535;
    }

    /* height is currently ignored by DoLayout.
       height = (constraint->request_mode & XCB_CONFIG_WINDOW_HEIGHT) ? constraint->height
		       : *preferred_height;
     */
    DoLayout(w, width, (Dimension)0,
	     &preferred_width, &preferred_height, FALSE);

    if (constraint->request_mode & XCB_CONFIG_WINDOW_HEIGHT &&
	preferred_height > constraint->height) {
	/* find minimum width for this height */
	if (preferred_width > constraint->width) {
	    /* punt; over-constrained */
	}
	else {
	    width = preferred_width;
	    do { /* find some width big enough to stay within this height */
		if (width > (constraint->width >> 1)) /* avoid short int overflow */
		    width = constraint->width;
		else
		    width <<= 1;
		DoLayout(w, width, 0, &preferred_width, &preferred_height, FALSE);
	    } while (preferred_height > constraint->height
		     && width < constraint->width);
	    if (width != constraint->width) {
		do { /* find minimum width */
		    width = preferred_width;
		    DoLayout(w, preferred_width-1, 0,
			     &preferred_width, &preferred_height, FALSE);
		} while (preferred_height < constraint->height);
		/* one last time */
		DoLayout(w, width, 0, &preferred_width, &preferred_height, FALSE);
	    }
	}
    }

    preferred->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    preferred->width = w->box.preferred_width = preferred_width;
    preferred->height = w->box.preferred_height = preferred_height;

    if (constraint->request_mode == (XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT)
	&& constraint->width == preferred_width
	&& constraint->height == preferred_height)
	return IswGeometryYes;
    else
	return IswGeometryAlmost;

}

/*
 *
 * Actually layout the box
 *
 */

static void
Resize(Widget w)
{
    Dimension junk;

    DoLayout((BoxWidget)w, w->core.width, w->core.height, &junk, &junk, TRUE);

} /* Resize */

/*
 *
 * Try to do a new layout within the current width and height;
 * if that fails try to resize and do it within the box returne
 * by PreferredSize.
 *
 * TryNewLayout just says if it's possible, and doesn't actually move the kids
 */

static Boolean
TryNewLayout(BoxWidget bbw)
{
    Dimension 	preferred_width, preferred_height;
    Dimension	proposed_width, proposed_height;
    int		iterations;

    DoLayout( bbw, bbw->core.width, bbw->core.height,
	      &preferred_width, &preferred_height, FALSE );

    /* at this point, preferred_width is guaranteed to not be greater
       than bbw->core.width unless some child is larger, so there's no
       point in re-computing another layout */

    if ((bbw->core.width == preferred_width) &&
	(bbw->core.height == preferred_height)) {
        /* Same size */
	return (TRUE);
    }

    /* let's see if our parent will go for a new size. */
    iterations = 0;
    proposed_width = preferred_width;
    proposed_height = preferred_height;
    do {
	switch (IswMakeResizeRequest((Widget)bbw,proposed_width,proposed_height,
				     &proposed_width, &proposed_height))
	{
	    case IswGeometryYes:
		return (TRUE);

	    case IswGeometryNo:
		if (iterations > 0)
		    /* protect from malicious parents who change their minds */
		    DoLayout( bbw, bbw->core.width, bbw->core.height,
			      &preferred_width, &preferred_height, FALSE );
		if ((preferred_width <= bbw->core.width) &&
		    (preferred_height <= bbw->core.height))
		    return (TRUE);
		else
		    return (FALSE);

	    case IswGeometryAlmost:
		if (proposed_height >= preferred_height &&
		    proposed_width >= preferred_width) {

		    /*
		     * Take it, and assume the parent knows what it is doing.
		     *
		     * The parent must accept this since it was returned in
		     * almost.
		     *
		     */
		    (void) IswMakeResizeRequest( (Widget)bbw,
				       proposed_width, proposed_height,
				       &proposed_width, &proposed_height);
		    return(TRUE);
		}
		else if (proposed_width != preferred_width) {
		    /* recalc bounding box; height might change */
		    DoLayout(bbw, proposed_width, 0,
			     &preferred_width, &preferred_height, FALSE);
		    proposed_height = preferred_height;
		}
		else { /* proposed_height != preferred_height */
		    IswWidgetGeometry constraints, reply;
		    constraints.request_mode = XCB_CONFIG_WINDOW_HEIGHT;
		    constraints.height = proposed_height;
		    (void)PreferredSize((Widget)bbw, &constraints, &reply);
		    proposed_width = preferred_width;
		}
		break;

	    case IswGeometryDone: /* ??? */
	    default:
		break;
	}
	iterations++;
    } while (iterations < 10);
    return (FALSE);
}

/*
 *
 * Geometry Manager
 *
 * 'reply' is unused; we say only yeay or nay, never almost.
 *
 */

/*ARGSUSED*/
static IswGeometryResult
GeometryManager(Widget w, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    Dimension	width, height, borderWidth;
    BoxWidget bbw;

    /* Position request always denied */
    if ((request->request_mode & XCB_CONFIG_WINDOW_X && request->x != w->core.x) ||
	(request->request_mode & XCB_CONFIG_WINDOW_Y && request->y != w->core.y))
        return (IswGeometryNo);

    /* Size changes must see if the new size can be accomodated */
    if (request->request_mode & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH)) {

	/* Make all three fields in the request valid */
	if ((request->request_mode & XCB_CONFIG_WINDOW_WIDTH) == 0)
	    request->width = w->core.width;
	if ((request->request_mode & XCB_CONFIG_WINDOW_HEIGHT) == 0)
	    request->height = w->core.height;
        if ((request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) == 0)
	    request->border_width = w->core.border_width;

	/* Save current size and set to new size */
	width = w->core.width;
	height = w->core.height;
	borderWidth = w->core.border_width;
	w->core.width = request->width;
	w->core.height = request->height;
	w->core.border_width = request->border_width;

	/* Decide if new layout works: (1) new widget is smaller,
	   (2) new widget fits in existing Box, (3) Box can be
	   expanded to allow new widget to fit */

	bbw = (BoxWidget) w->core.parent;

/* whenever a child changes his geometry, we attempt to
 * change ours to be the minimum enclosing size...
	if (((request->width + request->border_width <= width + borderWidth) &&
	    (request->height + request->border_width <= height + borderWidth))
	|| bbw->box.preferred_width < bbw->core.width
	|| bbw->box.preferred_height < bbw->core.height
	|| TryNewLayout(bbw)) {
 */
	if (TryNewLayout(bbw)) {
	    /* Fits in existing or new space, relayout */
	    (*IswClass((Widget)bbw)->core_class.resize)((Widget)bbw);
	    return (IswGeometryYes);
	} else {
	    /* Cannot satisfy request, change back to original geometry */
	    w->core.width = width;
	    w->core.height = height;
	    w->core.border_width = borderWidth;
	    return (IswGeometryNo);
	}
    }; /* if any size changes requested */

    /* Any stacking changes don't make a difference, so allow if that's all */
    return (IswGeometryYes);
}

static void
ChangeManaged(Widget w)
{
    /* Reconfigure the box */
    (void) TryNewLayout((BoxWidget)w);
    Resize(w);
}

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    BoxWidget bbw = (BoxWidget) w;
    ISWRenderContext *ctx;

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    /* Cache the context: with surface-per-widget the composite pass looks the
       widget's context up in the registry, so it must persist (a transient
       create/destroy would be gone by composite time). */
    ctx = bbw->box.render_ctx;
    if (ctx == NULL)
        ctx = bbw->box.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx == NULL)
        return;

    ISWRenderBegin(ctx);
    /* Fill the Box's own background: as a windowless widget it has no X window
       for the server to background-fill; children composite on top. */
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
    if (w->core.border_width > 0) {
        ISWRenderSetLineWidth(ctx, (double) w->core.border_width);
        ISWRenderStrokeRectangle(ctx, 0, 0, w->core.width, w->core.height);
    }
    ISWRenderEnd(ctx);
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter( IswRString, IswROrientation, ISWCvtStringToOrientation,
		        (IswConvertArgList)NULL, 0, IswCacheNone, (IswDestructor)NULL );
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    BoxWidget newbbw = (BoxWidget)new;

    /* HiDPI: dimensions stay in logical pixels; scaled at X boundary */

    new->core.windowless = True;
    newbbw->box.render_ctx = NULL;

    newbbw->box.last_query_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    newbbw->box.last_query_width = newbbw->box.last_query_height = 0;
    newbbw->box.preferred_width = IswMax(newbbw->box.h_space, 1);
    newbbw->box.preferred_height = IswMax(newbbw->box.v_space, 1);

    if (newbbw->core.width == 0)
        newbbw->core.width = newbbw->box.preferred_width;

    if (newbbw->core.height == 0)
	newbbw->core.height = newbbw->box.preferred_height;

} /* Initialize */

static void
Destroy(Widget w)
{
    BoxWidget bbw = (BoxWidget) w;
    if (bbw->box.render_ctx != NULL) {
        ISWRenderDestroy(bbw->box.render_ctx);
        bbw->box.render_ctx = NULL;
    }
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
   /* need to relayout if h_space or v_space change */

    return False;
}
