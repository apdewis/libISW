/*
 * Tabs.h - Tabbed container widget public header
 *
 * A Constraint widget that displays a tab bar at the top.
 * Each managed child gets a tab; clicking a tab shows that child.
 * Per-child constraint resources set the tab label.
 */

#ifndef _ISW_IswTabs_h
#define _ISW_IswTabs_h

#include <ISW/Constraint.h>

/* Widget resources */
#define IswNtabCallback    "tabCallback"
#define IswNtopWidget      "topWidget"
#define IswNtabHeight      "tabHeight"
#define IswNtabSizing      "tabSizing"
#define IswNtabBackground  "tabBackground"
#define IswNactiveTabColor "activeTabColor"
#define IswNtabBorderColor "tabBorderColor"

#define IswCTopWidget      "TopWidget"
#define IswCTabHeight      "TabHeight"
#define IswCTabSizing      "TabSizing"
#define IswCTabBackground  "TabBackground"
#define IswCActiveTabColor "ActiveTabColor"
#define IswCTabBorderColor "TabBorderColor"

#ifndef IswNcornerRadius
#define IswNcornerRadius   "cornerRadius"
#endif
#ifndef IswCCornerRadius
#define IswCCornerRadius   "CornerRadius"
#endif

#define IswRTabSizing      "TabSizing"

/* Tab sizing mode */
typedef enum {
    IswTabSizingText,  /* each tab sized to its label text (default) */
    IswTabSizingFill   /* tabs distributed evenly to fill widget width */
} IswTabSizing;

/* Constraint resources (per-child) */
#define IswNtabLabel       "tabLabel"
#define IswCTabLabel       "TabLabel"

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
