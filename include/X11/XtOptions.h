/*
 * XtOptions.h - Command-line option parsing types for libXt
 *
 * This replaces XrmOptionDescRec and related types from
 * <X11/Xresource.h>, providing the option table structures
 * used by XtDisplayInitialize and related functions.
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

#ifndef _XtOptions_h
#define _XtOptions_h

/*
 * Option argument kinds - how to interpret the option's value.
 */
typedef enum {
    XtOptionNoArg,      /* Value is specified in OptionDescRec.value */
    XtOptionIsArg,      /* Value is the option string itself */
    XtOptionStickyArg,  /* Value is chars immediately following option */
    XtOptionSepArg,     /* Value is next argument in argv */
    XtOptionResArg,     /* Resource and value in next argument in argv */
    XtOptionSkipArg,    /* Ignore this option and next argument in argv */
    XtOptionSkipLine,   /* Ignore this option and rest of argv */
    XtOptionSkipNArgs   /* Ignore this option and next N arguments in argv */
} XtOptionKind;

/*
 * Option description record - describes how to map a command-line
 * option to a resource specification.
 */
typedef struct {
    char        *option;    /* Option abbreviation in argv (e.g. "-bg") */
    char        *specifier; /* Resource specifier (e.g. "*background") */
    XtOptionKind argKind;   /* Which style of option it is */
    XtPointer   value;      /* Value to provide if XtOptionNoArg */
} XtOptionDescRec, *XtOptionDescList;

/*
 * Backward compatibility - map old Xrm names to new Xt names.
 */
typedef XtOptionKind    XrmOptionKind;
typedef XtOptionDescRec XrmOptionDescRec;
typedef XtOptionDescList XrmOptionDescList;

#define XrmoptionNoArg      XtOptionNoArg
#define XrmoptionIsArg      XtOptionIsArg
#define XrmoptionStickyArg  XtOptionStickyArg
#define XrmoptionSepArg     XtOptionSepArg
#define XrmoptionResArg     XtOptionResArg
#define XrmoptionSkipArg    XtOptionSkipArg
#define XrmoptionSkipLine   XtOptionSkipLine
#define XrmoptionSkipNArgs  XtOptionSkipNArgs

#endif /* _XtOptions_h */
