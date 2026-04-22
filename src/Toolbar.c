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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ToolbarP.h>
#include <ISW/Command.h>
#include <ISW/CommandP.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

#define superclass (&constraintClassRec)

#define Offset(field) IswOffsetOf(ToolbarRec, field)

static IswResource resources[] = {
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        IswOffsetOf(ToolbarRec, core.border_width), IswRImmediate, (IswPointer) 0},
    {IswNhSpace, IswCHSpace, IswRDimension, sizeof(Dimension),
        Offset(toolbar.h_space), IswRImmediate, (IswPointer) 2},
    {IswNvSpace, IswCVSpace, IswRDimension, sizeof(Dimension),
        Offset(toolbar.v_space), IswRImmediate, (IswPointer) 2},
};

#undef Offset

#define COffset(field) IswOffsetOf(ToolbarConstraintsRec, toolbar.field)
static IswResource constraintResources[] = {
    {IswNtoolbarAlignment, IswCtoolbarAlignment, IswRToolbarAlignment,
     sizeof(IswToolbarAlignment),
     COffset(alignment), IswRImmediate, (IswPointer)IswToolbarAlignLeft},
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
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static IswGeometryResult PreferredSize(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

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
    IswInheritRealize,                   /* realize                */
    NULL,                               /* actions                */
    0,                                  /* num_actions            */
    resources,                          /* resources              */
    IswNumber(resources),                /* resource_count         */
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
    IswInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    IswVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    PreferredSize,                      /* query_geometry         */
    IswInheritDisplayAccelerator,        /* display_accelerator    */
    NULL                                /* extension              */
  },
  { /* composite */
    GeometryManager,                    /* geometry_manager       */
    ChangeManaged,                      /* change_managed         */
    InsertChild,                        /* insert_child           */
    IswInheritDeleteChild,               /* delete_child           */
    NULL                                /* extension              */
  },
  { /* constraint */
    constraintResources,                /* subresources           */
    IswNumber(constraintResources),      /* subresource_count      */
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
        if      (q == QLeft)   align = IswToolbarAlignLeft;
        else if (q == QCenter) align = IswToolbarAlignCenter;
        else if (q == QRight)  align = IswToolbarAlignRight;
        else {
            toVal->size = 0;
            toVal->addr = NULL;
            return;
        }
        toVal->size = sizeof(align);
        toVal->addr = (IswPointer)&align;
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
    IswAddConverter(IswRString, IswRToolbarAlignment,
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
}

/*
 * Query a child's preferred width and height.
 * Uses the larger of IswQueryGeometry and the child's current core size,
 * so that explicitly set dimensions (e.g. IswNwidth 24) are respected.
 */
static void
ChildPreferredSize(Widget child, Dimension *w_out, Dimension *h_out)
{
    IswWidgetGeometry preferred;
    IswQueryGeometry(child, NULL, &preferred);

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
 *
 * Border-overlap accounting: X11 positions children at the outer corner of
 * their border, so a child at x spans [x, x + size + 2*bw]. Placing the
 * next child at (x + size + bw) makes its leading-border column coincide
 * with the previous child's trailing-border column — a single shared 1px
 * line instead of a 2px double line. Each child contributes (size + bw) to
 * the flow, plus one extra bw for the last child's trailing outer edge.
 */
static Dimension
GroupWidth(ToolbarWidget tw, IswToolbarAlignment align)
{
    Dimension w = 0;
    int count = 0;
    Dimension last_bw = 0;

    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != align)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        w += cw + child->core.border_width;
        last_bw = child->core.border_width;
        count++;
    }

    if (count > 0) {
        w += last_bw;
        w += (count - 1) * tw->toolbar.h_space;
    }

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
    Dimension last_bw = 0;

    /* First pass: measure all managed children using preferred geometry.
     * Adjacent children's borders overlap by one pixel column, so each child
     * costs (size + bw) along the main axis plus one trailing bw overall. */
    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw = child->core.border_width;
        Dimension bw2 = 2 * bw;
        total_width += cw + bw;
        last_bw = bw;
        if (ch + bw2 > max_height)
            max_height = ch + bw2;
        managed++;
    }

    if (managed > 0) {
        total_width += last_bw;
        total_width += (managed - 1) * h_space;
    }

    tw->toolbar.preferred_width = IswMax(total_width + 2 * h_space, 1);
    /* Height: v_space padding on top, child with its full border, and the
     * bottom separator/edge overlaps the child's bottom border (no trailing
     * v_space). When border_width > 0 the X border replaces the separator,
     * but the geometry is the same — the outer edge coincides with the
     * children's bottom border. */
    tw->toolbar.preferred_height = IswMax(max_height + v_space, 1);

    if (!set_children)
        return;

    /* Compute group widths */
    Dimension left_w  = GroupWidth(tw, IswToolbarAlignLeft);
    Dimension right_w = GroupWidth(tw, IswToolbarAlignRight);
    Dimension center_w = GroupWidth(tw, IswToolbarAlignCenter);

    Dimension container_w = tw->core.width;
    Dimension container_h = tw->core.height;

    /* Position left-aligned children */
    Position x = (Position)h_space;
    for (Cardinal i = 0; i < tw->composite.num_children; i++) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != IswToolbarAlignLeft)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw = child->core.border_width;
        Dimension bw2 = 2 * bw;
        Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
        if (y < (Position)v_space) y = (Position)v_space;
        IswConfigureWidget(child, x, y, cw, ch, bw);
        /* Advance by cw + bw so the next child's leading border overlaps
         * this child's trailing border by one pixel column. */
        x += (Position)(cw + bw + h_space);
    }

    /* Position right-aligned children (pack from right edge) */
    x = (Position)((int)container_w - (int)h_space);
    /* Walk children in reverse order so rightmost declared child is rightmost */
    for (int i = (int)tw->composite.num_children - 1; i >= 0; i--) {
        Widget child = tw->composite.children[i];
        if (!child->core.managed)
            continue;
        ToolbarConstraints tc = (ToolbarConstraints)child->core.constraints;
        if (tc->toolbar.alignment != IswToolbarAlignRight)
            continue;
        Dimension cw, ch;
        ChildPreferredSize(child, &cw, &ch);
        Dimension bw = child->core.border_width;
        Dimension bw2 = 2 * bw;
        x -= (Position)(cw + bw);
        Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
        if (y < (Position)v_space) y = (Position)v_space;
        IswConfigureWidget(child, x, y, cw, ch, bw);
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
            if (tc->toolbar.alignment != IswToolbarAlignCenter)
                continue;
            Dimension cw, ch;
            ChildPreferredSize(child, &cw, &ch);
            Dimension bw = child->core.border_width;
            Dimension bw2 = 2 * bw;
            Position y = (Position)((int)container_h - (int)ch - (int)bw2) / 2;
            if (y < (Position)v_space) y = (Position)v_space;
            IswConfigureWidget(child, x, y, cw, ch, bw);
            x += (Position)(cw + bw + h_space);
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
        IswWidgetGeometry req, reply;
        req.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        req.width = tw->toolbar.preferred_width;
        req.height = tw->toolbar.preferred_height;
        IswGeometryResult result = IswMakeGeometryRequest(w, &req, &reply);
        if (result == IswGeometryAlmost) {
            req.width = reply.width;
            req.height = reply.height;
            IswMakeGeometryRequest(w, &req, NULL);
        }
    }

    DoLayout(tw, TRUE);
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request,
                IswWidgetGeometry *reply)
{
    ToolbarWidget tw = (ToolbarWidget)IswParent(child);
    (void)reply;

    /* Strip position bits — we control placement */
    request->request_mode &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
    if (request->request_mode == 0)
        return IswGeometryNo;

    /* Save requested sizes, then let Xt apply them.
     * We must NOT pre-set core fields — DoLayout calls IswConfigureWidget,
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
        IswWidgetGeometry req, rep;
        req.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        req.width = tw->toolbar.preferred_width;
        req.height = tw->toolbar.preferred_height;
        IswGeometryResult result = IswMakeGeometryRequest((Widget)tw, &req, &rep);
        if (result == IswGeometryAlmost) {
            req.width = rep.width;
            req.height = rep.height;
            IswMakeGeometryRequest((Widget)tw, &req, NULL);
        }
    }

    /* Restore old values so IswConfigureWidget sees the delta */
    child->core.width = save_w;
    child->core.height = save_h;
    child->core.border_width = save_bw;

    /* Layout with real positioning — IswConfigureWidget will apply changes */
    DoLayout(tw, TRUE);

    return IswGeometryYes;
}

static IswGeometryResult
PreferredSize(Widget widget, IswWidgetGeometry *constraint,
              IswWidgetGeometry *preferred)
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
        return IswGeometryYes;
    if (preferred->width == tw->core.width && preferred->height == tw->core.height)
        return IswGeometryNo;
    return IswGeometryAlmost;
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
        DoLayout((ToolbarWidget)IswParent(new), TRUE);

    return FALSE;
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    (void)event; (void)region;

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
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
