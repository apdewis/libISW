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
#include "ShellP.h"
#include "ShellI.h"
#include <ISW/ISWRender.h>
#include <ISW/ISWPlatform.h>
#include <ISW/EventI.h>
#include <ISW/SimpleP.h>


#include <math.h>

static void
ClearRectObjAreas(RectObj r, uint32_t old_x, uint32_t old_y, uint32_t old_w, uint32_t old_h, uint32_t old_bw)
{
    Widget pw = _IswWidgetAncestor((Widget) r);
    int bw2;

    bw2 = old_bw << 1;
    _IswPlatformClearArea(IswDisplayOf(pw), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(pw)), (Widget)(pw)),
        (int16_t) old_x, (int16_t) old_y,
        (uint16_t) (old_w + bw2), (uint16_t) (old_h + bw2), False
    );

    {
        IswBorderSides bs = _IswGetBorderSides((Widget) r);
        _IswPlatformClearArea(IswDisplayOf(pw), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(pw)), (Widget)(pw)),
                   (int16_t) r->rectangle.x, (int16_t) r->rectangle.y,
                   (uint16_t) (r->rectangle.width + _IswBorderHoriz(bs)),
                   (uint16_t) (r->rectangle.height + _IswBorderVert(bs)), False);
    }
}

/*
 * Internal function used by IswMakeGeometryRequest and IswSetValues.
 * Returns more data than the public interface.  Does not convert
 * IswGeometryDone to IswGeometryYes.
 *
 * clear_rect_obj - *** RETURNED ***
 *                  TRUE if the rect obj has been cleared, false otherwise.
 */

IswGeometryResult
_IswMakeGeometryRequest(Widget widget,
                       IswWidgetGeometry *request,
                       IswWidgetGeometry *reply,
                       Boolean *clear_rect_obj)
{
    IswWidgetGeometry junk;
    IswGeometryHandler manager = (IswGeometryHandler) NULL;
    IswGeometryResult returnCode;
    Widget parent = widget->core.parent;
    Boolean managed;
    Boolean parentRealized = False;
    Boolean rgm = False;
    IswConfigureHookDataRec req;
    Widget hookobj;

    *clear_rect_obj = FALSE;

    CALLGEOTAT(_IswGeoTrace(widget,
                           "\"%s\" is making a %sgeometry request to its parent \"%s\".\n",
                           IswName(widget),
                           ((request->request_mode & IswCWQueryOnly)) ?
                           "query only " : "",
                           (IswParent(widget)) ? IswName(IswParent(widget)) :
                           "Root"));
    CALLGEOTAT(_IswGeoTab(1));

    if (IswIsShell(widget)) {
        ShellClassExtension ext;

        LOCK_PROCESS;
        for (ext = (ShellClassExtension) ((ShellWidgetClass) IswClass(widget))
             ->shell_class.extension;
             ext != NULL && ext->record_type != NULLQUARK;
             ext = (ShellClassExtension) ext->next_extension);

        if (ext != NULL) {
            if (ext->version == IswShellExtensionVersion
                && ext->record_size == sizeof(ShellClassExtensionRec)) {
                manager = ext->root_geometry_manager;
                rgm = True;
            }
            else {
                String params[1];
                Cardinal num_params = 1;

                params[0] = IswClass(widget)->core_class.class_name;
                IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                              "invalidExtension", "xtMakeGeometryRequest",
                              IswCIswToolkitError,
                              "widget class %s has invalid ShellClassExtension record",
                              params, &num_params);
            }
        }
        else {
            IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                          "internalError", "xtMakeGeometryRequest",
                          IswCIswToolkitError,
                          "internal error; ShellClassExtension is NULL",
                          NULL, NULL);
        }
        managed = True;
        parentRealized = TRUE;
        UNLOCK_PROCESS;
    }
    else {                      /* not shell */

        if (parent == NULL) {
            IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                          "invalidParent", "xtMakeGeometryRequest",
                          IswCIswToolkitError,
                          "non-shell has no parent in IswMakeGeometryRequest",
                          NULL, NULL);
        }
        else {
            managed = IswIsManaged(widget);
            parentRealized = IswIsRealized(parent);
            if (IswIsComposite(parent)) {
                LOCK_PROCESS;
                manager = ((CompositeWidgetClass) (parent->core.widget_class))
                    ->composite_class.geometry_manager;
                UNLOCK_PROCESS;
            }
        }
    }

#if 0
    /*
     * The Xt spec says that these conditions must generate
     * error messages (not warnings), but many Xt applications
     * and toolkits (including parts of Xaw, Motif and Netscape)
     * depend on the previous Xt behaviour.  Thus, these tests
     * should probably remain disabled.
     */
    if (parentRealized && managed) {
        if (parent && !IswIsComposite(parent)) {
            /*
             * This shouldn't ever happen, we only test for this to pass
             * VSW5.  Normally managing the widget will catch this, but VSW5
             * does some really screwy stuff to get here.
             */
            IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                          "invalidParent", "xtMakeGeometryRequest",
                          IswCIswToolkitError,
                          "IswMakeGeometryRequest - parent not composite",
                          NULL, NULL);
        }
        else if (manager == (IswGeometryHandler) NULL) {
            IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                          "invalidGeometryManager", "xtMakeGeometryRequest",
                          IswCIswToolkitError,
                          "IswMakeGeometryRequest - parent has no geometry manager",
                          NULL, NULL);
        }
    }
#else
    if (!manager)
        managed = False;
#endif

    if (widget->core.being_destroyed) {
        CALLGEOTAT(_IswGeoTab(-1));
        CALLGEOTAT(_IswGeoTrace(widget,
                               "It is being destroyed, just return IswGeometryNo.\n"));
        return IswGeometryNo;
    }

    /* see if requesting anything to change */
    req.changeMask = 0;
    if (request->request_mode &IswCWStackMode
        && request->stack_mode != IswSMDontChange) {
        req.changeMask |=IswCWStackMode;
        CALLGEOTAT(_IswGeoTrace(widget, "Asking for a change in StackMode!\n"));
        if (request->request_mode & IswCWSibling) {
            IswCheckSubclass(request->sibling, rectObjClass,
                            "IswMakeGeometryRequest");
            req.changeMask |= IswCWSibling;
        }
    }
    if (request->request_mode & IswCWX && widget->core.x != request->x) {
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Asking for a change in x: from %d to %d.\n",
                               widget->core.x, request->x));
        req.changeMask |= IswCWX;
    }
    if (request->request_mode & IswCWY && widget->core.y != request->y) {
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Asking for a change in y: from %d to %d.\n",
                               widget->core.y, request->y));
        req.changeMask |= IswCWY;
    }
    if (request->request_mode & IswCWWidth  && widget->core.width != request->width) {
        CALLGEOTAT(_IswGeoTrace
                   (widget, "Asking for a change in width: from %d to %d.\n",
                    widget->core.width, request->width));
        req.changeMask |= IswCWWidth ;
    }
    if (request->request_mode & IswCWHeight
        && widget->core.height != request->height) {
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Asking for a change in height: from %d to %d.\n",
                               widget->core.height, request->height));
        req.changeMask |= IswCWHeight;
    }
    if (request->request_mode & IswCWBorderWidth
        && widget->core.border_width != request->border_width) {
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Asking for a change in border_width: from %d to %d.\n",
                               widget->core.border_width,
                               request->border_width));
        req.changeMask |= IswCWBorderWidth;
    }
    if (!req.changeMask) {
        CALLGEOTAT(_IswGeoTrace(widget, "Asking for nothing new,\n"));
        CALLGEOTAT(_IswGeoTab(-1));
        CALLGEOTAT(_IswGeoTrace(widget, "just return IswGeometryYes.\n"));
        return IswGeometryYes;
    }
    req.changeMask |= (request->request_mode & IswCWQueryOnly);

    if (!(req.changeMask & IswCWQueryOnly) && IswIsRealized(widget)) {
        /* keep record of the current geometry so we know what's changed */
        req.changes_x = widget->core.x;
        req.changes_y = widget->core.y;
        req.changes_w = widget->core.width;
        req.changes_h = widget->core.height;
        req.changes_bw = widget->core.border_width;
    }

    if (!managed || !parentRealized) {
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Not Managed or Parent not realized.\n"));
        /* Don't get parent's manager involved--assume the answer is yes */
        if (req.changeMask & IswCWQueryOnly) {
            /* He was just asking, don't change anything, just tell him yes */
            CALLGEOTAT(_IswGeoTrace(widget, "QueryOnly request\n"));
            CALLGEOTAT(_IswGeoTab(-1));
            CALLGEOTAT(_IswGeoTrace(widget, "just return IswGeometryYes.\n"));
            return IswGeometryYes;
        }
        else {
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "Copy values from request to widget.\n"));
            /* copy values from request to widget */
            if (request->request_mode & IswCWX)
                widget->core.x = request->x;
            if (request->request_mode & IswCWY)
                widget->core.y = request->y;
            if (request->request_mode & IswCWWidth )
                widget->core.width = request->width;
            if (request->request_mode & IswCWHeight)
                widget->core.height = request->height;
            if (request->request_mode & IswCWBorderWidth)
                widget->core.border_width = request->border_width;
            if (!parentRealized) {
                CALLGEOTAT(_IswGeoTab(-1));
                CALLGEOTAT(_IswGeoTrace(widget, "and return IswGeometryYes.\n"));
                return IswGeometryYes;
            }
            else
                returnCode = IswGeometryYes;
        }
    }
    else {
        /* go ask the widget's geometry manager */
        CALLGEOTAT(_IswGeoTrace(widget,
                               "Go ask the parent geometry manager.\n"));
        if (reply == (IswWidgetGeometry *) NULL) {
            returnCode = (*manager) (widget, request, &junk);
        }
        else {
            returnCode = (*manager) (widget, request, reply);
        }
    }

    /*
     * If Unrealized, not a IswGeometryYes, or a query-only then we are done.
     */

    if ((returnCode != IswGeometryYes) ||
        (req.changeMask & IswCWQueryOnly) || !IswIsRealized(widget)) {

#ifdef ISW_GEO_TATTLER
        switch (returnCode) {
        case IswGeometryNo:
            CALLGEOTAT(_IswGeoTab(-1));
            CALLGEOTAT(_IswGeoTrace(widget, "\"%s\" returns IswGeometryNo.\n",
                                   (IswParent(widget)) ? IswName(IswParent(widget))
                                   : "Root"));
            /* check for no change */
            break;
        case IswGeometryDone:
            CALLGEOTAT(_IswGeoTab(-1));
            CALLGEOTAT(_IswGeoTrace(widget, "\"%s\" returns IswGeometryDone.\n",
                                   (IswParent(widget)) ? IswName(IswParent(widget))
                                   : "Root"));
            /* check for no change in queryonly */
            break;
        case IswGeometryAlmost:
            CALLGEOTAT(_IswGeoTab(-1));
            CALLGEOTAT(_IswGeoTrace(widget, "\"%s\" returns IswGeometryAlmost.\n",
                                   (IswParent(widget)) ? IswName(IswParent(widget))
                                   : "Root"));
            CALLGEOTAT(_IswGeoTab(1));
            CALLGEOTAT(_IswGeoTrace(widget, "Proposal: width %d height %d.\n",
                                   (reply) ? reply->width : junk.width,
                                   (reply) ? reply->height : junk.height));
            CALLGEOTAT(_IswGeoTab(-1));

            /* check for no change */
            break;
        case IswGeometryYes:
            if (req.changeMask & IswCWQueryOnly) {
                CALLGEOTAT(_IswGeoTrace(widget,
                                       "QueryOnly specified, no configuration.\n"));
            }
            if (!IswIsRealized(widget)) {
                CALLGEOTAT(_IswGeoTrace(widget,
                                       "\"%s\" not realized, no configuration.\n",
                                       IswName(widget)));
            }
            CALLGEOTAT(_IswGeoTab(-1));
            CALLGEOTAT(_IswGeoTrace(widget, "\"%s\" returns IswGeometryYes.\n",
                                   (IswParent(widget)) ? IswName(IswParent(widget))
                                   : "Root"));
            break;
        }
#endif
        return returnCode;
    }

    CALLGEOTAT(_IswGeoTab(-1));
    CALLGEOTAT(_IswGeoTrace(widget, "\"%s\" returns IswGeometryYes.\n",
                           (IswParent(widget)) ? IswName(IswParent(widget)) :
                           "Root"));

    if (IswIsWidget(widget)) {   /* reconfigure the window (if needed) */

        if (rgm)
            return returnCode;

        if (req.changes_x != widget->core.x) {
            req.changeMask |= IswCWX;
            req.changes_x = widget->core.x;
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "x changing to %d\n", widget->core.x));
        }
        if (req.changes_y != widget->core.y) {
            req.changeMask |= IswCWY;
            req.changes_y = widget->core.y;
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "y changing to %d\n", widget->core.y));
        }
        if (req.changes_w != widget->core.width) {
            req.changeMask |= IswCWWidth;
            req.changes_w = widget->core.width;
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "width changing to %d\n",
                                   widget->core.width));
        }
        if (req.changes_h != widget->core.height) {
            req.changeMask |= IswCWHeight;
            req.changes_h = widget->core.height;
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "height changing to %d\n",
                                   widget->core.height));
        }
        if (req.changes_bw != widget->core.border_width) {
            req.changeMask |= IswCWBorderWidth;
            req.changes_bw = widget->core.border_width;
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "border_width changing to %d\n",
                                   widget->core.border_width));
        }
        if (req.changeMask & IswCWStackMode) {
            req.changes_sm = request->stack_mode;
            CALLGEOTAT(_IswGeoTrace(widget, "stack_mode changing\n"));
            if (req.changeMask & IswCWSibling) {
                if (IswIsWidget(request->sibling))
                    req.changes_sb = (int32_t) _IswPlatformWindowId(_IswPlatformWidgetWindow(IswDisplayOf((Widget)(request->sibling)), (Widget)(request->sibling)));
                else
                    req.changeMask =
                        (IswGeometryMask) (req.changeMask & (unsigned long)
                                          (~(IswCWStackMode | IswCWSibling)));
            }
        }

#ifdef ISW_GEO_TATTLER
        if (req.changeMask) {
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "XConfigure \"%s\"'s window.\n",
                                   IswName(widget)));
        }
        else {
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "No window configuration needed for \"%s\".\n",
                                   IswName(widget)));
        }
#endif
        /* A widget owns no window — geometry is applied to core.x/y/width/
           height (done above) and the parent's geometry manager repaints the
           widget's surface.  There is no window to configure. */
    }
    else {                      /* RectObj child of realized Widget */
        *clear_rect_obj = TRUE;
        CALLGEOTAT(_IswGeoTrace(widget,
                               "ClearRectObj on \"%s\".\n", IswName(widget)));

        ClearRectObjAreas((RectObj) widget, req.changes_x, req.changes_y, req.changes_w, req.changes_h, req.changes_bw);
    }
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNconfigureHook) == IswCallbackHasSome) {
        req.type = IswHconfigure;
        req.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.confighook_callbacks,
                           (IswPointer) &req);
    }

    return returnCode;
}                               /* _IswMakeGeometryRequest */

/* Public routines */

IswGeometryResult
IswMakeGeometryRequest(Widget widget,
                      IswWidgetGeometry *request,
                      IswWidgetGeometry *reply)
{
    Boolean junk;
    IswGeometryResult r;
    IswGeometryHookDataRec call_data;
    Widget hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (IswHasCallbacks(hookobj, IswNgeometryHook) == IswCallbackHasSome) {
        call_data.type = IswHpreGeometry;
        call_data.widget = widget;
        call_data.request = request;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (IswPointer) &call_data);
        call_data.result = r =
            _IswMakeGeometryRequest(widget, request, reply, &junk);
        call_data.type = IswHpostGeometry;
        call_data.reply = reply;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (IswPointer) &call_data);
    }
    else {
        r = _IswMakeGeometryRequest(widget, request, reply, &junk);
    }
    UNLOCK_APP(app);

    return ((r == IswGeometryDone) ? IswGeometryYes : r);
}

IswGeometryResult
IswMakeResizeRequest(Widget widget,
                    _IswDimension width,
                    _IswDimension height,
                    Dimension *replyWidth,
                    Dimension *replyHeight)
{
    IswWidgetGeometry request, reply;
    IswGeometryResult r;
    IswGeometryHookDataRec call_data;
    Boolean junk;
    Widget hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    memset(&request, 0, sizeof(request));
    request.request_mode = IswCWWidth | IswCWHeight;
    request.width = (Dimension) width;
    request.height = (Dimension) height;

    if (IswHasCallbacks(hookobj, IswNgeometryHook) == IswCallbackHasSome) {
        call_data.type = IswHpreGeometry;
        call_data.widget = widget;
        call_data.request = &request;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (IswPointer) &call_data);
        call_data.result = r =
            _IswMakeGeometryRequest(widget, &request, &reply, &junk);
        call_data.type = IswHpostGeometry;
        call_data.reply = &reply;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (IswPointer) &call_data);
    }
    else {
        r = _IswMakeGeometryRequest(widget, &request, &reply, &junk);
    }
    if (replyWidth != NULL) {
        if (r == IswGeometryAlmost && reply.request_mode & IswCWWidth )
            *replyWidth = reply.width;
        else
            *replyWidth = (Dimension) width;
    }
    if (replyHeight != NULL) {
        if (r == IswGeometryAlmost && reply.request_mode & IswCWHeight)
            *replyHeight = reply.height;
        else
            *replyHeight = (Dimension) height;
    }
    UNLOCK_APP(app);
    return ((r == IswGeometryDone) ? IswGeometryYes : r);
}                               /* IswMakeResizeRequest */

void
IswResizeWindow(Widget w)
{
    IswConfigureHookDataRec req;

    WIDGET_TO_APPCON(w);

    LOCK_APP(app);
    if (IswIsRealized(w)) {
        Widget hookobj;

        req.changes_w = w->core.width;
        req.changes_h = w->core.height;
        req.changes_bw = w->core.border_width;
        req.changeMask = IswCWWidth | IswCWHeight | IswCWBorderWidth;
        /* HiDPI: convert logical pixels to physical for the X server. */
        {
            double sf = _IswGetScaleFactor(IswDisplayOf(w));
            IswWindowGeometry g;
            memset(&g, 0, sizeof(g));
            g.width = (uint32_t)lrint((double)req.changes_w * sf);
            g.height = (uint32_t)lrint((double)req.changes_h * sf);
            g.border_width = (uint32_t)lrint((double)req.changes_bw * sf);
            /* Windowless widgets own no X window — see _IswMakeGeometryRequest. */
            if (IswIsShell(w))
                _IswPlatformConfigureWindow(IswDisplayOf(w), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(w)), (Widget)(w)), &g,
                                            ISW_CONFIG_WIDTH | ISW_CONFIG_HEIGHT |
                                            ISW_CONFIG_BORDER,
                                            ISW_STACK_NONE, NULL);
        }
        hookobj = IswHooksOfDisplay(IswDisplayOfObject(w));
        if (IswHasCallbacks(hookobj, IswNconfigureHook) == IswCallbackHasSome) {
            req.type = IswHconfigure;
            req.widget = w;
            IswCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               confighook_callbacks, (IswPointer) &req);
        }
    }
    UNLOCK_APP(app);
}                               /* IswResizeWindow */

void
IswResizeWidget(Widget w,
               _IswDimension width,
               _IswDimension height,
               _IswDimension borderWidth)
{
    IswConfigureWidget(w, w->core.x, w->core.y, width, height, borderWidth);
}                               /* IswResizeWidget */

void
IswConfigureWidget(Widget w,
                  _IswPosition x,
                  _IswPosition y,
                  _IswDimension width,
                  _IswDimension height,
                  _IswDimension borderWidth)
{
    IswConfigureHookDataRec req;
    uint32_t old_x, old_y, old_h, old_w, old_bw;

    WIDGET_TO_APPCON(w);

    CALLGEOTAT(_IswGeoTrace(w,
                           "\"%s\" is being configured by its parent \"%s\"\n",
                           IswName(w),
                           (IswParent(w)) ? IswName(IswParent(w)) : "Root"));
    CALLGEOTAT(_IswGeoTab(1));

    LOCK_APP(app);
    req.changeMask = 0;
    if ((old_x = w->core.x) != x) {
        CALLGEOTAT(_IswGeoTrace(w, "x move from %d to %d\n", w->core.x, x));
        req.changes_x = w->core.x = (Position) x;
        req.changeMask |= IswCWX;
    }

    if ((old_y = w->core.y) != y) {
        CALLGEOTAT(_IswGeoTrace(w, "y move from %d to %d\n", w->core.y, y));
        req.changes_y = w->core.y = (Position) y;
        req.changeMask |= IswCWY;
    }

    if ((old_w = w->core.width) != width) {
        CALLGEOTAT(_IswGeoTrace(w,
                               "width move from %d to %d\n", w->core.width,
                               width));
        req.changes_w = w->core.width = (Dimension) width;
        req.changeMask |= IswCWWidth;
    }

    if ((old_h = w->core.height) != height) {
        CALLGEOTAT(_IswGeoTrace(w,
                               "height move from %d to %d\n", w->core.height,
                               height));
        req.changes_h = w->core.height = (Dimension) height;
        req.changeMask |= IswCWHeight;
    }

    if ((old_bw = w->core.border_width) != borderWidth) {
        CALLGEOTAT(_IswGeoTrace(w, "border_width move from %d to %d\n",
                               w->core.border_width, borderWidth));
        req.changes_bw = w->core.border_width =
            (Dimension) borderWidth;
        req.changeMask |= IswCWBorderWidth;
    }

    if (req.changeMask != 0) {
        Widget hookobj;

        /* Relayout BEFORE painting/compositing.  The resize proc updates layout
           state the expose proc reads (a Label recenters label_x to the new
           width; a container repositions children).  For a windowless widget
           the size-change repaint below is the only paint this call performs,
           so it must run after the resize proc, not before — otherwise the
           surface is repainted from stale pre-resize state. */
        {
            IswWidgetProc resize;

            LOCK_PROCESS;
            resize = IswClass(w)->core_class.resize;
            UNLOCK_PROCESS;
            if ((req.changeMask & (IswCWWidth | IswCWHeight)) &&
                resize != (IswWidgetProc) NULL) {
                CALLGEOTAT(_IswGeoTrace(w, "Resize proc is called.\n"));
                (*resize) (w);
            }
        }

        if (IswIsRealized(w) && IswIsShell(w)) {
            /* A shell backs a real top-level window; its geometry change must
               reach the server.  IswResizeWindow only carries W/H/border, so a
               post-realize reposition (e.g. a popped-up menu moved via
               IswConfigureWidget) would leave the window at its mapped origin.
               Push the changed components, scaled logical→physical like
               IswResizeWindow does. */
            double sf = _IswGetScaleFactor(IswDisplayOf(w));
            IswWindowGeometry g;
            uint32_t cfgMask = 0;
            memset(&g, 0, sizeof(g));
            if (req.changeMask & IswCWX) {
                g.x = (int32_t)lrint((double)w->core.x * sf);
                cfgMask |= ISW_CONFIG_X;
            }
            if (req.changeMask & IswCWY) {
                g.y = (int32_t)lrint((double)w->core.y * sf);
                cfgMask |= ISW_CONFIG_Y;
            }
            if (req.changeMask & IswCWWidth) {
                g.width = (uint32_t)lrint((double)w->core.width * sf);
                cfgMask |= ISW_CONFIG_WIDTH;
            }
            if (req.changeMask & IswCWHeight) {
                g.height = (uint32_t)lrint((double)w->core.height * sf);
                cfgMask |= ISW_CONFIG_HEIGHT;
            }
            if (req.changeMask & IswCWBorderWidth) {
                g.border_width = (uint32_t)lrint((double)w->core.border_width * sf);
                cfgMask |= ISW_CONFIG_BORDER;
            }
            if (cfgMask != 0)
                _IswPlatformConfigureWindow(IswDisplayOf(w),
                                            _IswPlatformWidgetWindow(IswDisplayOf(w), w),
                                            &g, cfgMask, ISW_STACK_NONE, NULL);
        }
        else if (IswIsRealized(w) && IswIsWidget(w)) {
            /* Windowless widgets have no X window to configure, and the server
               never sends them an Expose after a geometry change.

               A SIZE change means the widget's own surface is now the wrong
               size and its painted content is stale — it must be repainted at
               the new size (the windowed equivalent's post-resize Expose).
               _IswRepaintWindowless redraws the widget and its windowless
               descendants into their surfaces, then composites the ancestor.

               A position-only MOVE leaves the surface content valid; just
               re-composite the ancestor so the widget lands at its new offset.
               We must NOT xcb_clear_area the shared window to provoke an Expose
               — that flashes the cleared background before the async repaint.
               The coalesced composite avoids any cleared-state flicker. */
            Widget pw = _IswWidgetAncestor(w);

            if (!w->core.being_destroyed &&
                w->core.widget_class->core_class.expose != NULL) {
                /* Repaint THIS widget's surface by invoking its expose proc
                   directly.  A SIZE change obviously needs this (the surface is
                   reallocated at the new dimensions).  A position-only MOVE also
                   needs it: the widget may paint only the visible portion of its
                   content (e.g. ListView inside a Viewport), so shifting its
                   position changes which region is visible and the old surface
                   content is stale.  Suppress the per-end() auto-composite; the
                   request below folds the ancestor once. */
                ISWRenderBeginCompositeBatch();
                (*w->core.widget_class->core_class.expose)(w, NULL, 0);
                ISWRenderEndCompositeBatch();
            }
            /* The widget moved/resized, so its old footprint in the parent's
               persisted composite surface is stale.  Force the parent chain to
               re-expose (a position-only move does no repaint of w, so nothing
               else would mark it). */
            _ISWRenderMarkDirtyChain(w->core.parent);
            if (pw != NULL && IswIsRealized(pw) && !pw->core.being_destroyed)
                ISWRenderRequestComposite(pw);
        }
        hookobj = IswHooksOfDisplay(IswDisplayOfObject(w));
        if (IswHasCallbacks(hookobj, IswNconfigureHook) == IswCallbackHasSome) {
            req.type = IswHconfigure;
            req.widget = w;
            IswCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               confighook_callbacks, (IswPointer) &req);
        }
    }
    else {
        CALLGEOTAT(_IswGeoTrace(w, "No change in configuration\n"));
    }

    CALLGEOTAT(_IswGeoTab(-1));
    UNLOCK_APP(app);
}                               /* IswConfigureWidget */

void
IswMoveWidget(Widget w, _IswPosition x, _IswPosition y)
{
    IswConfigureWidget(w, x, y, w->core.width, w->core.height,
                      w->core.border_width);
}                               /* IswMoveWidget */

void
IswTranslateCoords(register Widget w,
                  _IswPosition x,
                  _IswPosition y,
                  register Position *rootx, /* return */
                  register Position *rooty) /* return */
{
    Position garbagex, garbagey;
    IswAppContext app = IswWidgetToApplicationContext(w);

    LOCK_APP(app);
    if (rootx == NULL)
        rootx = &garbagex;
    if (rooty == NULL)
        rooty = &garbagey;

    *rootx = (Position) x;
    *rooty = (Position) y;

    for (; w != NULL && !IswIsShell(w); w = w->core.parent) {
        IswBorderSides bs = _IswGetBorderSides(w);
        *rootx = (Position) (*rootx + w->core.x + bs.left);
        *rooty = (Position) (*rooty + w->core.y + bs.top);
    }

    if (w == NULL)
        IswAppWarningMsg(app,
                        "invalidShell", "xtTranslateCoords", IswCIswToolkitError,
                        "Widget has no shell ancestor", NULL, NULL);
    else {
        Position x2, y2;

        _IswShellGetCoordinates(w, &x2, &y2);
        *rootx = (Position) (*rootx + x2 + w->core.border_width);
        *rooty = (Position) (*rooty + y2 + w->core.border_width);
    }
    UNLOCK_APP(app);
}

IswGeometryResult IswQueryGeometry(Widget widget,
                                 register IswWidgetGeometry *intended, /* parent's changes; may be NULL */
                                 IswWidgetGeometry *reply) {    /* child's preferred geometry; never NULL */
    IswWidgetGeometry null_intended;
    IswGeometryHandler query;
    IswGeometryResult result;

    WIDGET_TO_APPCON(widget);

    CALLGEOTAT(_IswGeoTrace(widget,
                           "\"%s\" is asking its preferred geometry to \"%s\".\n",
                           (IswParent(widget)) ? IswName(IswParent(widget)) :
                           "Root", IswName(widget)));
    CALLGEOTAT(_IswGeoTab(1));

    LOCK_APP(app);
    LOCK_PROCESS;
    query = IswClass(widget)->core_class.query_geometry;
    UNLOCK_PROCESS;
    reply->request_mode = 0;
    if (query != NULL) {
        if (intended == NULL) {
            null_intended.request_mode = 0;
            intended = &null_intended;
#ifdef ISW_GEO_TATTLER
            CALLGEOTAT(_IswGeoTrace(widget, "without any constraint.\n"));
        }
        else {
            CALLGEOTAT(_IswGeoTrace(widget,
                                   "with the following constraints:\n"));

            if (intended->request_mode & IswCWX) {
                CALLGEOTAT(_IswGeoTrace(widget, " x = %d\n", intended->x));
            }
            if (intended->request_mode & IswCWY) {
                CALLGEOTAT(_IswGeoTrace(widget, " y = %d\n", intended->y));
            }
            if (intended->request_mode & IswCWWidth ) {
                CALLGEOTAT(_IswGeoTrace(widget,
                                       " width = %d\n", intended->width));
            }
            if (intended->request_mode & IswCWHeight) {
                CALLGEOTAT(_IswGeoTrace(widget,
                                       " height = %d\n", intended->height));
            }
            if (intended->request_mode & IswCWBorderWidth) {
                CALLGEOTAT(_IswGeoTrace(widget,
                                       " border_width = %d\n",
                                       intended->border_width));
            }
#endif
        }

        result = (*query) (widget, intended, reply);
    }
    else {
        CALLGEOTAT(_IswGeoTrace
                   (widget,
                    "\"%s\" has no QueryGeometry proc, return the current state\n",
                    IswName(widget)));

        result = IswGeometryYes;
    }

#ifdef ISW_GEO_TATTLER
#define FillIn(mask, field) \
        if (!(reply->request_mode & mask)) {\
              reply->field = widget->core.field;\
              _IswGeoTrace(widget," using core %s = %d.\n","field",\
                                                       widget->core.field);\
        } else {\
              _IswGeoTrace(widget," replied %s = %d\n","field",\
                                                   reply->field);\
        }
#else
#define FillIn(mask, field) \
        if (!(reply->request_mode & mask)) reply->field = widget->core.field;
#endif

    FillIn(IswCWX, x);
    FillIn(IswCWY, y);
    FillIn(IswCWWidth , width);
    FillIn(IswCWHeight, height);
    FillIn(IswCWBorderWidth, border_width);

    CALLGEOTAT(_IswGeoTab(-1));
#undef FillIn

    if (!(reply->request_mode &IswCWStackMode))
        reply->stack_mode = IswSMDontChange;
    UNLOCK_APP(app);
    return result;
}
