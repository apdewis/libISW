/*
 * ProgressBarP.h - Private definitions for ProgressBar widget
 */

#ifndef _ISW_ProgressBarP_h
#define _ISW_ProgressBarP_h

#include <ISW/ProgressBar.h>
#include <ISW/SimpleP.h>
#include <ISW/ISWRender.h>

typedef struct {
    int makes_compiler_happy;
} ProgressBarClassPart;

typedef struct _ProgressBarClassRec {
    CoreClassPart          core_class;
    SimpleClassPart        simple_class;
    ProgressBarClassPart   progress_bar_class;
} ProgressBarClassRec;

extern ProgressBarClassRec progressBarClassRec;

typedef struct {
    /* resources */
    int             value;          /* 0 to 100 */
    Pixel           foreground;
    XtOrientation   orientation;
    Boolean         show_value;
    XFontStruct    *font;

    /* private */
    ISWRenderContext *render_ctx;
} ProgressBarPart;

typedef struct _ProgressBarRec {
    CorePart          core;
    SimplePart        simple;
    ProgressBarPart   progress_bar;
} ProgressBarRec;

#endif /* _ISW_ProgressBarP_h */
