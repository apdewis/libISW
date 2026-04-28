#ifndef _ISW_IswToggleButton_h
#define _ISW_IswToggleButton_h

#include <ISW/Command.h>

#define IswNstate "state"
#define IswCState "State"

#define IswNimageOn "imageOn"
#define IswCImageOn "ImageOn"
#define IswNimageOff "imageOff"
#define IswCImageOff "ImageOff"

extern WidgetClass toggleButtonWidgetClass;

typedef struct _ToggleButtonClassRec *ToggleButtonWidgetClass;
typedef struct _ToggleButtonRec     *ToggleButtonWidget;

#endif /* _ISW_IswToggleButton_h */
