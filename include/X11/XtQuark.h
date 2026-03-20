/*
 * XtQuark.h - Standalone quark (string interning) system for libXt
 *
 * This replaces the quark functionality previously provided by
 * <X11/Xresource.h> (Xlib's XRM), allowing libXt to operate
 * without depending on Xlib's resource manager.
 *
 * Copyright (c) 2024 libXt contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _XtQuark_h
#define _XtQuark_h

#include <X11/XtFuncproto.h>

_XFUNCPROTOBEGIN

/*
 * Quark type - an integer handle representing an interned string.
 * Quarks allow fast string comparison via integer equality.
 */
typedef int XtQuark;

/* Null quark value - represents no string / uninitialized */
#define XT_NULLQUARK ((XtQuark)0)

/*
 * Convenience type aliases - these are all quarks, but named
 * to indicate their semantic role in the resource system.
 */
typedef XtQuark XtQuarkName;        /* Resource name quark */
typedef XtQuark XtQuarkClass;       /* Resource class quark */
typedef XtQuark XtRepresentation;   /* Type representation quark */

/* List types */
typedef XtQuark *XtQuarkList;

/*
 * Binding types for resource name/class paths.
 * Used when parsing resource specifications like "app.widget*resource".
 */
typedef enum {
    XtBindTightly,      /* '.' separator - tight binding */
    XtBindLoosely       /* '*' separator - loose binding */
} XtBindingType;

typedef XtBindingType *XtBindingList;

/*
 * Core quark functions
 */

/*
 * XtStringToQuark - Intern a string, making a copy.
 * The string is copied internally; the caller may free the original.
 * Returns the quark for the string.
 */
extern XtQuark XtStringToQuark(
    _Xconst char *  /* string */
);

/*
 * XtPermStringToQuark - Intern a permanent string without copying.
 * The caller guarantees the string will remain valid for the lifetime
 * of the program. This avoids an internal copy for string literals
 * and other permanent strings.
 * Returns the quark for the string.
 */
extern XtQuark XtPermStringToQuark(
    _Xconst char *  /* string */
);

/*
 * XtQuarkToString - Look up the string for a quark.
 * Returns the interned string, or NULL if the quark is invalid.
 * The returned string must not be freed or modified.
 */
extern _Xconst char *XtQuarkToString(
    XtQuark  /* quark */
);

/*
 * Convenience macros mapping semantic names to the core functions.
 */
#define XtStringToName(s)           XtStringToQuark(s)
#define XtStringToClass(s)          XtStringToQuark(s)
#define XtStringToRepresentation(s) XtStringToQuark(s)
#define XtNameToString(q)           XtQuarkToString(q)
#define XtClassToString(q)          XtQuarkToString(q)
#define XtRepresentationToString(q) XtQuarkToString(q)

/*
 * XtStringToBindingQuarkList - Parse a resource path string into
 * a list of bindings and quarks.
 *
 * Input:  "name.name*name"
 * Output: bindings[] = {Tight, Tight, Loose}
 *         quarks[]   = {quark("name"), quark("name"), quark("name"), NULLQUARK}
 *
 * The bindings and quarks arrays must be pre-allocated by the caller
 * with enough space for all components plus a NULLQUARK terminator.
 */
extern void XtStringToBindingQuarkList(
    _Xconst char *  /* name */,
    XtBindingType * /* bindings_return */,
    XtQuark *       /* quarks_return */
);

/*
 * Backward compatibility - map old Xrm names to new Xt names.
 * These allow existing code using XrmQuark, XrmStringToQuark, etc.
 * to compile without modification.
 */
typedef XtQuark         XrmQuark;
typedef XtQuark         XrmName;
typedef XtQuark         XrmClass;
typedef XtQuark         XrmRepresentation;
typedef XtQuarkList     XrmQuarkList;
typedef XtQuarkList     XrmNameList;
typedef XtQuarkList     XrmClassList;
typedef XtBindingType   XrmBinding;
typedef XtBindingList   XrmBindingList;

#define NULLQUARK       XT_NULLQUARK

#define XrmBindTightly  XtBindTightly
#define XrmBindLoosely  XtBindLoosely

/*
 * Backward compatibility function aliases.
 *
 * These are declared as actual extern functions (implemented as thin
 * wrappers in Quark.c) rather than macros, because some code takes
 * the address of XrmStringToQuark / XrmPermStringToQuark to use as
 * function pointers.
 */
extern XtQuark XrmStringToQuark(_Xconst char *);
extern XtQuark XrmPermStringToQuark(_Xconst char *);
extern _Xconst char *XrmQuarkToString(XtQuark);
extern void XrmStringToBindingQuarkList(_Xconst char *, XtBindingType *, XtQuark *);

/* These are safe as macros since they are never used as function pointers */
#define XrmStringToName(s)                      XtStringToQuark(s)
#define XrmStringToClass(s)                     XtStringToQuark(s)
#define XrmNameToString(q)                      XtQuarkToString(q)
#define XrmClassToString(q)                     XtQuarkToString(q)
#define XrmStringToRepresentation(s)            XtStringToQuark(s)
#define XrmRepresentationToString(q)            XtQuarkToString(q)

_XFUNCPROTOEND

#endif /* _XtQuark_h */
