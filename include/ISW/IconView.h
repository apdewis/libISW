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
#define XtNmultiSelect    "multiSelect"
#define XtCMultiSelect    "MultiSelect"
#define XtNcursorItem     "cursorItem"
#define XtCCursorItem     "CursorItem"

extern WidgetClass iconViewWidgetClass;

typedef struct _IconViewClassRec *IconViewWidgetClass;
typedef struct _IconViewRec      *IconViewWidget;

/* Callback data */
typedef struct {
    int    index;         /* last clicked index */
    String label;         /* label of last clicked item */
    int   *selected;      /* array of selected indices */
    int    num_selected;  /* count of selected items */
} IswIconViewCallbackData;

_XFUNCPROTOBEGIN

extern void IswIconViewSetItems(Widget w, String *labels, String *icon_data, int nitems);
extern int  IswIconViewGetSelected(Widget w);
extern int  IswIconViewGetSelectedItems(Widget w, int **indices_out);

_XFUNCPROTOEND

#endif /* _ISW_IswIconView_h */
