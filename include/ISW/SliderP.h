/*
 * SliderP.h - Private definitions for Slider widget
 */

#ifndef _ISW_IswSliderP_h
#define _ISW_IswSliderP_h

#include "ISWP.h"
#include <ISW/SimpleP.h>
#include <ISW/Slider.h>
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
    IswSliderValuePosition value_pos;
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
} SliderPart;

typedef struct _SliderRec {
    CorePart    core;
    SimplePart  simple;
    SliderPart  slider;
} SliderRec;

typedef struct {int empty;} SliderClassPart;

typedef struct _SliderClassRec {
    CoreClassPart   core_class;
    SimpleClassPart simple_class;
    SliderClassPart slider_class;
} SliderClassRec;

extern SliderClassRec sliderClassRec;

#endif /* _ISW_IswSliderP_h */
