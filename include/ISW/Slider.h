/*
 * Slider.h - Public header for Slider widget
 *
 * A slider control for selecting a value within a range.
 * Supports horizontal and vertical orientations.
 */

#ifndef _ISW_IswSlider_h
#define _ISW_IswSlider_h

/* Slider-specific resource names */
#define IswNminimumValue   "minimumValue"
#define IswCMinimumValue   "MinimumValue"
#define IswNmaximumValue   "maximumValue"
#define IswCMaximumValue   "MaximumValue"
#define IswNsliderValue    "sliderValue"
#define IswCSliderValue    "SliderValue"
#define IswNshowValue      "showValue"
#define IswCShowValue      "ShowValue"
#define IswNvalueChanged   "valueChanged"
#define IswNvaluePosition  "valuePosition"
#define IswCValuePosition  "ValuePosition"
#define IswNtickInterval   "tickInterval"
#define IswCTickInterval   "TickInterval"

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
