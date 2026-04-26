/*
 * ListBox.h - Public header for ListBox widget
 *
 * A Constraint-based container that lays out children as selectable
 * rows.  Each child can be any widget (Label, Box, Form, etc.).
 * Selection state is managed by the container; children are unaware.
 * Wrap in a Viewport for scrolling.
 */

#ifndef _ISW_IswListBox_h
#define _ISW_IswListBox_h

#include <ISW/Constraint.h>

/* Widget-level resource names */
#define IswNselectionMode      "selectionMode"
#define IswCSelectionMode      "SelectionMode"
#define IswRSelectionMode      "SelectionMode"
#define IswNrowSpacing         "rowSpacing"
#define IswCRowSpacing         "RowSpacing"
#define IswNshowSeparators     "showSeparators"
#define IswCShowSeparators     "ShowSeparators"
#define IswNselectCallback     "selectCallback"
#define IswNactivateCallback   "activateCallback"

/* Constraint resource names (per child) */
#define IswNselectable         "selectable"
#define IswCSelectable         "Selectable"
#define IswNlistBoxRowHeight   "listBoxRowHeight"
#define IswCListBoxRowHeight   "ListBoxRowHeight"
#define IswNseparator          "separator"
#define IswCSeparator          "Separator"

typedef enum {
    IswListBoxSelectNone   = 0,
    IswListBoxSelectSingle = 1,
    IswListBoxSelectMulti  = 2
} IswListBoxSelectionMode;

typedef struct {
    Widget  child;
    int     index;
    Widget *selected;
    int     num_selected;
} IswListBoxCallbackData;

extern WidgetClass listBoxWidgetClass;

typedef struct _ListBoxClassRec *ListBoxWidgetClass;
typedef struct _ListBoxRec      *ListBoxWidget;

_XFUNCPROTOBEGIN

extern Widget IswListBoxGetSelected(Widget w);
extern int    IswListBoxGetSelectedChildren(Widget w, Widget **children_out);
extern void   IswListBoxClearSelection(Widget w);
extern void   IswListBoxSelectChild(Widget w, Widget child);

_XFUNCPROTOEND

#endif /* _ISW_IswListBox_h */
