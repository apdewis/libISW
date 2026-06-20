/*
 * StatusBar.c - StatusBar widget implementation
 *
 * A horizontal Constraint container for status panes. Children with
 * statusStretch=True fill remaining space after fixed-width children
 * are laid out. Draws a top separator line.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/StatusBarP.h>
#include <ISW/Label.h>
#include <ISW/ISWRender.h>
#include <ISW/IswArgMacros.h>

#define superclass (&constraintClassRec)

/* --- Resources --- */

#define Offset(field) IswOffsetOf(StatusBarRec, field)

static IswResource resources[] = {
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
};

#undef Offset

/* Constraint resources (per-child) */
#define COffset(field) IswOffsetOf(StatusBarConstraintsRec, field)

static IswResource constraintResources[] = {
    {IswNstatusStretch, IswCStatusStretch, IswRBoolean, sizeof(Boolean),
        COffset(statusBar.stretch), IswRImmediate, (IswPointer) False},
};

#undef COffset

/* --- Forward declarations --- */

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static void InsertChild(Widget);
static void ChangeManaged(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

static void DoLayout(StatusBarWidget);

/* --- Class record --- */

StatusBarClassRec statusBarClassRec = {
  { /* core */
    (WidgetClass) superclass,           /* superclass             */
    "StatusBar",                        /* class_name             */
    sizeof(StatusBarRec),               /* size                   */
    IswInitializeWidgetSet,             /* class_initialize       */
    NULL,                               /* class_part_initialize  */
    FALSE,                              /* class_inited           */
    Initialize,                         /* initialize             */
    NULL,                               /* initialize_hook        */
    IswInheritRealize,                   /* realize                */
    NULL,                               /* actions                */
    0,                                  /* num_actions            */
    resources,                          /* resources              */
    IswNumber(resources),                /* resource_count         */
    ISW_NULLQUARK,                          /* xrm_class              */
    TRUE,                               /* compress_motion        */
    TRUE,                               /* compress_exposure      */
    TRUE,                               /* compress_enterleave    */
    FALSE,                              /* visible_interest       */
    NULL,                               /* destroy                */
    Resize,                             /* resize                 */
    Redisplay,                          /* expose                 */
    NULL,                               /* set_values             */
    NULL,                               /* set_values_hook        */
    IswInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    IswVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    IswInheritQueryGeometry,             /* query_geometry         */
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
    sizeof(StatusBarConstraintsRec),    /* constraint_size        */
    NULL,                               /* initialize             */
    NULL,                               /* destroy                */
    NULL,                               /* set_values             */
    NULL                                /* extension              */
  },
  { /* statusBar */
    0                                   /* empty                  */
  }
};

WidgetClass statusBarWidgetClass = (WidgetClass)&statusBarClassRec;

/* --- Layout --- */

static void
DoLayout(StatusBarWidget sw)
{
    Cardinal i;
    Dimension h_space = sw->statusBar.h_space;
    Dimension total_w = sw->core.width;
    Dimension bar_h = sw->core.height;

    /* First pass: sum fixed-width children, find stretch child */
    Dimension fixed_total = 0;
    Cardinal managed_count = 0;
    Widget stretch_child = NULL;

    for (i = 0; i < sw->composite.num_children; i++) {
        Widget child = sw->composite.children[i];
        if (!IswIsManaged(child))
            continue;

        StatusBarConstraintsPart *cp =
            &((StatusBarConstraintsRec *)child->core.constraints)->statusBar;

        managed_count++;
        if (cp->stretch && !stretch_child) {
            stretch_child = child;
        } else {
            IswWidgetGeometry pref;
            IswQueryGeometry(child, NULL, &pref);
            Dimension cw = (pref.request_mode & IswCWWidth) ? pref.width
                                                          : child->core.width;
            fixed_total += cw;
        }
    }

    /* Account for spacing */
    Dimension spacing = (managed_count > 1) ? h_space * (managed_count - 1) : 0;
    Dimension available = (total_w > fixed_total + spacing)
                          ? total_w - fixed_total - spacing : 1;

    /* Second pass: position children */
    Position x = 0;
    for (i = 0; i < sw->composite.num_children; i++) {
        Widget child = sw->composite.children[i];
        if (!IswIsManaged(child))
            continue;

        Dimension cw;
        if (child == stretch_child) {
            cw = available;
        } else {
            IswWidgetGeometry pref;
            IswQueryGeometry(child, NULL, &pref);
            cw = (pref.request_mode & IswCWWidth) ? pref.width
                                                : child->core.width;
        }

        /* Start at y=1 to leave room for top separator line */
        Dimension child_h = (bar_h > 1) ? bar_h - 1 : 1;
        IswConfigureWidget(child, x, 1, cw, child_h, 0);
        x += (Position)(cw + h_space);
    }
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    StatusBarWidget sw = (StatusBarWidget) new;
    (void)request; (void)args; (void)num_args;

    sw->statusBar.render_ctx = NULL;

    sw->statusBar.h_space = (4);

    /* Default height so MainWindow can lay us out before children arrive */
    if (sw->core.height == 0)
        sw->core.height = (20);
}

static void
InsertChild(Widget child)
{
    (*constraintClassRec.composite_class.insert_child)(child);

    /* Style Label children for flat appearance.  As windowless widgets the
       labels fill their OWN background over their footprint; the stretch label
       spans nearly the whole bar, so if its background differs from the bar it
       paints over the bar's fill (the bar then looks like the window shows
       through).  Match the child's background to the StatusBar's. */
    if (IswIsSubclass(child, labelWidgetClass)) {
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgBorderWidth(&ab, 0);
        IswArgBackground(&ab, IswParent(child)->core.background_pixel);
        IswSetValues(child, ab.args, ab.count);
    }
}

static void
Resize(Widget w)
{
    DoLayout((StatusBarWidget) w);
    /* A width/height change reallocates this windowless widget's surface to a
       new (transparent) footprint.  The resize path composites straight to the
       windowed ancestor without an Expose, so re-paint our background now —
       otherwise the new surface stays transparent and the window shows through
       the bar. */
    Redisplay(w, NULL, 0);
}

static void
ChangeManaged(Widget w)
{
    DoLayout((StatusBarWidget) w);
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    (void)child; (void)request; (void)reply;
    /* Deny child resize requests — layout controls sizing */
    DoLayout((StatusBarWidget) IswParent(child));
    return IswGeometryNo;
}

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    (void)event; (void)region;

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    /* Fill background + draw top separator line (windowless: own surface). */
    {
        StatusBarWidget sw = (StatusBarWidget) w;
        ISWRenderContext *ctx = sw->statusBar.render_ctx;
        if (ctx == NULL)
            ctx = sw->statusBar.render_ctx =
                ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
        if (ctx) {
            ISWRenderBegin(ctx);
            ISWRenderSetColor(ctx, w->core.background_pixel);
            ISWRenderFillRectangle(ctx, 0, 0, (int)w->core.width,
                                   (int)w->core.height);
            ISWRenderSetColor(ctx, w->core.border_pixel);
            ISWRenderDrawLine(ctx, 0, 0, (int)w->core.width, 0);
            ISWRenderEnd(ctx);
        }
    }
}
