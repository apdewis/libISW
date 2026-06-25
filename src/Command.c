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
 * Command.c - Command button widget
 */

#include <ISW/ISWP.h>
#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWPlatform.h>
#include <ISW/CommandP.h>
#include <math.h>
#include <ISW/Region.h>     /* region API for highlight frame */

#define MULTI_LINE_LABEL 32767
#define DEFAULT_HIGHLIGHT_THICKNESS 1

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

static char defaultTranslations[] =
    "<Btn1Down>:	set()			\n\
     <Btn1Up>:		notify() unset()	\n\
     Button1<Leave>:	reset()			\n\
     Button1<Enter>:	set()			\n\
     <Key>space:	set() notify() unset()	\n\
     <Key>Return:	set() notify() unset()	";

#define offset(field) IswOffsetOf(CommandRec, field)
static IswResource resources[] = {
   {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
      offset(command.callbacks), IswRCallback, (IswPointer)NULL},
   {IswNcornerRadius, IswCCornerRadius, IswRDimension, sizeof(Dimension),
      offset(simple.corner_radius), IswRImmediate, (IswPointer) 5},
   {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
      IswOffsetOf(RectObjRec,rectangle.border_width), IswRImmediate,
      (IswPointer) DEFAULT_HIGHLIGHT_THICKNESS},
   {IswNinternalWidth, IswCWidth, IswRDimension, sizeof(Dimension),
      offset(label.internal_width), IswRImmediate, (IswPointer) 8},
   {IswNinternalHeight, IswCHeight, IswRDimension, sizeof(Dimension),
      offset(label.internal_height), IswRImmediate, (IswPointer) 4},
};
#undef offset

static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, IswEvent *, IswRegion);
static void Set(Widget, IswEvent *, String *, Cardinal *);
static void Reset(Widget, IswEvent *, String *, Cardinal *);
static void Notify(Widget, IswEvent *, String *, Cardinal *);
static void Unset(Widget, IswEvent *, String *, Cardinal *);
static void Highlight(Widget, IswEvent *, String *, Cardinal *);
static void Unhighlight(Widget, IswEvent *, String *, Cardinal *);
static void Destroy(Widget);
static void PaintCommandWidget(Widget, IswEvent *, Region, Boolean);
static void ClassInitialize(void);

static IswActionsRec actionsList[] = {
  {"set",		Set},
  {"notify",		Notify},
  {"highlight",		Highlight},
  {"reset",		Reset},
  {"unset",		Unset},
  {"unhighlight",	Unhighlight}
};

#define SuperClass ((LabelWidgetClass)&labelClassRec)

CommandClassRec commandClassRec = {
  {
    (WidgetClass) SuperClass,		/* superclass		  */
    "Command",				/* class_name		  */
    sizeof(CommandRec),			/* size			  */
    ClassInitialize,			/* class_initialize	  */
    NULL,				/* class_part_initialize  */
    FALSE,				/* class_inited		  */
    Initialize,				/* initialize		  */
    NULL,				/* initialize_hook	  */
    IswInheritRealize,			/* realize		  */
    actionsList,			/* actions		  */
    IswNumber(actionsList),		/* num_actions		  */
    resources,				/* resources		  */
    IswNumber(resources),		/* resource_count	  */
    ISW_NULLQUARK,				/* xrm_class		  */
    FALSE,				/* compress_motion	  */
    TRUE,				/* compress_exposure	  */
    TRUE,				/* compress_enterleave    */
    FALSE,				/* visible_interest	  */
    Destroy,				/* destroy		  */
    IswInheritResize,			/* resize		  */
    Redisplay,				/* expose		  */
    SetValues,				/* set_values		  */
    NULL,				/* set_values_hook	  */
    IswInheritSetValuesAlmost,		/* set_values_almost	  */
    NULL,				/* get_values_hook	  */
    NULL,				/* accept_focus		  */
    IswVersion,				/* version		  */
    NULL,				/* callback_private	  */
    defaultTranslations,		/* tm_table		  */
    IswInheritQueryGeometry,		/* query_geometry	  */
    IswInheritDisplayAccelerator,	/* display_accelerator	  */
    NULL				/* extension		  */
  },  /* CoreClass fields initialization */
  {
    IswInheritChangeSensitive		/* change_sensitive	*/
  },  /* SimpleClass fields initialization */
  {
    0,                                     /* field not used    */
  },  /* LabelClass fields initialization */
  {
    0,                                     /* field not used    */
  },  /* CommandClass fields initialization */
};

  /* for public consumption */
WidgetClass commandWidgetClass = (WidgetClass) &commandClassRec;

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  CommandWidget cbw = (CommandWidget) new;

  /* HiDPI: dimensions stay in logical pixels; scaled at X boundary */

  cbw->command.set = FALSE;
  cbw->command.highlighted = HighlightNone;

  /* Opt this widget into Tab traversal (focus manager). */
  ((SimpleWidget) new)->simple.traversal_on = True;
}

static ISWRegionPtr
HighlightRegion(CommandWidget cbw)
{
  static ISWRegionPtr outerRegion = NULL, innerRegion, emptyRegion;
  IswRectangle rect;

  if (cbw->core.border_width == 0 ||
      cbw->core.border_width >
      (Dimension) ((Dimension) Min(cbw->core.width, cbw->core.height)/2))
    return(NULL);

  if (outerRegion == NULL) {
    /* save time by allocating scratch regions only once. */
    outerRegion = ISWCreateRegion();
    innerRegion = ISWCreateRegion();
    emptyRegion = ISWCreateRegion();
  }

  rect.x = rect.y = 0;
  rect.width = cbw->core.width;
  rect.height = cbw->core.height;
  ISWUnionRectWithRegion( &rect, emptyRegion, outerRegion );
  rect.x = rect.y += cbw->core.border_width;
  rect.width -= cbw->core.border_width * 2;
  rect.height -= cbw->core.border_width * 2;
  ISWUnionRectWithRegion( &rect, emptyRegion, innerRegion );
  ISWSubtractRegion( outerRegion, innerRegion, outerRegion );
  return outerRegion;
}

/***************************
*
*  Action Procedures
*
***************************/

/* ARGSUSED */
static void
Set(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (cbw->command.set)
    return;

  cbw->command.set= TRUE;
  if (IswIsRealized(w))
    PaintCommandWidget(w, iswev, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Unset(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (!cbw->command.set)
    return;

  cbw->command.set = FALSE;
  if (IswIsRealized(w))
    PaintCommandWidget(w, iswev, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Reset(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (cbw->command.set) {
    cbw->command.highlighted = HighlightNone;
    Unset(w, iswev, params, num_params);
  } else
    Unhighlight(w, iswev, params, num_params);
}

/* ARGSUSED */
static void
Highlight(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if ( *num_params == (Cardinal) 0)
    cbw->command.highlighted = HighlightWhenUnset;
  else {
    if ( *num_params != (Cardinal) 1)
      IswWarning("Too many parameters passed to highlight action table.");
    switch (params[0][0]) {
    case 'A':
    case 'a':
      cbw->command.highlighted = HighlightAlways;
      break;
    default:
      cbw->command.highlighted = HighlightWhenUnset;
      break;
    }
  }

  if (IswIsRealized(w))
    PaintCommandWidget(w, iswev, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Unhighlight(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  cbw->command.highlighted = HighlightNone;
  if (IswIsRealized(w))
    PaintCommandWidget(w, iswev, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Notify(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  /* check to be sure state is still Set so that user can cancel
     the action (e.g. by moving outside the window, in the default
     bindings.
  */
  if (cbw->command.set)
    IswCallCallbackList(w, cbw->command.callbacks, (IswPointer) NULL);
}

/*
 * Repaint the widget window
 */

/************************
*
*  REDISPLAY (DRAW)
*
************************/

/* ARGSUSED */
static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
  PaintCommandWidget(w, event, 0 /* FIXME: region */, FALSE);
}

/*	Function Name: PaintCommandWidget
 *	Description: Paints the command widget.
 *	Arguments: w - the command widget.
 *                 region - region to paint (passed to the superclass).
 *                 change - did it change either set or highlight state?
 *	Returns: none
 */

static void
PaintCommandWidget(Widget w, IswEvent *event, Region region, Boolean change)
{
  CommandWidget cbw = (CommandWidget) w;
  ISWRenderContext *ctx = cbw->label.render_ctx;
  
  /* Create render context on first use (Command bypasses Label.Redisplay) */
  if (!ctx && w->core.width > 0 && w->core.height > 0 && IswIsRealized(w)) {
    ctx = cbw->label.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
  }

  /* Save original colors for later restoration */
  Pixel saved_foreground = cbw->label.foreground;
  Pixel saved_background = w->core.background_pixel;

  /* Pressed: invert colors so Label draws the whole rounded shape with the
   * foreground as the fill.  Label owns all the rounded-rect drawing. */
  if (cbw->command.set) {
    w->core.background_pixel = saved_foreground;
    cbw->label.foreground = saved_background;
  }

  /* Wrap Label's expose and focus ring in a single begin/end pass so the
     backend sees one frame (no stale-content composite between passes). */
  ISWRenderBegin(ctx);

  /* Let Label draw the rounded background + border + text */
  (*SuperClass->core_class.expose)(w, event, 0);

  cbw->label.foreground = saved_foreground;
  w->core.background_pixel = saved_background;

  {
    Boolean insensitive = !IswIsSensitive(w);
    if (insensitive) ISWRenderPushGroup(ctx);
    double lw = cbw->core.border_width;
    double off = lw / 2.0;
    double bx = off;
    double by = off;
    double bw = cbw->core.width - lw;
    double bh = cbw->core.height - lw;
    double r = cbw->simple.corner_radius;

    ISWRenderSave(ctx);

    /* Focus ring: dashed inset rectangle, drawn when this widget owns
     * keyboard focus (set by the Tab traversal focus manager). */
    if (((SimpleWidget) w)->simple.has_focus) {
      double pad = 3.0;
      double rx = bx + pad;
      double ry = by + pad;
      double rw = bw - 2 * pad;
      double rh = bh - 2 * pad;
      if (rw > 0 && rh > 0) {
        double dashes[2] = { 2.0, 2.0 };
        ISWRenderPathBegin(ctx);
        if (r > 0) {
          double rr = r > pad ? r - pad : 0;
          if (rr > 0) {
            ISWRenderPathArc(ctx, rx + rw - rr, ry + rr, rr, -M_PI/2, 0);
            ISWRenderPathArc(ctx, rx + rw - rr, ry + rh - rr, rr, 0, M_PI/2);
            ISWRenderPathArc(ctx, rx + rr, ry + rh - rr, rr, M_PI/2, M_PI);
            ISWRenderPathArc(ctx, rx + rr, ry + rr, rr, M_PI, 3*M_PI/2);
            ISWRenderPathClose(ctx);
          } else {
            ISWRenderPathRectangle(ctx, rx, ry, rw, rh);
          }
        } else {
          ISWRenderPathRectangle(ctx, rx, ry, rw, rh);
        }
        ISWRenderSetColor(ctx, saved_foreground);
        ISWRenderSetDash(ctx, dashes, 2, 0);
        ISWRenderSetLineWidth(ctx, 1.0);
        ISWRenderStroke(ctx);
        ISWRenderSetDash(ctx, NULL, 0, 0);
      }
    }

    ISWRenderRestore(ctx);
    if (insensitive) ISWRenderPopGroupWithAlpha(ctx, 0.4);
  }

  ISWRenderEnd(ctx);
}

static void
Destroy(Widget w)
{
  (void) w;
}

/*
 * Set specified arguments into widget
 */

/* ARGSUSED */
static Boolean
SetValues (Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  CommandWidget oldcbw = (CommandWidget) current;
  CommandWidget cbw = (CommandWidget) new;
  Boolean redisplay = False;

  if ( oldcbw->core.sensitive != cbw->core.sensitive && !cbw->core.sensitive) {
    /* about to become insensitive */
    cbw->command.set = FALSE;
    cbw->command.highlighted = HighlightNone;
    redisplay = TRUE;
  }

  if ( (oldcbw->label.foreground != cbw->label.foreground)           ||
       (oldcbw->core.background_pixel != cbw->core.background_pixel) ||
       (oldcbw->core.border_width != cbw->core.border_width)         ||
       (oldcbw->label.font != cbw->label.font) )
  {
    redisplay = True;
  }

  return (redisplay);
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

