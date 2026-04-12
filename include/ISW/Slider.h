/*
 * Slider.h - Public header for Slider widget
 *
 * A slider control for selecting a value within a range.
 * Supports horizontal and vertical orientations.
 */

#ifndef _ISW_IswSlider_h
#define _ISW_IswSlider_h

#include <X11/Xfuncproto.h>

/* Slider-specific resource names */
#define XtNminimumValue   "minimumValue"
#define XtCMinimumValue   "MinimumValue"
#define XtNmaximumValue   "maximumValue"
#define XtCMaximumValue   "MaximumValue"
#define XtNsliderValue    "sliderValue"
#define XtCSliderValue    "SliderValue"
#define XtNshowValue      "showValue"
#define XtCShowValue      "ShowValue"
#define XtNvalueChanged   "valueChanged"
#define XtNvaluePosition  "valuePosition"
#define XtCValuePosition  "ValuePosition"
#define XtNtickInterval   "tickInterval"
#define XtCTickInterval   "TickInterval"

/* Value position enum */
typedef enum {
    IswSliderValueTop,
    IswSliderValueBottom,
    IswSliderValueLeft,
    IswSliderValueRight
} IswSliderValuePosition;

extern WidgetClass sliderWidgetClass;

typedef struct _SliderClassRec *SliderWidgetClass;
typedef struct _SliderRec      *SliderWidget;

/* Callback data passed with valueChanged */
typedef struct {
    int value;
} IswSliderCallbackData;

_XFUNCPROTOBEGIN

extern void IswSliderSetValue(Widget w, int value);
extern int  IswSliderGetValue(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswSlider_h */
