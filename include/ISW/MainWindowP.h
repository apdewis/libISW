/*
Copyright (c) 2024  Infi Systems

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
INFI SYSTEMS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _ISW_MainWindowP_h
#define _ISW_MainWindowP_h

#include <ISW/ISWP.h>
#include <ISW/MainWindow.h>

/* Class part */
typedef struct {
    int empty;
} MainWindowClassPart;

/* Full class record */
typedef struct _MainWindowClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    MainWindowClassPart    main_window_class;
} MainWindowClassRec;

extern MainWindowClassRec mainWindowClassRec;

/* Instance part */
typedef struct {
    /* private state */
    Widget     menubar;
} MainWindowPart;

/* Full instance record */
typedef struct _MainWindowRec {
    CorePart          core;
    CompositePart     composite;
    MainWindowPart    main_window;
} MainWindowRec;

#endif /* _ISW_MainWindowP_h */
