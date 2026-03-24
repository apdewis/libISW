/*
 * ToolbarP.h - Private definitions for Toolbar widget
 */

#ifndef _ISW_IswToolbarP_h
#define _ISW_IswToolbarP_h

#include <ISW/Toolbar.h>
#include <ISW/BoxP.h>

typedef struct {int empty;} ToolbarClassPart;

typedef struct _ToolbarClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    BoxClassPart        box_class;
    ToolbarClassPart    toolbar_class;
} ToolbarClassRec;

extern ToolbarClassRec toolbarClassRec;

typedef struct {int empty;} ToolbarPart;

typedef struct _ToolbarRec {
    CorePart        core;
    CompositePart   composite;
    BoxPart         box;
    ToolbarPart     toolbar;
} ToolbarRec;

#endif /* _ISW_IswToolbarP_h */
