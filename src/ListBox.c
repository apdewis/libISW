/*
 * ListBox.c - Constraint widget that lays out children as selectable rows
 *
 * Children are managed with IswManageChild.  The container stacks them
 * vertically and tracks selection state.  The child's X window background
 * is changed to reflect selection; children remain completely unaware.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOUBLE_CLICK_MS 400

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
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

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
    {IswNselectCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(select_callback), IswRCallback, NULL},
    {IswNactivateCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(activate_callback), IswRCallback, NULL},
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
};
#undef COffset

/* ================================================================
 * Translations and actions
 * ================================================================ */

static char defaultTranslations[] =
    "<Btn1Down>:         ListBoxSelect()\n"
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
    NULL,                                /* constraint destroy */
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

static int
ManagedCount(ListBoxWidget lbw)
{
    int count = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++)
        if (IswIsManaged(lbw->composite.children[i]))
            count++;
    return count;
}

static Widget
ManagedChild(ListBoxWidget lbw, int index)
{
    int n = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        if (n == index) return child;
        n++;
    }
    return NULL;
}

static int
HitTest(ListBoxWidget lbw, int y)
{
    int n = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        int cy = child->core.y;
        IswBorderSides bs = _IswGetBorderSides(child);
        int ch = (int)child->core.height + _IswBorderVert(bs);
        if (y >= cy && y < cy + ch)
            return n;
        n++;
    }
    return -1;
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

    if (IswEventButton(iswev) != IswButtonLeft)
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
        for (Cardinal i = 0; i < cw->composite.num_children; i++)
            InstallChildHandlers(cw->composite.children[i], listbox);
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
        for (Cardinal i = 0; i < cw->composite.num_children; i++)
            SwapFgBg(cw->composite.children[i], fg, bg);
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

    Pixel fg = FindLabelFg(child);
    Pixel bg = child->core.background_pixel;
    if (fg == (Pixel)-1) fg = ~bg;

    SwapFgBg(child, fg, bg);
    ExposeTree(child);
}

static void
ClearAllSelections(ListBoxWidget lbw)
{
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ApplySelectionVisual(lbw, child, False);
    }
}

static void
FireSelectCallback(ListBoxWidget lbw, Widget child, int index)
{
    IswListBoxCallbackData cb;
    Widget *sel = NULL;
    int nsel = 0;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget c = lbw->composite.children[i];
        if (!IswIsManaged(c)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)c->core.constraints;
        if (lbc->listBox.selected) nsel++;
    }

    if (nsel > 0) {
        sel = (Widget *)IswMalloc((Cardinal)(nsel * (int)sizeof(Widget)));
        int j = 0;
        for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
            Widget c = lbw->composite.children[i];
            if (!IswIsManaged(c)) continue;
            ListBoxConstraints lbc = (ListBoxConstraints)c->core.constraints;
            if (lbc->listBox.selected) sel[j++] = c;
        }
    }

    cb.child = child;
    cb.index = index;
    cb.selected = sel;
    cb.num_selected = nsel;

    IswCallCallbacks((Widget)lbw, IswNselectCallback, (IswPointer)&cb);

    if (sel) IswFree((char *)sel);
}

static void
FireActivateCallback(ListBoxWidget lbw, Widget child, int index)
{
    IswListBoxCallbackData cb;
    cb.child = child;
    cb.index = index;
    cb.selected = NULL;
    cb.num_selected = 0;
    IswCallCallbacks((Widget)lbw, IswNactivateCallback, (IswPointer)&cb);
}

/* ================================================================
 * Layout
 * ================================================================ */

static Dimension
ComputeTotalHeight(ListBoxWidget lbw)
{
    Dimension spacing = lbw->listBox.row_spacing;
    Dimension total = spacing;
    Boolean any = False;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;

        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        Dimension h = (lbc->listBox.row_height > 0)
            ? lbc->listBox.row_height : child->core.height;

        if (any)
            total += spacing;
        {
            IswBorderSides bs = _IswGetBorderSides(child);
            total += h + _IswBorderVert(bs);
        }
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

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;

        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        Dimension child_h = (lbc->listBox.row_height > 0)
            ? lbc->listBox.row_height : child->core.height;

        IswBorderSides bs = _IswGetBorderSides(child);
        Dimension child_w = w->core.width;
        if (child_w > (Dimension)_IswBorderHoriz(bs))
            child_w -= (Dimension)_IswBorderHoriz(bs);

        if (!first)
            y += (Position)lbw->listBox.row_spacing;

        if (position) {
            IswConfigureWidget(child, 0, y, child_w, child_h,
                               child->core.border_width);
        }

        y += (Position)(child_h + _IswBorderVert(bs));
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
    lbw->listBox.focused_index = -1;
    lbw->listBox.last_click_time = 0;
    lbw->listBox.last_click_index = -1;
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
    DoLayout(lbw, False);
    DoLayout(lbw, True);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        if (IswIsComposite(child))
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

    ISWRenderBegin(ctx);

    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);
        int row_h = (int)child->core.height + _IswBorderVert(bs);

        if (lbc->listBox.selected) {
            ISWRenderSetColor(ctx, lbw->listBox.foreground);
            ISWRenderFillRectangle(ctx, 0, child->core.y,
                                   w->core.width, (unsigned)row_h);
        }

        if (lbc->listBox.separator && lbw->listBox.show_separators) {
            int sep_y = child->core.y + row_h +
                        (int)lbw->listBox.row_spacing / 2;
            ISWRenderSetColor(ctx, lbw->listBox.foreground);
            ISWRenderSetLineWidth(ctx, 1.0);
            ISWRenderDrawLine(ctx, 0, sep_y, (int)w->core.width, sep_y);
        }
    }

    if (lbw->listBox.has_focus && lbw->listBox.focused_index >= 0) {
        Widget focused = ManagedChild(lbw, lbw->listBox.focused_index);
        if (focused) {
            ISWRenderSetColor(ctx, lbw->listBox.foreground);
            ISWRenderSetLineWidth(ctx, 1.0);
            {
                IswBorderSides fbs = _IswGetBorderSides(focused);
                ISWRenderStrokeRectangle(ctx,
                    focused->core.x - 1, focused->core.y - 1,
                    (int)focused->core.width + _IswBorderHoriz(fbs) + 2,
                    (int)focused->core.height + _IswBorderVert(fbs) + 2);
            }
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
}

static Boolean
ConstraintSetValues(Widget current, Widget request, Widget new,
                    ArgList args, Cardinal *num_args)
{
    ListBoxConstraints clbc = (ListBoxConstraints)current->core.constraints;
    ListBoxConstraints nlbc = (ListBoxConstraints)new->core.constraints;
    (void)request; (void)args; (void)num_args;

    if (clbc->listBox.row_height != nlbc->listBox.row_height) {
        ListBoxWidget lbw = (ListBoxWidget)IswParent(new);
        DoLayout(lbw, False);
        DoLayout(lbw, True);
    }

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
    (void)params; (void)num_params;

    if (lbw->listBox.selection_mode == IswListBoxSelectNone)
        return;

    int idx = HitTest(lbw, IswEventY(iswev));
    if (idx < 0) return;

    Widget child = ManagedChild(lbw, idx);
    if (!child) return;

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    /* Double-click detection */
    IswTime now = iswev->button.time;
    if (idx == lbw->listBox.last_click_index &&
        (now - lbw->listBox.last_click_time) < DOUBLE_CLICK_MS) {
        FireActivateCallback(lbw, child, idx);
        lbw->listBox.last_click_time = 0;
        return;
    }
    lbw->listBox.last_click_time = now;
    lbw->listBox.last_click_index = idx;

    if (lbw->listBox.selection_mode == IswListBoxSelectSingle) {
        ClearAllSelections(lbw);
        ApplySelectionVisual(lbw, child, True);
    } else {
        /* Multi mode: Ctrl toggles, plain click selects only this one */
        if (IswEventModifiers(iswev) & IswModControl) {
            ApplySelectionVisual(lbw, child, !lbc->listBox.selected);
        } else if (IswEventModifiers(iswev) & IswModShift) {
            /* Extend from focused_index to idx */
            int anchor = lbw->listBox.focused_index >= 0
                ? lbw->listBox.focused_index : 0;
            int lo = anchor < idx ? anchor : idx;
            int hi = anchor > idx ? anchor : idx;
            ClearAllSelections(lbw);
            for (int r = lo; r <= hi; r++) {
                Widget rc = ManagedChild(lbw, r);
                if (!rc) continue;
                ListBoxConstraints rlbc =
                    (ListBoxConstraints)rc->core.constraints;
                if (rlbc->listBox.selectable)
                    ApplySelectionVisual(lbw, rc, True);
            }
        } else {
            ClearAllSelections(lbw);
            ApplySelectionVisual(lbw, child, True);
        }
    }

    lbw->listBox.focused_index = idx;

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }

    FireSelectCallback(lbw, child, idx);
}

static void
MoveFocusAction(Widget w, IswEvent *iswev,
                String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev;

    if (!num_params || *num_params < 1) return;

    int count = ManagedCount(lbw);
    if (count == 0) return;

    int idx = lbw->listBox.focused_index;

    if (strcmp(params[0], "up") == 0) {
        idx = (idx > 0) ? idx - 1 : 0;
    } else if (strcmp(params[0], "down") == 0) {
        idx = (idx < count - 1) ? idx + 1 : count - 1;
    } else if (strcmp(params[0], "home") == 0) {
        idx = 0;
    } else if (strcmp(params[0], "end") == 0) {
        idx = count - 1;
    }

    if (idx == lbw->listBox.focused_index) return;

    lbw->listBox.focused_index = idx;

    if (lbw->listBox.selection_mode == IswListBoxSelectSingle) {
        Widget child = ManagedChild(lbw, idx);
        if (child) {
            ListBoxConstraints lbc =
                (ListBoxConstraints)child->core.constraints;
            if (lbc->listBox.selectable) {
                ClearAllSelections(lbw);
                ApplySelectionVisual(lbw, child, True);
                FireSelectCallback(lbw, child, idx);
            }
        }
    }

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }
}

static void
ToggleAction(Widget w, IswEvent *iswev,
             String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev; (void)params; (void)num_params;

    if (lbw->listBox.selection_mode == IswListBoxSelectNone) return;
    if (lbw->listBox.focused_index < 0) return;

    Widget child = ManagedChild(lbw, lbw->listBox.focused_index);
    if (!child) return;

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    if (lbw->listBox.selection_mode == IswListBoxSelectSingle) {
        ClearAllSelections(lbw);
        ApplySelectionVisual(lbw, child, True);
    } else {
        ApplySelectionVisual(lbw, child, !lbc->listBox.selected);
    }

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }

    FireSelectCallback(lbw, child, lbw->listBox.focused_index);
}

static void
ActivateAction(Widget w, IswEvent *iswev,
               String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev; (void)params; (void)num_params;

    if (lbw->listBox.focused_index < 0) return;

    Widget child = ManagedChild(lbw, lbw->listBox.focused_index);
    if (!child) return;

    FireActivateCallback(lbw, child, lbw->listBox.focused_index);
}

static void
ExtendAction(Widget w, IswEvent *iswev,
             String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev;

    if (lbw->listBox.selection_mode != IswListBoxSelectMulti) return;
    if (!num_params || *num_params < 1) return;

    int count = ManagedCount(lbw);
    if (count == 0) return;

    int idx = lbw->listBox.focused_index;
    if (strcmp(params[0], "up") == 0) {
        idx = (idx > 0) ? idx - 1 : 0;
    } else if (strcmp(params[0], "down") == 0) {
        idx = (idx < count - 1) ? idx + 1 : count - 1;
    }

    if (idx == lbw->listBox.focused_index) return;

    Widget child = ManagedChild(lbw, idx);
    if (child) {
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selectable)
            ApplySelectionVisual(lbw, child, True);
    }

    lbw->listBox.focused_index = idx;

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }

    if (child)
        FireSelectCallback(lbw, child, idx);
}

static void
SelectAllAction(Widget w, IswEvent *iswev,
                String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev; (void)params; (void)num_params;

    if (lbw->listBox.selection_mode != IswListBoxSelectMulti) return;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selectable)
            ApplySelectionVisual(lbw, child, True);
    }

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }
}

static void
FocusAction(Widget w, IswEvent *iswev,
            String *params, Cardinal *num_params)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    (void)iswev;

    if (!num_params || *num_params < 1) return;

    Boolean in = (strcmp(params[0], "in") == 0);
    lbw->listBox.has_focus = in;

    if (in && lbw->listBox.focused_index < 0 && ManagedCount(lbw) > 0)
        lbw->listBox.focused_index = 0;

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

Widget
IswListBoxGetSelected(Widget w)
{
    ListBoxWidget lbw = (ListBoxWidget)w;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) return child;
    }
    return NULL;
}

int
IswListBoxGetSelectedChildren(Widget w, Widget **children_out)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    int count = 0;

    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) count++;
    }

    if (count == 0) {
        *children_out = NULL;
        return 0;
    }

    *children_out = (Widget *)IswMalloc((Cardinal)(count * (int)sizeof(Widget)));
    int j = 0;
    for (Cardinal i = 0; i < lbw->composite.num_children; i++) {
        Widget child = lbw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
        if (lbc->listBox.selected) (*children_out)[j++] = child;
    }

    return count;
}

void
IswListBoxClearSelection(Widget w)
{
    ListBoxWidget lbw = (ListBoxWidget)w;
    ClearAllSelections(lbw);

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }
}

void
IswListBoxSelectChild(Widget w, Widget child)
{
    ListBoxWidget lbw = (ListBoxWidget)w;

    if (lbw->listBox.selection_mode == IswListBoxSelectNone) return;

    ListBoxConstraints lbc = (ListBoxConstraints)child->core.constraints;
    if (!lbc->listBox.selectable) return;

    if (lbw->listBox.selection_mode == IswListBoxSelectSingle)
        ClearAllSelections(lbw);

    ApplySelectionVisual(lbw, child, True);

    if (IswIsRealized(w)) {
        _IswRepaintWindowless(w);
    }
}
