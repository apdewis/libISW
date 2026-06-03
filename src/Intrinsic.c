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

#define INTRINSIC_C

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "VarargsI.h"           /* for geoTattler */
#include <sys/stat.h>
#ifdef WIN32
#include <direct.h>             /* for _getdrives() */
#endif

#include <stdio.h>
#include <stdlib.h>


String IswCIswToolkitError = "IswToolkitError";

Boolean
IswIsSubclass(Widget widget, WidgetClass myWidgetClass)
{
    register WidgetClass w;
    Boolean retval = FALSE;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    for (w = widget->core.widget_class; w != NULL; w = w->core_class.superclass)
        if (w == myWidgetClass) {
            retval = TRUE;
            break;
        }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return retval;
}                               /* IswIsSubclass */

Boolean
_IswCheckSubclassFlag(Widget object, _IswEnum flag)
{
    Boolean retval;

    LOCK_PROCESS;
    if (object->core.widget_class->core_class.class_inited & flag)
        retval = TRUE;
    else
        retval = FALSE;
    UNLOCK_PROCESS;
    return retval;
} /*_IswVerifySubclass */

Boolean
_IswIsSubclassOf(Widget object,
                WidgetClass myWidgetClass,
                WidgetClass superClass,
                _IswEnum flag)
{
    LOCK_PROCESS;
    if (!(object->core.widget_class->core_class.class_inited & flag)) {
        UNLOCK_PROCESS;
        return False;
    }
    else {
        register WidgetClass c = object->core.widget_class;

        while (c != superClass) {
            if (c == myWidgetClass) {
                UNLOCK_PROCESS;
                return True;
            }
            c = c->core_class.superclass;
        }
        UNLOCK_PROCESS;
        return False;
    }
} /*_IswIsSubclassOf */

IswPointer
IswGetClassExtension(WidgetClass object_class,
                    Cardinal byte_offset,
                    XrmQuark type, long version, Cardinal record_size)
{
    ObjectClassExtension ext;

    LOCK_PROCESS;

    ext = *(ObjectClassExtension *) ((char *) object_class + byte_offset);
    while (ext && (ext->record_type != type || ext->version < version
                   || ext->record_size < record_size)) {
        ext = (ObjectClassExtension) ext->next_extension;
    }

    UNLOCK_PROCESS;
    return (IswPointer) ext;
}

//#TODO, had LLM rework this for me, verify function
static void
ComputeWindowAttributes(Widget widget,
                        uint64_t *value_mask,
                        uint32_t *values)
{
    uint32_t mask = 0;
    uint32_t value_index = 0;

    /* IMPORTANT: XCB requires value_list entries to be in ascending bit-order
     * of the value_mask (lowest bit first).  The order here must match the
     * XCB_CW_* bit positions exactly. */

    /* XCB_CW_BACK_PIXMAP (bit 0) or XCB_CW_BACK_PIXEL (bit 1) */
    if (widget->core.background_pixmap != IswUnspecifiedPixmap) {
        mask |= XCB_CW_BACK_PIXMAP;
        values[value_index++] = widget->core.background_pixmap;
    }
    else {
        mask |= XCB_CW_BACK_PIXEL;
        values[value_index++] = widget->core.background_pixel;
    }

    /* XCB_CW_BORDER_PIXMAP (bit 2) or XCB_CW_BORDER_PIXEL (bit 3) */
    if (widget->core.border_pixmap != IswUnspecifiedPixmap) {
        mask |= XCB_CW_BORDER_PIXMAP;
        values[value_index++] = widget->core.border_pixmap;
    }
    else {
        mask |= XCB_CW_BORDER_PIXEL;
        values[value_index++] = widget->core.border_pixel;
    }

    /* XCB_CW_BIT_GRAVITY (bit 4)
     *
     * Original Xt only set NorthWest gravity for widgets without an expose
     * handler, leaving widgets with expose handlers at ForgetGravity (the
     * server default).  ForgetGravity discards all window content on any
     * geometry change and generates a full-window expose, which causes
     * visible flicker during scrolling — the Viewport moves its child
     * window, the server clears it, and every descendant repaints from
     * scratch.
     *
     * NorthWest gravity tells the server to preserve existing pixels on
     * geometry changes and only expose newly-revealed regions.  For
     * position-only moves (scrolling) this eliminates the
     * clear-then-redraw flash entirely.  For resizes, the resize proc
     * already handles the layout update, and partial exposes are handled
     * correctly by ISWRender's back-buffer seeding (cairo_xcb_begin copies
     * the current window surface into the back buffer before drawing).
     */
    mask |= XCB_CW_BIT_GRAVITY;
    values[value_index++] = XCB_GRAVITY_NORTH_WEST;

    /* XCB_CW_EVENT_MASK (bit 11) */
    mask |= XCB_CW_EVENT_MASK;
    {
        uint32_t evmask = IswBuildEventMask(widget);
        values[value_index++] = evmask;
    }

    /* XCB_CW_COLORMAP (bit 13) */
    mask |= XCB_CW_COLORMAP;
    values[value_index++] = widget->core.colormap;

    *value_mask = mask;
}                               /* ComputeWindowAttributes */

static void
CallChangeManaged(register Widget widget)
{
    register Cardinal i;
    IswWidgetProc change_managed;
    register WidgetList children;
    int managed_children = 0;

    register CompositePtr cpPtr;
    register CompositePartPtr clPtr;

    if (IswIsComposite(widget)) {
        cpPtr = (CompositePtr) &((CompositeWidget) widget)->composite;
        clPtr = (CompositePartPtr) &((CompositeWidgetClass)
                                      widget->core.
                                      widget_class)->composite_class;
    }
    else
        return;

    children = cpPtr->children;
    LOCK_PROCESS;
    change_managed = clPtr->change_managed;
    UNLOCK_PROCESS;

    /* CallChangeManaged for all children */
    for (i = cpPtr->num_children; i != 0; --i) {
        CallChangeManaged(children[i - 1]);
        if (IswIsManaged(children[i - 1]))
            managed_children++;
    }

    if (change_managed != NULL && managed_children != 0) {
        CALLGEOTAT(_IswGeoTrace(widget, "Call \"%s\"[%d,%d]'s changemanaged\n",
                               IswName(widget),
                               widget->core.width, widget->core.height));
        (*change_managed) (widget);
    }
}                               /* CallChangeManaged */

static void
MapChildren(CompositePart *cwp)
{
    Cardinal i;
    WidgetList children;

    children = cwp->children;
    for (i = 0; i < cwp->num_children; i++) {
        Widget child = children[i];

        if (IswIsWidget(child)) {
            if (child->core.managed && child->core.mapped_when_managed) {
                IswMapWidget(children[i]);
            }
        }
    }
}                               /* MapChildren */

static Boolean
ShouldMapAllChildren(CompositePart *cwp)
{
    Cardinal i;
    WidgetList children;

    children = cwp->children;
    for (i = 0; i < cwp->num_children; i++) {
        Widget child = children[i];

        if (IswIsWidget(child)) {
            if (IswIsRealized(child) && (!(child->core.managed
                                          && child->core.
                                          mapped_when_managed))) {
                return False;
            }
        }
    }

    return True;
}                               /* ShouldMapAllChildren */

static void
RealizeWidget(Widget widget)
{
    IswValueMask value_mask;
    uint32_t values[32];
    IswRealizeProc realize;
    xcb_window_t window;
    xcb_connection_t *display;
    String class_name;
    Widget hookobj;

    if (!IswIsWidget(widget) || IswIsRealized(widget))
        return;
    display = IswDisplay(widget);

    _IswInstallTranslations(widget);

    ComputeWindowAttributes(widget, &value_mask, values);
    LOCK_PROCESS;
    realize = widget->core.widget_class->core_class.realize;
    class_name = widget->core.widget_class->core_class.class_name;
    UNLOCK_PROCESS;
    if (realize == NULL)
        IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                      "invalidProcedure", "realizeProc", IswCIswToolkitError,
                      "No realize class procedure defined", NULL, NULL);
    else {
        CALLGEOTAT(_IswGeoTrace(widget, "Call \"%s\"[%d,%d]'s realize proc\n",
                               IswName(widget),
                               widget->core.width, widget->core.height));
        (*realize) (display, widget, &value_mask, values);
    }
    /* Own window (None for windowless); used for drawable registration
       and the optional window-identify property below. */
    window = widget->core.window;
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHrealizeWidget;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
#ifndef NO_IDENTIFY_WINDOWS
    if (_IswGetPerDisplay(display)->appContext->identify_windows) {
        int len_nm, len_cl;
        char *s;
        xcb_intern_atom_cookie_t cookie;
        xcb_intern_atom_reply_t *reply;
        xcb_atom_t xa_string;

        len_nm = widget->core.name ? (int) strlen(widget->core.name) : 0;
        len_cl = (int) strlen(class_name);
        s = __XtMalloc((unsigned) (len_nm + len_cl + 2));
        s[0] = '\0';
        if (len_nm)
            strcpy(s, widget->core.name);
        strcpy(s + len_nm + 1, class_name);
        
        /* Get the _MIT_OBJ_CLASS atom */
        cookie = xcb_intern_atom(display, 0, strlen("_MIT_OBJ_CLASS"), "_MIT_OBJ_CLASS");
        reply = xcb_intern_atom_reply(display, cookie, NULL);
        
        /* Get XCB_ATOM_STRING atom (predefined as 31 in X11) */
        xa_string = 31;
        
        if (reply) {
            xcb_change_property(display, XCB_PROP_MODE_REPLACE, window,
                              reply->atom, xa_string, 8,
                              len_nm + len_cl + 2, (const void *) s);
            free(reply);
        }
        IswFree(s);
    }
#endif
#ifdef notdef
    _IswRegisterAsyncHandlers(widget);
#endif
    /* (re)register any grabs extant in the translations */
    _IswRegisterGrabs(widget);
    /* reregister any grabs added with IswGrab{Button,Key} */
    _IswRegisterPassiveGrabs(widget);
    /* Windowless widgets share their ancestor's window; do not register
       the ancestor window as mapping to this widget (the ancestor already
       owns that mapping).  Mark realized via the windowless flag instead. */
    if (widget->core.windowless)
        widget->core.windowless_realized = True;
    else
        IswRegisterDrawable(display, window, widget);

    _IswExtensionSelect(widget);

    if (IswIsComposite(widget)) {
        Cardinal i;
        CompositePart *cwp = &(((CompositeWidget) widget)->composite);
        WidgetList children = cwp->children;

        /* Realize all children */
        for (i = cwp->num_children; i != 0; --i) {
            RealizeWidget(children[i - 1]);
        }
        /* Map children that are managed and mapped_when_managed */

        if (cwp->num_children != 0) {
            if (ShouldMapAllChildren(cwp)) {
                xcb_map_subwindows(display, window);
            }
            else {
                MapChildren(cwp);
            }
        }
    }

    /* If this is the application's popup shell, map it */
    if (widget->core.parent == NULL && widget->core.mapped_when_managed) {
        IswMapWidget(widget);
    }
}                               /* RealizeWidget */

void
IswRealizeWidget(Widget widget)
{
    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (IswIsRealized(widget)) {
        UNLOCK_APP(app);
        return;
    }
    CallChangeManaged(widget);
    RealizeWidget(widget);
    UNLOCK_APP(app);
}                               /* IswRealizeWidget */

static void
UnrealizeWidget(Widget widget)
{
    CompositeWidget cw;

    if (!IswIsWidget(widget) || !IswIsRealized(widget))
        return;

    /* If this is the application's popup shell, unmap it? */
    /* no, the window is being destroyed */

    /* Recurse on children */
    if (IswIsComposite(widget)) {
        Cardinal i;
        WidgetList children;

        cw = (CompositeWidget) widget;
        children = cw->composite.children;
        /* Unrealize all children */
        for (i = cw->composite.num_children; i != 0; --i) {
            UnrealizeWidget(children[i - 1]);
        }
        /* Unmap children that are managed and mapped_when_managed? */
        /* No, it's ok to be managed and unrealized as long as your parent */
        /* is unrealized. IswUnrealize widget makes sure the "top" widget */
        /* is unmanaged, we can ignore all descendents */
    }

    if (IswHasCallbacks(widget, IswNunrealizeCallback) == IswCallbackHasSome)
        IswCallCallbacks(widget, IswNunrealizeCallback, NULL);

    /* Unregister window.  Windowless widgets never registered a drawable
       (they share the ancestor's window), so skip both the unregister and
       the window clear; just mark them unrealized. */
    if (widget->core.windowless) {
        widget->core.windowless_realized = False;
    }
    else {
        IswUnregisterDrawable(IswDisplay(widget), IswWindow(widget));

        /* Remove Event Handlers */
        /* remove grabs. Happens automatically when window is destroyed. */

        /* Destroy X xcb_window_t, done at outer level with one request */
        widget->core.window = None;
    }

    /* Removing the event handler here saves having to keep track if
     * the translation table is changed while the widget is unrealized.
     */
    _IswRemoveTranslations(widget);
}                               /* UnrealizeWidget */

void
IswUnrealizeWidget(Widget widget)
{
    xcb_window_t window;
    Widget hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    /* Use the widget's own window, not the resolved ancestor window:
       windowless widgets must never destroy their ancestor's window. */
    window = widget->core.windowless ? None : widget->core.window;
    if (!IswIsRealized(widget)) {
        UNLOCK_APP(app);
        return;
    }
    if (widget->core.managed && widget->core.parent != NULL)
        IswUnmanageChild(widget);
    UnrealizeWidget(widget);
    if (window != None) {
        //XDestroyWindow(IswDisplay(widget), window);
        xcb_destroy_window(IswDisplay(widget), window);
        xcb_flush(IswDisplay(widget));
    }
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHunrealizeWidget;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}                               /* IswUnrealizeWidget */

void
IswCreateWindow(xcb_connection_t *display,
               Widget widget,
               unsigned int window_class,
               xcb_visualtype_t *visual,
               IswValueMask value_mask,
               uint32_t *attributes)
{
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    if (widget->core.windowless) {
        /* Windowless widgets never get an X window. */
        UNLOCK_APP(app);
        return;
    }
    if (widget->core.window == None) {
        if (widget->core.width == 0 || widget->core.height == 0) {
            Cardinal count = 1;

            IswAppErrorMsg(app,
                          "invalidDimension", "xtCreateWindow",
                          IswCIswToolkitError,
                          "Widget %s has zero width and/or height",
                          &widget->core.name, &count);
        }
        widget->core.window = xcb_generate_id(display);
        
        /* DIAGNOSTIC: Log visual pointer value */
        if (visual) {
        } else {
        }
        
        {
            xcb_window_t parent_win = widget->core.parent ?
                widget->core.parent->core.window :
                widget->core.screen->root;
            /* HiDPI: create window at physical pixel geometry */
            double _sf = _IswGetScaleFactor(display);
            xcb_void_cookie_t cookie = xcb_create_window_checked(
                display,
                widget->core.depth,
                widget->core.window,
                parent_win,
                (int16_t)(widget->core.x * _sf + 0.5),
                (int16_t)(widget->core.y * _sf + 0.5),
                (uint16_t)(widget->core.width * _sf + 0.5),
                (uint16_t)(widget->core.height * _sf + 0.5),
                (uint16_t)(widget->core.border_width * _sf + 0.5),
                window_class,
                visual ? visual->visual_id : XCB_COPY_FROM_PARENT,
                value_mask,
                (const uint32_t*)attributes);
            xcb_generic_error_t *err = xcb_request_check(display, cookie);
            if (err) {
                fprintf(stderr, "ERROR IswCreateWindow: xcb_create_window FAILED for widget=%s! "
                        "error_code=%d, resource_id=0x%x\n",
                        widget->core.name ? widget->core.name : "(null)",
                        (int)err->error_code, (unsigned)err->resource_id);
                free(err);
            } else {
            }
        }
            //XCreateWindow(IswDisplay(widget),
            //              (widget->core.parent ?
            //               widget->core.parent->core.window :
            //               widget->core.screen->root),
            //              (int) widget->core.x, (int) widget->core.y,
            //              (unsigned) widget->core.width,
            //              (unsigned) widget->core.height,
            //              (unsigned) widget->core.border_width,
            //              (int) widget->core.depth, window_class, visual,
            //              value_mask, attributes);
    }
    UNLOCK_APP(app);
}                               /* IswCreateWindow */

/* ---------------- IswNameToWidget ----------------- */

static Widget NameListToWidget(Widget root,
                               XrmNameList names,
                               XrmBindingList bindings,
                               int in_depth, int *out_depth, int *found_depth);

typedef Widget(*NameMatchProc) (XrmNameList,
                                XrmBindingList,
                                WidgetList, Cardinal, int, int *, int *);

static Widget
MatchExactChildren(XrmNameList names,
                   XrmBindingList bindings,
                   register WidgetList children,
                   register Cardinal num,
                   int in_depth,
                   int *out_depth,
                   int *found_depth)
{
    register Cardinal i;
    register XrmName name = *names;
    Widget w, result = NULL;
    int d, min = 10000;

    for (i = 0; i < num; i++) {
        if (name == children[i]->core.xrm_name) {
            w = NameListToWidget(children[i], &names[1], &bindings[1],
                                 in_depth + 1, &d, found_depth);
            if (w != NULL && d < min) {
                result = w;
                min = d;
            }
        }
    }
    *out_depth = min;
    return result;
}

static Widget
MatchWildChildren(XrmNameList names,
                  XrmBindingList bindings,
                  register WidgetList children,
                  register Cardinal num,
                  int in_depth,
                  int *out_depth,
                  int *found_depth)
{
    register Cardinal i;
    Widget w, result = NULL;
    int d, min = 10000;

    for (i = 0; i < num; i++) {
        w = NameListToWidget(children[i], names, bindings,
                             in_depth + 1, &d, found_depth);
        if (w != NULL && d < min) {
            result = w;
            min = d;
        }
    }
    *out_depth = min;
    return result;
}

static Widget
SearchChildren(Widget root,
               XrmNameList names,
               XrmBindingList bindings,
               NameMatchProc matchproc,
               int in_depth,
               int *out_depth,
               int *found_depth)
{
    Widget w1 = NULL, w2;
    int d1, d2;

    if (IswIsComposite(root)) {
        w1 = (*matchproc) (names, bindings,
                           ((CompositeWidget) root)->composite.children,
                           ((CompositeWidget) root)->composite.num_children,
                           in_depth, &d1, found_depth);
    }
    else
        d1 = 10000;
    w2 = (*matchproc) (names, bindings, root->core.popup_list,
                       root->core.num_popups, in_depth, &d2, found_depth);
    *out_depth = (d1 < d2 ? d1 : d2);
    return (d1 < d2 ? w1 : w2);
}

static Widget
NameListToWidget(register Widget root,
                 XrmNameList names,
                 XrmBindingList bindings,
                 int in_depth,
                 int *out_depth,
                 int *found_depth)
{
    int d1, d2;

    if (in_depth >= *found_depth) {
        *out_depth = 10000;
        return NULL;
    }

    if (names[0] == NULLQUARK) {
        *out_depth = *found_depth = in_depth;
        return root;
    }

    if (!IswIsWidget(root)) {
        *out_depth = 10000;
        return NULL;
    }

    if (*bindings == XrmBindTightly) {
        return SearchChildren(root, names, bindings, MatchExactChildren,
                              in_depth, out_depth, found_depth);

    }
    else {                      /* XrmBindLoosely */
        Widget w1, w2;

        w1 = SearchChildren(root, names, bindings, MatchExactChildren,
                            in_depth, &d1, found_depth);
        w2 = SearchChildren(root, names, bindings, MatchWildChildren,
                            in_depth, &d2, found_depth);
        *out_depth = (d1 < d2 ? d1 : d2);
        return (d1 < d2 ? w1 : w2);
    }
}                               /* NameListToWidget */

Widget
IswNameToWidget(Widget root, _Xconst char *name)
{
    XrmName *names;
    XrmBinding *bindings;
    int len, depth, found = 10000;
    Widget result;

    WIDGET_TO_APPCON(root);

    len = (int) strlen(name);
    if (len == 0)
        return NULL;

    LOCK_APP(app);
    names = (XrmName *) ALLOCATE_LOCAL((unsigned) (len + 1) * sizeof(XrmName));
    bindings = (XrmBinding *)
        ALLOCATE_LOCAL((unsigned) (len + 1) * sizeof(XrmBinding));
    if (names == NULL || bindings == NULL)
        _IswAllocError(NULL);

    XrmStringToBindingQuarkList(name, bindings, names);
    if (names[0] == NULLQUARK) {
        DEALLOCATE_LOCAL((char *) bindings);
        DEALLOCATE_LOCAL((char *) names);
        UNLOCK_APP(app);
        return NULL;
    }

    result = NameListToWidget(root, names, bindings, 0, &depth, &found);

    DEALLOCATE_LOCAL((char *) bindings);
    DEALLOCATE_LOCAL((char *) names);
    UNLOCK_APP(app);
    return result;
}                               /* IswNameToWidget */

/* Define user versions of intrinsics macros */

#undef IswDisplayOfObject
xcb_connection_t *
IswDisplayOfObject(Widget object)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    if (IswIsSubclass(object, hookObjectClass))
        return ((HookObject) object)->hooks.display;
    return IswDisplay(IswIsWidget(object) ? object : _IswWindowedAncestor(object));
}

#undef IswDisplay
xcb_connection_t *
IswDisplay(Widget widget)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    return widget->core.display;
}

#undef IswScreenOfObject
xcb_screen_t *
IswScreenOfObject(Widget object)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    if (IswIsSubclass(object, hookObjectClass))
        return ((HookObject) object)->hooks.screen;
    return IswScreen(IswIsWidget(object) ? object : _IswWindowedAncestor(object));
}

#undef IswScreen
xcb_screen_t *
IswScreen(Widget widget)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    return widget->core.screen;
}

#undef IswWindowOfObject
xcb_window_t
IswWindowOfObject(Widget object)
{
    return IswWindow(IswIsWidget(object) ? object : _IswWindowedAncestor(object));
}

#undef IswWindow
xcb_window_t
IswWindow(Widget widget)
{
    /* Windowless widgets share their nearest windowed ancestor's window. */
    if (widget->core.windowless)
        return _IswWindowedAncestor(widget)->core.window;
    return widget->core.window;
}

#undef IswSuperclass
WidgetClass
IswSuperclass(Widget widget)
{
    WidgetClass retval;

    LOCK_PROCESS;
    retval = IswClass(widget)->core_class.superclass;
    UNLOCK_PROCESS;
    return retval;
}

#undef IswClass
WidgetClass
IswClass(Widget widget)
{
    WidgetClass retval;

    LOCK_PROCESS;
    retval = widget->core.widget_class;
    UNLOCK_PROCESS;
    return retval;
}

#undef IswIsManaged
Boolean
IswIsManaged(Widget object)
{
    Boolean retval;

    WIDGET_TO_APPCON(object);

    LOCK_APP(app);
    if (IswIsRectObj(object))
        retval = object->core.managed;
    else
        retval = False;
    UNLOCK_APP(app);
    return retval;
}

#undef IswIsRealized
Boolean
IswIsRealized(Widget object)
{
    Boolean retval;

    WIDGET_TO_APPCON(object);

    LOCK_APP(app);
    if (IswIsWidget(object) && object->core.windowless)
        retval = object->core.windowless_realized;
    else
        retval = IswWindowOfObject(object) != None;
    UNLOCK_APP(app);
    return retval;
}                               /* IswIsRealized */

#undef IswIsSensitive
Boolean
IswIsSensitive(Widget object)
{
    Boolean retval;

    WIDGET_TO_APPCON(object);

    LOCK_APP(app);
    if (IswIsRectObj(object))
        retval = object->core.sensitive && object->core.ancestor_sensitive;
    else
        retval = False;
    UNLOCK_APP(app);
    return retval;
}

/*
 * Internal routine; must be called only after IswIsWidget returns false
 */
Widget
_IswWindowedAncestor(register Widget object)
{
    Widget obj = object;

    for (object = IswParent(object);
         object && (!IswIsWidget(object) || object->core.windowless);)
        object = IswParent(object);

    if (object == NULL) {
        String params = IswName(obj);
        Cardinal num_params = 1;

        IswErrorMsg("noWidgetAncestor", "windowedAncestor", IswCIswToolkitError,
                   "Object \"%s\" does not have windowed ancestor",
                   &params, &num_params);
    }

    return object;
}

#undef IswParent
Widget
IswParent(Widget widget)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    return widget->core.parent;
}

#undef IswName
String
IswName(Widget object)
{
    /* Attempts to LockApp() here will generate endless recursive loops */
    return XrmQuarkToString(object->core.xrm_name);
}

Boolean
IswIsObject(Widget object)
{
    WidgetClass wc;
    String class_name;

    /* perform basic sanity checks */
    if (object->core.self != object || object->core.xrm_name == NULLQUARK)
        return False;

    LOCK_PROCESS;
    wc = object->core.widget_class;
    if (wc->core_class.class_name == NULL) {
        UNLOCK_PROCESS;
        return False;
    }
    UNLOCK_PROCESS;

    if (IswIsWidget(object)) {
        if (object->core.name == NULL ||
            (class_name = XrmNameToString(object->core.xrm_name)) == NULL ||
            strcmp(object->core.name, class_name) != 0)
            return False;
    }
    return True;
}

#if defined(WIN32)
static int
access_file(char *path, char *pathbuf, int len_pathbuf, char **pathret)
{
    if (access(path, F_OK) == 0) {
        if (strlen(path) < len_pathbuf)
            *pathret = pathbuf;
        else
            *pathret = IswMalloc(strlen(path));
        if (*pathret) {
            strcpy(*pathret, path);
            return 1;
        }
    }
    return 0;
}

static int
AccessFile(char *path, char *pathbuf, int len_pathbuf, char **pathret)
{
    unsigned long drives;
    int i, len;
    char *drive;
    char buf[MAX_PATH];
    char *bufp;

    /* just try the "raw" name first and see if it works */
    if (access_file(path, pathbuf, len_pathbuf, pathret))
        return 1;

#if defined(WIN32) && defined(__MINGW32__)
    /* don't try others */
    return 0;
#endif

    /* try the places set in the environment */
    drive = getenv("_XBASEDRIVE");
    if (!drive)
        drive = "C:";
    len = strlen(drive) + strlen(path);
    bufp = IswStackAlloc(len + 1, buf);
    strcpy(bufp, drive);
    strcat(bufp, path);
    if (access_file(bufp, pathbuf, len_pathbuf, pathret)) {
        IswStackFree(bufp, buf);
        return 1;
    }

    /* one last place to look */
    drive = getenv("HOMEDRIVE");
    if (drive) {
        len = strlen(drive) + strlen(path);
        bufp = IswStackAlloc(len + 1, buf);
        strcpy(bufp, drive);
        strcat(bufp, path);
        if (access_file(bufp, pathbuf, len_pathbuf, pathret)) {
            IswStackFree(bufp, buf);
            return 1;
        }
    }

    /* does OS/2 (with or with gcc-emx) have getdrives()? */
    /* tried everywhere else, go fishing */
    drives = _getdrives();
#define C_DRIVE ('C' - 'A')
#define Z_DRIVE ('Z' - 'A')
    for (i = C_DRIVE; i <= Z_DRIVE; i++) {      /* don't check on A: or B: */
        if ((1 << i) & drives) {
            len = 2 + strlen(path);
            bufp = IswStackAlloc(len + 1, buf);
            *bufp = 'A' + i;
            *(bufp + 1) = ':';
            *(bufp + 2) = '\0';
            strcat(bufp, path);
            if (access_file(bufp, pathbuf, len_pathbuf, pathret)) {
                IswStackFree(bufp, buf);
                return 1;
            }
        }
    }
    return 0;
}
#endif

static Boolean
TestFile(String path)
{
    int ret = 0;
    struct stat status;

#if defined(WIN32)
    char buf[MAX_PATH];
    char *bufp;
    int len;
    UINT olderror = SetErrorMode(SEM_FAILCRITICALERRORS);

    if (AccessFile(path, buf, MAX_PATH, &bufp))
        path = bufp;

    (void) SetErrorMode(olderror);
#endif
    ret = (access(path, R_OK) == 0 &&   /* exists and is readable */
           stat(path, &status) == 0 &&  /* get the status */
#ifndef X_NOT_POSIX
           S_ISDIR(status.st_mode) == 0);       /* not a directory */
#else
           (status.st_mode & S_IFMT) != S_IFDIR);       /* not a directory */
#endif                          /* X_NOT_POSIX else */
    return (Boolean) ret;
}

/* return of TRUE = resolved string fit, FALSE = didn't fit.  Not
   null-terminated and not collapsed if it didn't fit */

static Boolean Resolve(register _Xconst char *source,   /* The source string */
                       register int len,        /* The length in bytes of *source */
                       Substitution sub,        /* Array of string values to substitute */
                       Cardinal num,    /* Number of substitution entries */
                       char *buf,       /* Where to put the resolved string; */
                       char collapse) { /* Character to collapse */
    register int bytesLeft = PATH_MAX;
    register char *bp = buf;

#ifndef DONT_COLLAPSE
    Boolean atBeginning = TRUE;
    Boolean prevIsCollapse = FALSE;

#define PUT(ch) \
    { \
        if (--bytesLeft == 0) return FALSE; \
        if (prevIsCollapse) \
            if ((*bp = ch) != collapse) { \
                prevIsCollapse = FALSE; \
                bp++; \
            } \
            else bytesLeft++; \
        else if ((*bp++ = ch) == collapse && !atBeginning) \
            prevIsCollapse = TRUE; \
    }
#else                           /* DONT_COLLAPSE */

#define PUT(ch) \
    { \
        if (--bytesLeft == 0) return FALSE; \
        *bp++ = ch; \
    }
#endif                          /* DONT_COLLAPSE */
#define escape '%'

    while (len--) {
#ifndef DONT_COLLAPSE
        if (*source == collapse) {
            PUT(*source);
            source++;
            continue;
        }
        else
#endif                          /* DONT_COLLAPSE */
        if (*source != escape) {
            PUT(*source);
        }
        else {
            source++;
            if (len-- == 0) {
                PUT(escape);
                break;
            }

            if (*source == ':' || *source == escape) {
                PUT(*source);
            }
            else {
                /* Match the character against the match array */
                register Cardinal j;

                for (j = 0; j < num && sub[j].match != *source; j++) {
                }

                /* Substitute the substitution string */

                if (j >= num) {
                    PUT(*source);
                }
                else if (sub[j].substitution != NULL) {
                    char *sp = sub[j].substitution;

                    while (*sp) {
                        PUT(*sp);
                        sp++;
                    }
                }
            }
        }
        source++;
#ifndef DONT_COLLAPSE
        atBeginning = FALSE;
#endif                          /* DONT_COLLAPSE */
    }
    PUT('\0');

    return TRUE;
#undef PUT
#undef escape
}

_IswString
IswFindFile(_Xconst _IswString path,
           Substitution substitutions,
           Cardinal num_substitutions,
           IswFilePredicate predicate)
{
    char *buf, *buf1, *buf2;
    _Xconst _IswString colon;
    int len;
    Boolean firstTime = TRUE;

    buf1 = __XtMalloc((unsigned) PATH_MAX);
    buf2 = __XtMalloc((unsigned) PATH_MAX);
    buf = buf1;

    if (predicate == NULL)
        predicate = TestFile;

    while (1) {
        colon = path;
        /* skip leading colons */
        while (*colon) {
            if (*colon != ':')
                break;
            colon++;
            path++;
        }
        /* now look for an un-escaped colon */
        for (; *colon; colon++) {
            if (*colon == '%' && *(path + 1)) {
                colon++;        /* bump it an extra time to skip %. */
                continue;
            }
            if (*colon == ':')
                break;
        }
        len = (int) (colon - path);
        if (Resolve(path, len, substitutions, num_substitutions, buf, '/')) {
            if (firstTime || strcmp(buf1, buf2) != 0) {
#ifdef XNL_DEBUG
                printf("Testing file %s\n", buf);
#endif                          /* XNL_DEBUG */
                /* Check out the file */
                if ((*predicate) (buf)) {
                    /* We've found it, return it */
#ifdef XNL_DEBUG
                    printf("File found.\n");
#endif                          /* XNL_DEBUG */
                    if (buf == buf1) {
                        IswFree(buf2);
                        return buf1;
                    }
                    IswFree(buf1);
                    return buf2;
                }
                if (buf == buf1)
                    buf = buf2;
                else
                    buf = buf1;
                firstTime = FALSE;
            }
        }

        /* Nope...any more paths? */

        if (*colon == '\0')
            break;
        path = colon + 1;
    }

    /* No file found */

    IswFree(buf1);
    IswFree(buf2);
    return NULL;
}

/* The implementation of this routine is operating system dependent */
/* Should match the code in Xlib _XlcMapOSLocaleName */

static String
ExtractLocaleName(String lang)
{

#if defined(CSRG_BASED) || defined(sun) || defined(SVR4) || defined(WIN32) || defined (linux) || defined(__FreeBSD__)
#ifdef WIN32
#define SKIPCOUNT 1
#define STARTCHAR '='
#define ENDCHAR ';'
#define WHITEFILL
#else
#if defined(linux)
#define STARTSTR "LC_CTYPE="
#define ENDCHAR ';'
#else
#if !defined(sun) || defined(SVR4)
#define STARTCHAR '/'
#define ENDCHAR '/'
#endif
#endif
#endif

    String start;
    String end;
    int len;

#ifdef SKIPCOUNT
    int n;
#endif
    static char *buf = NULL;

#ifdef WHITEFILL
    char *temp;
#endif

    start = lang;
#ifdef SKIPCOUNT
    for (n = SKIPCOUNT;
         --n >= 0 && start && (start = strchr(start, STARTCHAR)); start++);
    if (!start)
        start = lang;
#endif
#ifdef STARTCHAR
    if (start && (start = strchr(start, STARTCHAR)))
#elif  defined (STARTSTR)
    if (start && (start = strstr(start, STARTSTR)))
#endif
    {
#ifdef STARTCHAR
        start++;
#elif defined (STARTSTR)
        start += strlen(STARTSTR);
#endif

        if ((end = strchr(start, ENDCHAR))) {
            len = (int) (end - start);
            IswFree(buf);
            buf = IswMalloc((Cardinal) (len + 1));
            if (buf == NULL)
                return NULL;
            strncpy(buf, start, (size_t) len);
            *(buf + len) = '\0';
#ifdef WHITEFILL
            for (temp = buf; (temp = strchr(temp, ' ')) != NULL;)
                *temp++ = '-';
#endif
            return buf;
        }
        else                    /* if no ENDCHAR is found we are at the end of the line */
            return start;
    }
#ifdef WHITEFILL
    if (strchr(lang, ' ')) {
        IswFree(buf);
        buf = strdup(lang);
        if (buf == NULL)
            return NULL;
        for (temp = buf; (temp = strchr(temp, ' ')) != NULL;)
            *temp++ = '-';
        return buf;
    }
#endif
#undef STARTCHAR
#undef ENDCHAR
#undef WHITEFILL
#endif

    return lang;
}

static void
FillInLangSubs(Substitution subs, IswPerDisplay pd)
{
    int len;
    String string;
    char *p1, *p2, *p3;
    char **rest;
    char *ch;

    if (pd->language == NULL || pd->language[0] == '\0') {
        subs[0].substitution = subs[1].substitution =
            subs[2].substitution = subs[3].substitution = NULL;
        return;
    }

    string = ExtractLocaleName(pd->language);

    if (string == NULL || string[0] == '\0') {
        subs[0].substitution = subs[1].substitution =
            subs[2].substitution = subs[3].substitution = NULL;
        return;
    }

    len = (int) strlen(string) + 1;
    subs[0].substitution = (_IswString) string;
    p1 = subs[1].substitution = IswMallocArray(3, (Cardinal) len);
    p2 = subs[2].substitution = subs[1].substitution + len;
    p3 = subs[3].substitution = subs[2].substitution + len;

    /* Everything up to the first "_" goes into p1.  From "_" to "." in
       p2.  The rest in p3.  If no delimiters, all goes into p1.  We
       assume p1, p2, and p3 are large enough. */

    *p1 = *p2 = *p3 = '\0';

    ch = strchr(string, '_');
    if (ch != NULL) {
        len = (int) (ch - string);
        (void) strncpy(p1, string, (size_t) len);
        p1[len] = '\0';
        string = ch + 1;
        rest = &p2;
    }
    else
        rest = &p1;

    /* Rest points to where we put the first part */

    ch = strchr(string, '.');
    if (ch != NULL) {
        len = (int) (ch - string);
        strncpy(*rest, string, (size_t) len);
        (*rest)[len] = '\0';
        (void) strcpy(p3, ch + 1);
    }
    else
        (void) strcpy(*rest, string);
}

/*
 * default path used if environment variable XFILESEARCHPATH
 * is not defined.  Also substituted for %D.
 * The exact value should be documented in the implementation
 * notes for any Xt implementation.
 */
static const char *
implementation_default_path(void)
{
#if defined(WIN32)
    static char xfilesearchpath[] = "";

    return xfilesearchpath;
#else
    return XFILESEARCHPATHDEFAULT;
#endif
}


/* *INDENT-OFF* */
static SubstitutionRec defaultSubs[] = {
    {'N', NULL},
    {'T', NULL},
    {'S', NULL},
    {'C', NULL},
    {'L', NULL},
    {'l', NULL},
    {'t', NULL},
    {'c', NULL}
};
/* *INDENT-ON* */

_IswString
IswResolvePathname(xcb_connection_t *dpy,
                  _Xconst char *type,
                  _Xconst char *filename,
                  _Xconst char *suffix,
                  _Xconst char *path,
                  Substitution substitutions,
                  Cardinal num_substitutions,
                  IswFilePredicate predicate)
{
    IswPerDisplay pd;
    static const char *defaultPath = NULL;
    const char *impl_default = implementation_default_path();
    int idef_len = (int) strlen(impl_default);
    char *massagedPath;
    int bytesAllocd, bytesLeft;
    char *ch, *result;
    Substitution merged_substitutions;
    char *customization = NULL;
    Boolean pathMallocd = False;

    LOCK_PROCESS;
    pd = _IswGetPerDisplay(dpy);
    if (path == NULL) {
        if (defaultPath == NULL) {
            defaultPath = getenv("XFILESEARCHPATH");
            if (defaultPath == NULL)
                defaultPath = impl_default;
        }
        path = defaultPath;
    }

    if (path == NULL)
        path = "";              /* NULL would kill us later */

    if (filename == NULL) {
        /* pd->class is now a plain String (not XrmClass) */
        filename = pd->class ? pd->class : "";
    }

    bytesAllocd = bytesLeft = 1000;
    massagedPath = ALLOCATE_LOCAL((size_t) bytesAllocd);
    if (massagedPath == NULL)
        _IswAllocError(NULL);

    if (path[0] == ':') {
        strcpy(massagedPath, "%N%S");
        ch = &massagedPath[4];
        bytesLeft -= 4;
    }
    else
        ch = massagedPath;

    /* Insert %N%S between adjacent colons
     * and default path for %D.
     * Default path should not have any adjacent colons of its own.
     */

    while (*path != '\0') {
        if (bytesLeft < idef_len) {
            int bytesUsed = bytesAllocd - bytesLeft;
            char *new;

            bytesAllocd += 1000;
            new = __XtMalloc((Cardinal) bytesAllocd);
            strncpy(new, massagedPath, (size_t) bytesUsed);
            ch = new + bytesUsed;
            if (pathMallocd)
                IswFree(massagedPath);
            else
                DEALLOCATE_LOCAL(massagedPath);
            pathMallocd = True;
            massagedPath = new;
            bytesLeft = bytesAllocd - bytesUsed;
        }
        if (*path == '%' && *(path + 1) == ':') {
            *ch++ = '%';
            *ch++ = ':';
            path += 2;
            bytesLeft -= 2;
            continue;
        }
        if (*path == ':' && *(path + 1) == ':') {
            strcpy(ch, ":%N%S:");
            ch += 6;
            bytesLeft -= 6;
            while (*path == ':')
                path++;
            continue;
        }
        if (*path == '%' && *(path + 1) == 'D') {
            strcpy(ch, impl_default);
            ch += idef_len;
            bytesLeft -= idef_len;
            path += 2;
            continue;
        }
        *ch++ = *path++;
        bytesLeft--;
    }
    *ch = '\0';
#ifdef XNL_DEBUG
    printf("Massaged path: %s\n", massagedPath);
#endif                          /* XNL_DEBUG */

    if (num_substitutions == 0)
        merged_substitutions = defaultSubs;
    else {
        int i = IswNumber(defaultSubs);
        Substitution sub, def;

        merged_substitutions = sub = (Substitution)
            ALLOCATE_LOCAL((unsigned) (num_substitutions + (Cardinal) i) *
                           sizeof(SubstitutionRec));
        if (sub == NULL)
            _IswAllocError(NULL);
        for (def = defaultSubs; i--; sub++, def++)
            sub->match = def->match;
        for (i = (int) num_substitutions; i--;)
            *sub++ = *substitutions++;
    }
    merged_substitutions[0].substitution = (_IswString) filename;
    merged_substitutions[1].substitution = (_IswString) type;
    merged_substitutions[2].substitution = (_IswString) suffix;

    /* Look up "customization" resource using xcb-xrm.
     * Build the resource name as "appname.customization" and class as
     * "AppClass.Customization". */
    {
        xcb_xrm_database_t *db = NULL;
        /* Use the default screen's database if available */
        if (pd->per_screen_db) {
            db = pd->per_screen_db[0];
        }
        if (db != NULL && pd->name != NULL && pd->class != NULL) {
            char *res_name = NULL;
            char *res_class = NULL;
            IswAsprintf(&res_name, "%s.customization", pd->name);
            IswAsprintf(&res_class, "%s.Customization", pd->class);
            if (xcb_xrm_resource_get_string(db, res_name, res_class,
                                            &customization) < 0)
                customization = NULL;
            IswFree(res_name);
            IswFree(res_class);
        }
    }
    merged_substitutions[3].substitution = customization;
    FillInLangSubs(&merged_substitutions[4], pd);

    result = IswFindFile(massagedPath, merged_substitutions,
                        num_substitutions + IswNumber(defaultSubs), predicate);

    if (merged_substitutions[5].substitution != NULL)
        IswFree((IswPointer) merged_substitutions[5].substitution);

    if (merged_substitutions != defaultSubs)
        DEALLOCATE_LOCAL(merged_substitutions);

    if (pathMallocd)
        IswFree(massagedPath);
    else
        DEALLOCATE_LOCAL(massagedPath);

    if (customization != NULL)
        free(customization);  /* xcb_xrm_resource_get_string allocates with malloc */

    UNLOCK_PROCESS;
    return result;
}

Boolean
IswCallAcceptFocus(Widget widget, xcb_timestamp_t *time)
{
    IswAcceptFocusProc ac;
    Boolean retval;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    ac = IswClass(widget)->core_class.accept_focus;
    UNLOCK_PROCESS;

    if (ac != NULL)
        retval = (*ac) (widget, time);
    else
        retval = FALSE;
    UNLOCK_APP(app);
    return retval;
}

#ifdef ISW_GEO_TATTLER
/**************************************************************************
 GeoTattler:  This is used to debug Geometry management in Xt.

  It uses a pseudo resource IswNgeotattler.

  E.G. if those lines are found in the resource database:

    myapp*draw.XmScale.geoTattler: ON
    *XmScrollBar.geoTattler:ON
    *XmRowColumn.exit_button.geoTattler:ON

   then:

    all the XmScale children of the widget named draw,
    all the XmScrollBars,
    the widget named exit_button in any XmRowColumn

   will return True to the function IsTattled(), and will generate
   outlined trace to stdout.

*************************************************************************/

#define IswNgeoTattler "geoTattler"
#define IswCGeoTattler "GeoTattler"

typedef struct {
    Boolean geo_tattler;
} GeoDataRec;

/* *INDENT-OFF* */
static IswResource geo_resources[] = {
    { IswNgeoTattler, IswCGeoTattler, IswRBoolean, sizeof(Boolean),
      IswOffsetOf(GeoDataRec, geo_tattler),
      IswRImmediate, (IswPointer) False }
};
/* *INDENT-ON* */

/************************************************************************
  This function uses IswGetSubresources to find out if a widget
  needs to be geo-spied by the caller. */
static Boolean
IsTattled(Widget widget)
{
    GeoDataRec geo_data;

    IswGetSubresources(widget, (IswPointer) &geo_data,
                      (String) NULL, (String) NULL,
                      geo_resources, IswNumber(geo_resources), NULL, 0);

    return geo_data.geo_tattler;

}                               /* IsTattled */

static int n_tab = 0;           /* not MT for now */

void
_IswGeoTab(int direction)
{                               /* +1 or -1 */
    n_tab += direction;
}

void
_IswGeoTrace(Widget widget, const char *fmt, ...)
{
    if (IsTattled(widget)) {
        va_list args;
        int i;

        va_start(args, fmt);
        for (i = 0; i < n_tab; i++)
            printf("     ");
        (void) vprintf(fmt, args);
        va_end(args);
    }
}

#endif                          /* ISW_GEO_TATTLER */
