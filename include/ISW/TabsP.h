/*
 * TabsP.h - Tabbed container widget private header
 */

#ifndef _ISW_IswTabsP_h
#define _ISW_IswTabsP_h

#include <ISW/Tabs.h>
#include <ISW/ISWRender.h>

/* Tabs constraint record -- per-child data */
typedef struct _TabsConstraintsPart {
    /* Resources */
    String  tab_label;

    /* Private */
    Dimension tab_width;
    Position  tab_x;
} TabsConstraintsPart;

typedef struct _TabsConstraintsRec {
    TabsConstraintsPart tabs;
} TabsConstraintsRec, *TabsConstraints;

/* Class part */
typedef struct _TabsClassPart {
    int foo;  /* keep compiler happy */
} TabsClassPart;

/* Full class record */
typedef struct _TabsClassRec {
    CoreClassPart       core_class;
    CompositeClassPart  composite_class;
    ConstraintClassPart constraint_class;
    TabsClassPart       tabs_class;
} TabsClassRec;

extern TabsClassRec tabsClassRec;

/* Instance part */
typedef struct _TabsPart {
    /* Resources */
    XtCallbackList  tab_callbacks;
    XFontStruct    *font;
    Pixel           foreground;

    /* Private */
    Widget          top_widget;
    ISWRenderContext *render_ctx;
} TabsPart;

/* Full instance record */
typedef struct _TabsRec {
    CorePart        core;
    CompositePart   composite;
    ConstraintPart  constraint;
    TabsPart        tabs;
} TabsRec;

#endif /* _ISW_IswTabsP_h */
