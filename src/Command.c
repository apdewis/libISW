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
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
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
#include "ISWXcbDraw.h"     /* For XCB GC helpers */

#define DEFAULT_HIGHLIGHT_THICKNESS 2
#define DEFAULT_SHAPE_HIGHLIGHT 32767

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

static char defaultTranslations[] =
    "<Btn1Down>:	set()			\n\
     <Btn1Up>:		notify() unset()	";

#define offset(field) XtOffsetOf(CommandRec, field)
static XtResource resources[] = {
   {XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
      offset(command.callbacks), XtRCallback, (XtPointer)NULL},
   {XtNborderStrokeWidth, XtCBorderStrokeWidth, XtRDimension, sizeof(Dimension),
      offset(command.border_stroke_width), XtRImmediate,
      (XtPointer) DEFAULT_SHAPE_HIGHLIGHT},
   {XtNshapeStyle, XtCShapeStyle, XtRShapeStyle, sizeof(int),
      offset(command.shape_style), XtRImmediate, (XtPointer)IswShapeRectangle},
   {XtNcornerRoundPercent, XtCCornerRoundPercent, XtRDimension,
        sizeof(Dimension), offset(command.corner_round), XtRImmediate,
	(XtPointer) 25},
   {XtNcornerRadius, XtCCornerRadius, XtRDimension, sizeof(Dimension),
      offset(command.corner_radius), XtRImmediate, (XtPointer) 5},
   {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
      XtOffsetOf(RectObjRec,rectangle.border_width), XtRImmediate,
      (XtPointer) 0},
   {XtNinternalWidth, XtCWidth, XtRDimension, sizeof(Dimension),
      offset(label.internal_width), XtRImmediate, (XtPointer) 8},
   {XtNinternalHeight, XtCHeight, XtRDimension, sizeof(Dimension),
      offset(label.internal_height), XtRImmediate, (XtPointer) 4},
};
#undef offset

static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Set(Widget, XEvent *, String *, Cardinal *);
static void Reset(Widget, XEvent *, String *, Cardinal *);
static void Notify(Widget, XEvent *, String *, Cardinal *);
static void Unset(Widget, XEvent *, String *, Cardinal *);
static void Highlight(Widget, XEvent *, String *, Cardinal *);
static void Unhighlight(Widget, XEvent *, String *, Cardinal *);
static void Destroy(Widget);
static void PaintCommandWidget(Widget, XEvent *, Region, Boolean);
static void ClassInitialize(void);
static Boolean ShapeButton(CommandWidget, Boolean);
static void Realize(xcb_connection_t *, Widget, Mask *, uint32_t *);
static void Resize(Widget);

static XtActionsRec actionsList[] = {
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
    XtNumber(actionsList),		/* num_actions		  */
    resources,				/* resources		  */
    XtNumber(resources),		/* resource_count	  */
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
    XtInheritChangeSensitive		/* change_sensitive	*/
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

static GC
Get_GC(CommandWidget cbw, Pixel fg, Pixel bg)
{
  xcb_create_gc_value_list_t values;

  ISWInitGCValues(&values);
  values.foreground   = fg;
  values.background	= bg;
  
  
  if (cbw->label.font == NULL) {
      fprintf(stderr, "FATAL Command.c Get_GC: font is NULL for widget '%s'!\n",
              XtName((Widget)cbw));
      fprintf(stderr, "  This will cause a segfault. Aborting.\n");
      abort();
  }
  
  values.font		= cbw->label.font->fid;
  values.cap_style = XCB_CAP_STYLE_PROJECTING;

  if (cbw->command.border_stroke_width > 1 )
    values.line_width   = cbw->command.border_stroke_width;
  else
    values.line_width   = 0;

#ifdef ISW_INTERNATIONALIZATION
  if ( cbw->simple.international == True )
      return XtAllocateGC((Widget)cbw, 0,
		 (XCB_GC_FOREGROUND|XCB_GC_BACKGROUND|XCB_GC_LINE_WIDTH|XCB_GC_CAP_STYLE),
		 &values, XCB_GC_FONT, 0 );
  else
#endif
      return XtGetGC((Widget)cbw,
		 (XCB_GC_FOREGROUND|XCB_GC_BACKGROUND|XCB_GC_FONT|XCB_GC_LINE_WIDTH|XCB_GC_CAP_STYLE),
		 &values);
}


/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
  CommandWidget cbw = (CommandWidget) new;

  /* XCB Migration: Query shape extension using XCB */
  if (cbw->command.shape_style != IswShapeRectangle) {
      xcb_connection_t *conn = XtDisplay(new);
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

  /* HiDPI: scale dimension resources (after shape logic resolves values) */
  cbw->command.border_stroke_width = ISWScaleDim(new, cbw->command.border_stroke_width);
  cbw->command.corner_radius = ISWScaleDim(new, cbw->command.corner_radius);

  cbw->command.normal_GC = Get_GC(cbw, cbw->label.foreground,
				  cbw->core.background_pixel);
  cbw->command.inverse_GC = Get_GC(cbw, cbw->core.background_pixel,
				   cbw->label.foreground);
  XtReleaseGC(new, cbw->label.normal_GC);
  cbw->label.normal_GC = cbw->command.normal_GC;

  cbw->command.set = FALSE;
  cbw->command.highlighted = HighlightNone;
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
Set(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (cbw->command.set)
    return;

  cbw->command.set= TRUE;
  if (XtIsRealized(w))
    PaintCommandWidget(w, event, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Unset(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if (!cbw->command.set)
    return;

  cbw->command.set = FALSE;
  if (XtIsRealized(w))
    PaintCommandWidget(w, event, (Region) NULL, TRUE);
}

/* ARGSUSED */
static void
Reset(Widget w, XEvent *event, String *params, Cardinal *num_params)
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
Highlight(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  if ( *num_params == (Cardinal) 0)
    cbw->command.highlighted = HighlightWhenUnset;
  else {
    if ( *num_params != (Cardinal) 1)
      XtWarning("Too many parameters passed to highlight action table.");
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

  if (XtIsRealized(w))
    PaintCommandWidget(w, event, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Unhighlight(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  cbw->command.highlighted = HighlightNone;
  if (XtIsRealized(w))
    PaintCommandWidget(w, event, HighlightRegion(cbw), TRUE);
}

/* ARGSUSED */
static void
Notify(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  CommandWidget cbw = (CommandWidget)w;

  /* check to be sure state is still Set so that user can cancel
     the action (e.g. by moving outside the window, in the default
     bindings.
  */
  if (cbw->command.set)
    XtCallCallbackList(w, cbw->command.callbacks, (XtPointer) NULL);
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
  if (!ctx && w->core.width > 0 && w->core.height > 0 && XtIsRealized(w)) {
    ctx = cbw->label.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
  }

  very_thick = cbw->command.border_stroke_width >
               (Dimension)((Dimension) Min(cbw->core.width, cbw->core.height)/2);

  /* Save original foreground for later restoration */
  Pixel saved_foreground = cbw->label.foreground;

  /*
   * Single-pass repaint: clear background, draw border, then label text.
   * Everything is done in one Begin/End to avoid inter-frame flicker.
   */
  ISWRenderBegin(ctx);

  /* Clear to background */
  ISWRenderSetColor(ctx, w->core.background_pixel);
  ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

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

      /* Build rounded rect path */
      cairo_new_path(cr);
      cairo_arc(cr, bx + bw - r, by + r, r, -M_PI/2, 0);
      cairo_arc(cr, bx + bw - r, by + bh - r, r, 0, M_PI/2);
      cairo_arc(cr, bx + r, by + bh - r, r, M_PI/2, M_PI);
      cairo_arc(cr, bx + r, by + r, r, M_PI, 3*M_PI/2);
      cairo_close_path(cr);

      if (cbw->command.set) {
        cbw->label.normal_GC = cbw->command.inverse_GC;
        cbw->label.foreground = cbw->core.background_pixel;
        /* Fill with foreground color for inverted (pressed) look */
        ISWRenderSetColor(ctx, saved_foreground);
        cairo_fill_preserve(cr);
        region = NULL;  /* Force label to repaint text */
      } else {
        cbw->label.normal_GC = cbw->command.normal_GC;
      }

      /* Stroke the border */
      if (lw > 0 && !very_thick) {
        ISWRenderSetColor(ctx, saved_foreground);
        cairo_set_line_width(cr, lw);
        cairo_stroke(cr);
      } else {
        cairo_new_path(cr);
      }

      cairo_new_path(cr);
      cairo_restore(cr);
    }
  }

  ISWRenderEnd(ctx);

  /* Draw label text — always uses normal foreground */
  (*SuperClass->core_class.expose)(w, event, 0);

  cbw->label.foreground = saved_foreground;
}

static void
Destroy(Widget w)
{
  CommandWidget cbw = (CommandWidget) w;

  /* so Label can release it */
  if (cbw->label.normal_GC == cbw->command.normal_GC)
    XtReleaseGC( w, cbw->command.inverse_GC );
  else
    XtReleaseGC( w, cbw->command.normal_GC );
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
    if (oldcbw->label.normal_GC == oldcbw->command.normal_GC)
	/* Label has release one of these */
      XtReleaseGC(new, cbw->command.inverse_GC);
    else
      XtReleaseGC(new, cbw->command.normal_GC);

    cbw->command.normal_GC = Get_GC(cbw, cbw->label.foreground,
				    cbw->core.background_pixel);
    cbw->command.inverse_GC = Get_GC(cbw, cbw->core.background_pixel,
				     cbw->label.foreground);
    XtReleaseGC(new, cbw->label.normal_GC);
    cbw->label.normal_GC = (cbw->command.set
			    ? cbw->command.inverse_GC
			    : cbw->command.normal_GC);

    redisplay = True;
  }

  if (cbw->core.border_width != oldcbw->core.border_width)
      redisplay = True;

  if ( XtIsRealized(new)
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
                      XrmValue *fromVal, XrmValue *toVal, XtPointer *closure_ret)
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
        XtDisplayStringConversionWarning(conn, str, XtRShapeStyle);
        return False;
    }
    
    if (toVal->addr != NULL) {
        if (toVal->size < sizeof(int)) {
            toVal->size = sizeof(int);
            return False;
        }
        *(int *)toVal->addr = result;
    } else {
        toVal->addr = (XtPointer)&result;
    }
    toVal->size = sizeof(int);
    return True;
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    XtSetTypeConverter( XtRString, XtRShapeStyle, CvtStringToShapeStyle,
		        (XtConvertArgList)NULL, 0, XtCacheNone, (XtDestructor)NULL );
}


static Boolean
ShapeButton(CommandWidget cbw, Boolean checkRectangular)
{
    Dimension corner_size = 0;

    if (cbw->command.shape_style == IswShapeRoundedRectangle) {
	corner_size = (cbw->core.width < cbw->core.height) ? cbw->core.width
	                                                   : cbw->core.height;
	corner_size = (int) (corner_size * cbw->command.corner_round) / 100;
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
    if (XtIsRealized(w))
	ShapeButton( (CommandWidget) w, FALSE);

    (*commandWidgetClass->core_class.superclass->core_class.resize)(w);
}
