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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>		/* for IswN and IswC defines */
#include <ISW/ISWInit.h>		/* for IswInitializeWidgetSet */
#include <ISW/PannerP.h>		/* us */
#include <ISW/ISWRender.h>		/* Cairo rendering */
#include <X11/Xos.h>
#include <ctype.h>			/* for isascii() etc. */
#include <stdlib.h>			/* for atof() */
#ifdef HAVE_CAIRO
#include <cairo.h>
#include <cairo-xcb.h>
#endif
#include <ISW/IswArgMacros.h>
#include <ISW/ISWPlatform.h>

#if defined(ISC) && __STDC__ && !defined(ISC30)
extern double atof(char *);
#endif


#if IswVersion >= 11006
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


static void ActionStart(Widget, IswEvent *, String *, Cardinal *);
static void ActionStop(Widget, IswEvent *, String *, Cardinal *);
static void ActionAbort(Widget, IswEvent *, String *, Cardinal *);
static void ActionMove(Widget, IswEvent *, String *, Cardinal *);
static void ActionPage(Widget, IswEvent *, String *, Cardinal *);
static void ActionNotify(Widget, IswEvent *, String *, Cardinal *);
static void ActionSet(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actions[] = {
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
static IswResource resources[] = {
#define poff(field) IswOffsetOf(PannerRec, panner.field)
    { IswNallowOff, IswCAllowOff, IswRBoolean, sizeof(Boolean),
	poff(allow_off), IswRImmediate, (IswPointer) FALSE },
    { IswNresize, IswCResize, IswRBoolean, sizeof(Boolean),
	poff(resize_to_pref), IswRImmediate, (IswPointer) TRUE },
    { IswNreportCallback, IswCReportCallback, IswRCallback, sizeof(IswPointer),
	poff(report_callbacks), IswRCallback, (IswPointer) NULL },
    { IswNdefaultScale, IswCDefaultScale, IswRDimension, sizeof(Dimension),
	poff(default_scale), IswRImmediate, (IswPointer) PANNER_DEFAULT_SCALE },
    { IswNrubberBand, IswCRubberBand, IswRBoolean, sizeof(Boolean),
	poff(rubber_band), IswRImmediate, (IswPointer) FALSE },
    { IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
	poff(foreground), IswRString, (IswPointer) IswDefaultForeground },
    { IswNinternalSpace, IswCInternalSpace, IswRDimension, sizeof(Dimension),
	poff(internal_border), IswRImmediate, (IswPointer) 4 },
    { IswNlineWidth, IswCLineWidth, IswRDimension, sizeof(Dimension),
	poff(line_width), IswRImmediate, (IswPointer) 0 },
    { IswNcanvasWidth, IswCCanvasWidth, IswRDimension, sizeof(Dimension),
	poff(canvas_width), IswRImmediate, (IswPointer) 0 },
    { IswNcanvasHeight, IswCCanvasHeight, IswRDimension, sizeof(Dimension),
	poff(canvas_height), IswRImmediate, (IswPointer) 0 },
    { IswNsliderX, IswCSliderX, IswRPosition, sizeof(Position),
	poff(slider_x), IswRImmediate, (IswPointer) 0 },
    { IswNsliderY, IswCSliderY, IswRPosition, sizeof(Position),
	poff(slider_y), IswRImmediate, (IswPointer) 0 },
    { IswNsliderWidth, IswCSliderWidth, IswRDimension, sizeof(Dimension),
	poff(slider_width), IswRImmediate, (IswPointer) 0 },
    { IswNsliderHeight, IswCSliderHeight, IswRDimension, sizeof(Dimension),
	poff(slider_height), IswRImmediate, (IswPointer) 0 },
#undef poff
};


/*
 * widget class methods used below
 */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SetValuesAlmost(Widget, Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static IswGeometryResult QueryGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

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
    /* num_actions		*/	IswNumber(actions),
    /* resources		*/	resources,
    /* num_resources		*/	IswNumber(resources),
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
    /* version			*/	IswVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultTranslations,
    /* query_geometry		*/	QueryGeometry,
    /* display_accelerator	*/	IswInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	IswInheritChangeSensitive
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
get_event_xy (PannerWidget pw, IswEvent *event, int *x, int *y)
{
    int pad = pw->panner.internal_border;

    switch (event->kind) {
      case IswButtonDown:
      case IswButtonUp:
      case IswKeyDown:
      case IswKeyUp:
      case IswEnter:
      case IswLeave:
      case IswMotion:
	    *x = IswEventX(event) - pad;
	    *y = IswEventY(event) - pad;
	    return TRUE;

      default:
	    break;
    }

    return FALSE;
}

static int
parse_page_string (const char *s, int pagesize, int canvassize, Boolean *relative)
{
    const char *cp;
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
    }
    pw->panner.tmp.showing = !pw->panner.tmp.showing;
}

#define DRAW_TMP(pw) draw_tmp_rubber_band(pw)

#define UNDRAW_TMP(pw) \
{ \
    if (pw->panner.tmp.showing) DRAW_TMP(pw); \
}

#define PIXMAP_OKAY(pm) ((pm) != None && (pm) != IswUnspecifiedPixmap)


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

    gnew->core.windowless = True;

    /* HiDPI: dimensions stay in logical pixels; scaled at X boundary */
    if (req->panner.canvas_width < 1) new->panner.canvas_width = 1;
    if (req->panner.canvas_height < 1) new->panner.canvas_height = 1;
    if (req->panner.default_scale < 1)
      new->panner.default_scale = PANNER_DEFAULT_SCALE;

    get_default_size (req, &defwidth, &defheight);
    if (req->core.width < 1) new->core.width = defwidth;
    if (req->core.height < 1) new->core.height = defheight;

    rescale (new);			/* does a position check */
    new->panner.tmp.doing = FALSE;
    new->panner.tmp.showing = FALSE;
    
    new->panner.render_ctx = NULL;	/* Initialized lazily in Redisplay */
}


static void
Realize (IswDisplay conn, Widget gw, IswValueMask *valuemaskp, uint32_t *values)
{
    (*pannerWidgetClass->core_class.superclass->core_class.realize)
      (conn, gw, valuemaskp, values);
}


static void
Destroy (Widget gw)
{
    PannerWidget pw = (PannerWidget) gw;

    if (pw->panner.render_ctx) {
        ISWRenderDestroy(pw->panner.render_ctx);
        pw->panner.render_ctx = NULL;
    }

}


static void
Resize (Widget gw)
{
    PannerWidget pw = (PannerWidget) gw;
    
    rescale (pw);
}


/* ARGSUSED */
static void
Redisplay (Widget gw, IswEvent *event, IswRegion region)
{
    PannerWidget pw = (PannerWidget) gw;
    int pad = pw->panner.internal_border;
    int kx = pw->panner.knob_x + pad, ky = pw->panner.knob_y + pad;

    pw->panner.tmp.showing = FALSE;

    if (!pw->panner.render_ctx) {
        pw->panner.render_ctx = ISWRenderCreate(gw, ISW_RENDER_BACKEND_AUTO);
        if (!pw->panner.render_ctx) {
            return;
        }
    }

    ISWRenderBegin(pw->panner.render_ctx);
    ISWRenderSetColor(pw->panner.render_ctx, pw->core.background_pixel);
    ISWRenderFillStrokeRoundedRectangle(pw->panner.render_ctx,
                   (int) pw->panner.last_x - 1 + pad,
                   (int) pw->panner.last_y - 1 + pad,
                   (int) (pw->panner.knob_width + 1),
                   (int) (pw->panner.knob_height + 1),
                   0, 1, 1.5);
    ISWRenderEnd(pw->panner.render_ctx);
    pw->panner.last_x = pw->panner.knob_x;
    pw->panner.last_y = pw->panner.knob_y;

    ISWRenderBegin(pw->panner.render_ctx);
    ISWRenderSetColor(pw->panner.render_ctx, pw->panner.foreground);
    ISWRenderFillStrokeRoundedRectangle(pw->panner.render_ctx,
                          kx, ky,
                          pw->panner.knob_width - 1,
                          pw->panner.knob_height - 1,
                          0, 0.2, 1);
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

    if (cur->panner.foreground != new->panner.foreground ||
	cur->panner.line_width != new->panner.line_width ||
	cur->core.background_pixel != new->core.background_pixel) {
	    redisplay = TRUE;
    }
    if (cur->panner.rubber_band != new->panner.rubber_band) {
	if (new->panner.tmp.doing) redisplay = TRUE;
    }

    if (cur->core.background_pixel != new->core.background_pixel &&
	IswIsRealized(gnew)) {
	    IswWindowAttributes attrs;
	    attrs.background_pixel = new->core.background_pixel;
	    _IswPlatformChangeAttributes(IswDisplayOf(new),
		_IswPlatformWidgetWindow(IswDisplayOf((Widget)(new)), (Widget)(new)),
		&attrs, ISW_ATTR_BACK_PIXEL);
	    _IswPlatformFlush(IswDisplayOf(new));
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
SetValuesAlmost (Widget gold, Widget gnew, IswWidgetGeometry *req, IswWidgetGeometry *reply)
{
    if (reply->request_mode == 0) {	/* got turned down, so cope */
	    Resize (gnew);
    }
    (*pannerWidgetClass->core_class.superclass->core_class.set_values_almost)
	(gold, gnew, req, reply);
}

static IswGeometryResult
QueryGeometry (Widget gw, IswWidgetGeometry *intended, IswWidgetGeometry *pref)
{
    PannerWidget pw = (PannerWidget) gw;

    pref->request_mode = (IswCWWidth | IswCWHeight);
    get_default_size (pw, &pref->width, &pref->height);

    if (((intended->request_mode & (IswCWWidth | IswCWHeight)) ==
	 (IswCWWidth | IswCWHeight)) &&
	intended->width == pref->width &&
	intended->height == pref->height)
      return IswGeometryYes;
    else if (pref->width == pw->core.width && pref->height == pw->core.height)
      return IswGeometryNo;
    else
      return IswGeometryAlmost;
}


/*****************************************************************************
 *                                                                           *
 * 			      panner action procs                            *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void
ActionStart (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

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
ActionStop (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

    if (get_event_xy (pw, iswev, &x, &y)) {
	    pw->panner.tmp.x = ((Position) x) - pw->panner.tmp.dx;
	    pw->panner.tmp.y = ((Position) y) - pw->panner.tmp.dy;
	    if (!pw->panner.allow_off) check_knob (pw, FALSE);
    }
    if (pw->panner.rubber_band) UNDRAW_TMP (pw);
    pw->panner.tmp.doing = FALSE;
}

/* ARGSUSED */
static void
ActionAbort (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;

    if (!pw->panner.tmp.doing) return;

    if (pw->panner.rubber_band) UNDRAW_TMP (pw);

    if (!pw->panner.rubber_band) {		/* restore old position */
	    pw->panner.tmp.x = pw->panner.tmp.startx;
	    pw->panner.tmp.y = pw->panner.tmp.starty;
	    ActionNotify (gw, iswev, params, num_params);
    }
    pw->panner.tmp.doing = FALSE;
}


/* ARGSUSED */
static void
ActionMove (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    int x, y;

    if (!pw->panner.tmp.doing) return;

    if (pw->panner.rubber_band) UNDRAW_TMP (pw);
    pw->panner.tmp.x = ((Position) x) - pw->panner.tmp.dx;
    pw->panner.tmp.y = ((Position) y) - pw->panner.tmp.dy;

    if (!pw->panner.rubber_band) {
	    ActionNotify (gw, iswev, params, num_params);  /* does a check */
    } else {
	    if (!pw->panner.allow_off) check_knob (pw, FALSE);
	    DRAW_TMP (pw);
    }
}


/* ARGSUSED */
static void
ActionPage (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    Cardinal zero = 0;
    Boolean isin = pw->panner.tmp.doing;
    int x, y;
    Boolean relx, rely;
    int pad = pw->panner.internal_border * 2;

    x = parse_page_string (params[0], (int) pw->panner.knob_width,
			   ((int) pw->core.width) - pad, &relx);
    y = parse_page_string (params[1], (int) pw->panner.knob_height,
			   ((int) pw->core.height) - pad, &rely);

    if (relx) x += pw->panner.knob_x;
    if (rely) y += pw->panner.knob_y;

    if (isin) {				/* if in, then use move */
        /* Synthesize a neutral button event for the action proc. */
        IswEvent nev;
        memset(&nev, 0, sizeof(nev));
        nev.kind = IswButtonDown;
        nev.button.x = x;
        nev.button.y = y;
        ActionMove (gw, &nev, (String *) NULL, &zero);
    } else {				/* else just do it */
	    pw->panner.tmp.doing = TRUE;
	    pw->panner.tmp.x = x;
	    pw->panner.tmp.y = y;
	    ActionNotify (gw, iswev, (String *) NULL, &zero);
	    pw->panner.tmp.doing = FALSE;
    }
}


/* ARGSUSED */
static void
ActionNotify (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;

    if (!pw->panner.tmp.doing) return;

    if (!pw->panner.allow_off) check_knob (pw, FALSE);
    pw->panner.knob_x = pw->panner.tmp.x;
    pw->panner.knob_y = pw->panner.tmp.y;

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
	    IswCallCallbackList (gw, pw->panner.report_callbacks, (IswPointer) &rep);
    }
}

/* ARGSUSED */
static void
ActionSet (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    PannerWidget pw = (PannerWidget) gw;
    Boolean rb;

    if (ISWCompareISOLatin1 (params[1], "on") == 0) {
	    rb = TRUE;
    } else if (ISWCompareISOLatin1 (params[1], "off") == 0) {
	    rb = FALSE;
    } else if (ISWCompareISOLatin1 (params[1], "toggle") == 0) {
	    rb = !pw->panner.rubber_band;
    }

    if (rb != pw->panner.rubber_band) {
	    IswArgBuilder ab = IswArgBuilderInit();
	    IswArgRubberBand(&ab, rb);
	    IswSetValues (gw, ab.args, ab.count);
    }
}
