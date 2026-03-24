/*
 * SpinBox.h - Public header for SpinBox widget
 *
 * A numeric entry with up/down arrow buttons for increment/decrement.
 * Subclasses Form; creates internal AsciiText + two Repeater children.
 */

#ifndef _ISW_IswSpinBox_h
#define _ISW_IswSpinBox_h

#include <X11/Xfuncproto.h>

/* SpinBox-specific resource names */
#define XtNspinMinimum    "spinMinimum"
#define XtCSpinMinimum    "SpinMinimum"
#define XtNspinMaximum    "spinMaximum"
#define XtCSpinMaximum    "SpinMaximum"
#define XtNspinValue      "spinValue"
#define XtCSpinValue      "SpinValue"
#define XtNspinIncrement  "spinIncrement"
#define XtCSpinIncrement  "SpinIncrement"
#define XtNspinWrap       "spinWrap"
#define XtCSpinWrap       "SpinWrap"
#define XtNvalueChanged   "valueChanged"

extern WidgetClass spinBoxWidgetClass;

typedef struct _SpinBoxClassRec *SpinBoxWidgetClass;
typedef struct _SpinBoxRec      *SpinBoxWidget;

/* Callback data passed with valueChanged */
typedef struct {
    int value;
} IswSpinBoxCallbackData;

_XFUNCPROTOBEGIN

extern void IswSpinBoxSetValue(Widget w, int value);
extern int  IswSpinBoxGetValue(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswSpinBox_h */
