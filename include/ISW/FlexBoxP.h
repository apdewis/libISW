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

/* FlexBox widget private definitions */

#ifndef _ISW_FlexBoxP_h
#define _ISW_FlexBoxP_h

#include <ISW/ISWP.h>
#include <ISW/FlexBox.h>

typedef struct {
    XtPointer extension;
} FlexBoxClassPart;

typedef struct _FlexBoxClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    ConstraintClassPart constraint_class;
    FlexBoxClassPart    flexBox_class;
} FlexBoxClassRec;

extern FlexBoxClassRec flexBoxClassRec;

typedef struct _FlexBoxPart {
    /* resources */
    XtOrientation orientation;
    Dimension     spacing;
    /* private */
    Dimension     preferred_width;
    Dimension     preferred_height;
} FlexBoxPart;

typedef struct _FlexBoxRec {
    CorePart       core;
    CompositePart  composite;
    ConstraintPart constraint;
    FlexBoxPart    flexBox;
} FlexBoxRec;

typedef struct _FlexBoxConstraintsPart {
    /* resources */
    int           flex_grow;
    Dimension     flex_basis;
    IswFlexAlign  flex_align;
} FlexBoxConstraintsPart;

typedef struct _FlexBoxConstraintsRec {
    FlexBoxConstraintsPart flexBox;
} FlexBoxConstraintsRec, *FlexBoxConstraints;

#endif /* _ISW_FlexBoxP_h */
