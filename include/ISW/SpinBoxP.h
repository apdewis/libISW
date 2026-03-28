/*
 * SpinBoxP.h - Private definitions for SpinBox widget
 */

#ifndef _ISW_IswSpinBoxP_h
#define _ISW_IswSpinBoxP_h

#include <ISW/SpinBox.h>
#include <ISW/FormP.h>
#include <ISW/ISWRender.h>

typedef struct {int empty;} SpinBoxClassPart;

typedef struct _SpinBoxClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    ConstraintClassPart constraint_class;
    FormClassPart       form_class;
    SpinBoxClassPart    spinBox_class;
} SpinBoxClassRec;

extern SpinBoxClassRec spinBoxClassRec;

typedef struct _SpinBoxPart {
    /* resources */
    int           minimum;
    int           maximum;
    int           value;
    int           increment;
    Boolean       wrap;
    XtCallbackList value_changed;

    /* private */
    Widget        textW;       /* AsciiText child */
    Widget        upW;         /* up/increment Repeater */
    Widget        downW;       /* down/decrement Repeater */
    ISWRenderContext *render_ctx;
} SpinBoxPart;

typedef struct _SpinBoxRec {
    CorePart        core;
    CompositePart   composite;
    ConstraintPart  constraint;
    FormPart        form;
    SpinBoxPart     spinBox;
} SpinBoxRec;

typedef struct {int empty;} SpinBoxConstraintsPart;

typedef struct _SpinBoxConstraintsRec {
    FormConstraintsPart   form;
    SpinBoxConstraintsPart spinBox;
} SpinBoxConstraintsRec;

#endif /* _ISW_IswSpinBoxP_h */
