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

Copyright 1987, 1988, 1994, 1998  The Open Group

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
#include "IntrinsicI.h"

#include "PassivGraI.h"
#include "EventI.h"

#define _GetWindowedAncestor(w) (IswIsWidget(w) ? w : _IswWindowedAncestor(w))

/* InActiveSubtree cache of the current focus source and its ancestors */
static Widget *pathTrace = NULL;
static int pathTraceDepth = 0;
static int pathTraceMax = 0;

/* FindKeyDestination cache of focus destination and ancestors up to source */
static Widget *pseudoTrace = NULL;
static int pseudoTraceDepth = 0;
static int pseudoTraceMax = 0;

void
_IswClearAncestorCache(Widget widget)
{
    /* the caller must lock the process lock */
    if (pathTraceDepth && pathTrace[0] == widget)
        pathTraceDepth = 0;
}

static IswServerGrabPtr
CheckServerGrabs(xcb_generic_event_t *event, Widget *trace, Cardinal traceDepth)
{
    Cardinal i;

    for (i = traceDepth; i > 0; i--) {
        IswServerGrabPtr grab;

        if ((grab = _IswCheckServerGrabsOnWidget(event, trace[i - 1], KEYBOARD)))
            return (grab);
    }
    return (IswServerGrabPtr) 0;
}

static Boolean
IsParent(Widget a, Widget b)
{
    for (b = IswParent(b); b; b = IswParent(b)) {
        if (b == a)
            return TRUE;
        if (IswIsShell(b))
            return FALSE;
    }
    return FALSE;
}

#define RelRtn(lca, type) {*relTypeRtn = type; return lca;}

static Widget
CommonAncestor(register Widget a, register Widget b, IswGeneology *relTypeRtn)
{
    if (a == b) {
        RelRtn(a, IswMySelf)
    }
    else if (IsParent(a, b)) {
        RelRtn(a, IswMyAncestor)
    }
    else if (IsParent(b, a)) {
        RelRtn(b, IswMyDescendant)
    }
    else
        for (b = IswParent(b); b && !IswIsShell(b); b = IswParent(b))
            if (IsParent(b, a)) {
                RelRtn(b, IswMyCousin)
            }
    RelRtn(NULL, IswUnrelated)
}

#undef RelRtn

static Widget
_FindFocusWidget(Widget widget,
                 Widget *trace,
                 int traceDepth,
                 Boolean activeCheck,
                 Boolean *isTarget)
{
    int src;
    Widget dst;
    IswPerWidgetInput pwi = NULL;

    /* For each ancestor, starting at the top, see if it's forwarded */

    /* first check the trace list till done or we go to branch */
    for (src = traceDepth - 1, dst = widget; src > 0;) {
        if ((pwi = _IswGetPerWidgetInput(trace[src], FALSE))) {
            if (pwi->focusKid) {
                dst = pwi->focusKid;
                for (src--; src > 0 && trace[src] != dst; src--) {
                }
            }
            else
                dst = trace[--src];
        }
        else
            dst = trace[--src];
    }

    if (isTarget) {
        if (pwi && pwi->focusKid == widget)
            *isTarget = TRUE;
        else
            *isTarget = FALSE;
    }

    if (!activeCheck)
        while (IswIsWidget(dst)
               && (pwi = _IswGetPerWidgetInput(dst, FALSE))
               && pwi->focusKid)
            dst = pwi->focusKid;

    return dst;
}

static Widget
FindFocusWidget(Widget widget, IswPerDisplayInput pdi)
{
    if (pdi->focusWidget)
        return pdi->focusWidget;
    else
        return _FindFocusWidget(widget, pdi->trace, pdi->traceDepth, FALSE,
                                NULL);
}

Widget
IswGetKeyboardFocusWidget(Widget widget)
{
    IswPerDisplayInput pdi;
    Widget retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    pdi = _IswGetPerDisplayInput(IswDisplay(widget));
    retval = FindFocusWidget(widget, pdi);
    UNLOCK_APP(app);
    return retval;
}

static Boolean
IsOutside(xcb_key_press_event_t *e, Widget w)
{
    Position left, right, top, bottom;

    /*
     * if the pointer is outside the shell or inside
     * the window try to see if it would receive the
     * focus
     */
    IswTranslateCoords(w, 0, 0, &left, &top);
    /* We need to take borders into consideration */
    left = (Position) (left - w->core.border_width);
    top = (Position) (top - w->core.border_width);
    right = (Position) (left + w->core.width + w->core.border_width);
    bottom = (Position) (top + w->core.height + w->core.border_width);

    if ((e->root_x < left) || (e->root_y < top) ||
        (e->root_x > right) || (e->root_y > bottom))
        return TRUE;
    
    return FALSE;
}

static Widget
FindKeyDestination(Widget widget,
                   xcb_key_press_event_t *event,
                   IswServerGrabPtr prevGrab,
                   IswServerGrabType prevGrabType,
                   IswServerGrabPtr devGrab,
                   IswServerGrabType devGrabType,
                   IswPerDisplayInput pdi)
{

    Widget dspWidget;
    Widget focusWidget;

    LOCK_PROCESS;
    dspWidget =
        focusWidget =
        pdi->focusWidget = _GetWindowedAncestor(FindFocusWidget(widget, pdi));

    /*
     * If a grab is active from a previous activation then dispatch
     * based on owner_events ala protocol but with focus being
     * determined by IswSetKeyboardFocus.
     */
    if (IsAnyGrab(prevGrabType)) {
        if (prevGrab->ownerEvents)
            dspWidget = focusWidget;
        else
            dspWidget = prevGrab->widget;
    }
    else {
        /*
         * If the focus widget is the event widget or a descendant
         * then we can avoid the rest of this. Else ugh...
         */
        if (focusWidget != widget) {
            IswGeneology ewRelFw;        /* relationship of event widget to
                                           focusWidget */
            Widget lca;

            lca = CommonAncestor(widget, focusWidget, &ewRelFw);

            /*
             * if the event widget is an ancestor of focus due to the pointer
             * and/or the grab being in an ancestor and it's a passive grab
             * send to grab widget.
             * we are also dispatching to widget if ownerEvents and the event
             * is outside the client
             */
            if ((ewRelFw == IswMyAncestor) &&
                (devGrabType == IswPassiveServerGrab)) {
                if (IsOutside(event, widget) || event->response_type == XCB_KEY_PRESS)
                    dspWidget = devGrab->widget;
            }
            else {
                /*
                 * if the grab widget is not an ancestor of the focus
                 * release the grab in order to avoid locking. There
                 * is a possible case  in that ownerEvents true will fall
                 * through and if synch is set and the event widget
                 * could turn it off we'll lock. check for it ? why not
                 */
                if ((ewRelFw != IswMyAncestor)
                    && (devGrabType == IswPassiveServerGrab)
                    && (!IsAnyGrab(prevGrabType))
                    ) {
                    IswUngrabKeyboard(devGrab->widget, ((xcb_key_press_event_t *)event)->time);
                }
                /*
                 * if there isn't a grab with then check
                 * for a logical grab that would have been activated
                 * if the server was using Xt focus instead of server
                 * focus
                 */
                if ((event->response_type != XCB_KEY_PRESS) || (((xcb_key_press_event_t *)event)->detail == 0)  /* Xlib XIM composed input */
                    )
                    dspWidget = focusWidget;
                else {
                    IswServerGrabPtr grab;

                    if (!pseudoTraceDepth ||
                        !(focusWidget == pseudoTrace[0]) ||
                        !(lca == pseudoTrace[pseudoTraceDepth])) {
                        /*
                         * fill ancestor list from lca
                         * (non-inclusive)to focusWidget by
                         * passing in lca as breakWidget
                         */
                        _IswFillAncestorList(&pseudoTrace,
                                            &pseudoTraceMax,
                                            &pseudoTraceDepth,
                                            focusWidget, lca);
                        /* ignore lca */
                        pseudoTraceDepth--;
                    }
                    if ((grab = CheckServerGrabs((xcb_generic_event_t *) event,
                                                 pseudoTrace,
                                                 (Cardinal) pseudoTraceDepth)))
                    {
                        IswDevice device = &pdi->keyboard;

                        device->grabType = IswPseudoPassiveServerGrab;
                        pdi->activatingKey = (xcb_keycode_t) ((xcb_key_release_event_t *)event)->detail;
                        device->grab = *grab;
                        dspWidget = grab->widget;
                    }
                }
            }
        }
    }
    UNLOCK_PROCESS;
    return dspWidget;
}

Widget
_IswProcessKeyboardEvent(xcb_key_press_event_t *event, Widget widget, IswPerDisplayInput pdi)
{
    IswDevice device = &pdi->keyboard;
    IswServerGrabPtr devGrab = &device->grab;
    IswServerGrabRec prevGrabRec;
    IswServerGrabType prevGrabType = device->grabType;
    Widget dspWidget = NULL;
    Boolean deactivateGrab = FALSE;

    prevGrabRec = *devGrab;

    switch (event->response_type) {
    case XCB_KEY_PRESS:
    {
        xcb_key_press_event_t *keypress_event = (xcb_key_press_event_t *)event;
        IswServerGrabPtr newGrab;

        if (keypress_event->detail != 0 &&      /* Xlib XIM composed input */
            !IsServerGrab(device->grabType) &&
            (newGrab = CheckServerGrabs((xcb_generic_event_t *) event,
                                        pdi->trace,
                                        (Cardinal) pdi->traceDepth))) {
            /*
             * honor pseudo-grab from prior event by X
             * unlocking keyboard. Not Xt Unlock !
             */
            if (IsPseudoGrab(prevGrabType))
                xcb_ungrab_keyboard(IswDisplay(widget), keypress_event->time);
            else {
                /* Activate the grab */
                device->grab = *newGrab;
                pdi->activatingKey = (xcb_keycode_t) keypress_event->detail;
                device->grabType = IswPassiveServerGrab;
            }
        }
    }
        break;

    case XCB_KEY_RELEASE:
    {
        xcb_key_release_event_t *keyrelease_event = (xcb_key_release_event_t *)event;
        if (IsEitherPassiveGrab(device->grabType) &&
            (keyrelease_event->detail == pdi->activatingKey))
            deactivateGrab = TRUE;
    }
        break;
    }
    dspWidget = FindKeyDestination(widget, event,
                                   &prevGrabRec, prevGrabType,
                                   devGrab, device->grabType, pdi);
    if (deactivateGrab) {
        /* Deactivate the grab */
        device->grabType = IswNoServerGrab;
        pdi->activatingKey = 0;
    }
    return dspWidget;
}

static Widget
GetShell(Widget widget)
{
    Widget shell;

    for (shell = widget; shell && !IswIsShell(shell); shell = IswParent(shell)) {
    }
    return shell;
}

/*
 * Check that widget really has Xt focus due to it having received an
 * event
 */
typedef enum { NotActive = 0, IsActive, IsTarget } ActiveType;

static ActiveType
InActiveSubtree(Widget widget)
{
    Boolean isTarget;
    ActiveType retval;

    LOCK_PROCESS;
    if (!pathTraceDepth || widget != pathTrace[0]) {
        _IswFillAncestorList(&pathTrace,
                            &pathTraceMax, &pathTraceDepth, widget, NULL);
    }
    if (widget == _FindFocusWidget(widget,
                                   pathTrace, pathTraceDepth, TRUE, &isTarget))
        retval = (isTarget ? IsTarget : IsActive);
    else
        retval = NotActive;
    UNLOCK_PROCESS;
    return retval;
}

void
_IswHandleFocus(Widget widget,
               IswPointer client_data, /* child who wants focus */
              xcb_generic_event_t *event,
               Boolean *cont _X_UNUSED)
{
    IswPerDisplayInput pdi = _IswGetPerDisplayInput(IswDisplay(widget));
    IswPerWidgetInput pwi = (IswPerWidgetInput) client_data;
    IswGeneology oldFocalPoint = pwi->focalPoint;
    IswGeneology newFocalPoint = pwi->focalPoint;

    //#TODO verify this LLM output as well
    switch (event->response_type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        /*
         * We're getting the keyevents used to guarantee propagating
         * child interest ala ForwardEvent in R3
         */
        return;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        /*
         * If operating in a focus driven model, then enter and
         * leave events do not affect the keyboard focus.
         */
        {
            const xcb_enter_notify_event_t *enter_event = 
                (const xcb_enter_notify_event_t *)event;
            if ((enter_event->detail != XCB_NOTIFY_DETAIL_INFERIOR)
                && (enter_event->mode == XCB_NOTIFY_MODE_NORMAL)) {
                switch (oldFocalPoint) {
                case IswMyAncestor:
                    if (event->response_type == XCB_LEAVE_NOTIFY)
                        newFocalPoint = IswUnrelated;
                    break;
                case IswUnrelated:
                    break;
                case IswMySelf:
                    break;
                case IswMyDescendant:
                    break;
                }
            }
        }
        break;
    case XCB_FOCUS_IN:
        {
            const xcb_focus_in_event_t *focus_event = 
                (const xcb_focus_in_event_t *)event;
            switch (focus_event->detail) {
            case XCB_NOTIFY_DETAIL_NONLINEAR:
            case XCB_NOTIFY_DETAIL_ANCESTOR:
            case XCB_NOTIFY_DETAIL_INFERIOR:
                newFocalPoint = IswMySelf;
                break;
            case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
            case XCB_NOTIFY_DETAIL_VIRTUAL:
                newFocalPoint = IswMyDescendant;
                break;
            case XCB_NOTIFY_DETAIL_POINTER:
                newFocalPoint = IswMyAncestor;
                break;
            }
        }
        break;
    case XCB_FOCUS_OUT:
        {
            const xcb_focus_out_event_t *focus_event = 
                (const xcb_focus_out_event_t *)event;
            switch (focus_event->detail) {
            case XCB_NOTIFY_DETAIL_POINTER:
            case XCB_NOTIFY_DETAIL_NONLINEAR:
            case XCB_NOTIFY_DETAIL_ANCESTOR:
            case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
            case XCB_NOTIFY_DETAIL_VIRTUAL:
                newFocalPoint = IswUnrelated;
                break;
            case XCB_NOTIFY_DETAIL_INFERIOR:
                return;
            }
        }
        break;
    }

    if (newFocalPoint != oldFocalPoint) {
        Boolean add;
        Widget descendant = pwi->focusKid;

        pwi->focalPoint = newFocalPoint;

        if ((oldFocalPoint == IswUnrelated) &&
            InActiveSubtree(widget) != NotActive) {
            pdi->focusWidget = NULL;    /* invalidate the cache */
            pwi->haveFocus = TRUE;
            add = TRUE;
        }
        else if (newFocalPoint == IswUnrelated) {
            pdi->focusWidget = NULL;    /* invalidate the cache */
            pwi->haveFocus = FALSE;
            add = FALSE;
        }
        else
            return;

        if (descendant) {
            if (add) {
                _IswSendFocusEvent(descendant, XCB_FOCUS_IN);
            }
            else {
                _IswSendFocusEvent(descendant, XCB_FOCUS_OUT);
            }
        }
    }
}

static void
AddFocusHandler(Widget widget,
                Widget descendant,
                IswPerWidgetInput pwi,
                IswPerWidgetInput psi,
                IswPerDisplayInput pdi,
                EventMask oldEventMask)
{
    EventMask eventMask, targetEventMask;
    Widget target;

    /*
     * widget must now select for key events if the descendant is
     * interested in them.
     *
     * shell borders are not occluded by the child, they're occluded
     * by reparenting window managers. !!!
     */
    target = descendant ? _GetWindowedAncestor(descendant) : NULL;
    targetEventMask = IswBuildEventMask(target);
    eventMask = targetEventMask & (XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE);
    eventMask |= XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW;

    if (oldEventMask) {
        oldEventMask &= XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;
        oldEventMask |= XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW;

        if (oldEventMask != eventMask)
            IswRemoveEventHandler(widget, (oldEventMask & ~eventMask),
                                 False, _IswHandleFocus, (IswPointer) pwi);
    }

    if (oldEventMask != eventMask)
        IswAddEventHandler(widget, eventMask, False,
                          _IswHandleFocus, (IswPointer) pwi);

    /* What follows is too much grief to go through if the
     * target doesn't actually care about focus change events,
     * so just invalidate the focus cache & refill it when
     * the next input event actually arrives.
     */

    if (!(targetEventMask & XCB_EVENT_MASK_FOCUS_CHANGE)) {
        pdi->focusWidget = NULL;
        return;
    }

    if (IswIsRealized(widget) && !pwi->haveFocus) {
        if (psi->haveFocus) {
            int left, right, top, bottom;
            ActiveType act;

            /*
             * If the shell has the focus but the source widget
             * doesn't, it may only be because the source widget
             * wasn't previously tracking focus or crossing events.
             * If the target wants focus events, we have to
             * now determine whether the source has the focus.
             */

            if ((act = InActiveSubtree(widget)) == IsTarget)
                pwi->haveFocus = TRUE;
            else if (act == IsActive) {
                /*
                 * An ancestor contains the focus, so if source
                 * contains the pointer, then source has the focus.
                 */
                //#TODO another LLM rework to verify
                xcb_query_pointer_cookie_t cookie = xcb_query_pointer(IswDisplay(widget), IswWindow(widget));
                xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(IswDisplay(widget), cookie, NULL);
                if (reply) {
                    /* We need to take borders into consideration */
                    left = top = -((int) widget->core.border_width);
                    right = (int) (widget->core.width + (widget->core.border_width << 1));
                    bottom = (int) (widget->core.height + (widget->core.border_width << 1));

                    if (reply->win_x >= left && reply->win_x < right &&
                        reply->win_y >= top && reply->win_y < bottom)
                        pwi->haveFocus = TRUE;
                    
                    free(reply);
                }
            }
        }
    }
    if (pwi->haveFocus) {
        pdi->focusWidget = NULL;        /* invalidate the cache */
        _IswSendFocusEvent(target, XCB_FOCUS_IN);
    }
}

static void
QueryEventMask(Widget widget,           /* child who gets focus */
               IswPointer client_data,   /* ancestor giving it */
              xcb_generic_event_t *event _X_UNUSED,
               Boolean *cont _X_UNUSED)
{
    /* widget was once the target of an IswSetKeyboardFocus but
     * was unrealized at the time.   Make sure ancestor still wants
     * focus set here then install the handler now that we know the
     * complete event mask.
     */
    Widget ancestor = (Widget) client_data;
    IswPerWidgetInput pwi = _IswGetPerWidgetInput(ancestor, FALSE);

    if (pwi) {
        Widget target = pwi->queryEventDescendant;

        /* use of 'target' is non-standard hackery;
           allows focus to non-widget */
        if (pwi->focusKid == target) {
            AddFocusHandler(ancestor, target, pwi,
                            _IswGetPerWidgetInput(GetShell(ancestor), TRUE),
                            _IswGetPerDisplayInput(IswDisplay(ancestor)),
                            (EventMask) 0);
        }
        IswRemoveEventHandler(widget, IswAllEvents, True,
                             QueryEventMask, client_data);
        pwi->map_handler_added = FALSE;
    }
}

static void
FocusDestroyCallback(Widget widget _X_UNUSED,
                     IswPointer closure, /* Widget */
                     IswPointer call_data _X_UNUSED)
{
    IswSetKeyboardFocus((Widget) closure, NULL);
}

void
IswSetKeyboardFocus(Widget widget, Widget descendant)
{
    IswPerDisplayInput pdi;
    IswPerWidgetInput pwi;
    Widget oldDesc, oldTarget, target, hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    pdi = _IswGetPerDisplayInput(IswDisplay(widget));
    pwi = _IswGetPerWidgetInput(widget, TRUE);
    oldDesc = pwi->focusKid;

    if (descendant == widget)
        descendant = (Widget) None;

    target = descendant ? _GetWindowedAncestor(descendant) : NULL;
    oldTarget = oldDesc ? _GetWindowedAncestor(oldDesc) : NULL;

    if (descendant != oldDesc) {

        /* update the forward path */
        pwi->focusKid = descendant;

        /* all the rest handles focus ins and focus outs and misc gunk */

        if (oldDesc) {
            /* invalidate FindKeyDestination's ancestor list */
            if (pseudoTraceDepth && oldTarget == pseudoTrace[0])
                pseudoTraceDepth = 0;

            IswRemoveCallback(oldDesc, IswNdestroyCallback,
                             FocusDestroyCallback, (IswPointer) widget);

            if (!oldTarget->core.being_destroyed) {
                if (pwi->map_handler_added) {
                    IswRemoveEventHandler(oldTarget, IswAllEvents, True,
                                         QueryEventMask, (IswPointer) widget);
                    pwi->map_handler_added = FALSE;
                }
                if (pwi->haveFocus) {
                    _IswSendFocusEvent(oldTarget, XCB_FOCUS_OUT);
                }
            }
            else if (pwi->map_handler_added) {
                pwi->map_handler_added = FALSE;
            }

            if (pwi->haveFocus)
                pdi->focusWidget = NULL;        /* invalidate cache */

            /*
             * If there was a forward path then remove the handler if
             * the path is being set to null and it isn't a shell.
             * shells always have a handler for tracking focus for the
             * hierarchy.
             *
             * Keep the pwi record on the assumption that the client
             * will continue to dynamically assign focus for this widget.
             */
            if (!IswIsShell(widget) && !descendant) {
                IswRemoveEventHandler(widget, IswAllEvents, True,
                                     _IswHandleFocus, (IswPointer) pwi);
                pwi->haveFocus = FALSE;
            }
        }

        if (descendant) {
            Widget shell = GetShell(widget);
            IswPerWidgetInput psi = _IswGetPerWidgetInput(shell, TRUE);

            IswAddCallback(descendant, IswNdestroyCallback,
                          FocusDestroyCallback, (IswPointer) widget);

            AddFocusHandler(widget, descendant, pwi, psi, pdi,
                            oldTarget ? IswBuildEventMask(oldTarget) : 0);

            if (widget != shell)
                IswAddEventHandler(shell,
                                  XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW |
                                  XCB_EVENT_MASK_LEAVE_WINDOW, False, _IswHandleFocus,
                                  (IswPointer) psi);

            if (!IswIsRealized(target)) {
                IswAddEventHandler(target, (EventMask) XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                                  False, QueryEventMask, (IswPointer) widget);
                pwi->map_handler_added = TRUE;
                pwi->queryEventDescendant = descendant;
            }
        }
    }
    hookobj = IswHooksOfDisplay(IswDisplay(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHsetKeyboardFocus;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) descendant;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}
