/*
 * Tabs.c - Tabbed container widget
 *
 * A Constraint widget that displays a horizontal tab bar at the top.
 * Each managed child gets a tab; clicking a tab shows that child
 * and hides the others.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/TabsP.h>
#include <xcb/xcb.h>
#include <cairo/cairo.h>
#include <math.h>
#include <string.h>
#include "ISWXcbDraw.h"

#define TabInfo(w) ((TabsConstraints)(w)->core.constraints)

#define TAB_H_PAD  10
#define TAB_V_PAD  1

#define ForAllChildren(tw, childP) \
  for ((childP) = (tw)->composite.children; \
       (childP) < (tw)->composite.children + (tw)->composite.num_children; \
       (childP)++)

/****************************************************************
 * Translations and actions
 ****************************************************************/

static char defaultTranslations[] =
    "<Btn1Down>: TabSelect()";

static void TabSelect(Widget, XEvent *, String *, Cardinal *);

static XtActionsRec actionsList[] = {
    {"TabSelect", TabSelect},
};

/****************************************************************
 * Resources
 ****************************************************************/

#define offset(field) XtOffsetOf(TabsRec, tabs.field)
static XtResource resources[] = {
    {XtNtabCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
         offset(tab_callbacks), XtRCallback, (XtPointer)NULL},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
         offset(font), XtRString, XtDefaultFont},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
         offset(foreground), XtRString, XtDefaultForeground},
};
#undef offset

#define offset(field) XtOffsetOf(TabsConstraintsRec, tabs.field)
static XtResource subresources[] = {
    {XtNtabLabel, XtCTabLabel, XtRString, sizeof(String),
         offset(tab_label), XtRString, (XtPointer)NULL},
};
#undef offset

/****************************************************************
 * Method declarations
 ****************************************************************/

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static void ChangeManaged(Widget);
static void ConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void ConstraintDestroy(Widget);

static void LayoutChildren(TabsWidget);
static Dimension TabBarHeight(TabsWidget);

/****************************************************************
 * Class record
 ****************************************************************/

#define SuperClass ((ConstraintWidgetClass)&constraintClassRec)

TabsClassRec tabsClassRec = {
   {
/* core class fields */
    /* superclass         */   (WidgetClass) SuperClass,
    /* class name         */   "Tabs",
    /* size               */   sizeof(TabsRec),
    /* class_initialize   */   ClassInitialize,
    /* class_part init    */   NULL,
    /* class_inited       */   FALSE,
    /* initialize         */   Initialize,
    /* initialize_hook    */   NULL,
    /* realize            */   Realize,
    /* actions            */   actionsList,
    /* num_actions        */   XtNumber(actionsList),
    /* resources          */   resources,
    /* resource_count     */   XtNumber(resources),
    /* xrm_class          */   NULLQUARK,
    /* compress_motion    */   TRUE,
    /* compress_exposure  */   TRUE,
    /* compress_enterleave*/   TRUE,
    /* visible_interest   */   FALSE,
    /* destroy            */   Destroy,
    /* resize             */   Resize,
    /* expose             */   Redisplay,
    /* set_values         */   SetValues,
    /* set_values_hook    */   NULL,
    /* set_values_almost  */   XtInheritSetValuesAlmost,
    /* get_values_hook    */   NULL,
    /* accept_focus       */   NULL,
    /* version            */   XtVersion,
    /* callback_private   */   NULL,
    /* tm_table           */   defaultTranslations,
    /* query_geometry     */   XtInheritQueryGeometry,
    /* display_accelerator*/   XtInheritDisplayAccelerator,
    /* extension          */   NULL
   }, {
/* composite class fields */
    /* geometry_manager   */   GeometryManager,
    /* change_managed     */   ChangeManaged,
    /* insert_child       */   XtInheritInsertChild,
    /* delete_child       */   XtInheritDeleteChild,
    /* extension          */   NULL
   }, {
/* constraint class fields */
    /* subresources       */   subresources,
    /* subresource_count  */   XtNumber(subresources),
    /* constraint_size    */   sizeof(TabsConstraintsRec),
    /* initialize         */   ConstraintInitialize,
    /* destroy            */   ConstraintDestroy,
    /* set_values         */   ConstraintSetValues,
    /* extension          */   NULL
   }, {
/* tabs class fields */
    /* empty              */   0
   }
};

WidgetClass tabsWidgetClass = (WidgetClass) &tabsClassRec;

/****************************************************************
 * Private functions
 ****************************************************************/

static Dimension
TabBarHeight(TabsWidget tw)
{
    return ISWScaledFontHeight((Widget)tw, tw->tabs.font)
           + ISWScaleDim((Widget)tw, TAB_V_PAD * 2);
}

static void
LayoutChildren(TabsWidget tw)
{
    Widget *childP;
    Position x = 0;
    Dimension tab_h = TabBarHeight(tw);
    Dimension content_h = tw->core.height > tab_h ?
                          tw->core.height - tab_h : 0;

    /* Compute tab positions */
    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!XtIsManaged(child)) continue;
        TabsConstraints tc = TabInfo(child);
        String label = tc->tabs.tab_label ? tc->tabs.tab_label : XtName(child);
        int text_w = ISWScaledTextWidth((Widget)tw, tw->tabs.font,
                                        label, strlen(label));
        tc->tabs.tab_width = text_w + ISWScaleDim((Widget)tw, TAB_H_PAD * 2);
        tc->tabs.tab_x = x;
        x += tc->tabs.tab_width;
    }

    /* Position children: only top_widget is mapped */
    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!XtIsManaged(child)) continue;

        XtConfigureWidget(child, 0, tab_h,
                          tw->core.width, content_h,
                          child->core.border_width);

        if (child == tw->tabs.top_widget) {
            if (XtIsRealized(child)) {
                XtMapWidget(child);
            }
        } else {
            if (XtIsRealized(child)) {
                XtUnmapWidget(child);
            }
        }
    }
}

static void
DrawTabBar(Widget w)
{
    TabsWidget tw = (TabsWidget)w;
    ISWRenderContext *ctx = tw->tabs.render_ctx;
    if (!ctx) return;

    Dimension tab_h = TabBarHeight(tw);
    Widget *childP;

    ISWRenderBegin(ctx);

    /* Clear tab bar background */
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, tab_h);

    cairo_t *cr = (cairo_t *)ISWRenderGetCairoContext(ctx);

    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!XtIsManaged(child)) continue;
        TabsConstraints tc = TabInfo(child);
        String label = tc->tabs.tab_label ? tc->tabs.tab_label : XtName(child);
        Boolean is_top = (child == tw->tabs.top_widget);

        Position tx = tc->tabs.tab_x;
        Dimension tw_ = tc->tabs.tab_width;

        if (cr) {
            double r = 4.0 * ISWScaleFactor(w);
            cairo_save(cr);
            cairo_new_path(cr);

            /* Rounded top corners, flat bottom */
            cairo_arc(cr, tx + r, r, r, M_PI, 3*M_PI/2);
            cairo_arc(cr, tx + tw_ - r, r, r, -M_PI/2, 0);
            cairo_line_to(cr, tx + tw_, tab_h);
            cairo_line_to(cr, tx, tab_h);
            cairo_close_path(cr);

            if (is_top) {
                ISWRenderSetColor(ctx, w->core.background_pixel);
                cairo_fill_preserve(cr);
            } else {
                ISWRenderSetColorRGBA(ctx, 0.0, 0.0, 0.0, 0.06);
                cairo_fill_preserve(cr);
            }

            ISWRenderSetColor(ctx, tw->tabs.foreground);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            cairo_restore(cr);
        }

        /* Draw tab label */
        ISWRenderSetFont(ctx, tw->tabs.font);
        ISWRenderSetColor(ctx, tw->tabs.foreground);
        int text_y = ISWScaledFontAscent(w, tw->tabs.font)
                     + ISWScaleDim(w, TAB_V_PAD);
        int text_x = tx + ISWScaleDim(w, TAB_H_PAD);
        ISWRenderDrawString(ctx, label, strlen(label), text_x, text_y);
    }

    /* Draw bottom border across the full width */
    if (cr) {
        cairo_save(cr);
        ISWRenderSetColor(ctx, tw->tabs.foreground);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, tab_h - 0.5);
        cairo_line_to(cr, tw->core.width, tab_h - 0.5);

        /* Cut gap under the active tab */
        if (tw->tabs.top_widget && XtIsManaged(tw->tabs.top_widget)) {
            TabsConstraints tc = TabInfo(tw->tabs.top_widget);
            /* Overdraw the gap with background */
            cairo_stroke(cr);
            ISWRenderSetColor(ctx, w->core.background_pixel);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, tc->tabs.tab_x + 1, tab_h - 0.5);
            cairo_line_to(cr, tc->tabs.tab_x + tc->tabs.tab_width - 1,
                          tab_h - 0.5);
        }
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    ISWRenderEnd(ctx);
}

/****************************************************************
 * Core methods
 ****************************************************************/

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TabsWidget tw = (TabsWidget)new;

    tw->tabs.top_widget = NULL;
    tw->tabs.render_ctx = NULL;

    if (tw->core.width == 0)
        tw->core.width = ISWScaleDim(new, 200);
    if (tw->core.height == 0)
        tw->core.height = TabBarHeight(tw) + ISWScaleDim(new, 100);
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    TabsWidget tw = (TabsWidget)w;

    (*SuperClass->core_class.realize)(dpy, w, valueMask, attributes);

    tw->tabs.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);

    /* Now that we're realized, ensure correct mapping state */
    LayoutChildren(tw);
}

static void
Resize(Widget w)
{
    TabsWidget tw = (TabsWidget)w;
    ISWRenderContext *old = tw->tabs.render_ctx;

    if (old) {
        ISWRenderDestroy(old);
        tw->tabs.render_ctx = NULL;
    }
    if (XtIsRealized(w) && w->core.width > 0 && w->core.height > 0) {
        tw->tabs.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }

    LayoutChildren(tw);
}

/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    TabsWidget tw = (TabsWidget)w;

    if (!tw->tabs.render_ctx && XtIsRealized(w) &&
        w->core.width > 0 && w->core.height > 0) {
        tw->tabs.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }

    DrawTabBar(w);
}

static void
Destroy(Widget w)
{
    TabsWidget tw = (TabsWidget)w;
    if (tw->tabs.render_ctx)
        ISWRenderDestroy(tw->tabs.render_ctx);
}

/* ARGSUSED */
static Boolean
SetValues(Widget old, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TabsWidget oldtw = (TabsWidget)old;
    TabsWidget newtw = (TabsWidget)new;
    Boolean redisplay = False;

    if (oldtw->tabs.font != newtw->tabs.font ||
        oldtw->tabs.foreground != newtw->tabs.foreground ||
        oldtw->core.background_pixel != newtw->core.background_pixel) {
        redisplay = True;
    }

    return redisplay;
}

/****************************************************************
 * Composite/Constraint methods
 ****************************************************************/

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
    TabsWidget tw = (TabsWidget)XtParent(child);
    Dimension tab_h = TabBarHeight(tw);
    Dimension content_h = tw->core.height > tab_h ?
                          tw->core.height - tab_h : 0;

    XtConfigureWidget(child, 0, tab_h,
                      tw->core.width, content_h,
                      child->core.border_width);
    return XtGeometryDone;
}

static void
ChangeManaged(Widget w)
{
    TabsWidget tw = (TabsWidget)w;
    Widget *childP;

    /* If top_widget is no longer managed, pick a new one */
    if (tw->tabs.top_widget && !XtIsManaged(tw->tabs.top_widget))
        tw->tabs.top_widget = NULL;

    /* If no top widget, pick the first managed child */
    if (tw->tabs.top_widget == NULL) {
        ForAllChildren(tw, childP) {
            if (XtIsManaged(*childP)) {
                tw->tabs.top_widget = *childP;
                break;
            }
        }
    }

    LayoutChildren(tw);
    if (XtIsRealized(w)) {
        DrawTabBar(w);
    }
}

/* ARGSUSED */
static void
ConstraintInitialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TabsConstraints tc = TabInfo(new);

    if (tc->tabs.tab_label)
        tc->tabs.tab_label = XtNewString(tc->tabs.tab_label);
    tc->tabs.tab_width = 0;
    tc->tabs.tab_x = 0;
}

static void
ConstraintDestroy(Widget w)
{
    TabsConstraints tc = TabInfo(w);
    if (tc->tabs.tab_label)
        XtFree((char *)tc->tabs.tab_label);
}

/* ARGSUSED */
static Boolean
ConstraintSetValues(Widget old, Widget request, Widget new,
                    ArgList args, Cardinal *num_args)
{
    TabsConstraints old_tc = TabInfo(old);
    TabsConstraints new_tc = TabInfo(new);

    if (old_tc->tabs.tab_label != new_tc->tabs.tab_label) {
        if (old_tc->tabs.tab_label)
            XtFree((char *)old_tc->tabs.tab_label);
        if (new_tc->tabs.tab_label)
            new_tc->tabs.tab_label = XtNewString(new_tc->tabs.tab_label);

        TabsWidget tw = (TabsWidget)XtParent(new);
        LayoutChildren(tw);
        if (XtIsRealized((Widget)tw)) {
            DrawTabBar((Widget)tw);
        }
    }
    return False;
}

/****************************************************************
 * Action procedures
 ****************************************************************/

/* ARGSUSED */
static void
TabSelect(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    TabsWidget tw = (TabsWidget)w;
    xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
    int click_x = bev->event_x;
    int click_y = bev->event_y;
    Dimension tab_h = TabBarHeight(tw);
    Widget *childP;

    /* Only respond to clicks in the tab bar area */
    if (click_y > (int)tab_h) return;

    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!XtIsManaged(child)) continue;
        TabsConstraints tc = TabInfo(child);
        if (click_x >= tc->tabs.tab_x &&
            click_x < tc->tabs.tab_x + (int)tc->tabs.tab_width) {
            IswTabsSetTop(w, child);
            return;
        }
    }
}

/****************************************************************
 * Public API
 ****************************************************************/

void
IswTabsSetTop(Widget w, Widget child)
{
    TabsWidget tw = (TabsWidget)w;
    Widget *childP;
    int index = 0;

    if (tw->tabs.top_widget == child) return;

    Widget old_top = tw->tabs.top_widget;
    tw->tabs.top_widget = child;

    /* Map/unmap as needed */
    if (old_top && XtIsRealized(old_top)) {
        XtUnmapWidget(old_top);
    }

    if (child && XtIsRealized(child)) {
        Dimension tab_h = TabBarHeight(tw);
        Dimension content_h = tw->core.height > tab_h ?
                              tw->core.height - tab_h : 0;
        XtConfigureWidget(child, 0, tab_h,
                          tw->core.width, content_h,
                          child->core.border_width);
        XtMapWidget(child);
    }

    /* Redraw tab bar */
    if (XtIsRealized(w)) {
        DrawTabBar(w);
    }

    /* Compute index and fire callback */
    ForAllChildren(tw, childP) {
        if (!XtIsManaged(*childP)) continue;
        if (*childP == child) break;
        index++;
    }

    TabsCallbackStruct cbs;
    cbs.child = child;
    cbs.tab_index = index;
    XtCallCallbackList(w, tw->tabs.tab_callbacks, (XtPointer)&cbs);
}
