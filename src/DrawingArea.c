/*
 * DrawingArea.c - DrawingArea widget implementation
 *
 * A canvas widget that delegates all rendering to application callbacks.
 * Subclasses Simple; owns an ISWRenderContext that is passed to the app
 * in expose callbacks (bracketed by ISWRenderBegin/End).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/DrawingAreaP.h>

#define offset(field) XtOffsetOf(DrawingAreaRec, field)

static XtResource resources[] = {
    {XtNexposeCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(drawing_area.expose_callbacks), XtRCallback, NULL},
    {XtNresizeCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(drawing_area.resize_callbacks), XtRCallback, NULL},
    {XtNinputCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(drawing_area.input_callbacks), XtRCallback, NULL},
};
#undef offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void DrawingAreaInput(Widget, xcb_generic_event_t *, String *, Cardinal *);

static XtActionsRec actionsList[] = {
    {"DrawingAreaInput", DrawingAreaInput},
};

static char defaultTranslations[] =
    "<BtnDown>:   DrawingAreaInput() \n\
     <BtnUp>:     DrawingAreaInput() \n\
     <BtnMotion>: DrawingAreaInput() \n\
     <KeyDown>:   DrawingAreaInput() \n\
     <KeyUp>:     DrawingAreaInput() ";

#define SuperClass ((SimpleWidgetClass)&simpleClassRec)

DrawingAreaClassRec drawingAreaClassRec = {
  {
    (WidgetClass) SuperClass,		/* superclass		  */
    "DrawingArea",			/* class_name		  */
    sizeof(DrawingAreaRec),		/* size			  */
    IswInitializeWidgetSet,		/* class_initialize	  */
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
    TRUE,				/* compress_motion	  */
    FALSE,				/* compress_exposure	  */
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
  },
  {
    XtInheritChangeSensitive		/* change_sensitive	  */
  },
  {
    0					/* makes_compiler_happy   */
  }
};

WidgetClass drawingAreaWidgetClass = (WidgetClass) &drawingAreaClassRec;

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    DrawingAreaWidget daw = (DrawingAreaWidget) new;

    daw->drawing_area.render_ctx = NULL;

    if (daw->core.width == 0)  daw->core.width = 1;
    if (daw->core.height == 0) daw->core.height = 1;
}

static void
Destroy(Widget w)
{
    DrawingAreaWidget daw = (DrawingAreaWidget) w;

    if (daw->drawing_area.render_ctx) {
	ISWRenderDestroy(daw->drawing_area.render_ctx);
	daw->drawing_area.render_ctx = NULL;
    }
}

static void
Resize(Widget w)
{
    DrawingAreaWidget daw = (DrawingAreaWidget) w;
    ISWDrawingCallbackData call_data;

    if (daw->drawing_area.render_ctx) {
	ISWRenderDestroy(daw->drawing_area.render_ctx);
	daw->drawing_area.render_ctx = NULL;
    }

    call_data.render_ctx = NULL;
    call_data.event = NULL;
    call_data.window = XtIsRealized(w) ? XtWindow(w) : 0;

    XtCallCallbackList(w, daw->drawing_area.resize_callbacks,
		       (XtPointer)&call_data);
}

/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    DrawingAreaWidget daw = (DrawingAreaWidget) w;
    ISWRenderContext *ctx = daw->drawing_area.render_ctx;
    ISWDrawingCallbackData call_data;

    /* Lazy-create render context */
    if (!ctx && w->core.width > 0 && w->core.height > 0 && XtIsRealized(w)) {
	ctx = daw->drawing_area.render_ctx =
	    ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }
    if (!ctx) return;

    call_data.render_ctx = ctx;
    call_data.event = event;
    call_data.window = XtWindow(w);

    ISWRenderBegin(ctx);
    XtCallCallbackList(w, daw->drawing_area.expose_callbacks,
		       (XtPointer)&call_data);
    ISWRenderEnd(ctx);
}

static void
DrawingAreaInput(Widget w, xcb_generic_event_t *event,
		 String *params, Cardinal *num_params)
{
    DrawingAreaWidget daw = (DrawingAreaWidget) w;
    ISWDrawingCallbackData call_data;

    call_data.render_ctx = daw->drawing_area.render_ctx;
    call_data.event = event;
    call_data.window = XtIsRealized(w) ? XtWindow(w) : 0;

    XtCallCallbackList(w, daw->drawing_area.input_callbacks,
		       (XtPointer)&call_data);
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new,
	  ArgList args, Cardinal *num_args)
{
    DrawingAreaWidget cur = (DrawingAreaWidget) current;
    DrawingAreaWidget neww = (DrawingAreaWidget) new;

    if (cur->core.width != neww->core.width ||
	cur->core.height != neww->core.height) {
	if (neww->drawing_area.render_ctx) {
	    ISWRenderDestroy(neww->drawing_area.render_ctx);
	    neww->drawing_area.render_ctx = NULL;
	}
	return TRUE;
    }

    if (cur->core.background_pixel != neww->core.background_pixel)
	return TRUE;

    return FALSE;
}
