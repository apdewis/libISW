/*
 * Toolbar.h - Public header for Toolbar widget
 *
 * A horizontal strip for holding buttons and controls.
 * Subclasses Constraint to support per-child alignment (left, center, right).
 */

#ifndef _ISW_IswToolbar_h
#define _ISW_IswToolbar_h

#include <X11/Constraint.h>

/* Widget resources */
#ifndef XtNspacing
#define XtNspacing "spacing"
#endif
#ifndef XtCSpacing
#define XtCSpacing "Spacing"
#endif

/* Constraint resource names */
#define XtNtoolbarAlignment "toolbarAlignment"
#define XtCtoolbarAlignment "ToolbarAlignment"
#define XtRToolbarAlignment "ToolbarAlignment"

/* Alignment enum */
typedef enum {
    XtToolbarAlignLeft,
    XtToolbarAlignCenter,
    XtToolbarAlignRight
} IswToolbarAlignment;

extern WidgetClass toolbarWidgetClass;

typedef struct _ToolbarClassRec *ToolbarWidgetClass;
typedef struct _ToolbarRec      *ToolbarWidget;

#endif /* _ISW_IswToolbar_h */
