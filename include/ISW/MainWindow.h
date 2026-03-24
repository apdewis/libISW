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

#ifndef _ISW_MainWindow_h
#define _ISW_MainWindow_h

/*
 * MainWindow Widget (subclass of CompositeClass)
 *
 * Manages two zones: a MenuBar fixed at the top (always full width,
 * never scrolls) and a single content child below filling the rest.
 */

#include <X11/Intrinsic.h>

/* Class record constants */
extern WidgetClass mainWindowWidgetClass;

typedef struct _MainWindowClassRec *MainWindowWidgetClass;
typedef struct _MainWindowRec      *MainWindowWidget;

_XFUNCPROTOBEGIN

/* Return the built-in MenuBar child */
extern Widget IswMainWindowGetMenuBar(Widget /* mainWindow */);

_XFUNCPROTOEND

#endif /* _ISW_MainWindow_h */
