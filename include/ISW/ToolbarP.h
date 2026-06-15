/*
 * ToolbarP.h - Private definitions for Toolbar widget
 */

#ifndef _ISW_IswToolbarP_h
#define _ISW_IswToolbarP_h

#include <ISW/Toolbar.h>
#include <ISW/ISWP.h>
#include <ISW/ISWRender.h>

typedef struct {int empty;} ToolbarClassPart;

typedef struct _ToolbarClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    ConstraintClassPart constraint_class;
    ToolbarClassPart    toolbar_class;
} ToolbarClassRec;

extern ToolbarClassRec toolbarClassRec;

typedef struct {
    /* resources */
    Dimension   h_space;
    Dimension   v_space;
    /* private */
    Dimension   preferred_width;
    Dimension   preferred_height;
    ISWRenderContext *render_ctx;
} ToolbarPart;

typedef struct _ToolbarRec {
    CorePart        core;
    CompositePart   composite;
    ConstraintPart  constraint;
    ToolbarPart     toolbar;
} ToolbarRec;

typedef struct _ToolbarConstraintsPart {
    IswToolbarAlignment alignment;
} ToolbarConstraintsPart;

typedef struct _ToolbarConstraintsRec {
    ToolbarConstraintsPart toolbar;
} ToolbarConstraintsRec, *ToolbarConstraints;

#endif /* _ISW_IswToolbarP_h */
