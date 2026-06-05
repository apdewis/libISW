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
#include "ResourceI.h"
#include "EventI.h"

/*
 *      IswSetValues(), IswSetSubvalues()
 */

static void
SetValues(char *base,                           /* Base address to write values to */
          XrmResourceList *res,                 /* The current resource values. */
          register Cardinal num_resources,      /* number of items in resources      */
          ArgList args,                         /* The resource values to set */
          Cardinal num_args)                    /* number of items in arg list       */
{
    register ArgList arg;
    register Cardinal i;
    register XrmName argName;
    register XrmResourceList *xrmres;

    /* Resource lists are assumed to be in compiled form already via the
       initial IswGetResources, IswGetSubresources calls */

    for (arg = args; num_args != 0; num_args--, arg++) {
        argName = StringToName(arg->name);
        for (xrmres = res, i = 0; i < num_resources; i++, xrmres++) {
            if(xrmres == NULL) continue;

            if (argName == (*xrmres)->xrm_name) {
                _IswCopyFromArg(arg->value,
                               base - (*xrmres)->xrm_offset - 1,
                               (*xrmres)->xrm_size);
                break;
            }
        }
    }
}                               /* SetValues */

static Boolean
CallSetValues(WidgetClass class,
              Widget current,
              Widget request,
              Widget new,
              ArgList args,
              Cardinal num_args)
{
    Boolean redisplay = FALSE;
    WidgetClass superclass;
    IswArgsFunc set_values_hook;
    IswSetValuesFunc set_values;

    LOCK_PROCESS;
    superclass = class->core_class.superclass;
    UNLOCK_PROCESS;
    if (superclass)
        redisplay =
            CallSetValues(superclass, current, request, new, args, num_args);

    LOCK_PROCESS;
    set_values = class->core_class.set_values;
    UNLOCK_PROCESS;
    if (set_values)
        redisplay |= (*set_values) (current, request, new, args, &num_args);

    LOCK_PROCESS;
    set_values_hook = class->core_class.set_values_hook;
    UNLOCK_PROCESS;
    if (set_values_hook)
        redisplay |= (*set_values_hook) (new, args, &num_args);
    return (redisplay);
}

static Boolean
CallConstraintSetValues(ConstraintWidgetClass class,
                        Widget current,
                        Widget request,
                        Widget new,
                        ArgList args,
                        Cardinal num_args)
{
    Boolean redisplay = FALSE;
    IswSetValuesFunc set_values;

    if ((WidgetClass) class != constraintWidgetClass) {
        if (class == NULL) {
            IswAppErrorMsg(IswWidgetToApplicationContext(current),
                          "invalidClass", "constraintSetValue",
                          IswCIswToolkitError,
                          "Subclass of Constraint required in CallConstraintSetValues",
                          NULL, NULL);
        }
        else {
            ConstraintWidgetClass superclass;

            LOCK_PROCESS;
            superclass = (ConstraintWidgetClass) class->core_class.superclass;
            UNLOCK_PROCESS;
            redisplay =
                CallConstraintSetValues(superclass,
                                        current, request, new, args, num_args);
        }
    }
    LOCK_PROCESS;
    set_values = class ? class->constraint_class.set_values : NULL;
    UNLOCK_PROCESS;
    if (set_values)
        redisplay |= (*set_values) (current, request, new, args, &num_args);
    return (redisplay);
}

void
IswSetSubvalues(IswPointer base,                  /* Base address to write values to */
               register IswResourceList resources,       /* The current resource values.      */
               register Cardinal num_resources, /* number of items in resources      */
               ArgList args,                    /* The resource values to set */
               Cardinal num_args)               /* number of items in arg list       */
{
    register XrmResourceList *xrmres;

    xrmres = _IswCreateIndirectionTable(resources, num_resources);
    SetValues((char *) base, xrmres, num_resources, args, num_args);
    IswFree((char *) xrmres);
}

void
IswSetValues(register Widget w, ArgList args, Cardinal num_args)
{
    register Widget oldw, reqw;

    /* need to use strictest alignment rules possible in next two decls. */
    double oldwCache[100], reqwCache[100];
    double oldcCache[20], reqcCache[20];
    Cardinal widgetSize, constraintSize;
    Boolean redisplay, cleared_rect_obj = False;
    IswWidgetGeometry geoReq, geoReply;
    WidgetClass wc;
    ConstraintWidgetClass cwc = NULL;
    Boolean hasConstraints;
    IswAppContext app = IswWidgetToApplicationContext(w);
    Widget hookobj = IswHooksOfDisplay(IswDisplayOfObject(w));

    LOCK_APP(app);
    wc = IswClass(w);
    if ((args == NULL) && (num_args != 0)) {
        IswAppErrorMsg(app,
                      "invalidArgCount", "xtSetValues", IswCIswToolkitError,
                      "Argument count > 0 on NULL argument list in IswSetValues",
                      NULL, NULL);
    }

    /* Allocate and copy current widget into old widget */

    LOCK_PROCESS;
    widgetSize = wc->core_class.widget_size;
    UNLOCK_PROCESS;
    oldw = (Widget) IswStackAlloc(widgetSize, oldwCache);
    reqw = (Widget) IswStackAlloc(widgetSize, reqwCache);
    (void) memcpy(oldw, w, (size_t) widgetSize);

    /* Set resource values */

    LOCK_PROCESS;
    SetValues((char *) w, (XrmResourceList *) wc->core_class.resources,
              wc->core_class.num_resources, args, num_args);
    UNLOCK_PROCESS;

    (void) memcpy(reqw, w, (size_t) widgetSize);

    hasConstraints = (IswParent(w) != NULL && !IswIsShell(w) &&
                      IswIsConstraint(IswParent(w)));

    /* Some widget sets apparently do ugly things by freeing the
     * constraints on some children, thus the extra test here */
    if (hasConstraints) {
        cwc = (ConstraintWidgetClass) IswClass(w->core.parent);
        if (w->core.constraints) {
            LOCK_PROCESS;
            constraintSize = cwc->constraint_class.constraint_size;
            UNLOCK_PROCESS;
        }
        else
            constraintSize = 0;
    }
    else
        constraintSize = 0;

    if (constraintSize) {
        /* Allocate and copy current constraints into oldw */
        oldw->core.constraints = IswStackAlloc(constraintSize, oldcCache);
        reqw->core.constraints = IswStackAlloc(constraintSize, reqcCache);
        (void) memcpy(oldw->core.constraints,
                      w->core.constraints, (size_t) constraintSize);

        /* Set constraint values */
        LOCK_PROCESS;
        SetValues((char *) w->core.constraints,
                  (XrmResourceList *) (cwc->constraint_class.resources),
                  cwc->constraint_class.num_resources, args, num_args);
        UNLOCK_PROCESS;
        (void) memcpy(reqw->core.constraints,
                      w->core.constraints, (size_t) constraintSize);
    }

    /* Inform widget of changes, then inform parent of changes */
    redisplay = CallSetValues(wc, oldw, reqw, w, args, num_args);
    if (hasConstraints) {
        redisplay |=
            CallConstraintSetValues(cwc, oldw, reqw, w, args, num_args);
    }

    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;
        IswChangeHookSetValuesDataRec set_val;

        set_val.old = oldw;
        set_val.req = reqw;
        set_val.args = args;
        set_val.num_args = num_args;
        call_data.type = IswHsetValues;
        call_data.widget = w;
        call_data.event_data = (IswPointer) &set_val;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }

    if (IswIsRectObj(w)) {
        /* Now perform geometry request if needed */
        geoReq.request_mode = 0;
        if (oldw->core.x != w->core.x) {
            geoReq.x = w->core.x;
            w->core.x = oldw->core.x;
            geoReq.request_mode |= XCB_CONFIG_WINDOW_X;
        }
        if (oldw->core.y != w->core.y) {
            geoReq.y = w->core.y;
            w->core.y = oldw->core.y;
            geoReq.request_mode |= XCB_CONFIG_WINDOW_Y;
        }
        if (oldw->core.width != w->core.width) {
            geoReq.width = w->core.width;
            w->core.width = oldw->core.width;
            geoReq.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
        }
        if (oldw->core.height != w->core.height) {
            geoReq.height = w->core.height;
            w->core.height = oldw->core.height;
            geoReq.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
        }
        if (oldw->core.border_width != w->core.border_width) {
            geoReq.border_width = w->core.border_width;
            w->core.border_width = oldw->core.border_width;
            geoReq.request_mode |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        }

        if (geoReq.request_mode != 0) {
            IswGeometryResult result;

            /* Pass on any requests for unchanged geometry values */
            if (geoReq.request_mode !=
                (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH)) {
                for (; num_args != 0; num_args--, args++) {
                    if (!(geoReq.request_mode & XCB_CONFIG_WINDOW_X) &&
                        strcmp(IswNx, args->name) == 0) {
                        geoReq.x = w->core.x;
                        geoReq.request_mode |= XCB_CONFIG_WINDOW_X;
                    }
                    else if (!(geoReq.request_mode & XCB_CONFIG_WINDOW_Y) &&
                             strcmp(IswNy, args->name) == 0) {
                        geoReq.y = w->core.y;
                        geoReq.request_mode |= XCB_CONFIG_WINDOW_Y;
                    }
                    else if (!(geoReq.request_mode & XCB_CONFIG_WINDOW_WIDTH) &&
                             strcmp(IswNwidth, args->name) == 0) {
                        geoReq.width = w->core.width;
                        geoReq.request_mode |= XCB_CONFIG_WINDOW_WIDTH;
                    }
                    else if (!(geoReq.request_mode & XCB_CONFIG_WINDOW_HEIGHT) &&
                             strcmp(IswNheight, args->name) == 0) {
                        geoReq.height = w->core.height;
                        geoReq.request_mode |= XCB_CONFIG_WINDOW_HEIGHT;
                    }
                    else if (!(geoReq.request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) &&
                             strcmp(IswNborderWidth, args->name) == 0) {
                        geoReq.border_width = w->core.border_width;
                        geoReq.request_mode |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
                    }
                }
            }
            CALLGEOTAT(_IswGeoTrace(w,
                                   "\nXtSetValues sees some geometry changes for \"%s\".\n",
                                   IswName(w)));
            CALLGEOTAT(_IswGeoTab(1));
            do {
                IswGeometryHookDataRec call_data;
                IswAlmostProc set_values_almost;

                if (IswHasCallbacks(hookobj, IswNgeometryHook) ==
                    IswCallbackHasSome) {
                    call_data.type = IswHpreGeometry;
                    call_data.widget = w;
                    call_data.request = &geoReq;
                    IswCallCallbackList(hookobj,
                                       ((HookObject) hookobj)->hooks.
                                       geometryhook_callbacks,
                                       (IswPointer) &call_data);
                    call_data.result = result =
                        _IswMakeGeometryRequest(w, &geoReq, &geoReply,
                                               &cleared_rect_obj);
                    call_data.type = IswHpostGeometry;
                    call_data.reply = &geoReply;
                    IswCallCallbackList(hookobj,
                                       ((HookObject) hookobj)->hooks.
                                       geometryhook_callbacks,
                                       (IswPointer) &call_data);
                }
                else {
                    result = _IswMakeGeometryRequest(w, &geoReq, &geoReply,
                                                    &cleared_rect_obj);
                }
                if (result == IswGeometryYes || result == IswGeometryDone)
                    break;

                /* An Almost or No reply.  Call widget and let it munge
                   request, reply */
                LOCK_PROCESS;
                set_values_almost = wc->core_class.set_values_almost;
                UNLOCK_PROCESS;
                if (set_values_almost == NULL) {
                    IswAppWarningMsg(app,
                                    "invalidProcedure", "set_values_almost",
                                    IswCIswToolkitError,
                                    "set_values_almost procedure shouldn't be NULL",
                                    NULL, NULL);
                    break;
                }
                if (result == IswGeometryNo)
                    geoReply.request_mode = 0;
                CALLGEOTAT(_IswGeoTrace(w, "calling SetValuesAlmost.\n"));
                (*set_values_almost) (oldw, w, &geoReq, &geoReply);
            } while (geoReq.request_mode != 0);
            /* call resize proc if we changed size and parent
             * didn't already invoke resize */
            {
                IswWidgetProc resize;

                LOCK_PROCESS;
                resize = wc->core_class.resize;
                UNLOCK_PROCESS;
                if ((w->core.width != oldw->core.width ||
                     w->core.height != oldw->core.height)
                    && result != IswGeometryDone
                    && resize != (IswWidgetProc) NULL) {
                    CALLGEOTAT(_IswGeoTrace(w,
                                           "IswSetValues calls \"%s\"'s resize proc.\n",
                                           IswName(w)));
                    (*resize) (w);
                }
            }
            CALLGEOTAT(_IswGeoTab(-1));
        }
        /* Redisplay if needed.  No point in clearing if the window is
         * about to disappear, as the Expose event will just go straight
         * to the bit bucket. */
        if (IswIsWidget(w) && w->core.windowless) {
            /* A windowless widget shares its windowed ancestor's window, so
               xcb_clear_area(IswWindow(w), ...) would blank and re-expose the
               whole ancestor — flashing the cleared background before the
               async repaint.  Repaint just this widget's surface and composite
               the ancestor, mirroring the windowless branch in Geometry.c. */
            if (redisplay && IswIsRealized(w) && !w->core.being_destroyed) {
                CALLGEOTAT(_IswGeoTrace(w,
                                       "IswSetValues repaints windowless \"%s\".\n",
                                       IswName(w)));
                _IswRepaintWindowless(w);
            }
        }
        else if (IswIsWidget(w)) {
            /* widgets can distinguish between redisplay and resize, since
               the server will cause an expose on resize */
            if (redisplay && IswIsRealized(w) && !w->core.being_destroyed) {
                CALLGEOTAT(_IswGeoTrace(w,
                                       "IswSetValues calls ClearArea on \"%s\".\n",
                                       IswName(w)));
                xcb_clear_area(
                        IswDisplay(w), 1, IswWindow(w), 0, 0, 0, 0
                    );
                xcb_flush(IswDisplay(w));
            }
        }
        else {                  /*non-window object */
            if (redisplay && !cleared_rect_obj) {
                Widget pw = _IswWindowedAncestor(w);

                if (IswIsRealized(pw) && !pw->core.being_destroyed) {
                    RectObj r = (RectObj) w;
                    int bw2 = r->rectangle.border_width << 1;

                    CALLGEOTAT(_IswGeoTrace(w,
                                           "IswSetValues calls ClearArea on \"%s\"'s parent \"%s\".\n",
                                           IswName(w), IswName(pw)));

                    xcb_clear_area(
                        IswDisplay(pw),
                        1,  /* generate Expose event */
                        IswWindow(pw),
                        r->rectangle.x, 
                        r->rectangle.y,
                        (unsigned) (r->rectangle.width + bw2),
                        (unsigned) (r->rectangle.height + bw2)
                    );
                    xcb_flush(IswDisplay(pw));
                }
            }
        }
    }

    /* Free dynamic storage */
    if (constraintSize) {
        IswStackFree(oldw->core.constraints, oldcCache);
        IswStackFree(reqw->core.constraints, reqcCache);
    }
    IswStackFree((IswPointer) oldw, oldwCache);
    IswStackFree((IswPointer) reqw, reqwCache);
    UNLOCK_APP(app);
}                               /* IswSetValues */


