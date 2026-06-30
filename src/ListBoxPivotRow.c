/*
 * ListBoxPivotRow.c - Expandable row for ListBox
 *
 * A ListBoxRow subclass that shows a pivot icon and label.  Clicking
 * toggles the pivot open/closed, managing or unmanaging a single child
 * ListBox to reveal or hide nested content.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/IswArgMacros.h>
#include <ISW/ListBoxPivotRowP.h>
#include <ISW/ListBox.h>
#include <ISW/Label.h>

#include <string.h>

static const char pivot_closed_svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='32'>"
    "<path d='M10,4 L26,16 L10,28' stroke='currentColor' stroke-width='4' "
    "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";

static const char pivot_open_svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='32' height='32'>"
    "<path d='M4,10 L16,26 L28,10' stroke='currentColor' stroke-width='4' "
    "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void ChangeManaged(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *,
                                         IswWidgetGeometry *);
static void InsertChild(Widget);
static void DeleteChild(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void TogglePivotAction(Widget, IswEvent *, String *, Cardinal *);
static void HeaderClickHandler(Widget, IswPointer, IswEvent *, Boolean *);

#define Offset(field) IswOffsetOf(ListBoxPivotRowRec, listBoxPivotRow.field)
static IswResource resources[] = {
    {IswNpivotOpen, IswCPivotOpen, IswRBoolean, sizeof(Boolean),
        Offset(pivot_open), IswRImmediate, (IswPointer)False},
    {IswNpivotImage, IswCPivotImage, IswRString, sizeof(String),
        Offset(pivot_image), IswRImmediate, (IswPointer)NULL},
    {IswNpivotImageOpen, IswCPivotImageOpen, IswRString, sizeof(String),
        Offset(pivot_image_open), IswRImmediate, (IswPointer)NULL},
    {IswNlabel, IswCLabel, IswRString, sizeof(String),
        Offset(label), IswRImmediate, (IswPointer)NULL},
    {IswNpivotCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(pivot_callback), IswRCallback, NULL},
};
#undef Offset

static char defaultTranslations[] =
    "<BtnDown>,<BtnUp>: TogglePivot()";

static IswActionsRec actionsList[] = {
    {"TogglePivot", TogglePivotAction},
};

ListBoxPivotRowClassRec listBoxPivotRowClassRec = {
  { /* core_class */
    (WidgetClass) &listBoxRowClassRec,
    "ListBoxPivotRow",
    sizeof(ListBoxPivotRowRec),
    NULL,                                /* class_initialize */
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
    IswInheritExpose,
    SetValues,
    NULL,                                /* set_values_hook */
    IswInheritSetValuesAlmost,
    NULL,                                /* get_values_hook */
    NULL,                                /* accept_focus */
    IswVersion,
    NULL,                                /* callback_private */
    defaultTranslations,
    NULL,                                /* query_geometry */
    IswInheritDisplayAccelerator,
    NULL                                 /* extension */
  },
  { /* composite_class */
    GeometryManager,
    ChangeManaged,
    InsertChild,
    DeleteChild,
    NULL
  },
  { /* constraint_class */
    NULL,
    0,
    sizeof(ListBoxRowConstraintsRec),
    NULL,                                /* constraint initialize */
    NULL,                                /* constraint destroy */
    NULL,                                /* constraint set_values */
    NULL
  },
  { /* listBoxRow_class */
    0
  },
  { /* listBoxPivotRow_class */
    0
  }
};

WidgetClass listBoxPivotRowWidgetClass = (WidgetClass)&listBoxPivotRowClassRec;

static void
DoLayout(ListBoxPivotRowWidget pw)
{
    Widget w = (Widget)pw;
    Dimension pad = pw->listBoxRow.padding;
    Widget icon = pw->listBoxPivotRow.icon_w;
    Widget label = pw->listBoxPivotRow.label_w;
    Widget child = pw->listBoxPivotRow.child_listbox;

    Dimension icon_w = icon ? icon->core.width : 0;
    Dimension icon_h = icon ? icon->core.height : 0;
    Dimension label_h = label ? label->core.height : 0;
    Dimension header_h = icon_h > label_h ? icon_h : label_h;
    if (header_h == 0) header_h = 20;

    Position x = (Position)pad;
    Position icon_y = (Position)(pad + (header_h - icon_h) / 2);
    if (icon)
        IswConfigureWidget(icon, x, icon_y, icon->core.width,
                           icon->core.height, icon->core.border_width);
    x += (Position)(icon_w + pad);

    Dimension label_avail = (w->core.width > (Dimension)x + pad)
        ? w->core.width - (Dimension)x - pad : 1;
    Position label_y = (Position)(pad + (header_h - label_h) / 2);
    if (label)
        IswConfigureWidget(label, x, label_y, label_avail,
                           label->core.height, label->core.border_width);

    Dimension total_h = header_h + 2 * pad;

    if (child && IswIsManaged(child)) {
        Position child_x = x;
        Position child_y = (Position)(header_h + 2 * pad);
        Dimension child_w = (w->core.width > (Dimension)child_x)
            ? w->core.width - (Dimension)child_x : 1;
        IswConfigureWidget(child, child_x, child_y, child_w,
                           child->core.height, 0);
        total_h = (Dimension)child_y + child->core.height;
    }

    if (total_h != w->core.height) {
        Dimension got_w, got_h;
        IswGeometryResult r = IswMakeResizeRequest(w, w->core.width,
                                                    total_h, &got_w, &got_h);
        if (r == IswGeometryAlmost)
            IswMakeResizeRequest(w, got_w, got_h, NULL, NULL);
    }
}

static void
Resize(Widget w)
{
    DoLayout((ListBoxPivotRowWidget)w);
}

static void
ChangeManaged(Widget w)
{
    ListBoxPivotRowWidget pw = (ListBoxPivotRowWidget)w;
    Widget child = pw->listBoxPivotRow.child_listbox;
    if (child && IswIsManaged(child) && !pw->listBoxPivotRow.pivot_open)
        IswUnmanageChild(child);
    else
        DoLayout(pw);
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request,
                IswWidgetGeometry *reply)
{
    (void)reply;
    if (request->request_mode & IswCWWidth)
        child->core.width = request->width;
    if (request->request_mode & IswCWHeight)
        child->core.height = request->height;
    DoLayout((ListBoxPivotRowWidget)IswParent(child));
    return IswGeometryYes;
}

static Boolean
IsInternalChild(ListBoxPivotRowWidget pw, Widget child)
{
    return child == pw->listBoxPivotRow.icon_w ||
           child == pw->listBoxPivotRow.label_w;
}

static void
SyncIcon(ListBoxPivotRowWidget pw)
{
    const char *src;
    if (pw->listBoxPivotRow.pivot_open) {
        src = pw->listBoxPivotRow.pivot_image_open
            ? pw->listBoxPivotRow.pivot_image_open : pivot_open_svg;
    } else {
        src = pw->listBoxPivotRow.pivot_image
            ? pw->listBoxPivotRow.pivot_image : pivot_closed_svg;
    }
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgImage(&ab, src);
    IswSetValues(pw->listBoxPivotRow.icon_w, ab.args, ab.count);
}

static void
SyncChildVisibility(ListBoxPivotRowWidget pw)
{
    Widget child = pw->listBoxPivotRow.child_listbox;
    if (!child) return;

    if (pw->listBoxPivotRow.pivot_open)
        IswManageChild(child);
    else
        IswUnmanageChild(child);
}

static void
DoToggle(ListBoxPivotRowWidget pw)
{
    pw->listBoxPivotRow.pivot_open = !pw->listBoxPivotRow.pivot_open;
    SyncIcon(pw);
    SyncChildVisibility(pw);
    IswCallCallbacks((Widget)pw, IswNpivotCallback,
                     (IswPointer)(uintptr_t)pw->listBoxPivotRow.pivot_open);
}

static void
HeaderClickHandler(Widget w, IswPointer client_data, IswEvent *event,
                   Boolean *continue_to_dispatch)
{
    (void)w;
    *continue_to_dispatch = False;
    if (event->kind != IswButtonUp)
        return;
    DoToggle((ListBoxPivotRowWidget)client_data);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListBoxPivotRowWidget pw = (ListBoxPivotRowWidget)new;
    (void)request; (void)args; (void)num_args;

    IswArgBuilder ab = IswArgBuilderInit();

    const char *icon_src = pw->listBoxPivotRow.pivot_open
        ? (pw->listBoxPivotRow.pivot_image_open
            ? pw->listBoxPivotRow.pivot_image_open : pivot_open_svg)
        : (pw->listBoxPivotRow.pivot_image
            ? pw->listBoxPivotRow.pivot_image : pivot_closed_svg);

    IswArgLabel(&ab, "");
    IswArgImage(&ab, icon_src);
    IswArgBorderWidth(&ab, 0);
    IswArgInternalWidth(&ab, 2);
    IswArgInternalHeight(&ab, 0);
    IswArgResize(&ab, True);
    pw->listBoxPivotRow.icon_w = IswCreateManagedWidget(
        "pivotIcon", labelWidgetClass, new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, pw->listBoxPivotRow.label
                     ? pw->listBoxPivotRow.label : "");
    IswArgBorderWidth(&ab, 0);
    IswArgInternalWidth(&ab, 2);
    IswArgInternalHeight(&ab, 0);
    IswArgResize(&ab, True);
    IswArgJustify(&ab, IswJustifyLeft);
    pw->listBoxPivotRow.label_w = IswCreateManagedWidget(
        "pivotLabel", labelWidgetClass, new, ab.args, ab.count);

    IswAddEventHandler(pw->listBoxPivotRow.icon_w,
                       IswButtonPressMask | IswButtonReleaseMask, False,
                       HeaderClickHandler, (IswPointer)pw);
    IswAddEventHandler(pw->listBoxPivotRow.label_w,
                       IswButtonPressMask | IswButtonReleaseMask, False,
                       HeaderClickHandler, (IswPointer)pw);

    pw->listBoxPivotRow.child_listbox = NULL;

    if (pw->listBoxPivotRow.pivot_image)
        pw->listBoxPivotRow.pivot_image =
            IswNewString(pw->listBoxPivotRow.pivot_image);
    if (pw->listBoxPivotRow.pivot_image_open)
        pw->listBoxPivotRow.pivot_image_open =
            IswNewString(pw->listBoxPivotRow.pivot_image_open);
    if (pw->listBoxPivotRow.label)
        pw->listBoxPivotRow.label =
            IswNewString(pw->listBoxPivotRow.label);
}

static void
Destroy(Widget w)
{
    ListBoxPivotRowWidget pw = (ListBoxPivotRowWidget)w;
    IswFree(pw->listBoxPivotRow.pivot_image);
    IswFree(pw->listBoxPivotRow.pivot_image_open);
    IswFree(pw->listBoxPivotRow.label);
}

static void
InsertChild(Widget child)
{
    ListBoxPivotRowWidget pw = (ListBoxPivotRowWidget)IswParent(child);

    /* Chain up */
    (*listBoxRowClassRec.composite_class.insert_child)(child);

    if (IsInternalChild(pw, child))
        return;

    if (!pw->listBoxPivotRow.child_listbox)
        pw->listBoxPivotRow.child_listbox = child;
}

static void
DeleteChild(Widget child)
{
    ListBoxPivotRowWidget pw = (ListBoxPivotRowWidget)IswParent(child);

    if (child == pw->listBoxPivotRow.child_listbox)
        pw->listBoxPivotRow.child_listbox = NULL;

    (*listBoxRowClassRec.composite_class.delete_child)(child);
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList args, Cardinal *num_args)
{
    ListBoxPivotRowWidget cur = (ListBoxPivotRowWidget)current;
    ListBoxPivotRowWidget nw  = (ListBoxPivotRowWidget)new;
    Boolean redisplay = False;
    (void)request; (void)args; (void)num_args;

    if (nw->listBoxPivotRow.label != cur->listBoxPivotRow.label) {
        IswFree(cur->listBoxPivotRow.label);
        nw->listBoxPivotRow.label = IswNewString(
            nw->listBoxPivotRow.label ? nw->listBoxPivotRow.label : "");
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgLabel(&ab, nw->listBoxPivotRow.label);
        IswSetValues(nw->listBoxPivotRow.label_w, ab.args, ab.count);
        redisplay = True;
    }

    if (nw->listBoxPivotRow.pivot_image != cur->listBoxPivotRow.pivot_image) {
        IswFree(cur->listBoxPivotRow.pivot_image);
        nw->listBoxPivotRow.pivot_image = nw->listBoxPivotRow.pivot_image
            ? IswNewString(nw->listBoxPivotRow.pivot_image) : NULL;
        SyncIcon(nw);
        redisplay = True;
    }

    if (nw->listBoxPivotRow.pivot_image_open !=
        cur->listBoxPivotRow.pivot_image_open) {
        IswFree(cur->listBoxPivotRow.pivot_image_open);
        nw->listBoxPivotRow.pivot_image_open =
            nw->listBoxPivotRow.pivot_image_open
            ? IswNewString(nw->listBoxPivotRow.pivot_image_open) : NULL;
        SyncIcon(nw);
        redisplay = True;
    }

    if (nw->listBoxPivotRow.pivot_open != cur->listBoxPivotRow.pivot_open) {
        SyncIcon(nw);
        SyncChildVisibility(nw);
        redisplay = True;
    }

    return redisplay;
}

static void
TogglePivotAction(Widget w, IswEvent *iswev, String *params,
                  Cardinal *num_params)
{
    (void)iswev; (void)params; (void)num_params;

    while (w && !IswIsSubclass(w, listBoxPivotRowWidgetClass))
        w = IswParent(w);
    if (w)
        DoToggle((ListBoxPivotRowWidget)w);
}
