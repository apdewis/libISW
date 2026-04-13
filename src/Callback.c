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

Copyright 1987, 1988, 1998  The Open Group

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

static _Xconst _IswString IswNinvalidCallbackList = "invalidCallbackList";
static _Xconst _IswString IswNxtAddCallback = "xtAddCallback";
static _Xconst _IswString IswNxtRemoveCallback = "xtRemoveCallback";
static _Xconst _IswString IswNxtRemoveAllCallback = "xtRemoveAllCallback";
static _Xconst _IswString IswNxtCallCallback = "xtCallCallback";

/* However it doesn't contain a final NULL record */
#if __STDC_VERSION__ >= 199901L
#define ToList(p) ((p)->callbacks)
#else
#define ToList(p) ((IswCallbackList) ((p)+1))
#endif

static InternalCallbackList *
FetchInternalList(Widget widget,
                  _Xconst char *name)
{
    XrmQuark quark;
    int n;
    CallbackTable offsets;
    InternalCallbackList *retval = NULL;

    quark = StringToQuark(name);
    LOCK_PROCESS;
    offsets = (CallbackTable)
        widget->core.widget_class->core_class.callback_private;

    if (offsets == NULL) {
        UNLOCK_PROCESS;
        return NULL;
    }

    for (n = (int) (long) *(offsets++); --n >= 0; offsets++)
        if (quark == (*offsets)->xrm_name) {
            retval = (InternalCallbackList *)
                ((char *) widget - (*offsets)->xrm_offset - 1);
            break;
        }
    UNLOCK_PROCESS;
    return retval;
}

void
_IswAddCallback(InternalCallbackList *callbacks,
               IswCallbackProc callback,
               IswPointer closure)
{
    register InternalCallbackList icl;
    register IswCallbackList cl;
    register int count;

    icl = *callbacks;
    count = icl ? icl->count : 0;

    if (icl && icl->call_state) {
        icl->call_state |= _IswCBFreeAfterCalling;
        icl = (InternalCallbackList)
            __XtMalloc((Cardinal) (sizeof(InternalCallbackRec) +
                                   sizeof(IswCallbackRec) * (size_t) (count +
                                                                     1)));
        (void) memmove((char *) ToList(icl), (char *) ToList(*callbacks),
                       sizeof(IswCallbackRec) * (size_t) count);
    }
    else {
        icl = (InternalCallbackList)
            IswRealloc((char *) icl, (Cardinal) (sizeof(InternalCallbackRec) +
                                                sizeof(IswCallbackRec) *
                                                (size_t) (count + 1)));
    }
    *callbacks = icl;
    icl->count = (unsigned short) (count + 1);
    icl->is_padded = 0;
    icl->call_state = 0;
    cl = ToList(icl) + count;
    cl->callback = callback;
    cl->closure = closure;
}                               /* _IswAddCallback */

void
_IswAddCallbackOnce(register InternalCallbackList *callbacks,
                   IswCallbackProc callback,
                   IswPointer closure)
{
    register IswCallbackList cl = ToList(*callbacks);
    register int i;

    for (i = (*callbacks)->count; --i >= 0; cl++)
        if (cl->callback == callback && cl->closure == closure)
            return;

    _IswAddCallback(callbacks, callback, closure);
}                               /* _IswAddCallbackOnce */

void
IswAddCallback(Widget widget,
              _Xconst char *name,
              IswCallbackProc callback,
              IswPointer closure)
{
    InternalCallbackList *callbacks;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtAddCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswAddCallback", NULL,
                        NULL);
        UNLOCK_APP(app);
        return;
    }
    _IswAddCallback(callbacks, callback, closure);
    if (!_IswIsHookObject(widget)) {
        Widget hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));

        if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
            IswChangeHookDataRec call_data;

            call_data.type = IswHaddCallback;
            call_data.widget = widget;
            call_data.event_data = (IswPointer) name;
            IswCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               changehook_callbacks, (IswPointer) &call_data);
        }
    }
    UNLOCK_APP(app);
}                               /* IswAddCallback */

static void
AddCallbacks(Widget widget _X_UNUSED,
             InternalCallbackList *callbacks,
             IswCallbackList newcallbacks)
{
    register InternalCallbackList icl;
    register int i, j;
    register IswCallbackList cl;

    icl = *callbacks;
    i = icl ? icl->count : 0;
    for (j = 0, cl = newcallbacks; cl->callback; cl++, j++);
    if (icl && icl->call_state) {
        icl->call_state |= _IswCBFreeAfterCalling;
        icl = (InternalCallbackList)
            __XtMalloc((Cardinal)
                       (sizeof(InternalCallbackRec) +
                        sizeof(IswCallbackRec) * (size_t) (i + j)));
        (void) memmove((char *) ToList(*callbacks), (char *) ToList(icl),
                       sizeof(IswCallbackRec) * (size_t) i);
    }
    else {
        icl = (InternalCallbackList) IswRealloc((char *) icl,
                                               (Cardinal) (sizeof
                                                           (InternalCallbackRec)
                                                           +
                                                           sizeof(IswCallbackRec)
                                                           * (size_t) (i + j)));
    }
    *callbacks = icl;
    icl->count = (unsigned short) (i + j);
    icl->is_padded = 0;
    icl->call_state = 0;
    for (cl = ToList(icl) + i; --j >= 0;)
        *cl++ = *newcallbacks++;
}                               /* AddCallbacks */

void
IswAddCallbacks(Widget widget,
               _Xconst char *name,
               IswCallbackList xtcallbacks)
{
    InternalCallbackList *callbacks;
    Widget hookobj;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtAddCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswAddCallbacks", NULL,
                        NULL);
        UNLOCK_APP(app);
        return;
    }
    AddCallbacks(widget, callbacks, xtcallbacks);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHaddCallbacks;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) name;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswAddCallbacks */

void
_IswRemoveCallback(InternalCallbackList *callbacks,
                  IswCallbackProc callback,
                  IswPointer closure)
{
    register InternalCallbackList icl;
    register int i, j;
    register IswCallbackList cl, ncl, ocl;

    icl = *callbacks;
    if (!icl)
        return;

    cl = ToList(icl);
    for (i = icl->count; --i >= 0; cl++) {
        if (cl->callback == callback && cl->closure == closure) {
            if (icl->call_state) {
                icl->call_state |= _IswCBFreeAfterCalling;
                if (icl->count == 1) {
                    *callbacks = NULL;
                }
                else {
                    j = icl->count - i - 1;
                    ocl = ToList(icl);
                    icl = (InternalCallbackList)
                        __XtMalloc((Cardinal) (sizeof(InternalCallbackRec) +
                                               sizeof(IswCallbackRec) *
                                               (size_t) (i + j)));
                    icl->count = (unsigned short) (i + j);
                    icl->is_padded = 0;
                    icl->call_state = 0;
                    ncl = ToList(icl);
                    while (--j >= 0)
                        *ncl++ = *ocl++;
                    while (--i >= 0)
                        *ncl++ = *++cl;
                    *callbacks = icl;
                }
            }
            else {
                if (--icl->count) {
                    ncl = cl + 1;
                    while (--i >= 0)
                        *cl++ = *ncl++;
                    icl = (InternalCallbackList)
                        IswRealloc((char *) icl,
                                  (Cardinal) (sizeof(InternalCallbackRec)
                                              +
                                              sizeof(IswCallbackRec) *
                                              icl->count));
                    icl->is_padded = 0;
                    *callbacks = icl;
                }
                else {
                    IswFree((char *) icl);
                    *callbacks = NULL;
                }
            }
            return;
        }
    }
}                               /* _IswRemoveCallback */

void
IswRemoveCallback(Widget widget,
                 _Xconst char *name,
                 IswCallbackProc callback,
                 IswPointer closure)
{
    InternalCallbackList *callbacks;
    Widget hookobj;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtRemoveCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswRemoveCallback", NULL,
                        NULL);
        UNLOCK_APP(app);
        return;
    }
    _IswRemoveCallback(callbacks, callback, closure);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHremoveCallback;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) name;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswRemoveCallback */

void
IswRemoveCallbacks(Widget widget,
                  _Xconst char *name,
                  IswCallbackList xtcallbacks)
{
    InternalCallbackList *callbacks;
    Widget hookobj;
    int i;
    InternalCallbackList icl;
    IswCallbackList cl, ccl, rcl;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtRemoveCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswRemoveCallbacks", NULL,
                        NULL);
        UNLOCK_APP(app);
        return;
    }

    icl = *callbacks;
    if (!icl) {
        UNLOCK_APP(app);
        return;
    }

    i = icl->count;
    cl = ToList(icl);
    if (icl->call_state) {
        icl->call_state |= _IswCBFreeAfterCalling;
        icl =
            (InternalCallbackList)
            __XtMalloc((Cardinal)
                       (sizeof(InternalCallbackRec) +
                        sizeof(IswCallbackRec) * (size_t) i));
        icl->count = (unsigned short) i;
        icl->call_state = 0;
    }
    ccl = ToList(icl);
    while (--i >= 0) {
        *ccl++ = *cl;
        for (rcl = xtcallbacks; rcl->callback; rcl++) {
            if (cl->callback == rcl->callback && cl->closure == rcl->closure) {
                ccl--;
                icl->count--;
                break;
            }
        }
        cl++;
    }
    if (icl->count) {
        icl = (InternalCallbackList)
            IswRealloc((char *) icl, (Cardinal) (sizeof(InternalCallbackRec) +
                                                sizeof(IswCallbackRec) *
                                                icl->count));
        icl->is_padded = 0;
        *callbacks = icl;
    }
    else {
        IswFree((char *) icl);
        *callbacks = NULL;
    }
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHremoveCallbacks;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) name;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswRemoveCallbacks */

void
_IswRemoveAllCallbacks(InternalCallbackList *callbacks)
{
    register InternalCallbackList icl = *callbacks;

    if (icl) {
        if (icl->call_state)
            icl->call_state |= _IswCBFreeAfterCalling;
        else
            IswFree((char *) icl);
        *callbacks = NULL;
    }
}                               /* _IswRemoveAllCallbacks */

void
IswRemoveAllCallbacks(Widget widget, _Xconst char *name)
{
    InternalCallbackList *callbacks;
    Widget hookobj;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtRemoveAllCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswRemoveAllCallbacks",
                        NULL, NULL);
        UNLOCK_APP(app);
        return;
    }
    _IswRemoveAllCallbacks(callbacks);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHremoveAllCallbacks;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) name;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswRemoveAllCallbacks */

InternalCallbackList
_IswCompileCallbackList(IswCallbackList xtcallbacks)
{
    register int n;
    register IswCallbackList xtcl, cl;
    register InternalCallbackList callbacks;

    for (n = 0, xtcl = xtcallbacks; xtcl->callback; n++, xtcl++) {
    };
    if (n == 0)
        return (InternalCallbackList) NULL;

    callbacks =
        (InternalCallbackList)
        __XtMalloc((Cardinal)
                   (sizeof(InternalCallbackRec) +
                    sizeof(IswCallbackRec) * (size_t) n));
    callbacks->count = (unsigned short) n;
    callbacks->is_padded = 0;
    callbacks->call_state = 0;
    cl = ToList(callbacks);
    while (--n >= 0)
        *cl++ = *xtcallbacks++;
    return (callbacks);
}                               /* _IswCompileCallbackList */

IswCallbackList
_IswGetCallbackList(InternalCallbackList *callbacks)
{
    int i;
    InternalCallbackList icl;
    IswCallbackList cl;

    icl = *callbacks;
    if (!icl) {
        static IswCallbackRec emptyList[1] = { {NULL, NULL} };
        return (IswCallbackList) emptyList;
    }
    if (icl->is_padded)
        return ToList(icl);
    i = icl->count;
    if (icl->call_state) {
        IswCallbackList ocl;

        icl->call_state |= _IswCBFreeAfterCalling;
        ocl = ToList(icl);
        icl = (InternalCallbackList)
            __XtMalloc((Cardinal)
                       (sizeof(InternalCallbackRec) +
                        sizeof(IswCallbackRec) * (size_t) (i + 1)));
        icl->count = (unsigned short) i;
        icl->call_state = 0;
        cl = ToList(icl);
        while (--i >= 0)
            *cl++ = *ocl++;
    }
    else {
        icl = (InternalCallbackList)
            IswRealloc((char *) icl, (Cardinal) (sizeof(InternalCallbackRec)
                                                + sizeof(IswCallbackRec)
                                                * (size_t) (i + 1)));
        cl = ToList(icl) + i;
    }
    icl->is_padded = 1;
    cl->callback = (IswCallbackProc) NULL;
    cl->closure = NULL;
    *callbacks = icl;
    return ToList(icl);
}

void
IswCallCallbacks(Widget widget,
                _Xconst char *name,
                IswPointer call_data)
{
    InternalCallbackList *callbacks;
    InternalCallbackList icl;
    IswCallbackList cl;
    int i;
    char ostate;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, name);
    if (!callbacks) {
        IswAppWarningMsg(app,
                        IswNinvalidCallbackList, IswNxtCallCallback,
                        IswCIswToolkitError,
                        "Cannot find callback list in IswCallCallbacks", NULL,
                        NULL);
        UNLOCK_APP(app);
        return;
    }

    icl = *callbacks;
    if (!icl) {
        UNLOCK_APP(app);
        return;
    }
    cl = ToList(icl);
    if (icl->count == 1) {
        (*cl->callback) (widget, cl->closure, call_data);
        UNLOCK_APP(app);
        return;
    }
    ostate = icl->call_state;
    icl->call_state = _IswCBCalling;
    for (i = icl->count; --i >= 0; cl++)
        (*cl->callback) (widget, cl->closure, call_data);
    if (ostate)
        icl->call_state |= ostate;
    else if (icl->call_state & _IswCBFreeAfterCalling)
        IswFree((char *) icl);
    else
        icl->call_state = ostate;
    UNLOCK_APP(app);
}                               /* IswCallCallbacks */

IswCallbackStatus
IswHasCallbacks(Widget widget,
               _Xconst char *callback_name)
{
    InternalCallbackList *callbacks;
    IswCallbackStatus retval = IswCallbackHasSome;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    callbacks = FetchInternalList(widget, callback_name);
    if (!callbacks)
        retval = IswCallbackNoList;
    else if (!*callbacks)
        retval = IswCallbackHasNone;
    UNLOCK_APP(app);
    return retval;
}                               /* IswHasCallbacks */

void
IswCallCallbackList(Widget widget,
                   IswCallbackList callbacks,
                   IswPointer call_data)
{
    register InternalCallbackList icl;
    register IswCallbackList cl;
    register int i;
    char ostate;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (!callbacks) {
        UNLOCK_APP(app);
        return;
    }
    icl = (InternalCallbackList) callbacks;
    cl = ToList(icl);
    if (icl->count == 1) {
        (*cl->callback) (widget, cl->closure, call_data);
        UNLOCK_APP(app);
        return;
    }
    ostate = icl->call_state;
    icl->call_state = _IswCBCalling;
    for (i = icl->count; --i >= 0; cl++)
        (*cl->callback) (widget, cl->closure, call_data);
    if (ostate)
        icl->call_state |= ostate;
    else if (icl->call_state & _IswCBFreeAfterCalling)
        IswFree((char *) icl);
    else
        icl->call_state = 0;
    UNLOCK_APP(app);
}                               /* IswCallCallbackList */

void
_IswPeekCallback(Widget widget _X_UNUSED,
                IswCallbackList callbacks,
                IswCallbackProc *callback,
                IswPointer *closure)
{
    register InternalCallbackList icl = (InternalCallbackList) callbacks;
    register IswCallbackList cl;

    if (!callbacks) {
        *callback = (IswCallbackProc) NULL;
        return;
    }
    cl = ToList(icl);
    *callback = cl->callback;
    *closure = cl->closure;
    return;
}

void
_IswCallConditionalCallbackList(Widget widget,
                               IswCallbackList callbacks,
                               IswPointer call_data,
                               _IswConditionProc cond_proc)
{
    register InternalCallbackList icl;
    register IswCallbackList cl;
    register int i;
    char ostate;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (!callbacks) {
        UNLOCK_APP(app);
        return;
    }
    icl = (InternalCallbackList) callbacks;
    cl = ToList(icl);
    if (icl->count == 1) {
        (*cl->callback) (widget, cl->closure, call_data);
        (void) (*cond_proc) (call_data);
        UNLOCK_APP(app);
        return;
    }
    ostate = icl->call_state;
    icl->call_state = _IswCBCalling;
    for (i = icl->count; --i >= 0; cl++) {
        (*cl->callback) (widget, cl->closure, call_data);
        if (!(*cond_proc) (call_data))
            break;
    }
    if (ostate)
        icl->call_state |= ostate;
    else if (icl->call_state & _IswCBFreeAfterCalling)
        IswFree((char *) icl);
    else
        icl->call_state = 0;
    UNLOCK_APP(app);
}
