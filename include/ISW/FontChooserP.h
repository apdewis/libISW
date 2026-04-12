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
    int             weight;         /* FC_WEIGHT value */
    int             slant;          /* FC_SLANT value */
    String          preview_text;
    XtCallbackList  font_changed;

    /* private */
    Widget          familyListW;
    Widget          styleListW;
    Widget          sizeListW;
    Widget          previewW;
    String         *family_names;   /* allocated array of font family names */
    int             num_families;
    String         *style_names;    /* display names ("Regular", "Bold", ...) */
    int            *style_weights;  /* FC_WEIGHT values for each style */
    int            *style_slants;   /* FC_SLANT values for each style */
    int             num_styles;
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
