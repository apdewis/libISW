/*
 * IswQuark.h - Standalone quark (string interning) system for libXt
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

#ifndef _IswQuark_h
#define _IswQuark_h

#include <ISW/IswFuncproto.h>

_XFUNCPROTOBEGIN

/*
 * Quark type - an integer handle representing an interned string.
 * Quarks allow fast string comparison via integer equality.
 */
typedef int IswQuark;

/* Null quark value - represents no string / uninitialized */
#define ISW_NULLQUARK ((IswQuark)0)

/*
 * Convenience type aliases - these are all quarks, but named
 * to indicate their semantic role in the resource system.
 */
typedef IswQuark IswQuarkName;        /* Resource name quark */
typedef IswQuark IswQuarkClass;       /* Resource class quark */
typedef IswQuark IswRepresentation;   /* Type representation quark */

/* List types */
typedef IswQuark *IswQuarkList;

/*
 * Binding types for resource name/class paths.
 * Used when parsing resource specifications like "app.widget*resource".
 */
typedef enum {
    IswBindTightly,      /* '.' separator - tight binding */
    IswBindLoosely       /* '*' separator - loose binding */
} IswBindingType;

typedef IswBindingType *IswBindingList;

/*
 * Core quark functions
 */

/*
 * IswStringToQuark - Intern a string, making a copy.
 * The string is copied internally; the caller may free the original.
 * Returns the quark for the string.
 */
extern IswQuark IswStringToQuark(
    _Xconst char *  /* string */
);

/*
 * IswPermStringToQuark - Intern a permanent string without copying.
 * The caller guarantees the string will remain valid for the lifetime
 * of the program. This avoids an internal copy for string literals
 * and other permanent strings.
 * Returns the quark for the string.
 */
extern IswQuark IswPermStringToQuark(
    _Xconst char *  /* string */
);

/*
 * IswQuarkToString - Look up the string for a quark.
 * Returns the interned string, or NULL if the quark is invalid.
 * The returned string must not be freed or modified.
 */
extern _Xconst char *IswQuarkToString(
    IswQuark  /* quark */
);

/*
 * Convenience macros mapping semantic names to the core functions.
 */
#define IswStringToName(s)           IswStringToQuark(s)
#define IswStringToClass(s)          IswStringToQuark(s)
#define IswStringToRepresentation(s) IswStringToQuark(s)
#define IswNameToString(q)           IswQuarkToString(q)
#define IswClassToString(q)          IswQuarkToString(q)
#define IswRepresentationToString(q) IswQuarkToString(q)

/*
 * IswStringToBindingQuarkList - Parse a resource path string into
 * a list of bindings and quarks.
 *
 * Input:  "name.name*name"
 * Output: bindings[] = {Tight, Tight, Loose}
 *         quarks[]   = {quark("name"), quark("name"), quark("name"), NULLQUARK}
 *
 * The bindings and quarks arrays must be pre-allocated by the caller
 * with enough space for all components plus a NULLQUARK terminator.
 */
extern void IswStringToBindingQuarkList(
    _Xconst char *  /* name */,
    IswBindingType * /* bindings_return */,
    IswQuark *       /* quarks_return */
);

/*
 * Backward compatibility - map old Xrm names to new Xt names.
 * These allow existing code using XrmQuark, XrmStringToQuark, etc.
 * to compile without modification.
 */
typedef IswQuark         XrmQuark;
typedef IswQuark         XrmName;
typedef IswQuark         XrmClass;
typedef IswQuark         XrmRepresentation;
typedef IswQuarkList     XrmQuarkList;
typedef IswQuarkList     XrmNameList;
typedef IswQuarkList     XrmClassList;
typedef IswBindingType   XrmBinding;
typedef IswBindingList   XrmBindingList;

#define NULLQUARK       ISW_NULLQUARK

#define XrmBindTightly  IswBindTightly
#define XrmBindLoosely  IswBindLoosely

/*
 * Backward compatibility function aliases.
 *
 * These are declared as actual extern functions (implemented as thin
 * wrappers in Quark.c) rather than macros, because some code takes
 * the address of XrmStringToQuark / XrmPermStringToQuark to use as
 * function pointers.
 */
extern IswQuark XrmStringToQuark(_Xconst char *);
extern IswQuark XrmPermStringToQuark(_Xconst char *);
extern _Xconst char *XrmQuarkToString(IswQuark);
extern void XrmStringToBindingQuarkList(_Xconst char *, IswBindingType *, IswQuark *);

/* These are safe as macros since they are never used as function pointers */
#define XrmStringToName(s)                      IswStringToQuark(s)
#define XrmStringToClass(s)                     IswStringToQuark(s)
#define XrmNameToString(q)                      IswQuarkToString(q)
#define XrmClassToString(q)                     IswQuarkToString(q)
#define XrmStringToRepresentation(s)            IswStringToQuark(s)
#define XrmRepresentationToString(q)            IswQuarkToString(q)

_XFUNCPROTOEND

#endif /* _IswQuark_h */
