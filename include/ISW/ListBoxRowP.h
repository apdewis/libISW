/*
 * ListBoxRowP.h - Private header for ListBoxRow widget
 */

#ifndef _ISW_IswListBoxRowP_h
#define _ISW_IswListBoxRowP_h

#include <ISW/ListBoxRow.h>
#include <ISW/ISWP.h>
#include <ISW/ConstrainP.h>
#include <ISW/ISWRender.h>

typedef struct {
    int empty;
} ListBoxRowClassPart;

typedef struct _ListBoxRowClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    ConstraintClassPart    constraint_class;
    ListBoxRowClassPart    listBoxRow_class;
} ListBoxRowClassRec;

extern ListBoxRowClassRec listBoxRowClassRec;

typedef struct {
    Dimension  padding;
    ISWRenderContext *render_ctx;
} ListBoxRowPart;

typedef struct _ListBoxRowRec {
    CorePart         core;
    CompositePart    composite;
    ConstraintPart   constraint;
    ListBoxRowPart   listBoxRow;
} ListBoxRowRec;

typedef struct {
    IswJustify justify;
} ListBoxRowConstraintsPart;

typedef struct _ListBoxRowConstraintsRec {
    ListBoxRowConstraintsPart row;
} ListBoxRowConstraintsRec, *ListBoxRowConstraints;

#endif /* _ISW_IswListBoxRowP_h */
