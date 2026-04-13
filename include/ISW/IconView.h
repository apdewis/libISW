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
#define IswNiconLabels     "iconLabels"
#define IswCIconLabels     "IconLabels"
#define IswNiconData       "iconData"
#define IswCIconData       "IconData"
#define IswNnumIcons       "numIcons"
#define IswCNumIcons       "NumIcons"
#define IswNiconSize       "iconSize"
#define IswCIconSize       "IconSize"
#define IswNitemSpacing    "itemSpacing"
#define IswCItemSpacing    "ItemSpacing"
#define IswNlabelLines     "labelLines"
#define IswCLabelLines     "LabelLines"
#define IswNselectCallback "selectCallback"
#define IswNmultiSelect    "multiSelect"
#define IswCMultiSelect    "MultiSelect"
#define IswNcursorItem     "cursorItem"
#define IswCCursorItem     "CursorItem"

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
extern Boolean IswIconViewBandActive(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswIconView_h */
