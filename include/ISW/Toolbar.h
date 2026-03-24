/*
 * Toolbar.h - Public header for Toolbar widget
 *
 * A horizontal (or vertical) strip for holding buttons and controls.
 * Subclasses Box with tight spacing and consistent child styling.
 */

#ifndef _ISW_IswToolbar_h
#define _ISW_IswToolbar_h

#include <ISW/Box.h>

extern WidgetClass toolbarWidgetClass;

typedef struct _ToolbarClassRec *ToolbarWidgetClass;
typedef struct _ToolbarRec      *ToolbarWidget;

#endif /* _ISW_IswToolbar_h */
