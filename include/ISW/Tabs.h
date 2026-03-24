/*
 * Tabs.h - Tabbed container widget public header
 *
 * A Constraint widget that displays a tab bar at the top.
 * Each managed child gets a tab; clicking a tab shows that child.
 * Per-child constraint resources set the tab label.
 */

#ifndef _ISW_IswTabs_h
#define _ISW_IswTabs_h

#include <X11/Constraint.h>

/* Widget resources */
#define XtNtabCallback    "tabCallback"
#define XtNtopWidget      "topWidget"

#define XtCTopWidget      "TopWidget"

/* Constraint resources (per-child) */
#define XtNtabLabel       "tabLabel"
#define XtCTabLabel       "TabLabel"

/* Class record constant */
extern WidgetClass tabsWidgetClass;

typedef struct _TabsClassRec  *TabsWidgetClass;
typedef struct _TabsRec       *TabsWidget;

/* Callback structure passed as call_data */
typedef struct {
    Widget  child;      /* The child that is now on top */
    int     tab_index;  /* Index of the selected tab */
} TabsCallbackStruct;

_XFUNCPROTOBEGIN

extern void IswTabsSetTop(Widget tabs, Widget child);

_XFUNCPROTOEND

#endif /* _ISW_IswTabs_h */
