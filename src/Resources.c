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
#include <ISW/ISWPlatform.h>
#include "VarargsI.h"
#include "Shell.h"
#include "ShellP.h"
#include "StringDefs.h"
#include <stdio.h>

/* Static quarks used for resource lookup */
static XrmClass QBoolean, QString, QCallProc, QImmediate;
static XrmName QinitialResourcesPersistent, QInitialResourcesPersistent;
static XrmClass QTranslations, QTranslationTable;
static XrmName Qtranslations, QbaseTranslations;
static XrmName Qscreen;
static XrmClass QScreen;

/* Maximum depth for widget hierarchies */
#define MAX_WIDGET_DEPTH 100

/*
 * Structure to hold widget hierarchy path strings for resource lookup.
 * Instead of using quark lists and XrmQGetSearchList/XrmQGetSearchResource,
 * we build full resource name/class strings for _IswPlatformResourceGetString().
 */
typedef struct {
    String *names;      /* Array of widget name strings */
    String *classes;    /* Array of widget class strings */
    Cardinal depth;     /* Number of widgets in hierarchy */
} IswResourcePath;

void
_IswCopyFromParent(Widget widget, int offset, XrmValue *value)
{
    if (widget->core.parent == NULL) {
        /* Toplevel shell — no parent to copy from.
         * Supply screen defaults for depth and colormap. */
        static int default_depth;
        static IswColormap default_colormap;
        int colormap_offset = (int) IswOffsetOf(CoreRec, core.colormap);
        int depth_offset = (int) IswOffsetOf(CoreRec, core.depth);

        if (offset == colormap_offset && widget->core.screen != NULL) {
            default_colormap = _IswPlatformScreenDefaultColormap(
                IswDisplayOfObject(widget), widget->core.screen);
            value->addr = (IswPointer) &default_colormap;
            return;
        }
        if (offset == depth_offset && widget->core.screen != NULL) {
            default_depth = _IswPlatformScreenDepth(
                IswDisplayOfObject(widget), widget->core.screen);
            value->addr = (IswPointer) &default_depth;
            return;
        }
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidParent", "xtCopyFromParent", IswCIswToolkitError,
                        "CopyFromParent must have non-NULL parent", NULL, NULL);
        value->addr = NULL;
        return;
    }
    value->addr = (IswPointer) (((char *) widget->core.parent) + offset);
}                               /* _IswCopyFromParent */

void
_IswCopyFromArg(IswArgVal src, char *dst, register unsigned int size)
{
    if (size > sizeof(IswArgVal))
        (void) memmove((char *) dst, (char *) src, (size_t) size);
    else {
        union {
            long longval;
#ifdef LONG64
            int intval;
#endif
            short shortval;
            char charval;
            char *charptr;
            IswPointer ptr;
        } u;
        char *p = (char *) &u;

        if (size == sizeof(long))
            u.longval = (long) src;
#ifdef LONG64
        else if (size == sizeof(int))
            u.intval = (int) src;
#endif
        else if (size == sizeof(short))
            u.shortval = (short) src;
        else if (size == sizeof(char))
            u.charval = (char) src;
        else if (size == sizeof(IswPointer))
            u.ptr = (IswPointer) src;
        else if (size == sizeof(char *))
            u.charptr = (char *) src;
        else
            p = (char *) &src;

        (void) memcpy(dst, p, (size_t) size);
    }
}                               /* _IswCopyFromArg */

void
_IswCopyToArg(char *src, IswArgVal *dst, register unsigned int size)
{
    if (!*dst) {
#ifdef GETVALUES_BUG
        /* old GetValues semantics (storing directly into arglists) are bad,
         * but preserve for compatibility as long as arglist contains NULL.
         */
        union {
            long longval;
#ifdef LONG64
            int intval;
#endif
            short shortval;
            char charval;
            char *charptr;
            IswPointer ptr;
        } u;

        if (size <= sizeof(IswArgVal)) {
            (void) memcpy(&u, src, (size_t) size);
            if (size == sizeof(long))
                *dst = (IswArgVal) u.longval;
#ifdef LONG64
            else if (size == sizeof(int))
                *dst = (IswArgVal) u.intval;
#endif
            else if (size == sizeof(short))
                *dst = (IswArgVal) u.shortval;
            else if (size == sizeof(char))
                *dst = (IswArgVal) u.charval;
            else if (size == sizeof(char *))
                *dst = (IswArgVal) u.charptr;
            else if (size == sizeof(IswPointer))
                *dst = (IswArgVal) u.ptr;
            else
                (void) memmove((char *) dst, (char *) src, (size_t) size);
        }
        else
            (void) memmove((char *) dst, (char *) src, (size_t) size);
#else
        IswErrorMsg("invalidGetValues", "xtGetValues", IswCIswToolkitError,
                   "NULL ArgVal in IswGetValues", NULL, NULL);
#endif
    }
    else {
        /* proper GetValues semantics: argval is pointer to destination */
        (void) memmove((char *) *dst, (char *) src, (size_t) size);
    }
}                               /* _IswCopyToArg */

static void
CopyToArg(char *src, IswArgVal *dst, register unsigned int size)
{
    if (!*dst) {
        /* old GetValues semantics (storing directly into arglists) are bad,
         * but preserve for compatibility as long as arglist contains NULL.
         */
        union {
            long longval;
#ifdef LONG64
            int intval;
#endif
            short shortval;
            char charval;
            char *charptr;
            IswPointer ptr;
        } u;

        if (size <= sizeof(IswArgVal)) {
            (void) memcpy(&u, src, (size_t) size);
            if (size == sizeof(long))
                *dst = (IswArgVal) u.longval;
#ifdef LONG64
            else if (size == sizeof(int))
                *dst = (IswArgVal) u.intval;
#endif
            else if (size == sizeof(short))
                *dst = (IswArgVal) u.shortval;
            else if (size == sizeof(char))
                *dst = (IswArgVal) u.charval;
            else if (size == sizeof(char *))
                *dst = (IswArgVal) u.charptr;
            else if (size == sizeof(IswPointer))
                *dst = (IswArgVal) u.ptr;
            else
                (void) memmove((char *) dst, (char *) src, (size_t) size);
        }
        else
            (void) memmove((char *) dst, (char *) src, (size_t) size);
    }
    else {
        /* proper GetValues semantics: argval is pointer to destination */
        (void) memmove((char *) *dst, (char *) src, (size_t) size);
    }
}                               /* CopyToArg */

static Cardinal
CountTreeDepth(Widget w)
{
    Cardinal count;

    for (count = 1; w != NULL; w = (Widget) w->core.parent)
        count++;

    return count;
}

static void
GetNamesAndClasses(register Widget w,
                   register XrmNameList names,
                   register XrmClassList classes)
{
    register Cardinal length, j;
    register XrmQuark t;
    WidgetClass class;

    /* Return null-terminated quark arrays, with length the number of
       quarks (not including NULL) */

    LOCK_PROCESS;
    for (length = 0; w != NULL; w = (Widget) w->core.parent) {
        names[length] = w->core.xrm_name;
        class = IswClass(w);
        /* KLUDGE KLUDGE KLUDGE KLUDGE */
        if (w->core.parent == NULL && IswIsApplicationShell(w)) {
            classes[length] =
                XrmPermStringToQuark(
                    ((ApplicationShellWidget) w)->application.class);
        }
        else
            classes[length] = class->core_class.xrm_class;
        length++;
    }
    UNLOCK_PROCESS;
    /* They're in backwards order, flop them around */
    for (j = 0; j < length / 2; j++) {
        t = names[j];
        names[j] = names[length - j - 1];
        names[length - j - 1] = t;
        t = classes[j];
        classes[j] = classes[length - j - 1];
        classes[length - j - 1] = t;
    }
    names[length] = NULLQUARK;
    classes[length] = NULLQUARK;
}                               /* GetNamesAndClasses */

/*
 * _IswBuildResourcePath - Build a dot-separated resource path string
 * from a null-terminated quark list plus a final resource name quark.
 * Returns a newly allocated string that must be freed by the caller.
 */
static char *
_IswBuildResourcePath(XrmQuarkList quarks, XrmQuark resource_quark)
{
    char buf[2048];
    int pos = 0;
    int i;

    for (i = 0; quarks[i] != NULLQUARK; i++) {
        const char *s = XrmQuarkToString(quarks[i]);
        if (s == NULL) continue;
        if (pos > 0 && pos < (int)sizeof(buf) - 1)
            buf[pos++] = '.';
        while (*s && pos < (int)sizeof(buf) - 1)
            buf[pos++] = *s++;
    }
    /* Append the resource name */
    {
        const char *s = XrmQuarkToString(resource_quark);
        if (s != NULL) {
            if (pos > 0 && pos < (int)sizeof(buf) - 1)
                buf[pos++] = '.';
            while (*s && pos < (int)sizeof(buf) - 1)
                buf[pos++] = *s++;
        }
    }
    buf[pos] = '\0';
    return IswNewString(buf);
}

/*
 * _IswDbGetResource - Look up a resource in the xcb-util-xrm database
 * using full name and class path strings.
 * Returns True if found, with value->addr set to the string value.
 * The caller must free value->addr when done.
 */
static Boolean
_IswDbGetResource(IswDatabaseHandle db,
                 XrmNameList names, XrmClassList classes,
                 XrmName res_name, XrmClass res_class,
                 XrmValue *value)
{
    char *name_path;
    char *class_path;
    char *result = NULL;

    if (db == NULL)
        return False;

    name_path = _IswBuildResourcePath(names, res_name);
    class_path = _IswBuildResourcePath(classes, res_class);

    if (_IswPlatformResourceGetString(db, name_path, class_path,
                                     &result) >= 0 && result != NULL) {
        value->addr = (IswPointer) result;
        value->size = (unsigned int) strlen(result) + 1;
        IswFree(name_path);
        IswFree(class_path);
        return True;
    }

    IswFree(name_path);
    IswFree(class_path);
    return False;
}

/* Spiffy fast compiled form of resource list.                          */
/* IswResourceLists are compiled in-place into XrmResourceLists          */
/* All atoms are replaced by quarks, and offsets are -offset-1 to       */
/* indicate that this list has been compiled already                    */

void
_IswCompileResourceList(register IswResourceList resources,
                       Cardinal num_resources)
{
    register Cardinal count;

#define xrmres  ((XrmResourceList) resources)
#define PSToQ   XrmPermStringToQuark

    for (count = 0; count < num_resources; resources++, count++) {
        xrmres->xrm_name = PSToQ(resources->resource_name);
        xrmres->xrm_class = PSToQ(resources->resource_class);
        xrmres->xrm_type = PSToQ(resources->resource_type);
        xrmres->xrm_offset = (int)
            (-(int) resources->resource_offset - 1);
        xrmres->xrm_default_type = PSToQ(resources->default_type);
    }
#undef PSToQ
#undef xrmres
}                               /* _IswCompileResourceList */

/* Like _IswCompileResourceList, but strings are not permanent */
static void
XrmCompileResourceListEphem(register IswResourceList resources,
                            Cardinal num_resources)
{
    register Cardinal count;

#define xrmres  ((XrmResourceList) resources)

    for (count = 0; count < num_resources; resources++, count++) {
        xrmres->xrm_name = StringToName(resources->resource_name);
        xrmres->xrm_class = StringToClass(resources->resource_class);
        xrmres->xrm_type = StringToQuark(resources->resource_type);
        xrmres->xrm_offset = (int)
            (-(int) resources->resource_offset - 1);
        xrmres->xrm_default_type = StringToQuark(resources->default_type);
    }
#undef xrmres
}                               /* XrmCompileResourceListEphem */

static void
BadSize(Cardinal size, XrmQuark name)
{
    String params[2];
    Cardinal num_params = 2;

    params[0] = (String) (IswIntPtr) size;
    params[1] = XrmQuarkToString(name);
    IswWarningMsg("invalidSizeOverride", "xtDependencies", IswCIswToolkitError,
                 "Representation size %d must match superclass's to override %s",
                 params, &num_params);
}                               /* BadSize */

/*
 * Create a new resource list, with the class resources following the
 * superclass's resources.  If a resource in the class list overrides
 * a superclass resource, then just replace the superclass entry in place.
 *
 * At the same time, add a level of indirection to the IswResourceList to
 * create and XrmResourceList.
 */
void
_IswDependencies(IswResourceList *class_resp,    /* VAR */
                Cardinal *class_num_resp,      /* VAR */
                XrmResourceList *super_res,
                Cardinal super_num_res,
                Cardinal super_widget_size)
{
    register XrmResourceList *new_res;
    Cardinal new_num_res;
    XrmResourceList class_res = (XrmResourceList) *class_resp;
    Cardinal class_num_res = *class_num_resp;
    register Cardinal i, j;
    Cardinal new_next;

    if (class_num_res == 0) {
        /* Just point to superclass resource list */
        *class_resp = (IswResourceList) super_res;
        *class_num_resp = super_num_res;
        return;
    }

    /* Allocate and initialize new_res with superclass resource pointers */
    new_num_res = super_num_res + class_num_res;
    new_res = IswMallocArray(new_num_res, (Cardinal) sizeof(XrmResourceList));
    if (super_num_res > 0)
        memcpy(new_res, super_res, super_num_res * sizeof(XrmResourceList));

    /* Put pointers to class resource entries into new_res */
    new_next = super_num_res;
    for (i = 0; i < class_num_res; i++) {
        if ((Cardinal) (-class_res[i].xrm_offset - 1) < super_widget_size) {
            /* Probably an override of superclass resources--look for overlap */
            for (j = 0; j < super_num_res; j++) {
                if (class_res[i].xrm_offset == new_res[j]->xrm_offset) {
                    /* Spec is silent on what fields subclass can override.
                     * The only two of real concern are type & size.
                     * Although allowing type to be over-ridden introduces
                     * the possibility of errors, it's at present the only
                     * reasonable way to allow a subclass to force a private
                     * converter to be invoked for a subset of fields.
                     */
                    /* We do insist that size be identical to superclass */
                    if (class_res[i].xrm_size != new_res[j]->xrm_size) {
                        BadSize(class_res[i].xrm_size,
                                (XrmQuark) class_res[i].xrm_name);
                        class_res[i].xrm_size = new_res[j]->xrm_size;
                    }
                    new_res[j] = &(class_res[i]);
                    new_num_res--;
                    goto NextResource;
                }
            }                   /* for j */
        }
        /* Not an overlap, add an entry to new_res */
        new_res[new_next++] = &(class_res[i]);
 NextResource:;
    }                           /* for i */

    /* Okay, stuff new resources back into class record */
    *class_resp = (IswResourceList) new_res;
    *class_num_resp = new_num_res;
}                               /* _IswDependencies */

void
_IswResourceDependencies(WidgetClass wc)
{
    WidgetClass sc;

    sc = wc->core_class.superclass;
    if (sc == NULL) {
        _IswDependencies(&(wc->core_class.resources),
                        &(wc->core_class.num_resources),
                        (XrmResourceList *) NULL, (unsigned) 0, (unsigned) 0);
    }
    else {
        _IswDependencies(&(wc->core_class.resources),
                        &(wc->core_class.num_resources),
                        (XrmResourceList *) sc->core_class.resources,
                        sc->core_class.num_resources,
                        sc->core_class.widget_size);
    }
}                               /* _IswResourceDependencies */

void
_IswConstraintResDependencies(ConstraintWidgetClass wc)
{
    if (wc == (ConstraintWidgetClass) constraintWidgetClass) {
        _IswDependencies(&(wc->constraint_class.resources),
                        &(wc->constraint_class.num_resources),
                        (XrmResourceList *) NULL, (unsigned) 0, (unsigned) 0);
    }
    else {
        ConstraintWidgetClass sc;

        sc = (ConstraintWidgetClass) wc->core_class.superclass;
        _IswDependencies(&(wc->constraint_class.resources),
                        &(wc->constraint_class.num_resources),
                        (XrmResourceList *) sc->constraint_class.resources,
                        sc->constraint_class.num_resources,
                        sc->constraint_class.constraint_size);
    }
}                               /* _IswConstraintResDependencies */

XrmResourceList *
_IswCreateIndirectionTable(IswResourceList resources, Cardinal num_resources)
{
    register Cardinal idx;
    XrmResourceList *table;

    table = IswMallocArray(num_resources, (Cardinal) sizeof(XrmResourceList));
    for (idx = 0; idx < num_resources; idx++)
        table[idx] = (XrmResourceList) (&(resources[idx]));
    return table;
}

static IswCacheRef *
GetResources(Widget widget,             /* Widget resources are associated with */
             char *base,                /* Base address of memory to write to */
             XrmNameList names,         /* Full inheritance name of widget */
             XrmClassList classes,      /* Full inheritance class of widget     */
             XrmResourceList *table,    /* The list of resources required.      */
             unsigned num_resources,    /* number of items in resource list     */
             XrmQuarkList quark_args,   /* Arg names quarkified                 */
             ArgList args,              /* ArgList to override resources */
             unsigned num_args,         /* number of items in arg list  */
             IswTypedArgList typed_args, /* Typed arg list to override resources */
             Cardinal *pNumTypedArgs,   /* number of items in typed arg list    */
             Boolean tm_hack)           /* do baseTranslations                  */
{            
/*
 * assert: *pNumTypedArgs == 0 if num_args > 0
 * assert: num_args == 0 if *pNumTypedArgs > 0
 */
#define SEARCHLISTLEN 100
#define MAXRESOURCES  400

    XrmValue value;
    XrmQuark rawType;
    XrmValue convValue;
    Boolean found[MAXRESOURCES];
    int typed[MAXRESOURCES];
    IswCacheRef cache_ref[MAXRESOURCES];
    IswCacheRef *cache_ptr, *cache_base;
    Boolean persistent_resources = True;
    Boolean found_persistence = False;
    int num_typed_args = (int) *pNumTypedArgs;
    IswDatabaseHandle db;
    Boolean do_tm_hack = False;

    if ((args == NULL) && (num_args != 0)) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidArgCount", "getResources", IswCIswToolkitError,
                        "argument count > 0 on NULL argument list", NULL, NULL);
        num_args = 0;
    }
    if (num_resources == 0) {
        return NULL;
    }
    else if (num_resources >= MAXRESOURCES) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidResourceCount", "getResources",
                        IswCIswToolkitError, "too many resources", NULL, NULL);
        return NULL;
    }
    else if (table == NULL) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidResourceCount", "getResources",
                        IswCIswToolkitError,
                        "resource count > 0 on NULL resource list", NULL, NULL);
        return NULL;
    }

    /* Mark each resource as not found on arg list */
    memset((void *) found, 0, (size_t) (num_resources * sizeof(Boolean)));
    memset((void *) typed, 0, (size_t) (num_resources * sizeof(int)));

    /* Copy the args into the resources, mark each as found */
    {
        register ArgList arg;
        register IswTypedArgList typed_arg;
        register XrmName argName;
        register Cardinal j;
        register int i;
        register XrmResourceList rx;
        register XrmResourceList *res;

        for (arg = args, i = 0; (Cardinal) i < num_args; i++, arg++) {
            argName = quark_args[i];
            if (argName == QinitialResourcesPersistent) {
                persistent_resources = (Boolean) arg->value;
                found_persistence = True;
                continue;
            }
            for (j = 0, res = table; j < num_resources; j++, res++) {
                rx = *res;
                if (argName == rx->xrm_name) {
                    _IswCopyFromArg(arg->value,
                                   base - rx->xrm_offset - 1, rx->xrm_size);
                    found[j] = TRUE;
                    break;
                }
            }
        }
        for (typed_arg = typed_args, i = 0; i < num_typed_args;
             i++, typed_arg++) {
            register XrmRepresentation argType;

            argName = quark_args[i];
            argType = (typed_arg->type == NULL) ? NULLQUARK
                : XrmStringToRepresentation(typed_arg->type);
            if (argName == QinitialResourcesPersistent) {
                persistent_resources = (Boolean) typed_arg->value;
                found_persistence = True;
                break;
            }
            for (j = 0, res = table; j < num_resources; j++, res++) {
                rx = *res;
                if (argName == rx->xrm_name) {
                    if (argType != NULLQUARK && argType != rx->xrm_type) {
                        typed[j] = i + 1;
                    }
                    else {
                        _IswCopyFromArg(typed_arg->value,
                                       base - rx->xrm_offset - 1, rx->xrm_size);
                    }
                    found[j] = TRUE;
                    break;
                }
            }
        }
    }

    /* Ask resource manager for a list of database levels that we can
       do a single-level search on each resource */

    db = IswScreenDatabase(IswScreenOfObject(widget));

    if (persistent_resources)
        cache_base = NULL;
    else
        cache_base = cache_ref;
    /* geez, this is an ugly mess */
    if (IswIsShell(widget)) {
        register XrmResourceList *res;
        register Cardinal j;
        IswScreen oldscreen = widget->core.screen;

        /* look up screen resource first, since real rdb depends on it */
        for (res = table, j = 0; j < num_resources; j++, res++) {
            if ((*res)->xrm_name != Qscreen)
                continue;
            if (typed[j]) {
                register IswTypedArg *arg = typed_args + typed[j] - 1;
                XrmQuark from_type;
                XrmValue from_val, to_val;

                from_type = StringToQuark(arg->type);
                from_val.size = (Cardinal) arg->size;
                if ((from_type == QString) ||
                    ((unsigned) arg->size > sizeof(IswArgVal)))
                    from_val.addr = (IswPointer) arg->value;
                else
                    from_val.addr = (IswPointer) &arg->value;
                to_val.size = sizeof(IswScreen);
                to_val.addr = (IswPointer) &widget->core.screen;
                found[j] = _IswConvert(widget, from_type, &from_val,
                                      QScreen, &to_val, cache_base);
                if (cache_base && *cache_base)
                    cache_base++;
            }
            if (!found[j]) {
                if (_IswDbGetResource(db, names, classes, Qscreen, QScreen,
                                     &value)) {
                    /* xcb-util-xrm returns strings; need conversion */
                    rawType = QString;
                    convValue.size = sizeof(IswScreen);
                    convValue.addr = (IswPointer) &widget->core.screen;
                    (void) _IswConvert(widget, rawType, &value,
                                      QScreen, &convValue, cache_base);
                    if (cache_base && *cache_base)
                        cache_base++;
                    free(value.addr);
                }
            }
            break;
        }
        /* now get the database to use for the rest of the resources */
        if (widget->core.screen != oldscreen) {
            db = IswScreenDatabase(widget->core.screen);
        }
    }

    /* go to the resource manager for those resources not found yet */
    /* if it's not in the resource database use the default value   */

    {
        register XrmResourceList rx;
        register XrmResourceList *res;
        register Cardinal j;
        register XrmRepresentation xrm_type;
        register XrmRepresentation xrm_default_type;
        char char_val;
        short short_val;
        int int_val;
        long long_val;
        char *char_ptr;

        if (!found_persistence) {
            if (_IswDbGetResource(db, names, classes,
                                 QinitialResourcesPersistent,
                                 QInitialResourcesPersistent, &value)) {
                /* xcb-util-xrm returns strings; convert to Boolean */
                rawType = QString;
                convValue.size = sizeof(Boolean);
                convValue.addr = (IswPointer) &persistent_resources;
                (void) _IswConvert(widget, rawType, &value, QBoolean,
                                  &convValue, NULL);
                free(value.addr);
            }
        }
        if (persistent_resources)
            cache_ptr = NULL;
        else if (cache_base)
            cache_ptr = cache_base;
        else
            cache_ptr = cache_ref;

        for (res = table, j = 0; j < num_resources; j++, res++) {
            rx = *res;
            xrm_type = (XrmRepresentation) rx->xrm_type;
            if (typed[j]) {
                register IswTypedArg *arg = typed_args + typed[j] - 1;

                /*
                 * This resource value has been specified as a typed arg and
                 * has to be converted. Typed arg conversions are done here
                 * to correctly interpose them with normal resource conversions.
                 */
                XrmQuark from_type;
                XrmValue from_val, to_val;
                Boolean converted;

                from_type = StringToQuark(arg->type);
                from_val.size = (Cardinal) arg->size;
                if ((from_type == QString) ||
                    ((unsigned) arg->size > sizeof(IswArgVal)))
                    from_val.addr = (IswPointer) arg->value;
                else
                    from_val.addr = (IswPointer) &arg->value;
                to_val.size = rx->xrm_size;
                to_val.addr = base - rx->xrm_offset - 1;
                converted = _IswConvert(widget, from_type, &from_val,
                                       xrm_type, &to_val, cache_ptr);
                if (converted) {

                    /* Copy the converted value back into the typed argument.
                     * normally the data should be <= sizeof(IswArgVal) and
                     * is stored directly into the 'value' field .... BUT
                     * if the resource size is greater than sizeof(IswArgVal)
                     * then we dynamically alloc a block of store to hold the
                     * data and zap a copy in there !!! .... freeing it later
                     * the size field in the typed arg is negated to indicate
                     * that the store pointed to by the value field is
                     * dynamic .......
                     * "freeing" happens in the case of _IswCreate after the
                     * CallInitialize ..... other clients of GetResources
                     * using typed args should be aware of the need to free
                     * this store .....
                     */

                    if (rx->xrm_size > sizeof(IswArgVal)) {
                        arg->value =
                            (IswArgVal) (void *) __XtMalloc(rx->xrm_size);
                        arg->size = -(arg->size);
                    }
                    else {      /* will fit - copy directly into value field */
                        arg->value = (IswArgVal) NULL;
                    }
                    CopyToArg((char *) (base - rx->xrm_offset - 1),
                              &arg->value, rx->xrm_size);

                }
                else {
                    /* Conversion failed. Get default value. */
                    found[j] = False;
                }

                if (cache_ptr && *cache_ptr)
                    cache_ptr++;
            }

            if (!found[j]) {
                Boolean already_copied = False;
                Boolean have_value = False;

                if (_IswDbGetResource(db, names, classes,
                                     (XrmName) rx->xrm_name,
                                     (XrmClass) rx->xrm_class, &value)) {
                    /* xcb-util-xrm always returns strings */
                    rawType = QString;
                    if (rawType != xrm_type) {
                        convValue.size = rx->xrm_size;
                        convValue.addr = (IswPointer) (base - rx->xrm_offset - 1);
                        /* Watchpoint: check if this resource write corrupts core.display */
                {
                    int actual_offset = -(rx->xrm_offset) - 1;
                    int display_offset = 160; /* offsetof(WidgetRec, core.display) */
                    if (actual_offset <= display_offset + 7 && actual_offset + (int)rx->xrm_size > display_offset) {
                    }
                }
                already_copied = have_value =
                            _IswConvert(widget, rawType, &value,
                                       xrm_type, &convValue, cache_ptr);
                        if (cache_ptr && *cache_ptr)
                            cache_ptr++;
                    }
                    else
                        have_value = True;
                    if (have_value && rx->xrm_name == Qtranslations)
                        do_tm_hack = True;
                    free(value.addr);
                }
                LOCK_PROCESS;
                if (!have_value && ((rx->xrm_default_type == QImmediate)
                                    || (rx->xrm_default_type == xrm_type)
                                    || (rx->xrm_default_addr != NULL))) {
                    /* Convert default value to proper type */
                    xrm_default_type = (XrmRepresentation) rx->xrm_default_type;
                    if (xrm_default_type == QCallProc) {
                        (*(IswResourceDefaultProc) (rx->xrm_default_addr))
                            (widget, -(rx->xrm_offset + 1), &value);

                    }
                    else if (xrm_default_type == QImmediate) {
                        /* IswRImmediate == IswRString for type IswRString */
                        if (xrm_type == QString) {
                            value.addr = rx->xrm_default_addr;
                        }
                        else if (rx->xrm_size == sizeof(int)) {
                            int_val = (int) (long) rx->xrm_default_addr;
                            value.addr = (IswPointer) &int_val;
                        }
                        else if (rx->xrm_size == sizeof(short)) {
                            short_val = (short) (long) rx->xrm_default_addr;
                            value.addr = (IswPointer) &short_val;
                        }
                        else if (rx->xrm_size == sizeof(char)) {
                            char_val = (char) (long) rx->xrm_default_addr;
                            value.addr = (IswPointer) &char_val;
                        }
                        else if (rx->xrm_size == sizeof(long)) {
                            long_val = (long) rx->xrm_default_addr;
                            value.addr = (IswPointer) &long_val;
                        }
                        else if (rx->xrm_size == sizeof(char *)) {
                            char_ptr = (char *) rx->xrm_default_addr;
                            value.addr = (IswPointer) &char_ptr;
                        }
                        else {
                            value.addr = (IswPointer) &(rx->xrm_default_addr);
                        }
                    }
                    else if (xrm_default_type == xrm_type) {
                        value.addr = rx->xrm_default_addr;
                    }
                    else {
                        value.addr = rx->xrm_default_addr;
                        if (xrm_default_type == QString) {
                            value.size =
                                (unsigned) strlen((char *) value.addr) + 1;
                        }
                        else {
                            value.size = sizeof(IswPointer);
                        }
                        convValue.size = rx->xrm_size;
                        convValue.addr = (IswPointer) (base - rx->xrm_offset - 1);
                        already_copied =
                            _IswConvert(widget, xrm_default_type, &value,
                                       xrm_type, &convValue, cache_ptr);
                        if (!already_copied)
                            value.addr = NULL;
                        if (cache_ptr && *cache_ptr)
                            cache_ptr++;
                    }
                }
                if (!already_copied) {
                    if (xrm_type == QString) {
                        *((String *) (base - rx->xrm_offset - 1)) = value.addr;
                    }
                    else {
                        if (value.addr != NULL) {
                            IswMemmove(base - rx->xrm_offset - 1,
                                      value.addr, rx->xrm_size);
                        }
                        else {
                            /* didn't get value, initialize to NULL... */
                            IswBZero(base - rx->xrm_offset - 1, rx->xrm_size);
                        }
                    }
                }
                UNLOCK_PROCESS;
            }
        }
        for (res = table, j = 0; j < num_resources; j++, res++) {
            if (!found[j] && typed[j]) {
                /*
                 * This resource value was specified as a typed arg.
                 * However, the default value is being used here since
                 * type type conversion failed, so we compress the list.
                 */
                register IswTypedArg *arg = typed_args + typed[j] - 1;
                register int i;

                for (i = num_typed_args - typed[j]; i > 0; i--, arg++) {
                    *arg = *(arg + 1);
                }
                num_typed_args--;
            }
        }
        if (tm_hack)
            widget->core.tm.current_state = NULL;
        if (tm_hack &&
            (!widget->core.tm.translations ||
             (do_tm_hack &&
              widget->core.tm.translations->operation != IswTableReplace)) &&
            _IswDbGetResource(db, names, classes, QbaseTranslations,
                             QTranslations, &value)) {
            /* xcb-util-xrm returns strings; need conversion to TranslationTable */
            rawType = QString;
            convValue.size = sizeof(IswTranslations);
            convValue.addr = (IswPointer) &widget->core.tm.current_state;
            (void) _IswConvert(widget, rawType, &value,
                              QTranslationTable, &convValue, cache_ptr);
            if (cache_ptr && *cache_ptr)
                cache_ptr++;
            free(value.addr);
        }
    }
    if ((Cardinal) num_typed_args != *pNumTypedArgs)
        *pNumTypedArgs = (Cardinal) num_typed_args;
    /* No search list to free - xcb-util-xrm doesn't use search lists */
    if (!cache_ptr)
        cache_ptr = cache_base;
    if (cache_ptr && cache_ptr != cache_ref) {
        int cache_ref_size = (int) (cache_ptr - cache_ref);
        IswCacheRef *refs = IswMallocArray((Cardinal) cache_ref_size + 1,
                                         (Cardinal) sizeof(IswCacheRef));

        (void) memcpy(refs, cache_ref,
                      sizeof(IswCacheRef) * (size_t) cache_ref_size);
        refs[cache_ref_size] = NULL;
        return refs;
    }
    return (IswCacheRef *) NULL;
}

static void
CacheArgs(ArgList args,
          Cardinal num_args,
          IswTypedArgList typed_args,
          Cardinal num_typed_args,
          XrmQuarkList quark_cache,
          Cardinal num_quarks,
          XrmQuarkList *pQuarks)        /* RETURN */
{        
    register XrmQuarkList quarks;
    register Cardinal i;
    register Cardinal count;

    count = (args != NULL) ? num_args : num_typed_args;

    if (num_quarks < count) {
        quarks = IswMallocArray(count, (Cardinal) sizeof(XrmQuark));
    }
    else {
        quarks = quark_cache;
    }
    *pQuarks = quarks;

    if (args != NULL) {
        for (i = count; i; i--)
            *quarks++ = StringToQuark((args++)->name);
    }
    else {
        for (i = count; i; i--)
            *quarks++ = StringToQuark((typed_args++)->name);
    }
}

#define FreeCache(cache, pointer) \
          if (cache != pointer) IswFree((char *)pointer)

IswCacheRef *
_IswGetResources(register Widget w,
                ArgList args,
                Cardinal num_args,
                IswTypedArgList typed_args,
                Cardinal *num_typed_args)
{
    XrmName *names, names_s[50];
    XrmClass *classes, classes_s[50];
    XrmQuark quark_cache[100];
    XrmQuarkList quark_args;
    WidgetClass wc;
    IswCacheRef *cache_refs = NULL;
    Cardinal count;

    wc = IswClass(w);

    count = CountTreeDepth(w);
    names = (XrmName *) IswStackAlloc(count * sizeof(XrmName), names_s);
    classes = (XrmClass *) IswStackAlloc(count * sizeof(XrmClass), classes_s);
    if (names == NULL || classes == NULL) {
        _IswAllocError(NULL);
    }
    else {

        /* Get names, classes for widget and ancestors */
        GetNamesAndClasses(w, names, classes);

        /* Compile arg list into quarks */
        CacheArgs(args, num_args, typed_args, *num_typed_args, quark_cache,
                  IswNumber(quark_cache), &quark_args);

        /* Get normal resources */
        LOCK_PROCESS;
        cache_refs = GetResources(w, (char *) w, names, classes,
                                  (XrmResourceList *) wc->core_class.resources,
                                  wc->core_class.num_resources, quark_args,
                                  args, num_args, typed_args, num_typed_args,
                                  IswIsWidget(w));

        if (w->core.constraints != NULL) {
            ConstraintWidgetClass cwc;
            IswCacheRef *cache_refs_core;

            cwc = (ConstraintWidgetClass) IswClass(w->core.parent);
            cache_refs_core =
                GetResources(w, (char *) w->core.constraints, names, classes,
                             (XrmResourceList *) cwc->constraint_class.
                             resources, cwc->constraint_class.num_resources,
                             quark_args, args, num_args, typed_args,
                             num_typed_args, False);
            IswFree((char *) cache_refs_core);
        }
        FreeCache(quark_cache, quark_args);
        UNLOCK_PROCESS;
        IswStackFree((IswPointer) names, names_s);
        IswStackFree((IswPointer) classes, classes_s);
    }
    /* Check if core.display was corrupted by resource processing */
    if (IswIsWidget(w)) {
        if ((uintptr_t) w->core.display < 0x1000) {
            /* Check if constraint resources caused the corruption */
            if (w->core.constraints != NULL && w->core.parent != NULL) {
                ConstraintWidgetClass cwc2 = (ConstraintWidgetClass) IswClass(w->core.parent);
                fprintf(stderr, "  Parent class='%s', constraint_class.resources=%p, num_resources=%u\n",
                        IswClass(w->core.parent)->core_class.class_name,
                        (void*)cwc2->constraint_class.resources,
                        (unsigned)cwc2->constraint_class.num_resources);
            }
        }
    }
    return cache_refs;
}                               /* _IswGetResources */

void
_IswRefetchResources(Widget w, IswDatabaseHandle db)
{
    XrmName names_s[50], *names;
    XrmClass classes_s[50], *classes;
    Cardinal count;
    WidgetClass wc;
    XrmQuark pixelQ = XrmStringToQuark(IswRPixel);
    XrmQuark fontQ = XrmStringToQuark(IswRFontStruct);
    Arg args[64];
    IswArgVal saved[64];
    Cardinal nargs = 0;

    wc = IswClass(w);
    if (db == NULL)
        return;

    count = CountTreeDepth(w);
    names = (XrmName *) IswStackAlloc(count * sizeof(XrmName), names_s);
    classes = (XrmClass *) IswStackAlloc(count * sizeof(XrmClass), classes_s);
    if (names == NULL || classes == NULL) {
        _IswAllocError(NULL);
        return;
    }
    GetNamesAndClasses(w, names, classes);

    LOCK_PROCESS;
    {
        XrmResourceList *res = (XrmResourceList *) wc->core_class.resources;
        Cardinal i;
        for (i = 0; i < wc->core_class.num_resources && nargs < 64; i++) {
            XrmResource *rx = res[i];

            if (rx->xrm_type != pixelQ && rx->xrm_type != fontQ)
                continue;

            args[nargs].name = (char *) XrmQuarkToString(rx->xrm_name);
            args[nargs].value = (IswArgVal) &saved[nargs];
            saved[nargs] = 0;
            nargs++;
        }
    }
    UNLOCK_PROCESS;

    if (nargs == 0) {
        IswStackFree((IswPointer) names, names_s);
        IswStackFree((IswPointer) classes, classes_s);
        return;
    }

    IswGetValues(w, args, nargs);

    {
        Cardinal nchanged = 0;
        Arg changed[64];

        LOCK_PROCESS;
        {
            XrmResourceList *res = (XrmResourceList *) wc->core_class.resources;
            Cardinal ri, ai = 0;
            for (ri = 0; ri < wc->core_class.num_resources && ai < nargs; ri++) {
                XrmResource *rx = res[ri];
                XrmValue dbval;

                if (rx->xrm_type != pixelQ && rx->xrm_type != fontQ)
                    continue;

                if (_IswDbGetResource(db, names, classes,
                                     (XrmName) rx->xrm_name,
                                     (XrmClass) rx->xrm_class, &dbval)) {
                    XrmValue convResult;
                    IswArgVal converted = 0;

                    convResult.size = rx->xrm_size;
                    convResult.addr = (IswPointer) &converted;
                    if (_IswConvert(w, QString, &dbval,
                                   (XrmRepresentation) rx->xrm_type,
                                   &convResult, NULL)) {
                        if (converted != saved[ai]) {
                            fprintf(stderr, "RefetchResources: %s.%s: 0x%lx -> 0x%lx\n",
                                    IswName(w), args[ai].name,
                                    (unsigned long) saved[ai],
                                    (unsigned long) converted);
                            changed[nchanged].name = args[ai].name;
                            changed[nchanged].value = converted;
                            nchanged++;
                        }
                    }
                    free(dbval.addr);
                }
                ai++;
            }
        }
        UNLOCK_PROCESS;

        if (nchanged > 0)
            IswSetValues(w, changed, nchanged);
    }

    IswStackFree((IswPointer) names, names_s);
    IswStackFree((IswPointer) classes, classes_s);
}

void
_IswGetSubresources(Widget w,                    /* Widget "parent" of subobject */
                   IswPointer base,              /* Base address to write to */
                   const char *name,            /* name of subobject        */
                   const char *class,           /* class of subobject       */
                   IswResourceList resources,    /* resource list for subobject    */
                   Cardinal num_resources,
                   ArgList args,                /* arg list to override resources */
                   Cardinal num_args,
                   IswTypedArgList typed_args,
                   Cardinal num_typed_args)
{
    XrmName *names, names_s[50];
    XrmClass *classes, classes_s[50];
    XrmQuark quark_cache[100];
    XrmQuarkList quark_args;
    Cardinal count, ntyped_args = num_typed_args;
    IswCacheRef *Resrc = NULL;

    WIDGET_TO_APPCON(w);

    if (num_resources == 0)
        return;

    LOCK_APP(app);
    count = CountTreeDepth(w);
    count++;                    /* make sure there's enough room for name and class */
    names = (XrmName *) IswStackAlloc(count * sizeof(XrmName), names_s);
    classes = (XrmClass *) IswStackAlloc(count * sizeof(XrmClass), classes_s);
    if (names == NULL || classes == NULL) {
        _IswAllocError(NULL);
    }
    else {
        XrmResourceList *table;

        /* Get full name, class of subobject */
        GetNamesAndClasses(w, names, classes);
        count -= 2;
        names[count] = StringToName(name);
        classes[count] = StringToClass(class);
        count++;
        names[count] = NULLQUARK;
        classes[count] = NULLQUARK;

        /* Compile arg list into quarks */
        CacheArgs(args, num_args, typed_args, num_typed_args,
                  quark_cache, IswNumber(quark_cache), &quark_args);

        /* Compile resource list if needed */
        if (((int) resources->resource_offset) >= 0) {
            XrmCompileResourceListEphem(resources, num_resources);
        }
        table = _IswCreateIndirectionTable(resources, num_resources);
        Resrc =
            GetResources(w, (char *) base, names, classes, table, num_resources,
                         quark_args, args, num_args, typed_args, &ntyped_args,
                         False);
        FreeCache(quark_cache, quark_args);
        IswFree((char *) table);
        IswFree((char *) Resrc);
        IswStackFree((IswPointer) names, names_s);
        IswStackFree((IswPointer) classes, classes_s);
        UNLOCK_APP(app);
    }
}

void
IswGetSubresources(Widget w,                     /* Widget "parent" of subobject */
                  IswPointer base,               /* Base address to write to */
                  _Xconst char *name,           /* name of subobject        */
                  _Xconst char *class,          /* class of subobject       */
                  IswResourceList resources,     /* resource list for subobject    */
                  Cardinal num_resources,
                  ArgList args,                 /* arg list to override resources */
                  Cardinal num_args)
{
    _IswGetSubresources(w, base, name, class, resources, num_resources, args,
                       num_args, NULL, 0);
}

void
_IswGetApplicationResources(Widget w,            /* Application shell widget */
                           IswPointer base,      /* Base address to write to       */
                           IswResourceList resources,    /* resource list for subobject    */
                           Cardinal num_resources,
                           ArgList args,        /* arg list to override resources */
                           Cardinal num_args,
                           IswTypedArgList typed_args,
                           Cardinal num_typed_args)
{
    XrmName *names, names_s[50];
    XrmClass *classes, classes_s[50];
    XrmQuark quark_cache[100];
    XrmQuarkList quark_args;
    XrmResourceList *table;
    Cardinal ntyped_args = num_typed_args;

#ifdef XTHREADS
    IswAppContext app;
#endif
    IswCacheRef *Resrc = NULL;

    if (num_resources == 0)
        return;

#ifdef XTHREADS
    if (w == NULL)
        app = _IswDefaultAppContext();
    else
        app = IswWidgetToApplicationContext(w);
#endif

    LOCK_APP(app);
    /* Get full name, class of application */
    if (w == NULL) {
        /* hack for R2 compatibility */
        IswPerDisplay pd = _IswGetPerDisplay((IswDisplay) _IswDefaultAppContext()->list[0]);

        names = (XrmName *) IswStackAlloc(2 * sizeof(XrmName), names_s);
        classes = (XrmClass *) IswStackAlloc(2 * sizeof(XrmClass), classes_s);
        if (names == NULL || classes == NULL) {
            _IswAllocError(NULL);
        }
        else {
            names[0] = pd->name ? XrmStringToName(pd->name) : NULLQUARK;
            names[1] = NULLQUARK;
            classes[0] = pd->class ? XrmStringToClass(pd->class) : NULLQUARK;
            classes[1] = NULLQUARK;
        }
    }
    else {
        Cardinal count = CountTreeDepth(w);

        names = (XrmName *) IswStackAlloc(count * sizeof(XrmName), names_s);
        classes =
            (XrmClass *) IswStackAlloc(count * sizeof(XrmClass), classes_s);
        if (names == NULL || classes == NULL) {
            _IswAllocError(NULL);
        }
        else {
            GetNamesAndClasses(w, names, classes);
        }
    }

    /* Compile arg list into quarks */
    CacheArgs(args, num_args, typed_args, num_typed_args, quark_cache,
              IswNumber(quark_cache), &quark_args);
    /* Compile resource list if needed */
    if (((int) resources->resource_offset) >= 0) {
        XrmCompileResourceListEphem(resources, num_resources);
    }
    table = _IswCreateIndirectionTable(resources, num_resources);

    Resrc = GetResources(w, (char *) base, names, classes, table, num_resources,
                         quark_args, args, num_args,
                         typed_args, &ntyped_args, False);
    FreeCache(quark_cache, quark_args);
    IswFree((char *) table);
    IswFree((char *) Resrc);
    if (w != NULL) {
        IswStackFree((IswPointer) names, names_s);
        IswStackFree((IswPointer) classes, classes_s);
    }
    UNLOCK_APP(app);
}

void
IswGetApplicationResources(Widget w,     /* Application shell widget       */
                          IswPointer base,       /* Base address to write to       */
                          IswResourceList resources,     /* resource list for subobject    */
                          Cardinal num_resources,
                          ArgList args, /* arg list to override resources */
                          Cardinal num_args)
{
    _IswGetApplicationResources(w, base, resources, num_resources, args,
                               num_args, NULL, 0);
}

static Boolean initialized = FALSE;

void
_IswResourceListInitialize(void)
{
    LOCK_PROCESS;
    if (initialized) {
        IswWarningMsg("initializationError", "xtInitialize", IswCIswToolkitError,
                     "Initializing Resource Lists twice", NULL, NULL);
        UNLOCK_PROCESS;
        return;
    }
    initialized = TRUE;
    UNLOCK_PROCESS;

    QBoolean = XrmPermStringToQuark(IswCBoolean);
    QString = XrmPermStringToQuark(IswCString);
    QCallProc = XrmPermStringToQuark(IswRCallProc);
    QImmediate = XrmPermStringToQuark(IswRImmediate);
    QinitialResourcesPersistent =
        XrmPermStringToQuark(IswNinitialResourcesPersistent);
    QInitialResourcesPersistent =
        XrmPermStringToQuark(IswCInitialResourcesPersistent);
    Qtranslations = XrmPermStringToQuark(IswNtranslations);
    QbaseTranslations = XrmPermStringToQuark("baseTranslations");
    QTranslations = XrmPermStringToQuark(IswCTranslations);
    QTranslationTable = XrmPermStringToQuark(IswRTranslationTable);
    Qscreen = XrmPermStringToQuark(IswNscreen);
    QScreen = XrmPermStringToQuark(IswCScreen);
}

/*
 * XrmQGetResource - Query the resource database using quark name/class arrays.
 *
 * This is a compatibility wrapper around _IswPlatformResourceGetString().
 * It converts the quark arrays to dot-separated name and class strings,
 * then queries the xcb-util-xrm database.
 *
 * Returns True if the resource was found, False otherwise.
 * On success, sets *type_return to _IswQString and value_return->addr
 * to the resource value string (valid until the next call or database change).
 */
Bool
XrmQGetResource(XrmDatabase db,
                XrmNameList names,
                XrmClassList classes,
                XrmRepresentation *type_return,
                XrmValue *value_return)
{
    char name_buf[512];
    char class_buf[512];
    char *np = name_buf;
    char *cp = class_buf;
    char *value_str = NULL;
    int i;
    const char *s;

    if (db == NULL || names == NULL || classes == NULL)
        return False;

    /* Build dot-separated name string from quark array */
    name_buf[0] = '\0';
    for (i = 0; names[i] != NULLQUARK; i++) {
        s = IswQuarkToString(names[i]);
        if (s == NULL) return False;
        if (i > 0) {
            if (np + 1 >= name_buf + sizeof(name_buf)) return False;
            *np++ = '.';
        }
        size_t len = strlen(s);
        if (np + len >= name_buf + sizeof(name_buf)) return False;
        memcpy(np, s, len);
        np += len;
    }
    *np = '\0';

    /* Build dot-separated class string from quark array */
    class_buf[0] = '\0';
    for (i = 0; classes[i] != NULLQUARK; i++) {
        s = IswQuarkToString(classes[i]);
        if (s == NULL) return False;
        if (i > 0) {
            if (cp + 1 >= class_buf + sizeof(class_buf)) return False;
            *cp++ = '.';
        }
        size_t len = strlen(s);
        if (cp + len >= class_buf + sizeof(class_buf)) return False;
        memcpy(cp, s, len);
        cp += len;
    }
    *cp = '\0';

    /* Query xcb-util-xrm */
    if (_IswPlatformResourceGetString(db, name_buf, class_buf, &value_str) < 0)
        return False;

    /* Return as string type */
    *type_return = _IswQString;
    value_return->addr = (IswPointer) value_str;
    value_return->size = (unsigned int) strlen(value_str) + 1;

    return True;
}
