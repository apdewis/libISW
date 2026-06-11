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
#include "VarargsI.h"
#include "ShellP.h"
#include "CreateI.h"
#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
#include "ResConfigP.h"
#endif
#include "ISWPlatformPrivate.h"
#include <stdio.h>

static _Xconst _IswString IswNxtCreateWidget = "xtCreateWidget";
static _Xconst _IswString IswNxtCreatePopupShell = "xtCreatePopupShell";

static void
CallClassPartInit(WidgetClass ancestor, WidgetClass wc)
{
    if (ancestor->core_class.superclass != NULL) {
        CallClassPartInit(ancestor->core_class.superclass, wc);
    }
    if (ancestor->core_class.class_part_initialize != NULL) {
        (*(ancestor->core_class.class_part_initialize)) (wc);
    }
}

void
IswInitializeWidgetClass(WidgetClass wc)
{
    IswEnum inited;

    LOCK_PROCESS;
    if (wc->core_class.class_inited) {
        UNLOCK_PROCESS;
        return;
    }
    inited = 0x01;
    {
        WidgetClass pc;

#define LeaveIfClass(c, d) if (pc == c) { inited = d; break; }
        for (pc = wc; pc; pc = pc->core_class.superclass) {
            LeaveIfClass(rectObjClass, 0x01 | RectObjClassFlag);
            LeaveIfClass(coreWidgetClass, 0x01 |
                         RectObjClassFlag | WidgetClassFlag);
            LeaveIfClass(compositeWidgetClass, 0x01 |
                         RectObjClassFlag |
                         WidgetClassFlag | CompositeClassFlag);
            LeaveIfClass(constraintWidgetClass, 0x01 |
                         RectObjClassFlag |
                         WidgetClassFlag |
                         CompositeClassFlag | ConstraintClassFlag);
            LeaveIfClass(shellWidgetClass, 0x01 |
                         RectObjClassFlag |
                         WidgetClassFlag | CompositeClassFlag | ShellClassFlag);
            LeaveIfClass(wmShellWidgetClass, 0x01 |
                         RectObjClassFlag |
                         WidgetClassFlag |
                         CompositeClassFlag |
                         ShellClassFlag | WMShellClassFlag);
            LeaveIfClass(topLevelShellWidgetClass, 0x01 |
                         RectObjClassFlag |
                         WidgetClassFlag |
                         CompositeClassFlag |
                         ShellClassFlag | WMShellClassFlag | TopLevelClassFlag);
        }
#undef LeaveIfClass
    }
    if (wc->core_class.version != IswVersion &&
        wc->core_class.version != IswVersionDontCheck) {
        String param[3];
        _Xconst _IswString mismatch =
            "Widget class %s version mismatch (recompilation needed):\n  widget %d vs. intrinsics %d.";
        _Xconst _IswString recompile = "Widget class %s must be re-compiled.";
        Cardinal num_params;

        param[0] = wc->core_class.class_name;
        param[1] = (String) (IswIntPtr) wc->core_class.version;
        param[2] = (String) (IswIntPtr) IswVersion;

        if (wc->core_class.version == (11 * 1000 + 5) ||        /* MIT X11R5 */
            wc->core_class.version == (11 * 1000 + 4)) {        /* MIT X11R4 */
            if ((inited & WMShellClassFlag) &&
                (sizeof(Boolean) != sizeof(char) ||
                 sizeof(Atom) != sizeof(Widget) ||
                 sizeof(Atom) != sizeof(String))) {
                num_params = 3;
                IswWarningMsg("versionMismatch", "widget", IswCIswToolkitError,
                             mismatch, param, &num_params);
                num_params = 1;
                IswErrorMsg("R4orR5versionMismatch", "widget", IswCIswToolkitError,
                           recompile, param, &num_params);

            }
        }
        else if (wc->core_class.version == (11 * 1000 + 3)) {   /* MIT X11R3 */
            if (inited & ShellClassFlag) {
                num_params = 1;
                IswWarningMsg("r3versionMismatch", "widget", IswCIswToolkitError,
                             "Shell Widget class %s binary compiled for R3",
                             param, &num_params);
                IswErrorMsg("R3versionMismatch", "widget", IswCIswToolkitError,
                           recompile, param, &num_params);
            }
        }
        else {
            num_params = 3;
            IswWarningMsg("versionMismatch", "widget", IswCIswToolkitError,
                         mismatch, param, &num_params);
            if (wc->core_class.version == (2 * 1000 + 2)) {     /* MIT X11R2 */
                num_params = 1;
                IswErrorMsg("r2versionMismatch", "widget", IswCIswToolkitError,
                           recompile, param, &num_params);
            }
        }
    }

    if ((wc->core_class.superclass != NULL)
        && (!(wc->core_class.superclass->core_class.class_inited)))
        IswInitializeWidgetClass(wc->core_class.superclass);

    if (wc->core_class.class_initialize != NULL)
        (*(wc->core_class.class_initialize)) ();
    CallClassPartInit(wc, wc);
    wc->core_class.class_inited = inited;
    UNLOCK_PROCESS;
}

static void
CallInitialize(WidgetClass class,
               Widget req_widget,
               Widget new_widget,
               ArgList args,
               Cardinal num_args)
{
    WidgetClass superclass;
    IswInitProc initialize;
    IswArgsProc initialize_hook;

    LOCK_PROCESS;
    superclass = class->core_class.superclass;
    UNLOCK_PROCESS;
    if (superclass)
        CallInitialize(superclass, req_widget, new_widget, args, num_args);
    LOCK_PROCESS;
    initialize = class->core_class.initialize;
    UNLOCK_PROCESS;
    if (initialize)
        (*initialize) (req_widget, new_widget, args, &num_args);
    LOCK_PROCESS;
    initialize_hook = class->core_class.initialize_hook;
    UNLOCK_PROCESS;
    if (initialize_hook)
        (*initialize_hook) (new_widget, args, &num_args);
}

static void
CallConstraintInitialize(ConstraintWidgetClass class,
                         Widget req_widget,
                         Widget new_widget,
                         ArgList args,
                         Cardinal num_args)
{
    WidgetClass superclass;
    IswInitProc initialize;

    LOCK_PROCESS;
    superclass = class->core_class.superclass;
    UNLOCK_PROCESS;
    if (superclass != constraintWidgetClass)
        CallConstraintInitialize((ConstraintWidgetClass) superclass,
                                 req_widget, new_widget, args, num_args);
    LOCK_PROCESS;
    initialize = class->constraint_class.initialize;
    UNLOCK_PROCESS;
    if (initialize)
        (*initialize) (req_widget, new_widget, args, &num_args);
}

static Widget
xtWidgetAlloc(WidgetClass widget_class,
              ConstraintWidgetClass parent_constraint_class,
              Widget parent,
              _Xconst _IswString name,
              ArgList args,     /* must be NULL if typed_args is non-NULL */
              Cardinal num_args,
              IswTypedArgList typed_args,     /* must be NULL if args is non-NULL */
              Cardinal num_typed_args)
{
    Widget widget;
    Cardinal csize = 0;
    ObjectClassExtension ext;

    if (widget_class == NULL)
        return 0;

    LOCK_PROCESS;
    if (!(widget_class->core_class.class_inited))
        IswInitializeWidgetClass(widget_class);
    ext = (ObjectClassExtension)
        IswGetClassExtension(widget_class,
                            IswOffsetOf(ObjectClassRec, object_class.extension),
                            NULLQUARK, IswObjectExtensionVersion,
                            sizeof(ObjectClassExtensionRec));
    if (parent_constraint_class)
        csize = parent_constraint_class->constraint_class.constraint_size;
    if (ext && ext->allocate) {
        IswAllocateProc allocate;
        Cardinal extra = 0;
        Cardinal nargs = num_args;
        Cardinal ntyped = num_typed_args;

        allocate = ext->allocate;
        UNLOCK_PROCESS;
        (*allocate) (widget_class, &csize, &extra, args, &nargs,
                     typed_args, &ntyped, &widget, NULL);
    }
    else {
        Cardinal wsize = widget_class->core_class.widget_size;

        UNLOCK_PROCESS;
        if (csize) {
            if (sizeof(struct {
                       char a; double b;}) != (sizeof(struct {
                                                      char a;
                                                      unsigned long b;}) -
                                               sizeof(unsigned long) +
                                               sizeof(double))) {
                if (csize && !(csize & (sizeof(double) - 1)))
                    wsize = (Cardinal) ((wsize + sizeof(double) - 1)
                                        & ~(sizeof(double) - 1));
            }
        }
        widget = (Widget) __XtCalloc(1, (unsigned) (wsize + csize));
        widget->core.constraints =
            (csize ? (IswPointer) ((char *) widget + wsize) : NULL);
    }
    widget->core.self = widget;
    widget->core.parent = parent;
    widget->core.widget_class = widget_class;
    widget->core.xrm_name = StringToName((name != NULL) ? name : "");
    widget->core.being_destroyed =
        (parent != NULL ? parent->core.being_destroyed : FALSE);
    return widget;
}

static void
CompileCallbacks(Widget widget)
{
    CallbackTable offsets;
    int i;

    LOCK_PROCESS;
    offsets = (CallbackTable)
        widget->core.widget_class->core_class.callback_private;

    if (offsets != NULL) {
        for (i = (int) (long) *(offsets++); --i >= 0; offsets++) {
            InternalCallbackList *cl = (InternalCallbackList *)
                ((char *) widget - (*offsets)->xrm_offset - 1);
        
            if (*cl)
                *cl = _IswCompileCallbackList((IswCallbackList) *cl);
        }
    }

    UNLOCK_PROCESS;
}

static Widget
xtCreate(String name,
         String class,
         WidgetClass widget_class,
         Widget parent,
         IswScreen default_screen,    /* undefined when creating a nonwidget */
         IswDisplay conn, /* connection for use with provided screen */
         ArgList args,          /* must be NULL if typed_args is non-NULL */
         Cardinal num_args,
         IswTypedArgList typed_args,  /* must be NULL if args is non-NULL */
         Cardinal num_typed_args,
         ConstraintWidgetClass parent_constraint_class,
         /* NULL if not a subclass of Constraint or if child is popup shell */
         IswWidgetProc post_proc)
{
    /* need to use strictest alignment rules possible in next two decls. */
    double widget_cache[100];
    Widget req_widget;
    IswPointer req_constraints = NULL;
    Cardinal wsize;
    Widget widget;
    IswCacheRef *cache_refs = NULL;
    IswCreateHookDataRec call_data;

    widget = xtWidgetAlloc(widget_class, parent_constraint_class, parent,
                           name, args, num_args, typed_args, num_typed_args);

    if (IswIsRectObj(widget)) {
        widget->core.managed = FALSE;
    }
    if (IswIsWidget(widget)) {
        widget->core.name = XrmNameToString(widget->core.xrm_name);
        widget->core.screen = default_screen;
        widget->core.display = conn;
        widget->core.tm.translations = NULL;
        widget->core.visible = TRUE;
        widget->core.popup_list = NULL;
        widget->core.num_popups = 0;
        /* Resource defaults that would normally be set by _IswGetResources
         * (currently disabled). Set them explicitly here so the widget
         * behaves correctly without the resource system. */
        widget->core.mapped_when_managed = TRUE;  /* IswNmappedWhenManaged default */
        widget->core.sensitive = TRUE;            /* IswNsensitive default */
        widget->core.ancestor_sensitive = TRUE;   /* IswNancestorSensitive default */
        widget->core.border_width = 1;            /* IswNborderWidth default */
    };
    LOCK_PROCESS;
    if (IswIsApplicationShell(widget)) {
        ApplicationShellWidget a = (ApplicationShellWidget) widget;

        if (class != NULL)
            a->application.class = class;
        else
            a->application.class = widget_class->core_class.class_name;
    }
    UNLOCK_PROCESS;

    /* fetch resources */
    /* NOTE: _IswGetResources uses Xrm quarks but is NOT Xrm-specific -
     * it's the core resource system. This MUST be enabled. */
    cache_refs = _IswGetResources(widget, args, num_args,
                                 typed_args, &num_typed_args);

    /* Convert typed arg list to arg list */
    if (typed_args != NULL && num_typed_args > 0) {
        Cardinal i;

        args = (ArgList) ALLOCATE_LOCAL(sizeof(Arg) * num_typed_args);
        if (args == NULL)
            _IswAllocError(NULL);
        for (i = 0; i < num_typed_args; i++) {
            args[i].name = typed_args[i].name;
            args[i].value = typed_args[i].value;
        }
        num_args = num_typed_args;
    }

    /* HiDPI: widget internals operate in logical pixels.  Physical pixel
     * conversion happens only at the X boundary (window create/configure
     * in Geometry.c/Shell.c, and inbound event coordinate division in
     * Event.c).  No resource values are scaled here. */

    CompileCallbacks(widget);

    if (cache_refs != NULL) {
        IswAddCallback(widget, IswNdestroyCallback,
                      IswCallbackReleaseCacheRefList, (IswPointer) cache_refs);
    }

    wsize = widget_class->core_class.widget_size;
    req_widget = (Widget) IswStackAlloc(wsize, widget_cache);
    (void) memcpy(req_widget, (char *) widget, (size_t) wsize);
    CallInitialize(IswClass(widget), req_widget, widget, args, num_args);
    if (parent_constraint_class != NULL) {
        double constraint_cache[20];
        Cardinal csize;

        csize = parent_constraint_class->constraint_class.constraint_size;
        if (csize) {
            req_constraints = IswStackAlloc(csize, constraint_cache);
            (void) memcpy(req_constraints, widget->core.constraints,
                           (size_t) csize);
            req_widget->core.constraints = req_constraints;
        }
        else
            req_widget->core.constraints = NULL;
        CallConstraintInitialize(parent_constraint_class, req_widget, widget,
                                 args, num_args);
        if (csize) {
            IswStackFree(req_constraints, constraint_cache);
        }
    }
    IswStackFree((IswPointer) req_widget, widget_cache);
    if (post_proc != (IswWidgetProc) NULL && (parent != NULL)) {
        Widget hookobj;

        (*post_proc) (widget);

        hookobj = IswHooksOfDisplay((default_screen != (IswScreen) NULL) ?
                                   conn :
                                   IswDisplayOfObject(parent));
        if (IswHasCallbacks(hookobj, IswNcreateHook) == IswCallbackHasSome) {

            call_data.type = IswHcreate;
            call_data.widget = widget;
            call_data.args = args;
            call_data.num_args = num_args;
            IswCallCallbackList(hookobj,
                               ((HookObject) hookobj)->hooks.
                               createhook_callbacks, (IswPointer) &call_data);
        }
    }
    if (typed_args != NULL) {
        while (num_typed_args-- > 0) {

            /* In GetResources we may have dynamically alloc'd store to hold */
            /* a copy of a resource which was larger then sizeof(IswArgVal). */
            /* We must free this store now in order to prevent a memory leak */
            /* A typed arg that has a converted value in dynamic store has a */
            /* negated size field. */

            if (typed_args->type != NULL && typed_args->size < 0) {
                IswFree((char *) typed_args->value);
                typed_args->size = -(typed_args->size);
            }
            typed_args++;
        }
        DEALLOCATE_LOCAL((char *) args);
    }
    return (widget);
}

static void
widgetPostProc(Widget w)
{
    IswWidgetProc insert_child;
    Widget parent = IswParent(w);
    String param = IswName(w);
    Cardinal num_params = 1;

    if (IswIsComposite(parent)) {
        LOCK_PROCESS;
        insert_child =
            ((CompositeWidgetClass) parent->core.widget_class)->composite_class.
            insert_child;
        UNLOCK_PROCESS;
    }
    else {
        return;
    }
    if (insert_child == NULL) {
        IswAppErrorMsg(IswWidgetToApplicationContext(parent),
                      "nullProc", "insertChild", IswCIswToolkitError,
                      "\"%s\" parent has NULL insert_child method",
                      &param, &num_params);
    }
    else {
        (*insert_child) (w);
    }
}

Widget
_IswCreateWidget(String name,
                WidgetClass widget_class,
                Widget parent,
                ArgList args,
                Cardinal num_args,
                IswTypedArgList typed_args,
                Cardinal num_typed_args)
{
    register Widget widget;
    ConstraintWidgetClass cwc;
    IswDisplay conn;
    IswScreen default_screen;
    IswEnum class_inited;
    String params[3];
    Cardinal num_params;

    params[0] = name;
    num_params = 1;

    if (parent == NULL) {
        IswErrorMsg("invalidParent", IswNxtCreateWidget, IswCIswToolkitError,
                   "IswCreateWidget \"%s\" requires non-NULL parent",
                   params, &num_params);
    }
    else if (widget_class == NULL) {
        IswAppErrorMsg(IswWidgetToApplicationContext(parent),
                      "invalidClass", IswNxtCreateWidget, IswCIswToolkitError,
                      "IswCreateWidget \"%s\" requires non-NULL widget class",
                      params, &num_params);
    }
    LOCK_PROCESS;
    if (!widget_class->core_class.class_inited)
        IswInitializeWidgetClass(widget_class);
    class_inited = widget_class->core_class.class_inited;
    UNLOCK_PROCESS;
    if ((class_inited & WidgetClassFlag) == 0) {
        /* not a widget */
        default_screen = NULL;
        if (IswIsComposite(parent)) {
            CompositeClassExtension ext;

            ext = (CompositeClassExtension)
                IswGetClassExtension(IswClass(parent),
                                    IswOffsetOf(CompositeClassRec,
                                               composite_class.extension),
                                    NULLQUARK, 1L, (Cardinal) 0);
            LOCK_PROCESS;
            if (ext &&
                (ext->version > IswCompositeExtensionVersion ||
                 ext->record_size > sizeof(CompositeClassExtensionRec))) {
                params[1] = IswClass(parent)->core_class.class_name;
                num_params = 2;
                IswAppWarningMsg(IswWidgetToApplicationContext(parent),
                                "invalidExtension", IswNxtCreateWidget,
                                IswCIswToolkitError,
                                "widget \"%s\" class %s has invalid CompositeClassExtension record",
                                params, &num_params);
            }
            if (!ext || !ext->accepts_objects) {
                params[1] = IswName(parent);
                num_params = 2;
                IswAppErrorMsg(IswWidgetToApplicationContext(parent),
                              "nonWidget", IswNxtCreateWidget, IswCIswToolkitError,
                              "attempt to add non-widget child \"%s\" to parent \"%s\" which supports only widgets",
                              params, &num_params);
            }
            UNLOCK_PROCESS;
        }
    }
    else {
        conn = parent->core.display;
        default_screen = parent->core.screen;
    }

    if (IswIsConstraint(parent)) {
        cwc = (ConstraintWidgetClass) parent->core.widget_class;
    }
    else {
        cwc = NULL;
    }
    widget = xtCreate(name, (char *) NULL, widget_class, parent,
                      default_screen, conn, args, num_args,
                      typed_args, num_typed_args, cwc, widgetPostProc);
    return (widget);
}

Widget
IswCreateWidget(_Xconst char *name,
               WidgetClass widget_class,
               Widget parent,
               ArgList args,
               Cardinal num_args)
{
    Widget retval;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    retval =
        _IswCreateWidget((String) name, widget_class, parent, args, num_args,
                        (IswTypedArgList) NULL, (Cardinal) 0);
    UNLOCK_APP(app);
    return retval;
}

Widget
IswCreateManagedWidget(_Xconst char *name,
                      WidgetClass widget_class,
                      Widget parent,
                      ArgList args,
                      Cardinal num_args)
{
    register Widget widget;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    IswCheckSubclass(parent, compositeWidgetClass, "in IswCreateManagedWidget");
    widget = _IswCreateWidget((String) name, widget_class, parent, args,
                             num_args, (IswTypedArgList) NULL, (Cardinal) 0);
    IswManageChild(widget);
    UNLOCK_APP(app);
    return widget;
}

static void
popupPostProc(Widget w)
{
    Widget parent = IswParent(w);

    parent->core.popup_list = IswReallocArray(parent->core.popup_list,
                                             (parent->core.num_popups + 1),
                                             (Cardinal) sizeof(Widget));
    parent->core.popup_list[parent->core.num_popups++] = w;
}

Widget
_IswCreatePopupShell(String name,
                    WidgetClass widget_class,
                    Widget parent,
                    ArgList args,
                    Cardinal num_args,
                    IswTypedArgList typed_args,
                    Cardinal num_typed_args)
{
    register Widget widget;
    IswScreen default_screen;

    if (parent == NULL) {
        IswErrorMsg("invalidParent", IswNxtCreatePopupShell, IswCIswToolkitError,
                   "IswCreatePopupShell requires non-NULL parent", NULL, NULL);
    }
    else if (widget_class == NULL) {
        IswAppErrorMsg(IswWidgetToApplicationContext(parent),
                      "invalidClass", IswNxtCreatePopupShell, IswCIswToolkitError,
                      "IswCreatePopupShell requires non-NULL widget class",
                      NULL, NULL);
    }
    IswCheckSubclass(parent, coreWidgetClass, "in IswCreatePopupShell");
    default_screen = parent->core.screen;
    widget = xtCreate(name, (char *) NULL, widget_class, parent,
                      default_screen, parent->core.display, args, num_args, typed_args,
                      num_typed_args, (ConstraintWidgetClass) NULL,
                      popupPostProc);

#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
    IswAddEventHandler(widget, (EventMask) XCB_EVENT_MASK_PROPERTY_CHANGE, FALSE,
                      _IswResourceConfigurationEH, NULL);
#endif
    return (widget);
}

Widget
IswCreatePopupShell(_Xconst char *name,
                   WidgetClass widget_class,
                   Widget parent,
                   ArgList args,
                   Cardinal num_args)
{
    Widget retval;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    retval = _IswCreatePopupShell((String) name, widget_class, parent, args,
                                 num_args, (IswTypedArgList) NULL, (Cardinal) 0);
    UNLOCK_APP(app);
    return retval;
}

Widget
_IswAppCreateShell(String name,
                  String class,
                  WidgetClass widget_class,
                  IswDisplay display,
                  ArgList args,
                  Cardinal num_args,
                  IswTypedArgList typed_args,
                  Cardinal num_typed_args)
{
    Widget shell;

    if (widget_class == NULL) {
        IswAppErrorMsg(IswDisplayToApplicationContext(display),
                      "invalidClass", "xtAppCreateShell", IswCIswToolkitError,
                      "IswAppCreateShell requires non-NULL widget class",
                      NULL, NULL);
    }
    if (name == NULL) {
        IswPerDisplay pd = _IswGetPerDisplay(display);
        name = pd ? pd->name : "main";
    }

    shell = xtCreate(name, class, widget_class, (Widget) NULL,
                     _IswDefaultScreenOf(display),
                     display,
                     args, num_args, typed_args, num_typed_args,
                     (ConstraintWidgetClass) NULL, _IswAddShellToHookObj);

#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
    IswAddEventHandler(shell, (EventMask) XCB_EVENT_MASK_PROPERTY_CHANGE, FALSE,
                      _IswResourceConfigurationEH, NULL);
#endif

    return shell;
}

Widget
IswAppCreateShell(_Xconst char *name,
                 _Xconst char *class,
                 WidgetClass widget_class,
                 IswDisplay display, ArgList args, Cardinal num_args)
{
    Widget retval;
    DPY_TO_APPCON(display);

    LOCK_APP(app);
    retval = _IswAppCreateShell((String) name, (String) class, widget_class,
                               display, args, num_args, (IswTypedArgList) NULL,
                               (Cardinal) 0);
    UNLOCK_APP(app);
    return retval;
}

Widget
_IswCreateHookObj(IswScreen screen, IswDisplay dpy)
{
    Widget req_widget;
    double widget_cache[100];
    Cardinal wsize = 0;
    Widget hookobj = xtWidgetAlloc(hookObjectClass,
                                   (ConstraintWidgetClass) NULL,
                                   (Widget) NULL, "hooks",
                                   (ArgList) NULL, (Cardinal) 0,
                                   (IswTypedArgList) NULL, (Cardinal) 0);

    ((HookObject) hookobj)->hooks.screen = screen;
    ((HookObject) hookobj)->hooks.display = dpy;
    /* NOTE: Resource system is NOT Xrm-specific - it's core functionality */
    (void) _IswGetResources(hookobj, (ArgList) NULL, 0,
                           (IswTypedArgList) NULL, &wsize);
    CompileCallbacks(hookobj);
    wsize = hookObjectClass->core_class.widget_size;
    req_widget = (Widget) IswStackAlloc(wsize, widget_cache);
    (void) memcpy(req_widget, (char *) hookobj, (size_t) wsize);
    CallInitialize(hookObjectClass, req_widget, hookobj,
                   (ArgList) NULL, (Cardinal) 0);
    IswStackFree((IswPointer) req_widget, widget_cache);
    return hookobj;
}
