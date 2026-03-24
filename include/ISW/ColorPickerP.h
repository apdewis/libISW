/*
 * ColorPickerP.h - Private definitions for ColorPicker widget
 */

#ifndef _ISW_IswColorPickerP_h
#define _ISW_IswColorPickerP_h

#include <ISW/ColorPicker.h>
#include <ISW/FormP.h>

typedef struct {int empty;} ColorPickerClassPart;

typedef struct _ColorPickerClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    ConstraintClassPart    constraint_class;
    FormClassPart          form_class;
    ColorPickerClassPart   colorPicker_class;
} ColorPickerClassRec;

extern ColorPickerClassRec colorPickerClassRec;

typedef struct {
    /* resources */
    int             red, green, blue;
    XtCallbackList  color_changed;

    /* private */
    Widget          redScale;
    Widget          greenScale;
    Widget          blueScale;
    Widget          redLabel;
    Widget          greenLabel;
    Widget          blueLabel;
    Widget          swatchW;      /* preview swatch (Simple widget) */
} ColorPickerPart;

typedef struct _ColorPickerRec {
    CorePart           core;
    CompositePart      composite;
    ConstraintPart     constraint;
    FormPart           form;
    ColorPickerPart    colorPicker;
} ColorPickerRec;

typedef struct {int empty;} ColorPickerConstraintsPart;

typedef struct _ColorPickerConstraintsRec {
    FormConstraintsPart       form;
    ColorPickerConstraintsPart colorPicker;
} ColorPickerConstraintsRec;

#endif /* _ISW_IswColorPickerP_h */
