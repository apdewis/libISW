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
#include "ISWContextI.h"
#include <stdio.h>
#include <ISW/ISWPlatform.h>

/* General utilities */

static void HandleSelectionReplies(Widget, IswPointer,IswEvent *, Boolean *);
static void ReqTimedOut(IswPointer, IswIntervalId *);
static void HandlePropertyGone(Widget, IswPointer,IswEvent *, Boolean *);
static void HandleGetIncrement(Widget, IswPointer,IswEvent *, Boolean *);
static void HandleIncremental(IswDisplay, Widget, IswSelectionId, CallBackInfo,
                              unsigned long);

static XContext selectPropertyContext = 0;
static XContext paramPropertyContext = 0;
static XContext multipleContext = 0;

/* Multiple utilities */
static void AddSelectionRequests(Widget, IswSelectionId, int, IswSelectionId *,
                                 IswSelectionCallbackProc *, int, IswPointer *,
                                 Boolean *, IswSelectionId *);
static Boolean IsGatheringRequest(Widget, IswSelectionId);

#define PREALLOCED 32

/* Parameter utilities */
static void AddParamInfo(Widget, IswSelectionId, IswSelectionId);
static void RemoveParamInfo(Widget, IswSelectionId);
static IswSelectionId GetParamInfo(Widget, IswSelectionId);

static int StorageSize[3] = { 1, sizeof(short), sizeof(long) };

#define BYTELENGTH(length, format) ((length) * (size_t)StorageSize[(format)>>4])
#define NUMELEM(bytelength, format) ((bytelength) / StorageSize[(format)>>4])
#define NUMELEM2(bytelength, format) ((unsigned long)(bytelength) / (unsigned long) StorageSize[(format)>>4])

/* Xlib and Xt are permitted to have different memory allocators, and in the
 * IswSelectionCallbackProc the client is instructed to free the selection
 * value with IswFree, so the selection value received from XGetWindowProperty
 * should be copied to memory allocated through Xt.  But copying is
 * undesirable since the selection value may be large, and, under normal
 * library configuration copying is unnecessary.
 */
#ifdef XTTRACEMEMORY
#define ISW_COPY_SELECTION       1
#endif

static void
FreePropList(Widget w _X_UNUSED,
             IswPointer closure,
             IswPointer callData _X_UNUSED)
{
    PropList sarray = (PropList) closure;

    LOCK_PROCESS;
    IswDeleteContext(sarray->dpy,
                   _IswPlatformWindowId(_IswDefaultRootWindow(sarray->dpy)),
                   selectPropertyContext);
    UNLOCK_PROCESS;
    IswFree((char *) sarray->list);
    IswFree((char *) closure);
}

static PropList
GetPropList(IswDisplay dpy)
{
    PropList sarray;

    LOCK_PROCESS;
    if (selectPropertyContext == 0)
        selectPropertyContext = IswUniqueContext();
    if (IswFindContext(dpy, _IswPlatformWindowId(_IswDefaultRootWindow(dpy)),
                     selectPropertyContext, (void *) &sarray)) {
        IswSelectionId ids[4];

        static const char *names[] = {
            "INCR",
            "MULTIPLE",
            "TIMESTAMP",
            "_XT_SELECTION_0"
        };

        IswPerDisplay pd = _IswGetPerDisplay(dpy);

        sarray = (PropList) __XtMalloc((unsigned) sizeof(PropListRec));
        sarray->dpy = dpy;
        for(uint8_t i = 0; i < 4; i++) {
            ids[i] = _IswPlatformSelectionInternName(dpy, names[i], False);
        }

        sarray->incr_id = ids[0];
        sarray->indirect_id = ids[1];
        sarray->timestamp_id = ids[2];
        sarray->id_list_type = _IswPlatformSelectionStdType(dpy, ISW_SEL_STDTYPE_ID_LIST);
        sarray->propCount = 1;
        sarray->list =
            (SelectionProp) __XtMalloc((unsigned) sizeof(SelectionPropRec));
        sarray->list[0].prop = ids[3];
        sarray->list[0].avail = TRUE;
        (void) IswSaveContext(dpy,
                            _IswPlatformWindowId(_IswDefaultRootWindow(dpy)),
                            selectPropertyContext, (char *) sarray);
        _IswAddCallback(&pd->destroy_callbacks,
                       FreePropList, (IswPointer) sarray);
    }
    UNLOCK_PROCESS;
    return sarray;
}

static IswSelectionId
GetSelectionProperty(IswDisplay dpy)
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
    sarray->list = IswReallocArray(sarray->list, (Cardinal) sarray->propCount,
                                  (Cardinal) sizeof(SelectionPropRec));
    (void) snprintf(propname, sizeof(propname), "_XT_SELECTION_%d", propCount);
    sarray->list[propCount].prop =
        _IswPlatformSelectionInternName(dpy, propname, False);
    sarray->list[propCount].avail = FALSE;
    return (sarray->list[propCount].prop);
}

static void
FreeSelectionProperty(IswDisplay dpy, IswSelectionId prop)
{
    SelectionProp p;
    int propCount;
    PropList sarray;

    if (prop == ISW_SELECTION_NONE)
        return;
    LOCK_PROCESS;
    if (IswFindContext(dpy, _IswPlatformWindowId(_IswDefaultRootWindow(dpy)),
                     selectPropertyContext, (void *) &sarray))
        IswAppErrorMsg(IswDisplayToApplicationContext(dpy),
                      "noSelectionProperties", "freeSelectionProperty",
                      IswCIswToolkitError,
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
    IswFree((char *) info->incremental);
    IswFree((char *) info->callbacks);
    IswFree((char *) info->req_closure);
    IswFree((char *) info->target);
    IswFree((char *) info);
}

static CallBackInfo
MakeInfo(Select ctx,
         IswSelectionCallbackProc *callbacks,
         IswPointer *closures,
         int count,
         Widget widget,
         IswTime time,
         Boolean *incremental,
         IswSelectionId *properties)
{
    CallBackInfo info = IswNew(CallBackInfoRec);

    info->ctx = ctx;
    info->callbacks = IswMallocArray((Cardinal) count,
                                    (Cardinal) sizeof(IswSelectionCallbackProc));
    (void) memcpy(info->callbacks, callbacks,
                  (size_t) count * sizeof(IswSelectionCallbackProc));
    info->req_closure = IswMallocArray((Cardinal) count,
                                      (Cardinal) sizeof(IswPointer));
    (void) memcpy(info->req_closure, closures,
                  (size_t) count * sizeof(IswPointer));
    if (count == 1 && properties != NULL && properties[0] != ISW_SELECTION_NONE)
        info->property = properties[0];
    else {
        info->property = GetSelectionProperty(IswDisplayOf(widget));
        _IswPlatformDeleteProperty(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), info->property);
    }
    info->proc = HandleSelectionReplies;
    info->widget = widget;
    info->time = time;
    info->incremental = IswMallocArray((Cardinal) count,
                                      (Cardinal) sizeof(Boolean));
    (void) memcpy(info->incremental, incremental,
                  (size_t) count * sizeof(Boolean));
    info->current = 0;
    info->value = NULL;
    return (info);
}

static void
RequestSelectionValue(CallBackInfo info, IswSelectionId selection,
                      IswSelectionId target)
{
#ifndef DEBUG_WO_TIMERS
    IswAppContext app = IswWidgetToApplicationContext(info->widget);

    info->timeout = IswAppAddTimeOut(app,
                                    app->selectionTimeout, ReqTimedOut,
                                    (IswPointer) info);
#endif
    IswAddEventHandler(info->widget, (EventMask) 0, TRUE,
                      HandleSelectionReplies, (IswPointer) info);

    _IswPlatformConvertSelection(
        info->ctx->dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(info->widget)), (Widget)(info->widget)),
        selection, target, info->property, info->time);
}

static XContext selectContext = 0;

static Select
NewContext(IswDisplay dpy, IswSelectionId selection)
{
    /* assert(selectContext != 0) */
    Select ctx = IswNew(SelectRec);

    ctx->dpy = dpy;
    ctx->selection = selection;
    ctx->widget = NULL;
    ctx->prop_list = GetPropList(dpy);
    ctx->ref_count = 0;
    ctx->free_when_done = FALSE;
    ctx->was_disowned = FALSE;
    LOCK_PROCESS;
    (void) IswSaveContext(dpy, (XID) selection, selectContext, (char *) ctx);
    UNLOCK_PROCESS;
    return ctx;
}

static Select
FindCtx(IswDisplay dpy, IswSelectionId selection)
{
    Select ctx;

    LOCK_PROCESS;
    if (selectContext == 0)
        selectContext = IswUniqueContext();
    if (IswFindContext(dpy, (XID) selection, selectContext, (void *) &ctx))
        ctx = NewContext(dpy, selection);
    UNLOCK_PROCESS;
    return ctx;
}

static void
WidgetDestroyed(Widget widget, IswPointer closure, IswPointer data _X_UNUSED)
{
    Select ctx = (Select) closure;

    if (ctx->widget == widget) {
        if (ctx->free_when_done)
            IswFree((char *) ctx);
        else
            ctx->widget = NULL;
    }
}

/* Selection Owner code */

static void HandleSelectionEvents(Widget, IswPointer,IswEvent *, Boolean *);

static Boolean
LoseSelection(Select ctx, Widget widget, IswSelectionId selection, IswTime time)
{
    if ((ctx->widget == widget) && (ctx->selection == selection) &&     /* paranoia */
        !ctx->was_disowned && ((time == CurrentTime) || (time >= ctx->time))) {
        IswRemoveEventHandler(widget, (EventMask) 0, TRUE,
                             HandleSelectionEvents, (IswPointer) ctx);
        IswRemoveCallback(widget, IswNdestroyCallback,
                         WidgetDestroyed, (IswPointer) ctx);
        ctx->was_disowned = TRUE;       /* widget officially loses ownership */
        /* now inform widget */
        if (ctx->loses) {
            if (ctx->incremental)
                (*(IswLoseSelectionIncrProc) ctx->loses)
                    (widget, &ctx->selection, ctx->owner_closure);
            else
                (*ctx->loses) (widget, &ctx->selection);
        }
        return (TRUE);
    }
    else
        return (FALSE);
}

static XContext selectWindowContext = 0;

/* %%% Xlib.h should make this public! */
//typedef int (*xErrorHandler) (xcb_connection_t *, XErrorEvent *);

//static xErrorHandler oldErrorHandler = NULL;

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
AddHandler(Request req, EventMask mask, IswEventHandler proc, IswPointer closure)
{
    IswDisplay dpy = req->ctx->dpy;
    IswWindow window = req->requestor;
    IswWindowId window_id = _IswPlatformWindowId(window);
    Widget widget = IswWindowToWidget(dpy, window);

    if (widget != NULL)
        req->widget = widget;
    else
        widget = req->widget;

    if (_IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget))) == window_id)
        IswAddEventHandler(widget, mask, False, proc, closure);
    else {
        RequestWindowRec *requestWindowRec;

        LOCK_PROCESS;
        if (selectWindowContext == 0)
            selectWindowContext = IswUniqueContext();
        if (IswFindContext(dpy, window_id, selectWindowContext,
                         (void *) &requestWindowRec)) {
            requestWindowRec = IswNew(RequestWindowRec);
            requestWindowRec->active_transfer_count = 0;
            (void) IswSaveContext(dpy, window_id, selectWindowContext,
                                (char *) requestWindowRec);
        }
        UNLOCK_PROCESS;
        if (requestWindowRec->active_transfer_count++ == 0) {
            IswRegisterDrawable(dpy, window, widget);
            IswWindowAttributes attrs = { 0 };
            attrs.event_mask = mask;
            _IswPlatformChangeAttributes(dpy, window,
                                         &attrs, ISW_ATTR_EVENT_MASK);
        }
        IswAddRawEventHandler(widget, mask, FALSE, proc, closure);
    }
}

static void
RemoveHandler(Request req,
              EventMask mask,
              IswEventHandler proc,
              IswPointer closure)
{
    IswDisplay dpy = req->ctx->dpy;
    IswWindow window = req->requestor;
    IswWindowId window_id = _IswPlatformWindowId(window);
    Widget widget = req->widget;

    if ((IswWindowToWidget(dpy, window) == widget) &&
        (_IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget))) != window_id)) {
        /* we had to hang this window onto our widget; take it off */
        RequestWindowRec *requestWindowRec;

        IswRemoveRawEventHandler(widget, mask, TRUE, proc, closure);
        LOCK_PROCESS;
        (void) IswFindContext(dpy, window_id, selectWindowContext,
                            (void *) &requestWindowRec);
        UNLOCK_PROCESS;
        if (--requestWindowRec->active_transfer_count == 0) {
            IswUnregisterDrawable(dpy, window);
            IswWindowAttributes attrs = { 0 };
            attrs.event_mask = 0;
            _IswPlatformChangeAttributes(dpy, window,
                                         &attrs, ISW_ATTR_EVENT_MASK);
            LOCK_PROCESS;
            (void) IswDeleteContext(dpy, window_id, selectWindowContext);
            UNLOCK_PROCESS;
            IswFree((char *) requestWindowRec);
        }
    }
    else {
        IswRemoveEventHandler(widget, mask, TRUE, proc, closure);
    }
}

static void
OwnerTimedOut(IswPointer closure, IswIntervalId *id _X_UNUSED)
{
    Request req = (Request) closure;
    Select ctx = req->ctx;

    if (ctx->incremental && (ctx->owner_cancel != NULL)) {
        (*ctx->owner_cancel) (ctx->widget, &ctx->selection,
                              &req->target, (IswRequestId *) &req,
                              ctx->owner_closure);
    }
    else {
        if (ctx->notify == NULL)
            IswFree((char *) req->value);
        else {
            /* the requestor hasn't deleted the property, but
             * the owner needs to free the value.
             */
            if (ctx->incremental)
                (*(IswSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &ctx->selection, &req->target,
                     (IswRequestId *) &req, ctx->owner_closure);
            else
                (*ctx->notify) (ctx->widget, &ctx->selection, &req->target);
        }
    }

    RemoveHandler(req, (EventMask) IswPropertyChangeMask,
                  HandlePropertyGone, closure);
    IswFree((char *) req);
    if (--ctx->ref_count == 0 && ctx->free_when_done)
        IswFree((char *) ctx);
}

static void
SendIncrement(Request incr)
{
    IswDisplay dpy = incr->ctx->dpy;

    unsigned long incrSize = (unsigned long) MAX_SELECTION_INCR(dpy);

    if (incrSize > incr->bytelength - incr->offset)
        incrSize = incr->bytelength - incr->offset;
    _IswPlatformChangeProperty(dpy, incr->requestor,
                    incr->property, incr->type, incr->format,
                    ISW_PROP_MODE_REPLACE,
                    (unsigned char *) incr->value + incr->offset,
                    (uint32_t) NUMELEM((int) incrSize, incr->format));
    _IswPlatformFlush(dpy);
    incr->offset += incrSize;
}

static void
AllSent(Request req)
{
    Select ctx = req->ctx;

    _IswPlatformChangeProperty(ctx->dpy, req->requestor,
                    req->property, req->type, req->format,
                    ISW_PROP_MODE_REPLACE, NULL, 0);
    _IswPlatformFlush(ctx->dpy);
    req->allSent = TRUE;

    if (ctx->notify == NULL)
        IswFree((char *) req->value);
}

static void
HandlePropertyGone(Widget widget _X_UNUSED,
                   IswPointer closure,
                  IswEvent *iswev,
                   Boolean *cont _X_UNUSED)
{
    Request req = (Request) closure;
    Select ctx = req->ctx;
    IswSelectionEvent selev;

    if (!_IswPlatformSelectionDecodeEvent(ctx->dpy, IswEventNative(iswev),
                                          &selev))
        return;
    if ((selev.kind != ISW_SEL_EVENT_PROP_DELETE) ||
        (selev.property != req->property) ||
        (_IswPlatformWindowId(selev.requestor) !=
         _IswPlatformWindowId(req->requestor)))
        return;
#ifndef DEBUG_WO_TIMERS
    IswRemoveTimeOut(req->timeout);
#endif
    if (req->allSent) {
        if (ctx->notify) {
            if (ctx->incremental) {
                (*(IswSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &ctx->selection, &req->target,
                     (IswRequestId *) &req, ctx->owner_closure);
            }
            else
                (*ctx->notify) (ctx->widget, &ctx->selection, &req->target);
        }
        RemoveHandler(req, (EventMask) IswPropertyChangeMask,
                      HandlePropertyGone, closure);
        IswFree((char *) req);
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            IswFree((char *) ctx);
    }
    else {                      /* is this part of an incremental transfer? */
        if (ctx->incremental) {
            if (req->bytelength == 0)
                AllSent(req);
            else {
                unsigned long size =
                    (unsigned long) MAX_SELECTION_INCR(ctx->dpy);
                SendIncrement(req);
                (*(IswConvertSelectionIncrProc) ctx->convert)
                    (ctx->widget, &ctx->selection, &req->target,
                     &req->type, &req->value,
                     &req->bytelength, &req->format,
                     &size, ctx->owner_closure, (IswPointer *) &req);
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
            IswAppContext app = IswWidgetToApplicationContext(req->widget);

            req->timeout = IswAppAddTimeOut(app,
                                           app->selectionTimeout, OwnerTimedOut,
                                           (IswPointer) req);
        }
#endif
    }
}

static void
PrepareIncremental(Request req,
                   Widget widget,
                   IswWindow window,
                   IswSelectionId property _X_UNUSED,
                   IswSelectionId target,
                   IswSelectionId targetType,
                   IswPointer value,
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
        IswAppContext app = IswWidgetToApplicationContext(widget);

        req->timeout = IswAppAddTimeOut(app,
                                       app->selectionTimeout, OwnerTimedOut,
                                       (IswPointer) req);
    }
#endif
    AddHandler(req, (EventMask) IswPropertyChangeMask,
               HandlePropertyGone, (IswPointer) req);
/* now send client INCR property */
    _IswPlatformChangeProperty(req->ctx->dpy, window,
                   req->property, req->ctx->prop_list->incr_id, 32,
                   ISW_PROP_MODE_REPLACE, &req->bytelength, 1);
}

static Boolean
GetConversion(Select ctx,       /* logical owner */
              const IswSelectionRequest *request,
              IswSelectionId target,
              IswSelectionId property,  /* requestor's property */
              Widget widget)    /* physical owner (receives events) */
{
    IswPointer value = NULL;
    unsigned long length;
    int format;
    IswSelectionId targetType;
    IswSelectionId selection = request->selection;
    IswWindow requestor = request->requestor;
    Request req = IswNew(RequestRec);
    Boolean timestamp_target = (target == ctx->prop_list->timestamp_id);

    req->ctx = ctx;
    req->request = *request;
    req->property = property;
    req->requestor = requestor;

    if (timestamp_target) {
        value = __XtMalloc(sizeof(long));
        *(long *) value = (long) ctx->time;
        targetType = _IswPlatformSelectionInternName(ctx->dpy, "INTEGER", False);
        length = 1;
        format = 32;
    }
    else {
        ctx->ref_count++;
        if (ctx->incremental == TRUE) {
            unsigned long size = (unsigned long) MAX_SELECTION_INCR(ctx->dpy);

            if ((*(IswConvertSelectionIncrProc) ctx->convert)
                (ctx->widget, &selection, &target,
                 &targetType, &value, &length, &format,
                 &size, ctx->owner_closure, (IswRequestId *) &req)
                == FALSE) {
                IswFree((char *) req);
                ctx->ref_count--;
                return (FALSE);
            }
            PrepareIncremental(req, widget, requestor, property,
                               target, targetType, value, length, format);
            return (TRUE);
        }
        ctx->req = req;
        if ((*ctx->convert) (ctx->widget, &selection, &target,
                             &targetType, &value, &length, &format) == FALSE) {
            IswFree((char *) req);
            ctx->req = NULL;
            ctx->ref_count--;
            return (FALSE);
        }
        ctx->req = NULL;
    }
    if (BYTELENGTH(length, format) <=
        (unsigned long) MAX_SELECTION_INCR(ctx->dpy)) {
        if (!timestamp_target) {
            if (ctx->notify != NULL) {
                req->target = target;
                req->widget = widget;
                req->allSent = TRUE;
#ifndef DEBUG_WO_TIMERS
                {
                    IswAppContext app =
                        IswWidgetToApplicationContext(req->widget);
                    req->timeout =
                        IswAppAddTimeOut(app, app->selectionTimeout,
                                        OwnerTimedOut, (IswPointer) req);
                }
#endif
                AddHandler(req, (EventMask) IswPropertyChangeMask,
                           HandlePropertyGone, (IswPointer) req);
            }
            else
                ctx->ref_count--;
        }
        _IswPlatformChangeProperty(ctx->dpy, requestor, property,
                   targetType, format, ISW_PROP_MODE_REPLACE,
                   value, (uint32_t) length);
        /* free storage for client if no notify proc */
        if (timestamp_target || ctx->notify == NULL) {
            IswFree((char *) value);
            IswFree((char *) req);
        }
    }
    else {
        PrepareIncremental(req, widget, requestor, property,
                           target, targetType, value, length, format);
    }
    return (TRUE);
}

static void
HandleSelectionEvents(Widget widget,
                      IswPointer closure,
                      IswEvent *iswev,
                      Boolean *cont _X_UNUSED)
{
    Select ctx = (Select) closure;
    IswSelectionEvent selev;
    IswSelectionRequest request;
    IswSelectionId notify_property;

    if (!_IswPlatformSelectionDecodeEvent(ctx->dpy, IswEventNative(iswev),
                                          &selev))
        return;

    switch (selev.kind) {
    case ISW_SEL_EVENT_CLEAR:
        /* if this event is not for the selection we registered for,
         * don't do anything */
        if (ctx->selection != selev.selection ||
            ctx->serial > selev.serial)
            break;
        (void) LoseSelection(ctx, widget, selev.selection, selev.time);
        break;
    case ISW_SEL_EVENT_REQUEST:
        /* if this event is not for the selection we registered for,
         * don't do anything */
        if (ctx->selection != selev.selection)
            break;
        request = selev.request;
        if (request.property == ISW_SELECTION_NONE)  /* obsolete requestor */
            request.property = request.target;
        if (ctx->widget != widget || ctx->was_disowned
            || ((request.time != ISW_CURRENT_TIME)
                && (request.time < ctx->time))) {
            notify_property = ISW_SELECTION_NONE;
        }
        else if (request.target == ctx->prop_list->indirect_id) {
            IndirectPair *p;
            int format = 0;
            unsigned long length = 0;
            unsigned char *value = NULL;
            int count;
            Boolean writeback = FALSE;

            notify_property = request.property;
            IswProperty prop;
            if (_IswPlatformGetProperty(ctx->dpy, request.requestor,
                    request.property, ctx->prop_list->id_list_type, 0, 1000000, &prop)) {
                count = (int) (BYTELENGTH(prop.num_items, prop.type) / sizeof(IndirectPair));
            } else {
                count = 0;
            }
            for (p = (IndirectPair *) value; count; p++, count--) {
                if (!GetConversion(ctx, &request, p->target, p->property,
                                   widget)) {
                    p->target = ISW_SELECTION_NONE;
                    writeback = TRUE;
                }
            }
            if (writeback)
                _IswPlatformChangeProperty(ctx->dpy, request.requestor,
                    request.property, ISW_SELECTION_NONE,
                    format, ISW_PROP_MODE_REPLACE, value, (uint32_t) length);
            _IswPlatformFreeProperty(&prop);
            IswFree((char *) value);
        }
        else {              /* not multiple */
            if (GetConversion(ctx, &request, request.target,
                              request.property, widget))
                notify_property = request.property;
            else
                notify_property = ISW_SELECTION_NONE;
        }
        _IswPlatformSelectionSendNotify(ctx->dpy, &request, notify_property);
        break;
    default:
        break;
    }
}

static Boolean
OwnSelection(Widget widget,
             IswSelectionId selection,
             IswTime time,
             IswConvertSelectionProc convert,
             IswLoseSelectionProc lose,
             IswSelectionDoneProc notify,
             IswCancelConvertSelectionProc cancel,
             IswPointer closure,
             Boolean incremental)
{
    Select ctx;
    Select oldctx = NULL;

    if (!IswIsRealized(widget))
        return False;

    ctx = FindCtx(IswDisplayOf(widget), selection);
    if (ctx->widget != widget || ctx->time != time ||
        ctx->ref_count || ctx->was_disowned) {
        Boolean replacement = FALSE;
        IswWindow window = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget));

        _IswPlatformSetSelectionOwner(ctx->dpy, window,
                                      selection, time);
        if (_IswPlatformGetSelectionOwner(ctx->dpy, selection)
            != window) {
            return FALSE;
        }

        if (ctx->ref_count) {   /* exchange is in-progress */
#ifdef DEBUG_ACTIVE
            printf
                ("Active exchange for widget \"%s\"; selection=0x%lx, ref_count=%d\n",
                 IswName(widget), (long) selection, ctx->ref_count);
#endif
            if (ctx->widget != widget ||
                ctx->convert != convert ||
                ctx->loses != lose ||
                ctx->notify != notify ||
                ctx->owner_cancel != cancel ||
                ctx->incremental != incremental ||
                ctx->owner_closure != closure) {
                if (ctx->widget == widget) {
                    IswRemoveEventHandler(widget, (EventMask) 0, TRUE,
                                         HandleSelectionEvents,
                                         (IswPointer) ctx);
                    IswRemoveCallback(widget, IswNdestroyCallback,
                                     WidgetDestroyed, (IswPointer) ctx);
                    replacement = TRUE;
                }
                else if (!ctx->was_disowned) {
                    oldctx = ctx;
                }
                ctx->free_when_done = TRUE;
                ctx = NewContext(IswDisplayOf(widget), selection);
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
                ctx = NewContext(IswDisplayOf(widget), selection);
            }
            IswAddEventHandler(widget, (EventMask) 0, TRUE,
                              HandleSelectionEvents, (IswPointer) ctx);
            IswAddCallback(widget, IswNdestroyCallback,
                          WidgetDestroyed, (IswPointer) ctx);
        }
        ctx->widget = widget;   /* Selection officially changes hands. */
        ctx->time = time;
        //ctx->serial = serial;
    }
    ctx->convert = convert;
    ctx->loses = lose;
    ctx->notify = notify;
    ctx->owner_cancel = cancel;
    IswSetBit(ctx->incremental, incremental);
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
            IswFree((char *) oldctx);
    }
    return TRUE;
}

Boolean
IswOwnSelection(Widget widget,
               IswSelectionId selection,
               IswTime time,
               IswConvertSelectionProc convert,
               IswLoseSelectionProc lose,
               IswSelectionDoneProc notify)
{
    Boolean retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    retval = OwnSelection(widget, selection, time, convert, lose, notify,
                          (IswCancelConvertSelectionProc) NULL,
                          (IswPointer) NULL, FALSE);
    UNLOCK_APP(app);
    return retval;
}

Boolean
IswOwnSelectionIncremental(Widget widget,
                          IswSelectionId selection,
                          IswTime time,
                          IswConvertSelectionIncrProc convert,
                          IswLoseSelectionIncrProc lose,
                          IswSelectionDoneIncrProc notify,
                          IswCancelConvertSelectionProc cancel,
                          IswPointer closure)
{
    Boolean retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    retval = OwnSelection(widget, selection, time,
                          (IswConvertSelectionProc) convert,
                          (IswLoseSelectionProc) lose,
                          (IswSelectionDoneProc) notify, cancel, closure, TRUE);
    UNLOCK_APP(app);
    return retval;
}

void
IswDisownSelection(Widget widget, IswSelectionId selection, IswTime time)
{
    Select ctx;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    ctx = FindCtx(IswDisplayOf(widget), selection);
    if (LoseSelection(ctx, widget, selection, time))
        _IswPlatformSetSelectionOwner(
            IswDisplayOf(widget), NULL, selection, time);
    UNLOCK_APP(app);
}


/* Selection Requestor code */

static Boolean
IsINCRtype(CallBackInfo info, IswWindow window, IswSelectionId prop)
{
    IswSelectionId type;

    if (prop == ISW_SELECTION_NONE)
        return False;

    IswProperty propr;
    if (!_IswPlatformGetProperty(IswDisplayOf(info->widget),
                                 window, prop, info->type,
                                 0, 0, &propr)) {
        return False;
    }

    type = propr.type;
    _IswPlatformFreeProperty(&propr);

    return (type == info->ctx->prop_list->incr_id);
}

static void
ReqCleanup(Widget widget,
           IswPointer closure,
           IswEvent *iswev,
           Boolean *cont _X_UNUSED)
{
    CallBackInfo info = (CallBackInfo) closure;
    unsigned long length;
    IswSelectionEvent selev;


    if (selev.kind == ISW_SEL_EVENT_REQUEST) {
        if (!MATCH_SELECT(&selev, info))
            return;             /* not really for us */
        IswRemoveEventHandler(widget, (EventMask) 0, TRUE,
                             ReqCleanup, (IswPointer) info);
        if (IsINCRtype(info, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), selev.property)) {
            info->proc = HandleGetIncrement;
            IswAddEventHandler(info->widget, (EventMask) IswPropertyChangeMask,
                              FALSE, ReqCleanup, (IswPointer) info);
        }
        else {
            if (selev.property != ISW_SELECTION_NONE)
                _IswPlatformDeleteProperty(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                                selev.property);
            FreeSelectionProperty(IswDisplayOf(widget), info->property);
            FreeInfo(info);
        }
    }
    else if (selev.kind == ISW_SEL_EVENT_PROP_NEW) {
        if (selev.property == info->property) {
                IswProperty propr;
                length = 0;
                if (_IswPlatformGetProperty(IswDisplayOf(widget),
                        _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), selev.property, info->ctx->prop_list->id_list_type,
                        0L, 1000000, &propr)) {
                    length = propr.num_items;
                    _IswPlatformFreeProperty(&propr);
                }

            if (length == 0) {
                IswRemoveEventHandler(widget, (EventMask) IswPropertyChangeMask,
                                     FALSE, ReqCleanup, (IswPointer) info);
                FreeSelectionProperty(IswDisplayOf(widget), info->property);
                IswFree(info->value);    /* requestor never got this, so free now */
                FreeInfo(info);
            }
        }
    }
}

static void
ReqTimedOut(IswPointer closure, IswIntervalId *id _X_UNUSED)
{
    IswPointer value = NULL;
    unsigned long length = 0;
    int format = 8;
    IswSelectionId resulttype = (IswSelectionId) ISW_CONVERT_FAIL;
    CallBackInfo info = (CallBackInfo) closure;
    unsigned long proplength;

    if (*info->target == info->ctx->prop_list->indirect_id) {
        IswProperty propr;
        if (_IswPlatformGetProperty(IswDisplayOf(info->widget),
                _IswPlatformWidgetWindow(IswDisplayOf((Widget)(info->widget)), (Widget)(info->widget)), info->property, info->ctx->prop_list->id_list_type,
                0, 10000000, &propr)) {
            format = propr.format;
            proplength = propr.num_items;
            _IswPlatformFreeProperty(&propr);

            IswPointer *c;
            int i;

            //IswFree(pairs);
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
        IswRemoveEventHandler(info->widget, (EventMask) 0,
                             TRUE, info->proc, (IswPointer) info);
        IswAddEventHandler(info->widget, (EventMask) 0, TRUE,
                          ReqCleanup, (IswPointer) info);
    }
    else {
        IswRemoveEventHandler(info->widget, (EventMask) IswPropertyChangeMask,
                             FALSE, info->proc, (IswPointer) info);
        IswAddEventHandler(info->widget, (EventMask) IswPropertyChangeMask,
                          FALSE, ReqCleanup, (IswPointer) info);
    }

}

static void
HandleGetIncrement(Widget widget,
                   IswPointer closure,
                   IswEvent *iswev,
                   Boolean *cont _X_UNUSED)
{
    CallBackInfo info = (CallBackInfo) closure;
    Select ctx = info->ctx;
    char *value;
    unsigned long length;
    int n = info->current;
    IswSelectionEvent selev;

    if (!_IswPlatformSelectionDecodeEvent(IswDisplayOf(widget),
                                          IswEventNative(iswev), &selev))
        return;
    if ((selev.kind != ISW_SEL_EVENT_PROP_NEW) ||
        (selev.property != info->property))
        return;

    IswProperty propr;
    if (_IswPlatformGetProperty(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                               selev.property, ctx->prop_list->id_list_type, 0, 10000000, &propr)) {
        info->type = propr.type;
        info->format = propr.format;
        length = propr.num_items;
        /* Copy into an Isw-allocated buffer: downstream frees via IswFree,
           which is not free()-compatible with the backend's malloc'd payload. */
        if (propr.value && length > 0) {
            size_t nbytes = (size_t) BYTELENGTH(length, propr.format);
            value = __XtMalloc((Cardinal) nbytes);
            memcpy(value, propr.value, nbytes);
        } else {
            value = NULL;
        }
        _IswPlatformFreeProperty(&propr);
    } else {
        return;
    }
#ifndef DEBUG_WO_TIMERS
    IswRemoveTimeOut(info->timeout);
#endif
    if (length == 0) {
        unsigned long u_offset = NUMELEM2(info->offset, info->format);

        (*info->callbacks[n]) (widget, *info->req_closure, &ctx->selection,
                               &info->type,
                               (info->offset == 0 ? value : info->value),
                               &u_offset, &info->format);
        /* assert ((info->offset != 0) == (info->incremental[n]) */
        if (info->offset != 0)
            IswFree(value);
        IswRemoveEventHandler(widget, (EventMask) IswPropertyChangeMask, FALSE,
                             HandleGetIncrement, (IswPointer) info);
        FreeSelectionProperty(IswDisplayOf(widget), info->property);

        FreeInfo(info);
    }
    else {                      /* add increment to collection */
        if (info->incremental[n]) {
#ifdef ISW_COPY_SELECTION
            int size = (int) BYTELENGTH(length, info->format) + 1;
            char *tmp = __XtMalloc((Cardinal) size);

            (void) memcpy(tmp, value, (size_t) size);
            IswFree(value);
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
                info->value = IswRealloc(info->value,
                                        (Cardinal) info->bytelength);
            }
            (void) memcpy(&info->value[info->offset], value, (size_t) size);
            info->offset += size;
            IswFree(value);
        }
        /* reset timer */
#ifndef DEBUG_WO_TIMERS
        {
            IswAppContext app = IswWidgetToApplicationContext(info->widget);

            info->timeout = IswAppAddTimeOut(app,
                                            app->selectionTimeout, ReqTimedOut,
                                            (IswPointer) info);
        }
#endif
    }
}

static void
HandleNone(Widget widget,
           IswSelectionCallbackProc callback,
           IswPointer closure,
           IswSelectionId selection)
{
    unsigned long length = 0;
    int format = 8;
    IswSelectionId type = ISW_SELECTION_NONE;

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
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "badFormat", "xtGetSelectionValue", IswCIswToolkitError,
                        "Selection owner returned type INCR property with format != 32",
                        NULL, NULL);
        return 0;
    }
}

static
    Boolean
HandleNormal(IswDisplay dpy,
             Widget widget,
             IswSelectionId property,
             CallBackInfo info,
             IswPointer closure,
             IswSelectionId selection)
{
    unsigned long length;
    int format;
    IswSelectionId type;
    unsigned char *value = NULL;
    int number = info->current;

    IswProperty propr;
    if (!_IswPlatformGetProperty(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                                 property, info->ctx->prop_list->id_list_type, 0, 10000000, &propr)) {
        return FALSE;
    }
    _IswPlatformFreeProperty(&propr);

    if (type == info->ctx->prop_list->incr_id) {
        unsigned long size = IncrPropSize(widget, value, format, length);

        IswFree((char *) value);
        if (info->property != property) {
            /* within MULTIPLE */
            CallBackInfo ninfo;

            ninfo = MakeInfo(info->ctx, &info->callbacks[number],
                             &info->req_closure[number], 1, widget,
                             info->time, &info->incremental[number], &property);
            ninfo->target = (IswSelectionId *) __XtMalloc((unsigned) sizeof(IswSelectionId));
            *ninfo->target = info->target[number + 1];
            info = ninfo;
        }
        HandleIncremental(dpy, widget, property, info, size);
        return FALSE;
    }

    _IswPlatformDeleteProperty(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), property);
#ifdef ISW_COPY_SELECTION
    if (value) {                /* it could have been deleted after the SelectionNotify */
        int size = (int) BYTELENGTH(length, info->format) + 1;
        char *tmp = __XtMalloc((Cardinal) size);

        (void) memcpy(tmp, value, (size_t) size);
        IswFree(value);
        value = (unsigned char *) tmp;
    }
#endif
    (*info->callbacks[number]) (widget, closure, &selection,
                                &type, (IswPointer) value, &length, &format);

    if (info->incremental[number]) {
        /* let requestor know the whole thing has been received */
        value = (unsigned char *) __XtMalloc((unsigned) 1);
        length = 0;
        (*info->callbacks[number]) (widget, closure, &selection,
                                    &type, (IswPointer) value, &length, &format);
    }
    return TRUE;
}

static void
HandleIncremental(IswDisplay dpy,
                  Widget widget,
                  IswSelectionId property,
                  CallBackInfo info,
                  unsigned long size)
{
    IswAddEventHandler(widget, (EventMask) IswPropertyChangeMask, FALSE,
                      HandleGetIncrement, (IswPointer) info);

    /* now start the transfer */
    _IswPlatformDeleteProperty(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), property);
    _IswPlatformFlush(dpy);

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
        IswAppContext app = IswWidgetToApplicationContext(info->widget);

        info->timeout = IswAppAddTimeOut(app,
                                        app->selectionTimeout, ReqTimedOut,
                                        (IswPointer) info);
    }
#endif
}

static void
HandleSelectionReplies(Widget widget,
                       IswPointer closure,
                       IswEvent *iswev,
                       Boolean *cont _X_UNUSED) {
    IswDisplay dpy = IswDisplayOf(widget);
    CallBackInfo info = (CallBackInfo) closure;
    Select ctx = info->ctx;
    unsigned long length = 0;
    int format = 0;
    IswSelectionEvent selev;

    if (!_IswPlatformSelectionDecodeEvent(dpy, IswEventNative(iswev), &selev))
        return;
    if (selev.kind != ISW_SEL_EVENT_NOTIFY)
        return;
    if (!MATCH_SELECT(&selev, info))
        return;                 /* not really for us */
#ifndef DEBUG_WO_TIMERS
    IswRemoveTimeOut(info->timeout);
#endif
    IswRemoveEventHandler(widget, (EventMask) 0, TRUE,
                         HandleSelectionReplies, (IswPointer) info);
    if (selev.target == ctx->prop_list->indirect_id) {
        IndirectPair *pairs = NULL, *p;
        IswPointer *c;

        IswProperty propr;
        Boolean got = _IswPlatformGetProperty(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                          info->property, ctx->prop_list->id_list_type, 0, 10000000, &propr);
        /* original used delete=1 (delete after read) */
        _IswPlatformDeleteProperty(dpy, _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), info->property);
        if (got)
            _IswPlatformFreeProperty(&propr);

        if (!got) {
            length = 0;
        for (length = length / IndirectPairWordSize, p = pairs,
             c = info->req_closure;
             length; length--, p++, c++, info->current++) {
                if (selev.property == ISW_SELECTION_NONE || format != 32
                    || p->target == ISW_SELECTION_NONE
                    || /* bug compatibility */ p->property == ISW_SELECTION_NONE) {
                    HandleNone(widget, info->callbacks[info->current],
                               *c, selev.selection);
                    if (p->property != ISW_SELECTION_NONE)
                        FreeSelectionProperty(IswDisplayOf(widget), p->property);
                }
                else {
                    if (HandleNormal(dpy, widget, p->property, info, *c,
                                     selev.selection)) {
                        FreeSelectionProperty(IswDisplayOf(widget), p->property);
                    }
                }
            }
        }
        IswFree((char *) pairs);
        FreeSelectionProperty(dpy, info->property);
        FreeInfo(info);
    }
    else if (selev.property == ISW_SELECTION_NONE) {
        HandleNone(widget, info->callbacks[0], *info->req_closure,
                   selev.selection);
        FreeSelectionProperty(IswDisplayOf(widget), info->property);
        FreeInfo(info);
    }
    else {
        if (HandleNormal(dpy, widget, selev.property, info,
                         *info->req_closure, selev.selection)) {
            FreeSelectionProperty(IswDisplayOf(widget), info->property);
            FreeInfo(info);
        }
    }
}

static void
DoLocalTransfer(Request req,
                IswSelectionId selection,
                IswSelectionId target,
                Widget widget, /* The widget requesting the value. */
                IswSelectionCallbackProc callback,
                IswPointer closure,    /* the closure for the callback, not the conversion */
                Boolean incremental, IswSelectionId property)
{
    Select ctx = req->ctx;
    IswPointer value = NULL, temp, total = NULL;
    unsigned long length;
    int format;
    IswSelectionId resulttype;
    unsigned long totallength = 0;

    req->request.selection = selection;
    req->request.target = target;
    req->request.property = req->property = property;
    req->request.requestor = req->requestor = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget));

    if (ctx->incremental) {
        unsigned long size = (unsigned long) MAX_SELECTION_INCR(ctx->dpy);

        if (!(*(IswConvertSelectionIncrProc) ctx->convert)
            (ctx->widget, &selection, &target,
             &resulttype, &value, &length, &format,
             &size, ctx->owner_closure, (IswRequestId *) &req)) {
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
                        (*(IswConvertSelectionIncrProc) ctx->convert)
                            (ctx->widget, &selection, &target,
                             &resulttype, &value, &length, &format,
                             &size, ctx->owner_closure, (IswRequestId *) &req);
                    }
                    else
                        allSent = TRUE;
                }
            }
            else {
                while (length) {
                    int bytelength = (int) BYTELENGTH(length, format);

                    total = IswRealloc(total,
                                      (Cardinal) (totallength =
                                                  totallength +
                                                  (unsigned long) bytelength));
                    (void) memcpy((char *) total + totallength - bytelength,
                                   value, (size_t) bytelength);
                    (*(IswConvertSelectionIncrProc) ctx->convert)
                        (ctx->widget, &selection, &target,
                         &resulttype, &value, &length, &format,
                         &size, ctx->owner_closure, (IswRequestId *) &req);
                }
                if (total == NULL)
                    total = __XtMalloc(1);
                totallength = NUMELEM2(totallength, format);
                (*callback) (widget, closure, &selection, &resulttype,
                             total, &totallength, &format);
            }
            if (ctx->notify)
                (*(IswSelectionDoneIncrProc) ctx->notify)
                    (ctx->widget, &selection, &target,
                     (IswRequestId *) &req, ctx->owner_closure);
            else
                IswFree((char *) value);
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
                  IswSelectionId selection,
                  IswSelectionId target,
                  IswSelectionCallbackProc callback,
                  IswPointer closure,
                  IswTime time,
                  Boolean incremental,
                  IswSelectionId property)
{
    Select ctx;
    IswSelectionId properties[1];

    properties[0] = property;

    ctx = FindCtx(IswDisplayOf(widget), selection);
    if (ctx->widget && !ctx->was_disowned) {
        RequestRec req;

        ctx->req = &req;
        memset(&req, 0, sizeof(req));
        req.ctx = ctx;
        req.request.time = time;
        ctx->ref_count++;
        DoLocalTransfer(&req, selection, target, widget,
                        callback, closure, incremental, property);
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            IswFree((char *) ctx);
        else
            ctx->req = NULL;
    }
    else {
        CallBackInfo info;

        info = MakeInfo(ctx, &callback, &closure, 1, widget,
                        time, &incremental, properties);
        info->target = (IswSelectionId *) __XtMalloc((unsigned) sizeof(IswSelectionId));
        *(info->target) = target;
        RequestSelectionValue(info, selection, target);
    }
}

void
IswGetSelectionValue(Widget widget,
                    IswSelectionId selection,
                    IswSelectionId target,
                    IswSelectionCallbackProc callback,
                    IswPointer closure,
                    IswTime time)
{
    IswSelectionId property;
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
IswGetSelectionValueIncremental(Widget widget,
                               IswSelectionId selection,
                               IswSelectionId target,
                               IswSelectionCallbackProc callback,
                               IswPointer closure,
                               IswTime time)
{
    IswSelectionId property;
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
                   IswSelectionId selection,
                   IswSelectionId *targets,
                   int count,
                   IswSelectionCallbackProc *callbacks,
                   int num_callbacks,
                   IswPointer *closures,
                   IswTime time,
                   Boolean *incremental,
                   IswSelectionId *properties)
{
    Select ctx;
    IndirectPair *pairs;

    if (count == 0)
        return;
    ctx = FindCtx(IswDisplayOf(widget), selection);
    if (ctx->widget && !ctx->was_disowned) {
        int j, i;
        RequestRec req;

        ctx->req = &req;
        req.ctx = ctx;
        req.request.time = time;
        ctx->ref_count++;
        for (i = 0, j = 0; count > 0; count--, i++, j++) {
            if (j >= num_callbacks)
                j = 0;

            DoLocalTransfer(&req, selection, targets[i], widget,
                            callbacks[j], closures[i], incremental[i],
                            properties ? properties[i] : ISW_SELECTION_NONE);

        }
        if (--ctx->ref_count == 0 && ctx->free_when_done)
            IswFree((char *) ctx);
        else
            ctx->req = NULL;
    }
    else {
        IswSelectionCallbackProc *passed_callbacks;
        IswSelectionCallbackProc stack_cbs[32];
        CallBackInfo info;
        IndirectPair *p;
        IswSelectionId *t;
        int i = 0, j = 0;

        passed_callbacks = (IswSelectionCallbackProc *)
            IswStackAlloc(sizeof(IswSelectionCallbackProc) * (size_t) count,
                         stack_cbs);

        /* To deal with the old calls from IswGetSelectionValues* we
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
        IswStackFree((IswPointer) passed_callbacks, stack_cbs);

        info->target = IswMallocArray ((Cardinal) count + 1,
                                      (Cardinal) sizeof(IswSelectionId));
        (*info->target) = ctx->prop_list->indirect_id;
        (void) memcpy((char *) info->target + sizeof(IswSelectionId), targets,
                       (size_t) count * sizeof(IswSelectionId));
        pairs = IswMallocArray ((Cardinal) count + 1,
                               (Cardinal) sizeof(IndirectPair));
        for (p = &pairs[count - 1], t = &targets[count - 1], i = count - 1;
             p >= pairs; p--, t--, i--) {
            p->target = *t;
            if (properties == NULL || properties[i] == ISW_SELECTION_NONE) {
                p->property = GetSelectionProperty(IswDisplayOf(widget));
                _IswPlatformDeleteProperty(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                                p->property);
            }
            else {
                p->property = properties[i];
            }
        }
        _IswPlatformChangeProperty(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)), info->property,
                   info->property, 32, ISW_PROP_MODE_REPLACE, pairs,
                   (uint32_t) (count * IndirectPairWordSize));
        IswFree((char *) pairs);
        RequestSelectionValue(info, selection, ctx->prop_list->indirect_id);
    }
}

void
IswGetSelectionValues(Widget widget,
                     IswSelectionId selection,
                     IswSelectionId *targets,
                     int count,
                     IswSelectionCallbackProc callback,
                     IswPointer *closures,
                     IswTime time)
{
    Boolean incremental_values[32];
    Boolean *incremental;
    int i;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    incremental =
        IswStackAlloc((size_t) count * sizeof(Boolean), incremental_values);
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
    IswStackFree((IswPointer) incremental, incremental_values);
    UNLOCK_APP(app);
}

void
IswGetSelectionValuesIncremental(Widget widget,
                                IswSelectionId selection,
                                IswSelectionId *targets,
                                int count,
                                IswSelectionCallbackProc callback,
                                IswPointer *closures,
                                IswTime time)
{
    Boolean incremental_values[32];
    Boolean *incremental;
    int i;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    incremental =
        IswStackAlloc((size_t) count * sizeof(Boolean), incremental_values);
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
    IswStackFree((IswPointer) incremental, incremental_values);
    UNLOCK_APP(app);
}

static Request
GetRequestRecord(Widget widget, IswSelectionId selection, IswRequestId id)
{
    Request req = (Request) id;
    Select ctx = NULL;

    if ((req == NULL
         && ((ctx = FindCtx(IswDisplayOf(widget), selection)) == NULL
             || ctx->req == NULL
             || ctx->selection != selection || ctx->widget == NULL))
        || (req != NULL
            && (req->ctx == NULL
                || req->ctx->selection != selection
                || req->ctx->widget != widget))) {
        String params = IswName(widget);
        Cardinal num_params = 1;

        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "notInConvertSelection", "xtGetSelectionRequest",
                        IswCIswToolkitError,
                        "IswGetSelectionRequest or IswGetSelectionParameters called for widget \"%s\" outside of ConvertSelection proc",
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

   
IswSelectionRequest *
IswGetSelectionRequest(Widget widget, IswSelectionId selection, IswRequestId id)
{
    Request req = (Request) id;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);

    req = GetRequestRecord(widget, selection, id);

    if (!req) {
        UNLOCK_APP(app);
        return (IswSelectionRequest *) NULL;
    }

    if (req->type == 0) {
        /* owner is local; fill in the remainder of the request identity */
        req->request.owner = _IswPlatformWidgetWindow(IswDisplayOf(req->ctx->widget), req->ctx->widget);
        req->request.selection = selection;
    }
    UNLOCK_APP(app);
    return &req->request;
}


/* Multiple utilities */

/* All requests are put in a single list per widget.  It is
   very unlikely anyone will be gathering multiple MULTIPLE
   requests at the same time,  so the loss in efficiency for
   this case is acceptable */

/* Queue one or more requests to the one we're gathering */
void
AddSelectionRequests(Widget wid,
                     IswSelectionId sel,
                     int count,
                     IswSelectionId *targets,
                     IswSelectionCallbackProc *callbacks,
                     int num_cb,
                     IswPointer *closures,
                     Boolean *incrementals,
                     IswSelectionId *properties)
{
    QueuedRequestInfo qi;
    IswWindow window = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(wid)), (Widget)(wid));
    IswWindowId window_id = _IswPlatformWindowId(window);
    IswDisplay dpy = IswDisplayOf(wid);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = IswUniqueContext();

    qi = NULL;
    (void) IswFindContext(dpy, window_id, multipleContext, (void *) &qi);

    if (qi != NULL) {
        QueuedRequest *req = qi->requests;
        int start = qi->count;
        int i = 0;
        int j = 0;

        qi->count += count;
        req = IswReallocArray(req, (Cardinal) (start + count),
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
                _IswPlatformDeleteProperty(dpy, window, newreq->param);
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
IsGatheringRequest(Widget wid, IswSelectionId sel)
{
    QueuedRequestInfo qi;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(wid)), (Widget)(wid)));
    IswDisplay dpy = IswDisplayOf(wid);
    Boolean found = False;

    if (multipleContext == 0)
        multipleContext = IswUniqueContext();

    qi = NULL;
    (void) IswFindContext(dpy, window_id, multipleContext, (void *) &qi);

    if (qi != NULL) {
        int i = 0;

        while (qi->selections[i] != ISW_SELECTION_NONE) {
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
static void
CleanupRequest(IswDisplay dpy, QueuedRequestInfo qi, IswSelectionId sel)
{
    int i, j, n;

    if (qi == NULL)
        return;

    i = 0;

    /* Remove this selection from the list */
    n = 0;
    while (qi->selections[n] != sel && qi->selections[n] != ISW_SELECTION_NONE)
        n++;
    if (qi->selections[n] == sel) {
        while (qi->selections[n] != ISW_SELECTION_NONE) {
            qi->selections[n] = qi->selections[n + 1];
            n++;
        }
    }

    while (i < qi->count) {
        QueuedRequest req = qi->requests[i];

        if (req->selection == sel) {
            /* Match */
            if (req->param != ISW_SELECTION_NONE)
                FreeSelectionProperty(dpy, req->param);
            qi->count--;

            for (j = i; j < qi->count; j++)
                qi->requests[j] = qi->requests[j + 1];

            IswFree((char *) req);
        }
        else {
            i++;
        }
    }
}

void
IswCreateSelectionRequest(Widget widget, IswSelectionId selection)
{
    QueuedRequestInfo queueInfo;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
    IswDisplay dpy = IswDisplayOf(widget);
    Cardinal n;

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = IswUniqueContext();

    queueInfo = NULL;
    (void) IswFindContext(dpy, window_id, multipleContext, (void *) &queueInfo);

    /* If there is one,  then cancel it */
    if (queueInfo != NULL)
        CleanupRequest(dpy, queueInfo, selection);
    else {
        /* Create it */
        queueInfo =
            (QueuedRequestInfo) __XtMalloc(sizeof(QueuedRequestInfoRec));
        queueInfo->count = 0;
        queueInfo->selections = IswMallocArray(2, (Cardinal) sizeof(IswSelectionId));
        queueInfo->selections[0] = ISW_SELECTION_NONE;
        queueInfo->requests = (QueuedRequest *)
            __XtMalloc(sizeof(QueuedRequest));
    }

    /* Append this selection to list */
    n = 0;
    while (queueInfo->selections[n] != ISW_SELECTION_NONE)
        n++;
    queueInfo->selections = IswReallocArray(queueInfo->selections, (n + 2),
                                           (Cardinal) sizeof(IswSelectionId));
    queueInfo->selections[n] = selection;
    queueInfo->selections[n + 1] = ISW_SELECTION_NONE;

    (void) IswSaveContext(dpy, window_id, multipleContext, (char *) queueInfo);
    UNLOCK_PROCESS;
}

void
IswSendSelectionRequest(Widget widget, IswSelectionId selection, IswTime time)
{
    QueuedRequestInfo queueInfo;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
    IswDisplay dpy = IswDisplayOf(widget);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = IswUniqueContext();

    queueInfo = NULL;
    (void) IswFindContext(dpy, window_id, multipleContext, (void *) &queueInfo);
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
                IswSelectionId *targets;
                IswSelectionId t[PREALLOCED];
                IswSelectionCallbackProc *cbs;
                IswSelectionCallbackProc c[PREALLOCED];
                IswPointer *closures;
                IswPointer cs[PREALLOCED];
                Boolean *incrs;
                Boolean ins[PREALLOCED];
                IswSelectionId *props;
                IswSelectionId p[PREALLOCED];
                int j = 0;

                /* Allocate */
                targets =
                    (IswSelectionId *) IswStackAlloc((size_t) count * sizeof(IswSelectionId), t);
                cbs = (IswSelectionCallbackProc *)
                    IswStackAlloc((size_t) count *
                                 sizeof(IswSelectionCallbackProc), c);
                closures =
                    (IswPointer *) IswStackAlloc((size_t) count *
                                               sizeof(IswPointer), cs);
                incrs =
                    (Boolean *) IswStackAlloc((size_t) count * sizeof(Boolean),
                                             ins);
                props = (IswSelectionId *) IswStackAlloc((size_t) count * sizeof(IswSelectionId), p);

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
                IswStackFree((IswPointer) targets, t);
                IswStackFree((IswPointer) cbs, c);
                IswStackFree((IswPointer) closures, cs);
                IswStackFree((IswPointer) incrs, ins);
                IswStackFree((IswPointer) props, p);
            }
        }
    }

    CleanupRequest(dpy, queueInfo, selection);
    UNLOCK_PROCESS;
}

void
IswCancelSelectionRequest(Widget widget, IswSelectionId selection)
{
    QueuedRequestInfo queueInfo;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
    IswDisplay dpy = IswDisplayOf(widget);

    LOCK_PROCESS;
    if (multipleContext == 0)
        multipleContext = IswUniqueContext();

    queueInfo = NULL;
    (void) IswFindContext(dpy, window_id, multipleContext, (void *) &queueInfo);
    /* If there is one,  then cancel it */
    if (queueInfo != NULL)
        CleanupRequest(dpy, queueInfo, selection);
    UNLOCK_PROCESS;
}

/* Parameter utilities */

/* Parameters on a selection request */
/* Places data on an allocated parameter property,  then records the
   parameter id for use in the next call to one of
   the IswGetSelectionValue functions. */
void
IswSetSelectionParameters(Widget requestor,
                         IswSelectionId selection,
                         IswSelectionId type,
                         IswPointer value,
                         unsigned long length,
                         int format)
{
    IswDisplay dpy = IswDisplayOf(requestor);
    IswWindow window = _IswPlatformWidgetWindow(IswDisplayOf((Widget)(requestor)), (Widget)(requestor));
    IswSelectionId property = GetParamInfo(requestor, selection);

    if (property == ISW_SELECTION_NONE) {
        property = GetSelectionProperty(dpy);
        AddParamInfo(requestor, selection, property);
    }

    _IswPlatformChangeProperty(dpy, window,
                               property, type, format, ISW_PROP_MODE_REPLACE,
                               value, (uint32_t) length);
}

/* Retrieves data passed in a parameter. Data for this is stored
   on the originator's window */
void
IswGetSelectionParameters(Widget owner,
                         IswSelectionId selection,
                         IswRequestId request_id,
                         IswSelectionId *type_return,
                         IswPointer *value_return,
                         unsigned long *length_return,
                         int *format_return)
{
    Request req;
    IswDisplay dpy = IswDisplayOf(owner);

    WIDGET_TO_APPCON(owner);

    *value_return = NULL;
    *length_return = (unsigned long) (*format_return = 0);
    *type_return = ISW_SELECTION_NONE;

    LOCK_APP(app);

    req = GetRequestRecord(owner, selection, request_id);

    if (req && req->property) {
        IswProperty propr;
        if (_IswPlatformGetProperty(dpy,
                req->requestor, req->property,
                AnyPropertyType, 0L, 10000000, &propr)) {
            *type_return = propr.type;
            *format_return = propr.format;
            *length_return = propr.num_items;

            if (propr.value && propr.num_items > 0) {
                size_t nbytes = (size_t) BYTELENGTH(propr.num_items, propr.format);
                *value_return = malloc(nbytes);
                if (*value_return)
                    memcpy(*value_return, propr.value, nbytes);
            }
            _IswPlatformFreeProperty(&propr);
        }
        //EndProtectedSection(dpy);
#ifdef ISW_COPY_SELECTION
        if (*value_return) {
            int size = (int) BYTELENGTH(*length_return, *format_return) + 1;
            char *tmp = __XtMalloc((Cardinal) size);

            (void) memcpy(tmp, *value_return, (size_t) size);
            IswFree(*value_return);
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
AddParamInfo(Widget w, IswSelectionId selection, IswSelectionId param_id)
{
    Param p;
    ParamInfo pinfo;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)));

    LOCK_PROCESS;
    if (paramPropertyContext == 0)
        paramPropertyContext = IswUniqueContext();

    if (IswFindContext(IswDisplayOf(w), window_id, paramPropertyContext,
                     (void *) &pinfo)) {
        pinfo = (ParamInfo) __XtMalloc(sizeof(ParamInfoRec));
        pinfo->count = 1;
        pinfo->paramlist = IswNew(ParamRec);
        p = pinfo->paramlist;
        (void) IswSaveContext(IswDisplayOf(w), window_id, paramPropertyContext,
                            (char *) pinfo);
    }
    else {
        int n;

        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++) {
            if (p->selection == ISW_SELECTION_NONE || p->selection == selection)
                break;
        }
        if (n == 0) {
            pinfo->count++;
            pinfo->paramlist = IswReallocArray(pinfo->paramlist, pinfo->count,
                                              (Cardinal) sizeof(ParamRec));
            p = &pinfo->paramlist[pinfo->count - 1];
            (void) IswSaveContext(IswDisplayOf(w), window_id,
                                paramPropertyContext, (char *) pinfo);
        }
    }
    p->selection = selection;
    p->param = param_id;
    UNLOCK_PROCESS;
}

void
RemoveParamInfo(Widget w, IswSelectionId selection)
{
    ParamInfo pinfo;
    Boolean retain = False;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)));

    LOCK_PROCESS;
    if (paramPropertyContext
        && (IswFindContext(IswDisplayOf(w), window_id, paramPropertyContext,
                         (void *) &pinfo) == 0)) {
        Param p;
        int n;

        /* Find and invalidate the parameter data. */
        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++) {
            if (p->selection != ISW_SELECTION_NONE) {
                if (p->selection == selection)
                    p->selection = ISW_SELECTION_NONE;
                else
                    retain = True;
            }
        }
        /* If there's no valid data remaining, release the context entry. */
        if (!retain) {
            IswFree((char *) pinfo->paramlist);
            IswFree((char *) pinfo);
            IswDeleteContext(IswDisplayOf(w), window_id, paramPropertyContext);
        }
    }
    UNLOCK_PROCESS;
}

IswSelectionId
GetParamInfo(Widget w, IswSelectionId selection)
{
    ParamInfo pinfo;
    IswSelectionId param_id = ISW_SELECTION_NONE;
    IswWindowId window_id = _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)));

    LOCK_PROCESS;
    if (paramPropertyContext
        && (IswFindContext(IswDisplayOf(w), window_id, paramPropertyContext,
                         (void *) &pinfo) == 0)) {
        Param p;
        int n;

        for (n = (int) pinfo->count, p = pinfo->paramlist; n; n--, p++)
            if (p->selection == selection) {
                param_id = p->param;
                break;
            }
    }
    UNLOCK_PROCESS;
    return param_id;
}
