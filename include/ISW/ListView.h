/*
 * ListView.h - Public header for ListView widget
 *
 * A multi-column list view with resizable columns, multiselect,
 * and rubber band selection. Designed to be used inside a Viewport
 * for scrolling.
 */

#ifndef _ISW_IswListView_h
#define _ISW_IswListView_h

#include <ISW/Simple.h>

/* Resource names */
#define IswNlistViewColumns    "listViewColumns"
#define IswCListViewColumns    "ListViewColumns"
#define IswNnumColumns         "numColumns"
#define IswCNumColumns         "NumColumns"
#define IswNnumRows            "numRows"
#define IswCNumRows            "NumRows"
#define IswNlistViewData       "listViewData"
#define IswCListViewData       "ListViewData"
#define IswNrowHeight          "rowHeight"
#define IswCRowHeight          "RowHeight"
#define IswNheaderHeight       "headerHeight"
#define IswCHeaderHeight       "HeaderHeight"
#define IswNselectCallback     "selectCallback"
#define IswNmultiSelect        "multiSelect"
#define IswCMultiSelect        "MultiSelect"
#define IswNcursorRow          "cursorRow"
#define IswCCursorRow          "CursorRow"
#define IswNshowHeader         "showHeader"
#define IswCShowHeader         "ShowHeader"
#define IswNreorderCallback    "reorderCallback"

/* Sort direction */
typedef enum {
    IswListViewSortNone = 0,
    IswListViewSortAscending,
    IswListViewSortDescending
} IswListViewSortDirection;

extern WidgetClass listViewWidgetClass;

typedef struct _ListViewClassRec *ListViewWidgetClass;
typedef struct _ListViewRec      *ListViewWidget;

/* Column definition */
typedef struct {
    String title;           /* column header text */
    Dimension width;        /* column width in pixels (0 = auto) */
    Dimension min_width;    /* minimum width for resize (0 = 30) */
} IswListViewColumn;

/* Callback data for selectCallback */
typedef struct {
    int    row;             /* last clicked row index */
    int    column;          /* column of the click */
    int   *selected;        /* array of selected row indices */
    int    num_selected;    /* count of selected rows */
} IswListViewCallbackData;

/* Callback data for reorderCallback (header click) */
typedef struct {
    int                      column;     /* column index clicked */
    IswListViewSortDirection direction;  /* new sort direction */
} IswListViewReorderCallbackData;

_XFUNCPROTOBEGIN

extern void IswListViewSetData(Widget w, String *data,
                               int nrows, int ncols);
extern void IswListViewSetColumns(Widget w, IswListViewColumn *cols,
                                  int ncols);
extern int  IswListViewAddColumn(Widget w, const char *title,
                                 Dimension width, Dimension min_width);
extern int  IswListViewGetSelected(Widget w);
extern int  IswListViewGetSelectedRows(Widget w, int **indices_out);
extern Boolean IswListViewBandActive(Widget w);
extern void IswListViewSetSort(Widget w, int column,
                               IswListViewSortDirection direction);
extern void IswListViewSetDropHighlight(Widget w, int row_index);
extern int  IswListViewHitTest(Widget w, int x, int y);

_XFUNCPROTOEND

#endif /* _ISW_IswListView_h */
