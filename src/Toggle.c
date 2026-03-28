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
 * Toggle.c - Toggle button widget
 *
 * Author: Chris D. Peterson
 *         MIT X Consortium
 *         kit@expo.lcs.mit.edu
 *
 * Date:   January 12, 1989
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <math.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/Label.h>
#include <ISW/ToggleP.h>
#include <xcb/xcb.h>
#include <cairo/cairo.h>

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

/*
 * The order of toggle and notify are important, as the state has
 * to be set when we call the notify proc.
 */

static char defaultTranslations[] =
    "<Btn1Down>,<Btn1Up>:   toggle() notify()";

#define offset(field) XtOffsetOf(ToggleRec, field)

static XtResource resources[] = {
   {XtNstate, XtCState, XtRBoolean, sizeof(Boolean),
      offset(command.set), XtRString, "off"},
   {XtNradioGroup, XtCWidget, XtRWidget, sizeof(Widget),
      offset(toggle.widget), XtRWidget, (XtPointer) NULL },
   {XtNradioData, XtCRadioData, XtRPointer, sizeof(XtPointer),
      offset(toggle.radio_data), XtRPointer, (XtPointer) NULL },
};

#undef offset


static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Toggle(Widget, XEvent *, String *, Cardinal *);
static void Notify(Widget, XEvent *, String *, Cardinal *);
static void ToggleSet(Widget, XEvent *, String *, Cardinal *);
static void ToggleDestroy(Widget, XtPointer, XtPointer);
static void ClassInit(void);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

/* Functions for handling the Radio Group. */

static RadioGroup * GetRadioGroup(Widget);
static void CreateRadioGroup(Widget, Widget);
static void AddToRadioGroup(RadioGroup *, Widget);
static void TurnOffRadioSiblings(Widget);
static void RemoveFromRadioGroup(Widget);

/* Forward declarations for custom Set/Unset that don't clear the widget */
static void ToggleSetAction(Widget, XEvent *, String *, Cardinal *);
static void ToggleUnsetAction(Widget, XEvent *, String *, Cardinal *);

static XtActionsRec actionsList[] =
{
  {"toggle",	        Toggle},
  {"notify",	        Notify},
  {"set",	        ToggleSetAction},
  {"unset",             ToggleUnsetAction},
};

#define SuperClass ((CommandWidgetClass)&commandClassRec)

/* Forward declaration for Redisplay */
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);

ToggleClassRec toggleClassRec = {
  {
    (WidgetClass) SuperClass,		/* superclass		  */
    "Toggle",				/* class_name		  */
    sizeof(ToggleRec),			/* size			  */
    ClassInit,				/* class_initialize	  */
    NULL,				/* class_part_initialize  */
    FALSE,				/* class_inited		  */
    Initialize,				/* initialize		  */
    NULL,				/* initialize_hook	  */
    XtInheritRealize,			/* realize		  */
    actionsList,			/* actions		  */
    XtNumber(actionsList),		/* num_actions		  */
    resources,				/* resources		  */
    XtNumber(resources),		/* resource_count	  */
    NULLQUARK,				/* xrm_class		  */
    FALSE,				/* compress_motion	  */
    TRUE,				/* compress_exposure	  */
    TRUE,				/* compress_enterleave    */
    FALSE,				/* visible_interest	  */
    NULL,         			/* destroy		  */
    XtInheritResize,			/* resize		  */
    Redisplay,				/* expose		  */
    SetValues,				/* set_values		  */
    NULL,				/* set_values_hook	  */
    XtInheritSetValuesAlmost,		/* set_values_almost	  */
    NULL,				/* get_values_hook	  */
    NULL,				/* accept_focus		  */
    XtVersion,				/* version		  */
    NULL,				/* callback_private	  */
    defaultTranslations,		/* tm_table		  */
    XtInheritQueryGeometry,		/* query_geometry	  */
    XtInheritDisplayAccelerator,	/* display_accelerator	  */
    NULL				/* extension		  */
  },  /* CoreClass fields initialization */
  {
    XtInheritChangeSensitive		/* change_sensitive	  */
  },  /* SimpleClass fields initialization */
  {
    0                                     /* field not used    */
  },  /* LabelClass fields initialization */
  {
    0                                     /* field not used    */
  },  /* CommandClass fields initialization */
  {
      NULL,			        /* Set Procedure. */
      NULL,			        /* Unset Procedure. */
      NULL			        /* extension. */
  }  /* ToggleClass fields initialization */
};

  /* for public consumption */
WidgetClass toggleWidgetClass = (WidgetClass) &toggleClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

/*
 * ISWCvtStringToWidget - Convert string widget name to Widget
 * Used for radioGroup resource conversion
 */
/*ARGSUSED*/
static Boolean
ISWCvtStringToWidget(xcb_connection_t *dpy, XrmValuePtr args, Cardinal *num_args,
                     XrmValuePtr fromVal, XrmValuePtr toVal, XtPointer *data)
{
    Widget widget;
    Widget parent;
    
    if (*num_args != 1) {
        XtAppWarningMsg(XtDisplayToApplicationContext(dpy),
                       "wrongParameters", "cvtStringToWidget", "IswError",
                       "String to Widget conversion needs parent argument",
                       (String *)NULL, (Cardinal *)NULL);
        return False;
    }
    
    parent = *(Widget*)args[0].addr;
    if (parent == NULL) {
        XtAppWarningMsg(XtDisplayToApplicationContext(dpy),
                       "missingParent", "cvtStringToWidget", "IswError",
                       "String to Widget conversion: parent is NULL",
                       (String *)NULL, (Cardinal *)NULL);
        return False;
    }
    
    widget = XtNameToWidget(parent, (char*)fromVal->addr);
    if (widget == NULL) {
        XtAppWarningMsg(XtDisplayToApplicationContext(dpy),
                       "noWidget", "cvtStringToWidget", "IswError",
                       "Cannot find widget '%s'",
                       (String *)&fromVal->addr, (Cardinal *)NULL);
        return False;
    }
    
    if (toVal->addr != NULL) {
        if (toVal->size < sizeof(Widget)) {
            toVal->size = sizeof(Widget);
            return False;
        }
        *(Widget*)(toVal->addr) = widget;
    } else {
        static Widget widget_ret;
        widget_ret = widget;
        toVal->addr = (XtPointer)&widget_ret;
    }
    
    toVal->size = sizeof(Widget);
    return True;
}

static void
ClassInit(void)
{
  ToggleWidgetClass class = (ToggleWidgetClass) toggleWidgetClass;
  static XtConvertArgRec parentCvtArgs[] = {
      {XtBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.parent),
	   sizeof(Widget)}
  };

  IswInitializeWidgetSet();
  XtSetTypeConverter(XtRString, XtRWidget, ISWCvtStringToWidget,
		     parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		     (XtDestructor)NULL);
/*
 * Use Toggle's own Set/Unset actions so that state changes redraw
 * through Toggle's Redisplay (which draws checkbox/radio indicators)
 * rather than Command's PaintCommandWidget.
 */

  class->toggle_class.Set = ToggleSetAction;
  class->toggle_class.Unset = ToggleUnsetAction;
}

/*ARGSUSED*/
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ToggleWidget tw = (ToggleWidget) new;
    ToggleWidget tw_req = (ToggleWidget) request;

    tw->toggle.radio_group = NULL;

    if (tw->toggle.radio_data == NULL)
      tw->toggle.radio_data = (XtPointer) new->core.name;

    if (tw->toggle.widget != NULL) {
      if ( GetRadioGroup(tw->toggle.widget) == NULL)
	CreateRadioGroup(new, tw->toggle.widget);
      else
	AddToRadioGroup( GetRadioGroup(tw->toggle.widget), new);
    }
    XtAddCallback(new, XtNdestroyCallback, ToggleDestroy, (XtPointer)NULL);

    /*
     * Reserve space on the left for the radio button or checkbox indicator.
     * We increase the internal_width (left padding) to make room.
     */
    {
        /* Reserve space for the indicator: cap height + gap, derived from
         * actual Cairo font metrics so it adapts to any DPI/font. */
        int cap_h = ISWScaledFontCapHeight(new, tw->label.font);
        int gap = cap_h;  /* ~1em spacing between indicator and label */
        Dimension min_iw = (Dimension)(cap_h + gap);
        if (tw->label.internal_width < min_iw) {
            Dimension old_iw = tw->label.internal_width;
            tw->label.internal_width = min_iw;
            /* Label's Initialize already computed core.width using the old
             * internal_width.  Widen the widget to account for the difference. */
            if (tw->core.width > 0)
                tw->core.width += 2 * (min_iw - old_iw);
        }
    }

    /*
     * Remove button chrome - radio buttons and checkboxes should look
     * like plain text with indicators, not like pressed buttons.
     */
    tw->command.border_stroke_width = 0;

/*
 * Command widget assumes that the widget is unset, so we only
 * have to handle the case where it needs to be set.
 *
 * If this widget is in a radio group then it may cause another
 * widget to be unset, thus calling the notify proceedure.
 *
 * I want to set the toggle if the user set the state to "On" in
 * the resource group, reguardless of what my ancestors did.
 */

    /* Re-run Resize to recalculate label_x with the updated internal_width */
    (*XtClass(new)->core_class.resize)(new);

    if (tw_req->command.set)
      ToggleSet(new, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
}

/************************************************************
 *
 *  Action Procedures
 *
 ************************************************************/

/* ARGSUSED */
/*	Function Name: ToggleSetAction
 *	Description: Custom Set action that redraws everything properly
 *	Arguments: standard action arguments
 *	Returns: none.
 */

static void
ToggleSetAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ToggleWidget cbw = (ToggleWidget)w;

    if (cbw->command.set)
        return;

    cbw->command.set = TRUE;
    if (XtIsRealized(w)) {
        /* Clear background and completely redraw */
        ISWRenderContext *ctx = cbw->label.render_ctx;
        if (ctx) {
            ISWRenderBegin(ctx);
            ISWRenderSetColor(ctx, w->core.background_pixel);
            ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
            ISWRenderEnd(ctx);
        }
        Redisplay(w, NULL, 0);
    }
}

/*	Function Name: ToggleUnsetAction
 *	Description: Custom Unset action that redraws everything properly
 *	Arguments: standard action arguments
 *	Returns: none.
 */

static void
ToggleUnsetAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ToggleWidget cbw = (ToggleWidget)w;

    if (!cbw->command.set)
        return;

    cbw->command.set = FALSE;
    if (XtIsRealized(w)) {
        /* Clear background and completely redraw */
        ISWRenderContext *ctx = cbw->label.render_ctx;
        if (ctx) {
            ISWRenderBegin(ctx);
            ISWRenderSetColor(ctx, w->core.background_pixel);
            ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
            ISWRenderEnd(ctx);
        }
        Redisplay(w, NULL, 0);
    }
}

static void
ToggleSet(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    TurnOffRadioSiblings(w);
    ToggleSetAction(w, event, NULL, 0);
}

/* ARGSUSED */
static void
Toggle(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  ToggleWidget tw = (ToggleWidget)w;
  ToggleWidgetClass class = (ToggleWidgetClass) w->core.widget_class;

  if (tw->command.set)
    class->toggle_class.Unset(w, event, NULL, 0);
  else
    ToggleSet(w, event, params, num_params);
}

/* ARGSUSED */
static void Notify(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  ToggleWidget tw = (ToggleWidget) w;
  long antilint = tw->command.set;

  XtCallCallbacks( w, XtNcallback, (XtPointer) antilint );
}

/************************************************************
 *
 * Set specified arguments into widget
 *
 ***********************************************************/

/* ARGSUSED */
static Boolean
SetValues (Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ToggleWidget oldtw = (ToggleWidget) current;
    ToggleWidget tw = (ToggleWidget) new;
    ToggleWidget rtw = (ToggleWidget) request;

    if (oldtw->toggle.widget != tw->toggle.widget)
      IswToggleChangeRadioGroup(new, tw->toggle.widget);

    if (!tw->core.sensitive && oldtw->core.sensitive && rtw->command.set)
	tw->command.set = True;

    if (oldtw->command.set != tw->command.set) {
	tw->command.set = oldtw->command.set;
	Toggle(new, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
    }
    return(FALSE);
}

/*	Function Name: ToggleDestroy
 *	Description: Destroy Callback for toggle widget.
 *	Arguments: w - the toggle widget that is being destroyed.
 *                 junk, grabage - not used.
 *	Returns: none.
 */

/* ARGSUSED */
static void
ToggleDestroy(Widget w, XtPointer junk, XtPointer garbage)
{
  RemoveFromRadioGroup(w);
}

/************************************************************
 *
 * Below are all the private procedures that handle
 * radio toggle buttons.
 *
 ************************************************************/

/*	Function Name: GetRadioGroup
 *	Description: Gets the radio group associated with a give toggle
 *                   widget.
 *	Arguments: w - the toggle widget who's radio group we are getting.
 *	Returns: the radio group associated with this toggle group.
 */

static RadioGroup *
GetRadioGroup(Widget w)
{
  ToggleWidget tw = (ToggleWidget) w;

  if (tw == NULL) return(NULL);
  return( tw->toggle.radio_group );
}

/*	Function Name: CreateRadioGroup
 *	Description: Creates a radio group. give two widgets.
 *	Arguments: w1, w2 - the toggle widgets to add to the radio group.
 *	Returns: none.
 *
 *      NOTE:  A pointer to the group is added to each widget's radio_group
 *             field.
 */

static void
CreateRadioGroup(Widget w1, Widget w2)
{
  char error_buf[BUFSIZ];
  ToggleWidget tw1 = (ToggleWidget) w1;
  ToggleWidget tw2 = (ToggleWidget) w2;

  if ( (tw1->toggle.radio_group != NULL) || (tw2->toggle.radio_group != NULL) ) {
    (void) sprintf(error_buf, "%s %s", "Toggle Widget Error - Attempting",
	    "to create a new toggle group, when one already exists.");
    XtWarning(error_buf);
  }

  AddToRadioGroup( (RadioGroup *)NULL, w1 );
  AddToRadioGroup( GetRadioGroup(w1), w2 );
}

/*	Function Name: AddToRadioGroup
 *	Description: Adds a toggle to the radio group.
 *	Arguments: group - any element of the radio group the we are adding to.
 *                 w - the new toggle widget to add to the group.
 *	Returns: none.
 */

static void
AddToRadioGroup(RadioGroup *group, Widget w)
{
  ToggleWidget tw = (ToggleWidget) w;
  RadioGroup * local;

  local = (RadioGroup *) XtMalloc( sizeof(RadioGroup) );
  local->widget = w;
  tw->toggle.radio_group = local;

  if (group == NULL) {		/* Creating new group. */
    group = local;
    group->next = NULL;
    group->prev = NULL;
    return;
  }
  local->prev = group;		/* Adding to previous group. */
  if ((local->next = group->next) != NULL)
      local->next->prev = local;
  group->next = local;
}

/*	Function Name: TurnOffRadioSiblings
 *	Description: Deactivates all radio siblings.
 *	Arguments: widget - a toggle widget.
 *	Returns: none.
 */

static void
TurnOffRadioSiblings(Widget w)
{
  RadioGroup * group;
  ToggleWidgetClass class = (ToggleWidgetClass) w->core.widget_class;

  if ( (group = GetRadioGroup(w)) == NULL)  /* Punt if there is no group */
    return;

  /* Go to the top of the group. */

  for ( ; group->prev != NULL ; group = group->prev );

  while ( group != NULL ) {
    ToggleWidget local_tog = (ToggleWidget) group->widget;
    if ( local_tog->command.set ) {
      /* Use our custom unset action that doesn't clear the widget */
      ToggleUnsetAction(group->widget, NULL, NULL, 0);
      Notify( group->widget, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
    }
    group = group->next;
  }
}

/*	Function Name: RemoveFromRadioGroup
 *	Description: Removes a toggle from a RadioGroup.
 *	Arguments: w - the toggle widget to remove.
 *	Returns: none.
 */

static void
RemoveFromRadioGroup(Widget w)
{
  RadioGroup * group = GetRadioGroup(w);
  if (group != NULL) {
    if (group->prev != NULL)
      (group->prev)->next = group->next;
    if (group->next != NULL)
      (group->next)->prev = group->prev;
    XtFree((char *) group);
  }
}

/************************************************************
 *
 * Toggle State Indicator Drawing Helpers
 *
 ************************************************************/

/*	Function Name: DrawCheckbox
 *	Description: Draws a checkbox indicator (square outline with optional checkmark)
 *	Arguments: ctx - rendering context
 *                 x, y - position to draw checkbox (top-left)
 *                 size - size of the checkbox
 *                 checked - whether to draw the checkmark
 *	Returns: none.
 */

static void
DrawCheckbox(ISWRenderContext *ctx, int x, int y, int size, Boolean checked,
             double scale)
{
    cairo_t *cr = (cairo_t *)ISWRenderGetCairoContext(ctx);
    if (!cr) return;

    double r = 2.0 * scale;
    double bx = x;
    double by = y;
    double bs = size;

    cairo_save(cr);

    /* Rounded rect outline (always visible) */
    cairo_new_path(cr);
    cairo_arc(cr, bx + bs - r, by + r, r, -M_PI/2, 0);
    cairo_arc(cr, bx + bs - r, by + bs - r, r, 0, M_PI/2);
    cairo_arc(cr, bx + r, by + bs - r, r, M_PI/2, M_PI);
    cairo_arc(cr, bx + r, by + r, r, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_set_line_width(cr, 1.5 * scale);
    cairo_stroke(cr);

    /* Solid pip if checked */
    if (checked) {
        double inset = 2.5 * scale;
        double ix = bx + inset;
        double iy = by + inset;
        double is = bs - 2 * inset;
        double ir = r * 0.5;

        cairo_new_path(cr);
        cairo_arc(cr, ix + is - ir, iy + ir, ir, -M_PI/2, 0);
        cairo_arc(cr, ix + is - ir, iy + is - ir, ir, 0, M_PI/2);
        cairo_arc(cr, ix + ir, iy + is - ir, ir, M_PI/2, M_PI);
        cairo_arc(cr, ix + ir, iy + ir, ir, M_PI, 3*M_PI/2);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    cairo_new_path(cr);
    cairo_restore(cr);
}

/*	Function Name: DrawRadioButton
 *	Description: Draws a radio button indicator (circle outline with optional filled center)
 *	Arguments: ctx - rendering context
 *                 x, y - position to draw radio button (top-left of bounding box)
 *                 size - size of the radio button
 *                 selected - whether to draw the filled center
 *	Returns: none.
 */

static void
DrawRadioButton(ISWRenderContext *ctx, int x, int y, int size, Boolean selected,
                double scale)
{
    /* Use the Cairo context directly for true circles */
    cairo_t *cr = (cairo_t *)ISWRenderGetCairoContext(ctx);
    if (!cr) return;

    cairo_save(cr);

    double cx = x + size / 2.0;
    double cy = y + size / 2.0;
    double radius = size / 2.0;

    /* Draw circle outline (always visible) */
    cairo_set_line_width(cr, 1.5 * scale);
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy, radius, 0, 2.0 * M_PI);
    cairo_stroke(cr);

    /* Draw filled center circle if selected */
    if (selected) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, cx, cy, radius * 0.45, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

/************************************************************
 *
 * Redisplay Function
 *
 ************************************************************/

/*	Function Name: Redisplay
 *	Description: Redisplays the toggle widget, including state indicator.
 *	Arguments: w - the toggle widget.
 *                 event - the exposure event.
 *                 region - the region to redisplay.
 *	Returns: none.
 */

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    ToggleWidget tw = (ToggleWidget) w;
    ISWRenderContext *ctx;
    
    /* Call Label's Redisplay to draw just the text (not Command's button appearance) */
    /* labelWidgetClass is Command's superclass, so we skip the 3D button drawing */
    (*labelWidgetClass->core_class.expose)(w, event, region);
    
    /* Get rendering context (created by Label's Redisplay if needed) */
    ctx = tw->label.render_ctx;
    
    /* If no rendering context, we can't draw indicators */
    if (ctx == NULL) {
        return;
    }
    
    /*
     * Derive indicator size from the actual font metrics rather than
     * hardcoded pixel values.  This ensures correct sizing at any DPI
     * and adapts automatically if the font changes.
     */
    int cap_height = ISWScaledFontCapHeight((Widget)tw, tw->label.font);
    double scale = ISWScaleFactor(w);
    int padding = (int)(2 * scale + 0.5);
    int indicator_size = cap_height;

    /* Ensure indicator fits within widget bounds */
    if (indicator_size > (int)(tw->core.height - 2 * padding)) {
        indicator_size = tw->core.height - 2 * padding;
    }

    /* Minimum size to be visible */
    int min_size = (int)(8 * scale + 0.5);
    if (indicator_size < min_size) {
        indicator_size = min_size;
    }

    /* Position: left edge + padding, vertically centered */
    int x = padding;
    int y = (tw->core.height - indicator_size) / 2;
    
    /* Begin rendering */
    ISWRenderBegin(ctx);
    
    /* Set color for the indicator - use foreground color */
    ISWRenderSetColor(ctx, tw->label.foreground);
    
    /* Determine if this is a radio button or checkbox */
    /* Radio button: has a radio_group (part of a group) */
    /* Checkbox: standalone toggle */
    if (tw->toggle.radio_group != NULL) {
        /* Radio button - draw circle outline with optional filled center */
        DrawRadioButton(ctx, x, y, indicator_size, tw->command.set, scale);
    } else {
        /* Checkbox - draw square outline with optional checkmark */
        DrawCheckbox(ctx, x, y, indicator_size, tw->command.set, scale);
    }
    
    /* End rendering */
    ISWRenderEnd(ctx);
}

/************************************************************
 *
 * Public Routines
 *
 ************************************************************/

/*	Function Name: IswToggleChangeRadioGroup
 *	Description: Allows a toggle widget to change radio groups.
 *	Arguments: w - The toggle widget to change groups.
 *                 radio_group - any widget in the new group.
 *	Returns: none.
 */

void
IswToggleChangeRadioGroup(Widget w, Widget radio_group)
{
  ToggleWidget tw = (ToggleWidget) w;
  RadioGroup * group;

  RemoveFromRadioGroup(w);

/*
 * If the toggle that we are about to add is set then we will
 * unset all toggles in the new radio group.
 */

  if ( tw->command.set && radio_group != NULL )
    IswToggleUnsetCurrent(radio_group);

  if (radio_group != NULL) {
      if ((group = GetRadioGroup(radio_group)) == NULL)
	  CreateRadioGroup(w, radio_group);
      else AddToRadioGroup(group, w);
  }
}

/*	Function Name: IswToggleGetCurrent
 *	Description: Returns the RadioData associated with the toggle
 *                   widget that is currently active in a toggle group.
 *	Arguments: w - any toggle widget in the toggle group.
 *	Returns: The XtNradioData associated with the toggle widget.
 */

XtPointer
IswToggleGetCurrent(Widget w)
{
  RadioGroup * group;

  if ( (group = GetRadioGroup(w)) == NULL) return(NULL);
  for ( ; group->prev != NULL ; group = group->prev);

  while ( group != NULL ) {
    ToggleWidget local_tog = (ToggleWidget) group->widget;
    if ( local_tog->command.set )
      return( local_tog->toggle.radio_data );
    group = group->next;
  }
  return(NULL);
}

/*	Function Name: IswToggleSetCurrent
 *	Description: Sets the Toggle widget associated with the
 *                   radio_data specified.
 *	Arguments: radio_group - any toggle widget in the toggle group.
 *                 radio_data - radio data of the toggle widget to set.
 *	Returns: none.
 */

void
IswToggleSetCurrent(Widget radio_group, XtPointer radio_data)
{
  RadioGroup * group;
  ToggleWidget local_tog;

/* Special case of no radio group. */

  if ( (group = GetRadioGroup(radio_group)) == NULL) {
    local_tog = (ToggleWidget) radio_group;
    if (local_tog->toggle.radio_data == radio_data)
      if (!local_tog->command.set) {
	ToggleSet((Widget) local_tog, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
	Notify((Widget) local_tog, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
      }
    return;
  }

/*
 * find top of radio_roup
 */

  for ( ; group->prev != NULL ; group = group->prev);

/*
 * search for matching radio data.
 */

  while ( group != NULL ) {
    local_tog = (ToggleWidget) group->widget;
    if (local_tog->toggle.radio_data == radio_data) {
      if (!local_tog->command.set) { /* if not already set. */
	ToggleSet((Widget) local_tog, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
	Notify((Widget) local_tog, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
      }
      return;			/* found it, done */
    }
    group = group->next;
  }
}

/*	Function Name: IswToggleUnsetCurrent
 *	Description: Unsets all Toggles in the radio_group specified.
 *	Arguments: radio_group - any toggle widget in the toggle group.
 *	Returns: none.
 */

void
IswToggleUnsetCurrent(Widget radio_group)
{
  ToggleWidgetClass class;
  ToggleWidget local_tog = (ToggleWidget) radio_group;

  /* Special Case no radio group. */

  if (local_tog->command.set) {
    class = (ToggleWidgetClass) local_tog->core.widget_class;
    class->toggle_class.Unset(radio_group, NULL, NULL, 0);
    Notify(radio_group, (XEvent *)NULL, (String *)NULL, (Cardinal *)0);
  }
  if ( GetRadioGroup(radio_group) == NULL) return;
  TurnOffRadioSiblings(radio_group);
}

