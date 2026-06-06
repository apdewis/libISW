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
 * Grip.c - Grip Widget (Used by Paned Widget)
 *
 */
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/GripP.h>

static IswResource resources[] = {
   {IswNwidth, IswCWidth, IswRDimension, sizeof(Dimension),
      IswOffsetOf(GripRec, core.width), IswRImmediate,
      (IswPointer) DEFAULT_GRIP_SIZE},
   {IswNheight, IswCHeight, IswRDimension, sizeof(Dimension),
      IswOffsetOf(GripRec, core.height), IswRImmediate,
      (IswPointer) DEFAULT_GRIP_SIZE},
   {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
      IswOffsetOf(GripRec, core.background_pixel), IswRString,
      IswDefaultForeground},
   {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
      IswOffsetOf(GripRec, core.border_width), IswRImmediate, (IswPointer)0},
   {IswNcallback, IswCCallback, IswRCallback, sizeof(IswPointer),
      IswOffsetOf(GripRec, grip.grip_action), IswRCallback, NULL},
};

static void GripAction(Widget, IswEvent *, String *, Cardinal *);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Redisplay(Widget, IswEvent *, xcb_xfixes_region_t);
static void Destroy(Widget);

static IswActionsRec actionsList[] =
{
  {"GripAction",      GripAction},
};

#define SuperClass (&simpleClassRec)

GripClassRec gripClassRec = {
   {
/* core class fields */
    /* superclass         */   (WidgetClass) SuperClass,
    /* class name         */   "Grip",
    /* size               */   sizeof(GripRec),
    /* class initialize   */   IswInitializeWidgetSet,
    /* class_part_init    */   NULL,
    /* class_inited       */   FALSE,
    /* initialize         */   Initialize,
    /* initialize_hook    */   NULL,
    /* realize            */   IswInheritRealize,
    /* actions            */   actionsList,
    /* num_actions        */   IswNumber(actionsList),
    /* resources          */   resources,
    /* resource_count     */   IswNumber(resources),
    /* xrm_class          */   NULLQUARK,
    /* compress_motion    */   TRUE,
    /* compress_exposure  */   TRUE,
    /* compress_enterleave*/   TRUE,
    /* visible_interest   */   FALSE,
    /* destroy            */   Destroy,
    /* resize             */   NULL,
    /* expose             */   Redisplay,
    /* set_values         */   NULL,
    /* set_values_hook    */   NULL,
    /* set_values_almost  */   IswInheritSetValuesAlmost,
    /* get_values_hook    */   NULL,
    /* accept_focus       */   NULL,
    /* version            */   IswVersion,
    /* callback_private   */   NULL,
    /* tm_table           */   NULL,
    /* query_geometry     */   IswInheritQueryGeometry,
    /* display_accelerator*/   IswInheritDisplayAccelerator,
    /* extension          */   NULL
   },
/* Simple class fields initialization */
   {
    /* change_sensitive   */   IswInheritChangeSensitive
   },
/* Grip class fields initialization */
   {
    /* not used		  */   0
   }
};

WidgetClass gripWidgetClass = (WidgetClass) &gripClassRec;

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    /* core.width / core.height are now scaled centrally in xtCreate() */
    new->core.windowless = True;
    ((GripWidget) new)->grip.render_ctx = NULL;
}

/* Windowless: the Grip has no X window for the server to background-fill, so
   paint its own background into its surface.  Without this the grip is
   invisible (its surface stays transparent) and Paned's drag handles vanish. */
static void
Redisplay(Widget w, IswEvent *event, xcb_xfixes_region_t region)
{
    GripWidget gw = (GripWidget) w;
    (void) event; (void) region;

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    if (gw->grip.render_ctx == NULL)
        gw->grip.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (gw->grip.render_ctx) {
        ISWRenderBegin(gw->grip.render_ctx);
        ISWRenderSetColor(gw->grip.render_ctx, w->core.background_pixel);
        ISWRenderFillRectangle(gw->grip.render_ctx, 0, 0,
                               (int) w->core.width, (int) w->core.height);
        ISWRenderEnd(gw->grip.render_ctx);
    }
}

static void
Destroy(Widget w)
{
    GripWidget gw = (GripWidget) w;
    if (gw->grip.render_ctx != NULL) {
        ISWRenderDestroy(gw->grip.render_ctx);
        gw->grip.render_ctx = NULL;
    }
}

static void
GripAction(Widget widget, IswEvent *iswev, String *params, Cardinal *num_params)
{
    /* Kept bridge: call_data.event carries the native event opaquely to the
       grip callback; Paned's grip handler casts it back to xcb_generic_event_t*
       (GetEventLocation), so a neutral read here would break that consumer. */
    xcb_generic_event_t *event = (xcb_generic_event_t *) IswEventNative(iswev);
    IswGripCallDataRec call_data;

    call_data.event = event;
    call_data.params = params;
    call_data.num_params = *num_params;

    IswCallCallbacks( widget, IswNcallback, (IswPointer)&call_data );
}
