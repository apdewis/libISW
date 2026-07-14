/*
Copyright (c) 1990, 1994  X Consortium

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
 *
 * This widget is used for press-and-hold style buttons.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>		/* for IswN and IswC defines */
#include <ISW/ISWInit.h>		/* for IswInitializeWidgetSet() */
#include <ISW/RepeaterP.h>		/* us */
#include <ISW/ISWRender.h>

static void tic(IswPointer, IswIntervalId *);	/* clock timeout */

#define DO_CALLBACK(rw) \
    IswCallCallbackList ((Widget) rw, rw->command.callbacks, (IswPointer)NULL)


#define ADD_TIMEOUT(rw,delay) \
  IswAppAddTimeOut (IswWidgetToApplicationContext ((Widget) rw), \
		   (unsigned long) delay, tic, (IswPointer) rw)

#define CLEAR_TIMEOUT(rw) \
  if ((rw)->repeater.timer) { \
      IswRemoveTimeOut ((rw)->repeater.timer); \
      (rw)->repeater.timer = 0; \
  }


/*
 * Translations to give user interface of press-notify...-release_or_leave
 */
static char defaultTranslations[] =
  "<EnterWindow>:     highlight() \n\
   <LeaveWindow>:     unhighlight() \n\
   <PrimaryDown>:        set() start() \n\
   <PrimaryUp>:          stop() unset() ";


/*
 * Actions added by this widget
 */
static void ActionStart(Widget, IswEvent *, String *, Cardinal *);
static void ActionStop(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actions[] = {
    { "start", ActionStart },		/* trigger timers */
    { "stop", ActionStop },		/* clear timers */
};


/*
 * New resources added by this widget
 */
static IswResource resources[] = {
#define off(field) IswOffsetOf(RepeaterRec, repeater.field)
    { IswNdecay, IswCDecay, IswRInt, sizeof (int),
	off(decay), IswRImmediate, (IswPointer) REP_DEF_DECAY },
    { IswNinitialDelay, IswCDelay, IswRInt, sizeof (int),
	off(initial_delay), IswRImmediate, (IswPointer) REP_DEF_INITIAL_DELAY },
    { IswNminimumDelay, IswCMinimumDelay, IswRInt, sizeof (int),
	off(minimum_delay), IswRImmediate, (IswPointer) REP_DEF_MINIMUM_DELAY },
    { IswNrepeatDelay, IswCDelay, IswRInt, sizeof (int),
	off(repeat_delay), IswRImmediate, (IswPointer) REP_DEF_REPEAT_DELAY },
    { IswNflash, IswCBoolean, IswRBoolean, sizeof (Boolean),
	off(flash), IswRImmediate, (IswPointer) FALSE },
    { IswNstartCallback, IswCStartCallback, IswRCallback, sizeof (IswPointer),
	off(start_callbacks), IswRImmediate, (IswPointer) NULL },
    { IswNstopCallback, IswCStopCallback, IswRCallback, sizeof (IswPointer),
	off(stop_callbacks), IswRImmediate, (IswPointer) NULL },
#undef off
};


/*
 * Class Methods
 */

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

RepeaterClassRec repeaterClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &commandClassRec,
    /* class_name		*/	"Repeater",
    /* widget_size		*/	sizeof(RepeaterRec),
    /* class_initialize		*/	IswInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	IswInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	IswNumber(actions),
    /* resources		*/	resources,
    /* num_resources		*/	IswNumber(resources),
    /* xrm_class		*/	ISW_NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	IswInheritResize,
    /* expose			*/	IswInheritExpose,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	IswInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	IswVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultTranslations,
    /* query_geometry		*/	IswInheritQueryGeometry,
    /* display_accelerator	*/	IswInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple fields */
    /* change_sensitive		*/	IswInheritChangeSensitive
  },
  { /* label fields */
    /* ignore			*/	0
  },
  { /* command fields */
    /* ignore			*/	0
  },
  { /* repeater fields */
    /* ignore                   */	0
  }
};

WidgetClass repeaterWidgetClass = (WidgetClass) &repeaterClassRec;


/*****************************************************************************
 *                                                                           *
 *			   repeater utility routines                         *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void
tic (IswPointer client_data, IswIntervalId *id)
{
    RepeaterWidget rw = (RepeaterWidget) client_data;

    rw->repeater.timer = 0;		/* timer is removed */
    if (rw->repeater.flash) {
 IswExposeProc expose;
 expose = repeaterWidgetClass->core_class.superclass->core_class.expose;
 ISWRenderContext *ctx = rw->label.render_ctx;
 if (ctx) {
     ISWRenderBegin(ctx);
     ISWRenderSetColor(ctx, rw->core.background_pixel);
     ISWRenderFillRectangle(ctx, 0, 0, rw->core.width, rw->core.height);
     ISWRenderEnd(ctx);
 }
 rw->command.set = FALSE;
 (*expose) ((Widget) rw, (IswEvent *) NULL, 0);
 if (ctx) {
     ISWRenderBegin(ctx);
     ISWRenderSetColor(ctx, rw->core.background_pixel);
     ISWRenderFillRectangle(ctx, 0, 0, rw->core.width, rw->core.height);
     ISWRenderEnd(ctx);
 }
 rw->command.set = TRUE;
 (*expose) ((Widget) rw, (IswEvent *) NULL, 0);
    }
    DO_CALLBACK (rw);

    rw->repeater.timer = ADD_TIMEOUT (rw, rw->repeater.next_delay);

					/* decrement delay time, but clamp */
    if (rw->repeater.decay) {
	rw->repeater.next_delay -= rw->repeater.decay;
	if (rw->repeater.next_delay < rw->repeater.minimum_delay)
	  rw->repeater.next_delay = rw->repeater.minimum_delay;
    }
}


/*****************************************************************************
 *                                                                           *
 * 			    repeater class methods                           *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void
Initialize (Widget greq, Widget gnew, ArgList args, Cardinal *num_args)
{
    RepeaterWidget new = (RepeaterWidget) gnew;

    if (new->repeater.minimum_delay < 0) new->repeater.minimum_delay = 0;
    new->repeater.timer = (IswIntervalId) 0;
}

static void
Destroy (Widget gw)
{
    CLEAR_TIMEOUT ((RepeaterWidget) gw);
}

/* ARGSUSED */
static Boolean
SetValues (Widget gcur, Widget greq, Widget gnew, ArgList args, Cardinal *num_args)
{
    RepeaterWidget cur = (RepeaterWidget) gcur;
    RepeaterWidget new = (RepeaterWidget) gnew;
    Boolean redisplay = FALSE;

    if (cur->repeater.minimum_delay != new->repeater.minimum_delay) {
	if (new->repeater.next_delay < new->repeater.minimum_delay)
	  new->repeater.next_delay = new->repeater.minimum_delay;
    }

    return redisplay;
}

/*****************************************************************************
 *                                                                           *
 * 			     repeater action procs                           *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void
ActionStart (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    RepeaterWidget rw = (RepeaterWidget) gw;

    CLEAR_TIMEOUT (rw);
    if (rw->repeater.start_callbacks)
      IswCallCallbackList (gw, rw->repeater.start_callbacks, (IswPointer)NULL);

    DO_CALLBACK (rw);
    rw->repeater.timer = ADD_TIMEOUT (rw, rw->repeater.initial_delay);
    rw->repeater.next_delay = rw->repeater.repeat_delay;
}


/* ARGSUSED */
static void
ActionStop (Widget gw, IswEvent *iswev, String *params, Cardinal *num_params)
{
    RepeaterWidget rw = (RepeaterWidget) gw;

    CLEAR_TIMEOUT ((RepeaterWidget) gw);
    if (rw->repeater.stop_callbacks)
      IswCallCallbackList (gw, rw->repeater.stop_callbacks, (IswPointer)NULL);
}

