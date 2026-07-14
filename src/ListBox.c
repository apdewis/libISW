/*
 * ListBox.c - Constraint widget that lays out children as selectable rows
 *
 * Children are managed with IswManageChild.  The container stacks them
 * vertically and tracks selection state.  The child's X window background
 * is changed to reflect selection; children remain completely unaware.
 *
 * A child that is itself a ListBox is a collapsible group: this widget
 * draws its header band (chevron + pivotLabel), toggles it on chevron
 * clicks, and indents the nested body.  Selection, focus, and callbacks
 * for the whole tree are owned by the selection root — the outermost
 * ListBox.  A closed group's body is hidden via IswUnmapWidget, so it
 * neither paints nor hit-tests; the child stays managed and remains one
 * entry (its header) in the parent.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/EventI.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ListBoxP.h>
#include <ISW/LabelP.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWImage.h>
#include <ISW/ISWPlatform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOUBLE_CLICK_MS 400

/* Group header metrics.  The chevron zone is a square of the header
   height at the left edge; the open body is indented past it. */
#define GROUP_HEADER_VPAD 3   /* padding above/below header text */
#define GROUP_ICON_PAD    3   /* chevron inset within its square zone */
#define GROUP_LABEL_GAP   4   /* gap between chevron zone and header text */

static const char pivot_closed_svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='32'>"
    "<path d='M10,4 L26,16 L10,28' stroke='currentColor' stroke-width='4' "
    "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";

static const char pivot_open_svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='32'>"
    "<path d='M4,10 L16,26 L28,10' stroke='currentColor' stroke-width='4' "
    "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";

/* ================================================================
 * Forward declarations
 * ================================================================ */

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *,
                                         IswWidgetGeometry *);
static IswGeometryResult PreferredGeometry(Widget, IswWidgetGeometry *,
                                           IswWidgetGeometry *);
static void ChangeManaged(Widget);
static void ConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static void ConstraintDestroy(Widget);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void DoLayout(ListBoxWidget, Boolean);
static void ApplyPivotState(ListBoxWidget, Widget);
static void FireSelectCallback(ListBoxWidget, ListBoxWidget, Widget, int);

static void ChildEventHandler(Widget, IswPointer, IswEvent *, Boolean *);
static void InstallChildHandlers(Widget, Widget);

static void SelectAction(Widget, IswEvent *, String *, Cardinal *);
static void MoveFocusAction(Widget, IswEvent *, String *, Cardinal *);
static void ToggleAction(Widget, IswEvent *, String *, Cardinal *);
static void ActivateAction(Widget, IswEvent *, String *, Cardinal *);
static void ExtendAction(Widget, IswEvent *, String *, Cardinal *);
static void SelectAllAction(Widget, IswEvent *, String *, Cardinal *);
static void FocusAction(Widget, IswEvent *, String *, Cardinal *);

/* ================================================================
 * Resources
 * ================================================================ */

#define Offset(field) IswOffsetOf(ListBoxRec, listBox.field)
static IswResource resources[] = {
    {IswNselectionMode, IswCSelectionMode, IswRSelectionMode,
        sizeof(IswListBoxSelectionMode),
        Offset(selection_mode), IswRImmediate,
        (IswPointer)IswListBoxSelectSingle},
    {IswNrowSpacing, IswCRowSpacing, IswRDimension, sizeof(Dimension),
        Offset(row_spacing), IswRImmediate, (IswPointer)2},
    {IswNshowSeparators, IswCShowSeparators, IswRBoolean, sizeof(Boolean),
        Offset(show_separators), IswRImmediate, (IswPointer)False},
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
        Offset(foreground), IswRString, IswDefaultForeground},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
        Offset(font), IswRString, IswDefaultFont},
    {IswNselectCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(select_callback), IswRCallback, NULL},
    {IswNactivateCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(activate_callback), IswRCallback, NULL},
    {IswNpivotCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(pivot_callback), IswRCallback, NULL},
};
#undef Offset

#define COffset(field) IswOffsetOf(ListBoxConstraintsRec, listBox.field)
static IswResource constraintResources[] = {
    {IswNselectable, IswCSelectable, IswRBoolean, sizeof(Boolean),
        COffset(selectable), IswRImmediate, (IswPointer)True},
    {IswNlistBoxRowHeight, IswCListBoxRowHeight, IswRDimension, sizeof(Dimension),
        COffset(row_height), IswRImmediate, (IswPointer)0},
    {IswNseparator, IswCSeparator, IswRBoolean, sizeof(Boolean),
        COffset(separator), IswRImmediate, (IswPointer)False},
    {IswNpivotLabel, IswCPivotLabel, IswRString, sizeof(String),
        COffset(pivot_label), IswRImmediate, (IswPointer)NULL},
    {IswNpivotOpen, IswCPivotOpen, IswRBoolean, sizeof(Boolean),
        COffset(pivot_open), IswRImmediate, (IswPointer)False},
    {IswNpivotImage, IswCPivotImage, IswRString, sizeof(String),
        COffset(pivot_image), IswRImmediate, (IswPointer)NULL},
    {IswNpivotImageOpen, IswCPivotImageOpen, IswRString, sizeof(String),
        COffset(pivot_image_open), IswRImmediate, (IswPointer)NULL},
};
#undef COffset

/* ================================================================
 * Translations and actions
 * ================================================================ */

static char defaultTranslations[] =
    "<PrimaryDown>:         ListBoxSelect()\n"
    "~Shift ~Ctrl<Key>Up:   ListBoxMoveFocus(up)\n"
    "~Shift ~Ctrl<Key>Down: ListBoxMoveFocus(down)\n"
    "<Key>Home:           ListBoxMoveFocus(home)\n"
    "<Key>End:            ListBoxMoveFocus(end)\n"
    "<Key>space:          ListBoxToggle()\n"
    "<Key>Return:         ListBoxActivate()\n"
    "Shift<Key>Up:        ListBoxExtend(up)\n"
    "Shift<Key>Down:      ListBoxExtend(down)\n"
    "Ctrl<Key>a:          ListBoxSelectAll()\n"
    "<FocusIn>:           ListBoxFocus(in)\n"
    "<FocusOut>:          ListBoxFocus(out)";

static IswActionsRec actionsList[] = {
    {"ListBoxSelect",    SelectAction},
    {"ListBoxMoveFocus", MoveFocusAction},
    {"ListBoxToggle",    ToggleAction},
    {"ListBoxActivate",  ActivateAction},
    {"ListBoxExtend",    ExtendAction},
    {"ListBoxSelectAll", SelectAllAction},
    {"ListBoxFocus",     FocusAction},
};

/* ================================================================
 * Class record
 * ================================================================ */

ListBoxClassRec listBoxClassRec = {
  { /* core_class */
    (WidgetClass) &constraintClassRec,
    "ListBox",
    sizeof(ListBoxRec),
    ClassInitialize,
    NULL,                                /* class_part_initialize */
    FALSE,
    Initialize,
    NULL,                                /* initialize_hook */
    IswInheritRealize,
    actionsList,
    IswNumber(actionsList),
    resources,
    IswNumber(resources),
    ISW_NULLQUARK,
    TRUE,                                /* compress_motion */
    TRUE,                                /* compress_exposure */
    TRUE,                                /* compress_enterleave */
    FALSE,                               /* visible_interest */
    Destroy,
    Resize,
    Redisplay,
    SetValues,
    NULL,                                /* set_values_hook */
    IswInheritSetValuesAlmost,
    NULL,                                /* get_values_hook */
    NULL,                                /* accept_focus */
    IswVersion,
    NULL,                                /* callback_private */
    defaultTranslations,
    PreferredGeometry,
    IswInheritDisplayAccelerator,
    NULL                                 /* extension */
  },
  { /* composite_class */
    GeometryManager,
    ChangeManaged,
    IswInheritInsertChild,
    IswInheritDeleteChild,
    NULL
  },
  { /* constraint_class */
    constraintResources,
    IswNumber(constraintResources),
    sizeof(ListBoxConstraintsRec),
    ConstraintInitialize,
    ConstraintDestroy,
    ConstraintSetValues,
    NULL
  },
  { /* listBox_class */
    0
  }
};

WidgetClass listBoxWidgetClass = (WidgetClass)&listBoxClassRec;

/* ================================================================
 * Helpers
 * ================================================================ */

static Boolean
IsGroup(Widget child)
{
    return IswIsSubclass(child, listBoxWidgetClass);
}

/* The outermost ListBox of a nested tree: owner of selection, focus,
   and callbacks for every entry beneath it. */
static ListBoxWidget
SelectionRoot(ListBoxWidget lbw)
{
    Widget w = (Widget)lbw;
    while (IswParent(w) != NULL &&
           IswIsSubclass(IswParent(w), listBoxWidgetClass))
        w = IswParent(w);
    return (ListBoxWidget)w;
}

static Dimension
HeaderHeight(ListBoxWidget lbw)
{
    if (lbw->listBox.font)
        return (Dimension)(ISWScaledFontHeight((Widget)lbw, lbw->listBox.font)
                           + 2 * GROUP_HEADER_VPAD);
    return 20;
}

static int
IndexOfManagedChild(ListBoxWidget lbw, Widget child)
{
    int n = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget c = lbw->composite.children[i];
        if (!IswIsManaged(c)) continue;
        if (c == child) return n;
        n++;
    }
    return -1;
}

static Boolean
WidgetIsInside(Widget w, Widget ancestor)
{
    for (w = IswParent(w); w != NULL; w = IswParent(w))
        if (w == ancestor) return True;
    return False;
}

/* Flat visible-entry order for keyboard navigation: each managed child
   is an entry; an open group contributes its own entries right after
   its header.  Closed groups contribute the header only. */
static void
FlattenInto(ListBoxWidget lbw, Widget **arr, int *n, int *cap)
{
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        if (*n == *cap) {
            *cap = *cap ? *cap * 2 : 16;
            *arr = (Widget *)IswRealloc((char *)*arr,
                       (Cardinal)(*cap * (int)sizeof(Widget)));
        }
        (*arr)[(*n)++] = child;
        if (IsGroup(child)) {
            ListBoxConstraints lbc =
                (ListBoxConstraints)child->core.constraints;
            if (lbc->listBox.pivot_open)
                FlattenInto((ListBoxWidget)child, arr, n, cap);
        }
    }
}

static Widget *
FlattenEntries(ListBoxWidget root, int *count)
{
    Widget *arr = NULL;
    int n = 0, cap = 0;
    FlattenInto(root, &arr, &n, &cap);
    *count = n;
    return arr;
}

typedef enum { HitNone, HitRow, HitHeader, HitChevron } HitZone;

/* Resolve a point in lbw's frame to one of its own entries.  Points
   inside an open group's body resolve to HitNone: those events dispatch
   to the nested ListBox directly and are never this widget's business. */
static HitZone
HitTestEx(ListBoxWidget lbw, int x, int y, Widget *child_out)
{
    Dimension header_h = HeaderHeight(lbw);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;

        if (IsGroup(child)) {
            int hy = lbc->listBox.header_y;
            if (y >= hy && y < hy + (int)header_h) {
                *child_out = child;
                return (x < (int)header_h) ? HitChevron : HitHeader;
            }
        } else {
            int cy = child->core.y;
            IswBorderSides bs = _IswGetBorderSides(child);
            int ch = (int)child->core.height + _IswBorderVert(bs);
            if (y >= cy && y < cy + ch) {
                *child_out = child;
                return HitRow;
            }
        }
    }
    return HitNone;
}

/*
 * Forward button presses from descendant widgets to the ListBox so that
 * clicks on labels inside a composite row still trigger selection.
 */
static void
ChildEventHandler(Widget child, IswPointer closure,
                  IswEvent *iswev, Boolean *continue_to_dispatch)
{
    (void)continue_to_dispatch;
    Widget listbox = (Widget)closure;

    if (iswev->kind != IswButtonDown)
        return;

    if (IswEventButton(iswev) != IswButtonPrimary)
        return;

    /* Translate event coordinates from child frame to ListBox frame */
    Position rx, ry, lx, ly;
    IswTranslateCoords(child, (Position)IswEventX(iswev),
                       (Position)IswEventY(iswev), &rx, &ry);
    IswTranslateCoords(listbox, 0, 0, &lx, &ly);

    /* Synthetic button event in the ListBox frame, with neutral fields
       carried over so SelectAction reads them directly. */
    IswEvent nev = *iswev;
    nev.button.x = (int16_t)(rx - lx);
    nev.button.y = (int16_t)(ry - ly);

    String action = "ListBoxSelect";
    Cardinal one = 1;
    SelectAction(listbox, &nev, &action, &one);
}

static void
InstallChildHandlers(Widget child, Widget listbox)
{
    IswAddEventHandler(child, IswButtonPressMask, False,
                       ChildEventHandler, (IswPointer)listbox);

    if (IswIsComposite(child)) {
        CompositeWidget cw = (CompositeWidget)child;
        for (Cardinal i = 0; i < cw->composite.num_children; i++) {
            if (IswIsSubclass(cw->composite.children[i], listBoxWidgetClass))
                continue;
            InstallChildHandlers(cw->composite.children[i], listbox);
        }
    }
}

static void
SwapFgBg(Widget w, Pixel fg, Pixel bg)
{
    Pixel old_bg = w->core.background_pixel;
    w->core.background_pixel = (old_bg == bg) ? fg : bg;

    if (IswIsSubclass(w, labelWidgetClass)) {
        LabelWidget lw = (LabelWidget)w;
        Pixel old_fg = lw->label.foreground;
        lw->label.foreground = (old_fg == fg) ? bg : fg;
    }

    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget)w;
        for (Cardinal i = 0; i < cw->composite.num_children; i++) {
            if (IswIsSubclass(cw->composite.children[i], listBoxWidgetClass))
                continue;
            SwapFgBg(cw->composite.children[i], fg, bg);
        }
    }
}

static void
ExposeTree(Widget w)
{
    if (IswIsRealized(w))
        _IswRepaintWindowless(w);
}

static Pixel
FindLabelFg(Widget w)
{
    if (IswIsSubclass(w, labelWidgetClass))
        return ((LabelWidget)w)->label.foreground;
    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget)w;
        for (Cardinal i = 0; i < cw->composite.num_children; i++) {
            if (IswIsSubclass(cw->composite.children[i], listBoxWidgetClass))
                continue;
            Pixel fg = FindLabelFg(cw->composite.children[i]);
            if (fg != (Pixel)-1) return fg;
        }
    }
    return (Pixel)-1;
}

static void
ApplySelectionVisual(ListBoxWidget lbw, Widget child, Boolean select)
{
    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    (void)lbw;
    if (select == lbc->listBox.selected) return;
    lbc->listBox.selected = select;

    /* A group's highlight is its parent-drawn header band; never swap
       colors inside the nested widget. */
    if (IsGroup(child))
        return;

    Pixel fg = FindLabelFg(child);
    Pixel bg = child->core.background_pixel;
    if (fg == (Pixel)-1) fg = ~bg;

    SwapFgBg(child, fg, bg);
    ExposeTree(child);
}

/* Deselect every entry in lbw's subtree.  Returns True if anything was
   actually deselected. */
static Boolean
ClearAllSelections(ListBoxWidget lbw)
{
    Boolean changed = False;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) {
            ApplySelectionVisual(lbw, child, False);
            changed = True;
        }
        if (IsGroup(child))
            changed |= ClearAllSelections((ListBoxWidget)child);
    }
    return changed;
}

static int
CountSelectedIn(ListBoxWidget lbw)
{
    int n = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) n++;
        if (IsGroup(child))
            n += CountSelectedIn((ListBoxWidget)child);
    }
    return n;
}

static void
GatherSelectedIn(ListBoxWidget lbw, Widget *out, int *j)
{
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) out[(*j)++] = child;
        if (IsGroup(child))
            GatherSelectedIn((ListBoxWidget)child, out, j);
    }
}

static void
FireSelectCallback(ListBoxWidget root, ListBoxWidget list, Widget child,
                   int index)
{
    IswListBoxCallbackData cb;
    Widget *sel = NULL;
    int nsel = CountSelectedIn(root);

    if (nsel > 0) {
        sel = (Widget *)IswMalloc((Cardinal)(nsel * (int)sizeof(Widget)));
        int j = 0;
        GatherSelectedIn(root, sel, &j);
    }

    cb.child = child;
    cb.index = index;
    cb.list = (Widget)list;
    cb.selected = sel;
    cb.num_selected = nsel;

    IswCallCallbacks((Widget)root, IswNselectCallback, (IswPointer)&cb);

    if (sel) IswFree((char *)sel);
}

static void
FireActivateCallback(ListBoxWidget root, ListBoxWidget list, Widget child,
                     int index)
{
    IswListBoxCallbackData cb;
    cb.child = child;
    cb.index = index;
    cb.list = (Widget)list;
    cb.selected = NULL;
    cb.num_selected = 0;
    IswCallCallbacks((Widget)root, IswNactivateCallback, (IswPointer)&cb);
}

/* ================================================================
 * Group pivot (open/close) machinery
 * ================================================================ */

/* Apply lbc->pivot_open (already set to the desired state): show or
   hide the body, enforce the no-hidden-selection invariant, relayout,
   and fire callbacks.  Shared by chevron clicks and programmatic
   pivotOpen changes so the collapse rules always hold. */
static void
ApplyPivotState(ListBoxWidget lbw, Widget group)
{
    ListBoxConstraints lbc = (ListBoxConstraints)group->core.constraints;
    ListBoxWidget root = SelectionRoot(lbw);
    Boolean open = lbc->listBox.pivot_open;
    Boolean sel_changed = False;
    int index = IndexOfManagedChild(lbw, group);

    if (open) {
        IswMapWidget(group);
    } else {
        sel_changed = ClearAllSelections((ListBoxWidget)group);
        Widget f = root->listBox.focused_child;
        if (f != NULL && WidgetIsInside(f, group))
            root->listBox.focused_child = group;
        IswUnmapWidget(group);
    }

    DoLayout(lbw, False);
    DoLayout(lbw, True);
    if (IswIsRealized((Widget)root))
        _IswRepaintWindowless((Widget)root);

    IswListBoxPivotCallbackData pcb;
    pcb.child = group;
    pcb.index = index;
    pcb.open = open;
    IswCallCallbacks((Widget)root, IswNpivotCallback, (IswPointer)&pcb);

    if (sel_changed)
        FireSelectCallback(root, lbw, group, index);
}

static void
DoToggle(ListBoxWidget lbw, Widget group)
{
    ListBoxConstraints lbc = (ListBoxConstraints)group->core.constraints;
    lbc->listBox.pivot_open = !lbc->listBox.pivot_open;
    ApplyPivotState(lbw, group);
}

/* ================================================================
 * Layout
 * ================================================================ */

static Dimension
ComputeTotalHeight(ListBoxWidget lbw)
{
    Dimension spacing = lbw->listBox.row_spacing;
    Dimension header_h = HeaderHeight(lbw);
    Dimension total = spacing;
    Boolean any = False;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;

        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);
        Dimension h;
        if (IsGroup(child)) {
            /* Body height is content-derived (recursive), never the
               child's core height: an empty group's core height is the
               100px constructor default and nothing ever corrects it. */
            h = header_h;
            if (lbc->listBox.pivot_open)
                h += ComputeTotalHeight((ListBoxWidget)child)
                     + _IswBorderVert(bs);
        } else {
            h = (lbc->listBox.row_height > 0)
                ? lbc->listBox.row_height : child->core.height;
            h += _IswBorderVert(bs);
        }

        if (any)
            total += spacing;
        total += h;
        any = True;
    }

    if (any)
        total += spacing;

    return total > 0 ? total : 1;
}

static void
DoLayout(ListBoxWidget lbw, Boolean position)
{
    Widget w = (Widget)lbw;
    Position y = (Position)lbw->listBox.row_spacing;
    Boolean first = True;
    Dimension header_h = HeaderHeight(lbw);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;

        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);

        if (!first)
            y += (Position)lbw->listBox.row_spacing;

        if (IsGroup(child)) {
            if (position)
                lbc->listBox.header_y = y;
            y += (Position)header_h;
            if (lbc->listBox.pivot_open) {
                Dimension body_h = ComputeTotalHeight((ListBoxWidget)child);
                Dimension child_w = (w->core.width > header_h)
                    ? w->core.width - header_h : 1;
                if (child_w > (Dimension)_IswBorderHoriz(bs))
                    child_w -= (Dimension)_IswBorderHoriz(bs);
                if (position)
                    IswConfigureWidget(child, (Position)header_h, y, child_w,
                                       body_h, child->core.border_width);
                y += (Position)(body_h + _IswBorderVert(bs));
            }
        } else {
            Dimension child_h = (lbc->listBox.row_height > 0)
                ? lbc->listBox.row_height : child->core.height;
            Dimension child_w = w->core.width;
            if (child_w > (Dimension)_IswBorderHoriz(bs))
                child_w -= (Dimension)_IswBorderHoriz(bs);

            if (position) {
                IswConfigureWidget(child, 0, y, child_w, child_h,
                                   child->core.border_width);
            }

            y += (Position)(child_h + _IswBorderVert(bs));
        }
        first = False;
    }

    if (!position) {
        Dimension want_h = ComputeTotalHeight(lbw);
        if (want_h != w->core.height) {
            Dimension got_w, got_h;
            IswGeometryResult r = IswMakeResizeRequest(w, w->core.width,
                                                       want_h, &got_w, &got_h);
            if (r == IswGeometryAlmost)
                IswMakeResizeRequest(w, got_w, got_h, NULL, NULL);
        }
    }
}

/* ================================================================
 * Type converter: String -> SelectionMode
 * ================================================================ */

static Boolean
CvtStringToSelectionMode(IswDisplay dpy, IswValuePtr args,
    Cardinal *num_args, IswValuePtr from, IswValuePtr to,
    IswPointer *converter_data)
{
    static IswListBoxSelectionMode mode;
    char lower[64];
    const char *str = (const char *)from->addr;

    (void)dpy; (void)args; (void)num_args; (void)converter_data;

    if (!str || strlen(str) >= sizeof(lower))
        return False;

    ISWCopyISOLatin1Lowered(lower, str);

    if (strcmp(lower, "none") == 0)
        mode = IswListBoxSelectNone;
    else if (strcmp(lower, "single") == 0)
        mode = IswListBoxSelectSingle;
    else if (strcmp(lower, "multi") == 0 || strcmp(lower, "multiple") == 0)
        mode = IswListBoxSelectMulti;
    else
        return False;

    if (to->addr == NULL) {
        to->addr = (IswPointer)&mode;
    } else if (to->size < sizeof(mode)) {
        to->size = sizeof(mode);
        return False;
    } else {
        *(IswListBoxSelectionMode *)to->addr = mode;
    }
    to->size = sizeof(mode);
    return True;
}

/* ================================================================
 * Core methods
 * ================================================================ */

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter(IswRString, IswRSelectionMode,
                        CvtStringToSelectionMode,
                        NULL, 0, IswCacheNone, NULL);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListBoxWidget lbw = (ListBoxWidget)new;
    (void)request; (void)args; (void)num_args;

    lbw->listBox.render_ctx = NULL;
    lbw->listBox.focused_child = NULL;
    lbw->listBox.last_click_time = 0;
    lbw->listBox.last_click_child = NULL;
    lbw->listBox.has_focus = False;

    if (new->core.width == 0)
        new->core.width = 200;
    if (new->core.height == 0)
        new->core.height = 100;
}

static void
Destroy(Widget w)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    if (lbw->listBox.render_ctx) {
        ISWRenderDestroy(lbw->listBox.render_ctx);
        lbw->listBox.render_ctx = NULL;
    }
}

static void
Resize(Widget w)
{
    DoLayout((ListBoxWidget)w, True);
}

static void
ChangeManaged(Widget w)
{
    ListBoxWidget lbw = (ListBoxWidget)w;

    /* A closed group's body must neither paint nor hit-test.  Unmap it
       before layout; ApplyPivotState re-maps it when opened.  Pre-realize
       the unmap must run even though windowless_mapped is still False:
       it records the explicit-unmap flag that stops the realize-time map
       pass from showing the body. */
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        if (IsGroup(child)) {
            ListBoxConstraints lbc =
                (ListBoxConstraints)child->core.constraints;
            if (!lbc->listBox.pivot_open &&
                (child->core.windowless_mapped || !IswIsRealized(child)))
                IswUnmapWidget(child);
        }
    }

    DoLayout(lbw, False);
    DoLayout(lbw, True);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        /* Group children run their own ListBox event handling. */
        if (IswIsComposite(child) && !IsGroup(child))
            InstallChildHandlers(child, w);
    }
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request,
                IswWidgetGeometry *reply)
{
    ListBoxWidget lbw = (ListBoxWidget)IswParent(child);
    (void)reply;

    if ((request->request_mode & IswCWX) ||
        (request->request_mode & IswCWY))
        return IswGeometryNo;

    if (request->request_mode & IswCWWidth)
        request->width = child->core.width;

    if (request->request_mode & IswCWHeight)
        child->core.height = request->height;

    DoLayout(lbw, False);
    DoLayout(lbw, True);

    return IswGeometryYes;
}

static IswGeometryResult
PreferredGeometry(Widget w, IswWidgetGeometry *request,
                  IswWidgetGeometry *reply)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    Dimension pref_h = ComputeTotalHeight(lbw);

    reply->width = w->core.width;
    reply->height = pref_h;
    reply->request_mode = IswCWWidth | IswCWHeight;

    if ((request->request_mode &
         (IswCWWidth | IswCWHeight)) ==
        (IswCWWidth | IswCWHeight) &&
        request->width == reply->width && request->height == reply->height)
        return IswGeometryYes;

    if (reply->width == w->core.width && reply->height == w->core.height)
        return IswGeometryNo;

    return IswGeometryAlmost;
}

static Boolean
ForegroundHex(ListBoxWidget lbw, char *hex, size_t hex_size)
{
    IswDisplay dpy = ((Widget)lbw)->core.display;
    IswColormap cmap = ((Widget)lbw)->core.colormap;
    IswColor c;

    hex[0] = '\0';
    if (_IswPlatformQueryColor(dpy, cmap,
                               (unsigned long)lbw->listBox.foreground, &c)) {
        snprintf(hex, hex_size, "#%02X%02X%02X",
                 c.red >> 8, c.green >> 8, c.blue >> 8);
    }
    return hex[0] != '\0';
}

/* Lazily load the chevron image for the group's current state.  Cached
   per constraint; invalidated when the source resource changes. */
static ISWImage *
GroupIcon(ListBoxWidget lbw, ListBoxConstraints lbc)
{
    Boolean open = lbc->listBox.pivot_open;
    ISWImage **slot = open ? &lbc->listBox.icon_open
                           : &lbc->listBox.icon_closed;
    if (*slot == NULL) {
        const char *src = open
            ? (lbc->listBox.pivot_image_open
                ? lbc->listBox.pivot_image_open : pivot_open_svg)
            : (lbc->listBox.pivot_image
                ? lbc->listBox.pivot_image : pivot_closed_svg);
        double dpi = 96.0 * ISWScaleFactor((Widget)lbw);
        char hex[8];
        const char *color = ForegroundHex(lbw, hex, sizeof(hex)) ? hex : NULL;
        *slot = ISWImageLoad(src, dpi, color);
    }
    return *slot;
}

static void
DrawGroupChevron(ListBoxWidget lbw, ListBoxConstraints lbc,
                 Position hy, Dimension header_h, Pixel fg)
{
    ISWRenderContext *ctx = lbw->listBox.render_ctx;
    ISWImage *img = GroupIcon(lbw, lbc);
    if (!img) return;

    int icon_sz = ((int)header_h > 2 * GROUP_ICON_PAD)
        ? (int)header_h - 2 * GROUP_ICON_PAD : (int)header_h;
    float sf = (float)ISWScaleFactor((Widget)lbw);
    unsigned int raster_sz = (unsigned int)(icon_sz * sf + 0.5f);
    unsigned int rw, rh;
    const unsigned char *pixels = ISWImageRasterize(img, raster_sz, raster_sz,
                                                    &rw, &rh);
    if (!pixels) return;

    Boolean mono = ISWImageIsMonochrome(img);
    Pixel mfg = mono ? fg : 0;

    /* Retained handle: upload (tinted for monochrome) once, redraw by
       handle; re-upload only when the raster, its dims, or the tint
       foreground change. */
    if (lbc->listBox.icon_handle_raster != pixels ||
        lbc->listBox.icon_handle_w != rw ||
        lbc->listBox.icon_handle_h != rh ||
        lbc->listBox.icon_handle_fg != mfg ||
        lbc->listBox.icon_handle == 0) {
        if (lbc->listBox.icon_handle)
            ISWRenderImageFree(ctx, lbc->listBox.icon_handle);
        lbc->listBox.icon_handle = mono
            ? ISWRenderImageUploadMasked(ctx, mfg, pixels, rw, rh)
            : ISWRenderImageUpload(ctx, pixels, rw, rh);
        lbc->listBox.icon_handle_raster = pixels;
        lbc->listBox.icon_handle_w = rw;
        lbc->listBox.icon_handle_h = rh;
        lbc->listBox.icon_handle_fg = mfg;
    }

    int ix = GROUP_ICON_PAD;
    int iy = (int)hy + ((int)header_h - icon_sz) / 2;
    if (lbc->listBox.icon_handle)
        ISWRenderDrawImageHandle(ctx, lbc->listBox.icon_handle,
                                 ix, iy, (unsigned)icon_sz,
                                 (unsigned)icon_sz);
    else if (mono)
        ISWRenderDrawImageMasked(ctx, fg, pixels, rw, rh,
                                 ix, iy, (unsigned)icon_sz,
                                 (unsigned)icon_sz);
    else
        ISWRenderDrawImageRGBA(ctx, pixels, rw, rh,
                               ix, iy, (unsigned)icon_sz,
                               (unsigned)icon_sz);
}

/* Draw a group's header band (selection fill, chevron, label text).
   Called between ISWRenderBegin/End on lbw's render context. */
static void
DrawGroupHeader(ListBoxWidget lbw, Widget child, Dimension header_h)
{
    ISWRenderContext *ctx = lbw->listBox.render_ctx;
    Widget w = (Widget)lbw;
    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    Position hy = lbc->listBox.header_y;
    Pixel fg = lbc->listBox.selected ? w->core.background_pixel
                                     : lbw->listBox.foreground;

    if (lbc->listBox.selected) {
        ISWRenderSetColor(ctx, lbw->listBox.foreground);
        ISWRenderFillRectangle(ctx, 0, hy, w->core.width, header_h);
    }

    DrawGroupChevron(lbw, lbc, hy, header_h, fg);

    if (lbc->listBox.pivot_label && lbw->listBox.font) {
        int len = (int)strlen(lbc->listBox.pivot_label);
        if (len > 0) {
            int cap = ISWScaledFontCapHeight(w, lbw->listBox.font);
            int baseline = (int)hy + ((int)header_h + cap) / 2;
            ISWRenderSetFont(ctx, lbw->listBox.font);
            ISWRenderSetColor(ctx, fg);
            ISWRenderDrawString(ctx, lbc->listBox.pivot_label, len,
                                (int)header_h + GROUP_LABEL_GAP, baseline);
        }
    }
}

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)event; (void)region;

    if (!IswIsRealized(w))
        return;

    if (!lbw->listBox.render_ctx)
        lbw->listBox.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    ISWRenderContext *ctx = lbw->listBox.render_ctx;
    if (!ctx) return;

    ListBoxWidget root = SelectionRoot(lbw);
    Dimension header_h = HeaderHeight(lbw);

    ISWRenderBegin(ctx);

    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);
        int entry_y, entry_h;

        if (IsGroup(child)) {
            entry_y = lbc->listBox.header_y;
            entry_h = (int)header_h;
            if (lbc->listBox.pivot_open)
                entry_h += (int)child->core.height + _IswBorderVert(bs);
            DrawGroupHeader(lbw, child, header_h);
        } else {
            entry_y = child->core.y;
            entry_h = (int)child->core.height + _IswBorderVert(bs);
            if (lbc->listBox.selected) {
                ISWRenderSetColor(ctx, lbw->listBox.foreground);
                ISWRenderFillRectangle(ctx, 0, entry_y,
                                       w->core.width, (unsigned)entry_h);
            }
        }

        if (lbc->listBox.separator && lbw->listBox.show_separators) {
            int sep_y = entry_y + entry_h +
                        (int)lbw->listBox.row_spacing / 2;
            ISWRenderSetColor(ctx, lbw->listBox.foreground);
            ISWRenderSetLineWidth(ctx, 1.0);
            ISWRenderDrawLine(ctx, 0, sep_y, (int)w->core.width, sep_y);
        }
    }

    Widget focused = root->listBox.focused_child;
    if (root->listBox.has_focus && focused != NULL &&
        IswParent(focused) == w) {
        ISWRenderSetColor(ctx, lbw->listBox.foreground);
        ISWRenderSetLineWidth(ctx, 1.0);
        if (IsGroup(focused)) {
            ListBoxConstraints flbc =
                (ListBoxConstraints)focused->core.constraints;
            ISWRenderStrokeRectangle(ctx, 1, flbc->listBox.header_y,
                                     (int)w->core.width - 2, (int)header_h);
        } else {
            IswBorderSides fbs = _IswGetBorderSides(focused);
            ISWRenderStrokeRectangle(ctx,
                focused->core.x - 1, focused->core.y - 1,
                (int)focused->core.width + _IswBorderHoriz(fbs) + 2,
                (int)focused->core.height + _IswBorderVert(fbs) + 2);
        }
    }

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList args, Cardinal *num_args)
{
    ListBoxWidget cur = (ListBoxWidget)current;
    ListBoxWidget lbw = (ListBoxWidget)new;
    Boolean redisplay = False;
    (void)request; (void)args; (void)num_args;

    if (cur->listBox.row_spacing != lbw->listBox.row_spacing) {
        DoLayout(lbw, False);
        DoLayout(lbw, True);
        redisplay = True;
    }

    if (cur->listBox.show_separators != lbw->listBox.show_separators)
        redisplay = True;

    if (cur->listBox.font != lbw->listBox.font) {
        /* Header band height is font-derived */
        DoLayout(lbw, False);
        DoLayout(lbw, True);
        redisplay = True;
    }

    if (cur->listBox.foreground != lbw->listBox.foreground) {
        for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
            Widget child = lbw->composite.children[i];
            if (!IswIsManaged(child)) continue;
            ListBoxConstraints lbc =
                (ListBoxConstraints)child->core.constraints;
            if (lbc->listBox.selected) {
                Pixel fg = FindLabelFg(child);
                Pixel bg = child->core.background_pixel;
                if (fg == (Pixel)-1) fg = ~bg;
                SwapFgBg(child, fg, bg);
                SwapFgBg(child, fg, bg);
                ExposeTree(child);
            }
        }
        redisplay = True;
    }

    if (cur->listBox.selection_mode != lbw->listBox.selection_mode) {
        if (lbw->listBox.selection_mode == IswListBoxSelectNone)
            ClearAllSelections(lbw);
    }

    return redisplay;
}

/* ================================================================
 * Constraint methods
 * ================================================================ */

static void
ConstraintInitialize(Widget request, Widget new,
                     ArgList args, Cardinal *num_args)
{
    ListBoxConstraints lbc = (ListBoxConstraints)new->core.constraints;
    (void)request; (void)args; (void)num_args;

    lbc->listBox.selected = False;

    lbc->listBox.pivot_label = lbc->listBox.pivot_label
        ? IswNewString(lbc->listBox.pivot_label) : NULL;
    lbc->listBox.pivot_image = lbc->listBox.pivot_image
        ? IswNewString(lbc->listBox.pivot_image) : NULL;
    lbc->listBox.pivot_image_open = lbc->listBox.pivot_image_open
        ? IswNewString(lbc->listBox.pivot_image_open) : NULL;

    lbc->listBox.header_y = 0;
    lbc->listBox.icon_closed = NULL;
    lbc->listBox.icon_open = NULL;
    lbc->listBox.icon_handle = 0;
    lbc->listBox.icon_handle_raster = NULL;
    lbc->listBox.icon_handle_w = 0;
    lbc->listBox.icon_handle_h = 0;
    lbc->listBox.icon_handle_fg = 0;
}

static void
ConstraintDestroy(Widget w)
{
    ListBoxConstraints lbc = (ListBoxConstraints)w->core.constraints;
    ListBoxWidget lbw = (ListBoxWidget)IswParent(w);
    ListBoxWidget root = SelectionRoot(lbw);

    if (root->listBox.focused_child == w)
        root->listBox.focused_child = NULL;
    if (root->listBox.last_click_child == w)
        root->listBox.last_click_child = NULL;

    if (lbc->listBox.icon_handle && lbw->listBox.render_ctx)
        ISWRenderImageFree(lbw->listBox.render_ctx, lbc->listBox.icon_handle);
    ISWImageDestroy(lbc->listBox.icon_closed);
    ISWImageDestroy(lbc->listBox.icon_open);
    IswFree(lbc->listBox.pivot_label);
    IswFree(lbc->listBox.pivot_image);
    IswFree(lbc->listBox.pivot_image_open);
}

static Boolean
ConstraintSetValues(Widget current, Widget request, Widget new,
                    ArgList args, Cardinal *num_args)
{
    ListBoxConstraints clbc = (ListBoxConstraints)current->core.constraints;
    ListBoxConstraints nlbc = (ListBoxConstraints)new->core.constraints;
    ListBoxWidget lbw = (ListBoxWidget)IswParent(new);
    (void)request; (void)args; (void)num_args;

    if (clbc->listBox.row_height != nlbc->listBox.row_height) {
        DoLayout(lbw, False);
        DoLayout(lbw, True);
    }

    if (nlbc->listBox.pivot_label != clbc->listBox.pivot_label) {
        IswFree(clbc->listBox.pivot_label);
        nlbc->listBox.pivot_label = nlbc->listBox.pivot_label
            ? IswNewString(nlbc->listBox.pivot_label) : NULL;
        if (IswIsRealized((Widget)lbw))
            _IswRepaintWindowless((Widget)lbw);
    }

    if (nlbc->listBox.pivot_image != clbc->listBox.pivot_image) {
        IswFree(clbc->listBox.pivot_image);
        nlbc->listBox.pivot_image = nlbc->listBox.pivot_image
            ? IswNewString(nlbc->listBox.pivot_image) : NULL;
        ISWImageDestroy(nlbc->listBox.icon_closed);
        nlbc->listBox.icon_closed = NULL;
        if (IswIsRealized((Widget)lbw))
            _IswRepaintWindowless((Widget)lbw);
    }

    if (nlbc->listBox.pivot_image_open != clbc->listBox.pivot_image_open) {
        IswFree(clbc->listBox.pivot_image_open);
        nlbc->listBox.pivot_image_open = nlbc->listBox.pivot_image_open
            ? IswNewString(nlbc->listBox.pivot_image_open) : NULL;
        ISWImageDestroy(nlbc->listBox.icon_open);
        nlbc->listBox.icon_open = NULL;
        if (IswIsRealized((Widget)lbw))
            _IswRepaintWindowless((Widget)lbw);
    }

    if (nlbc->listBox.pivot_open != clbc->listBox.pivot_open &&
        IsGroup(new))
        ApplyPivotState(lbw, new);

    return False;
}

/* ================================================================
 * Action handlers
 * ================================================================ */

static void
SelectAction(Widget w, IswEvent *iswev,
             String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    ListBoxWidget root = SelectionRoot(lbw);
    (void)params; (void)num_params;

    Widget child;
    HitZone zone = HitTestEx(lbw, IswEventX(iswev), IswEventY(iswev), &child);
    if (zone == HitNone) return;

    if (zone == HitChevron) {
        DoToggle(lbw, child);
        return;
    }

    if (root->listBox.selection_mode == IswListBoxSelectNone)
        return;

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    int idx = IndexOfManagedChild(lbw, child);

    /* Double-click detection */
    IswTime now = iswev->button.time;
    if (child == root->listBox.last_click_child &&
        (now - root->listBox.last_click_time) < DOUBLE_CLICK_MS) {
        FireActivateCallback(root, lbw, child, idx);
        root->listBox.last_click_time = 0;
        return;
    }
    root->listBox.last_click_time = now;
    root->listBox.last_click_child = child;

    if (root->listBox.selection_mode == IswListBoxSelectSingle) {
        ClearAllSelections(root);
        ApplySelectionVisual(lbw, child, True);
    } else {
        /* Multi mode: Ctrl toggles, plain click selects only this one */
        if (IswEventModifiers(iswev) & IswModControl) {
            ApplySelectionVisual(lbw, child, !lbc->listBox.selected);
        } else if (IswEventModifiers(iswev) & IswModShift) {
            /* Extend from the focused entry to the clicked one; ranges
               only have meaning within one immediate ListBox, so a
               cross-boundary anchor degrades to a plain select. */
            Widget fc = root->listBox.focused_child;
            int anchor = (fc != NULL && IswParent(fc) == w)
                ? IndexOfManagedChild(lbw, fc) : -1;
            ClearAllSelections(root);
            if (anchor >= 0) {
                int lo = anchor < idx ? anchor : idx;
                int hi = anchor > idx ? anchor : idx;
                int n = 0;
                for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
                    Widget rc = lbw->composite.children[i];
                    if (!IswIsManaged(rc)) continue;
                    if (n >= lo && n <= hi) {
                        ListBoxConstraints rlbc =
                            (ListBoxConstraints)rc->core.constraints;
                        if (rlbc->listBox.selectable)
                            ApplySelectionVisual(lbw, rc, True);
                    }
                    n++;
                }
            } else {
                ApplySelectionVisual(lbw, child, True);
            }
        } else {
            ClearAllSelections(root);
            ApplySelectionVisual(lbw, child, True);
        }
    }

    root->listBox.focused_child = child;

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }

    FireSelectCallback(root, lbw, child, idx);
}

static void
MoveFocusAction(Widget w, IswEvent *iswev,
                String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev;

    if (!num_params || *num_params < 1) return;

    int count;
    Widget *flat = FlattenEntries(root, &count);
    if (count == 0) {
        if (flat) IswFree((char *)flat);
        return;
    }

    int cur = -1;
    for (int i = 0; i < count; i++)
        if (flat[i] == root->listBox.focused_child) { cur = i; break; }

    int idx = cur;
    if (strcmp(params[0], "up") == 0) {
        idx = (cur > 0) ? cur - 1 : 0;
    } else if (strcmp(params[0], "down") == 0) {
        idx = (cur < count - 1) ? cur + 1 : count - 1;
    } else if (strcmp(params[0], "home") == 0) {
        idx = 0;
    } else if (strcmp(params[0], "end") == 0) {
        idx = count - 1;
    }
    if (idx < 0) idx = 0;

    Widget entry = flat[idx];
    IswFree((char *)flat);

    if (entry == root->listBox.focused_child) return;

    root->listBox.focused_child = entry;

    if (root->listBox.selection_mode == IswListBoxSelectSingle) {
        ListBoxConstraints lbc = (ListBoxConstraints)entry->core.constraints;
        if (lbc->listBox.selectable) {
            ListBoxWidget list = (ListBoxWidget)IswParent(entry);
            ClearAllSelections(root);
            ApplySelectionVisual(list, entry, True);
            FireSelectCallback(root, list, entry,
                               IndexOfManagedChild(list, entry));
        }
    }

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }
}

static void
ToggleAction(Widget w, IswEvent *iswev,
             String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev; (void)params; (void)num_params;

    if (root->listBox.selection_mode == IswListBoxSelectNone) return;

    Widget child = root->listBox.focused_child;
    if (!child) return;

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    ListBoxWidget list = (ListBoxWidget)IswParent(child);
    if (root->listBox.selection_mode == IswListBoxSelectSingle) {
        ClearAllSelections(root);
        ApplySelectionVisual(list, child, True);
    } else {
        ApplySelectionVisual(list, child, !lbc->listBox.selected);
    }

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }

    FireSelectCallback(root, list, child, IndexOfManagedChild(list, child));
}

static void
ActivateAction(Widget w, IswEvent *iswev,
               String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev; (void)params; (void)num_params;

    Widget child = root->listBox.focused_child;
    if (!child) return;

    ListBoxWidget list = (ListBoxWidget)IswParent(child);
    FireActivateCallback(root, list, child, IndexOfManagedChild(list, child));
}

static void
ExtendAction(Widget w, IswEvent *iswev,
             String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev;

    if (root->listBox.selection_mode != IswListBoxSelectMulti) return;
    if (!num_params || *num_params < 1) return;

    int count;
    Widget *flat = FlattenEntries(root, &count);
    if (count == 0) {
        if (flat) IswFree((char *)flat);
        return;
    }

    int cur = -1;
    for (int i = 0; i < count; i++)
        if (flat[i] == root->listBox.focused_child) { cur = i; break; }

    int idx = cur;
    if (strcmp(params[0], "up") == 0) {
        idx = (cur > 0) ? cur - 1 : 0;
    } else if (strcmp(params[0], "down") == 0) {
        idx = (cur < count - 1) ? cur + 1 : count - 1;
    }
    if (idx < 0) idx = 0;

    Widget entry = flat[idx];
    IswFree((char *)flat);

    if (entry == root->listBox.focused_child) return;

    Widget prev = root->listBox.focused_child;
    ListBoxWidget list = (ListBoxWidget)IswParent(entry);
    ListBoxConstraints lbc = (ListBoxConstraints)entry->core.constraints;
    if (lbc->listBox.selectable) {
        /* Extending across a group boundary has no range meaning:
           degrade to a plain single select. */
        if (prev == NULL || IswParent(entry) != IswParent(prev))
            ClearAllSelections(root);
        ApplySelectionVisual(list, entry, True);
    }

    root->listBox.focused_child = entry;

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }

    FireSelectCallback(root, list, entry, IndexOfManagedChild(list, entry));
}

static void
SelectAllIn(ListBoxWidget lbw)
{
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selectable)
            ApplySelectionVisual(lbw, child, True);
        /* Only visible entries: closed groups stay untouched inside. */
        if (IsGroup(child) && lbc->listBox.pivot_open)
            SelectAllIn((ListBoxWidget)child);
    }
}

static void
SelectAllAction(Widget w, IswEvent *iswev,
                String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev; (void)params; (void)num_params;

    if (root->listBox.selection_mode != IswListBoxSelectMulti) return;

    SelectAllIn(root);

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }
}

static void
FocusAction(Widget w, IswEvent *iswev,
            String *params, Cardinal *num_params)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    (void)iswev;

    if (!num_params || *num_params < 1) return;

    Boolean in = (strcmp(params[0], "in") == 0);
    root->listBox.has_focus = in;

    if (in && root->listBox.focused_child == NULL) {
        int count;
        Widget *flat = FlattenEntries(root, &count);
        if (count > 0)
            root->listBox.focused_child = flat[0];
        if (flat) IswFree((char *)flat);
    }

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

static Widget
FirstSelectedIn(ListBoxWidget lbw)
{
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) return child;
        if (IsGroup(child)) {
            Widget found = FirstSelectedIn((ListBoxWidget)child);
            if (found) return found;
        }
    }
    return NULL;
}

Widget
IswListBoxGetSelected(Widget w)
{
    return FirstSelectedIn(SelectionRoot((ListBoxWidget)w));
}

int
IswListBoxGetSelectedChildren(Widget w, Widget **children_out)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    int count = CountSelectedIn(root);

    if (count == 0) {
        *children_out = NULL;
        return 0;
    }

    *children_out = (Widget *)IswMalloc((Cardinal)(count * (int)sizeof(Widget)));
    int j = 0;
    GatherSelectedIn(root, *children_out, &j);

    return count;
}

void
IswListBoxClearSelection(Widget w)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);
    ClearAllSelections(root);

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }
}

void
IswListBoxSelectChild(Widget w, Widget child)
{
    ListBoxWidget root = SelectionRoot((ListBoxWidget)w);

    if (root->listBox.selection_mode == IswListBoxSelectNone) return;

    Widget pw = IswParent(child);
    if (pw == NULL || !IswIsSubclass(pw, listBoxWidgetClass)) return;
    ListBoxWidget list = (ListBoxWidget)pw;

    /* Never create a hidden selection: refuse if any group between the
       entry and the root is closed. */
    for (Widget a = pw; a != (Widget)root; a = IswParent(a)) {
        ListBoxConstraints albc = (ListBoxConstraints)a->core.constraints;
        if (!albc->listBox.pivot_open) return;
    }

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    if (root->listBox.selection_mode == IswListBoxSelectSingle)
        ClearAllSelections(root);

    ApplySelectionVisual(list, child, True);

    if (IswIsRealized((Widget)root)) {
        _IswRepaintWindowless((Widget)root);
    }
}
