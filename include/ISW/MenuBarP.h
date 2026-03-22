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

#ifndef _ISW_IswMenuBarP_h
#define _ISW_IswMenuBarP_h

#include <ISW/MenuBar.h>
#include <ISW/BoxP.h>

/* New fields for the MenuBar widget class record */
typedef struct {
    int makes_compiler_happy;  /* not used */
} MenuBarClassPart;

/* Full class record declaration */
typedef struct _MenuBarClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    BoxClassPart        box_class;
    MenuBarClassPart    menu_bar_class;
} MenuBarClassRec;

extern MenuBarClassRec menuBarClassRec;

/* New fields for the MenuBar widget record */
typedef struct {
    /* private state */
    Widget      active_button;    /* MenuButton whose menu is currently open */
    Widget      active_menu;      /* The SimpleMenu widget currently popped up */
    Boolean     menu_is_open;     /* TRUE when a dropdown is visible */
} MenuBarPart;

/* Full instance record declaration */
typedef struct _MenuBarRec {
    CorePart        core;
    CompositePart   composite;
    BoxPart         box;
    MenuBarPart     menu_bar;
} MenuBarRec;

#endif /* _ISW_IswMenuBarP_h */
