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
#include "StringDefs.h"
#include "Shell.h"
#include "VarargsI.h"
#include "CreateI.h"
#include "ISWPlatformPrivate.h"

static Widget
_IswVaCreateWidget(String name,
                  WidgetClass widget_class,
                  Widget parent,
                  va_list var,
                  int count)
{
    register Widget widget;
    IswTypedArgList typed_args = NULL;
    Cardinal num_args;

    _IswVaToTypedArgList(var, count, &typed_args, &num_args);

    widget = _IswCreateWidget(name, widget_class, parent, (ArgList) NULL,
                             (Cardinal) 0, typed_args, num_args);

    IswFree((IswPointer) typed_args);

    return widget;
}

Widget
IswVaCreateWidget(_Xconst char *name,
                 WidgetClass widget_class,
                 Widget parent,
                 ...)
{
    va_list var;
    register Widget widget;
    int total_count, typed_count;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    va_start(var, parent);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, parent);
    widget = _IswVaCreateWidget((String) name, widget_class, parent, var,
                               total_count);
    va_end(var);
    UNLOCK_APP(app);
    return widget;
}

Widget
IswVaCreateManagedWidget(_Xconst char *name,
                        WidgetClass widget_class,
                        Widget parent,
                        ...)
{
    va_list var;
    register Widget widget;
    int total_count, typed_count;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    va_start(var, parent);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, parent);
    widget = _IswVaCreateWidget((String) name, widget_class, parent, var,
                               total_count);
    IswManageChild(widget);
    va_end(var);
    UNLOCK_APP(app);
    return widget;
}

Widget
IswVaAppCreateShell(_Xconst char *name,
                   _Xconst char *class,
                   WidgetClass widget_class,
                   IswDisplay display,
                   ...)
{
    va_list var;
    register Widget widget;
    IswTypedArgList typed_args = NULL;
    Cardinal num_args;
    int total_count, typed_count;
    DPY_TO_APPCON(display);

    LOCK_APP(app);
    va_start(var, display);

    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, display);

    _IswVaToTypedArgList(var, total_count, &typed_args, &num_args);
    widget = _IswAppCreateShell((String) name, (String) class, widget_class,
                               display, (ArgList) NULL, (Cardinal) 0,
                               typed_args, num_args);

    IswFree((IswPointer) typed_args);

    va_end(var);
    UNLOCK_APP(app);
    return widget;
}

Widget
IswVaCreatePopupShell(_Xconst char *name,
                     WidgetClass widget_class,
                     Widget parent,
                     ...)
{
    va_list var;
    register Widget widget;
    IswTypedArgList typed_args = NULL;
    Cardinal num_args;
    int total_count, typed_count;

    WIDGET_TO_APPCON(parent);

    LOCK_APP(app);
    va_start(var, parent);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, parent);

    _IswVaToTypedArgList(var, total_count, &typed_args, &num_args);
    widget = _IswCreatePopupShell((String) name, widget_class, parent,
                                 (ArgList) NULL, (Cardinal) 0, typed_args,
                                 num_args);

    IswFree((IswPointer) typed_args);

    va_end(var);
    UNLOCK_APP(app);
    return widget;
}

void
IswVaSetValues(Widget widget, ...)
{
    va_list var;
    ArgList args = NULL;
    Cardinal num_args;
    int total_count, typed_count;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    va_start(var, widget);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, widget);

    _IswVaToArgList(widget, var, total_count, &args, &num_args);
    IswSetValues(widget, args, num_args);
    _IswFreeArgList(args, total_count, typed_count);

    UNLOCK_APP(app);
    va_end(var);
}

void
IswVaSetSubvalues(IswPointer base,
                 IswResourceList resources,
                 Cardinal num_resources,
                 ...)
{
    va_list var;
    ArgList args;
    Cardinal num_args;
    int total_count, typed_count;

    va_start(var, num_resources);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    if (typed_count != 0) {
        IswWarning("IswVaTypedArg is not valid in IswVaSetSubvalues()\n");
    }

    va_start(var, num_resources);
    _IswVaToArgList((Widget) NULL, var, total_count, &args, &num_args);

    IswSetSubvalues(base, resources, num_resources, args, num_args);

    IswFree((IswPointer) args);

    va_end(var);
}

Widget
_IswVaOpenApplication(IswAppContext *app_context_return,
                     _Xconst char *application_class,
                     XrmOptionDescList options,
                     Cardinal num_options,
                     int *argc_in_out,
                     _IswString *argv_in_out,
                     String *fallback_resources,
                     WidgetClass widget_class,
                     va_list var_args)
{
    IswAppContext app_con;
    IswDisplay dpy;
    register int saved_argc = *argc_in_out;
    Widget root;
    String attr;
    int count = 0;
    IswTypedArgList typed_args;

    IswToolkitInitialize();      /* cannot be moved into _IswAppInit */

    dpy = _IswAppInit(&app_con, (String) application_class, options, num_options,
                     argc_in_out, &argv_in_out, fallback_resources);

    typed_args = (IswTypedArgList) __XtMalloc((unsigned) sizeof(IswTypedArg));
    attr = va_arg(var_args, String);
    for (; attr != NULL; attr = va_arg(var_args, String)) {
        if (strcmp(attr, IswVaTypedArg) == 0) {
            typed_args[count].name = va_arg(var_args, String);
            typed_args[count].type = va_arg(var_args, String);
            typed_args[count].value = va_arg(var_args, IswArgVal);
            typed_args[count].size = va_arg(var_args, int);
        }
        else {
            typed_args[count].name = attr;
            typed_args[count].type = NULL;
            typed_args[count].value = va_arg(var_args, IswArgVal);
            typed_args[count].size = 0;
        }
        count++;
        typed_args = IswReallocArray(typed_args, (Cardinal) count + 1,
                                    (Cardinal) sizeof(IswTypedArg));
    }
    typed_args[count].name = NULL;

    va_end(var_args);

    IswScreen def_screen = _IswDefaultScreenOf(dpy);
    root =
        IswVaAppCreateShell(NULL, application_class,
                           widget_class, (IswDisplay) dpy,
                           IswNscreen, (IswArgVal) def_screen,
                           IswNargc, (IswArgVal) saved_argc,
                           IswNargv, (IswArgVal) argv_in_out,
                           IswVaNestedList, (IswVarArgsList) typed_args, NULL);
    
    if (app_context_return != NULL)
        *app_context_return = app_con;

    IswFree((IswPointer) typed_args);
    IswFree((IswPointer) argv_in_out);
    return (root);
}

Widget
_IswVaAppInitialize(IswAppContext *app_context_return,
                   _Xconst char *application_class,
                   XrmOptionDescList options,
                   Cardinal num_options,
                   int *argc_in_out,
                   _IswString *argv_in_out,
                   String *fallback_resources,
                   va_list var_args)
{
    return _IswVaOpenApplication(app_context_return, application_class,
                                options, num_options,
                                argc_in_out, argv_in_out, fallback_resources,
                                applicationShellWidgetClass, var_args);
}

/*
 * If not used as a shared library, we still need a front end to
 * _IswVaOpenApplication and to _IswVaAppInitialize.
 */

Widget
IswVaOpenApplication(IswAppContext *app_context_return,
                    _Xconst char *application_class,
                    XrmOptionDescList options,
                    Cardinal num_options,
                    int *argc_in_out,
                    _IswString *argv_in_out,
                    String *fallback_resources,
                    WidgetClass widget_class,
                    ...)
{
    Widget code;
    va_list var;

    va_start(var, widget_class);
    code = _IswVaOpenApplication(app_context_return, (String) application_class,
                                options, num_options, argc_in_out, argv_in_out,
                                fallback_resources, widget_class, var);
    va_end(var);
    return code;
}

Widget
IswVaAppInitialize(IswAppContext *app_context_return,
                  _Xconst char *application_class,
                  XrmOptionDescList options,
                  Cardinal num_options,
                  int *argc_in_out,
                  _IswString *argv_in_out,
                  String *fallback_resources,
                  ...)
{
    Widget code;
    va_list var;

    va_start(var, fallback_resources);
    code = _IswVaOpenApplication(app_context_return, (String) application_class,
                                options, num_options, argc_in_out, argv_in_out,
                                fallback_resources,
                                applicationShellWidgetClass, var);
    va_end(var);
    return code;
}
