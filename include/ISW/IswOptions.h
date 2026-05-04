/*
 * IswOptions.h - Command-line option parsing types for libXt
 *
 * This replaces XrmOptionDescRec and related types from
 * <X11/Xresource.h>, providing the option table structures
 * used by IswDisplayInitialize and related functions.
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

#ifndef _IswOptions_h
#define _IswOptions_h

/*
 * Option argument kinds - how to interpret the option's value.
 */
typedef enum {
    IswOptionNoArg,      /* Value is specified in OptionDescRec.value */
    IswOptionIsArg,      /* Value is the option string itself */
    IswOptionStickyArg,  /* Value is chars immediately following option */
    IswOptionSepArg,     /* Value is next argument in argv */
    IswOptionResArg,     /* Resource and value in next argument in argv */
    IswOptionSkipArg,    /* Ignore this option and next argument in argv */
    IswOptionSkipLine,   /* Ignore this option and rest of argv */
    IswOptionSkipNArgs   /* Ignore this option and next N arguments in argv */
} IswOptionKind;

/*
 * Option description record - describes how to map a command-line
 * option to a resource specification.
 */
typedef struct {
    const char  *option;    /* Option abbreviation in argv (e.g. "-bg") */
    const char  *specifier; /* Resource specifier (e.g. "*background") */
    IswOptionKind argKind;   /* Which style of option it is */
    IswPointer   value;      /* Value to provide if IswOptionNoArg */
} IswOptionDescRec, *IswOptionDescList;

/*
 * Backward compatibility - map old Xrm names to new Xt names.
 */
typedef IswOptionKind    XrmOptionKind;
typedef IswOptionDescRec XrmOptionDescRec;
typedef IswOptionDescList XrmOptionDescList;

#define XrmoptionNoArg      IswOptionNoArg
#define XrmoptionIsArg      IswOptionIsArg
#define XrmoptionStickyArg  IswOptionStickyArg
#define XrmoptionSepArg     IswOptionSepArg
#define XrmoptionResArg     IswOptionResArg
#define XrmoptionSkipArg    IswOptionSkipArg
#define XrmoptionSkipLine   IswOptionSkipLine
#define XrmoptionSkipNArgs  IswOptionSkipNArgs

#endif /* _IswOptions_h */
