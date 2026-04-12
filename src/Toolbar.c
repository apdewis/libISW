/*
 * Toolbar.c - Toolbar widget implementation
 *
 * A horizontal container for buttons and controls.
 * Subclasses Constraint to provide per-child alignment (left, center, right).
 * Children (typically Command/Toggle buttons) are automatically
 * styled with no border and no highlight for a flat toolbar look.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ToolbarP.h>
#include <ISW/Command.h>
#include <ISW/CommandP.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

#define superclass (&constraintClassRec)

#define Offset(field) XtOffsetOf(ToolbarRec, field)

static XtResource resources[] = {
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        XtOffsetOf(ToolbarRec, core.border_width), XtRImmediate, (XtPointer) 0},
    {XtNhSpace, XtCHSpace, XtRDimension, sizeof(Dimension),
        Offset(toolbar.h_space), XtRImmediate, (XtPointer) 2},
    {XtNvSpace, XtCVSpace, XtRDimension, sizeof(Dimension),
        Offset(toolbar.v_space), XtRImmediate, (XtPointer) 2},
};

#undef Offset

#define COffset(field) XtOffsetOf(ToolbarConstraintsRec, toolbar.field)
static XtResource constraintResources[] = {
    {XtNtoolbarAlignment, XtCtoolbarAlignment, XtRToolbarAlignment,
     sizeof(IswToolbarAlignment),
     COffset(alignment), XtRImmediate, (XtPointer)XtToolbarAlignLeft},
};
#undef COffset

/* Forward declarations */
static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void InsertChild(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void ChangeManaged(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static XtGeometryResult PreferredSize(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

ToolbarClassRec toolbarClassRec = {
  { /* core */
    (WidgetClass) superclass,           /* superclass             */
    "Toolbar",                          /* class_name             */
    sizeof(ToolbarRec),                 /* size                   */
    ClassInitialize,                    /* class_initialize       */
    NULL,                               /* class_part_initialize  */
    FALSE,                              /* class_inited           */
    Initialize,                         /* initialize             */
    NULL,                               /* initialize_hook        */
    XtInheritRealize,                   /* realize                */
    NULL,                               /* actions                */
    0,                                  /* num_actions            */
    resources,                          /* resources              */
    XtNumber(resources),                /* resource_count         */
    NULLQUARK,                          /* xrm_class              */
    TRUE,                               /* compress_motion        */
    TRUE,                               /* compress_exposure      */
    TRUE,                               /* compress_enterleave    */
    FALSE,                              /* visible_interest       */
    NULL,                               /* destroy                */
    Resize,                             /* resize                 */
    Redisplay,                          /* expose                 */
    SetValues,                          /* set_values             */
    NULL,                               /* set_values_hook        */
    XtInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    XtVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    PreferredSize,                      /* query_geometry         */
    XtInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    GeometryManager,                    /* geometry_manager       */
    ChangeManaged,                      /* change_managed         */
    InsertChild,                        /* insert_child           */
    XtInheritDeleteChild,               /* delete_child           */
    NULL                                /* extension              */
  },
  { /* constraint */
    constraintResources,                /* subresources           */
    XtNumber(constraintResources),      /* subresource_count      */
    sizeof(ToolbarConstraintsRec),      /* constraint_size        */
    NULL,                               /* initialize             */
    NULL,                               /* destroy                */
    ConstraintSetValues,                /* set_values             */
    NULL                                /* extension              */
  },
  { /* toolbar */
    0                                   /* empty                  */
  }
};

WidgetClass toolbarWidgetClass = (WidgetClass)&toolbarClassRec;

/* --- String-to-ToolbarAlignment converter --- */

static XrmQuark QLeft, QCenter, QRight;

static void
_CvtStringToToolbarAlignment(XrmValuePtr args, Cardinal *num_args,
                             XrmValuePtr fromVal, XrmValuePtr toVal)
{
    static IswToolbarAlignment align;
    XrmQuark q;
    char lower[40];
    (void)args; (void)num_args;

    if (strlen((char *)fromVal->addr) < sizeof(lower)) {
        ISWCopyISOLatin1Lowered(lower, (char *)fromVal->addr);
        q = XrmStringToQuark(lower);
        if      (q == QLeft)   align = XtToolbarAlignLeft;
        else if (q == QCenter) align = XtToolbarAlignCenter;
        else if (q == QRight)  align = XtToolbarAlignRight;
        else {
            toVal->size = 0;
            toVal->addr = NULL;
            return;
        }
        toVal->size = sizeof(align);
        toVal->addr = (XtPointer)&align;
        return;
    }
    toVal->addr = NULL;
    toVal->size = 0;
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    QLeft   = XrmPermStringToQuark("left");
    QCenter = XrmPermStringToQuark("center");
    QRight  = XrmPermStringToQuark("right");
    XtAddConverter(XtRString, XtRToolbarAlignment,
                   _CvtStringToToolbarAlignment, NULL, 0);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ToolbarWidget tw = (ToolbarWidget) new;
    (void)request; (void)args; (void)num_args;

    tw->toolbar.preferred_width = IswMax(tw->toolbar.h_space, 1);
    tw->toolbar.preferred_height = IswMax(tw->toolbar.v_space, 1);

    if (tw->core.width == 0)
        tw->core.width = tw->toolbar.preferred_width;
    if (tw->core.height == 0)
        tw->core.height = tw->toolbar.preferred_height;
}

static void
InsertChild(Widget child)
{
    /* Call Constraint's insert_child */
    (*constraintClassRec.composite_class.insert_child)(child);

    /* Style Command-subclass children for flat toolbar appearance */
    if (XtIsSubclass(child, commandWidgetClass)) {
        Arg args[4];
        Cardinal n = 0;

        XtSetArg(args[n], XtNborderWidth, 0); n++;
        XtSetArg(args[n], XtNborderStrokeWidth, 0); n++;
        XtSetValues(child, args, n);

        /* Force border_stroke_width to 0 (bypasses Command's Initialize default) */
        ((CommandWidget)child)->command.border_stroke_width = 0;
    }
}

/*
 * Query a child's preferred width and height.
 * Uses the larger of XtQueryGeometry and the child's current core size,
 * so that explicitly set dimensions (e.g. XtNwidth 24) are respected.
 */
static void
ChildPreferredSize(Widget child, Dimension *w_out, Dimension *h_out)
{
    XtWidgetGeometry preferred;
    XtQueryGeometry(child, NULL, &preferred);

    Dimension pw = (preferred.request_mode & XCB_CONFIG_WINDOW_WIDTH)
                   ? preferred.width : child->core.width;
    Dimension ph = (preferred.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
                   ? preferred.height : child->core.height;

    *w_out = IswMax(pw, child->core.width);
    *h_out = IswMax(ph, child->core.height);
}

/*
 * Compute the width needed by a group of children.
 * Returns total width including inter-child spacing.
 */
static Dimension
GroupWidth(ToolbarWidget tw, IswToolbarAlignment align)
{
    Dimension w = 0;
    int count = 0;

    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != align)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        w += cw + 2 * child->core.border_width;
        count++;
    }

    if (count > 0)
        w += (count - 1) * tw->toolbar.h_space;

    return w;
}

/*
 * Layout children within the toolbar.
 * Left-aligned children pack from the left edge.
 * Right-aligned children pack from the right edge.
 * Center-aligned children are centered in the remaining space.
 *
 * If set_children is FALSE, only computes preferred_width/height.
 */
static void
DoLayout(ToolbarWidget tw, Boolean set_children)
{
    Dimension h_space = tw->toolbar.h_space;
    Dimension v_space = tw->toolbar.v_space;
    int managed = 0;
    Dimension total_width = 0;
    Dimension max_height = 0;

    /* First pass: measure all managed children using preferred geometry */
    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw2 = 2 * child->core.border_width;
        total_width += cw + bw2;
        if (ch + bw2 > max_height)
            max_height = ch + bw2;
        managed++;
    }

    if (managed > 0)
        total_width += (managed - 1) * h_space;

    tw->toolbar.preferred_width = IswMax(total_width + 2 * h_space, 1);
    tw->toolbar.preferred_height = IswMax(max_height + 2 * v_space, 1);

    if (!set_children)
        return;

    /* Compute group widths */
    Dimension left_w  = GroupWidth(tw, XtToolbarAlignLeft);
    Dimension right_w = GroupWidth(tw, XtToolbarAlignRight);
    Dimension center_w = GroupWidth(tw, XtToolbarAlignCenter);

    Dimension container_w = tw->core.width;
    Dimension container_h = tw->core.height;

    /* Position left-aligned children */
    Position x = (Position)h_space;
    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != XtToolbarAlignLeft)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw2 = 2 * child->core.border_width;
        Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
        if (y < (Position)v_space) y = (Position)v_space;
        XtConfigureWidget(child, x, y, cw, ch, child->core.border_width);
        x += (Position)(cw + bw2 + h_space);
    }

    /* Position right-aligned children (pack from right edge) */
    x = (Position)((int)container_w - (int)h_space);
    /* Walk children in reverse order so rightmost declared child is rightmost */
    for (int i = (int)tw->composite.num_children - 1; i >= 0; i--) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != XtToolbarAlignRight)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw2 = 2 * child->core.border_width;
        x -= (Position)(cw + bw2);
        Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
        if (y < (Position)v_space) y = (Position)v_space;
        XtConfigureWidget(child, x, y, cw, ch, child->core.border_width);
        x -= (Position)h_space;
    }

    /* Position center-aligned children */
    if (center_w > 0) {
        /* Center the group's midpoint on the container's midpoint */
        int center_start = ((int)container_w - (int)center_w) / 2;

        /* Clamp so we don't overlap left or right groups */
        int left_edge = (int)h_space + (int)left_w;
        if (left_w > 0)
            left_edge += (int)h_space;
        int right_edge = (int)container_w - (int)h_space - (int)right_w;
        if (right_w > 0)
            right_edge -= (int)h_space;
        if (center_start < left_edge)
            center_start = left_edge;
        if (center_start + (int)center_w > right_edge)
            center_start = right_edge - (int)center_w;

        x = (Position)center_start;
        for (Cardinal i = 0; i < tw->composite.num_children; i++) {
            Widget child = tw->composite.children[i];
            if (!child->core.managed)
                continue;
            ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
            if (tc->toolbar.alignment != XtToolbarAlignCenter)
                continue;
            Dimension cw, ch;
            ChildPreferredSize(child, &cw, &ch);
            Dimension bw2 = 2 * child->core.border_width;
            Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
            if (y < (Position)v_space) y = (Position)v_space;
            XtConfigureWidget(child, x, y, cw, ch, child->core.border_width);
            x += (Position)(cw + bw2 + h_space);
        }
    }
}

static void
Resize(Widget w)
{
    DoLayout((ToolbarWidget)w, TRUE);
}

static void
ChangeManaged(Widget w)
{
    ToolbarWidget tw = (ToolbarWidget)w;

    DoLayout(tw, FALSE);

    if (tw->toolbar.preferred_width != tw->core.width ||
        tw->toolbar.preferred_height != tw->core.height)
    {
        XtWidgetGeometry req, reply;
        req.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        req.width = tw->toolbar.preferred_width;
        req.height = tw->toolbar.preferred_height;
        XtGeometryResult result = XtMakeGeometryRequest(w, &req, &reply);
        if (result == XtGeometryAlmost) {
            req.width = reply.width;
            req.height = reply.height;
            XtMakeGeometryRequest(w, &req, NULL);
        }
    }

    DoLayout(tw, TRUE);
}

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request,
                XtWidgetGeometry *reply)
{
    ToolbarWidget tw = (ToolbarWidget)XtParent(child);
    (void)reply;

    /* Strip position bits — we control placement */
    request->request_mode &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
    if (request->request_mode == 0)
        return XtGeometryNo;

    /* Save requested sizes, then let Xt apply them.
     * We must NOT pre-set core fields — DoLayout calls XtConfigureWidget,
     * which skips the xcb_configure_window if it sees no delta. */
    Dimension save_w = child->core.width;
    Dimension save_h = child->core.height;
    Dimension save_bw = child->core.border_width;

    if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH)
        child->core.width = request->width;
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT)
        child->core.height = request->height;
    if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH)
        child->core.border_width = request->border_width;

    /* Recompute preferred toolbar size and negotiate with parent */
    DoLayout(tw, FALSE);
    if (tw->toolbar.preferred_width != tw->core.width ||
        tw->toolbar.preferred_height != tw->core.height)
    {
        XtWidgetGeometry req, rep;
        req.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        req.width = tw->toolbar.preferred_width;
        req.height = tw->toolbar.preferred_height;
        XtGeometryResult result = XtMakeGeometryRequest((Widget)tw, &req, &rep);
        if (result == XtGeometryAlmost) {
            req.width = rep.width;
            req.height = rep.height;
            XtMakeGeometryRequest((Widget)tw, &req, NULL);
        }
    }

    /* Restore old values so XtConfigureWidget sees the delta */
    child->core.width = save_w;
    child->core.height = save_h;
    child->core.border_width = save_bw;

    /* Layout with real positioning — XtConfigureWidget will apply changes */
    DoLayout(tw, TRUE);

    return XtGeometryYes;
}

static XtGeometryResult
PreferredSize(Widget widget, XtWidgetGeometry *constraint,
              XtWidgetGeometry *preferred)
{
    ToolbarWidget tw = (ToolbarWidget)widget;

    DoLayout(tw, FALSE);

    preferred->width = tw->toolbar.preferred_width;
    preferred->height = tw->toolbar.preferred_height;
    preferred->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    if ((constraint->request_mode & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
        == (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
        && constraint->width == preferred->width
        && constraint->height == preferred->height)
        return XtGeometryYes;
    if (preferred->width == tw->core.width && preferred->height == tw->core.height)
        return XtGeometryNo;
    return XtGeometryAlmost;
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList args, Cardinal *num_args)
{
    ToolbarWidget cur = (ToolbarWidget)current;
    ToolbarWidget nw  = (ToolbarWidget)new;
    (void)request; (void)args; (void)num_args;

    if (cur->toolbar.h_space != nw->toolbar.h_space ||
        cur->toolbar.v_space != nw->toolbar.v_space)
    {
        DoLayout(nw, TRUE);
    }
    return FALSE;
}

static Boolean
ConstraintSetValues(Widget current, Widget request, Widget new,
                    ArgList args, Cardinal *num_args)
{
    ToolbarConstraints ctc = (ToolbarConstraints)current->core.constraints;
    ToolbarConstraints ntc = (ToolbarConstraints)new->core.constraints;
    (void)request; (void)args; (void)num_args;

    if (ctc->toolbar.alignment != ntc->toolbar.alignment)
        DoLayout((ToolbarWidget)XtParent(new), TRUE);

    return FALSE;
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    (void)event; (void)region;

    if (!XtIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    /* Skip separator when the X window already has a border */
    if (w->core.border_width > 0)
        return;

    /* Draw bottom separator line */
    ISWRenderContext *ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx) {
        int y = (int)w->core.height - 1;
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, w->core.border_pixel);
        ISWRenderDrawLine(ctx, 0, y, (int)w->core.width, y);
        ISWRenderEnd(ctx);
        ISWRenderDestroy(ctx);
    }
}
