/***********************************************************
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
#include "StringDefs.h"
#include "SelectionI.h"
#include "ContextI.h"
#include <stdio.h>

void
_XtSetDefaultSelectionTimeout(unsigned long *timeout)
{
    *timeout = 5000;            /* default to 5 seconds */
}

void
XtSetSelectionTimeout(unsigned long timeout)
{
    XtAppSetSelectionTimeout(_XtDefaultAppContext(), timeout);
}

void
XtAppSetSelectionTimeout(XtAppContext app, unsigned long timeout)
{
    LOCK_APP(app);
    app->selectionTimeout = timeout;
    UNLOCK_APP(app);
}

unsigned long
XtGetSelectionTimeout(void)
{
    return XtAppGetSelectionTimeout(_XtDefaultAppContext());
}

unsigned long
XtAppGetSelectionTimeout(XtAppContext app)
{
    unsigned long retval;

    LOCK_APP(app);
    retval = app->selectionTimeout;
    UNLOCK_APP(app);
    return retval;
}

/* General utilities */

static void HandleSelectionReplies(Widget, XtPointer,xcb_generic_event_t *, Boolean *);
static void ReqTimedOut(XtPointer, XtIntervalId *);
static void HandlePropertyGone(Widget, XtPointer,xcb_generic_event_t *, Boolean *);
static void HandleGetIncrement(Widget, XtPointer,xcb_generic_event_t *, Boolean *);
static void HandleIncremental(xcb_connection_t *, Widget, xcb_atom_t, CallBackInfo,
                              unsigned long);

static XtContext selectPropertyContext = NULL;
static XtContext paramPropertyContext = NULL;
static XtContext multipleContext = NULL;

/* Multiple utilities */
static void AddSelectionRequests(Widget, xcb_atom_t, int, xcb_atom_t *,
                                 XtSelectionCallbackProc *, int, XtPointer *,
                                 Boolean *, xcb_atom_t *);
static Boolean IsGatheringRequest(Widget, xcb_atom_t);

#define PREALLOCED 32

/* Parameter utilities */
static void AddParamInfo(Widget, xcb_atom_t, xcb_atom_t);
static void RemoveParamInfo(Widget, xcb_atom_t);
static xcb_atom_t GetParamInfo(Widget, xcb_atom_t);

static int StorageSize[3] = { 1, sizeof(short), sizeof(long) };

#define BYTELENGTH(length, format) ((length) * (size_t)StorageSize[(format)>>4])
#define NUMELEM(bytelength, format) ((bytelength) / StorageSize[(format)>>4])
#define NUMELEM2(bytelength, format) ((unsigned long)(bytelength) / (unsigned long) StorageSize[(format)>>4])

/* Xlib and Xt are permitted to have different memory allocators, and in the
 * XtSelectionCallbackProc the client is instructed to free the selection
 * value with XtFree, so the selection value received from XGetWindowProperty
 * should be copied to memory allocated through Xt.  But copying is
 * undesirable since the selection value may be large, and, under normal
 * library configuration copying is unnecessary.
 */
#ifdef XTTRACEMEMORY
#define XT_COPY_SELECTION       1
#endif

static void
FreePropList(Widget w _X_UNUSED,
             XtPointer closure,
             XtPointer callData _X_UNUSED)
{
    PropList sarray = (PropList) closure;

    LOCK_PROCESS;
    XtDeleteContext(sarray->dpy, DefaultRootWindow(sarray->dpy),
                   selectPropertyContext);
    UNLOCK_PROCESS;
    XtFree((char *) sarray->list);
    XtFree((char *) closure);
}

static PropList
GetPropList(xcb_connection_t *dpy)
{
    PropList sarray;

    LOCK_PROCESS;
    if (selectPropertyContext == 0)
        selectPropertyContext = XtUniqueContext();
    if (XtFindContext(dpy, DefaultRootWindow(dpy), selectPropertyContext,
                     (void *) &sarray)) {
        xcb_atom_t atoms[4];
        xcb_intern_atom_cookie_t cookies[4];

        static char *names[] = {
            "INCR",
            "MULTIPLE",
            "TIMESTAMP",
            "_XT_SELECTION_0"
        };

        XtPerDisplay pd = _XtGetPerDisplay(dpy);

        sarray = (PropList) __XtMalloc((unsigned) sizeof(PropListRec));
        sarray->dpy = dpy;
        for(uint8_t i = 0; i < 4; i++) {
            cookies[i] = xcb_intern_atom(dpy, FALSE, strlen(names[i]), names[i]);
        }
        for(uint8_t i = 0; i < 4; i++) {
            xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(dpy, cookies[i], NULL);
            if (reply) {
                atoms[i] = reply->atom;
                free(reply);
            }
        }
        
        sarray->incr_atom = atoms[0];
        sarray->indirect_atom = atoms[1];
        sarray->timestamp_atom = atoms[2];
        sarray->propCount = 1;
        sarray->list =
            (SelectionProp) __XtMalloc((unsigned) sizeof(SelectionPropRec));
        sarray->list[0].prop = atoms[3];
        sarray->list[0].avail = TRUE;
        (void) XtSaveContext(dpy, DefaultRootWindow(dpy), selectPropertyContext,
                            (char *) sarray);
        _XtAddCallback(&pd->destroy_callbacks,
                       FreePropList, (XtPointer) sarray);
    }
    UNLOCK_PROCESS;
    return sarray;
}

static xcb_atom_t
GetSelectionProperty(xcb_connection_t *dpy)
{
    SelectionProp p;
    int propCount;
    char propname[80];
    PropList sarray = GetPropList(dpy);

    for (p = sarray->list, propCount = sarray->propCount;
         propCount; p++, propCount--) {
        if (p->avail) {
            p->avail = FALSE;
            return (p->prop);
        }
    }
    propCount = sarray->propCount++;
    sarray->list = XtReallocArray(sarray->list, (Cardinal) sarray->propCount,
                                  (Cardinal) sizeof(SelectionPropRec));
    (void) snprintf(propname, sizeof(propname), "_XT_SELECTION_%d", propCount);
    //sarray->list[propCount].prop = XInternAtom(dpy, propname, FALSE);
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(dpy, 0, strlen(propname), propname);
    sarray->list[propCount].prop = xcb_intern_atom_reply(dpy, cookie, NULL)->atom;
    sarray->list[propCount].avail = FALSE;
    return (sarray->list[propCount].prop);
}

static void
FreeSelectionProperty(xcb_connection_t *dpy, xcb_atom_t prop)
{
    SelectionProp p;
    int propCount;
    PropList sarray;

    if (prop == None)
        return;
    LOCK_PROCESS;
    if (XtFindContext(dpy, DefaultRootWindow(dpy), selectPropertyContext,
                     (void *) &sarray))
        XtAppErrorMsg(XtDisplayToApplicationContext(dpy),
                      "noSelectionProperties", "freeSelectionProperty",
                      XtCXtToolkitError,
                      "internal error: no selection property context for display",
                      NULL, NULL);
    UNLOCK_PROCESS;
    for (p = sarray->list, propCount = sarray->propCount;
         propCount; p++, propCount--)
        if (p->prop == prop) {
            p->avail = TRUE;
            return;
        }
}

static void
FreeInfo(CallBackInfo info)
{
    XtFree((char *) info->incremental);
    XtFree((char *) info->callbacks);
    XtFree((char *) info->req_closure);
    XtFree((char *) info->target);
    XtFree((char *) info);
}

static CallBackInfo
MakeInfo(Select ctx,
         XtSelectionCallbackProc *callbacks,
         XtPointer *closures,
         int count,
         Widget widget,
         xcb_timestamp_t time,
         Boolean *incremental,
         xcb_atom_t *properties)
{
    CallBackInfo info = XtNew(CallBackInfoRec);

    info->ctx = ctx;
    info->callbacks = XtMallocArray((Cardinal) count,
                                    (Cardinal) sizeof(XtSelectionCallbackProc));
    (void) memcpy(info->callbacks, callbacks,
                  (size_t) count * sizeof(XtSelectionCallbackProc));
    info->req_closure = XtMallocArray((Cardinal) count,
                                      (Cardinal) sizeof(XtPointer));
    (void) memcpy(info->req_closure, closures,
                  (size_t) count * sizeof(XtPointer));
    if (count == 1 && properties != NULL && properties[0] != None)
        info->property = properties[0];
    else {
        info->property = GetSelectionProperty(XtDisplay(widget));
        xcb_delete_property(XtDisplay(widget), XtWindow(widget), info->property);
    }
    info->proc = HandleSelectionReplies;
    info->widget = widget;
    info->time = time;
    info->incremental = XtMallocArray((Cardinal) count,
                                      (Cardinal) sizeof(Boolean));
    (void) memcpy(info->incremental, incremental,
                  (size_t) count * sizeof(Boolean));
    info->current = 0;
    info->value = NULL;
    return (info);
}

static void
RequestSelectionValue(CallBackInfo info, xcb_atom_t selection, xcb_atom_t target)
{
#ifndef DEBUG_WO_TIMERS
    XtAppContext app = XtWidgetToApplicationContext(info->widget);

    info->timeout = XtAppAddTimeOut(app,
                                    app->selectionTimeout, ReqTimedOut,
                                    (XtPointer) info);
#endif
    XtAddEventHandler(info->widget, (EventMask) 0, TRUE,
                      HandleSelectionReplies, (XtPointer) info);

    xcb_convert_selection(info->ctx->dpy, XtWindow(info->widget), selection, target, info->property, info->time);
}

static XtContext selectContext = NULL;

static Select
NewContext(xcb_connection_t *dpy, xcb_atom_t selection)
{
    /* assert(selectContext != 0) */
    Select ctx = XtNew(SelectRec);

    ctx->dpy = dpy;
    ctx->selection = selection;
    ctx->widget = NULL;
    ctx->prop_list = GetPropList(dpy);
    ctx->ref_count = 0;
    ctx->free_when_done = FALSE;
    ctx->was_disowned = FALSE;
    LOCK_PROCESS;
    (void) XtSaveContext(dpy, (xcb_window_t) selection, selectContext, (char *) ctx);
    UNLOCK_PROCESS;
    return ctx;
}

static Select
FindCtx(xcb_connection_t *dpy, xcb_atom_t selection)
{
    Select ctx;

    LOCK_PROCESS;
    if (selectContext == 0)
        selectContext = XtUniqueContext();
    if (XtFindContext(dpy, (xcb_window_t) selection, selectContext, (void *) &ctx))
        ctx = NewContext(dpy, selection);
    UNLOCK_PROCESS;
    return ctx;
}

static void
WidgetDestroyed(Widget widget, XtPointer closure, XtPointer data _X_UNUSED)
{
    Select ctx = (Select) closure;

    if (ctx->widget == widget) {
        if (ctx->free_when_done)
            XtFree((char *) ctx);
        else
            ctx->widget = NULL;
    }
}

/* Selection Owner code */

static void HandleSelectionEvents(Widget, XtPointer,xcb_generic_event_t *, Boolean *);

static Boolean
LoseSelection(Select ctx, Widget widget, xcb_atom_t selection, xcb_timestamp_t time)
{
    if ((ctx->widget == widget) && (ctx->selection == selection) &&     /* paranoia */
        !ctx->was_disowned && ((time == CurrentTime) || (time >= ctx->time))) {
        XtRemoveEventHandler(widget, (EventMask) 0, TRUE,
                             HandleSelectionEvents, (XtPointer) ctx);
        XtRemoveCallback(widget, XtNdestroyCallback,
                         WidgetDestroyed, (XtPointer) ctx);
        ctx->was_disowned = TRUE;       /* widget officially loses ownership */
        /* now inform widget */
        if (ctx->loses) {
            if (ctx->incremental)
                (*(XtLoseSelectionIncrProc) ctx->loses)
                    (widget, &ctx->selection, ctx->owner_closure);
            else
                (*ctx->loses) (widget, &ctx->selection);
        }
        return (TRUE);
    }
    else
        return (FALSE);
}

static XtContext selectWindowContext = NULL;

/* %%% Xlib.h should make this public! */
//typedef int (*xErrorHandler) (xcb_connection_t *, XErrorEvent *);

//static xErrorHandler oldErrorHandler = NULL;
static unsigned long firstProtectRequest;
static xcb_window_t errorWindow;

//#TODO given what this code claims to do it will be worth further examination on how 
//things work in XCB to see if some other means is necessary
//static int
//LocalErrorHandler(xcb_connection_t *dpy, XErrorEvent *error)
//{
//    int retval;
//
//    /* If BadWindow error on selection requestor, nothing to do but let
//     * the transfer timeout.  Otherwise, invoke saved error handler. */
//
//    LOCK_PROCESS;
//
//    if (error->error_code == BadWindow && error->resourceid == errorWindow &&
//        error->serial >= firstProtectRequest) {
//        UNLOCK_PROCESS;
//        return 0;
//    }
//
//    if (oldErrorHandler == NULL) {
//        UNLOCK_PROCESS;
//        return 0;               /* should never happen */
//    }
//
//    retval = (*oldErrorHandler) (dpy, error);
//    UNLOCK_PROCESS;
//    return retval;
//}
//
//static void
//StartProtectedSection(xcb_connection_t *dpy, xcb_window_t window)
//{
//    /* protect ourselves against request window being destroyed
//     * before completion of transfer */
//
//    LOCK_PROCESS;
//    oldErrorHandler = XSetErrorHandler(LocalErrorHandler);
//    firstProtectRequest = NextRequest(dpy);
//    errorWindow = window;
//    UNLOCK_PROCESS;
//}
//
//static void
//EndProtectedSection(xcb_connection_t *dpy)
//{
//    /* flush any generated errors on requestor and
//     * restore original error handler */
//
//    XSync(dpy, False);
//
//    LOCK_PROCESS;
//    XSetErrorHandler(oldErrorHandler);
//    oldErrorHandler = NULL;
//    UNLOCK_PROCESS;
//}

static void
AddHandler(Request req, EventMask mask, XtEventHandler proc, XtPointer closure)
{
    xcb_connection_t *dpy = req->ctx->dpy;
    xcb_window_t window = req->requestor;
    Widget widget = XtWindowToWidget(dpy, window);

    if (widget != NULL)
        req->widget = widget;
    else
        widget = req->widget;

    if (XtWindow(widget) == window)
        XtAddEventHandler(widget, mask, False, proc, closure);
    else {
        RequestWindowRec *requestWindowRec;

        LOCK_PROCESS;
        if (selectWindowContext == 0)
            selectWindowContext = XtUniqueContext();
        if (XtFindContext(dpy, window, selectWindowContext,
                         (void *) &requestWindowRec)) {
            requestWindowRec = XtNew(RequestWindowRec);
            requestWindowRec->active_transfer_count = 0;
            (void) XtSaveContext(dpy, window, selectWindowContext,
                                (char *) requestWindowRec);
        }
        UNLOCK_PROCESS;
        if (requestWindowRec->active_transfer_count++ == 0) {
            XtRegisterDrawable(dpy, window, widget);
            xcb_change_window_attributes(dpy, window, XCB_CW_EVENT_MASK, &mask);
        }
        XtAddRawEventHandler(widget, mask, FALSE, proc, closure);
    }
}

static void
RemoveHandler(Request req,
              EventMask mask,
              XtEventHandler proc,
              XtPointer closure)
{
    xcb_connection_t *dpy = req->ctx->dpy;
    xcb_window_t window = req->requestor;
    Widget widget = req->widget;

    if ((XtWindowToWidget(dpy, window) == widget) &&
        (XtWindow(widget) != window)) {
        /* we had to hang this window onto our widget; take it off */
        RequestWindowRec *requestWindowRec;

        XtRemoveRawEventHandler(widget, mask, TRUE, proc, closure);
        LOCK_PROCESS;
        (void) XtFindContext(dpy, window, selectWindowContext,
                            (void *) &requestWindowRec);
        UNLOCK_PROCESS;
        if (--requestWindowRec->active_transfer_count == 0) {
            XtUnregisterDrawable(dpy, window);
            //StartProtectedSection(dpy, window);
            xcb_change_window_attributes(dpy, window, XCB_CW_EVENT_MASK, (uint32_t[]){0});
            //EndProtectedSection(dpy);
            LOCK_PROCESS;
            (void) XtDeleteContext(dpy, window, selectWindowContext);
            UNLOCK_PROCESS;
            XtFree((char *) requestWindowRec);
        }
    }
    else {
        XtRemoveEventHandler(widget, mask, TRUE, proc, closure);
    }
}

static void
OwnerTimedOut(XtPointer closure, XtIntervalId *id _X_UNUSED)
{
    Request req = (Request) closure;
    Select ctx = req->ctx;

    if (ctx->incremental && (ctx->owner_cancel != NULL)) {
        (*ctx->owner_cancel) (ctx->widget, &ctx->selection,
                              &req->target, (XtRequestId *) &req,
                              ctx->owner_closure);
    }
    else {
        if (ctx->notify == NULL)
            XtFree((char *) req->value);
        else {
            /* the requestor hasn't deleted the property, but
             * the owner needs to free the value.
             */
            if (ctx->incremental)
                (*(XtSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &ctx->selection, &req->target,
                     (XtRequestId *) &req, ctx->owner_closure);
            else
                (*ctx->notify) (ctx->widget, &ctx->selection, &req->target);
        }
    }

    RemoveHandler(req, (EventMask) PropertyChangeMask,
                  HandlePropertyGone, closure);
    XtFree((char *) req);
    if (--ctx->ref_count == 0 && ctx->free_when_done)
        XtFree((char *) ctx);
}

static void
SendIncrement(Request incr)
{
    xcb_connection_t *dpy = incr->ctx->dpy;

    unsigned long incrSize = (unsigned long) MAX_SELECTION_INCR(dpy);

    if (incrSize > incr->bytelength - incr->offset)
        incrSize = incr->bytelength - incr->offset;
    //StartProtectedSection(dpy, incr->requestor);
    xcb_change_property(dpy, XCB_PROP_MODE_REPLACE, incr->requestor, incr->property,
                    incr->type, incr->format,
                    NUMELEM((int) incrSize, incr->format),
                    (unsigned char *) incr->value + incr->offset);
    xcb_flush(dpy);
    //EndProtectedSection(dpy);
    incr->offset += incrSize;
}

static void
AllSent(Request req)
{
    Select ctx = req->ctx;

    //StartProtectedSection(ctx->dpy, req->requestor);
    xcb_change_property(ctx->dpy, XCB_PROP_MODE_REPLACE, req->requestor,
                    req->property, req->type, req->format,
                    0, (unsigned char *) NULL);
    xcb_flush(ctx->dpy);
    //EndProtectedSection(ctx->dpy);
    req->allSent = TRUE;

    if (ctx->notify == NULL)
        XtFree((char *) req->value);
}

static void
HandlePropertyGone(Widget widget _X_UNUSED,
                   XtPointer closure,
                  xcb_generic_event_t *ev,
                   Boolean *cont _X_UNUSED)
{
    xcb_property_notify_event_t *event = (xcb_property_notify_event_t *) ev;
    Request req = (Request) closure;
    Select ctx = req->ctx;

    if (((event->response_type & ~0x80) != XCB_PROPERTY_NOTIFY) ||
        (event->state != XCB_PROPERTY_DELETE) ||
        (event->atom != req->property) || (event->window != req->requestor))
        return;
#ifndef DEBUG_WO_TIMERS
    XtRemoveTimeOut(req->timeout);
#endif
    if (req->allSent) {
        if (ctx->notify) {
            if (ctx->incremental) {
                (*(XtSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &ctx->selection, &req->target,
                     (XtRequestId *) &req, ctx->owner_closure);
            }
            else
                (*ctx->notify) (ctx->widget, &ctx->selection, &req->target);
        }
        RemoveHandler(req, (EventMask) PropertyChangeMask,
                      HandlePropertyGone, closure);
        XtFree((char *) req);
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            XtFree((char *) ctx);
    }
    else {                      /* is this part of an incremental transfer? */
        if (ctx->incremental) {
            if (req->bytelength == 0)
                AllSent(req);
            else {
                unsigned long size =
                    (unsigned long) MAX_SELECTION_INCR(ctx->dpy);
                SendIncrement(req);
                (*(XtConvertSelectionIncrProc) ctx->convert)
                    (ctx->widget, &ctx->selection, &req->target,
                     &req->type, &req->value,
                     &req->bytelength, &req->format,
                     &size, ctx->owner_closure, (XtPointer *) &req);
                if (req->bytelength)
                    req->bytelength = BYTELENGTH(req->bytelength, req->format);
                req->offset = 0;
            }
        }
        else {
            if (req->offset < req->bytelength)
                SendIncrement(req);
            else
                AllSent(req);
        }
#ifndef DEBUG_WO_TIMERS
        {
            XtAppContext app = XtWidgetToApplicationContext(req->widget);

            req->timeout = XtAppAddTimeOut(app,
                                           app->selectionTimeout, OwnerTimedOut,
                                           (XtPointer) req);
        }
#endif
    }
}

static void
PrepareIncremental(Request req,
                   Widget widget,
                   xcb_window_t window,
                   xcb_atom_t property _X_UNUSED,
                   xcb_atom_t target,
                   xcb_atom_t targetType,
                   XtPointer value,
                   unsigned long length,
                   int format)
{
    req->type = targetType;
    req->value = value;
    req->bytelength = BYTELENGTH(length, format);
    req->format = format;
    req->offset = 0;
    req->target = target;
    req->widget = widget;
    req->allSent = FALSE;
#ifndef DEBUG_WO_TIMERS
    {
        XtAppContext app = XtWidgetToApplicationContext(widget);

        req->timeout = XtAppAddTimeOut(app,
                                       app->selectionTimeout, OwnerTimedOut,
                                       (XtPointer) req);
    }
#endif
    AddHandler(req, (EventMask) PropertyChangeMask,
               HandlePropertyGone, (XtPointer) req);
/* now send client INCR property */
    xcb_change_property(req->ctx->dpy, XCB_PROP_MODE_REPLACE, window, req->property,
                   req->ctx->prop_list->incr_atom, 32, 1, &req->bytelength);
}

static Boolean
GetConversion(Select ctx,       /* logical owner */
              xcb_selection_request_event_t *event,
              xcb_atom_t target,
              xcb_atom_t property,    /* requestor's property */
              Widget widget)    /* physical owner (receives events) */
{                  
    XtPointer value = NULL;
    unsigned long length;
    int format;
    xcb_atom_t targetType;
    Request req = XtNew(RequestRec);
    Boolean timestamp_target = (target == ctx->prop_list->timestamp_atom);

    req->ctx = ctx;
    req->event = *event;
    req->property = property;
    req->requestor = event->requestor;

    if (timestamp_target) {
        value = __XtMalloc(sizeof(long));
        *(long *) value = (long) ctx->time;
        targetType = XA_INTEGER;
        length = 1;
        format = 32;
    }
    else {
        ctx->ref_count++;
        if (ctx->incremental == TRUE) {
            unsigned long size = (unsigned long) MAX_SELECTION_INCR(ctx->dpy);

            if ((*(XtConvertSelectionIncrProc) ctx->convert)
                (ctx->widget, &event->selection, &target,
                 &targetType, &value, &length, &format,
                 &size, ctx->owner_closure, (XtRequestId *) &req)
                == FALSE) {
                XtFree((char *) req);
                ctx->ref_count--;
                return (FALSE);
            }
            //StartProtectedSection(ctx->dpy, event->requestor);
            PrepareIncremental(req, widget, event->requestor, property,
                               target, targetType, value, length, format);
            return (TRUE);
        }
        ctx->req = req;
        if ((*ctx->convert) (ctx->widget, &event->selection, &target,
                             &targetType, &value, &length, &format) == FALSE) {
            XtFree((char *) req);
            ctx->req = NULL;
            ctx->ref_count--;
            return (FALSE);
        }
        ctx->req = NULL;
    }
    //StartProtectedSection(ctx->dpy, event->requestor);
    if (BYTELENGTH(length, format) <=
        (unsigned long) MAX_SELECTION_INCR(ctx->dpy)) {
        if (!timestamp_target) {
            if (ctx->notify != NULL) {
                req->target = target;
                req->widget = widget;
                req->allSent = TRUE;
#ifndef DEBUG_WO_TIMERS
                {
                    XtAppContext app =
                        XtWidgetToApplicationContext(req->widget);
                    req->timeout =
                        XtAppAddTimeOut(app, app->selectionTimeout,
                                        OwnerTimedOut, (XtPointer) req);
                }
#endif
                AddHandler(req, (EventMask) PropertyChangeMask,
                           HandlePropertyGone, (XtPointer) req);
            }
            else
                ctx->ref_count--;
        }
        xcb_change_property(ctx->dpy, XCB_PROP_MODE_REPLACE, event->requestor, property,
                   targetType, format, length, value);
        /* free storage for client if no notify proc */
        if (timestamp_target || ctx->notify == NULL) {
            XtFree((char *) value);
            XtFree((char *) req);
        }
    }
    else {
        PrepareIncremental(req, widget, event->requestor, property,
                           target, targetType, value, length, format);
    }
    return (TRUE);
}

static void
HandleSelectionEvents(Widget widget,
                      XtPointer closure,
                      xcb_generic_event_t *event,
                      Boolean *cont _X_UNUSED)
{
    Select ctx;
    xcb_selection_request_event_t ev;
    xcb_atom_t target;

    ctx = (Select) closure;
    switch (event->response_type) {
    case XCB_SELECTION_CLEAR:
        xcb_selection_clear_event_t *sce = (xcb_selection_clear_event_t *)event;
        /* if this event is not for the selection we registered for,
         * don't do anything */
        if (ctx->selection != sce->selection ||
            ctx->serial > sce->sequence)
            break;
        (void) LoseSelection(ctx, widget, sce->selection,
                             sce->time);
        break;
    case XCB_SELECTION_REQUEST:
        xcb_selection_request_event_t *sre = (xcb_selection_request_event_t *)event;
        /* if this event is not for the selection we registered for,
         * don't do anything */
        if (ctx->selection != sre->selection)
            break;
        ev.response_type = XCB_SELECTION_NOTIFY;
        //ev.display = widget->display;

        ev.requestor = sre->requestor;
        ev.selection = sre->selection;
        ev.time = sre->time;
        ev.target = sre->target;
        if (sre->property == None)  /* obsolete requestor */
            sre->property = sre->target;
        if (ctx->widget != widget || ctx->was_disowned
            || ((sre->time != XCB_CURRENT_TIME)
                && (sre->time < ctx->time))) {
            ev.property = None;
            //StartProtectedSection(ctx->dpy, ev.requestor);
        }
        else {
            if (ev.target == ctx->prop_list->indirect_atom) {
                IndirectPair *p;
                int format;
                unsigned long length;
                unsigned char *value = NULL;
                int count;
                Boolean writeback = FALSE;

                ev.property = sre->property;
                //StartProtectedSection(ctx->dpy, ev.requestor);
                xcb_get_property_cookie_t cookie = xcb_get_property(
                    ctx->dpy, 0, ev.requestor, sre->property, XCB_ATOM_ATOM, 0, 1000000);
                xcb_get_property_reply_t *reply = xcb_get_property_reply(ctx->dpy, cookie, NULL);

                if (reply != NULL) {
                    count = (int) (BYTELENGTH(reply->value_len, reply->type) / sizeof(IndirectPair));
                } else {
                    count = 0;
                }
                for (p = (IndirectPair *) value; count; p++, count--) {
                    //EndProtectedSection(ctx->dpy);
                    if (!GetConversion(ctx, (xcb_selection_request_event_t *) event,
                                       p->target, p->property, widget)) {

                        p->target = None;
                        writeback = TRUE;
                        //StartProtectedSection(ctx->dpy, ev.requestor);
                    }
                }
                if (writeback)
                    xcb_change_property(ctx->dpy, XCB_PROP_MODE_REPLACE, ev.requestor,
                        sre->property, target,
                        format, length, value);
                XFree((char *) value);
            }
            else {              /* not multiple */

                if (GetConversion(ctx, sre,
                                  sre->target,
                                  sre->property, widget))
                    ev.property = sre->property;
                else {
                    ev.property = None;
                    //StartProtectedSection(ctx->dpy, ev.requestor);
                }
            }
        }
        xcb_send_event(ctx->dpy, 0, ev.requestor, 0, (char *)&ev);

        //EndProtectedSection(ctx->dpy);

        break;
    }
}

static Boolean
OwnSelection(Widget widget,
             xcb_atom_t selection,
             xcb_timestamp_t time,
             XtConvertSelectionProc convert,
             XtLoseSelectionProc lose,
             XtSelectionDoneProc notify,
             XtCancelConvertSelectionProc cancel,
             XtPointer closure,
             Boolean incremental)
{
    Select ctx;
    Select oldctx = NULL;

    if (!XtIsRealized(widget))
        return False;

    ctx = FindCtx(XtDisplay(widget), selection);
    if (ctx->widget != widget || ctx->time != time ||
        ctx->ref_count || ctx->was_disowned) {
        Boolean replacement = FALSE;
        xcb_window_t window = XtWindow(widget);
        //uint32_t serial = xcb_get_serial(ctx->dpy);

        //XSetSelectionOwner(ctx->dpy, selection, window, time);
        xcb_set_selection_owner(ctx->dpy, XCB_NONE, selection, time);
        xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(ctx->dpy, selection);
        xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply(ctx->dpy, cookie, NULL);
        if(!reply) return FALSE;
        if (reply->owner != window) {
            free(reply);
            return FALSE;
        }

        free(reply);

        if (ctx->ref_count) {   /* exchange is in-progress */
#ifdef DEBUG_ACTIVE
            printf
                ("Active exchange for widget \"%s\"; selection=0x%lx, ref_count=%d\n",
                 XtName(widget), (long) selection, ctx->ref_count);
#endif
            if (ctx->widget != widget ||
                ctx->convert != convert ||
                ctx->loses != lose ||
                ctx->notify != notify ||
                ctx->owner_cancel != cancel ||
                ctx->incremental != incremental ||
                ctx->owner_closure != closure) {
                if (ctx->widget == widget) {
                    XtRemoveEventHandler(widget, (EventMask) 0, TRUE,
                                         HandleSelectionEvents,
                                         (XtPointer) ctx);
                    XtRemoveCallback(widget, XtNdestroyCallback,
                                     WidgetDestroyed, (XtPointer) ctx);
                    replacement = TRUE;
                }
                else if (!ctx->was_disowned) {
                    oldctx = ctx;
                }
                ctx->free_when_done = TRUE;
                ctx = NewContext(XtDisplay(widget), selection);
            }
            else if (!ctx->was_disowned) {      /* current owner is new owner */
                ctx->time = time;
                return TRUE;
            }
        }
        if (ctx->widget != widget || ctx->was_disowned || replacement) {
            if (ctx->widget && !ctx->was_disowned && !replacement) {
                oldctx = ctx;
                oldctx->free_when_done = TRUE;
                ctx = NewContext(XtDisplay(widget), selection);
            }
            XtAddEventHandler(widget, (EventMask) 0, TRUE,
                              HandleSelectionEvents, (XtPointer) ctx);
            XtAddCallback(widget, XtNdestroyCallback,
                          WidgetDestroyed, (XtPointer) ctx);
        }
        ctx->widget = widget;   /* Selection officially changes hands. */
        ctx->time = time;
        //ctx->serial = serial;
    }
    ctx->convert = convert;
    ctx->loses = lose;
    ctx->notify = notify;
    ctx->owner_cancel = cancel;
    XtSetBit(ctx->incremental, incremental);
    ctx->owner_closure = closure;
    ctx->was_disowned = FALSE;

    /* Defer calling the previous selection owner's lose selection procedure
     * until the new selection is established, to allow the previous
     * selection owner to ask for the new selection to be converted in
     * the lose selection procedure.  The context pointer is the closure
     * of the event handler and the destroy callback, so the old context
     * pointer and the record contents must be preserved for LoseSelection.
     */
    if (oldctx) {
        (void) LoseSelection(oldctx, oldctx->widget, selection, oldctx->time);
        if (!oldctx->ref_count && oldctx->free_when_done)
            XtFree((char *) oldctx);
    }
    return TRUE;
}

Boolean
XtOwnSelection(Widget widget,
               xcb_atom_t selection,
               xcb_timestamp_t time,
               XtConvertSelectionProc convert,
               XtLoseSelectionProc lose,
               XtSelectionDoneProc notify)
{
    Boolean retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    retval = OwnSelection(widget, selection, time, convert, lose, notify,
                          (XtCancelConvertSelectionProc) NULL,
                          (XtPointer) NULL, FALSE);
    UNLOCK_APP(app);
    return retval;
}

Boolean
XtOwnSelectionIncremental(Widget widget,
                          xcb_atom_t selection,
                          xcb_timestamp_t time,
                          XtConvertSelectionIncrProc convert,
                          XtLoseSelectionIncrProc lose,
                          XtSelectionDoneIncrProc notify,
                          XtCancelConvertSelectionProc cancel,
                          XtPointer closure)
{
    Boolean retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    retval = OwnSelection(widget, selection, time,
                          (XtConvertSelectionProc) convert,
                          (XtLoseSelectionProc) lose,
                          (XtSelectionDoneProc) notify, cancel, closure, TRUE);
    UNLOCK_APP(app);
    return retval;
}

void
XtDisownSelection(Widget widget, xcb_atom_t selection, xcb_timestamp_t time)
{
    Select ctx;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    ctx = FindCtx(XtDisplay(widget), selection);
    if (LoseSelection(ctx, widget, selection, time))
        xcb_set_selection_owner(XtDisplay(widget), XCB_NONE, selection, time);
        //XSetSelectionOwner(XtDisplay(widget), selection, None, time);
    UNLOCK_APP(app);
}

/* Selection Requestor code */

static Boolean
IsINCRtype(CallBackInfo info, xcb_window_t window, xcb_atom_t prop)
{
    //unsigned char *value;
    xcb_atom_t type;

    if (prop == None)
        return False;

    xcb_get_property_cookie_t cookie = xcb_get_property(
        XtDisplay(info->widget),
        0,  // delete
        window,
        prop,
        info->type,
        0,  // offset
        0   // length
    );

    xcb_get_property_reply_t *reply = xcb_get_property_reply(XtDisplay(info->widget), cookie, NULL);

    if (!reply) {
        return False;
    }

    type = reply->type;
    free(reply);

    return (type == info->ctx->prop_list->incr_atom);
}

static void
ReqCleanup(Widget widget,
           XtPointer closure,
           xcb_generic_event_t *event,
           Boolean *cont _X_UNUSED)
{
    CallBackInfo info = (CallBackInfo) closure;
    unsigned long length;
    //int format;
    //xcb_atom_t target;

    if (event->response_type == XCB_SELECTION_REQUEST) {
        xcb_selection_request_event_t *ev = (xcb_selection_request_event_t *) event;

        if (!MATCH_SELECT(ev, info))
            return;             /* not really for us */
        XtRemoveEventHandler(widget, (EventMask) 0, TRUE,
                             ReqCleanup, (XtPointer) info);
        if (IsINCRtype(info, XtWindow(widget), ev->property)) {
            info->proc = HandleGetIncrement;
            XtAddEventHandler(info->widget, (EventMask) PropertyChangeMask,
                              FALSE, ReqCleanup, (XtPointer) info);
        }
        else {
            if (ev->property != None)
                xcb_delete_property(XtDisplay(widget), XtWindow(widget),
                                ev->property);
            FreeSelectionProperty(XtDisplay(widget), info->property);
            FreeInfo(info);
        }
    }
    else if ((event->response_type == XCB_SELECTION_NOTIFY)) {
        xcb_property_notify_event_t *ev = (xcb_property_notify_event_t *) event;
        if ((ev->state == PropertyNewValue) &&
             (ev->atom == info->property)) {
                char *value = NULL;
                xcb_get_property_cookie_t cookie = xcb_get_property(
                    XtDisplay(widget), 
                    0,
                    XtWindow(widget), 
                    ev->atom, 
                    XCB_ATOM_ATOM,
                    0L, 
                    1000000
                );

                xcb_get_property_reply_t *reply = xcb_get_property_reply(XtDisplay(widget), cookie, NULL);

                if (reply) {
                    value = xcb_get_property_value(reply);
                    length = reply->value_len;
                    free(reply);
                    free(value);
                }
                
            if (length == 0) {
                XtRemoveEventHandler(widget, (EventMask) PropertyChangeMask,
                                     FALSE, ReqCleanup, (XtPointer) info);
                FreeSelectionProperty(XtDisplay(widget), info->property);
                XtFree(info->value);    /* requestor never got this, so free now */
                FreeInfo(info);
            }
        }
    }
}

static void
ReqTimedOut(XtPointer closure, XtIntervalId *id _X_UNUSED)
{
    XtPointer value = NULL;
    unsigned long length = 0;
    int format = 8;
    xcb_atom_t resulttype = XT_CONVERT_FAIL;
    CallBackInfo info = (CallBackInfo) closure;
    unsigned long proplength;

    if (*info->target == info->ctx->prop_list->indirect_atom) {
        IndirectPair *pairs = NULL;

        xcb_get_property_cookie_t cookie = xcb_get_property(
            XtDisplay(info->widget), 0, XtWindow(info->widget), info->property, XCB_ATOM_ATOM, 0, 10000000);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(XtDisplay(info->widget), cookie, NULL);

        if (reply) {
            format = reply->format;
            proplength = reply->value_len;
            //pairs = (XprotoAtomPair *)xcb_get_property_value(reply);
            free(reply);
        
            XtPointer *c;
            int i;

            //XtFree(pairs);
            for (proplength = proplength / IndirectPairWordSize, i = 0,
                 c = info->req_closure; proplength; proplength--, c++, i++)
                (*info->callbacks[i]) (info->widget, *c, &info->ctx->selection,
                                       &resulttype, value, &length, &format);
        }
    }
    else {
        (*info->callbacks[0]) (info->widget, *info->req_closure,
                               &info->ctx->selection, &resulttype, value,
                               &length, &format);
    }

    /* change event handlers for straggler events */
    if (info->proc == HandleSelectionReplies) {
        XtRemoveEventHandler(info->widget, (EventMask) 0,
                             TRUE, info->proc, (XtPointer) info);
        XtAddEventHandler(info->widget, (EventMask) 0, TRUE,
                          ReqCleanup, (XtPointer) info);
    }
    else {
        XtRemoveEventHandler(info->widget, (EventMask) PropertyChangeMask,
                             FALSE, info->proc, (XtPointer) info);
        XtAddEventHandler(info->widget, (EventMask) PropertyChangeMask,
                          FALSE, ReqCleanup, (XtPointer) info);
    }

}

static void
HandleGetIncrement(Widget widget,
                   XtPointer closure,
                   xcb_generic_event_t *ev,
                   Boolean *cont _X_UNUSED)
{
    xcb_property_notify_event_t *event = (xcb_property_notify_event_t *) ev;
    CallBackInfo info = (CallBackInfo) closure;
    Select ctx = info->ctx;
    char *value;
    unsigned long length;
    int n = info->current;

    if ((event->state != PropertyNewValue) || (event->atom != info->property))
        return;

    xcb_get_property_cookie_t cookie = xcb_get_property(
        XtDisplay(widget), 0, XtWindow(widget), event->atom, XCB_ATOM_ATOM, 0, 10000000);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(XtDisplay(widget), cookie, NULL);
        
    if (reply) {
        info->type = reply->type;
        info->format = reply->format;
        length = reply->value_len;
        value = (unsigned char *)xcb_get_property_value(reply);
    } else {
        return;
    }
#ifndef DEBUG_WO_TIMERS
    XtRemoveTimeOut(info->timeout);
#endif
    if (length == 0) {
        unsigned long u_offset = NUMELEM2(info->offset, info->format);

        (*info->callbacks[n]) (widget, *info->req_closure, &ctx->selection,
                               &info->type,
                               (info->offset == 0 ? value : info->value),
                               &u_offset, &info->format);
        /* assert ((info->offset != 0) == (info->incremental[n]) */
        if (info->offset != 0)
            XtFree(value);
        XtRemoveEventHandler(widget, (EventMask) PropertyChangeMask, FALSE,
                             HandleGetIncrement, (XtPointer) info);
        FreeSelectionProperty(XtDisplay(widget), info->property);

        FreeInfo(info);
    }
    else {                      /* add increment to collection */
        if (info->incremental[n]) {
#ifdef XT_COPY_SELECTION
            int size = (int) BYTELENGTH(length, info->format) + 1;
            char *tmp = __XtMalloc((Cardinal) size);

            (void) memcpy(tmp, value, (size_t) size);
            XtFree(value);
            value = tmp;
#endif
            (*info->callbacks[n]) (widget, *info->req_closure, &ctx->selection,
                                   &info->type, value, &length, &info->format);
        }
        else {
            int size = (int) BYTELENGTH(length, info->format);

            if (info->offset + size > info->bytelength) {
                /* allocate enough for this and the next increment */
                info->bytelength = info->offset + size * 2;
                info->value = XtRealloc(info->value,
                                        (Cardinal) info->bytelength);
            }
            (void) memcpy(&info->value[info->offset], value, (size_t) size);
            info->offset += size;
            XtFree(value);
        }
        /* reset timer */
#ifndef DEBUG_WO_TIMERS
        {
            XtAppContext app = XtWidgetToApplicationContext(info->widget);

            info->timeout = XtAppAddTimeOut(app,
                                            app->selectionTimeout, ReqTimedOut,
                                            (XtPointer) info);
        }
#endif
    }
}

static void
HandleNone(Widget widget,
           XtSelectionCallbackProc callback,
           XtPointer closure,
           xcb_atom_t selection)
{
    unsigned long length = 0;
    int format = 8;
    xcb_atom_t type = None;

    (*callback) (widget, closure, &selection, &type, NULL, &length, &format);
}

static unsigned long
IncrPropSize(Widget widget,
             unsigned char *value,
             int format,
             unsigned long length)
{
    if (format == 32) {
        unsigned long size;

        size = ((unsigned long *) value)[length - 1];   /* %%% what order for longs? */
        return size;
    }
    else {
        XtAppWarningMsg(XtWidgetToApplicationContext(widget),
                        "badFormat", "xtGetSelectionValue", XtCXtToolkitError,
                        "Selection owner returned type INCR property with format != 32",
                        NULL, NULL);
        return 0;
    }
}

static
    Boolean
HandleNormal(xcb_connection_t *dpy,
             Widget widget,
             xcb_atom_t property,
             CallBackInfo info,
             XtPointer closure,
             xcb_atom_t selection)
{
    unsigned long length;
    int format;
    xcb_atom_t type;
    unsigned char *value = NULL;
    int number = info->current;

    xcb_get_property_cookie_t cookie = xcb_get_property(dpy, 0, XtWindow(widget), property, XCB_ATOM_ATOM, 0, 10000000);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(dpy, cookie, NULL);

    if (reply == NULL) {
        return FALSE;
    }

    if (type == info->ctx->prop_list->incr_atom) {
        unsigned long size = IncrPropSize(widget, value, format, length);

        XFree((char *) value);
        if (info->property != property) {
            /* within MULTIPLE */
            CallBackInfo ninfo;

            ninfo = MakeInfo(info->ctx, &info->callbacks[number],
                             &info->req_closure[number], 1, widget,
                             info->time, &info->incremental[number], &property);
            ninfo->target = (xcb_atom_t *) __XtMalloc((unsigned) sizeof(xcb_atom_t));
            *ninfo->target = info->target[number + 1];
            info = ninfo;
        }
        HandleIncremental(dpy, widget, property, info, size);
        return FALSE;
    }

    xcb_delete_property(dpy, XtWindow(widget), property);
#ifdef XT_COPY_SELECTION
    if (value) {                /* it could have been deleted after the SelectionNotify */
        int size = (int) BYTELENGTH(length, info->format) + 1;
        char *tmp = __XtMalloc((Cardinal) size);

        (void) memcpy(tmp, value, (size_t) size);
        XFree(value);
        value = (unsigned char *) tmp;
    }
#endif
    (*info->callbacks[number]) (widget, closure, &selection,
                                &type, (XtPointer) value, &length, &format);

    if (info->incremental[number]) {
        /* let requestor know the whole thing has been received */
        value = (unsigned char *) __XtMalloc((unsigned) 1);
        length = 0;
        (*info->callbacks[number]) (widget, closure, &selection,
                                    &type, (XtPointer) value, &length, &format);
    }
    return TRUE;
}

static void
HandleIncremental(xcb_connection_t *dpy,
                  Widget widget,
                  xcb_atom_t property,
                  CallBackInfo info,
                  unsigned long size)
{
    XtAddEventHandler(widget, (EventMask) PropertyChangeMask, FALSE,
                      HandleGetIncrement, (XtPointer) info);

    /* now start the transfer */
    xcb_delete_property(dpy, XtWindow(widget), property);
    xcb_flush(dpy);
    
    info->bytelength = (int) size;
    if (info->incremental[info->current])       /* requestor wants incremental too */
        info->value = NULL;     /* so no need for buffer to assemble value */
    else
        info->value = (char *) __XtMalloc((unsigned) info->bytelength);
    info->offset = 0;

    /* reset the timer */
    info->proc = HandleGetIncrement;
#ifndef DEBUG_WO_TIMERS
    {
        XtAppContext app = XtWidgetToApplicationContext(info->widget);

        info->timeout = XtAppAddTimeOut(app,
                                        app->selectionTimeout, ReqTimedOut,
                                        (XtPointer) info);
    }
#endif
}

static void
HandleSelectionReplies(Widget widget,
                       XtPointer closure,
                       xcb_generic_event_t *ev,
                       Boolean *cont _X_UNUSED) {
    xcb_selection_notify_event_t *event = (xcb_selection_notify_event_t *) ev;
    xcb_connection_t *dpy = XtDisplay(widget);
    CallBackInfo info = (CallBackInfo) closure;
    Select ctx = info->ctx;
    unsigned long length;
    int format;
    xcb_atom_t type;

    if (event->response_type != SelectionNotify)
        return;
    if (!MATCH_SELECT(event, info))
        return;                 /* not really for us */
#ifndef DEBUG_WO_TIMERS
    XtRemoveTimeOut(info->timeout);
#endif
    XtRemoveEventHandler(widget, (EventMask) 0, TRUE,
                         HandleSelectionReplies, (XtPointer) info);
    if (event->target == ctx->prop_list->indirect_atom) {
        IndirectPair *pairs = NULL, *p;
        XtPointer *c;

        xcb_get_property_cookie_t cookie = xcb_get_property(
        dpy, 1, XtWindow(widget), info->property, XCB_ATOM_ATOM, 0, 10000000);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(dpy, cookie, NULL);

        if (reply == NULL) {
            length = 0;
        for (length = length / IndirectPairWordSize, p = pairs,
             c = info->req_closure;
             length; length--, p++, c++, info->current++) {
                if (event->property == None || format != 32 || p->target == None
                    || /* bug compatibility */ p->property == None) {
                    HandleNone(widget, info->callbacks[info->current],
                               *c, event->selection);
                    if (p->property != None)
                        FreeSelectionProperty(XtDisplay(widget), p->property);
                }
                else {
                    if (HandleNormal(dpy, widget, p->property, info, *c,
                                     event->selection)) {
                        FreeSelectionProperty(XtDisplay(widget), p->property);
                    }
                }
            }
        }
        XFree((char *) pairs);
        FreeSelectionProperty(dpy, info->property);
        FreeInfo(info);
    }
    else if (event->property == None) {
        HandleNone(widget, info->callbacks[0], *info->req_closure,
                   event->selection);
        FreeSelectionProperty(XtDisplay(widget), info->property);
        FreeInfo(info);
    }
    else {
        if (HandleNormal(dpy, widget, event->property, info,
                         *info->req_closure, event->selection)) {
            FreeSelectionProperty(XtDisplay(widget), info->property);
            FreeInfo(info);
        }
    }
}

static void
DoLocalTransfer(Request req,
                xcb_atom_t selection,
                xcb_atom_t target,
                Widget widget, /* The widget requesting the value. */
                XtSelectionCallbackProc callback,
                XtPointer closure,    /* the closure for the callback, not the conversion */
                Boolean incremental, xcb_atom_t property)
{
    Select ctx = req->ctx;
    XtPointer value = NULL, temp, total = NULL;
    unsigned long length;
    int format;
    xcb_atom_t resulttype;
    unsigned long totallength = 0;

    req->event.response_type = 0;
    req->event.target = target;
    req->event.property = req->property = property;
    req->event.requestor = req->requestor = XtWindow(widget);

    if (ctx->incremental) {
        unsigned long size = (unsigned long) MAX_SELECTION_INCR(ctx->dpy);

        if (!(*(XtConvertSelectionIncrProc) ctx->convert)
            (ctx->widget, &selection, &target,
             &resulttype, &value, &length, &format,
             &size, ctx->owner_closure, (XtRequestId *) &req)) {
            HandleNone(widget, callback, closure, selection);
        }
        else {
            if (incremental) {
                Boolean allSent = FALSE;

                while (!allSent) {
                    if (ctx->notify && (value != NULL)) {
                        int bytelength = (int) BYTELENGTH(length, format);

                        /* both sides think they own this storage */
                        temp = __XtMalloc((unsigned) bytelength);
                        (void) memcpy(temp, value, (size_t) bytelength);
                        value = temp;
                    }
                    /* use care; older clients were never warned that
                     * they must return a value even if length==0
                     */
                    if (value == NULL)
                        value = __XtMalloc((unsigned) 1);
                    (*callback) (widget, closure, &selection,
                                 &resulttype, value, &length, &format);
                    if (length) {
                        /* should owner be notified on end-of-piece?
                         * Spec is unclear, but non-local transfers don't.
                         */
                        (*(XtConvertSelectionIncrProc) ctx->convert)
                            (ctx->widget, &selection, &target,
                             &resulttype, &value, &length, &format,
                             &size, ctx->owner_closure, (XtRequestId *) &req);
                    }
                    else
                        allSent = TRUE;
                }
            }
            else {
                while (length) {
                    int bytelength = (int) BYTELENGTH(length, format);

                    total = XtRealloc(total,
                                      (Cardinal) (totallength =
                                                  totallength +
                                                  (unsigned long) bytelength));
                    (void) memcpy((char *) total + totallength - bytelength,
                                   value, (size_t) bytelength);
                    (*(XtConvertSelectionIncrProc) ctx->convert)
                        (ctx->widget, &selection, &target,
                         &resulttype, &value, &length, &format,
                         &size, ctx->owner_closure, (XtRequestId *) &req);
                }
                if (total == NULL)
                    total = __XtMalloc(1);
                totallength = NUMELEM2(totallength, format);
                (*callback) (widget, closure, &selection, &resulttype,
                             total, &totallength, &format);
            }
            if (ctx->notify)
                (*(XtSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &selection, &target,
                     (XtRequestId *) &req, ctx->owner_closure);
            else
                XtFree((char *) value);
        }
    }
    else {                      /* not incremental owner */
        if (!(*ctx->convert) (ctx->widget, &selection, &target,
                              &resulttype, &value, &length, &format)) {
            HandleNone(widget, callback, closure, selection);
        }
        else {
            if (ctx->notify && (value != NULL)) {
                int bytelength = (int) BYTELENGTH(length, format);

                /* both sides think they own this storage; better copy */
                temp = __XtMalloc((unsigned) bytelength);
                (void) memcpy(temp, value, (size_t) bytelength);
                value = temp;
            }
            if (value == NULL)
                value = __XtMalloc((unsigned) 1);
            (*callback) (widget, closure, &selection, &resulttype,
                         value, &length, &format);
            if (ctx->notify)
                (*ctx->notify) (ctx->widget, &selection, &target);
        }
    }
}

static void
GetSelectionValue(Widget widget,
                  xcb_atom_t selection,
                  xcb_atom_t target,
                  XtSelectionCallbackProc callback,
                  XtPointer closure,
                  xcb_timestamp_t time,
                  Boolean incremental,
                  xcb_atom_t property)
{
    Select ctx;
    xcb_atom_t properties[1];

    properties[0] = property;

    ctx = FindCtx(XtDisplay(widget), selection);
    if (ctx->widget && !ctx->was_disowned) {
        RequestRec req;

        ctx->req = &req;
        memset(&req, 0, sizeof(req));
        req.ctx = ctx;
        req.event.time = time;
        ctx->ref_count++;
        DoLocalTransfer(&req, selection, target, widget,
                        callback, closure, incremental, property);
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            XtFree((char *) ctx);
        else
            ctx->req = NULL;
    }
    else {
        CallBackInfo info;

        info = MakeInfo(ctx, &callback, &closure, 1, widget,
                        time, &incremental, properties);
        info->target = (xcb_atom_t *) __XtMalloc((unsigned) sizeof(xcb_atom_t));
        *(info->target) = target;
        RequestSelectionValue(info, selection, target);
    }
}

void
XtGetSelectionValue(Widget widget,
                    xcb_atom_t selection,
                    xcb_atom_t target,
                    XtSelectionCallbackProc callback,
                    XtPointer closure,
                    xcb_timestamp_t time)
{
    xcb_atom_t property;
    Boolean incr = False;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    property = GetParamInfo(widget, selection);
    RemoveParamInfo(widget, selection);

    if (IsGatheringRequest(widget, selection)) {
        AddSelectionRequests(widget, selection, 1, &target, &callback, 1,
                             &closure, &incr, &property);
    }
    else {
        GetSelectionValue(widget, selection, target, callback,
                          closure, time, FALSE, property);
    }
    UNLOCK_APP(app);
}

void
XtGetSelectionValueIncremental(Widget widget,
                               xcb_atom_t selection,
                               xcb_atom_t target,
                               XtSelectionCallbackProc callback,
                               XtPointer closure,
                               xcb_timestamp_t time)
{
    xcb_atom_t property;
    Boolean incr = TRUE;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    property = GetParamInfo(widget, selection);
    RemoveParamInfo(widget, selection);

    if (IsGatheringRequest(widget, selection)) {
        AddSelectionRequests(widget, selection, 1, &target, &callback, 1,
                             &closure, &incr, &property);
    }
    else {
        GetSelectionValue(widget, selection, target, callback,
                          closure, time, TRUE, property);
    }

    UNLOCK_APP(app);
}

static void
GetSelectionValues(Widget widget,
                   xcb_atom_t selection,
                   xcb_atom_t *targets,
                   int count,
                   XtSelectionCallbackProc *callbacks,
                   int num_callbacks,
                   XtPointer *closures,
                   xcb_timestamp_t time,
                   Boolean *incremental,
                   xcb_atom_t *properties)
{
    Select ctx;
    IndirectPair *pairs;

    if (count == 0)
        return;
    ctx = FindCtx(XtDisplay(widget), selection);
    if (ctx->widget && !ctx->was_disowned) {
        int j, i;
        RequestRec req;

        ctx->req = &req;
        req.ctx = ctx;
        req.event.time = time;
        ctx->ref_count++;
        for (i = 0, j = 0; count > 0; count--, i++, j++) {
            if (j >= num_callbacks)
                j = 0;

            DoLocalTransfer(&req, selection, targets[i], widget,
                            callbacks[j], closures[i], incremental[i],
                            properties ? properties[i] : None);

        }
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            XtFree((char *) ctx);
        else
            ctx->req = NULL;
    }
    else {
        XtSelectionCallbackProc *passed_callbacks;
        XtSelectionCallbackProc stack_cbs[32];
        CallBackInfo info;
        IndirectPair *p;
        xcb_atom_t *t;
        int i = 0, j = 0;

        passed_callbacks = (XtSelectionCallbackProc *)
            XtStackAlloc(sizeof(XtSelectionCallbackProc) * (size_t) count,
                         stack_cbs);

        /* To deal with the old calls from XtGetSelectionValues* we
           will repeat however many callbacks have been passed into
           the array */
        for (i = 0; i < count; i++) {
            if (j >= num_callbacks)
                j = 0;
            passed_callbacks[i] = callbacks[j];
            j++;
        }
        info = MakeInfo(ctx, passed_callbacks, closures, count, widget,
                        time, incremental, properties);
        XtStackFree((XtPointer) passed_callbacks, stack_cbs);

        info->target = XtMallocArray ((Cardinal) count + 1,
                                      (Cardinal) sizeof(xcb_atom_t));
        (*info->target) = ctx->prop_list->indirect_atom;
        (void) memcpy((char *) info->target + sizeof(xcb_atom_t), targets,
                       (size_t) count * sizeof(xcb_atom_t));
        pairs = XtMallocArray ((Cardinal) count + 1,
                               (Cardinal) sizeof(IndirectPair));
        for (p = &pairs[count - 1], t = &targets[count - 1], i = count - 1;
             p >= pairs; p--, t--, i--) {
            p->target = *t;
            if (properties == NULL || properties[i] == None) {
                p->property = GetSelectionProperty(XtDisplay(widget));
                xcb_delete_property(XtDisplay(widget), XtWindow(widget),
                                p->property);
            }
            else {
                p->property = properties[i];
            }
        }
        xcb_change_property(XtDisplay(widget), XCB_PROP_MODE_REPLACE, XtWindow(widget), info->property,
                   info->property, 32, count * IndirectPairWordSize, pairs);
        XtFree((char *) pairs);
        RequestSelectionValue(info, selection, ctx->prop_list->indirect_atom);
    }
}

void
XtGetSelectionValues(Widget widget,
                     xcb_atom_t selection,
                     xcb_atom_t *targets,
                     int count,
                     XtSelectionCallbackProc callback,
                     XtPointer *closures,
                     xcb_timestamp_t time)
{
    Boolean incremental_values[32];
    Boolean *incremental;
    int i;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    incremental =
        XtStackAlloc((size_t) count * sizeof(Boolean), incremental_values);
    for (i = 0; i < count; i++)
        incremental[i] = FALSE;
    if (IsGatheringRequest(widget, selection)) {
        AddSelectionRequests(widget, selection, count, targets, &callback,
                             1, closures, incremental, NULL);
    }
    else {
        GetSelectionValues(widget, selection, targets, count, &callback, 1,
                           closures, time, incremental, NULL);
    }
    XtStackFree((XtPointer) incremental, incremental_values);
    UNLOCK_APP(app);
}

void
XtGetSelectionValuesIncremental(Widget widget,
                                xcb_atom_t selection,
                                xcb_atom_t *targets,
                                int count,
                                XtSelectionCallbackProc callback,
                                XtPointer *closures,
                                xcb_timestamp_t time)
{
    Boolean incremental_values[32];
    Boolean *incremental;
    int i;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    incremental =
        XtStackAlloc((size_t) count * sizeof(Boolean), incremental_values);
    for (i = 0; i < count; i++)
        incremental[i] = TRUE;
    if (IsGatheringRequest(widget, selection)) {
        AddSelectionRequests(widget, selection, count, targets, &callback,
                             1, closures, incremental, NULL);
    }
    else {
        GetSelectionValues(widget, selection, targets, count,
                           &callback, 1, closures, time, incremental, NULL);
    }
    XtStackFree((XtPointer) incremental, incremental_values);
    UNLOCK_APP(app);
}

static Request
GetRequestRecord(Widget widget, xcb_atom_t selection, XtRequestId id)
{
    Request req = (Request) id;
    Select ctx = NULL;

    if ((req == NULL
         && ((ctx = FindCtx(XtDisplay(widget), selection)) == NULL
             || ctx->req == NULL
             || ctx->selection != selection || ctx->widget == NULL))
        || (req != NULL
            && (req->ctx == NULL
                || req->ctx->selection != selection
                || req->ctx->widget != widget))) {
        String params = XtName(widget);
        Cardinal num_params = 1;

        XtAppWarningMsg(XtWidgetToApplicationContext(widget),
                        "notInConvertSelection", "xtGetSelectionRequest",
                        XtCXtToolkitError,
                        "XtGetSelectionRequest or XtGetSelectionParameters called for widget \"%s\" outside of ConvertSelection proc",
                        &params, &num_params);
        return NULL;
    }

    if (req == NULL) {
        /* non-incremental owner; only one request can be
         * outstanding at a time, so it's safe to keep ptr in ctx */
        req = ctx->req;
    }
    return req;
}

   
xcb_selection_request_event_t *
XtGetSelectionRequest(Widget widget, xcb_atom_t selection, XtRequestId id)
{
    Request req = (Request) id;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);

    req = GetRequestRecord(widget, selection, id);

    if (!req) {
        UNLOCK_APP(app);
        return (xcb_selection_request_event_t *) NULL;
    }

    if (req->type == 0) {
        /* owner is local; construct the remainder of the event */
        req->event.response_type = XCB_SELECTION_REQUEST;
        //req->event.serial = LastKnownRequestProcessed(XtDisplay(widget));
        //req->event.send_event = True;
        //req->event.display = XtDisplay(widget);

        req->event.owner = XtWindow(req->ctx->widget);
        req->event.selection = selection;
    }
    UNLOCK_APP(app);
    return &req->event;
}

/* Property atom access */
xcb_atom_t
XtReservePropertyxcb_atom_t(Widget w)
{
    return (GetSelectionProperty(XtDisplay(w)));
}

void
XtReleasePropertyxcb_atom_t(Widget w, xcb_atom_t atom)
{
    FreeSelectionProperty(XtDisplay(w), atom);
}

/* Multiple utilities */

/* All requests are put in a single list per widget.  It is
   very unlikely anyone will be gathering multiple MULTIPLE
   requests at the same time,  so the loss in efficiency for
   this case is acceptable */

/* Queue one or more requests to the one we're gathering */
void
AddSelectionRequests(Widget wid,
                     xcb_atom_t sel,
                     int count,
                     xcb_atom_t *targets,
                     XtSelectionCallbackProc *callbacks,
                     int num_cb,
                     XtPointer *closures,
                     Boolean *incrementals,
                     xcb_atom_t *properties)
{
    QueuedRequestInfo qi;
    xcb_window_t window = XtWindow(wid);
    xcb_connection_t *dpy = XtDisplay(wid);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = XtUniqueContext();

    qi = NULL;
    (void) XtFindContext(dpy, window, multipleContext, (void *) &qi);

    if (qi != NULL) {
        QueuedRequest *req = qi->requests;
        int start = qi->count;
        int i = 0;
        int j = 0;

        qi->count += count;
        req = XtReallocArray(req, (Cardinal) (start + count),
                             (Cardinal) sizeof(QueuedRequest));
        while (i < count) {
            QueuedRequest newreq = (QueuedRequest)
                __XtMalloc(sizeof(QueuedRequestRec));

            newreq->selection = sel;
            newreq->target = targets[i];
            if (properties != NULL)
                newreq->param = properties[i];
            else {
                newreq->param = GetSelectionProperty(dpy);
                xcb_delete_property(dpy, window, newreq->param);
            }
            newreq->callback = callbacks[j];
            newreq->closure = closures[i];
            newreq->incremental = incrementals[i];

            req[start] = newreq;
            start++;
            i++;
            j++;
            if (j > num_cb)
                j = 0;
        }

        qi->requests = req;
    }
    else {
        /* Impossible */
    }

    UNLOCK_PROCESS;
}

/* Only call IsGatheringRequest when we have a lock already */

static Boolean
IsGatheringRequest(Widget wid, xcb_atom_t sel)
{
    QueuedRequestInfo qi;
    xcb_window_t window = XtWindow(wid);
    xcb_connection_t *dpy = XtDisplay(wid);
    Boolean found = False;

    if (multipleContext == 0)
        multipleContext = XtUniqueContext();

    qi = NULL;
    (void) XtFindContext(dpy, window, multipleContext, (void *) &qi);

    if (qi != NULL) {
        int i = 0;

        while (qi->selections[i] != None) {
            if (qi->selections[i] == sel) {
                found = True;
                break;
            }
            i++;
        }
    }

    return (found);
}

/* Cleanup request scans the request queue and releases any
   properties queued, and removes any requests queued */
void
CleanupRequest(xcb_connection_t *dpy, QueuedRequestInfo qi, xcb_atom_t sel)
{
    int i, j, n;

    if (qi == NULL)
        return;

    i = 0;

    /* Remove this selection from the list */
    n = 0;
    while (qi->selections[n] != sel && qi->selections[n] != None)
        n++;
    if (qi->selections[n] == sel) {
        while (qi->selections[n] != None) {
            qi->selections[n] = qi->selections[n + 1];
            n++;
        }
    }

    while (i < qi->count) {
        QueuedRequest req = qi->requests[i];

        if (req->selection == sel) {
            /* Match */
            if (req->param != None)
                FreeSelectionProperty(dpy, req->param);
            qi->count--;

            for (j = i; j < qi->count; j++)
                qi->requests[j] = qi->requests[j + 1];

            XtFree((char *) req);
        }
        else {
            i++;
        }
    }
}

void
XtCreateSelectionRequest(Widget widget, xcb_atom_t selection)
{
    QueuedRequestInfo queueInfo;
    xcb_window_t window = XtWindow(widget);
    xcb_connection_t *dpy = XtDisplay(widget);
    Cardinal n;

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = XtUniqueContext();

    queueInfo = NULL;
    (void) XtFindContext(dpy, window, multipleContext, (void *) &queueInfo);

    /* If there is one,  then cancel it */
    if (queueInfo != NULL)
        CleanupRequest(dpy, queueInfo, selection);
    else {
        /* Create it */
        queueInfo =
            (QueuedRequestInfo) __XtMalloc(sizeof(QueuedRequestInfoRec));
        queueInfo->count = 0;
        queueInfo->selections = XtMallocArray(2, (Cardinal) sizeof(xcb_atom_t));
        queueInfo->selections[0] = None;
        queueInfo->requests = (QueuedRequest *)
            __XtMalloc(sizeof(QueuedRequest));
    }

    /* Append this selection to list */
    n = 0;
    while (queueInfo->selections[n] != None)
        n++;
    queueInfo->selections = XtReallocArray(queueInfo->selections, (n + 2),
                                           (Cardinal) sizeof(xcb_atom_t));
    queueInfo->selections[n] = selection;
    queueInfo->selections[n + 1] = None;

    (void) XtSaveContext(dpy, window, multipleContext, (char *) queueInfo);
    UNLOCK_PROCESS;
}

void
XtSendSelectionRequest(Widget widget, xcb_atom_t selection, xcb_timestamp_t time)
{
    QueuedRequestInfo queueInfo;
    xcb_window_t window = XtWindow(widget);
    xcb_connection_t *dpy = XtDisplay(widget);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = XtUniqueContext();

    queueInfo = NULL;
    (void) XtFindContext(dpy, window, multipleContext, (void *) &queueInfo);
    if (queueInfo != NULL) {
        int i;
        int count = 0;
        QueuedRequest *req = queueInfo->requests;

        /* Construct the requests and send it using
           GetSelectionValues */
        for (i = 0; i < queueInfo->count; i++)
            if (req[i]->selection == selection)
                count++;

        if (count > 0) {
            if (count == 1) {
                for (i = 0; i < queueInfo->count; i++)
                    if (req[i]->selection == selection)
                        break;

                /* special case a multiple which isn't needed */
                GetSelectionValue(widget, selection, req[i]->target,
                                  req[i]->callback, req[i]->closure, time,
                                  req[i]->incremental, req[i]->param);
            }
            else {
                xcb_atom_t *targets;
                xcb_atom_t t[PREALLOCED];
                XtSelectionCallbackProc *cbs;
                XtSelectionCallbackProc c[PREALLOCED];
                XtPointer *closures;
                XtPointer cs[PREALLOCED];
                Boolean *incrs;
                Boolean ins[PREALLOCED];
                xcb_atom_t *props;
                xcb_atom_t p[PREALLOCED];
                int j = 0;

                /* Allocate */
                targets =
                    (xcb_atom_t *) XtStackAlloc((size_t) count * sizeof(xcb_atom_t), t);
                cbs = (XtSelectionCallbackProc *)
                    XtStackAlloc((size_t) count *
                                 sizeof(XtSelectionCallbackProc), c);
                closures =
                    (XtPointer *) XtStackAlloc((size_t) count *
                                               sizeof(XtPointer), cs);
                incrs =
                    (Boolean *) XtStackAlloc((size_t) count * sizeof(Boolean),
                                             ins);
                props = (xcb_atom_t *) XtStackAlloc((size_t) count * sizeof(xcb_atom_t), p);

                /* Copy */
                for (i = 0; i < queueInfo->count; i++) {
                    if (req[i]->selection == selection) {
                        targets[j] = req[i]->target;
                        cbs[j] = req[i]->callback;
                        closures[j] = req[i]->closure;
                        incrs[j] = req[i]->incremental;
                        props[j] = req[i]->param;
                        j++;
                    }
                }

                /* Make the request */
                GetSelectionValues(widget, selection, targets, count,
                                   cbs, count, closures, time, incrs, props);

                /* Free */
                XtStackFree((XtPointer) targets, t);
                XtStackFree((XtPointer) cbs, c);
                XtStackFree((XtPointer) closures, cs);
                XtStackFree((XtPointer) incrs, ins);
                XtStackFree((XtPointer) props, p);
            }
        }
    }

    CleanupRequest(dpy, queueInfo, selection);
    UNLOCK_PROCESS;
}

void
XtCancelSelectionRequest(Widget widget, xcb_atom_t selection)
{
    QueuedRequestInfo queueInfo;
    xcb_window_t window = XtWindow(widget);
    xcb_connection_t *dpy = XtDisplay(widget);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = XtUniqueContext();

    queueInfo = NULL;
    (void) XtFindContext(dpy, window, multipleContext, (void *) &queueInfo);
    /* If there is one,  then cancel it */
    if (queueInfo != NULL)
        CleanupRequest(dpy, queueInfo, selection);
    UNLOCK_PROCESS;
}

/* Parameter utilities */

/* Parameters on a selection request */
/* Places data on allocated parameter atom,  then records the
   parameter atom data for use in the next call to one of
   the XtGetSelectionValue functions. */
void
XtSetSelectionParameters(Widget requestor,
                         xcb_atom_t selection,
                         xcb_atom_t type,
                         XtPointer value,
                         unsigned long length,
                         int format)
{
    xcb_connection_t *dpy = XtDisplay(requestor);
    xcb_window_t window = XtWindow(requestor);
    xcb_atom_t property = GetParamInfo(requestor, selection);

    if (property == None) {
        property = GetSelectionProperty(dpy);
        AddParamInfo(requestor, selection, property);
    }

    //XChangeProperty(dpy, window, property,
    //                type, format, PropModeReplace,
    //                (unsigned char *) value, (int) length);
    xcb_change_property(
        dpy, 
        XCB_PROP_MODE_REPLACE,  // mode
        window,                 // window
        property,               // property
        type,                   // type
        format,                 // format
        length,                 // data_len
        value                   // data
    );
}

/* Retrieves data passed in a parameter. Data for this is stored
   on the originator's window */
void
XtGetSelectionParameters(Widget owner,
                         xcb_atom_t selection,
                         XtRequestId request_id,
                         xcb_atom_t *type_return,
                         XtPointer *value_return,
                         unsigned long *length_return,
                         int *format_return)
{
    Request req;
    xcb_connection_t *dpy = XtDisplay(owner);

    WIDGET_TO_APPCON(owner);

    *value_return = NULL;
    *length_return = (unsigned long) (*format_return = 0);
    *type_return = None;

    LOCK_APP(app);

    req = GetRequestRecord(owner, selection, request_id);

    if (req && req->property) {
        //StartProtectedSection(dpy, req->requestor);
        xcb_get_property_cookie_t cookie = xcb_get_property(
            dpy, 
            0,                           // delete
            req->requestor,              // window
            req->property,               // property
            AnyPropertyType,             // type
            0L,                          // long_offset
            10000000                     // long_length
        );

        xcb_get_property_reply_t *reply = xcb_get_property_reply(dpy, cookie, NULL);

        if (reply) {
            *type_return = reply->type;
            *format_return = reply->format;
            *length_return = reply->value_len;
            
            if (reply->value_len > 0) {
                *value_return = malloc(reply->value_len);
                memcpy(*value_return, xcb_get_property_value(reply), reply->value_len);
            }
            
            free(reply);
        }
        //EndProtectedSection(dpy);
#ifdef XT_COPY_SELECTION
        if (*value_return) {
            int size = (int) BYTELENGTH(*length_return, *format_return) + 1;
            char *tmp = __XtMalloc((Cardinal) size);

            (void) memcpy(tmp, *value_return, (size_t) size);
            XFree(*value_return);
            *value_return = tmp;
        }
#endif
    }
    UNLOCK_APP(app);
}

/*  Parameters are temporarily stashed in an XContext.  A list is used because
 *  there may be more than one selection request in progress.  The context
 *  data is deleted when the list is empty.  In the future, the parameter
 *  context could be merged with other contexts used during selections.
 */

void
AddParamInfo(Widget w, xcb_atom_t selection, xcb_atom_t param_atom)
{
    Param p;
    ParamInfo pinfo;

    LOCK_PROCESS;
    if (paramPropertyContext == 0)
        paramPropertyContext = XtUniqueContext();

    if (XtFindContext(XtDisplay(w), XtWindow(w), paramPropertyContext,
                     (void *) &pinfo)) {
        pinfo = (ParamInfo) __XtMalloc(sizeof(ParamInfoRec));
        pinfo->count = 1;
        pinfo->paramlist = XtNew(ParamRec);
        p = pinfo->paramlist;
        (void) XtSaveContext(XtDisplay(w), XtWindow(w), paramPropertyContext,
                            (char *) pinfo);
    }
    else {
        int n;

        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++) {
            if (p->selection == None || p->selection == selection)
                break;
        }
        if (n == 0) {
            pinfo->count++;
            pinfo->paramlist = XtReallocArray(pinfo->paramlist, pinfo->count,
                                              (Cardinal) sizeof(ParamRec));
            p = &pinfo->paramlist[pinfo->count - 1];
            (void) XtSaveContext(XtDisplay(w), XtWindow(w),
                                paramPropertyContext, (char *) pinfo);
        }
    }
    p->selection = selection;
    p->param = param_atom;
    UNLOCK_PROCESS;
}

void
RemoveParamInfo(Widget w, xcb_atom_t selection)
{
    ParamInfo pinfo;
    Boolean retain = False;

    LOCK_PROCESS;
    if (paramPropertyContext
        && (XtFindContext(XtDisplay(w), XtWindow(w), paramPropertyContext,
                         (void *) &pinfo) == 0)) {
        Param p;
        int n;

        /* Find and invalidate the parameter data. */
        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++) {
            if (p->selection != None) {
                if (p->selection == selection)
                    p->selection = None;
                else
                    retain = True;
            }
        }
        /* If there's no valid data remaining, release the context entry. */
        if (!retain) {
            XtFree((char *) pinfo->paramlist);
            XtFree((char *) pinfo);
            XtDeleteContext(XtDisplay(w), XtWindow(w), paramPropertyContext);
        }
    }
    UNLOCK_PROCESS;
}

xcb_atom_t
GetParamInfo(Widget w, xcb_atom_t selection)
{
    ParamInfo pinfo;
    xcb_atom_t atom = None;

    LOCK_PROCESS;
    if (paramPropertyContext
        && (XtFindContext(XtDisplay(w), XtWindow(w), paramPropertyContext,
                         (void *) &pinfo) == 0)) {
        Param p;
        int n;

        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++)
            if (p->selection == selection) {
                atom = p->param;
                break;
            }
    }
    UNLOCK_PROCESS;
    return atom;
}
