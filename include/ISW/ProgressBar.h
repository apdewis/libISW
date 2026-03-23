/*
 * ProgressBar.h - Public definitions for ProgressBar widget
 *
 * A display-only widget showing a filled bar proportional to a value (0-100).
 * Supports horizontal and vertical orientations, and optional percentage text.
 */

#ifndef _ISW_ProgressBar_h
#define _ISW_ProgressBar_h

#include <ISW/Simple.h>

/* Resources:

 Name             Class            RepType       Default Value
 ----             -----            -------       -------------
 value            Value            Int           0
 foreground       Foreground       Pixel         XtDefaultForeground
 orientation      Orientation      Orientation   horizontal
 showValue        ShowValue        Boolean       True
 font             Font             FontStruct    XtDefaultFont

*/

#define XtNshowValue "showValue"
#define XtCShowValue "ShowValue"

#ifndef XtNvalue
#define XtNvalue "value"
#endif
#ifndef XtCValue
#define XtCValue "Value"
#endif

extern WidgetClass progressBarWidgetClass;

typedef struct _ProgressBarClassRec *ProgressBarWidgetClass;
typedef struct _ProgressBarRec      *ProgressBarWidget;

#endif /* _ISW_ProgressBar_h */
