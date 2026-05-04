/*

Copyright (c) 1993, Oracle and/or its affiliates.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/
/********************************************************

Copyright 1988 by Hewlett-Packard Company
Copyright 1987, 1988, 1989 by Digital Equipment Corporation, Maynard, Massachusetts

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that copyright notice and this permission
notice appear in supporting documentation, and that the names of
Hewlett-Packard or Digital not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

/*

Copyright 1987, 1988, 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xinput.h>
#include "IntrinsicI.h"
#include "PassivGraI.h"
#include "StringDefs.h"
#include "EventI.h"
#include "uthash.h"

void
_IswFreePerWidgetInput(Widget w, IswPerWidgetInput pwi)
{
    LOCK_PROCESS;
    xcb_connection_t *dpy = w->core.display;
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    //XDeleteContext(IswDisplay(w), (Window) w, perWidgetInputContext);
    HASH_DEL(pd->PerWidgetContext, pwi);

    IswFree((char *) pwi);
    UNLOCK_PROCESS;
}

/*
 * This routine gets the passive list associated with the widget
 * from the context manager.
 */
IswPerWidgetInput
_IswGetPerWidgetInput(Widget widget, _IswBoolean create)
{
    IswPerWidgetInput pwi = NULL;
    xcb_connection_t *dpy = widget->core.display;
    IswPerDisplay pd = _IswGetPerDisplay(dpy);

    LOCK_PROCESS;
    //if (!perWidgetInputContext)
    //    perWidgetInputContext = XUniqueContext();

    /* Key on widget pointer, matching the original XSaveContext(dpy,
     * (Window)widget, ctx, data) semantics.  Using the X window ID
     * fails because the window hasn't been created yet when
     * IswSetKeyboardFocus is first called during ChangeManaged. */
    HASH_FIND_PTR(pd->PerWidgetContext, &widget, pwi);
    if (pwi == NULL && create) {
        pwi = (IswPerWidgetInput)
            __XtMalloc((unsigned) sizeof(IswPerWidgetInputRec));

        pwi->id = widget;
        pwi->focusKid = NULL;
        pwi->queryEventDescendant = NULL;
        pwi->focalPoint = IswUnrelated;
        pwi->keyList = pwi->ptrList = NULL;

        pwi->haveFocus =
            pwi->map_handler_added =
            pwi->realize_handler_added = pwi->active_handler_added = FALSE;

        IswAddCallback(widget, IswNdestroyCallback,
                      _IswDestroyServerGrabs, (IswPointer) pwi);

        HASH_ADD_PTR(pd->PerWidgetContext, id, pwi);
    }
    UNLOCK_PROCESS;
    return pwi;
}

void
_IswFillAncestorList(Widget **listPtr,
                    int *maxElemsPtr,
                    int *numElemsPtr,
                    Widget start,
                    Widget breakWidget)
{
#define CACHESIZE 16
    Cardinal i;
    Widget w;
    Widget *trace = *listPtr;

    /* First time in, allocate the ancestor list */
    if (trace == NULL) {
        trace = IswMallocArray(CACHESIZE, (Cardinal) sizeof(Widget));
        *maxElemsPtr = CACHESIZE;
    }
    /* First fill in the ancestor list */

    trace[0] = start;

    for (i = 1, w = IswParent(start);
         w != NULL && !IswIsShell(trace[i - 1]) && trace[i - 1] != breakWidget;
         w = IswParent(w), i++) {
        if (i == (Cardinal) *maxElemsPtr) {
            /* This should rarely happen, but if it does it'll probably
               happen again, so grow the ancestor list */
            *maxElemsPtr += CACHESIZE;
            trace = IswReallocArray(trace, (Cardinal) *maxElemsPtr,
                                   (Cardinal) sizeof(Widget));
        }
        trace[i] = w;
    }
    *listPtr = trace;
    *numElemsPtr = (int) i;
#undef CACHESIZE
}

Widget
_IswFindRemapWidget(xcb_generic_event_t *event,
                   Widget widget,
                   xcb_event_mask_t mask,
                   IswPerDisplayInput pdi)
{
    Widget dspWidget = widget;

    if (!pdi->traceDepth || !(widget == pdi->trace[0])) {
        _IswFillAncestorList(&pdi->trace, &pdi->traceMax,
                            &pdi->traceDepth, widget, NULL);
        pdi->focusWidget = NULL;        /* invalidate the focus
                                           cache */
    }
    if (mask & (XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE))
        dspWidget = _IswProcessKeyboardEvent((xcb_key_press_event_t *)event, widget, pdi);
    else if (mask & (XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE))
        dspWidget = _IswProcessPointerEvent((xcb_button_press_event_t *)event, widget, pdi);

    return dspWidget;
}

void
_IswUngrabBadGrabs(xcb_generic_event_t *event,
                  Widget widget,
                  xcb_event_mask_t mask,
                  IswPerDisplayInput pdi)
{

    if (event->response_type == XCB_INPUT_DEVICE_KEY_PRESS) {
        xcb_input_key_press_event_t *ke = (xcb_input_key_press_event_t *) event;
        if (IsServerGrab(pdi->keyboard.grabType) &&
            !_IswOnGrabList(pdi->keyboard.grab.widget, pdi->grabList))
            IswUngrabKeyboard(widget, ke->time);
    } else if (event->response_type == XCB_INPUT_DEVICE_KEY_RELEASE) {
        xcb_input_key_release_event_t *ke = (xcb_input_key_press_event_t *) event;
        if (IsServerGrab(pdi->keyboard.grabType) &&
            !_IswOnGrabList(pdi->keyboard.grab.widget, pdi->grabList))
            IswUngrabKeyboard(widget, ke->time);
    } else {
        xcb_input_button_press_event_t *ke = (xcb_input_button_press_event_t *) event;
        if (IsServerGrab(pdi->pointer.grabType) &&
            !_IswOnGrabList(pdi->pointer.grab.widget, pdi->grabList))
             IswUngrabPointer(widget, ke->time);
    }
}
