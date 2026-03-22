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
 *
 * Author:  Jim Fulton, MIT X Consortium
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>		/* for XtN and XtC defines */
#include <ISW/ISWInit.h>		/* for IswInitializeWidgetSet */
#include <ISW/PannerP.h>		/* us */
#include <ISW/ISWRender.h>		/* Cairo rendering */
#include <X11/Xos.h>
#include <ctype.h>			/* for isascii() etc. */
#include <stdlib.h>			/* for atof() */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#ifdef HAVE_CAIRO
#include <cairo.h>
#include <cairo-xcb.h>
#endif
#include "ISWXcbDraw.h"

#if defined(ISC) && __STDC__ && !defined(ISC30)
extern double atof(char *);
#endif


#if XtVersion >= 11006
static char defaultTranslations[] =
  "<Btn1Down>:    start() \n\
   <Btn1Motion>:  move() \n\
   <Btn1Up>:      notify() stop() \n\
   <Btn2Down>:    abort() \n\
   :<Key>KP_Enter: set(rubberband,toggle) \n\
   <Key>space:    page(+1p,+1p) \n\
   <Key>Delete:   page(-1p,-1p) \n\
   :<Key>KP_Delete: page(-1p,-1p) \n\
   <Key>BackSpace: page(-1p,-1p) \n\
   <Key>Left:     page(-.5p,+0) \n\
   :<Key>KP_Left:  page(-.5p,+0) \n\
   <Key>Right:    page(+.5p,+0) \n\
   :<Key>KP_Right: page(+.5p,+0) \n\
   <Key>Up:       page(+0,-.5p) \n\
   :<Key>KP_Up:    page(+0,-.5p) \n\
   <Key>Down:     page(+0,+.5p) \n\
   :<Key>KP_Down:  page(+0,+.5p) \n\
   <Key>Home:     page(0,0) \n\
   :<Key>KP_Home:  page(0,0)";
#else
static char defaultTranslations[] =
  "<Btn1Down>:    start() \n\
   <Btn1Motion>:  move() \n\
   <Btn1Up>:      notify() stop() \n\
   <Btn2Down>:    abort() \n\
   <Key>KP_Enter: set(rubberband,toggle) \n\
   <Key>space:    page(+1p,+1p) \n\
   <Key>Delete:   page(-1p,-1p) \n\
   <Key>BackSpace: page(-1p,-1p) \n\
   <Key>Left:     page(-.5p,+0) \n\
   <Key>Right:    page(+.5p,+0) \n\
   <Key>Up:       page(+0,-.5p) \n\
   <Key>Down:     page(+0,+.5p) \n\
   <Key>Home:     page(0,0)";
#endif


static void ActionStart(Widget, XEvent *, String *, Cardinal *);
static void ActionStop(Widget, XEvent *, String *, Cardinal *);
static void ActionAbort(Widget, XEvent *, String *, Cardinal *);
static void ActionMove(Widget, XEvent *, String *, Cardinal *);
static void ActionPage(Widget, XEvent *, String *, Cardinal *);
static void ActionNotify(Widget, XEvent *, String *, Cardinal *);
static void ActionSet(Widget, XEvent *, String *, Cardinal *);

static XtActionsRec actions[] = {
    { "start", ActionStart },		/* start tmp graphics */
    { "stop", ActionStop },		/* stop tmp graphics */
    { "abort", ActionAbort },		/* punt */
    { "move", ActionMove },		/* move tmp graphics on Motion event */
    { "page", ActionPage },		/* page around usually from keyboard */
    { "notify", ActionNotify },		/* callback new position */
    { "set", ActionSet },		/* set various parameters */
};


/*
 * resources for the panner
 */
static XtResource resources[] = {
#define poff(field) XtOffsetOf(PannerRec, panner.field)
    { XtNallowOff, XtCAllowOff, XtRBoolean, sizeof(Boolean),
	poff(allow_off), XtRImmediate, (XtPointer) FALSE },
    { XtNresize, XtCResize, XtRBoolean, sizeof(Boolean),
	poff(resize_to_pref), XtRImmediate, (XtPointer) TRUE },
    { XtNreportCallback, XtCReportCallback, XtRCallback, sizeof(XtPointer),
	poff(report_callbacks), XtRCallback, (XtPointer) NULL },
    { XtNdefaultScale, XtCDefaultScale, XtRDimension, sizeof(Dimension),
	poff(default_scale), XtRImmediate, (XtPointer) PANNER_DEFAULT_SCALE },
    { XtNrubberBand, XtCRubberBand, XtRBoolean, sizeof(Boolean),
	poff(rubber_band), XtRImmediate, (XtPointer) FALSE },
    { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	poff(foreground), XtRString, (XtPointer) XtDefaultBackground },
    { XtNinternalSpace, XtCInternalSpace, XtRDimension, sizeof(Dimension),
	poff(internal_border), XtRImmediate, (XtPointer) 4 },
    { XtNlineWidth, XtCLineWidth, XtRDimension, sizeof(Dimension),
	poff(line_width), XtRImmediate, (XtPointer) 0 },
    { XtNcanvasWidth, XtCCanvasWidth, XtRDimension, sizeof(Dimension),
	poff(canvas_width), XtRImmediate, (XtPointer) 0 },
    { XtNcanvasHeight, XtCCanvasHeight, XtRDimension, sizeof(Dimension),
	poff(canvas_height), XtRImmediate, (XtPointer) 0 },
    { XtNsliderX, XtCSliderX, XtRPosition, sizeof(Position),
	poff(slider_x), XtRImmediate, (XtPointer) 0 },
    { XtNsliderY, XtCSliderY, XtRPosition, sizeof(Position),
	poff(slider_y), XtRImmediate, (XtPointer) 0 },
    { XtNsliderWidth, XtCSliderWidth, XtRDimension, sizeof(Dimension),
	poff(slider_width), XtRImmediate, (XtPointer) 0 },
    { XtNsliderHeight, XtCSliderHeight, XtRDimension, sizeof(Dimension),
	poff(slider_height), XtRImmediate, (XtPointer) 0 },
    { XtNshadowColor, XtCShadowColor, XtRPixel, sizeof(Pixel),
	poff(shadow_color), XtRString, (XtPointer) XtDefaultForeground },
    { XtNshadowThickness, XtCShadowThickness, XtRDimension, sizeof(Dimension),
	poff(shadow_thickness), XtRImmediate, (XtPointer) 2 },
    { XtNbackgroundStipple, XtCBackgroundStipple, XtRString, sizeof(String),
	poff(stipple_name), XtRImmediate, (XtPointer) NULL },
#undef poff
};


/*
 * widget class methods used below
 */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SetValuesAlmost(Widget, Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static XtGeometryResult QueryGeometry(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

PannerClassRec pannerClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &simpleClassRec,
    /* class_name		*/	"Panner",
    /* widget_size		*/	sizeof(PannerRec),
    /* class_initialize		*/	IswInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	Resize,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	SetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultTranslations,
    /* query_geometry		*/	QueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	XtInheritChangeSensitive
  },
  { /* panner fields */
    /* ignore                   */	0
  }
};

WidgetClass pannerWidgetClass = (WidgetClass) &pannerClassRec;


/*****************************************************************************
 *                                                                           *
 *			    panner utility routines                          *
 *                                                                           *
 *****************************************************************************/

static void
reset_shadow_gc (PannerWidget pw)	/* used when resources change */
{
    XtGCMask valuemask = GCForeground;
    xcb_create_gc_value_list_t values;
    unsigned long   pixels[3];

    if (pw->panner.shadow_gc) XtReleaseGC ((Widget) pw, pw->panner.shadow_gc);

    pixels[0] = pw->panner.foreground;
    pixels[1] = pw->core.background_pixel;
    pixels[2] = pw->panner.shadow_color;
    if (!pw->panner.stipple_name &&
 !ISWDistinguishablePixels (XtDisplay (pw), pw->core.colormap,
        pixels, 3) &&
 ISWDistinguishablePixels (XtDisplay (pw), pw->core.colormap,
        pixels, 2))
    {
 valuemask = GCTile | GCFillStyle;
 values.fill_style = XCB_FILL_STYLE_TILED;
 /* XCB: Use connection and window instead of Screen* */
 values.tile = IswCreateStippledPixmap(XtDisplay((Widget)pw),
                                         XtWindow((Widget)pw),
           pw->panner.foreground,
           pw->core.background_pixel,
           pw->core.depth);
    }
    else
    {
 if (!pw->panner.line_width &&
     !ISWDistinguishablePixels (XtDisplay (pw), pw->core.colormap,
           pixels, 2))
	    pw->panner.line_width = 1;
	valuemask = GCForeground;
	values.foreground = pw->panner.shadow_color;
    }
    if (pw->panner.line_width > 0) {
 values.line_width = pw->panner.line_width;
 valuemask |= GCLineWidth;
    }

    /* XCB: Use xcb_create_gc_value_list_t* cast */
    pw->panner.shadow_gc = XtGetGC ((Widget) pw, valuemask, (xcb_create_gc_value_list_t*)&values);
}

static void
reset_slider_gc (PannerWidget pw)	/* used when resources change */
{
    XtGCMask valuemask = GCForeground;
    xcb_create_gc_value_list_t values;

    if (pw->panner.slider_gc) XtReleaseGC ((Widget) pw, pw->panner.slider_gc);

    values.foreground = pw->panner.foreground;

    /* XCB: Use xcb_create_gc_value_list_t* cast */
    pw->panner.slider_gc = XtGetGC ((Widget) pw, valuemask, (xcb_create_gc_value_list_t*)&values);
}

static void
reset_xor_gc (PannerWidget pw)		/* used when resources change */
{
    if (pw->panner.xor_gc) XtReleaseGC ((Widget) pw, pw->panner.xor_gc);

    if (pw->panner.rubber_band) {
 XtGCMask valuemask = (GCForeground | GCFunction);
 xcb_create_gc_value_list_t values;
 Pixel tmp;

 tmp = ((pw->panner.foreground == pw->core.background_pixel) ?
        pw->panner.shadow_color : pw->panner.foreground);
 values.foreground = tmp ^ pw->core.background_pixel;
 values.function = XCB_GX_XOR;
 if (pw->panner.line_width > 0) {
     valuemask |= GCLineWidth;
     values.line_width = pw->panner.line_width;
 }
 /* XCB: Use xcb_create_gc_value_list_t* cast */
 pw->panner.xor_gc = XtGetGC ((Widget) pw, valuemask, (xcb_create_gc_value_list_t*)&values);
    } else {
 pw->panner.xor_gc = XCB_NONE;
    }
}


static void
check_knob (PannerWidget pw, Boolean knob)
{
    Position pad = pw->panner.internal_border * 2;
    Position maxx = (((Position) pw->core.width) - pad -
		     ((Position) pw->panner.knob_width));
    Position maxy = (((Position) pw->core.height) - pad -
		     ((Position) pw->panner.knob_height));
    Position *x = (knob ? &pw->panner.knob_x : &pw->panner.tmp.x);
    Position *y = (knob ? &pw->panner.knob_y : &pw->panner.tmp.y);

    /*
     * note that positions are already normalized (i.e. internal_border
     * has been subtracted out)
     */
    if (*x < 0) *x = 0;
    if (*x > maxx) *x = maxx;

    if (*y < 0) *y = 0;
    if (*y > maxy) *y = maxy;

    if (knob) {
	pw->panner.slider_x = (Position) (((double) pw->panner.knob_x) /
					  pw->panner.haspect + 0.5);
	pw->panner.slider_y = (Position) (((double) pw->panner.knob_y) /
					  pw->panner.vaspect + 0.5);
	pw->panner.last_x = pw->panner.last_y = PANNER_OUTOFRANGE;
    }
}


static void
move_shadow (PannerWidget pw)
{
    if (pw->panner.shadow_thickness > 0) {
	int lw = pw->panner.shadow_thickness + pw->panner.line_width * 2;
	int pad = pw->panner.internal_border;

	if ((int)pw->panner.knob_height > lw && (int)pw->panner.knob_width > lw) {
	    xcb_rectangle_t *r = pw->panner.shadow_rects;
	    r->x = (int16_t) (pw->panner.knob_x + pad + pw->panner.knob_width);
	    r->y = (int16_t) (pw->panner.knob_y + pad + lw);
	    r->width = pw->panner.shadow_thickness;
	    r->height = (uint16_t) (pw->panner.knob_height - lw);
	    r++;
	    r->x = (int16_t) (pw->panner.knob_x + pad + lw);
	    r->y = (int16_t) (pw->panner.knob_y + pad + pw->panner.knob_height);
	    r->width = (uint16_t) (pw->panner.knob_width - lw +
					 pw->panner.shadow_thickness);
	    r->height = pw->panner.shadow_thickness;
	    pw->panner.shadow_valid = TRUE;
	    return;
	}
    }
    pw->panner.shadow_valid = FALSE;
}

static void
scale_knob (PannerWidget pw, Boolean location, Boolean size)  /* set knob size and/or loc */
{
    if (location) {
	pw->panner.knob_x = (Position) PANNER_HSCALE (pw, pw->panner.slider_x);
	pw->panner.knob_y = (Position) PANNER_VSCALE (pw, pw->panner.slider_y);
    }
    if (size) {
	Dimension width, height;

	if (pw->panner.slider_width < 1) {
	    pw->panner.slider_width = pw->panner.canvas_width;
	}
	if (pw->panner.slider_height < 1) {
	    pw->panner.slider_height = pw->panner.canvas_height;
	}
	width = Min (pw->panner.slider_width, pw->panner.canvas_width);
	height = Min (pw->panner.slider_height, pw->panner.canvas_height);

	pw->panner.knob_width = (Dimension) PANNER_HSCALE (pw, width);
	pw->panner.knob_height = (Dimension) PANNER_VSCALE (pw, height);
    }
    if (!pw->panner.allow_off) check_knob (pw, TRUE);
    move_shadow (pw);
}

static void
rescale (PannerWidget pw)
{
    int hpad = pw->panner.internal_border * 2;
    int vpad = hpad;

    if (pw->panner.canvas_width < 1)
      pw->panner.canvas_width = pw->core.width;
    if (pw->panner.canvas_height < 1)
      pw->panner.canvas_height = pw->core.height;

    if ((int)pw->core.width <= hpad) hpad = 0;
    if ((int)pw->core.height <= vpad) vpad = 0;

    pw->panner.haspect = ((double) pw->core.width - hpad) /
			  (double) pw->panner.canvas_width;
    pw->panner.vaspect = ((double) pw->core.height - vpad) /
			  (double) pw->panner.canvas_height;
    scale_knob (pw, TRUE, TRUE);
}


static void
get_default_size (PannerWidget pw, Dimension *wp, Dimension *hp)
{
    Dimension pad = pw->panner.internal_border * 2;
    *wp = PANNER_DSCALE (pw, pw->panner.canvas_width) + pad;
    *hp = PANNER_DSCALE (pw, pw->panner.canvas_height) + pad;
}

static Boolean
get_event_xy (PannerWidget pw, XEvent *event, int *x, int *y)
{
    int pad = pw->panner.internal_border;

    /* XCB: Use response_type & ~0x80 to get event type, cast to specific types */
    switch (event->response_type & ~0x80) {
      case XCB_BUTTON_PRESS:
      case XCB_BUTTON_RELEASE: {
	xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
	*x = bev->event_x - pad;
	*y = bev->event_y - pad;
	return TRUE;
      }

      case XCB_KEY_PRESS:
      case XCB_KEY_RELEASE: {
	xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
	*x = kev->event_x - pad;
	*y = kev->event_y - pad;
	return TRUE;
      }

      case XCB_ENTER_NOTIFY:
      case XCB_LEAVE_NOTIFY: {
	xcb_enter_notify_event_t *cev = (xcb_enter_notify_event_t *)event;
	*x = cev->event_x - pad;
	*y = cev->event_y - pad;
	return TRUE;
      }

      case XCB_MOTION_NOTIFY: {
	xcb_motion_notify_event_t *mev = (xcb_motion_notify_event_t *)event;
	*x = mev->event_x - pad;
	*y = mev->event_y - pad;
	return TRUE;
      }
    }

    return FALSE;
}

static int
parse_page_string (char *s, int pagesize, int canvassize, Boolean *relative)
{
    char *cp;
    double val = 1.0;
    Boolean rel = FALSE;

    /*
     * syntax:    spaces [+-] number spaces [pc\0] spaces
     */

    for (; isascii(*s) && isspace(*s); s++) ;	/* skip white space */

    if (*s == '+' || *s == '-') {	/* deal with signs */
	rel = TRUE;
	if (*s == '-') val = -1.0;
	s++;
    }
    if (!*s) {				/* if null then return nothing */
	*relative = TRUE;
	return 0;
    }

					/* skip over numbers */
    for (cp = s; isascii(*s) && (isdigit(*s) || *s == '.'); s++) ;
    val *= atof (cp);

					/* skip blanks */
    for (; isascii(*s) && isspace(*s); s++) ;

    if (*s) {				/* if units */
	switch (s[0]) {
	  case 'p': case 'P':
	    val *= (double) pagesize;
	    break;

	  case 'c': case 'C':
	    val *= (double) canvassize;
	    break;
	}
    }
    *relative = rel;
    return ((int) val);
}


static void
draw_tmp_rubber_band(PannerWidget pw)
{
    int rx = (int)(pw->panner.tmp.x + pw->panner.internal_border);
    int ry = (int)(pw->panner.tmp.y + pw->panner.internal_border);
    unsigned int rw = (unsigned int)(pw->panner.knob_width - 1);
    unsigned int rh = (unsigned int)(pw->panner.knob_height - 1);

#ifdef HAVE_CAIRO
    ISWRenderContext *ctx = pw->panner.render_ctx;
    void *cr_ptr = ctx ? ISWRenderGetCairoContext(ctx) : NULL;
    if (cr_ptr) {
        cairo_t *cr = (cairo_t *)cr_ptr;
        double lw = (pw->panner.line_width > 0) ?
                     (double)pw->panner.line_width : 1.0;
        ISWRenderBegin(ctx);
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, lw);
        cairo_rectangle(cr, rx + 0.5, ry + 0.5, rw, rh);
        cairo_stroke(cr);
        cairo_restore(cr);
        ISWRenderEnd(ctx);
    } else
#endif
    {
        xcb_connection_t *conn = XtDisplay(pw);
        xcb_rectangle_t rect = { rx, ry, rw, rh };
        xcb_poly_rectangle(conn, XtWindow(pw), pw->panner.xor_gc, 1, &rect);
        xcb_flush(conn);
    }
    pw->panner.tmp.showing = !pw->panner.tmp.showing;
}

#define DRAW_TMP(pw) draw_tmp_rubber_band(pw)

#define UNDRAW_TMP(pw) \
{ \
    if (pw->panner.tmp.showing) DRAW_TMP(pw); \
}

#define BACKGROUND_STIPPLE(pw) \
  IswLocatePixmapFile (pw->core.screen, pw->panner.stipple_name, \
		       pw->panner.shadow_color, pw->core.background_pixel, \
		       pw->core.depth, NULL, 0, NULL, NULL, NULL, NULL)

#define PIXMAP_OKAY(pm) ((pm) != None && (pm) != XtUnspecifiedPixmap)


/*****************************************************************************
 *                                                                           *
 * 			     panner class methods                            *
 *                                                                           *
 *****************************************************************************/


/*ARGSUSED*/
static void
Initialize (Widget greq, Widget gnew, ArgList args, Cardinal *num_args)
{
    PannerWidget req = (PannerWidget) greq, new = (PannerWidget) gnew;
    Dimension defwidth, defheight;

    /* HiDPI: scale dimension resources */
    new->panner.internal_border = ISWScaleDim(gnew, new->panner.internal_border);
    new->panner.shadow_thickness = ISWScaleDim(gnew, new->panner.shadow_thickness);

    if (req->panner.canvas_width < 1) new->panner.canvas_width = 1;
    if (req->panner.canvas_height < 1) new->panner.canvas_height = 1;
    if (req->panner.default_scale < 1)
      new->panner.default_scale = PANNER_DEFAULT_SCALE;

    get_default_size (req, &defwidth, &defheight);
    if (req->core.width < 1) new->core.width = defwidth;
    if (req->core.height < 1) new->core.height = defheight;

    new->panner.shadow_gc = XCB_NONE;
    reset_shadow_gc (new);		/* shadowColor */
    new->panner.slider_gc = XCB_NONE;
    reset_slider_gc (new);		/* foreground */
    new->panner.xor_gc = XCB_NONE;
    reset_xor_gc (new);			/* foreground ^ background */

    rescale (new);			/* does a position check */
    new->panner.shadow_valid = FALSE;
    new->panner.tmp.doing = FALSE;
    new->panner.tmp.showing = FALSE;
    
    new->panner.render_ctx = NULL;	/* Initialized lazily in Redisplay */
}


static void
Realize (xcb_connection_t *conn, Widget gw, XtValueMask *valuemaskp, uint32_t *values)
{
    PannerWidget pw = (PannerWidget) gw;
    Pixmap pm = XtUnspecifiedPixmap;
    Boolean gotpm = FALSE;

    if (pw->core.background_pixmap == XtUnspecifiedPixmap) {
	if (pw->panner.stipple_name) pm = BACKGROUND_STIPPLE (pw);

	if (PIXMAP_OKAY(pm)) {
	    values[0] = pm;  /* background_pixmap */
	    *valuemaskp |= CWBackPixmap;
	    *valuemaskp &= ~CWBackPixel;
	    gotpm = TRUE;
	}
    }
    (*pannerWidgetClass->core_class.superclass->core_class.realize)
      (conn, gw, valuemaskp, values);

    if (gotpm) {
        xcb_free_pixmap(conn, pm);
        xcb_flush(conn);
    }
}


static void
Destroy (Widget gw)
{
    PannerWidget pw = (PannerWidget) gw;

    if (pw->panner.render_ctx) {
        ISWRenderDestroy(pw->panner.render_ctx);
        pw->panner.render_ctx = NULL;
    }

    XtReleaseGC (gw, pw->panner.shadow_gc);
    XtReleaseGC (gw, pw->panner.slider_gc);
    XtReleaseGC (gw, pw->panner.xor_gc);
}


static void
Resize (Widget gw)
{
    PannerWidget pw = (PannerWidget) gw;
    
    /* Destroy and recreate context on resize - Cairo contexts need to match window size */
    if (pw->panner.render_ctx) {
        ISWRenderDestroy(pw->panner.render_ctx);
        pw->panner.render_ctx = NULL;
        /* Context will be recreated lazily in next Redisplay */
    }
    
    rescale (pw);
}


/* ARGSUSED */
static void
Redisplay (Widget gw, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    PannerWidget pw = (PannerWidget) gw;
    xcb_connection_t *dpy = XtDisplay(gw);
    xcb_window_t w = XtWindow(gw);
    int pad = pw->panner.internal_border;
    Dimension lw = pw->panner.line_width;
    Dimension extra = pw->panner.shadow_thickness + lw * 2;
    int kx = pw->panner.knob_x + pad, ky = pw->panner.knob_y + pad;

    pw->panner.tmp.showing = FALSE;
    
    /* Clear old knob position */
    if (pw->panner.render_ctx) {
        ISWRenderBegin(pw->panner.render_ctx);
        ISWRenderSetColor(pw->panner.render_ctx, pw->core.background_pixel);
        ISWRenderFillRectangle(pw->panner.render_ctx,
                       (int) pw->panner.last_x - ((int) lw) + pad,
                       (int) pw->panner.last_y - ((int) lw) + pad,
                       (int) (pw->panner.knob_width + extra),
                       (int) (pw->panner.knob_height + extra));
        ISWRenderEnd(pw->panner.render_ctx);
    } else {
        xcb_connection_t *clear_conn = XtDisplay(pw);
        xcb_clear_area(clear_conn, 0, XtWindow(pw),
                       (int) pw->panner.last_x - ((int) lw) + pad,
                       (int) pw->panner.last_y - ((int) lw) + pad,
                       (unsigned int) (pw->panner.knob_width + extra),
                       (unsigned int) (pw->panner.knob_height + extra));
        xcb_flush(clear_conn);
    }
    pw->panner.last_x = pw->panner.knob_x;
    pw->panner.last_y = pw->panner.knob_y;

    /* Lazy initialization of Cairo render context */
    if (!pw->panner.render_ctx) {
        pw->panner.render_ctx = ISWRenderCreate(gw, ISW_RENDER_BACKEND_AUTO);
        if (!pw->panner.render_ctx) {
            /* Fallback to XCB if Cairo context creation fails */
            xcb_connection_t *conn = dpy;
            xcb_rectangle_t rect = {kx, ky, pw->panner.knob_width - 1, pw->panner.knob_height - 1};
            xcb_poly_fill_rectangle(conn, w, pw->panner.slider_gc, 1, &rect);
            if (lw) {
                xcb_poly_rectangle(conn, w, pw->panner.shadow_gc, 1, &rect);
            }
            if (pw->panner.shadow_valid) {
                xcb_poly_fill_rectangle(conn, w, pw->panner.shadow_gc,
                    2, pw->panner.shadow_rects);
            }
            xcb_flush(conn);
            if (pw->panner.tmp.doing && pw->panner.rubber_band) DRAW_TMP (pw);
            return;
        }
    }

    /* Use Cairo rendering for normal operations */
    ISWRenderBegin(pw->panner.render_ctx);
    
    /* Draw the slider/knob fill */
    ISWRenderSetColor(pw->panner.render_ctx, pw->panner.foreground);
    ISWRenderFillRectangle(pw->panner.render_ctx,
                          kx, ky, 
                          pw->panner.knob_width - 1, 
                          pw->panner.knob_height - 1);

    /* Draw the slider/knob outline if line_width > 0 */
    if (lw) {
        ISWRenderSetColor(pw->panner.render_ctx, pw->panner.shadow_color);
        ISWRenderSetLineWidth(pw->panner.render_ctx, (double)lw);
        ISWRenderStrokeRectangle(pw->panner.render_ctx,
                              kx, ky,
                              pw->panner.knob_width - 1,
                              pw->panner.knob_height - 1);
    }

    /* Draw shadow rectangles if valid */
    if (pw->panner.shadow_valid) {
        int i;
        ISWRenderSetColor(pw->panner.render_ctx, pw->panner.shadow_color);
        for (i = 0; i < 2; i++) {
            ISWRenderFillRectangle(pw->panner.render_ctx,
                                  pw->panner.shadow_rects[i].x,
                                  pw->panner.shadow_rects[i].y,
                                  pw->panner.shadow_rects[i].width,
                                  pw->panner.shadow_rects[i].height);
        }
    }

    ISWRenderEnd(pw->panner.render_ctx);
    
    /* XOR rubber-band drawing stays in XCB */
    if (pw->panner.tmp.doing && pw->panner.rubber_band) DRAW_TMP (pw);
}


/* ARGSUSED */
static Boolean
SetValues (Widget gcur, Widget greq, Widget gnew, ArgList args, Cardinal *num_args)
{
    PannerWidget cur = (PannerWidget) gcur;
    PannerWidget new = (PannerWidget) gnew;
    Boolean redisplay = FALSE;

    if (cur->panner.foreground != new->panner.foreground) {
	reset_slider_gc (new);
	if (cur->panner.foreground != cur->core.background_pixel)
	  reset_xor_gc (new);
	redisplay = TRUE;
    } else if (cur->panner.line_width != new->panner.line_width ||
	       cur->core.background_pixel != new->core.background_pixel) {
	reset_xor_gc (new);
	redisplay = TRUE;
    }
    if (cur->panner.shadow_color != new->panner.shadow_color) {
	reset_shadow_gc (new);
	if (cur->panner.foreground == cur->core.background_pixel)
	  reset_xor_gc (new);
	redisplay = TRUE;
    }
    if (cur->panner.shadow_thickness != new->panner.shadow_thickness) {
	move_shadow (new);
	redisplay = TRUE;
    }
    if (cur->panner.rubber_band != new->panner.rubber_band) {
	reset_xor_gc (new);
	if (new->panner.tmp.doing) redisplay = TRUE;
    }

    if ((cur->panner.stipple_name != new->panner.stipple_name ||
  cur->panner.shadow_color != new->panner.shadow_color ||
  cur->core.background_pixel != new->core.background_pixel) &&
 XtIsRealized(gnew)) {
 Pixmap pm = (new->panner.stipple_name ? BACKGROUND_STIPPLE (new)
       : XtUnspecifiedPixmap);

 if (PIXMAP_OKAY(pm)) {
     xcb_connection_t *conn = XtDisplay(new);
     uint32_t pixmap_val = pm;
     xcb_change_window_attributes(conn, XtWindow(new), XCB_CW_BACK_PIXMAP, &pixmap_val);
     xcb_free_pixmap(conn, pm);
     xcb_flush(conn);
 } else {
     xcb_connection_t *conn = XtDisplay(new);
     uint32_t pixel_val = new->core.background_pixel;
     xcb_change_window_attributes(conn, XtWindow(new), XCB_CW_BACK_PIXEL, &pixel_val);
     xcb_flush(conn);
 }
 redisplay = TRUE;
    }

    if (new->panner.resize_to_pref &&
	(cur->panner.canvas_width != new->panner.canvas_width ||
	 cur->panner.canvas_height != new->panner.canvas_height ||
	 cur->panner.resize_to_pref != new->panner.resize_to_pref)) {
	get_default_size (new, &new->core.width, &new->core.height);
	redisplay = TRUE;
    } else if (cur->panner.canvas_width != new->panner.canvas_width ||
	cur->panner.canvas_height != new->panner.canvas_height ||
	cur->panner.internal_border != new->panner.internal_border) {
	rescale (new);			/* does a scale_knob as well */
	redisplay = TRUE;
    } else {
	Boolean loc = (cur->panner.slider_x != new->panner.slider_x ||
		       cur->panner.slider_y != new->panner.slider_y);
	Boolean siz = (cur->panner.slider_width != new->panner.slider_width ||
		       cur->panner.slider_height != new->panner.slider_height);
	if (loc || siz ||
	    (cur->panner.allow_off != new->panner.allow_off &&
	     new->panner.allow_off)) {
	    scale_knob (new, loc, siz);
	    redisplay = TRUE;
	}
    }

    return redisplay;
}

static void
SetValuesAlmost (Widget gold, Widget gnew, XtWidgetGeometry *req, XtWidgetGeometry *reply)
{
    if (reply->request_mode == 0) {	/* got turned down, so cope */
	Resize (gnew);
    }
    (*pannerWidgetClass->core_class.superclass->core_class.set_values_almost)
	(gold, gnew, req, reply);
}

static XtGeometryResult
QueryGeometry (Widget gw, XtWidgetGeometry *intended, XtWidgetGeometry *pref)
{
    PannerWidget pw = (PannerWidget) gw;

    pref->request_mode = (CWWidth | CWHeight);
    get_default_size (pw, &pref->width, &pref->height);

    if (((intended->request_mode & (CWWidth | CWHeight)) ==
	 (CWWidth | CWHeight)) &&
	intended->width == pref->width &&
	intended->height == pref->height)
      return XtGeometryYes;
    else if (pref->width == pw->core.width && pref->height == pw->core.height)
      return XtGeometryNo;
    else
      return XtGeometryAlmost;
}


/*****************************************************************************
 *                                                                           *
 * 			      panner action procs                            *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void
ActionStart (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

    if (!get_event_xy (pw, event, &x, &y)) {
 xcb_connection_t *conn = XtDisplay(gw);
 xcb_bell(conn, 0);
 xcb_flush(conn);
 return;
    }

    pw->panner.tmp.doing = TRUE;
    pw->panner.tmp.startx = pw->panner.knob_x;
    pw->panner.tmp.starty = pw->panner.knob_y;
    pw->panner.tmp.dx = (((Position) x) - pw->panner.knob_x);
    pw->panner.tmp.dy = (((Position) y) - pw->panner.knob_y);
    pw->panner.tmp.x = pw->panner.knob_x;
    pw->panner.tmp.y = pw->panner.knob_y;
    if (pw->panner.rubber_band) DRAW_TMP (pw);
}

/* ARGSUSED */
static void
ActionStop (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

    if (get_event_xy (pw, event, &x, &y)) {
	pw->panner.tmp.x = ((Position) x) - pw->panner.tmp.dx;
	pw->panner.tmp.y = ((Position) y) - pw->panner.tmp.dy;
	if (!pw->panner.allow_off) check_knob (pw, FALSE);
    }
    if (pw->panner.rubber_band) UNDRAW_TMP (pw);
    pw->panner.tmp.doing = FALSE;
}

/* ARGSUSED */
static void
ActionAbort (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;

    if (!pw->panner.tmp.doing) return;

    if (pw->panner.rubber_band) UNDRAW_TMP (pw);

    if (!pw->panner.rubber_band) {		/* restore old position */
	pw->panner.tmp.x = pw->panner.tmp.startx;
	pw->panner.tmp.y = pw->panner.tmp.starty;
	ActionNotify (gw, event, params, num_params);
    }
    pw->panner.tmp.doing = FALSE;
}


/* ARGSUSED */
static void
ActionMove (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

    if (!pw->panner.tmp.doing) return;

    if (!get_event_xy (pw, event, &x, &y)) {
	xcb_connection_t *conn = XtDisplay(gw);
	xcb_bell(conn, 0);
	xcb_flush(conn);
	return;
    }

    if (pw->panner.rubber_band) UNDRAW_TMP (pw);
    pw->panner.tmp.x = ((Position) x) - pw->panner.tmp.dx;
    pw->panner.tmp.y = ((Position) y) - pw->panner.tmp.dy;

    if (!pw->panner.rubber_band) {
	ActionNotify (gw, event, params, num_params);  /* does a check */
    } else {
	if (!pw->panner.allow_off) check_knob (pw, FALSE);
	DRAW_TMP (pw);
    }
}


/* ARGSUSED */
static void
ActionPage (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    Cardinal zero = 0;
    Boolean isin = pw->panner.tmp.doing;
    int x, y;
    Boolean relx, rely;
    int pad = pw->panner.internal_border * 2;

    if (*num_params != 2) {
	xcb_connection_t *conn = XtDisplay(gw);
	xcb_bell(conn, 0);
	xcb_flush(conn);
	return;
    }

    x = parse_page_string (params[0], (int) pw->panner.knob_width,
			   ((int) pw->core.width) - pad, &relx);
    y = parse_page_string (params[1], (int) pw->panner.knob_height,
			   ((int) pw->core.height) - pad, &rely);

    if (relx) x += pw->panner.knob_x;
    if (rely) y += pw->panner.knob_y;

    if (isin) {				/* if in, then use move */
 xcb_button_press_event_t ev;
 memset(&ev, 0, sizeof(ev));
 ev.response_type = XCB_BUTTON_PRESS;
 ev.event_x = x;
 ev.event_y = y;
 ActionMove (gw, (xcb_generic_event_t*)&ev, (String *) NULL, &zero);
    } else {				/* else just do it */
	pw->panner.tmp.doing = TRUE;
	pw->panner.tmp.x = x;
	pw->panner.tmp.y = y;
	ActionNotify (gw, event, (String *) NULL, &zero);
	pw->panner.tmp.doing = FALSE;
    }
}


/* ARGSUSED */
static void
ActionNotify (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;

    if (!pw->panner.tmp.doing) return;

    if (!pw->panner.allow_off) check_knob (pw, FALSE);
    pw->panner.knob_x = pw->panner.tmp.x;
    pw->panner.knob_y = pw->panner.tmp.y;
    move_shadow (pw);

    pw->panner.slider_x = (Position) (((double) pw->panner.knob_x) /
				      pw->panner.haspect + 0.5);
    pw->panner.slider_y = (Position) (((double) pw->panner.knob_y) /
				      pw->panner.vaspect + 0.5);
    if (!pw->panner.allow_off) {
	Position tmp;

	if (pw->panner.slider_x >
	    (tmp = (((Position) pw->panner.canvas_width) -
		    ((Position) pw->panner.slider_width))))
	  pw->panner.slider_x = tmp;
	if (pw->panner.slider_x < 0) pw->panner.slider_x = 0;
	if (pw->panner.slider_y >
	    (tmp = (((Position) pw->panner.canvas_height) -
		    ((Position) pw->panner.slider_height))))
	  pw->panner.slider_y = tmp;
	if (pw->panner.slider_y < 0) pw->panner.slider_y = 0;
    }

    if (pw->panner.last_x != pw->panner.knob_x ||
	pw->panner.last_y != pw->panner.knob_y) {
	IswPannerReport rep;

	Redisplay (gw, NULL, 0);
	rep.changed = (IswPRSliderX | IswPRSliderY);
	rep.slider_x = pw->panner.slider_x;
	rep.slider_y = pw->panner.slider_y;
	rep.slider_width = pw->panner.slider_width;
	rep.slider_height = pw->panner.slider_height;
	rep.canvas_width = pw->panner.canvas_width;
	rep.canvas_height = pw->panner.canvas_height;
	XtCallCallbackList (gw, pw->panner.report_callbacks, (XtPointer) &rep);
    }
}

/* ARGSUSED */
static void
ActionSet (Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    Boolean rb;

    if (*num_params < 2 ||
	ISWCompareISOLatin1 (params[0], "rubberband") != 0) {
	xcb_connection_t *conn = XtDisplay(gw);
	xcb_bell(conn, 0);
	xcb_flush(conn);
	return;
    }

    if (ISWCompareISOLatin1 (params[1], "on") == 0) {
	rb = TRUE;
    } else if (ISWCompareISOLatin1 (params[1], "off") == 0) {
	rb = FALSE;
    } else if (ISWCompareISOLatin1 (params[1], "toggle") == 0) {
	rb = !pw->panner.rubber_band;
    } else {
	xcb_connection_t *conn = XtDisplay(gw);
	xcb_bell(conn, 0);
	xcb_flush(conn);
	return;
    }

    if (rb != pw->panner.rubber_band) {
	Arg args[1];
	XtSetArg (args[0], XtNrubberBand, rb);
	XtSetValues (gw, args, (Cardinal) 1);
    }
}
