/*
Copyright (c) 1989  X Consortium

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
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */

/*
 * Sme.c - Source code for the generic menu entry
 *
 * Date:    September 26, 1989
 *
 * By:      Chris D. Peterson
 *          MIT X Consortium
 *          kit@expo.lcs.mit.edu
 */

#ifdef HAVE_CONFIG_H
#include "config.h"

#endif
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/SimpleMenP.h>
#include <ISW/SmeLineP.h>
#include <ISW/Cardinals.h>

#define offset(field) IswOffsetOf(SmeLineRec, sme_line.field)
static IswResource resources[] = {
  {IswNlineWidth, IswCLineWidth, IswRDimension, sizeof(Dimension),
     offset(line_width), IswRImmediate, (IswPointer) 1},
  {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
     offset(foreground), IswRString, IswDefaultForeground},
};
#undef offset

/*
 * Function definitions.
 */

static void Redisplay(Widget, IswEvent *, IswRegion);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult QueryGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);


#define SUPERCLASS (&smeClassRec)

SmeLineClassRec smeLineClassRec = {
  {
    /* superclass         */    (WidgetClass) SUPERCLASS,
    /* class_name         */    "SmeLine",
    /* size               */    sizeof(SmeLineRec),
    /* class_initialize   */	IswInitializeWidgetSet,
    /* class_part_initialize*/	NULL,
    /* Class init'ed      */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    NULL,
    /* actions            */    NULL,
    /* num_actions        */    ZERO,
    /* resources          */    resources,
    /* resource_count     */	IswNumber(resources),
    /* xrm_class          */    ISW_NULLQUARK,
    /* compress_motion    */    FALSE,
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/ 	FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    NULL,
    /* expose             */    Redisplay,
    /* set_values         */    SetValues,
    /* set_values_hook    */	NULL,
    /* set_values_almost  */	IswInheritSetValuesAlmost,
    /* get_values_hook    */	NULL,
    /* accept_focus       */    NULL,
    /* intrinsics version */	IswVersion,
    /* callback offsets   */    NULL,
    /* tm_table		  */    NULL,
    /* query_geometry	  */    QueryGeometry,
    /* display_accelerator*/    NULL,
    /* extension	  */    NULL
  },{
    /* Menu Entry Fields */

    /* highlight */             IswInheritHighlight,
    /* unhighlight */           IswInheritUnhighlight,
    /* notify */		IswInheritNotify,
    /* extension */             NULL
  },{
    /* Line Menu Entry Fields */
    /* extension */             NULL
  }
};

WidgetClass smeLineObjectClass = (WidgetClass) &smeLineClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*      Function Name: Initialize
 *      Description: Initializes the simple menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 */

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SmeLineObject entry = (SmeLineObject) new;

    entry->sme_line.line_width = (entry->sme_line.line_width);

    if (entry->rectangle.height == 0)
	entry->rectangle.height = entry->sme_line.line_width;

}

/*	Function Name: Redisplay
 *	Description: Paints the Line
 *	Arguments: w - the menu entry.
 *                 event, region - NOT USED.
 *	Returns: none
 */

/*ARGSUSED*/
static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    SmeLineObject entry = (SmeLineObject) w;
    SimpleMenuWidget smw = (SimpleMenuWidget) IswParent (w);
    Dimension s = ((SimpleMenuWidget)IswParent(w))->core.border_width;  /* inset from SimpleMenu's 1px drawn border */
    int y = entry->rectangle.y +
	    (int)(entry->rectangle.height - entry->sme_line.line_width) / 2;

    /* Use the parent SimpleMenu's shared render context */
    ISWRenderContext *ctx = smw->simple_menu.render_ctx;

    if (ctx && IswIsRealized(w)) {
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, entry->sme_line.foreground);
        ISWRenderFillRectangle(ctx, s, y,
                              entry->rectangle.width - 2 * s,
                              entry->sme_line.line_width);
        ISWRenderEnd(ctx);
    }
}

/*      Function Name: SetValues
 *      Description: Relayout the menu when one of the resources is changed.
 *      Arguments: current - current state of the widget.
 *                 request - what was requested.
 *                 new - what the widget will become.
 *      Returns: none
 */

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SmeLineObject entry = (SmeLineObject) new;
    SmeLineObject old_entry = (SmeLineObject) current;

    if (entry->sme_line.line_width != old_entry->sme_line.line_width ||
	entry->sme_line.foreground != old_entry->sme_line.foreground)
	return(TRUE);

    return(FALSE);
}

/*	Function Name: QueryGeometry.
 *	Description: Returns the preferred geometry for this widget.
 *	Arguments: w - the menu entry object.
 *                 itended, return - the intended and return geometry info.
 *	Returns: A Geometry Result.
 *
 * See the Intrinsics manual for details on what this function is for.
 *
 * I just return the height and a width of 1.
 */

static IswGeometryResult
QueryGeometry(Widget w, IswWidgetGeometry *intended, IswWidgetGeometry *return_val)
{
    SmeObject entry = (SmeObject) w;
    Dimension width;
    IswGeometryResult ret_val = IswGeometryYes;
    IswGeometryMask mode = intended->request_mode;

    width = 1;			/* we can be really small. */

    if ( ((mode & IswCWWidth) && (intended->width != width)) ||
	 !(mode & IswCWWidth) ) {
	return_val->request_mode |= IswCWWidth;
	return_val->width = width;
	mode = return_val->request_mode;

	if ( (mode & IswCWWidth) && (width == entry->rectangle.width) )
	    return(IswGeometryNo);
	return(IswGeometryAlmost);
    }
    return(ret_val);
}
