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
#define XtNlistViewColumns    "listViewColumns"
#define XtCListViewColumns    "ListViewColumns"
#define XtNnumColumns         "numColumns"
#define XtCNumColumns         "NumColumns"
#define XtNnumRows            "numRows"
#define XtCNumRows            "NumRows"
#define XtNlistViewData       "listViewData"
#define XtCListViewData       "ListViewData"
#define XtNrowHeight          "rowHeight"
#define XtCRowHeight          "RowHeight"
#define XtNheaderHeight       "headerHeight"
#define XtCHeaderHeight       "HeaderHeight"
#define XtNselectCallback     "selectCallback"
#define XtNmultiSelect        "multiSelect"
#define XtCMultiSelect        "MultiSelect"
#define XtNcursorRow          "cursorRow"
#define XtCCursorRow          "CursorRow"
#define XtNshowHeader         "showHeader"
#define XtCShowHeader         "ShowHeader"

extern WidgetClass listViewWidgetClass;

typedef struct _ListViewClassRec *ListViewWidgetClass;
typedef struct _ListViewRec      *ListViewWidget;

/* Column definition */
typedef struct {
    String title;           /* column header text */
    Dimension width;        /* column width in pixels (0 = auto) */
    Dimension min_width;    /* minimum width for resize (0 = 30) */
} IswListViewColumn;

/* Callback data */
typedef struct {
    int    row;             /* last clicked row index */
    int    column;          /* column of the click */
    int   *selected;        /* array of selected row indices */
    int    num_selected;    /* count of selected rows */
} IswListViewCallbackData;

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

_XFUNCPROTOEND

#endif /* _ISW_IswListView_h */
