/*
 * Toolbar.c - Toolbar widget implementation
 *
 * A horizontal (or vertical) container for buttons and controls.
 * Subclasses Box with tight spacing and consistent child styling.
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

#define superclass (&boxClassRec)

#define Offset(field) XtOffsetOf(ToolbarRec, field)

static XtResource resources[] = {
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
};

#undef Offset

/* Forward declarations */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void InsertChild(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);

ToolbarClassRec toolbarClassRec = {
  { /* core */
    (WidgetClass) superclass,           /* superclass             */
    "Toolbar",                          /* class_name             */
    sizeof(ToolbarRec),                 /* size                   */
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
    XtInheritResize,                    /* resize                 */
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
    XtInheritGeometryManager,           /* geometry_manager       */
    XtInheritChangeManaged,             /* change_managed         */
    InsertChild,                        /* insert_child           */
    XtInheritDeleteChild,               /* delete_child           */
    NULL                                /* extension              */
  },
  { /* box */
    0                                   /* empty                  */
  },
  { /* toolbar */
    0                                   /* empty                  */
  }
};

WidgetClass toolbarWidgetClass = (WidgetClass)&toolbarClassRec;

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ToolbarWidget tw = (ToolbarWidget) new;
    (void)request; (void)args; (void)num_args;

    /* Force horizontal layout with tight spacing */
    tw->box.orientation = XtorientHorizontal;
    tw->box.h_space = ISWScaleDim(new, 2);
    tw->box.v_space = ISWScaleDim(new, 2);
}

static void
InsertChild(Widget child)
{
    /* Call Box's insert_child */
    (*boxClassRec.composite_class.insert_child)(child);

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

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    (void)event; (void)region;

    if (!XtIsRealized(w) || w->core.width == 0 || w->core.height == 0)
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
