/*
 * StatusBar.h - Public header for StatusBar widget
 *
 * A horizontal strip for displaying status text, typically at the
 * bottom of a window. Children with statusStretch=True fill remaining space.
 */

#ifndef _ISW_IswStatusBar_h
#define _ISW_IswStatusBar_h

#include <ISW/Intrinsic.h>

/* Constraint resource: set on children to make them fill remaining space */
#define IswNstatusStretch  "statusStretch"
#define IswCStatusStretch  "StatusStretch"

extern WidgetClass statusBarWidgetClass;

typedef struct _StatusBarClassRec *StatusBarWidgetClass;
typedef struct _StatusBarRec      *StatusBarWidget;

#endif /* _ISW_IswStatusBar_h */
