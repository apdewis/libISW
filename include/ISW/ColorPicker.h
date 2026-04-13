/*
 * ColorPicker.h - Public header for ColorPicker widget
 *
 * A color selection widget with R/G/B sliders and a preview swatch.
 */

#ifndef _ISW_IswColorPicker_h
#define _ISW_IswColorPicker_h

#include <X11/Xfuncproto.h>

#define IswNcolorRed       "colorRed"
#define IswCColorRed       "ColorRed"
#define IswNcolorGreen     "colorGreen"
#define IswCColorGreen     "ColorGreen"
#define IswNcolorBlue      "colorBlue"
#define IswCColorBlue      "ColorBlue"
#define IswNcolorChanged   "colorChanged"

extern WidgetClass colorPickerWidgetClass;

typedef struct _ColorPickerClassRec *ColorPickerWidgetClass;
typedef struct _ColorPickerRec      *ColorPickerWidget;

typedef struct {
    int red, green, blue;   /* 0-255 each */
} IswColorPickerCallbackData;

_XFUNCPROTOBEGIN

extern void IswColorPickerGetColor(Widget w, int *r, int *g, int *b);
extern void IswColorPickerSetColor(Widget w, int r, int g, int b);

_XFUNCPROTOEND

#endif /* _ISW_IswColorPicker_h */
