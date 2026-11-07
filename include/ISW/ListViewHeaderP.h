/*
 * ListViewHeaderP.h - Private definitions for ListViewHeader widget
 */

#ifndef _ISW_IswListViewHeaderP_h
#define _ISW_IswListViewHeaderP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/ListViewHeader.h>
#include <ISW/ListViewP.h>      /* ListViewColumnInfo */
#include <ISW/ISWRender.h>

typedef struct {
    /* public resources */
    Pixel           foreground;
    IswFontStruct    *font;
    Dimension       header_height;  /* explicit height (0 = auto from font) */
    IswCallbackList  sort_callback;
    IswCallbackList  resize_callback;

    /* shared column model (owned by the parent ListView, not by us) */
    ListViewColumnInfo *col_info;
    int             col_count;

    /* sort display state (the model lives in the parent ListView) */
    int             sort_column;     /* -1 = no sort */
    IswListViewSortDirection sort_direction;

    ISWRenderContext *render_ctx;

    /* column resize drag */
    Boolean         col_resize_active;
    int             col_resize_index;  /* which column separator */
    Position        col_resize_start_x;
    Dimension       col_resize_start_w;

    /* column resize cursors */
    IswCursor    resize_cursor;     /* sb_h_double_arrow, loaded on use */
    unsigned long resize_cursor_gen; /* display cursor generation it was loaded in */
    IswCursor    default_cursor;    /* widget's normal cursor */
    Boolean         resize_cursor_set; /* True when resize cursor is active */
} ListViewHeaderPart;

typedef struct _ListViewHeaderRec {
    CorePart            core;
    SimplePart          simple;
    ListViewHeaderPart  listViewHeader;
} ListViewHeaderRec;

typedef struct {int empty;} ListViewHeaderClassPart;

typedef struct _ListViewHeaderClassRec {
    CoreClassPart            core_class;
    SimpleClassPart          simple_class;
    ListViewHeaderClassPart  listViewHeader_class;
} ListViewHeaderClassRec;

extern ListViewHeaderClassRec listViewHeaderClassRec;

/* Parent-side interface (called by ListView.c).  The column array is shared
   by pointer — the header reads it live and never writes or frees it. */
extern void _IswListViewHeaderSetColumns(Widget w, ListViewColumnInfo *cols,
                                         int ncols);
extern void _IswListViewHeaderSetSort(Widget w, int column,
                                      IswListViewSortDirection direction);
extern Dimension _IswListViewHeaderPreferredHeight(Widget w);

#endif /* _ISW_IswListViewHeaderP_h */
