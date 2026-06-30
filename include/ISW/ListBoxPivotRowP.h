/*
 * ListBoxPivotRowP.h - Private header for ListBoxPivotRow widget
 */

#ifndef _ISW_IswListBoxPivotRowP_h
#define _ISW_IswListBoxPivotRowP_h

#include <ISW/ListBoxPivotRow.h>
#include <ISW/ListBoxRowP.h>

typedef struct {
    int empty;
} ListBoxPivotRowClassPart;

typedef struct _ListBoxPivotRowClassRec {
    CoreClassPart              core_class;
    CompositeClassPart         composite_class;
    ConstraintClassPart        constraint_class;
    ListBoxRowClassPart        listBoxRow_class;
    ListBoxPivotRowClassPart   listBoxPivotRow_class;
} ListBoxPivotRowClassRec;

extern ListBoxPivotRowClassRec listBoxPivotRowClassRec;

typedef struct {
    /* resources */
    Boolean         pivot_open;
    String          pivot_image;
    String          pivot_image_open;
    String          label;
    IswCallbackList pivot_callback;

    /* private */
    Widget          icon_w;
    Widget          label_w;
    Widget          child_listbox;
} ListBoxPivotRowPart;

typedef struct _ListBoxPivotRowRec {
    CorePart               core;
    CompositePart          composite;
    ConstraintPart         constraint;
    ListBoxRowPart         listBoxRow;
    ListBoxPivotRowPart    listBoxPivotRow;
} ListBoxPivotRowRec;

#endif /* _ISW_IswListBoxPivotRowP_h */
