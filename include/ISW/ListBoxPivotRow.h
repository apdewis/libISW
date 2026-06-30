/*
 * ListBoxPivotRow.h - Expandable row for use inside a ListBox
 *
 * Displays a pivot icon and label.  Clicking toggles the pivot state,
 * which manages/unmanages a single child ListBox to show or hide its
 * contents.
 */

#ifndef _ISW_IswListBoxPivotRow_h
#define _ISW_IswListBoxPivotRow_h

#include <ISW/ListBoxRow.h>

#define IswNpivotOpen       "pivotOpen"
#define IswCPivotOpen       "PivotOpen"
#define IswNpivotImage      "pivotImage"
#define IswCPivotImage      "PivotImage"
#define IswNpivotImageOpen  "pivotImageOpen"
#define IswCPivotImageOpen  "PivotImageOpen"
#define IswNpivotCallback   "pivotCallback"

extern WidgetClass listBoxPivotRowWidgetClass;

typedef struct _ListBoxPivotRowClassRec *ListBoxPivotRowWidgetClass;
typedef struct _ListBoxPivotRowRec      *ListBoxPivotRowWidget;

#endif /* _ISW_IswListBoxPivotRow_h */
