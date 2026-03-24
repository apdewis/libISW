/*
 * Scale.h - Public header for Scale widget
 *
 * A slider control for selecting a value within a range.
 * Supports horizontal and vertical orientations.
 */

#ifndef _ISW_IswScale_h
#define _ISW_IswScale_h

#include <X11/Xfuncproto.h>

/* Scale-specific resource names */
#define XtNminimumValue   "minimumValue"
#define XtCMinimumValue   "MinimumValue"
#define XtNmaximumValue   "maximumValue"
#define XtCMaximumValue   "MaximumValue"
#define XtNscaleValue     "scaleValue"
#define XtCScaleValue     "ScaleValue"
#define XtNshowValue      "showValue"
#define XtCShowValue      "ShowValue"
#define XtNvalueChanged   "valueChanged"
#define XtNvaluePosition  "valuePosition"
#define XtCValuePosition  "ValuePosition"
#define XtNtickInterval   "tickInterval"
#define XtCTickInterval   "TickInterval"

/* Value position enum */
typedef enum {
    IswScaleValueTop,
    IswScaleValueBottom,
    IswScaleValueLeft,
    IswScaleValueRight
} IswScaleValuePosition;

extern WidgetClass scaleWidgetClass;

typedef struct _ScaleClassRec *ScaleWidgetClass;
typedef struct _ScaleRec      *ScaleWidget;

/* Callback data passed with valueChanged */
typedef struct {
    int value;
} IswScaleCallbackData;

_XFUNCPROTOBEGIN

extern void IswScaleSetValue(Widget w, int value);
extern int  IswScaleGetValue(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswScale_h */
