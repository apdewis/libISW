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

/* NOTE: THIS IS NOT A WIDGET!  Rather, this is an interface to a widget.
   It implements policy, and gives a (hopefully) easier-to-use interface
   than just directly making your own form. */


#include <ISW/IntrinsicP.h>
#include <X11/Xos.h>
#include <ISW/StringDefs.h>

#include <ISW/ISWInit.h>
#include <ISW/Text.h>
#include <ISW/Command.h>
#include <ISW/Label.h>
#include <ISW/DialogP.h>
#include <ISW/Cardinals.h>

/*
 * After we have set the string in the value widget we set the
 * string to a magic value.  So that when a SetValues request is made
 * on the dialog value we will notice it, and reset the string.
 */

#define MAGIC_VALUE ((char *) 3)

#define streq(a,b) (strcmp( (a), (b) ) == 0)

static IswResource resources[] = {
  {IswNlabel, IswCLabel, IswRString, sizeof(String),
     IswOffsetOf(DialogRec, dialog.label), IswRString, NULL},
  {IswNvalue, IswCValue, IswRString, sizeof(String),
     IswOffsetOf(DialogRec, dialog.value), IswRString, NULL},
  {IswNicon, IswCIcon, IswRBitmap, sizeof(xcb_pixmap_t),
     IswOffsetOf(DialogRec, dialog.icon), IswRImmediate, 0},
};

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void ConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static void CreateDialogValueWidget(Widget);
static void GetValuesHook(Widget, ArgList, Cardinal *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

DialogClassRec dialogClassRec = {
  { /* core_class fields */
    /* superclass         */    (WidgetClass) &formClassRec,
    /* class_name         */    "Dialog",
    /* widget_size        */    sizeof(DialogRec),
    /* class_initialize   */    IswInitializeWidgetSet,
    /* class_part init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */    NULL,
    /* realize            */    IswInheritRealize,
    /* actions            */    NULL,
    /* num_actions        */    0,
    /* resources          */    resources,
    /* num_resources      */    IswNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    TRUE,
    /* compress_exposure  */    TRUE,
    /* compress_enterleave*/    TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    IswInheritResize,
    /* expose             */    IswInheritExpose,
    /* set_values         */    SetValues,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    IswInheritSetValuesAlmost,
    /* get_values_hook    */    GetValuesHook,
    /* accept_focus       */    NULL,
    /* version            */    IswVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	IswInheritQueryGeometry,
    /* display_accelerator*/	IswInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */   IswInheritGeometryManager,
    /* change_managed     */   IswInheritChangeManaged,
    /* insert_child       */   IswInheritInsertChild,
    /* delete_child       */   IswInheritDeleteChild,
    /* extension          */   NULL
  },
  { /* constraint_class fields */
    /* subresourses       */   NULL,
    /* subresource_count  */   0,
    /* constraint_size    */   sizeof(DialogConstraintsRec),
    /* initialize         */   ConstraintInitialize,
    /* destroy            */   NULL,
    /* set_values         */   NULL,
    /* extension          */   NULL
  },
  { /* form_class fields */
    /* layout             */   IswInheritLayout
  },
  { /* dialog_class fields */
    /* empty              */   0
  }
};

WidgetClass dialogWidgetClass = (WidgetClass)&dialogClassRec;

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    DialogWidget dw = (DialogWidget)new;
    Arg arglist[9];
    Cardinal arg_cnt = 0;

    IswSetArg(arglist[arg_cnt], IswNborderWidth, 0); arg_cnt++;
    IswSetArg(arglist[arg_cnt], IswNleft, IswChainLeft); arg_cnt++;

    if (dw->dialog.icon != (xcb_pixmap_t)0) {
	IswSetArg(arglist[arg_cnt], IswNbitmap, dw->dialog.icon); arg_cnt++;
	IswSetArg(arglist[arg_cnt], IswNright, IswChainLeft); arg_cnt++;
	dw->dialog.iconW =
	    IswCreateManagedWidget( "icon", labelWidgetClass,
				   new, arglist, arg_cnt );
	arg_cnt = 2;
	IswSetArg(arglist[arg_cnt], IswNfromHoriz, dw->dialog.iconW);arg_cnt++;
    } else dw->dialog.iconW = (Widget)NULL;

    IswSetArg(arglist[arg_cnt], IswNlabel, dw->dialog.label); arg_cnt++;
    IswSetArg(arglist[arg_cnt], IswNright, IswChainRight); arg_cnt++;

    dw->dialog.labelW = IswCreateManagedWidget( "label", labelWidgetClass,
					      new, arglist, arg_cnt);

    if (dw->dialog.iconW != (Widget)NULL &&
	(dw->dialog.labelW->core.height < dw->dialog.iconW->core.height)) {
	IswSetArg( arglist[0], IswNheight, dw->dialog.iconW->core.height );
	IswSetValues( dw->dialog.labelW, arglist, ONE );
    }
    if (dw->dialog.value != NULL)
        CreateDialogValueWidget( (Widget) dw);
    else
        dw->dialog.valueW = NULL;
}

/* ARGSUSED */
static void
ConstraintInitialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    DialogWidget dw = (DialogWidget)new->core.parent;
    DialogConstraints constraint = (DialogConstraints)new->core.constraints;

    if (!IswIsSubclass(new, commandWidgetClass))	/* if not a button */
	return;					/* then just use defaults */

    constraint->form.left = constraint->form.right = IswChainLeft;
    if (dw->dialog.valueW == NULL)
      constraint->form.vert_base = dw->dialog.labelW;
    else
      constraint->form.vert_base = dw->dialog.valueW;

    if (dw->composite.num_children > 1) {
	WidgetList children = dw->composite.children;
	Widget *childP;
        for (childP = children + dw->composite.num_children - 1;
	     childP >= children; childP-- ) {
	    if (*childP == dw->dialog.labelW || *childP == dw->dialog.valueW)
	        break;
	    if (IswIsManaged(*childP) &&
		IswIsSubclass(*childP, commandWidgetClass)) {
	        constraint->form.horiz_base = *childP;
		break;
	    }
	}
    }
}

#define ICON 0
#define LABEL 1
#define NUM_CHECKS 2

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList in_args, Cardinal *in_num_args)
{
    DialogWidget w = (DialogWidget)new;
    DialogWidget old = (DialogWidget)current;
    Arg args[5];
    Cardinal num_args;
    int i;
    Boolean checks[NUM_CHECKS];

    for (i = 0; i < NUM_CHECKS; i++)
	checks[i] = FALSE;

    for (i = 0; i < *in_num_args; i++) {
	if (streq(IswNicon, in_args[i].name))
	    checks[ICON] = TRUE;
	if (streq(IswNlabel, in_args[i].name))
	    checks[LABEL] = TRUE;
    }

    if (checks[ICON]) {
	if (w->dialog.icon != (xcb_pixmap_t)0) {
	    IswSetArg( args[0], IswNbitmap, w->dialog.icon );
	    if (old->dialog.iconW != (Widget)NULL) {
		IswSetValues( old->dialog.iconW, args, ONE );
	    } else {
		IswSetArg( args[1], IswNborderWidth, 0);
		IswSetArg( args[2], IswNleft, IswChainLeft);
		IswSetArg( args[3], IswNright, IswChainLeft);
		w->dialog.iconW =
		    IswCreateWidget( "icon", labelWidgetClass,
				    new, args, FOUR );
		((DialogConstraints)w->dialog.labelW->core.constraints)->
		    form.horiz_base = w->dialog.iconW;
		IswManageChild(w->dialog.iconW);
	    }
	} else if (old->dialog.icon != (xcb_pixmap_t)0) {
	    ((DialogConstraints)w->dialog.labelW->core.constraints)->
		    form.horiz_base = (Widget)NULL;
	    IswDestroyWidget(old->dialog.iconW);
	    w->dialog.iconW = (Widget)NULL;
	}
    }

    if ( checks[LABEL] ) {
        num_args = 0;
        IswSetArg( args[num_args], IswNlabel, w->dialog.label ); num_args++;
	if (w->dialog.iconW != (Widget)NULL &&
	    (w->dialog.labelW->core.height <= w->dialog.iconW->core.height)) {
	    IswSetArg(args[num_args], IswNheight, w->dialog.iconW->core.height);
	    num_args++;
	}
	IswSetValues( w->dialog.labelW, args, num_args );
    }

    if ( w->dialog.value != old->dialog.value ) {
        if (w->dialog.value == NULL)  /* only get here if it
					  wasn't NULL before. */
	    IswDestroyWidget(old->dialog.valueW);
	else if (old->dialog.value == NULL) { /* create a new value widget. */
	    w->core.width = old->core.width;
	    w->core.height = old->core.height;
#ifdef notdef
/* this would be correct if Form had the same semantics on Resize
 * as on MakeGeometryRequest.  Unfortunately, Form botched it, so
 * any subclasses will currently have to deal with the fact that
 * we're about to change our real size.
 */
	    w->form.resize_in_layout = False;
	    CreateDialogValueWidget( (Widget) w);
	    w->core.width = w->form.preferred_width;
	    w->core.height = w->form.preferred_height;
	    w->form.resize_in_layout = True;
#else /*notdef*/
	    CreateDialogValueWidget( (Widget) w);
#endif /*notdef*/
	}
	else {			/* Widget ok, just change string. */
	    Arg args[1];
	    IswSetArg(args[0], IswNstring, w->dialog.value);
	    IswSetValues(w->dialog.valueW, args, ONE);
	    w->dialog.value = MAGIC_VALUE;
	}
    }
    return False;
}

/*	Function Name: GetValuesHook
 *	Description: This is a get values hook routine that gets the
 *                   values in the dialog.
 *	Arguments: w - the Text Widget.
 *                 args - the argument list.
 *                 num_args - the number of args.
 *	Returns: none.
 */

static void
GetValuesHook(Widget w, ArgList args, Cardinal *num_args)
{
  Arg a[1];
  String s;
  DialogWidget src = (DialogWidget) w;
  int i;

  for (i=0; i < *num_args; i++)
    if (streq(args[i].name, IswNvalue)) {
      IswSetArg(a[0], IswNstring, &s);
      IswGetValues(src->dialog.valueW, a, 1);
      *((char **) args[i].value) = s;
    }
}


/*	Function Name: CreateDialogValueWidget
 *	Description: Creates the dialog widgets value widget.
 *	Arguments: w - the dialog widget.
 *	Returns: none.
 *
 *	must be called only when w->dialog.value is non-nil.
 */

static void
CreateDialogValueWidget(Widget w)
{
    DialogWidget dw = (DialogWidget) w;
    Arg arglist[10];
    Cardinal num_args = 0;

#ifdef notdef
    IswSetArg(arglist[num_args], IswNwidth,
	     dw->dialog.labelW->core.width); num_args++; /* ||| hack */
#endif /*notdef*/
    IswSetArg(arglist[num_args], IswNstring, dw->dialog.value);     num_args++;
    IswSetArg(arglist[num_args], IswNresizable, True);              num_args++;
    IswSetArg(arglist[num_args], IswNresize, IswtextResizeBoth);    num_args++;
    IswSetArg(arglist[num_args], IswNeditType, IswtextEdit);        num_args++;
    IswSetArg(arglist[num_args], IswNfromVert, dw->dialog.labelW);  num_args++;
    IswSetArg(arglist[num_args], IswNleft, IswChainLeft);            num_args++;
    IswSetArg(arglist[num_args], IswNright, IswChainRight);          num_args++;

    dw->dialog.valueW = IswCreateWidget("value", textWidgetClass,
				     w, arglist, num_args);

    /* if the value widget is being added after buttons,
     * then the buttons need new layout constraints.
     */
    if (dw->composite.num_children > 1) {
	WidgetList children = dw->composite.children;
	Widget *childP;
        for (childP = children + dw->composite.num_children - 1;
	     childP >= children; childP-- ) {
	    if (*childP == dw->dialog.labelW || *childP == dw->dialog.valueW)
		continue;
	    if (IswIsManaged(*childP) &&
		IswIsSubclass(*childP, commandWidgetClass)) {
	        ((DialogConstraints)(*childP)->core.constraints)->
		    form.vert_base = dw->dialog.valueW;
	    }
	}
    }
    IswManageChild(dw->dialog.valueW);

/*
 * Value widget gets the keyboard focus.
 */

    IswSetKeyboardFocus(w, dw->dialog.valueW);
    dw->dialog.value = MAGIC_VALUE;
}


void
IswDialogAddButton(Widget dialog, _Xconst char* name, IswCallbackProc function,
		   IswPointer param)
{
/*
 * Correct Constraints are all set in ConstraintInitialize().
 */
    Widget button;

    button = IswCreateManagedWidget( name, commandWidgetClass, dialog,
				    (ArgList)NULL, (Cardinal)0 );

    if (function != NULL)	/* don't add NULL callback func. */
        IswAddCallback(button, IswNcallback, function, param);
}


char *
IswDialogGetValueString(Widget w)
{
    Arg args[1];
    char * value;

    IswSetArg(args[0], IswNstring, &value);
    IswGetValues( ((DialogWidget)w)->dialog.valueW, args, ONE);
    return(value);
}
