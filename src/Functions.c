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
#include <ISW/ISWPlatform.h>

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
    /* Windowless widgets have no X window to map.  windowless_mapped is the
       live equivalent of "the window is mapped" — the shown state the
       composite/paint/hit-test walks gate on.  Set it (mirroring xcb_map_window
       on a real window) and re-composite the windowed ancestor so the now-shown
       widget appears.  Mapping the shared ancestor window here would be wrong. */
    if (IswIsWidget(w)) {
        w->core.windowless_mapped = True;
        /* The app has explicitly mapped this widget; clear any prior explicit
           unmap so the realize-time map pass no longer suppresses it. */
        w->core.windowless_unmapped_explicit = False;
        /* Paint the now-shown subtree (it may never have been drawn while
           hidden) and composite it up — the composite pass folds surfaces but
           does not itself drive expose. */
        _IswRepaintWindowless(w);

        /* A shell backs the platform's real top-level window: map it so the
           composited surface becomes visible.  Pure-surface widgets have no
           window of their own — the flag above is all they need. */
        if (IswIsShell(w)) {
            _IswPlatformMapWindow(IswDisplayOf(w),
                                  _IswPlatformWidgetWindow(IswDisplayOf(w), w));
            _IswPlatformFlush(IswDisplayOf(w));
        }
        UNLOCK_APP(app);
        return;
    }
    hookobj = IswHooksOfDisplay(IswDisplayOf(w));
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
    /* Windowless: clear the live shown flag (mirroring xcb_unmap_window) and
       re-composite so the now-hidden widget stops contributing pixels.
       Unmapping the shared ancestor window would hide the whole window. */
    if (IswIsWidget(w) && w->core.windowless) {
        Widget anc;
        w->core.windowless_mapped = False;
        /* Record an explicit unmap so a later realize does not auto-map this
           widget.  A windowed widget unmapped before realize is genuinely lost
           (no window yet), but a windowless widget exists pre-realize, so the
           app's intent to keep it hidden must survive realize — matching how a
           windowed widget kept unmapped stays off-screen. */
        if (!IswIsRealized(w))
            w->core.windowless_unmapped_explicit = True;
        /* The widget's pixels are now vacated from its parent's persisted
           composite surface; force the parent chain to re-expose so the
           background is repainted over the hole on the next fold. */
        _ISWRenderMarkDirtyChain(w->core.parent);
        anc = _IswWidgetAncestor(w);
        if (anc != NULL && IswIsRealized(anc))
            ISWRenderRequestComposite(anc);
        UNLOCK_APP(app);
        return;
    }
    _IswPlatformUnmapWindow(IswDisplayOf(w),
                            _IswPlatformWidgetWindow(IswDisplayOf(w), w));
    _IswPlatformFlush(IswDisplayOf(w));
    hookobj = IswHooksOfDisplay(IswDisplayOf(w));
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
ReloadSubtree(Widget w, IswDatabaseHandle db)
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
    IswDatabaseHandle db;

    if (subtree_root == NULL)
        return;

    IswReloadScreenDatabase(IswScreenOfObject(subtree_root));
    db = IswScreenDatabase(IswScreenOfObject(subtree_root));
    if (db == NULL)
        return;
    ReloadSubtree(subtree_root, db);
    RedisplaySubtree(subtree_root);
    if (IswIsRealized(subtree_root))
        _IswPlatformFlush(IswDisplayOf(subtree_root));
}

#undef IswNewString
String
IswNewString(String str)
{
    if (str == NULL)
        return NULL;

    return strdup(str);
}

/*
 * ISWCopyISOLatin1Lowered - copy `src` to `dst`, lowercasing ISO Latin-1.
 *
 * Neutral string utility (replacement for libXmu's XmuCopyISOLatin1Lowered);
 * pure character arithmetic, no platform coupling.  Uppercase ranges:
 *   0x41-0x5A (A-Z)          -> 0x61-0x7A (a-z)
 *   0xC0-0xDE accented       -> 0xE0-0xFE   (except 0xD7, the multiply sign)
 */
void
ISWCopyISOLatin1Lowered(char *dst, const char *src)
{
    unsigned char c;

    if (!dst || !src)
        return;

    while ((c = (unsigned char) *src++) != '\0') {
        if (c >= 0x41 && c <= 0x5A)
            c += 0x20;
        else if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
            c += 0x20;
        *dst++ = (char) c;
    }
    *dst = '\0';
}

/*
 * ISWCompareISOLatin1 - case-insensitive comparison of two strings.
 *
 * Neutral string utility (ASCII case folding); returns <0/0/>0 like strcmp.
 */
int
ISWCompareISOLatin1(const char *first, const char *second)
{
    const unsigned char *p1 = (const unsigned char *) first;
    const unsigned char *p2 = (const unsigned char *) second;
    unsigned char c1, c2;

    while (*p1 && *p2) {
        c1 = *p1;
        c2 = *p2;
        if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 += 'a' - 'A';
        if (c1 != c2)
            return c1 - c2;
        p1++;
        p2++;
    }

    return *p1 - *p2;
}
