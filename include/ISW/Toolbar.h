/*
 * Toolbar.h - Public header for Toolbar widget
 *
 * A horizontal strip for holding buttons and controls.
 * Subclasses Constraint to support per-child alignment (left, center, right).
 */

#ifndef _ISW_IswToolbar_h
#define _ISW_IswToolbar_h

#include <ISW/Constraint.h>

/* Widget resources */
#ifndef IswNspacing
#define IswNspacing "spacing"
#endif
#ifndef IswCSpacing
#define IswCSpacing "Spacing"
#endif

/* Constraint resource names */
#define IswNtoolbarAlignment "toolbarAlignment"
#define IswCtoolbarAlignment "ToolbarAlignment"
#define IswRToolbarAlignment "ToolbarAlignment"

/* Alignment enum */
typedef enum {
    IswToolbarAlignLeft,
    IswToolbarAlignCenter,
    IswToolbarAlignRight
} IswToolbarAlignment;

extern WidgetClass toolbarWidgetClass;

typedef struct _ToolbarClassRec *ToolbarWidgetClass;
typedef struct _ToolbarRec      *ToolbarWidget;

#endif /* _ISW_IswToolbar_h */
