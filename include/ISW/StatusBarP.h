/*
 * StatusBarP.h - Private definitions for StatusBar widget
 */

#ifndef _ISW_IswStatusBarP_h
#define _ISW_IswStatusBarP_h

#include <ISW/ISWP.h>
#include <ISW/StatusBar.h>
#include <ISW/ISWRender.h>
#include <X11/ConstrainP.h>

typedef struct {int empty;} StatusBarClassPart;

typedef struct _StatusBarClassRec {
    CoreClassPart          core_class;
    CompositeClassPart     composite_class;
    ConstraintClassPart    constraint_class;
    StatusBarClassPart     statusBar_class;
} StatusBarClassRec;

extern StatusBarClassRec statusBarClassRec;

typedef struct {
    Dimension   h_space;    /* horizontal spacing between children */
    ISWRenderContext *render_ctx;
} StatusBarPart;

typedef struct _StatusBarRec {
    CorePart          core;
    CompositePart     composite;
    ConstraintPart    constraint;
    StatusBarPart     statusBar;
} StatusBarRec;

/* Per-child constraint */
typedef struct {
    Boolean stretch;    /* if True, this child fills remaining space */
} StatusBarConstraintsPart;

typedef struct _StatusBarConstraintsRec {
    StatusBarConstraintsPart statusBar;
} StatusBarConstraintsRec;

#endif /* _ISW_IswStatusBarP_h */
