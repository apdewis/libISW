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

Copyright 1985, 1986, 1987, 1988, 1989, 1998  The Open Group

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
#include "StringDefs.h"
#include <ISW/IswArgMacros.h>

static String IswNxtGetTypedArg = "xtGetTypedArg";

void
IswVaGetSubresources(Widget widget,
                    IswPointer base,
                    _Xconst char *name,
                    _Xconst char *class,
                    IswResourceList resources,
                    Cardinal num_resources,
                    ...)
{
    va_list var;
    IswTypedArgList args;
    Cardinal num_args;
    int total_count, typed_count;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    va_start(var, num_resources);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, num_resources);

    _IswVaToTypedArgList(var, total_count, &args, &num_args);

    _IswGetSubresources(widget, base, name, class, resources, num_resources,
                       NULL, 0, args, num_args);

    IswFree((IswPointer) args);

    va_end(var);
    UNLOCK_APP(app);
}

void
IswVaGetApplicationResources(Widget widget,
                            IswPointer base,
                            IswResourceList resources,
                            Cardinal num_resources,
                            ...)
{
    va_list var;
    IswTypedArgList args;
    Cardinal num_args;
    int total_count, typed_count;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    va_start(var, num_resources);
    _IswCountVaList(var, &total_count, &typed_count);
    va_end(var);

    va_start(var, num_resources);

    _IswVaToTypedArgList(var, total_count, &args, &num_args);

    _IswGetApplicationResources(widget, base, resources, num_resources,
                               NULL, 0, args, num_args);

    IswFree((IswPointer) args);

    va_end(var);
    UNLOCK_APP(app);
}

static void
GetTypedArg(Widget widget,
            IswTypedArgList typed_arg,
            IswResourceList resources,
            Cardinal num_resources)
{
    String from_type = NULL;
    Cardinal from_size = 0;
    XrmValue from_val, to_val;
    register Cardinal i;
    IswArgBuilder ab = IswArgBuilderInit();
    IswPointer value;

    /* note we presume that the IswResourceList to be un-compiled */

    for (i = 0; i < num_resources; i++) {
        if (StringToName(typed_arg->name) ==
            StringToName(resources[i].resource_name)) {
            from_type = resources[i].resource_type;
            from_size = resources[i].resource_size;
            break;
        }
    }

    if (i == num_resources) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "unknownType", IswNxtGetTypedArg, IswCIswToolkitError,
                        "Unable to find type of resource for conversion",
                        NULL, NULL);
        return;
    }

    value = ALLOCATE_LOCAL(from_size);
    if (value == NULL)
        _IswAllocError(NULL);
    IswArgBuilderAdd(&ab, typed_arg->name, (IswArgVal)value);
    IswGetValues(widget, ab.args, ab.count);

    from_val.size = from_size;
    from_val.addr = (IswPointer) value;
    to_val.addr = (IswPointer) typed_arg->value;
    to_val.size = (unsigned) typed_arg->size;

    if (!IswConvertAndStore(widget, from_type, &from_val,
                           typed_arg->type, &to_val)) {
        if (to_val.size > (unsigned) typed_arg->size) {
            String params[2];
            Cardinal num_params = 2;

            params[0] = typed_arg->type;
            params[1] = IswName(widget);
            IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                            "insufficientSpace", IswNxtGetTypedArg,
                            IswCIswToolkitError,
                            "Insufficient space for converted type '%s' in widget '%s'",
                            params, &num_params);
        }
        else {
            String params[3];
            Cardinal num_params = 3;

            params[0] = from_type;
            params[1] = typed_arg->type;
            params[2] = IswName(widget);
            IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                            "conversionFailed", IswNxtGetTypedArg,
                            IswCIswToolkitError,
                            "Type conversion (%s to %s) failed for widget '%s'",
                            params, &num_params);
        }
    }
    DEALLOCATE_LOCAL(value);
}

static int
GetNestedArg(Widget widget,
             IswTypedArgList avlist,
             ArgList args,
             IswResourceList resources,
             Cardinal num_resources)
{
    int count = 0;

    for (; avlist->name != NULL; avlist++) {
        if (avlist->type != NULL) {
            GetTypedArg(widget, avlist, resources, num_resources);
        }
        else if (strcmp(avlist->name, IswVaNestedList) == 0) {
            count += GetNestedArg(widget, (IswTypedArgList) avlist->value,
                                  args, resources, num_resources);
        }
        else {
            (args + count)->name = avlist->name;
            (args + count)->value = avlist->value;
            ++count;
        }
    }

    return (count);
}

void
IswVaGetValues(Widget widget, ...)
{
    va_list var;
    String attr;
    ArgList args;
    IswTypedArg typed_arg;
    IswResourceList resources = (IswResourceList) NULL;
    Cardinal num_resources;
    int count, total_count, typed_count;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    va_start(var, widget);

    _IswCountVaList(var, &total_count, &typed_count);

    if (total_count != typed_count) {
        size_t limit = (size_t) (total_count - typed_count);

        args = IswMallocArray((Cardinal) limit, (Cardinal) sizeof(Arg));
    }
    else
        args = NULL;            /* for lint; really unused */
    va_end(var);

    if (args != NULL) {
        va_start(var, widget);
        for (attr = va_arg(var, String), count = 0; attr != NULL;
             attr = va_arg(var, String)) {
            if (strcmp(attr, IswVaTypedArg) == 0) {
                typed_arg.name = va_arg(var, String);
                typed_arg.type = va_arg(var, String);
                typed_arg.value = va_arg(var, IswArgVal);
                typed_arg.size = va_arg(var, int);

                if (resources == NULL) {
                    IswGetResourceList(IswClass(widget), &resources,
                                      &num_resources);
                }

                GetTypedArg(widget, &typed_arg, resources, num_resources);
            }
            else if (strcmp(attr, IswVaNestedList) == 0) {
                if (resources == NULL) {
                    IswGetResourceList(IswClass(widget), &resources,
                                      &num_resources);
                }

                count += GetNestedArg(widget, va_arg(var, IswTypedArgList),
                                      (args + count), resources, num_resources);
            }
            else {
                args[count].name = attr;
                args[count].value = va_arg(var, IswArgVal);
                count++;
            }
        }
        va_end(var);
    }

    IswFree((IswPointer) resources);

    if (args != NULL) {
        IswGetValues(widget, args, (Cardinal) count);
        IswFree((IswPointer) args);
    }
    UNLOCK_APP(app);
}

void
IswVaGetSubvalues(IswPointer base,
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

    if (typed_count != 0) {
        IswWarning
            ("IswVaTypedArg is an invalid argument to IswVaGetSubvalues()\n");
    }
    va_end(var);

    va_start(var, num_resources);
    _IswVaToArgList((Widget) NULL, var, total_count, &args, &num_args);
    va_end(var);

    IswGetSubvalues(base, resources, num_resources, args, num_args);

    IswFree((IswPointer) args);
}
