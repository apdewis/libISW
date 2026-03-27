/*
 * Copyright (c) 2024-2026 ISW Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _ISW_FlexBox_h
#define _ISW_FlexBox_h

#include <X11/Constraint.h>

/*
 * FlexBox Widget
 *
 * A layout container that distributes space along a primary axis.
 * Children declare how much extra space they want via constraint resources.
 *
 * Widget resources:
 *
 * Name              Class           RepType         Default Value
 * ----              -----           -------         -------------
 * orientation       Orientation     XtOrientation   XtorientVertical
 * spacing           Spacing         Dimension       0
 *
 * Constraint resources (per-child):
 *
 * Name              Class           RepType         Default Value
 * ----              -----           -------         -------------
 * flexGrow          FlexGrow        Int             0
 * flexBasis         FlexBasis       Dimension       0
 * flexAlign         FlexAlign       FlexAlign       XtflexAlignStretch
 */

/* Widget-level resource names/classes */
#ifndef XtNspacing
#define XtNspacing "spacing"
#endif
#ifndef XtCSpacing
#define XtCSpacing "Spacing"
#endif

/* Constraint resource names */
#define XtNflexGrow  "flexGrow"
#define XtNflexBasis "flexBasis"
#define XtNflexAlign "flexAlign"

/* Constraint resource classes */
#define XtCFlexGrow  "FlexGrow"
#define XtCFlexBasis "FlexBasis"
#define XtCFlexAlign "FlexAlign"

/* Representation type */
#define XtRFlexAlign "FlexAlign"

/* FlexAlign enum */
typedef enum {
    XtflexAlignStart,
    XtflexAlignEnd,
    XtflexAlignCenter,
    XtflexAlignStretch
} IswFlexAlign;

typedef struct _FlexBoxClassRec *FlexBoxWidgetClass;
typedef struct _FlexBoxRec      *FlexBoxWidget;

extern WidgetClass flexBoxWidgetClass;

#endif /* _ISW_FlexBox_h */
