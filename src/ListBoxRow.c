/*
 * ListBoxRow.c - Simple horizontal row for ListBox
 *
 * Lays children left-to-right.  Accepts whatever geometry its parent
 * imposes.  Fills its entire window with background_pixel so the
 * ListBox's selection highlight covers edge to edge.
 *
 * Per-child constraint: rowJustify (Left/Center/Right) controls
 * horizontal placement within the row.  Left-justified children pack
 * from the left edge; right-justified children pack from the right.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ListBoxRowP.h>
#include <ISW/ISWRender.h>

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, xcb_xfixes_region_t);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *,
                                         IswWidgetGeometry *);
static void ChangeManaged(Widget);

#define Offset(field) IswOffsetOf(ListBoxRowRec, listBoxRow.field)
static IswResource resources[] = {
    {IswNrowPadding, IswCRowPadding, IswRDimension, sizeof(Dimension),
        Offset(padding), IswRImmediate, (IswPointer)4},
};
#undef Offset

#define COffset(field) IswOffsetOf(ListBoxRowConstraintsRec, row.field)
static IswResource constraintResources[] = {
    {IswNjustify, IswCJustify, IswRJustify, sizeof(IswJustify),
        COffset(justify), IswRImmediate, (IswPointer)IswJustifyLeft},
};
#undef COffset

static void DoLayout(ListBoxRowWidget rw);

ListBoxRowClassRec listBoxRowClassRec = {
  { /* core_class */
    (WidgetClass) &constraintClassRec,
    "ListBoxRow",
    sizeof(ListBoxRowRec),
    NULL,                                /* class_initialize */
    NULL,                                /* class_part_initialize */
    FALSE,
    Initialize,
    NULL,                                /* initialize_hook */
    IswInheritRealize,
    NULL,                                /* actions */
    0,                                   /* num_actions */
    resources,
    IswNumber(resources),
    NULLQUARK,
    TRUE,                                /* compress_motion */
    TRUE,                                /* compress_exposure */
    TRUE,                                /* compress_enterleave */
    FALSE,                               /* visible_interest */
    Destroy,
    Resize,
    Redisplay,
    NULL,                                /* set_values */
    NULL,                                /* set_values_hook */
    IswInheritSetValuesAlmost,
    NULL,                                /* get_values_hook */
    NULL,                                /* accept_focus */
    IswVersion,
    NULL,                                /* callback_private */
    NULL,                                /* tm_table */
    NULL,                                /* query_geometry */
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
    sizeof(ListBoxRowConstraintsRec),
    NULL,                                /* constraint initialize */
    NULL,                                /* constraint destroy */
    NULL,                                /* constraint set_values */
    NULL
  },
  { /* listBoxRow_class */
    0
  }
};

WidgetClass listBoxRowWidgetClass = (WidgetClass)&listBoxRowClassRec;

static Dimension
ChildExtent(Widget child)
{
    return child->core.width + 2 * child->core.border_width;
}

static void
DoLayout(ListBoxRowWidget rw)
{
    Widget w = (Widget)rw;
    Dimension pad = rw->listBoxRow.padding;

    /* Measure left-group and right-group widths */
    Dimension left_w = 0, right_w = 0, center_w = 0;
    int nleft = 0, nright = 0, ncenter = 0;

    for (Cardinal i = 0; i < rw->composite.num_children; i++) {
        Widget child = rw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxRowConstraints rc =
            (ListBoxRowConstraints)child->core.constraints;
        Dimension cw = ChildExtent(child);
        switch (rc->row.justify) {
        case IswJustifyRight:
            right_w += cw;
            nright++;
            break;
        case IswJustifyCenter:
            center_w += cw;
            ncenter++;
            break;
        default:
            left_w += cw;
            nleft++;
            break;
        }
    }
    if (nleft)   left_w   += (Dimension)(nleft - 1) * pad;
    if (nright)  right_w  += (Dimension)(nright - 1) * pad;
    if (ncenter) center_w += (Dimension)(ncenter - 1) * pad;

    /* Place left-justified children from the left */
    Position xl = (Position)pad;
    for (Cardinal i = 0; i < rw->composite.num_children; i++) {
        Widget child = rw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxRowConstraints rc =
            (ListBoxRowConstraints)child->core.constraints;
        if (rc->row.justify != IswJustifyLeft) continue;

        Position y = (Position)pad;
        Dimension ch = w->core.height > 2 * pad
            ? w->core.height - 2 * pad : child->core.height;
        IswConfigureWidget(child, xl, y, child->core.width, ch,
                           child->core.border_width);
        xl += (Position)(ChildExtent(child) + pad);
    }

    /* Place right-justified children from the right */
    Position xr = (Position)((int)w->core.width - (int)pad - (int)right_w);
    if (xr < (Position)pad) xr = (Position)pad;
    for (Cardinal i = 0; i < rw->composite.num_children; i++) {
        Widget child = rw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        ListBoxRowConstraints rc =
            (ListBoxRowConstraints)child->core.constraints;
        if (rc->row.justify != IswJustifyRight) continue;

        Position y = (Position)pad;
        Dimension ch = w->core.height > 2 * pad
            ? w->core.height - 2 * pad : child->core.height;
        IswConfigureWidget(child, xr, y, child->core.width, ch,
                           child->core.border_width);
        xr += (Position)(ChildExtent(child) + pad);
    }

    /* Place center-justified children in the middle of the remaining space */
    if (ncenter > 0) {
        Position xc = (Position)(((int)w->core.width - (int)center_w) / 2);
        if (xc < xl) xc = xl;
        for (Cardinal i = 0; i < rw->composite.num_children; i++) {
            Widget child = rw->composite.children[i];
            if (!IswIsManaged(child)) continue;
            ListBoxRowConstraints rc =
                (ListBoxRowConstraints)child->core.constraints;
            if (rc->row.justify != IswJustifyCenter) continue;

            Position y = (Position)pad;
            Dimension ch = w->core.height > 2 * pad
                ? w->core.height - 2 * pad : child->core.height;
            IswConfigureWidget(child, xc, y, child->core.width, ch,
                               child->core.border_width);
            xc += (Position)(ChildExtent(child) + pad);
        }
    }
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListBoxRowWidget rw = (ListBoxRowWidget)new;
    (void)request; (void)args; (void)num_args;
    new->core.windowless = True;
    rw->listBoxRow.render_ctx = NULL;
    if (new->core.height == 0)
        new->core.height = 20;
}

static void
Destroy(Widget w)
{
    ListBoxRowWidget rw = (ListBoxRowWidget)w;
    if (rw->listBoxRow.render_ctx) {
        ISWRenderDestroy(rw->listBoxRow.render_ctx);
        rw->listBoxRow.render_ctx = NULL;
    }
}

static void
Resize(Widget w)
{
    DoLayout((ListBoxRowWidget)w);
}

static void
Redisplay(Widget w, IswEvent *event, xcb_xfixes_region_t region)
{
    ListBoxRowWidget rw = (ListBoxRowWidget)w;
    (void)event; (void)region;
    if (!IswIsRealized(w))
        return;

    if (!rw->listBoxRow.render_ctx)
        rw->listBoxRow.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    ISWRenderContext *ctx = rw->listBoxRow.render_ctx;
    if (!ctx) return;

    ISWRenderBegin(ctx);
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
    ISWRenderEnd(ctx);
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request,
                IswWidgetGeometry *reply)
{
    (void)reply;
    if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH)
        child->core.width = request->width;
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT)
        child->core.height = request->height;
    DoLayout((ListBoxRowWidget)IswParent(child));
    return IswGeometryYes;
}

static void
ChangeManaged(Widget w)
{
    ListBoxRowWidget rw = (ListBoxRowWidget)w;

    Dimension pad = rw->listBoxRow.padding;
    Dimension max_h = 0;
    for (Cardinal i = 0; i < rw->composite.num_children; i++) {
        Widget child = rw->composite.children[i];
        if (!IswIsManaged(child)) continue;
        Dimension h = child->core.height + 2 * child->core.border_width;
        if (h > max_h) max_h = h;
    }
    if (max_h > 0) {
        Dimension want = max_h + 2 * pad;
        Dimension got_w, got_h;
        IswGeometryResult r = IswMakeResizeRequest(w, w->core.width,
                                                   want, &got_w, &got_h);
        if (r == IswGeometryAlmost)
            IswMakeResizeRequest(w, got_w, got_h, NULL, NULL);
    }

    DoLayout(rw);
}
