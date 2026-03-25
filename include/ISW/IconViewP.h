/*
 * IconViewP.h - Private definitions for IconView widget
 */

#ifndef _ISW_IswIconViewP_h
#define _ISW_IswIconViewP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/IconView.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWSVG.h>
#include <ISW/ISWXftCompat.h>

/* Cached per-item rendering state */
typedef struct {
    ISWSVGImage    *svg_image;
    unsigned char  *raster;
    unsigned int    raster_w, raster_h;
} IconViewItemCache;

typedef struct {
    /* public resources */
    String         *labels;       /* array of label strings */
    String         *icon_data;    /* array of SVG data strings */
    int             nitems;
    Dimension       icon_size;    /* square icon dimension */
    Dimension       item_spacing;
    Pixel           foreground;
    XFontStruct    *font;
#ifdef ISW_INTERNATIONALIZATION
    ISWFontSet     *fontset;
#endif
    XtCallbackList  select_callback;
    Boolean         multi_select;

    /* private state */
    Boolean        *sel_flags;    /* per-item selection flags */
    int             anchor;       /* anchor index for shift-click range select */
    int             ncols;        /* computed columns */
    int             nrows;        /* computed rows */
    Dimension       cell_w;       /* computed cell width */
    Dimension       cell_h;       /* computed cell height */
    IconViewItemCache *cache;     /* per-item raster cache */
    ISWRenderContext  *render_ctx;
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
