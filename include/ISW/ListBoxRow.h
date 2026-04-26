/*
 * ListBoxRow.h - Row widget for use inside a ListBox
 *
 * A simple Composite that lays children out left-to-right, accepts
 * whatever size its parent gives it, and fills its entire background.
 */

#ifndef _ISW_IswListBoxRow_h
#define _ISW_IswListBoxRow_h

#include <ISW/Constraint.h>

#define IswNrowPadding  "rowPadding"
#define IswCRowPadding  "RowPadding"

extern WidgetClass listBoxRowWidgetClass;

typedef struct _ListBoxRowClassRec *ListBoxRowWidgetClass;
typedef struct _ListBoxRowRec      *ListBoxRowWidget;

#endif /* _ISW_IswListBoxRow_h */
