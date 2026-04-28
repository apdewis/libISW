#ifndef _ISW_IswToggleButtonP_h
#define _ISW_IswToggleButtonP_h

#include <ISW/ToggleButton.h>
#include <ISW/CommandP.h>
#include <ISW/ISWImage.h>

typedef struct _ToggleButtonClass {
    int makes_compiler_happy;
} ToggleButtonClassPart;

typedef struct _ToggleButtonClassRec {
    CoreClassPart          core_class;
    SimpleClassPart        simple_class;
    LabelClassPart         label_class;
    CommandClassPart       command_class;
    ToggleButtonClassPart  toggle_button_class;
} ToggleButtonClassRec;

extern ToggleButtonClassRec toggleButtonClassRec;

typedef struct {
    /* resources */
    String    image_on_source;
    String    image_off_source;

    /* private */
    ISWImage *image_on;
    ISWImage *image_off;
} ToggleButtonPart;

typedef struct _ToggleButtonRec {
    CorePart           core;
    SimplePart         simple;
    LabelPart          label;
    CommandPart        command;
    ToggleButtonPart   toggle_button;
} ToggleButtonRec;

#endif /* _ISW_IswToggleButtonP_h */
