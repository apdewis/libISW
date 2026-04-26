/*
 * ListBoxRowP.h - Private header for ListBoxRow widget
 */

#ifndef _ISW_IswListBoxRowP_h
#define _ISW_IswListBoxRowP_h

#include <ISW/ListBoxRow.h>
#include <ISW/CompositeP.h>
#include <ISW/ISWRender.h>

typedef struct {
    int empty;
} ListBoxRowClassPart;

typedef struct _ListBoxRowClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    ListBoxRowClassPart    listBoxRow_class;
} ListBoxRowClassRec;

extern ListBoxRowClassRec listBoxRowClassRec;

typedef struct {
    Dimension padding;
    ISWRenderContext *render_ctx;
} ListBoxRowPart;

typedef struct _ListBoxRowRec {
    CorePart         core;
    CompositePart    composite;
    ListBoxRowPart   listBoxRow;
} ListBoxRowRec;

#endif /* _ISW_IswListBoxRowP_h */
