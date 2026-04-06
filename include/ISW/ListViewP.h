/*
 * ListViewP.h - Private definitions for ListView widget
 */

#ifndef _ISW_IswListViewP_h
#define _ISW_IswListViewP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/ListView.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWXftCompat.h>

/* Internal column state */
typedef struct {
    String      title;
    Dimension   width;       /* current width */
    Dimension   min_width;   /* minimum resize width */
} ListViewColumnInfo;

typedef struct {
    /* public resources */
    Pixel           foreground;
    XFontStruct    *font;
#ifdef ISW_INTERNATIONALIZATION
    ISWFontSet     *fontset;
#endif
    XtCallbackList  select_callback;
    XtCallbackList  reorder_callback;
    Boolean         multi_select;
    Boolean         show_header;
    Dimension       row_height;     /* row height (0 = auto from font) */
    Dimension       header_height;  /* header height (0 = auto) */

    /* column definitions (set via API, not XtResource directly) */
    IswListViewColumn *columns_res; /* resource pointer (not owned) */
    int             ncols_res;      /* resource column count */

    /* cell data: flat row-major array, data[row * ncols + col] */
    String         *data;           /* not owned */
    int             nrows;
    int             ncols;          /* actual column count (from columns) */

    /* private state */
    ListViewColumnInfo *col_info;   /* owned: internal column array */
    int             col_count;      /* length of col_info */
    Boolean        *sel_flags;      /* per-row selection flags */
    Boolean        *band_saved;     /* selection state before rubber band */
    int             anchor;         /* anchor row for shift-click range */
    int             cursor;         /* focused row, -1 = none */
    Boolean         has_focus;

    Dimension       computed_row_h; /* scaled row height */
    Dimension       computed_hdr_h; /* scaled header height */
    Dimension       total_col_w;    /* sum of all column widths */

    ISWRenderContext *render_ctx;

    /* deferred deselect */
    Boolean         deselect_pending;
    int             deselect_index;

    /* rubber band state */
    Boolean         band_active;
    Position        band_start_x, band_start_y;
    Position        band_cur_x, band_cur_y;
    double          fg_r, fg_g, fg_b;
    Boolean         redraw_pending;
    XtWorkProcId    work_proc_id;

    /* sort state */
    int             sort_column;     /* -1 = no sort */
    IswListViewSortDirection sort_direction;

    /* column resize drag */
    Boolean         col_resize_active;
    int             col_resize_index;  /* which column separator */
    Position        col_resize_start_x;
    Dimension       col_resize_start_w;
} ListViewPart;

typedef struct _ListViewRec {
    CorePart       core;
    SimplePart     simple;
    ListViewPart   listView;
} ListViewRec;

typedef struct {int empty;} ListViewClassPart;

typedef struct _ListViewClassRec {
    CoreClassPart      core_class;
    SimpleClassPart    simple_class;
    ListViewClassPart  listView_class;
} ListViewClassRec;

extern ListViewClassRec listViewClassRec;

#endif /* _ISW_IswListViewP_h */
