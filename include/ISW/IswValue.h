/*
 * XtValue.h - Generic value container for libXt
 *
 * This replaces XrmValue from <X11/Xresource.h>, providing a simple
 * {size, address} pair used throughout the type converter system.
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

#ifndef _XtValue_h
#define _XtValue_h

/*
 * XtValueRec - A generic value container holding a size and a pointer
 * to the value data. Used extensively in the type converter system.
 */
typedef struct {
    unsigned int    size;   /* Size of the value data in bytes */
    XtPointer       addr;   /* Pointer to the value data */
} XtValueRec, *XtValuePtr;

/*
 * Backward compatibility - map old Xrm names to new Xt names.
 * These allow existing code using XrmValue, XrmValuePtr, etc.
 * to compile without modification.
 */
typedef XtValueRec  XrmValue;
typedef XtValuePtr  XrmValuePtr;

#endif /* _XtValue_h */
