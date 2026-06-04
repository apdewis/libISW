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

static _Xconst _IswString IswNinvalidChild = "invalidChild";
static _Xconst _IswString IswNxtUnmanageChildren = "xtUnmanageChildren";
static _Xconst _IswString IswNxtManageChildren = "xtManageChildren";
static _Xconst _IswString IswNxtChangeManagedSet = "xtChangeManagedSet";

static void
UnmanageChildren(WidgetList children,
                 Cardinal num_children,
                 Widget parent,
                 Cardinal *num_unique_children,
                 Boolean call_change_managed,
                 _Xconst _IswString caller_func)
{
    Widget child;
    Cardinal i;
    IswWidgetProc change_managed = NULL;
    Bool parent_realized = False;

    *num_unique_children = 0;

    if (IswIsComposite((Widget) parent)) {
        LOCK_PROCESS;
        change_managed = ((CompositeWidgetClass) parent->core.widget_class)
            ->composite_class.change_managed;
        UNLOCK_PROCESS;
        parent_realized = IswIsRealized((Widget) parent);
    }
    else {
        IswAppErrorMsg(IswWidgetToApplicationContext((Widget) parent),
                      "invalidParent", caller_func, IswCIswToolkitError,
                      "Attempt to unmanage a child when parent is not Composite",
                      NULL, NULL);
    }

    for (i = 0; i < num_children; i++) {
        child = children[i];
        if (child == NULL) {
            IswAppWarningMsg(IswWidgetToApplicationContext(parent),
                            IswNinvalidChild, caller_func, IswCIswToolkitError,
                            "Null child passed to IswUnmanageChildren",
                            NULL, NULL);
            return;
        }
        if (child->core.parent != parent) {
            IswAppWarningMsg(IswWidgetToApplicationContext(parent),
                            "ambiguousParent", caller_func, IswCIswToolkitError,
                            "Not all children have same parent in UnmanageChildren",
                            NULL, NULL);
        }
        else if (child->core.managed) {
            (*num_unique_children)++;
            CALLGEOTAT(_IswGeoTrace(child, "Child \"%s\" is marked unmanaged\n",
                                   IswName(child)));
            child->core.managed = FALSE;
            if (IswIsWidget(child) && child->core.windowless) {
                /* Windowless: unmanaging hides the widget, mirroring how
                   unmanaging a windowed widget unmaps its window.  IswUnmapWidget
                   clears windowless_mapped and recomposites the ancestor so the
                   widget's pixels are erased.  Unconditional (not gated on
                   mapped_when_managed) — a shown widget must be hidden when
                   unmanaged; if it was already hidden the unmap is a no-op. */
                if (IswIsRealized(child))
                    IswUnmapWidget(child);
            } else if (IswIsWidget(child)
                && IswIsRealized(child)
                && child->core.mapped_when_managed) {
                IswUnmapWidget(child);
            } else {              /* RectObj child */
                Widget pw = child->core.parent;
                RectObj r = (RectObj) child;

                while ((pw != NULL) && (!IswIsWidget(pw))) {
                    pw = pw->core.parent;
                }

                if ((pw != NULL) && IswIsRealized(pw)) {
                    xcb_clear_area(
                        IswDisplay(pw), 
                        0,  // exposure flag (0 = no exposure)
                        IswWindow(pw),
                        r->rectangle.x, 
                        r->rectangle.y,
                        r->rectangle.width + (r->rectangle.border_width << 1),
                        r->rectangle.height + (r->rectangle.border_width << 1)
                    );
                    xcb_flush(IswDisplay(pw));
                }
            }

        }
    }

    if (call_change_managed && *num_unique_children != 0 &&
        change_managed != NULL && parent_realized) {
        CALLGEOTAT(_IswGeoTrace((Widget) parent,
                               "Call parent: \"%s\"[%d,%d]'s changemanaged proc\n",
                               IswName((Widget) parent),
                               parent->core.width, parent->core.height));
        (*change_managed) (parent);
    }
}                               /* UnmanageChildren */

void
IswUnmanageChildren(WidgetList children, Cardinal num_children)
{
    Widget parent, hookobj;
    Cardinal ii;

#ifdef XTHREADS
    IswAppContext app;
#endif

    if (num_children == 0)
        return;
    if (children[0] == NULL) {
        IswWarningMsg(IswNinvalidChild, IswNxtUnmanageChildren, IswCIswToolkitError,
                     "Null child found in argument list to unmanage",
                     NULL, NULL);
        return;
    }
#ifdef XTHREADS
    app = IswWidgetToApplicationContext(children[0]);
#endif
    LOCK_APP(app);
    parent = children[0]->core.parent;
    if (parent->core.being_destroyed) {
        UNLOCK_APP(app);
        return;
    }
    UnmanageChildren(children, num_children, parent, &ii,
                     (Boolean) True, IswNxtUnmanageChildren);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(children[0]));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHunmanageChildren;
        call_data.widget = parent;
        call_data.event_data = (IswPointer) children;
        call_data.num_event_data = num_children;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswUnmanageChildren */

void
IswUnmanageChild(Widget child)
{
    IswUnmanageChildren(&child, (Cardinal) 1);
}                               /* IswUnmanageChild */

static void
ManageChildren(WidgetList children,
               Cardinal num_children,
               Widget parent,
               Boolean call_change_managed,
               _Xconst _IswString caller_func)
{
#define MAXCHILDREN 100
    Widget child;
    Cardinal num_unique_children, i;
    IswWidgetProc change_managed = NULL;
    WidgetList unique_children;
    Widget cache[MAXCHILDREN];
    Bool parent_realized = False;

    if (IswIsComposite((Widget) parent)) {
        LOCK_PROCESS;
        change_managed = ((CompositeWidgetClass) parent->core.widget_class)
            ->composite_class.change_managed;
        UNLOCK_PROCESS;
        parent_realized = IswIsRealized((Widget) parent);
    }
    else {
        IswAppErrorMsg(IswWidgetToApplicationContext((Widget) parent),
                      "invalidParent", caller_func, IswCIswToolkitError,
                      "Attempt to manage a child when parent is not Composite",
                      NULL, NULL);
    }

    /* Construct new list of children that really need to be operated upon. */
    if (num_children <= MAXCHILDREN) {
        unique_children = cache;
    }
    else {
        unique_children = IswMallocArray(num_children, (Cardinal) sizeof(Widget));
    }
    num_unique_children = 0;
    for (i = 0; i < num_children; i++) {
        child = children[i];
        if (child == NULL) {
            IswAppWarningMsg(IswWidgetToApplicationContext((Widget) parent),
                            IswNinvalidChild, caller_func, IswCIswToolkitError,
                            "null child passed to ManageChildren", NULL, NULL);
            if (unique_children != cache)
                IswFree((char *) unique_children);
            return;
        }
#ifdef DEBUG
        if (!IswIsRectObj(child)) {
            String params[2];
            Cardinal num_params = 2;

            params[0] = IswName(child);
            params[1] = child->core.widget_class->core_class.class_name;
            IswAppWarningMsg(IswWidgetToApplicationContext((Widget) parent),
                            "notRectObj", caller_func, IswCIswToolkitError,
                            "child \"%s\", class %s is not a RectObj",
                            params, &num_params);
            continue;
        }
#endif   /*DEBUG*/
            if (child->core.parent != parent) {
            IswAppWarningMsg(IswWidgetToApplicationContext((Widget) parent),
                            "ambiguousParent", caller_func, IswCIswToolkitError,
                            "Not all children have same parent in IswManageChildren",
                            NULL, NULL);
        }
        else if (!child->core.managed && !child->core.being_destroyed) {
            unique_children[num_unique_children++] = child;
            CALLGEOTAT(_IswGeoTrace(child,
                                   "Child \"%s\"[%d,%d] is marked managed\n",
                                   IswName(child),
                                   child->core.width, child->core.height));
            child->core.managed = TRUE;
        }
    }

    if ((call_change_managed || num_unique_children != 0) && parent_realized) {
        /* Compute geometry of new managed set of children. */
        if (change_managed != NULL) {
            CALLGEOTAT(_IswGeoTrace((Widget) parent,
                                   "Call parent: \"%s\"[%d,%d]'s changemanaged\n",
                                   IswName((Widget) parent),
                                   parent->core.width, parent->core.height));
            (*change_managed) ((Widget) parent);
        }

        /* Realize each child if necessary, then map if necessary */
        for (i = 0; i < num_unique_children; i++) {
            child = unique_children[i];
            if (IswIsWidget(child)) {
                if (!IswIsRealized(child))
                    IswRealizeWidget(child);
                if (child->core.mapped_when_managed)
                    IswMapWidget(child);
            }
            else {              /* RectObj child */
                Widget pw = child->core.parent;
                RectObj r = (RectObj) child;

                while ((pw != NULL) && (!IswIsWidget(pw)))
                    pw = pw->core.parent;
                if (pw != NULL) {
                    xcb_clear_area(
                        IswDisplay(pw), 
                        0,  // exposure flag (0 = no exposure)
                        IswWindow(pw),
                        r->rectangle.x, 
                        r->rectangle.y,
                        r->rectangle.width + (r->rectangle.border_width << 1),
                        r->rectangle.height + (r->rectangle.border_width << 1)
                    );
                    xcb_flush(IswDisplay(pw));
                }
            }
        }
    }

    if (unique_children != cache)
        IswFree((char *) unique_children);
}                               /* ManageChildren */

void
IswManageChildren(WidgetList children, Cardinal num_children)
{
    Widget parent, hookobj;

#ifdef XTHREADS
    IswAppContext app;
#endif

    if (num_children == 0)
        return;
    if (children[0] == NULL) {
        IswWarningMsg(IswNinvalidChild, IswNxtManageChildren, IswCIswToolkitError,
                     "null child passed to IswManageChildren", NULL, NULL);
        return;
    }
#ifdef XTHREADS
    app = IswWidgetToApplicationContext(children[0]);
#endif
    LOCK_APP(app);
    parent = children[0]->core.parent;
    if (parent->core.being_destroyed) {
        UNLOCK_APP(app);
        return;
    }
    ManageChildren(children, num_children, parent, (Boolean) False,
                   IswNxtManageChildren);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(children[0]));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHmanageChildren;
        call_data.widget = parent;
        call_data.event_data = (IswPointer) children;
        call_data.num_event_data = num_children;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswManageChildren */

void
IswManageChild(Widget child)
{
    IswManageChildren(&child, (Cardinal) 1);
}                               /* IswManageChild */

void
IswSetMappedWhenManaged(Widget widget, _IswBoolean mapped_when_managed)
{
    Widget hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (widget->core.mapped_when_managed == mapped_when_managed) {
        UNLOCK_APP(app);
        return;
    }
    widget->core.mapped_when_managed = (Boolean) mapped_when_managed;

    hookobj = IswHooksOfDisplay(IswDisplay(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHsetMappedWhenManaged;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) (IswUIntPtr) mapped_when_managed;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }

    if (!IswIsManaged(widget)) {
        UNLOCK_APP(app);
        return;
    }

    if (mapped_when_managed) {
        /* Didn't used to be mapped when managed.               */
        if (IswIsRealized(widget))
            IswMapWidget(widget);
    }
    else {
        /* Used to be mapped when managed.                      */
        if (IswIsRealized(widget))
            IswUnmapWidget(widget);
    }
    UNLOCK_APP(app);
}                               /* IswSetMappedWhenManaged */

void
IswChangeManagedSet(WidgetList unmanage_children,
                   Cardinal num_unmanage,
                   IswDoChangeProc do_change_proc,
                   IswPointer client_data,
                   WidgetList manage_children,
                   Cardinal num_manage)
{
    WidgetList childp;
    Widget parent;
    int i;
    Cardinal some_unmanaged;
    Boolean call_out;
    IswAppContext app;
    Widget hookobj;
    IswChangeHookDataRec call_data;

    if (num_unmanage == 0 && num_manage == 0)
        return;

    /* specification doesn't state that library will check for NULL in list */

    childp = num_unmanage ? unmanage_children : manage_children;
    app = IswWidgetToApplicationContext(*childp);
    LOCK_APP(app);

    parent = IswParent(*childp);
    childp = unmanage_children;
    for (i = (int) num_unmanage; --i >= 0 && IswParent(*childp) == parent;
         childp++);
    call_out = (i >= 0);
    childp = manage_children;
    for (i = (int) num_manage; --i >= 0 && IswParent(*childp) == parent;
         childp++);
    if (call_out || i >= 0) {
        IswAppWarningMsg(app, "ambiguousParent", IswNxtChangeManagedSet,
                        IswCIswToolkitError, "Not all children have same parent",
                        NULL, NULL);
    }
    if (!IswIsComposite(parent)) {
        UNLOCK_APP(app);
        IswAppErrorMsg(app, "invalidParent", IswNxtChangeManagedSet,
                      IswCIswToolkitError,
                      "Attempt to manage a child when parent is not Composite",
                      NULL, NULL);
    }
    if (parent->core.being_destroyed) {
        UNLOCK_APP(app);
        return;
    }

    call_out = False;
    if (do_change_proc) {
        CompositeClassExtension ext = (CompositeClassExtension)
            IswGetClassExtension(parent->core.widget_class,
                                IswOffsetOf(CompositeClassRec,
                                           composite_class.extension),
                                NULLQUARK, IswCompositeExtensionVersion,
                                sizeof(CompositeClassExtensionRec));

        if (!ext || !ext->allows_change_managed_set)
            call_out = True;
    }

    UnmanageChildren(unmanage_children, num_unmanage, parent,
                     &some_unmanaged, call_out, IswNxtChangeManagedSet);

    hookobj = IswHooksOfDisplay(IswDisplay(parent));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        call_data.type = IswHunmanageSet;
        call_data.widget = parent;
        call_data.event_data = (IswPointer) unmanage_children;
        call_data.num_event_data = num_unmanage;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }

    if (do_change_proc)
        (*do_change_proc) (parent, unmanage_children, &num_unmanage,
                           manage_children, &num_manage, client_data);

    call_out = (some_unmanaged && !call_out);
    ManageChildren(manage_children, num_manage, parent, call_out,
                   IswNxtChangeManagedSet);

    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        call_data.type = IswHmanageSet;
        call_data.event_data = (IswPointer) manage_children;
        call_data.num_event_data = num_manage;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswChangeManagedSet */
