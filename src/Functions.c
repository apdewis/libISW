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

/*

Copyright 1985, 1986, 1987, 1988, 1989, 1994, 1998  The Open Group

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
#include <ISW/Shell.h>
#include <ISW/Vendor.h>
#include <ISW/ISWRender.h>
#include <ISW/EventI.h>

/*
 * This file defines functional equivalents to all macros defined
 * in Intrinsic.h
 *
 */

#undef IswIsRectObj
Boolean
IswIsRectObj(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x02);
}

#undef IswIsWidget
Boolean
IswIsWidget(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x04);
}

#undef IswIsComposite
Boolean
IswIsComposite(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x08);
}

#undef IswIsConstraint
Boolean
IswIsConstraint(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x10);
}

#undef IswIsShell
Boolean
IswIsShell(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x20);
}

#undef IswIsOverrideShell
Boolean
IswIsOverrideShell(Widget object)
{
    return _IswIsSubclassOf(object, (WidgetClass) overrideShellWidgetClass,
                           (WidgetClass) shellWidgetClass, 0x20);
}

#undef IswIsWMShell
Boolean
IswIsWMShell(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x40);
}

#undef IswIsVendorShell
Boolean
IswIsVendorShell(Widget object)
{
    Boolean retval;

    LOCK_PROCESS;
    retval = _IswIsSubclassOf(object,
#ifdef notdef
/*
 * We don't refer to vendorShell directly, because some shared libraries
 * bind local references tightly.
 */
                             (WidgetClass) vendorShellWidgetClass,
#endif
                             transientShellWidgetClass->core_class.superclass,
                             (WidgetClass) wmShellWidgetClass, 0x40);
    UNLOCK_PROCESS;
    return retval;
}

#undef IswIsTransientShell
Boolean
IswIsTransientShell(Widget object)
{
    return _IswIsSubclassOf(object, (WidgetClass) transientShellWidgetClass,
                           (WidgetClass) wmShellWidgetClass, 0x40);
}

#undef IswIsTopLevelShell
Boolean
IswIsTopLevelShell(Widget object)
{
    return _IswCheckSubclassFlag(object, 0x80);
}

#undef IswIsApplicationShell
Boolean
IswIsApplicationShell(Widget object)
{
    return _IswIsSubclassOf(object, (WidgetClass) applicationShellWidgetClass,
                           (WidgetClass) topLevelShellWidgetClass, 0x80);
}

#undef IswMapWidget
void
IswMapWidget(Widget w)
{
    Widget hookobj;

    WIDGET_TO_APPCON(w);

    LOCK_APP(app);
    /* Windowless widgets have no X window to map.  mapped_when_managed is their
       live "is shown" flag (consulted by the composite/paint/hit-test walks):
       set it and re-composite the windowed ancestor so the now-shown widget
       appears.  Mapping the shared ancestor window here would be wrong. */
    if (IswIsWidget(w) && w->core.windowless) {
        w->core.mapped_when_managed = True;
        /* Paint the now-shown subtree (it may never have been drawn while
           hidden) and composite it up — the composite pass folds surfaces but
           does not itself drive expose. */
        _IswRepaintWindowless(w);
        UNLOCK_APP(app);
        return;
    }
    xcb_map_window(IswDisplay(w), IswWindow(w));
    xcb_flush(IswDisplay(w));
    hookobj = IswHooksOfDisplay(IswDisplay(w));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHmapWidget;
        call_data.widget = w;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}

#undef IswUnmapWidget
void
IswUnmapWidget(Widget w)
{
    Widget hookobj;

    WIDGET_TO_APPCON(w);

    LOCK_APP(app);
    /* Windowless: clear the live "is shown" flag and re-composite so the now-
       hidden widget stops contributing pixels.  Unmapping the shared ancestor
       window would hide the whole window. */
    if (IswIsWidget(w) && w->core.windowless) {
        Widget anc;
        w->core.mapped_when_managed = False;
        anc = _IswWindowedAncestor(w);
        if (anc != NULL && IswIsRealized(anc))
            ISWRenderRequestComposite(anc);
        UNLOCK_APP(app);
        return;
    }
    xcb_unmap_window(IswDisplay(w), IswWindow(w));
    xcb_flush(IswDisplay(w));
    hookobj = IswHooksOfDisplay(IswDisplay(w));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHunmapWidget;
        call_data.widget = w;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}

static void
ReloadSubtree(Widget w, xcb_xrm_database_t *db)
{
    _IswRefetchResources(w, db);

    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            ReloadSubtree(cw->composite.children[i], db);
    }
}

static void
RedisplaySubtree(Widget w)
{
    if (IswIsRealized(w) && IswIsWidget(w)) {
        IswExposeProc expose;

        LOCK_PROCESS;
        expose = IswClass(w)->core_class.expose;
        UNLOCK_PROCESS;
        if (expose)
            (*expose)(w, NULL, 0);
    }
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        Cardinal i;
        for (i = 0; i < cw->composite.num_children; i++)
            RedisplaySubtree(cw->composite.children[i]);
    }
}

void
IswReloadResources(Widget subtree_root)
{
    xcb_xrm_database_t *db;

    if (subtree_root == NULL)
        return;

    IswReloadScreenDatabase(IswScreenOfObject(subtree_root));
    db = IswScreenDatabase(IswScreenOfObject(subtree_root));
    if (db == NULL)
        return;
    ReloadSubtree(subtree_root, db);
    RedisplaySubtree(subtree_root);
    if (IswIsRealized(subtree_root))
        xcb_flush(IswDisplay(subtree_root));
}

#undef IswNewString
String
IswNewString(String str)
{
    if (str == NULL)
        return NULL;

    return strdup(str);
}
