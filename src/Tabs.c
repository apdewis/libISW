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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/TabsP.h>
#include <math.h>
#include <string.h>

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
    "<PrimaryDown>: TabSelect()";

static void TabSelect(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actionsList[] = {
    {"TabSelect", TabSelect},
};

/****************************************************************
 * Resources
 ****************************************************************/

#define offset(field) IswOffsetOf(TabsRec, tabs.field)
static IswResource resources[] = {
    {IswNtabCallback, IswCCallback, IswRCallback, sizeof(IswCallbackList),
         offset(tab_callbacks), IswRCallback, (IswPointer)NULL},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
         offset(font), IswRString, IswDefaultFont},
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
         offset(foreground), IswRString, IswDefaultForeground},
    {IswNtabHeight, IswCTabHeight, IswRDimension, sizeof(Dimension),
         offset(tab_height), IswRImmediate, (IswPointer)0},
    {IswNtabSizing, IswCTabSizing, IswRTabSizing, sizeof(IswTabSizing),
         offset(tab_sizing), IswRImmediate, (IswPointer)IswTabSizingText},
    {IswNtabBackground, IswCTabBackground, IswRPixel, sizeof(Pixel),
         offset(tab_background), IswRString, IswDefaultBackground},
    {IswNactiveTabColor, IswCActiveTabColor, IswRPixel, sizeof(Pixel),
         offset(active_tab_color), IswRString, IswDefaultBackground},
    {IswNtabBorderColor, IswCTabBorderColor, IswRPixel, sizeof(Pixel),
         offset(tab_border_color), IswRString, IswDefaultForeground},
    {IswNcornerRadius, IswCCornerRadius, IswRDimension, sizeof(Dimension),
         IswOffsetOf(TabsRec, core.corner_radius), IswRImmediate, (IswPointer)0},
};
#undef offset

#define offset(field) IswOffsetOf(TabsConstraintsRec, tabs.field)
static IswResource subresources[] = {
    {IswNtabLabel, IswCTabLabel, IswRString, sizeof(String),
         offset(tab_label), IswRString, (IswPointer)NULL},
};
#undef offset

/****************************************************************
 * Method declarations
 ****************************************************************/

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void ChangeManaged(Widget);
static void ConstraintInitialize(Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void ConstraintDestroy(Widget);

static void LayoutChildren(TabsWidget);
static Dimension TabBarHeight(TabsWidget);
static Boolean CvtStringToTabSizing(IswDisplay, IswValuePtr, Cardinal *,
                                     IswValuePtr, IswValuePtr, IswPointer *);

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
    /* num_actions        */   IswNumber(actionsList),
    /* resources          */   resources,
    /* resource_count     */   IswNumber(resources),
    /* xrm_class          */   ISW_NULLQUARK,
    /* compress_motion    */   TRUE,
    /* compress_exposure  */   TRUE,
    /* compress_enterleave*/   TRUE,
    /* visible_interest   */   FALSE,
    /* destroy            */   Destroy,
    /* resize             */   Resize,
    /* expose             */   Redisplay,
    /* set_values         */   SetValues,
    /* set_values_hook    */   NULL,
    /* set_values_almost  */   IswInheritSetValuesAlmost,
    /* get_values_hook    */   NULL,
    /* accept_focus       */   NULL,
    /* version            */   IswVersion,
    /* callback_private   */   NULL,
    /* tm_table           */   defaultTranslations,
    /* query_geometry     */   IswInheritQueryGeometry,
    /* display_accelerator*/   IswInheritDisplayAccelerator,
    /* extension          */   NULL
   }, {
/* composite class fields */
    /* geometry_manager   */   GeometryManager,
    /* change_managed     */   ChangeManaged,
    /* insert_child       */   IswInheritInsertChild,
    /* delete_child       */   IswInheritDeleteChild,
    /* extension          */   NULL
   }, {
/* constraint class fields */
    /* subresources       */   subresources,
    /* subresource_count  */   IswNumber(subresources),
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
    if (tw->tabs.tab_height > 0)
        return tw->tabs.tab_height;
    return ISWScaledFontHeight((Widget)tw, tw->tabs.font)
           + (TAB_V_PAD * 2);
}

static void
LayoutChildren(TabsWidget tw)
{
    Widget *childP;
    Position x = 0;
    Dimension tab_h = TabBarHeight(tw);
    Dimension bw = tw->tabs.border_w;
    Dimension content_h = tw->core.height > tab_h + bw ?
                          tw->core.height - tab_h - bw : 0;
    Dimension content_w = tw->core.width > bw * 2 ?
                          tw->core.width - bw * 2 : 0;

    /* Compute tab positions */
    if (tw->tabs.tab_sizing == IswTabSizingFill) {
        Cardinal n_managed = 0;
        ForAllChildren(tw, childP)
            if (IswIsManaged(*childP)) n_managed++;
        Dimension each = n_managed > 0 ? tw->core.width / n_managed : 0;
        Dimension remainder = n_managed > 0 ? tw->core.width % n_managed : 0;
        ForAllChildren(tw, childP) {
            Widget child = *childP;
            if (!IswIsManaged(child)) continue;
            TabsConstraints tc = TabInfo(child);
            tc->tabs.tab_width = each + (remainder > 0 ? 1 : 0);
            if (remainder > 0) remainder--;
            tc->tabs.tab_x = x;
            x += tc->tabs.tab_width;
        }
    } else {
        ForAllChildren(tw, childP) {
            Widget child = *childP;
            if (!IswIsManaged(child)) continue;
            TabsConstraints tc = TabInfo(child);
            String label = tc->tabs.tab_label ? tc->tabs.tab_label : IswName(child);
            int text_w = ISWScaledTextWidth((Widget)tw, tw->tabs.font,
                                            label, strlen(label));
            tc->tabs.tab_width = text_w + (TAB_H_PAD * 2);
            tc->tabs.tab_x = x;
            x += tc->tabs.tab_width;
        }
    }

    /* Position children: only top_widget is mapped */
    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!IswIsManaged(child)) continue;

        IswConfigureWidget(child, bw, tab_h,
                          content_w, content_h,
                          child->core.border_width);

        if (child == tw->tabs.top_widget) {
            if (IswIsRealized(child))
                IswMapWidget(child);
            else if (!IswIsShell(child))
                child->core.mapped_when_managed = True;
        } else {
            /* Non-top page: must start hidden.  When already realized, unmap
               (toggles the windowless shown flag + recomposites).  When not yet
               realized, clear mapped_when_managed — the realize-time map pass
               (RealizeWidget) maps every managed child whose mapped_when_managed
               is set, so leaving it True would composite every page on init. */
            if (IswIsRealized(child))
                IswUnmapWidget(child);
            else if (!IswIsShell(child))
                child->core.mapped_when_managed = False;
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

    Dimension bw = tw->tabs.border_w;
    double lw = (double)bw;
    double half = lw / 2.0;

    ISWRenderBegin(ctx);

    _IswCoreDrawBackground(w, ctx);

    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!IswIsManaged(child)) continue;
        TabsConstraints tc = TabInfo(child);
        String label = tc->tabs.tab_label ? tc->tabs.tab_label : IswName(child);
        Boolean is_top = (child == tw->tabs.top_widget);

        Position tx = tc->tabs.tab_x;
        Dimension tw_ = tc->tabs.tab_width;

        double r = (double)tw->core.corner_radius;
        Boolean is_first = (tx == 0);
        Boolean is_last = (tx + tw_ >= (Position)tw->core.width);
        double x0 = is_first ? half : (double)tx;
        double x1 = is_last ? tw_ + tx - half : (double)(tx + tw_);
        double y0 = half;
        double y1 = tab_h;
        ISWRenderSave(ctx);
        ISWRenderPathBegin(ctx);

        if (r > 0) {
            ISWRenderPathArc(ctx, x0 + r, y0 + r, r, M_PI, 3*M_PI/2);
            ISWRenderPathArc(ctx, x1 - r, y0 + r, r, -M_PI/2, 0);
        } else {
            ISWRenderPathMoveTo(ctx, x0, y0);
            ISWRenderPathLineTo(ctx, x1, y0);
        }
        ISWRenderPathLineTo(ctx, x1, y1);
        ISWRenderPathLineTo(ctx, x0, y1);
        ISWRenderPathClose(ctx);

        if (is_top)
            ISWRenderSetColor(ctx, tw->tabs.active_tab_color);
        else
            ISWRenderSetColor(ctx, tw->tabs.tab_background);

        if (bw > 0) {
            ISWRenderFillPreserve(ctx);
            ISWRenderSetColor(ctx, tw->tabs.tab_border_color);
            ISWRenderSetLineWidth(ctx, lw);
            ISWRenderStroke(ctx);
        } else {
            ISWRenderFill(ctx);
        }

        ISWRenderRestore(ctx);

        /* Draw tab label */
        ISWRenderSetFont(ctx, tw->tabs.font);
        ISWRenderSetColor(ctx, tw->tabs.foreground);
        int label_len = strlen(label);
        int text_w = ISWScaledTextWidth(w, tw->tabs.font, label, label_len);
        int text_y = ISWScaledFontAscent(w, tw->tabs.font) + TAB_V_PAD;
        int text_x;
        if (tw->tabs.tab_height > 0)
            text_y = (tab_h - ISWScaledFontHeight(w, tw->tabs.font)) / 2
                     + ISWScaledFontAscent(w, tw->tabs.font);
        if (tw->tabs.tab_sizing == IswTabSizingFill)
            text_x = tx + (tw_ - text_w) / 2;
        else
            text_x = tx + TAB_H_PAD;
        ISWRenderDrawString(ctx, label, label_len, text_x, text_y);
    }

    /* Draw content box border connected to the active tab */
    if (bw > 0) {
        Position active_x = 0;
        Dimension active_w = 0;
        if (tw->tabs.top_widget && IswIsManaged(tw->tabs.top_widget)) {
            TabsConstraints tc = TabInfo(tw->tabs.top_widget);
            active_x = tc->tabs.tab_x;
            active_w = tc->tabs.tab_width;
        }

        double r = (double)tw->core.corner_radius;
        double right = w->core.width - half;
        double bottom = w->core.height - half;

        ISWRenderSave(ctx);
        ISWRenderSetColor(ctx, tw->tabs.tab_border_color);
        ISWRenderSetLineWidth(ctx, lw);
        ISWRenderPathBegin(ctx);

        /* Top edge: left of active tab */
        ISWRenderPathMoveTo(ctx, half, tab_h);
        if (active_w > 0) {
            ISWRenderPathLineTo(ctx, active_x, tab_h);
            ISWRenderPathMoveTo(ctx, active_x + active_w, tab_h);
        }
        ISWRenderPathLineTo(ctx, right, tab_h);

        /* Right edge → bottom-right corner */
        if (r > 0) {
            ISWRenderPathLineTo(ctx, right, bottom - r);
            ISWRenderPathArc(ctx, right - r, bottom - r, r, 0, M_PI/2);
        } else {
            ISWRenderPathLineTo(ctx, right, bottom);
        }

        /* Bottom edge → bottom-left corner */
        if (r > 0) {
            ISWRenderPathLineTo(ctx, half + r, bottom);
            ISWRenderPathArc(ctx, half + r, bottom - r, r, M_PI/2, M_PI);
        } else {
            ISWRenderPathLineTo(ctx, half, bottom);
        }

        /* Left edge */
        ISWRenderPathLineTo(ctx, half, tab_h);

        ISWRenderStroke(ctx);
        ISWRenderRestore(ctx);
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
    IswSetTypeConverter(IswRString, IswRTabSizing, CvtStringToTabSizing,
                        (IswConvertArgList)NULL, 0,
                        IswCacheNone, (IswDestructor)NULL);
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TabsWidget tw = (TabsWidget)new;

    tw->tabs.top_widget = NULL;
    tw->tabs.render_ctx = NULL;
    tw->tabs.border_w = tw->core.border_width;
    tw->core.border_width = 0;

    if (tw->core.width == 0)
        tw->core.width = (200);
    if (tw->core.height == 0)
        tw->core.height = TabBarHeight(tw) + (100);
}

static void
Realize(IswDisplay dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
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
    if (IswIsRealized(w) && w->core.width > 0 && w->core.height > 0) {
        tw->tabs.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }

    LayoutChildren(tw);
}

/* ARGSUSED */
static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    TabsWidget tw = (TabsWidget)w;

    if (!tw->tabs.render_ctx && IswIsRealized(w) &&
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

    if (newtw->core.border_width != oldtw->core.border_width) {
        newtw->tabs.border_w = newtw->core.border_width;
        newtw->core.border_width = 0;
    }

    if (oldtw->tabs.font != newtw->tabs.font ||
        oldtw->tabs.foreground != newtw->tabs.foreground ||
        oldtw->core.background_pixel != newtw->core.background_pixel ||
        oldtw->tabs.tab_height != newtw->tabs.tab_height ||
        oldtw->tabs.tab_sizing != newtw->tabs.tab_sizing ||
        oldtw->tabs.tab_background != newtw->tabs.tab_background ||
        oldtw->tabs.active_tab_color != newtw->tabs.active_tab_color ||
        oldtw->tabs.tab_border_color != newtw->tabs.tab_border_color ||
        oldtw->tabs.border_w != newtw->tabs.border_w ||
        oldtw->core.corner_radius != newtw->core.corner_radius) {
        LayoutChildren(newtw);
        redisplay = True;
    }

    return redisplay;
}

/****************************************************************
 * Composite/Constraint methods
 ****************************************************************/

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    TabsWidget tw = (TabsWidget)IswParent(child);
    Dimension tab_h = TabBarHeight(tw);
    Dimension bw = tw->tabs.border_w;
    Dimension content_h = tw->core.height > tab_h + bw ?
                          tw->core.height - tab_h - bw : 0;
    Dimension content_w = tw->core.width > bw * 2 ?
                          tw->core.width - bw * 2 : 0;

    IswConfigureWidget(child, bw, tab_h,
                      content_w, content_h,
                      child->core.border_width);
    return IswGeometryDone;
}

static void
ChangeManaged(Widget w)
{
    TabsWidget tw = (TabsWidget)w;
    Widget *childP;

    /* If top_widget is no longer managed, pick a new one */
    if (tw->tabs.top_widget && !IswIsManaged(tw->tabs.top_widget))
        tw->tabs.top_widget = NULL;

    /* If no top widget, pick the first managed child */
    if (tw->tabs.top_widget == NULL) {
        ForAllChildren(tw, childP) {
            if (IswIsManaged(*childP)) {
                tw->tabs.top_widget = *childP;
                break;
            }
        }
    }

    LayoutChildren(tw);
    if (IswIsRealized(w)) {
        DrawTabBar(w);
    }
}

/* ARGSUSED */
static void
ConstraintInitialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    TabsConstraints tc = TabInfo(new);

    if (tc->tabs.tab_label)
        tc->tabs.tab_label = IswNewString(tc->tabs.tab_label);
    tc->tabs.tab_width = 0;
    tc->tabs.tab_x = 0;
}

static void
ConstraintDestroy(Widget w)
{
    TabsConstraints tc = TabInfo(w);
    if (tc->tabs.tab_label)
        IswFree((char *)tc->tabs.tab_label);
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
            IswFree((char *)old_tc->tabs.tab_label);
        if (new_tc->tabs.tab_label)
            new_tc->tabs.tab_label = IswNewString(new_tc->tabs.tab_label);

        TabsWidget tw = (TabsWidget)IswParent(new);
        LayoutChildren(tw);
        if (IswIsRealized((Widget)tw)) {
            DrawTabBar((Widget)tw);
        }
    }
    return False;
}

/****************************************************************
 * Type converter
 ****************************************************************/

static Boolean
CvtStringToTabSizing(IswDisplay display, IswValuePtr args, Cardinal *num_args,
                     IswValuePtr from, IswValuePtr to, IswPointer *converter_data)
{
    static IswTabSizing sizing;
    char lowerName[64];
    const char *str = (const char *)from->addr;

    (void)display; (void)args; (void)num_args; (void)converter_data;

    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;

    ISWCopyISOLatin1Lowered(lowerName, str);

    if (strcmp(lowerName, "text") == 0)
        sizing = IswTabSizingText;
    else if (strcmp(lowerName, "fill") == 0)
        sizing = IswTabSizingFill;
    else
        return False;

    if (to->addr == NULL) {
        to->addr = (IswPointer)&sizing;
    } else if (to->size < sizeof(IswTabSizing)) {
        to->size = sizeof(IswTabSizing);
        return False;
    } else {
        *(IswTabSizing *)to->addr = sizing;
    }
    to->size = sizeof(IswTabSizing);

    return True;
}

/****************************************************************
 * Action procedures
 ****************************************************************/

/* ARGSUSED */
static void
TabSelect(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    TabsWidget tw = (TabsWidget)w;
    int click_x = IswEventX(iswev);
    int click_y = IswEventY(iswev);
    Dimension tab_h = TabBarHeight(tw);
    Widget *childP;

    /* Only respond to clicks in the tab bar area */
    if (click_y > (int)tab_h) return;

    ForAllChildren(tw, childP) {
        Widget child = *childP;
        if (!IswIsManaged(child)) continue;
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
    if (old_top && IswIsRealized(old_top)) {
        IswUnmapWidget(old_top);
    }

    if (child && IswIsRealized(child)) {
        Dimension tab_h = TabBarHeight(tw);
        Dimension bw = tw->tabs.border_w;
        Dimension content_h = tw->core.height > tab_h + bw ?
                              tw->core.height - tab_h - bw : 0;
        Dimension content_w = tw->core.width > bw * 2 ?
                              tw->core.width - bw * 2 : 0;
        IswConfigureWidget(child, bw, tab_h,
                          content_w, content_h,
                          child->core.border_width);
        IswMapWidget(child);
    }

    /* Redraw tab bar */
    if (IswIsRealized(w)) {
        DrawTabBar(w);
    }

    /* Compute index and fire callback */
    ForAllChildren(tw, childP) {
        if (!IswIsManaged(*childP)) continue;
        if (*childP == child) break;
        index++;
    }

    TabsCallbackStruct cbs;
    cbs.child = child;
    cbs.tab_index = index;
    IswCallCallbackList(w, tw->tabs.tab_callbacks, (IswPointer)&cbs);
}
