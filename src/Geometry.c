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

static void
ClearRectObjAreas(RectObj r, uint32_t old_x, uint32_t old_y, uint32_t old_w, uint32_t old_h, uint32_t old_bw)
{
    Widget pw = _XtWindowedAncestor((Widget) r);
    int bw2;

    bw2 = old_bw << 1;
    xcb_void_cookie_t cookie = xcb_clear_area_checked(XtDisplay(pw), 0, XtWindow(pw),
        old_x, old_y,
        (unsigned) (old_w + bw2), (unsigned) (old_h + bw2)
    );

    bw2 = r->rectangle.border_width << 1;
    cookie = xcb_clear_area_checked(XtDisplay(pw), 0, XtWindow(pw),
               (int) r->rectangle.x, (int) r->rectangle.y,
               (unsigned int) (r->rectangle.width + bw2),
               (unsigned int) (r->rectangle.height + bw2));
}

/*
 * Internal function used by XtMakeGeometryRequest and XtSetValues.
 * Returns more data than the public interface.  Does not convert
 * XtGeometryDone to XtGeometryYes.
 *
 * clear_rect_obj - *** RETURNED ***
 *                  TRUE if the rect obj has been cleared, false otherwise.
 */

XtGeometryResult
_XtMakeGeometryRequest(Widget widget,
                       XtWidgetGeometry *request,
                       XtWidgetGeometry *reply,
                       Boolean *clear_rect_obj)
{
    XtWidgetGeometry junk;
    XtGeometryHandler manager = (XtGeometryHandler) NULL;
    XtGeometryResult returnCode;
    Widget parent = widget->core.parent;
    Boolean managed;
    Boolean parentRealized = False;
    Boolean rgm = False;
    XtConfigureHookDataRec req;
    Widget hookobj;

    *clear_rect_obj = FALSE;

    CALLGEOTAT(_XtGeoTrace(widget,
                           "\"%s\" is making a %sgeometry request to its parent \"%s\".\n",
                           XtName(widget),
                           ((request->request_mode & XtCWQueryOnly)) ?
                           "query only " : "",
                           (XtParent(widget)) ? XtName(XtParent(widget)) :
                           "Root"));
    CALLGEOTAT(_XtGeoTab(1));

    if (XtIsShell(widget)) {
        ShellClassExtension ext;

        LOCK_PROCESS;
        for (ext = (ShellClassExtension) ((ShellWidgetClass) XtClass(widget))
             ->shell_class.extension;
             ext != NULL && ext->record_type != NULLQUARK;
             ext = (ShellClassExtension) ext->next_extension);

        if (ext != NULL) {
            if (ext->version == XtShellExtensionVersion
                && ext->record_size == sizeof(ShellClassExtensionRec)) {
                manager = ext->root_geometry_manager;
                rgm = True;
            }
            else {
                String params[1];
                Cardinal num_params = 1;

                params[0] = XtClass(widget)->core_class.class_name;
                XtAppErrorMsg(XtWidgetToApplicationContext(widget),
                              "invalidExtension", "xtMakeGeometryRequest",
                              XtCXtToolkitError,
                              "widget class %s has invalid ShellClassExtension record",
                              params, &num_params);
            }
        }
        else {
            XtAppErrorMsg(XtWidgetToApplicationContext(widget),
                          "internalError", "xtMakeGeometryRequest",
                          XtCXtToolkitError,
                          "internal error; ShellClassExtension is NULL",
                          NULL, NULL);
        }
        managed = True;
        parentRealized = TRUE;
        UNLOCK_PROCESS;
    }
    else {                      /* not shell */

        if (parent == NULL) {
            XtAppErrorMsg(XtWidgetToApplicationContext(widget),
                          "invalidParent", "xtMakeGeometryRequest",
                          XtCXtToolkitError,
                          "non-shell has no parent in XtMakeGeometryRequest",
                          NULL, NULL);
        }
        else {
            managed = XtIsManaged(widget);
            parentRealized = XtIsRealized(parent);
            if (XtIsComposite(parent)) {
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
        if (parent && !XtIsComposite(parent)) {
            /*
             * This shouldn't ever happen, we only test for this to pass
             * VSW5.  Normally managing the widget will catch this, but VSW5
             * does some really screwy stuff to get here.
             */
            XtAppErrorMsg(XtWidgetToApplicationContext(widget),
                          "invalidParent", "xtMakeGeometryRequest",
                          XtCXtToolkitError,
                          "XtMakeGeometryRequest - parent not composite",
                          NULL, NULL);
        }
        else if (manager == (XtGeometryHandler) NULL) {
            XtAppErrorMsg(XtWidgetToApplicationContext(widget),
                          "invalidGeometryManager", "xtMakeGeometryRequest",
                          XtCXtToolkitError,
                          "XtMakeGeometryRequest - parent has no geometry manager",
                          NULL, NULL);
        }
    }
#else
    if (!manager)
        managed = False;
#endif

    if (widget->core.being_destroyed) {
        CALLGEOTAT(_XtGeoTab(-1));
        CALLGEOTAT(_XtGeoTrace(widget,
                               "It is being destroyed, just return XtGeometryNo.\n"));
        return XtGeometryNo;
    }

    /* see if requesting anything to change */
    req.changeMask = 0;
    if (request->request_mode &XCB_CONFIG_WINDOW_STACK_MODE
        && request->stack_mode != XtSMDontChange) {
        req.changeMask |=XCB_CONFIG_WINDOW_STACK_MODE;
        CALLGEOTAT(_XtGeoTrace(widget, "Asking for a change in StackMode!\n"));
        if (request->request_mode & XCB_CONFIG_WINDOW_SIBLING) {
            XtCheckSubclass(request->sibling, rectObjClass,
                            "XtMakeGeometryRequest");
            req.changeMask |= XCB_CONFIG_WINDOW_SIBLING;
        }
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_X && widget->core.x != request->x) {
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Asking for a change in x: from %d to %d.\n",
                               widget->core.x, request->x));
        req.changeMask |= XCB_CONFIG_WINDOW_X;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_Y && widget->core.y != request->y) {
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Asking for a change in y: from %d to %d.\n",
                               widget->core.y, request->y));
        req.changeMask |= XCB_CONFIG_WINDOW_Y;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH  && widget->core.width != request->width) {
        CALLGEOTAT(_XtGeoTrace
                   (widget, "Asking for a change in width: from %d to %d.\n",
                    widget->core.width, request->width));
        req.changeMask |= XCB_CONFIG_WINDOW_WIDTH ;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT
        && widget->core.height != request->height) {
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Asking for a change in height: from %d to %d.\n",
                               widget->core.height, request->height));
        req.changeMask |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH
        && widget->core.border_width != request->border_width) {
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Asking for a change in border_width: from %d to %d.\n",
                               widget->core.border_width,
                               request->border_width));
        req.changeMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }
    if (!req.changeMask) {
        CALLGEOTAT(_XtGeoTrace(widget, "Asking for nothing new,\n"));
        CALLGEOTAT(_XtGeoTab(-1));
        CALLGEOTAT(_XtGeoTrace(widget, "just return XtGeometryYes.\n"));
        return XtGeometryYes;
    }
    req.changeMask |= (request->request_mode & XtCWQueryOnly);

    if (!(req.changeMask & XtCWQueryOnly) && XtIsRealized(widget)) {
        /* keep record of the current geometry so we know what's changed */
        req.changes_x = widget->core.x;
        req.changes_y = widget->core.y;
        req.changes_w = widget->core.width;
        req.changes_h = widget->core.height;
        req.changes_bw = widget->core.border_width;
    }

    if (!managed || !parentRealized) {
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Not Managed or Parent not realized.\n"));
        /* Don't get parent's manager involved--assume the answer is yes */
        if (req.changeMask & XtCWQueryOnly) {
            /* He was just asking, don't change anything, just tell him yes */
            CALLGEOTAT(_XtGeoTrace(widget, "QueryOnly request\n"));
            CALLGEOTAT(_XtGeoTab(-1));
            CALLGEOTAT(_XtGeoTrace(widget, "just return XtGeometryYes.\n"));
            return XtGeometryYes;
        }
        else {
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "Copy values from request to widget.\n"));
            /* copy values from request to widget */
            if (request->request_mode & XCB_CONFIG_WINDOW_X)
                widget->core.x = request->x;
            if (request->request_mode & XCB_CONFIG_WINDOW_Y)
                widget->core.y = request->y;
            if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH )
                widget->core.width = request->width;
            if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT)
                widget->core.height = request->height;
            if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                widget->core.border_width = request->border_width;
            if (!parentRealized) {
                CALLGEOTAT(_XtGeoTab(-1));
                CALLGEOTAT(_XtGeoTrace(widget, "and return XtGeometryYes.\n"));
                return XtGeometryYes;
            }
            else
                returnCode = XtGeometryYes;
        }
    }
    else {
        /* go ask the widget's geometry manager */
        CALLGEOTAT(_XtGeoTrace(widget,
                               "Go ask the parent geometry manager.\n"));
        if (reply == (XtWidgetGeometry *) NULL) {
            returnCode = (*manager) (widget, request, &junk);
        }
        else {
            returnCode = (*manager) (widget, request, reply);
        }
    }

    /*
     * If Unrealized, not a XtGeometryYes, or a query-only then we are done.
     */

    if ((returnCode != XtGeometryYes) ||
        (req.changeMask & XtCWQueryOnly) || !XtIsRealized(widget)) {

#ifdef XT_GEO_TATTLER
        switch (returnCode) {
        case XtGeometryNo:
            CALLGEOTAT(_XtGeoTab(-1));
            CALLGEOTAT(_XtGeoTrace(widget, "\"%s\" returns XtGeometryNo.\n",
                                   (XtParent(widget)) ? XtName(XtParent(widget))
                                   : "Root"));
            /* check for no change */
            break;
        case XtGeometryDone:
            CALLGEOTAT(_XtGeoTab(-1));
            CALLGEOTAT(_XtGeoTrace(widget, "\"%s\" returns XtGeometryDone.\n",
                                   (XtParent(widget)) ? XtName(XtParent(widget))
                                   : "Root"));
            /* check for no change in queryonly */
            break;
        case XtGeometryAlmost:
            CALLGEOTAT(_XtGeoTab(-1));
            CALLGEOTAT(_XtGeoTrace(widget, "\"%s\" returns XtGeometryAlmost.\n",
                                   (XtParent(widget)) ? XtName(XtParent(widget))
                                   : "Root"));
            CALLGEOTAT(_XtGeoTab(1));
            CALLGEOTAT(_XtGeoTrace(widget, "Proposal: width %d height %d.\n",
                                   (reply) ? reply->width : junk.width,
                                   (reply) ? reply->height : junk.height));
            CALLGEOTAT(_XtGeoTab(-1));

            /* check for no change */
            break;
        case XtGeometryYes:
            if (req.changeMask & XtCWQueryOnly) {
                CALLGEOTAT(_XtGeoTrace(widget,
                                       "QueryOnly specified, no configuration.\n"));
            }
            if (!XtIsRealized(widget)) {
                CALLGEOTAT(_XtGeoTrace(widget,
                                       "\"%s\" not realized, no configuration.\n",
                                       XtName(widget)));
            }
            CALLGEOTAT(_XtGeoTab(-1));
            CALLGEOTAT(_XtGeoTrace(widget, "\"%s\" returns XtGeometryYes.\n",
                                   (XtParent(widget)) ? XtName(XtParent(widget))
                                   : "Root"));
            break;
        }
#endif
        return returnCode;
    }

    CALLGEOTAT(_XtGeoTab(-1));
    CALLGEOTAT(_XtGeoTrace(widget, "\"%s\" returns XtGeometryYes.\n",
                           (XtParent(widget)) ? XtName(XtParent(widget)) :
                           "Root"));

    if (XtIsWidget(widget)) {   /* reconfigure the window (if needed) */

        if (rgm)
            return returnCode;

        if (req.changes_x != widget->core.x) {
            req.changeMask |= XCB_CONFIG_WINDOW_X;
            req.changes_x = widget->core.x;
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "x changing to %d\n", widget->core.x));
        }
        if (req.changes_y != widget->core.y) {
            req.changeMask |= XCB_CONFIG_WINDOW_Y;
            req.changes_y = widget->core.y;
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "y changing to %d\n", widget->core.y));
        }
        if (req.changes_w != widget->core.width) {
            req.changeMask |= XCB_CONFIG_WINDOW_WIDTH;
            req.changes_w = widget->core.width;
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "width changing to %d\n",
                                   widget->core.width));
        }
        if (req.changes_h != widget->core.height) {
            req.changeMask |= XCB_CONFIG_WINDOW_HEIGHT;
            req.changes_h = widget->core.height;
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "height changing to %d\n",
                                   widget->core.height));
        }
        if (req.changes_bw != widget->core.border_width) {
            req.changeMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
            req.changes_bw = widget->core.border_width;
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "border_width changing to %d\n",
                                   widget->core.border_width));
        }
        if (req.changeMask & XCB_CONFIG_WINDOW_STACK_MODE) {
            req.changes_sm = request->stack_mode;
            CALLGEOTAT(_XtGeoTrace(widget, "stack_mode changing\n"));
            if (req.changeMask & XCB_CONFIG_WINDOW_SIBLING) {
                if (XtIsWidget(request->sibling))
                    req.changes_sb = XtWindow(request->sibling);
                else
                    req.changeMask =
                        (XtGeometryMask) (req.changeMask & (unsigned long)
                                          (~(XCB_CONFIG_WINDOW_STACK_MODE | XCB_CONFIG_WINDOW_SIBLING)));
            }
        }

#ifdef XT_GEO_TATTLER
        if (req.changeMask) {
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "XConfigure \"%s\"'s window.\n",
                                   XtName(widget)));
        }
        else {
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "No window configuration needed for \"%s\".\n",
                                   XtName(widget)));
        }
#endif
        /* XCB requires values in ascending bit order, only for set bits */
        {
            uint32_t values[5];
            int vi = 0;
            if (req.changeMask & XCB_CONFIG_WINDOW_X)
                values[vi++] = (uint32_t)(int32_t)req.changes_x;
            if (req.changeMask & XCB_CONFIG_WINDOW_Y)
                values[vi++] = (uint32_t)(int32_t)req.changes_y;
            if (req.changeMask & XCB_CONFIG_WINDOW_WIDTH)
                values[vi++] = req.changes_w;
            if (req.changeMask & XCB_CONFIG_WINDOW_HEIGHT)
                values[vi++] = req.changes_h;
            if (req.changeMask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                values[vi++] = req.changes_bw;
            xcb_configure_window(XtDisplay(widget), XtWindow(widget), req.changeMask, values);
        }
        //  XConfigureWindow(XtDisplay(widget), XtWindow(widget),
        //                 req.changeMask, &req.changes);
    }
    else {                      /* RectObj child of realized Widget */
        *clear_rect_obj = TRUE;
        CALLGEOTAT(_XtGeoTrace(widget,
                               "ClearRectObj on \"%s\".\n", XtName(widget)));

        ClearRectObjAreas((RectObj) widget, req.changes_x, req.changes_y, req.changes_w, req.changes_h, req.changes_bw);
    }
    hookobj = XtHooksOfDisplay(XtDisplayOfObject(widget));
    if (XtHasCallbacks(hookobj, XtNconfigureHook) == XtCallbackHasSome) {
        req.type = XtHconfigure;
        req.widget = widget;
        XtCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.confighook_callbacks,
                           (XtPointer) &req);
    }

    return returnCode;
}                               /* _XtMakeGeometryRequest */

/* Public routines */

XtGeometryResult
XtMakeGeometryRequest(Widget widget,
                      XtWidgetGeometry *request,
                      XtWidgetGeometry *reply)
{
    Boolean junk;
    XtGeometryResult r;
    XtGeometryHookDataRec call_data;
    Widget hookobj = XtHooksOfDisplay(XtDisplayOfObject(widget));

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (XtHasCallbacks(hookobj, XtNgeometryHook) == XtCallbackHasSome) {
        call_data.type = XtHpreGeometry;
        call_data.widget = widget;
        call_data.request = request;
        XtCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (XtPointer) &call_data);
        call_data.result = r =
            _XtMakeGeometryRequest(widget, request, reply, &junk);
        call_data.type = XtHpostGeometry;
        call_data.reply = reply;
        XtCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (XtPointer) &call_data);
    }
    else {
        r = _XtMakeGeometryRequest(widget, request, reply, &junk);
    }
    UNLOCK_APP(app);

    return ((r == XtGeometryDone) ? XtGeometryYes : r);
}

XtGeometryResult
XtMakeResizeRequest(Widget widget,
                    _XtDimension width,
                    _XtDimension height,
                    Dimension *replyWidth,
                    Dimension *replyHeight)
{
    XtWidgetGeometry request, reply;
    XtGeometryResult r;
    XtGeometryHookDataRec call_data;
    Boolean junk;
    Widget hookobj = XtHooksOfDisplay(XtDisplayOfObject(widget));

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    memset(&request, 0, sizeof(request));
    request.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    request.width = (Dimension) width;
    request.height = (Dimension) height;

    if (XtHasCallbacks(hookobj, XtNgeometryHook) == XtCallbackHasSome) {
        call_data.type = XtHpreGeometry;
        call_data.widget = widget;
        call_data.request = &request;
        XtCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (XtPointer) &call_data);
        call_data.result = r =
            _XtMakeGeometryRequest(widget, &request, &reply, &junk);
        call_data.type = XtHpostGeometry;
        call_data.reply = &reply;
        XtCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.geometryhook_callbacks,
                           (XtPointer) &call_data);
    }
    else {
        r = _XtMakeGeometryRequest(widget, &request, &reply, &junk);
    }
    if (replyWidth != NULL) {
        if (r == XtGeometryAlmost && reply.request_mode & XCB_CONFIG_WINDOW_WIDTH )
            *replyWidth = reply.width;
        else
            *replyWidth = (Dimension) width;
    }
    if (replyHeight != NULL) {
        if (r == XtGeometryAlmost && reply.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            *replyHeight = reply.height;
        else
            *replyHeight = (Dimension) height;
    }
    UNLOCK_APP(app);
    return ((r == XtGeometryDone) ? XtGeometryYes : r);
}                               /* XtMakeResizeRequest */

void
XtResizeWindow(Widget w)
{
    XtConfigureHookDataRec req;

    WIDGET_TO_APPCON(w);

    LOCK_APP(app);
    if (XtIsRealized(w)) {
        Widget hookobj;

        req.changes_w = w->core.width;
        req.changes_h = w->core.height;
        req.changes_bw = w->core.border_width;
        req.changeMask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;
        /* XCB requires values in ascending bit order, only for set bits */
        {
            uint32_t values[3];
            int vi = 0;
            if (req.changeMask & XCB_CONFIG_WINDOW_WIDTH)
                values[vi++] = req.changes_w;
            if (req.changeMask & XCB_CONFIG_WINDOW_HEIGHT)
                values[vi++] = req.changes_h;
            if (req.changeMask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                values[vi++] = req.changes_bw;
            xcb_configure_window(XtDisplay(w), XtWindow(w), req.changeMask, values);
        }
        //XConfigureWindow(XtDisplay(w), XtWindow(w),
        //                 (unsigned) req.changeMask, &req.changes);
        hookobj = XtHooksOfDisplay(XtDisplayOfObject(w));
        if (XtHasCallbacks(hookobj, XtNconfigureHook) == XtCallbackHasSome) {
            req.type = XtHconfigure;
            req.widget = w;
            XtCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               confighook_callbacks, (XtPointer) &req);
        }
    }
    UNLOCK_APP(app);
}                               /* XtResizeWindow */

void
XtResizeWidget(Widget w,
               _XtDimension width,
               _XtDimension height,
               _XtDimension borderWidth)
{
    XtConfigureWidget(w, w->core.x, w->core.y, width, height, borderWidth);
}                               /* XtResizeWidget */

void
XtConfigureWidget(Widget w,
                  _XtPosition x,
                  _XtPosition y,
                  _XtDimension width,
                  _XtDimension height,
                  _XtDimension borderWidth)
{
    XtConfigureHookDataRec req;
    uint32_t old_x, old_y, old_h, old_w, old_bw;
    xcb_connection_t *dpy = w->core.display;

    WIDGET_TO_APPCON(w);

    CALLGEOTAT(_XtGeoTrace(w,
                           "\"%s\" is being configured by its parent \"%s\"\n",
                           XtName(w),
                           (XtParent(w)) ? XtName(XtParent(w)) : "Root"));
    CALLGEOTAT(_XtGeoTab(1));

    LOCK_APP(app);
    req.changeMask = 0;
    if ((old_x = w->core.x) != x) {
        CALLGEOTAT(_XtGeoTrace(w, "x move from %d to %d\n", w->core.x, x));
        req.changes_x = w->core.x = (Position) x;
        req.changeMask |= XCB_CONFIG_WINDOW_X;
    }

    if ((old_y = w->core.y) != y) {
        CALLGEOTAT(_XtGeoTrace(w, "y move from %d to %d\n", w->core.y, y));
        req.changes_y = w->core.y = (Position) y;
        req.changeMask |= XCB_CONFIG_WINDOW_Y;
    }

    if ((old_w = w->core.width) != width) {
        CALLGEOTAT(_XtGeoTrace(w,
                               "width move from %d to %d\n", w->core.width,
                               width));
        req.changes_w = w->core.width = (Dimension) width;
        req.changeMask |= XCB_CONFIG_WINDOW_WIDTH;
    }

    if ((old_h = w->core.height) != height) {
        CALLGEOTAT(_XtGeoTrace(w,
                               "height move from %d to %d\n", w->core.height,
                               height));
        req.changes_h = w->core.height = (Dimension) height;
        req.changeMask |= XCB_CONFIG_WINDOW_HEIGHT;
    }

    if ((old_bw = w->core.border_width) != borderWidth) {
        CALLGEOTAT(_XtGeoTrace(w, "border_width move from %d to %d\n",
                               w->core.border_width, borderWidth));
        req.changes_bw = w->core.border_width =
            (Dimension) borderWidth;
        req.changeMask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }

    if (req.changeMask != 0) {
        Widget hookobj;

        if (XtIsRealized(w)) {
            if (XtIsWidget(w)) {
                CALLGEOTAT(_XtGeoTrace(w,
                                       "XConfigure \"%s\"'s window\n",
                                       XtName(w)));

                /* XCB requires values in ascending bit order, only for set bits */
                {
                    uint32_t values[5];
                    int vi = 0;
                    if (req.changeMask & XCB_CONFIG_WINDOW_X)
                        values[vi++] = (uint32_t)(int32_t)req.changes_x;
                    if (req.changeMask & XCB_CONFIG_WINDOW_Y)
                        values[vi++] = (uint32_t)(int32_t)req.changes_y;
                    if (req.changeMask & XCB_CONFIG_WINDOW_WIDTH)
                        values[vi++] = req.changes_w;
                    if (req.changeMask & XCB_CONFIG_WINDOW_HEIGHT)
                        values[vi++] = req.changes_h;
                    if (req.changeMask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                        values[vi++] = req.changes_bw;
                    xcb_configure_window(dpy, XtWindow(w), req.changeMask, values);
                }
                //XConfigureWindow(XtDisplay(w), XtWindow(w), req.changeMask,
                //                 &req.changes);
            }
            else {
                CALLGEOTAT(_XtGeoTrace(w,
                                       "ClearRectObj called on \"%s\"\n",
                                       XtName(w)));
                ClearRectObjAreas((RectObj) w, old_x,old_y,old_w,old_h,old_bw);
            }
        }
        hookobj = XtHooksOfDisplay(XtDisplayOfObject(w));
        if (XtHasCallbacks(hookobj, XtNconfigureHook) == XtCallbackHasSome) {
            req.type = XtHconfigure;
            req.widget = w;
            XtCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               confighook_callbacks, (XtPointer) &req);
        }
        {
            XtWidgetProc resize;

            LOCK_PROCESS;
            resize = XtClass(w)->core_class.resize;
            UNLOCK_PROCESS;
            if ((req.changeMask & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)) &&
                resize != (XtWidgetProc) NULL) {
                CALLGEOTAT(_XtGeoTrace(w, "Resize proc is called.\n"));
                (*resize) (w);
            }
        }
    }
    else {
        CALLGEOTAT(_XtGeoTrace(w, "No change in configuration\n"));
    }

    CALLGEOTAT(_XtGeoTab(-1));
    UNLOCK_APP(app);
}                               /* XtConfigureWidget */

void
XtMoveWidget(Widget w, _XtPosition x, _XtPosition y)
{
    XtConfigureWidget(w, x, y, w->core.width, w->core.height,
                      w->core.border_width);
}                               /* XtMoveWidget */

void
XtTranslateCoords(register Widget w,
                  _XtPosition x,
                  _XtPosition y,
                  register Position *rootx, /* return */
                  register Position *rooty) /* return */
{
    Position garbagex, garbagey;
    XtAppContext app = XtWidgetToApplicationContext(w);

    LOCK_APP(app);
    if (rootx == NULL)
        rootx = &garbagex;
    if (rooty == NULL)
        rooty = &garbagey;

    *rootx = (Position) x;
    *rooty = (Position) y;

    for (; w != NULL && !XtIsShell(w); w = w->core.parent) {
        *rootx = (Position) (*rootx + w->core.x + w->core.border_width);
        *rooty = (Position) (*rooty + w->core.y + w->core.border_width);
    }

    if (w == NULL)
        XtAppWarningMsg(app,
                        "invalidShell", "xtTranslateCoords", XtCXtToolkitError,
                        "Widget has no shell ancestor", NULL, NULL);
    else {
        Position x2, y2;

        _XtShellGetCoordinates(w, &x2, &y2);
        *rootx = (Position) (*rootx + x2 + w->core.border_width);
        *rooty = (Position) (*rooty + y2 + w->core.border_width);
    }
    UNLOCK_APP(app);
}

XtGeometryResult XtQueryGeometry(Widget widget,
                                 register XtWidgetGeometry *intended, /* parent's changes; may be NULL */
                                 XtWidgetGeometry *reply) {    /* child's preferred geometry; never NULL */
    XtWidgetGeometry null_intended;
    XtGeometryHandler query;
    XtGeometryResult result;

    WIDGET_TO_APPCON(widget);

    CALLGEOTAT(_XtGeoTrace(widget,
                           "\"%s\" is asking its preferred geometry to \"%s\".\n",
                           (XtParent(widget)) ? XtName(XtParent(widget)) :
                           "Root", XtName(widget)));
    CALLGEOTAT(_XtGeoTab(1));

    LOCK_APP(app);
    LOCK_PROCESS;
    query = XtClass(widget)->core_class.query_geometry;
    UNLOCK_PROCESS;
    reply->request_mode = 0;
    if (query != NULL) {
        if (intended == NULL) {
            null_intended.request_mode = 0;
            intended = &null_intended;
#ifdef XT_GEO_TATTLER
            CALLGEOTAT(_XtGeoTrace(widget, "without any constraint.\n"));
        }
        else {
            CALLGEOTAT(_XtGeoTrace(widget,
                                   "with the following constraints:\n"));

            if (intended->request_mode & XCB_CONFIG_WINDOW_X) {
                CALLGEOTAT(_XtGeoTrace(widget, " x = %d\n", intended->x));
            }
            if (intended->request_mode & XCB_CONFIG_WINDOW_Y) {
                CALLGEOTAT(_XtGeoTrace(widget, " y = %d\n", intended->y));
            }
            if (intended->request_mode & XCB_CONFIG_WINDOW_WIDTH ) {
                CALLGEOTAT(_XtGeoTrace(widget,
                                       " width = %d\n", intended->width));
            }
            if (intended->request_mode & XCB_CONFIG_WINDOW_HEIGHT) {
                CALLGEOTAT(_XtGeoTrace(widget,
                                       " height = %d\n", intended->height));
            }
            if (intended->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
                CALLGEOTAT(_XtGeoTrace(widget,
                                       " border_width = %d\n",
                                       intended->border_width));
            }
#endif
        }

        result = (*query) (widget, intended, reply);
    }
    else {
        CALLGEOTAT(_XtGeoTrace
                   (widget,
                    "\"%s\" has no QueryGeometry proc, return the current state\n",
                    XtName(widget)));

        result = XtGeometryYes;
    }

#ifdef XT_GEO_TATTLER
#define FillIn(mask, field) \
        if (!(reply->request_mode & mask)) {\
              reply->field = widget->core.field;\
              _XtGeoTrace(widget," using core %s = %d.\n","field",\
                                                       widget->core.field);\
        } else {\
              _XtGeoTrace(widget," replied %s = %d\n","field",\
                                                   reply->field);\
        }
#else
#define FillIn(mask, field) \
        if (!(reply->request_mode & mask)) reply->field = widget->core.field;
#endif

    FillIn(XCB_CONFIG_WINDOW_X, x);
    FillIn(XCB_CONFIG_WINDOW_Y, y);
    FillIn(XCB_CONFIG_WINDOW_WIDTH , width);
    FillIn(XCB_CONFIG_WINDOW_HEIGHT, height);
    FillIn(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width);

    CALLGEOTAT(_XtGeoTab(-1));
#undef FillIn

    if (!(reply->request_mode &XCB_CONFIG_WINDOW_STACK_MODE))
        reply->stack_mode = XtSMDontChange;
    UNLOCK_APP(app);
    return result;
}
