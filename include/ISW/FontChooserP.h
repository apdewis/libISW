/*
 * FontChooserP.h - Private definitions for FontChooser widget
 */

#ifndef _ISW_IswFontChooserP_h
#define _ISW_IswFontChooserP_h

#include <ISW/FontChooser.h>
#include <ISW/FormP.h>

typedef struct {int empty;} FontChooserClassPart;

typedef struct _FontChooserClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    ConstraintClassPart    constraint_class;
    FormClassPart          form_class;
    FontChooserClassPart   fontChooser_class;
} FontChooserClassRec;

extern FontChooserClassRec fontChooserClassRec;

typedef struct {
    /* resources */
    String          family;
    int             size;
    String          preview_text;
    XtCallbackList  font_changed;

    /* private */
    Widget          familyListW;
    Widget          sizeListW;
    Widget          previewW;
    String         *family_names;   /* allocated array of font family names */
    int             num_families;
} FontChooserPart;

typedef struct _FontChooserRec {
    CorePart           core;
    CompositePart      composite;
    ConstraintPart     constraint;
    FormPart           form;
    FontChooserPart    fontChooser;
} FontChooserRec;

typedef struct {int empty;} FontChooserConstraintsPart;

typedef struct _FontChooserConstraintsRec {
    FormConstraintsPart        form;
    FontChooserConstraintsPart fontChooser;
} FontChooserConstraintsRec;

#endif /* _ISW_IswFontChooserP_h */
