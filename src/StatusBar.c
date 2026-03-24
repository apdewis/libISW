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
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/StatusBarP.h>
#include <ISW/Label.h>
#include <ISW/ISWRender.h>

#define superclass (&constraintClassRec)

/* --- Resources --- */

#define Offset(field) XtOffsetOf(StatusBarRec, field)

static XtResource resources[] = {
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
};

#undef Offset

/* Constraint resources (per-child) */
#define COffset(field) XtOffsetOf(StatusBarConstraintsRec, field)

static XtResource constraintResources[] = {
    {XtNstatusStretch, XtCStatusStretch, XtRBoolean, sizeof(Boolean),
        COffset(statusBar.stretch), XtRImmediate, (XtPointer) False},
};

#undef COffset

/* --- Forward declarations --- */

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void InsertChild(Widget);
static void ChangeManaged(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

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
    NULL,                               /* set_values             */
    NULL,                               /* set_values_hook        */
    XtInheritSetValuesAlmost,           /* set_values_almost      */
    NULL,                               /* get_values_hook        */
    NULL,                               /* accept_focus           */
    XtVersion,                          /* version                */
    NULL,                               /* callback_private       */
    NULL,                               /* tm_table               */
    XtInheritQueryGeometry,             /* query_geometry         */
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
        if (!XtIsManaged(child))
            continue;

        StatusBarConstraintsPart *cp =
            &((StatusBarConstraintsRec *)child->core.constraints)->statusBar;

        managed_count++;
        if (cp->stretch && !stretch_child) {
            stretch_child = child;
        } else {
            XtWidgetGeometry pref;
            XtQueryGeometry(child, NULL, &pref);
            Dimension cw = (pref.request_mode & CWWidth) ? pref.width
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
        if (!XtIsManaged(child))
            continue;

        Dimension cw;
        if (child == stretch_child) {
            cw = available;
        } else {
            XtWidgetGeometry pref;
            XtQueryGeometry(child, NULL, &pref);
            cw = (pref.request_mode & CWWidth) ? pref.width
                                                : child->core.width;
        }

        /* Start at y=1 to leave room for top separator line */
        Dimension child_h = (bar_h > 1) ? bar_h - 1 : 1;
        XtConfigureWidget(child, x, 1, cw, child_h, 0);
        x += (Position)(cw + h_space);
    }
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    StatusBarWidget sw = (StatusBarWidget) new;
    (void)request; (void)args; (void)num_args;

    sw->statusBar.h_space = ISWScaleDim(new, 4);

    /* Default height so MainWindow can lay us out before children arrive */
    if (sw->core.height == 0)
        sw->core.height = ISWScaleDim(new, 20);
}

static void
InsertChild(Widget child)
{
    (*constraintClassRec.composite_class.insert_child)(child);

    /* Style Label children for flat appearance */
    if (XtIsSubclass(child, labelWidgetClass)) {
        Arg args[1];
        XtSetArg(args[0], XtNborderWidth, 0);
        XtSetValues(child, args, 1);
    }
}

static void
Resize(Widget w)
{
    DoLayout((StatusBarWidget) w);
}

static void
ChangeManaged(Widget w)
{
    DoLayout((StatusBarWidget) w);
}

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
    (void)child; (void)request; (void)reply;
    /* Deny child resize requests — layout controls sizing */
    DoLayout((StatusBarWidget) XtParent(child));
    return XtGeometryNo;
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    (void)event; (void)region;

    if (!XtIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    /* Draw top separator line */
    ISWRenderContext *ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx) {
        ISWRenderBegin(ctx);
        ISWRenderSetColor(ctx, w->core.border_pixel);
        ISWRenderDrawLine(ctx, 0, 0, (int)w->core.width, 0);
        ISWRenderEnd(ctx);
        ISWRenderDestroy(ctx);
    }
}
