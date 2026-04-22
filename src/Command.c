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
/* XCB Migration: Removed Xmu includes - not compatible with XCB */
/* #include <X11/Xmu/Misc.h> */
/* #include <X11/Xmu/Converters.h> */
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/CommandP.h>
/* XCB Migration: Use XCB shape extension instead of Xlib */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/shape.h>
#include <cairo/cairo.h>
#include <math.h>
#include "ISWXcbDraw.h"     /* For XCB xcb_gcontext_t helpers */

#define MULTI_LINE_LABEL 32767
#define DEFAULT_HIGHLIGHT_THICKNESS 1
#define DEFAULT_SHAPE_HIGHLIGHT 32767

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

static char defaultTranslations[] =
    "<Btn1Down>:	set()			\n\
     <Btn1Up>:		notify() unset()	\n\
     <Key>space:	set() notify() unset()	\n\
     <Key>Return:	set() notify() unset()	";

#define offset(field) IswOffsetOf(CommandRec, field)
static IswResource resources[] = {
   {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
      offset(command.callbacks), IswRCallback, (IswPointer)NULL},
   {IswNborderStrokeWidth, IswCBorderStrokeWidth, IswRDimension, sizeof(Dimension),
      offset(command.border_stroke_width), IswRImmediate,
      (IswPointer) DEFAULT_SHAPE_HIGHLIGHT},
   {IswNshapeStyle, IswCShapeStyle, IswRShapeStyle, sizeof(int),
      offset(command.shape_style), IswRImmediate, (IswPointer)IswShapeRectangle},
   {IswNcornerRadius, IswCCornerRadius, IswRDimension, sizeof(Dimension),
      offset(command.corner_radius), IswRImmediate, (IswPointer) 5},
   {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
      IswOffsetOf(RectObjRec,rectangle.border_width), IswRImmediate,
      (IswPointer) 0},
   {IswNinternalWidth, IswCWidth, IswRDimension, sizeof(Dimension),
      offset(label.internal_width), IswRImmediate, (IswPointer) 8},
   {IswNinternalHeight, IswCHeight, IswRDimension, sizeof(Dimension),
      offset(label.internal_height), IswRImmediate, (IswPointer) 4},
};
#undef offset

static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Set(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Reset(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Notify(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Unset(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Highlight(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Unhighlight(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Destroy(Widget);
static void PaintCommandWidget(Widget, xcb_generic_event_t *, Region, Boolean);
static void ClassInitialize(void);
static Boolean ShapeButton(CommandWidget, Boolean);
static void Realize(xcb_connection_t *, Widget, Mask *, uint32_t *);
static void Resize(Widget);

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
    Realize,				/* realize		  */
    actionsList,			/* actions		  */
    IswNumber(actionsList),		/* num_actions		  */
    resources,				/* resources		  */
    IswNumber(resources),		/* resource_count	  */
    NULLQUARK,				/* xrm_class		  */
    FALSE,				/* compress_motion	  */
    TRUE,				/* compress_exposure	  */
    TRUE,				/* compress_enterleave    */
    FALSE,				/* visible_interest	  */
    Destroy,				/* destroy		  */
    Resize,				/* resize		  */
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

  /* XCB Migration: Query shape extension using XCB */
  if (cbw->command.shape_style != IswShapeRectangle) {
      xcb_connection_t *conn = IswDisplay(new);
      xcb_shape_query_version_cookie_t cookie = xcb_shape_query_version(conn);
      xcb_shape_query_version_reply_t *reply = xcb_shape_query_version_reply(conn, cookie, NULL);
      if (!reply) {
          cbw->command.shape_style = IswShapeRectangle;
      } else {
          free(reply);
      }
  }
  if (cbw->command.border_stroke_width == DEFAULT_SHAPE_HIGHLIGHT) {
      if (cbw->command.shape_style != IswShapeRectangle)
	  cbw->command.border_stroke_width = 0;
      else
	  cbw->command.border_stroke_width = DEFAULT_HIGHLIGHT_THICKNESS;
  }

  if (cbw->command.shape_style != IswShapeRectangle) {
    cbw->core.border_width = 1;
  }

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
  xcb_rectangle_t rect;

  if (cbw->command.border_stroke_width == 0 ||
      cbw->command.border_stroke_width >
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
  rect.x = rect.y += cbw->command.border_stroke_width;
  rect.width -= cbw->command.border_stroke_width * 2;
  rect.height -= cbw->command.border_stroke_width * 2;
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
Set(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (cbw->command.set)
    return;

  cbw->command.set= TRUE;
  if (IswIsRealized(w))
    PaintCommandWidget(w, event, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Unset(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (!cbw->command.set)
    return;

  cbw->command.set = FALSE;
  if (IswIsRealized(w))
    PaintCommandWidget(w, event, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Reset(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (cbw->command.set) {
    cbw->command.highlighted = HighlightNone;
    Unset(w, event, params, num_params);
  } else
    Unhighlight(w, event, params, num_params);
}

/* ARGSUSED */
static void
Highlight(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
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
    PaintCommandWidget(w, event, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Unhighlight(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  cbw->command.highlighted = HighlightNone;
  if (IswIsRealized(w))
    PaintCommandWidget(w, event, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Notify(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
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
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
  PaintCommandWidget(w, event, 0 /* FIXME: XCB region */, FALSE);
}

/*	Function Name: PaintCommandWidget
 *	Description: Paints the command widget.
 *	Arguments: w - the command widget.
 *                 region - region to paint (passed to the superclass).
 *                 change - did it change either set or highlight state?
 *	Returns: none
 */

static void
PaintCommandWidget(Widget w, xcb_generic_event_t *event, Region region, Boolean change)
{
  CommandWidget cbw = (CommandWidget) w;
  Boolean very_thick;
  ISWRenderContext *ctx = cbw->label.render_ctx;

  /* Create render context on first use (Command bypasses Label.Redisplay) */
  if (!ctx && w->core.width > 0 && w->core.height > 0 && IswIsRealized(w)) {
    ctx = cbw->label.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
  }

  very_thick = cbw->command.border_stroke_width >
               (Dimension)((Dimension) Min(cbw->core.width, cbw->core.height)/2);

  /* Save original colors for later restoration */
  Pixel saved_foreground = cbw->label.foreground;
  Pixel saved_background = w->core.background_pixel;

  /* Let Label draw normal background + text */
  (*SuperClass->core_class.expose)(w, event, 0);

  /* Now draw pressed-state fill (if set) and border on top */
  ISWRenderBegin(ctx);

  {
    cairo_t *cr = (cairo_t *)ISWRenderGetCairoContext(ctx);
    if (cr) {
      double lw = cbw->command.border_stroke_width;
      double off = lw / 2.0;
      double bx = off;
      double by = off;
      double bw = cbw->core.width - lw;
      double bh = cbw->core.height - lw;
      double r = cbw->command.corner_radius;

      cairo_save(cr);

      if (cbw->command.set) {
        /* Pressed: fill with foreground, redraw content in background.
         * Clip to the border shape so the fill respects corner_radius. */
        cairo_new_path(cr);
        if (r > 0) {
          cairo_arc(cr, bx + bw - r, by + r, r, -M_PI/2, 0);
          cairo_arc(cr, bx + bw - r, by + bh - r, r, 0, M_PI/2);
          cairo_arc(cr, bx + r, by + bh - r, r, M_PI/2, M_PI);
          cairo_arc(cr, bx + r, by + r, r, M_PI, 3*M_PI/2);
          cairo_close_path(cr);
        } else {
          cairo_rectangle(cr, bx, by, bw, bh);
        }
        cairo_save(cr);
        cairo_clip(cr);

        /* Fill background */
        ISWRenderSetColor(ctx, saved_foreground);
        cairo_paint(cr);

        /* Redraw content with inverted colors */
        cbw->label.foreground = saved_background;
        w->core.background_pixel = saved_foreground;
        (*SuperClass->core_class.expose)(w, event, 0);
        cbw->label.foreground = saved_foreground;
        w->core.background_pixel = saved_background;

        cairo_restore(cr);
      }

      /* Build border path */
      cairo_new_path(cr);
      if (r > 0) {
        cairo_arc(cr, bx + bw - r, by + r, r, -M_PI/2, 0);
        cairo_arc(cr, bx + bw - r, by + bh - r, r, 0, M_PI/2);
        cairo_arc(cr, bx + r, by + bh - r, r, M_PI/2, M_PI);
        cairo_arc(cr, bx + r, by + r, r, M_PI, 3*M_PI/2);
        cairo_close_path(cr);
      } else {
        cairo_rectangle(cr, bx, by, bw, bh);
      }

      /* Stroke the border */
      if (lw > 0 && !very_thick) {
        ISWRenderSetColor(ctx, saved_foreground);
        cairo_set_line_width(cr, lw);
        cairo_stroke(cr);
      } else {
        cairo_new_path(cr);
      }

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
          cairo_new_path(cr);
          if (r > 0) {
            double rr = r > pad ? r - pad : 0;
            if (rr > 0) {
              cairo_arc(cr, rx + rw - rr, ry + rr, rr, -M_PI/2, 0);
              cairo_arc(cr, rx + rw - rr, ry + rh - rr, rr, 0, M_PI/2);
              cairo_arc(cr, rx + rr, ry + rh - rr, rr, M_PI/2, M_PI);
              cairo_arc(cr, rx + rr, ry + rr, rr, M_PI, 3*M_PI/2);
              cairo_close_path(cr);
            } else {
              cairo_rectangle(cr, rx, ry, rw, rh);
            }
          } else {
            cairo_rectangle(cr, rx, ry, rw, rh);
          }
          ISWRenderSetColor(ctx, saved_foreground);
          cairo_set_dash(cr, dashes, 2, 0);
          cairo_set_line_width(cr, 1.0);
          cairo_stroke(cr);
          cairo_set_dash(cr, NULL, 0, 0);
        }
      }

      cairo_restore(cr);
    }
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
       (oldcbw->command.border_stroke_width !=
                                   cbw->command.border_stroke_width) ||
       (oldcbw->label.font != cbw->label.font) )
  {
    redisplay = True;
  }

  if (cbw->core.border_width != oldcbw->core.border_width)
      redisplay = True;

  if ( IswIsRealized(new)
       && oldcbw->command.shape_style != cbw->command.shape_style
       && !ShapeButton(cbw, TRUE))
  {
      cbw->command.shape_style = oldcbw->command.shape_style;
  }

  if (cbw->command.shape_style != IswShapeRectangle) {
      ShapeButton(cbw, FALSE);
      redisplay = True;
  }

  return (redisplay);
}

/* XCB Migration: Simple ShapeStyle converter to replace XmuCvtStringToShapeStyle */
static Boolean
CvtStringToShapeStyle(xcb_connection_t *conn, XrmValue *args, Cardinal *num_args,
                      XrmValue *fromVal, XrmValue *toVal, IswPointer *closure_ret)
{
    String str = (String)fromVal->addr;
    static int result;
    
    if (strcmp(str, "Rectangle") == 0 || strcmp(str, "rectangle") == 0) {
        result = IswShapeRectangle;
    } else if (strcmp(str, "Oval") == 0 || strcmp(str, "oval") == 0) {
        result = IswShapeOval;
    } else if (strcmp(str, "Ellipse") == 0 || strcmp(str, "ellipse") == 0) {
        result = IswShapeEllipse;
    } else if (strcmp(str, "RoundedRectangle") == 0 || strcmp(str, "roundedRectangle") == 0) {
        result = IswShapeRoundedRectangle;
    } else {
        IswDisplayStringConversionWarning(conn, str, IswRShapeStyle);
        return False;
    }
    
    if (toVal->addr != NULL) {
        if (toVal->size < sizeof(int)) {
            toVal->size = sizeof(int);
            return False;
        }
        *(int *)toVal->addr = result;
    } else {
        toVal->addr = (IswPointer)&result;
    }
    toVal->size = sizeof(int);
    return True;
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter( IswRString, IswRShapeStyle, CvtStringToShapeStyle,
		        (IswConvertArgList)NULL, 0, IswCacheNone, (IswDestructor)NULL );
}


static Boolean
ShapeButton(CommandWidget cbw, Boolean checkRectangular)
{
    Dimension corner_size = 0;

    if (cbw->command.shape_style == IswShapeRoundedRectangle) {
	corner_size = cbw->command.corner_radius;
    }

    if (checkRectangular || cbw->command.shape_style != IswShapeRectangle) {
 if (!ISWReshapeWidget((Widget) cbw, cbw->command.shape_style,
         corner_size, corner_size)) {
     cbw->command.shape_style = IswShapeRectangle;
     return(False);
 }
    }
    return(TRUE);
}

static void
Realize(xcb_connection_t *conn, Widget w, Mask *valueMask, uint32_t *attributes)
{
    /* XCB Migration: superclass realize now takes conn as first parameter */
    (*commandWidgetClass->core_class.superclass->core_class.realize)
	(conn, w, valueMask, attributes);

    ShapeButton( (CommandWidget) w, FALSE);
}

static void
Resize(Widget w)
{
    if (IswIsRealized(w))
	ShapeButton( (CommandWidget) w, FALSE);

    (*commandWidgetClass->core_class.superclass->core_class.resize)(w);
}
