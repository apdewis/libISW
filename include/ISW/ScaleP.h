/*
 * ScaleP.h - Private definitions for Scale widget
 */

#ifndef _ISW_IswScaleP_h
#define _ISW_IswScaleP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/Scale.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWXftCompat.h>

/* Reuse XtOrientation from ScrollbarP.h */
#ifndef _IswXtOrientation_defined
#define _IswXtOrientation_defined
typedef enum {
    XtorientHorizontal,
    XtorientVertical
} XtOrientation;
#endif

typedef struct {
    /* public resources */
    Pixel         foreground;
    XtOrientation orientation;
    int           minimum;
    int           maximum;
    int           value;
    int           tick_interval;   /* 0 = no ticks */
    Boolean       show_value;
    Dimension     length;
    Dimension     thickness;
    XtCallbackList value_changed;
    XFontStruct  *font;
#ifdef ISW_INTERNATIONALIZATION
    ISWFontSet   *fontset;
#endif

    /* private state */
    Position      thumb_pos;      /* pixel position of thumb center */
    Boolean       dragging;
    int           drag_offset;    /* pixel offset from thumb center at grab */
    ISWRenderContext *render_ctx;
} ScalePart;

typedef struct _ScaleRec {
    CorePart    core;
    SimplePart  simple;
    ScalePart   scale;
} ScaleRec;

typedef struct {int empty;} ScaleClassPart;

typedef struct _ScaleClassRec {
    CoreClassPart   core_class;
    SimpleClassPart simple_class;
    ScaleClassPart  scale_class;
} ScaleClassRec;

extern ScaleClassRec scaleClassRec;

#endif /* _ISW_IswScaleP_h */
