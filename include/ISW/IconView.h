/*
 * IconView.h - Public header for IconView widget
 *
 * A scrollable grid of items, each with an icon and text label.
 * Designed to be used inside a Viewport for scrolling.
 */

#ifndef _ISW_IswIconView_h
#define _ISW_IswIconView_h

#include <ISW/Simple.h>

/* Resource names */
#define XtNiconLabels     "iconLabels"
#define XtCIconLabels     "IconLabels"
#define XtNiconData       "iconData"
#define XtCIconData       "IconData"
#define XtNnumIcons       "numIcons"
#define XtCNumIcons       "NumIcons"
#define XtNiconSize       "iconSize"
#define XtCIconSize       "IconSize"
#define XtNitemSpacing    "itemSpacing"
#define XtCItemSpacing    "ItemSpacing"
#define XtNselectCallback "selectCallback"

extern WidgetClass iconViewWidgetClass;

typedef struct _IconViewClassRec *IconViewWidgetClass;
typedef struct _IconViewRec      *IconViewWidget;

/* Callback data */
typedef struct {
    int    index;
    String label;
} IswIconViewCallbackData;

_XFUNCPROTOBEGIN

extern void IswIconViewSetItems(Widget w, String *labels, String *icon_data, int nitems);
extern int  IswIconViewGetSelected(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswIconView_h */
