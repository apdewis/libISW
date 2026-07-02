/*
 * ListBox.h - Public header for ListBox widget
 *
 * A Constraint-based container that lays out children as selectable
 * rows.  Each child can be any widget (Label, Box, Form, etc.).
 * Selection state is managed by the container; children are unaware.
 * Wrap in a Viewport for scrolling.
 *
 * A ListBox child that is itself a ListBox is treated as a collapsible
 * group: the parent draws a header band (chevron + pivotLabel text)
 * above the nested body, toggles it open/closed on chevron clicks, and
 * indents the body.  Selection, focus, and callbacks for the whole tree
 * are owned by the outermost ListBox; nested ListBoxes' own
 * selectionMode and callbacks are ignored.
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

/* Group constraint resources (apply to nested-ListBox children) */
#define IswNpivotLabel         "pivotLabel"
#define IswCPivotLabel         "PivotLabel"
#define IswNpivotOpen          "pivotOpen"
#define IswCPivotOpen          "PivotOpen"
#define IswNpivotImage         "pivotImage"
#define IswCPivotImage         "PivotImage"
#define IswNpivotImageOpen     "pivotImageOpen"
#define IswCPivotImageOpen     "PivotImageOpen"
#define IswNpivotCallback      "pivotCallback"

typedef enum {
    IswListBoxSelectNone   = 0,
    IswListBoxSelectSingle = 1,
    IswListBoxSelectMulti  = 2
} IswListBoxSelectionMode;

typedef struct {
    Widget  child;      /* the entry: a row, or a nested ListBox (group) */
    int     index;      /* index within the entry's immediate ListBox */
    Widget  list;       /* the immediate ListBox containing the entry */
    Widget *selected;   /* tree-wide selected entries */
    int     num_selected;
} IswListBoxCallbackData;

typedef struct {
    Widget  child;      /* the nested ListBox that was toggled */
    int     index;      /* index within its immediate parent ListBox */
    Boolean open;
} IswListBoxPivotCallbackData;

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
