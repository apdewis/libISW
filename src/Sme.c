/*
Copyright (c) 1989, 1994  X Consortium

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
#include <ISW/ISWP.h>
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/SmeP.h>
#include <ISW/Cardinals.h>

#define offset(field) IswOffsetOf(SmeRec, sme.field)
static IswResource resources[] = {
  {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
     offset(callbacks), IswRCallback, (IswPointer)NULL},
  {IswNinternational, IswCInternational, IswRBoolean, sizeof(Boolean),
     offset(international), IswRImmediate, (IswPointer) FALSE},
};
#undef offset

/*
 * Semi Public function definitions.
 */

static void Unhighlight(Widget);
static void Highlight(Widget);
static void Notify(Widget);
static void ClassPartInitialize(WidgetClass);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult QueryGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

#define SUPERCLASS (&rectObjClassRec)

SmeClassRec smeClassRec = {
  {
    /* superclass         */    (WidgetClass) SUPERCLASS,
    /* class_name         */    "Sme",
    /* size               */    sizeof(SmeRec),
    /* class_initialize   */	IswInitializeWidgetSet,
    /* class_part_initialize*/	ClassPartInitialize,
    /* Class init'ed      */	FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */	NULL,
    /* realize            */    NULL,
    /* actions            */    NULL,
    /* num_actions        */    ZERO,
    /* resources          */    resources,
    /* resource_count     */	IswNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    FALSE,
    /* compress_exposure  */    FALSE,
    /* compress_enterleave*/ 	FALSE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    NULL,
    /* expose             */    NULL,
    /* set_values         */    NULL,
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
    /* Simple Menu Entry Fields */

    /* highlight */             Highlight,
    /* unhighlight */           Unhighlight,
    /* notify */		Notify,
    /* extension */             NULL
  }
};

WidgetClass smeObjectClass = (WidgetClass) &smeClassRec;

/************************************************************
 *
 * Semi-Public Functions.
 *
 ************************************************************/

/*	Function Name: ClassPartInitialize
 *	Description: handles inheritance of class functions.
 *	Arguments: class - the widget classs of this widget.
 *	Returns: none.
 */

static void
ClassPartInitialize(WidgetClass class)
{
    SmeObjectClass m_ent, superC;

    m_ent = (SmeObjectClass) class;
    superC = (SmeObjectClass) m_ent->rect_class.superclass;

/*
 * We don't need to check for null super since we'll get to TextSink
 * eventually.
 */

    if (m_ent->sme_class.highlight == IswInheritHighlight)
	m_ent->sme_class.highlight = superC->sme_class.highlight;

    if (m_ent->sme_class.unhighlight == IswInheritUnhighlight)
	m_ent->sme_class.unhighlight = superC->sme_class.unhighlight;

    if (m_ent->sme_class.notify == IswInheritNotify)
	m_ent->sme_class.notify = superC->sme_class.notify;
}

/*      Function Name: Initialize
 *      Description: Initializes the simple menu widget
 *      Arguments: request - the widget requested by the argument list.
 *                 new     - the new widget with both resource and non
 *                           resource values.
 *      Returns: none.
 *
 * MENU ENTRIES CANNOT HAVE BORDERS.
 */

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SmeObject entry = (SmeObject) new;

    entry->rectangle.border_width = 0;
}

/*	Function Name: Highlight
 *	Description: The default highlight proceedure for menu entries.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

/* ARGSUSED */
static void
Highlight(Widget w)
{
/* This space intentionally left blank. */
}

/*	Function Name: Unhighlight
 *	Description: The default unhighlight proceedure for menu entries.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

/* ARGSUSED */
static void
Unhighlight(Widget w)
{
/* This space intentionally left blank. */
}

/*	Function Name: Notify
 *	Description: calls the callback proceedures for this entry.
 *	Arguments: w - the menu entry.
 *	Returns: none.
 */

static void
Notify(Widget w)
{
    IswCallCallbacks(w, IswNcallback, (IswPointer)NULL);
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
