/*
 * IconViewP.h - Private definitions for IconView widget
 */

#ifndef _ISW_IswIconViewP_h
#define _ISW_IswIconViewP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/IconView.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>
#include <pthread.h>

/* Cached per-item rendering state */
typedef struct {
    ISWImage       *image;
    const unsigned char *raster;
    unsigned int    raster_w, raster_h;
    int             img_handle;       /* retained GPU texture (0 = none) */
    const unsigned char *handle_raster; /* raster ptr the handle was built from */
    int             mask_handle;      /* retained tinted-mask texture (0 = none) */
    const unsigned char *mask_raster; /* raster ptr the mask handle was built from */
    Pixel           mask_fg;          /* foreground the mask handle was tinted with */
    Boolean         load_pending;     /* async decode requested */
} IconViewItemCache;

/* Result from a background decode thread */
typedef struct {
    int           index;
    ISWImage     *image;
    const unsigned char *raster;
    unsigned int  raster_w, raster_h;
} IconViewDecodeResult;

/* In-flight background decode job */
typedef struct {
    Widget        widget;
    int           pipe_fd[2];
    IswInputId    input_id;
    pthread_t     thread;

    /* Requests (written by main, read by worker) */
    int          *indices;
    char        **sources;
    double        dpi;
    char          fg_hex[8];
    unsigned int  phys_sz;
    int           nrequests;

    /* Results (written by worker, read by main) */
    IconViewDecodeResult *results;
    int           nresults;
} IconViewDecodeJob;

typedef struct {
    /* public resources */
    String         *labels;       /* array of label strings */
    String         *icon_data;    /* array of SVG data strings */
    int             nitems;
    Dimension       icon_size;    /* square icon dimension */
    Dimension       item_spacing;
    int             label_lines;  /* max label lines (0 or 1 = single line) */
    Pixel           foreground;
    IswFontStruct    *font;
    IswCallbackList  select_callback;
    Boolean         multi_select;

    /* private state */
    Boolean        *sel_flags;    /* per-item selection flags */
    Boolean        *band_saved;   /* selection state before rubber band started */
    int             anchor;       /* anchor index for shift-click range select */
    int             ncols;        /* computed columns */
    int             nrows;        /* computed rows */
    Dimension       cell_w;       /* computed cell width */
    Dimension       content_h;    /* total content height (preferred height) */
    Dimension      *row_h;        /* per-row cell heights */
    int            *row_y;        /* cumulative Y offset per row */
    IconViewItemCache *cache;     /* per-item raster cache */
    int             cache_size;   /* allocated length of cache[] */
    ISWRenderContext  *render_ctx;

    /* cursor / keyboard focus */
    int             cursor;        /* focused item index, -1 = none */
    Boolean         has_focus;     /* widget has keyboard focus */

    /* deferred deselect: press on selected item defers clear to release */
    Boolean         deselect_pending;
    int             deselect_index;

    /* rubber band state */
    Boolean         band_active;
    Position        band_start_x, band_start_y;
    Position        band_cur_x, band_cur_y;
    Boolean         redraw_pending;    /* coalesce band drag redraws */
    IswWorkProcId    work_proc_id;

    int             drop_highlight;    /* item index to highlight as drop target, -1 = none */

    /* Async icon decode */
    IswIntervalId   decode_timer;     /* debounce timer for deferred loads */
    IconViewDecodeJob *decode_job;    /* in-flight background decode, or NULL */
} IconViewPart;

typedef struct _IconViewRec {
    CorePart       core;
    SimplePart     simple;
    IconViewPart   iconView;
} IconViewRec;

typedef struct {int empty;} IconViewClassPart;

typedef struct _IconViewClassRec {
    CoreClassPart      core_class;
    SimpleClassPart    simple_class;
    IconViewClassPart  iconView_class;
} IconViewClassRec;

extern IconViewClassRec iconViewClassRec;

#endif /* _ISW_IswIconViewP_h */
