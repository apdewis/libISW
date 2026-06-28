/***********************************************************

Copyright (c) 1987, 1988, 1994  X Consortium

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


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

#ifndef _ISW_SimpleP_h
#define _ISW_SimpleP_h

#include "ISWP.h"
#include <ISW/Simple.h>

typedef struct {
    Boolean	(*change_sensitive)(Widget);
    /* Optional: report a windowless child of this widget at content point
       (x,y) for event hit-testing.  Used by non-composite widgets (e.g.
       Text) that own windowless sub-widgets not held in composite.children.
       Returns the child and sets *dx,*dy to its content origin offset, or
       NULL if the point is not over a hit-child.  NULL hook = no children. */
    Widget	(*hit_child)(Widget, int x, int y, int *dx, int *dy);
    /* Optional: enumerate this widget's windowless sub-widgets that are not in
       composite.children (e.g. Text's scrollbars), for the paint/composite
       passes.  Return the i-th such child (in stacking order) or NULL once past
       the end.  NULL hook = no such children. */
    Widget	(*nth_windowless_child)(Widget, int i);
} SimpleClassPart;

#define IswInheritChangeSensitive ((Boolean (*)(Widget))_IswInherit)
#define IswInheritHitChild ((Widget (*)(Widget,int,int,int*,int*))_IswInherit)
#define IswInheritNthWindowlessChild ((Widget (*)(Widget,int))_IswInherit)

typedef struct _SimpleClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
} SimpleClassRec;

extern SimpleClassRec simpleClassRec;

typedef struct {
    /* resources */
    IswCursor	cursor;
    String      cursor_name;	/* cursor specified by name. */

    Pixel       pointer_fg, pointer_bg;	/* Pointer colors. */
    Boolean     international;

    /* keyboard focus traversal */
    Boolean     traversal_on;   /* widget participates in Tab cycle */
    int         tab_index;      /* explicit ordering; 0 = follow tree order */

    /* private state */
    Boolean     has_focus;      /* runtime: drawn-focus state */

    /* Widget renders its own border (e.g. Command's rounded Cairo stroke
       driven by core.border_width).  Suppresses the windowless backend's
       generic border ring so the border is not drawn twice. */
    Boolean     self_border;

    /* Corner radius (logical px) for the backend's border ring and the
       content background.  0 = square.  Set by widgets that want rounded
       corners (Label, Command); read by the windowless backend so the ring
       is stroked as a rounded rectangle instead of a square frame. */
    Dimension   corner_radius;

    /* Border color override for the backend's ring.  Widgets whose border
       tracks a non-Core color (Label: its foreground) keep this in sync;
       use_border_color selects it over core.border_pixel.  Read by the
       windowless backend. */
    Pixel       border_color;
    Boolean     use_border_color;

    Pixel       active_color;

    /* Drag-and-drop callbacks (IswDragDrop service).  Declared here, on the
       common DnD-aware base, so any Simple-derived widget can register them
       via IswAddCallback(w, IswNdropCallback, ...).  Non-Simple widgets use
       IswDndSetDropCallback() etc. instead (the DropConfig path). */
    IswCallbackList drop_callbacks;
    IswCallbackList drag_enter_callbacks;
    IswCallbackList drag_motion_callbacks;
    IswCallbackList drag_leave_callbacks;
} SimplePart;

typedef struct _SimpleRec {
    CorePart	core;
    SimplePart	simple;
} SimpleRec;

/* Apply a windowless widget's cursor to its windowed ancestor's window;
   called from the event dispatcher on pointer-widget change. */
extern void _IswSimpleApplyCursor(Widget /* pointer widget */);

/* Top-level cursor primitives.  Widgets must not issue XCB cursor calls
   directly; these own the underlying XCB operations.

   _IswSetWindowCursor  - set the pointer cursor on a windowed target's
                          window (owns xcb_change_window_attributes/CURSOR).
   _IswFreeCursor       - release a server cursor (owns xcb_free_cursor).
   _IswUpdatePointerCaptureCursor
                        - update the cursor of the active pointer capture. */
extern void _IswSetWindowCursor(Widget /* windowed target */,
                                IswCursor /* cursor */);
extern void _IswFreeCursor(Widget /* widget */, IswCursor /* cursor */);
extern void _IswUpdatePointerCaptureCursor(Widget /* widget */,
                                           IswCursor /* cursor */,
                                           IswTime /* time */,
                                           uint16_t /* event_mask */);

#endif /* _ISW_SimpleP_h */
