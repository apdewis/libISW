/*
 * ListViewHeader.h - Public header for ListViewHeader widget
 *
 * The column header strip of a ListView: titles, sort arrows, and
 * drag-to-resize column separators. Created internally by ListView
 * when its showHeader resource is True — applications do not
 * instantiate this widget directly.
 */

#ifndef _ISW_IswListViewHeader_h
#define _ISW_IswListViewHeader_h

#include <ISW/Simple.h>
#include <ISW/ListView.h>

/* Resource names */
#define IswNsortCallback         "sortCallback"
#define IswNcolumnResizeCallback "columnResizeCallback"

extern WidgetClass listViewHeaderWidgetClass;

typedef struct _ListViewHeaderClassRec *ListViewHeaderWidgetClass;
typedef struct _ListViewHeaderRec      *ListViewHeaderWidget;

/* Callback data for columnResizeCallback (separator drag).
   sortCallback uses IswListViewReorderCallbackData from ListView.h. */
typedef struct {
    int       column;       /* column being resized */
    Dimension width;        /* new width, clamped to the column minimum */
} IswListViewHeaderResizeCallbackData;

#endif /* _ISW_IswListViewHeader_h */
