/*
 * Copyright (c) 2024-2026 ISW Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/FlexBoxP.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

/* --- Forward declarations --- */

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static IswGeometryResult PreferredGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void ChangeManaged(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void DoLayout(FlexBoxWidget fw, Boolean set_children);

/* --- Resources --- */

static IswFlexAlign defAlign = XtflexAlignStretch;

#define Offset(field) IswOffsetOf(FlexBoxRec, flexBox.field)
static IswResource resources[] = {
    {IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
     Offset(orientation), IswRImmediate, (IswPointer)XtorientVertical},
    {IswNspacing, IswCSpacing, IswRDimension, sizeof(Dimension),
     Offset(spacing), IswRImmediate, (IswPointer)0},
};
#undef Offset

#define COffset(field) IswOffsetOf(FlexBoxConstraintsRec, flexBox.field)
static IswResource constraintResources[] = {
    {IswNflexGrow, IswCFlexGrow, IswRInt, sizeof(int),
     COffset(flex_grow), IswRImmediate, (IswPointer)0},
    {IswNflexBasis, IswCFlexBasis, IswRDimension, sizeof(Dimension),
     COffset(flex_basis), IswRImmediate, (IswPointer)0},
    {IswNflexAlign, IswCFlexAlign, IswRFlexAlign, sizeof(IswFlexAlign),
     COffset(flex_align), IswRFlexAlign, (IswPointer)&defAlign},
};
#undef COffset

/* --- Class record --- */

FlexBoxClassRec flexBoxClassRec = {
  { /* core_class */
    /* superclass         */ (WidgetClass) &constraintClassRec,
    /* class_name         */ "FlexBox",
    /* widget_size        */ sizeof(FlexBoxRec),
    /* class_initialize   */ ClassInitialize,
    /* class_part_init    */ NULL,
    /* class_inited       */ FALSE,
    /* initialize         */ Initialize,
    /* initialize_hook    */ NULL,
    /* realize            */ IswInheritRealize,
    /* actions            */ NULL,
    /* num_actions        */ 0,
    /* resources          */ resources,
    /* num_resources      */ IswNumber(resources),
    /* xrm_class          */ NULLQUARK,
    /* compress_motion    */ TRUE,
    /* compress_exposure  */ TRUE,
    /* compress_enterleave*/ TRUE,
    /* visible_interest   */ FALSE,
    /* destroy            */ NULL,
    /* resize             */ Resize,
    /* expose             */ NULL,
    /* set_values         */ SetValues,
    /* set_values_hook    */ NULL,
    /* set_values_almost  */ IswInheritSetValuesAlmost,
    /* get_values_hook    */ NULL,
    /* accept_focus       */ NULL,
    /* version            */ IswVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ NULL,
    /* query_geometry     */ PreferredGeometry,
    /* display_accelerator*/ IswInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* composite_class */
    /* geometry_manager   */ GeometryManager,
    /* change_managed     */ ChangeManaged,
    /* insert_child       */ IswInheritInsertChild,
    /* delete_child       */ IswInheritDeleteChild,
    /* extension          */ NULL
  },
  { /* constraint_class */
    /* subresources       */ constraintResources,
    /* subresource_count  */ IswNumber(constraintResources),
    /* constraint_size    */ sizeof(FlexBoxConstraintsRec),
    /* initialize         */ NULL,
    /* destroy            */ NULL,
    /* set_values         */ ConstraintSetValues,
    /* extension          */ NULL
  },
  { /* flexBox_class */
    /* extension          */ NULL
  }
};

WidgetClass flexBoxWidgetClass = (WidgetClass)&flexBoxClassRec;

/* --- String-to-FlexAlign converter --- */

static XrmQuark QStart, QEnd, QCenter, QStretch;

static void
_CvtStringToFlexAlign(XrmValuePtr args, Cardinal *num_args,
                      XrmValuePtr fromVal, XrmValuePtr toVal)
{
    static IswFlexAlign align;
    XrmQuark q;
    char lower[40];
    (void)args; (void)num_args;

    if (strlen((char *)fromVal->addr) < sizeof(lower)) {
        ISWCopyISOLatin1Lowered(lower, (char *)fromVal->addr);
        q = XrmStringToQuark(lower);
        if      (q == QStart)   align = XtflexAlignStart;
        else if (q == QEnd)     align = XtflexAlignEnd;
        else if (q == QCenter)  align = XtflexAlignCenter;
        else if (q == QStretch) align = XtflexAlignStretch;
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
    QStart   = XrmPermStringToQuark("start");
    QEnd     = XrmPermStringToQuark("end");
    QCenter  = XrmPermStringToQuark("center");
    QStretch = XrmPermStringToQuark("stretch");
    IswAddConverter(IswRString, IswRFlexAlign, _CvtStringToFlexAlign, NULL, 0);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    (void)request; (void)args; (void)num_args;
    FlexBoxWidget fw = (FlexBoxWidget)new;
    fw->flexBox.preferred_width = 0;
    fw->flexBox.preferred_height = 0;
    fw->flexBox.layout_in_progress = False;
}

/* --- Layout engine --- */

/*
 * Query a child's preferred size along the primary axis.
 * If flexBasis is set, use it (scaled); otherwise use the child's
 * preferred geometry or current size.
 */
static Dimension
ChildBasis(FlexBoxWidget fw, Widget child, Boolean is_horizontal)
{
    FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;

    if (fc->flexBox.flex_basis > 0)
        return ((int)fc->flexBox.flex_basis);

    /* Ask the child what size it wants */
    IswWidgetGeometry preferred;
    IswQueryGeometry(child, NULL, &preferred);

    if (is_horizontal) {
        if (preferred.request_mode & XCB_CONFIG_WINDOW_WIDTH)
            return preferred.width;
        return child->core.width;
    } else {
        if (preferred.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            return preferred.height;
        return child->core.height;
    }
}

static Dimension
ChildCrossPreferred(Widget child, Boolean is_horizontal)
{
    IswWidgetGeometry preferred;
    IswQueryGeometry(child, NULL, &preferred);

    if (is_horizontal) {
        if (preferred.request_mode & XCB_CONFIG_WINDOW_HEIGHT)
            return preferred.height;
        return child->core.height;
    } else {
        if (preferred.request_mode & XCB_CONFIG_WINDOW_WIDTH)
            return preferred.width;
        return child->core.width;
    }
}

/*
 * Core layout algorithm. Computes preferred_width/height and optionally
 * configures children.
 */
static void
DoLayout(FlexBoxWidget fw, Boolean set_children)
{
    int n = fw->composite.num_children;
    WidgetList children = fw->composite.children;
    Boolean horiz = (fw->flexBox.orientation == XtorientHorizontal);
    Dimension spacing = ((int)fw->flexBox.spacing);

    /* Count managed children and compute fixed-size total.
     * flexGrow=0 children claim their preferred/basis size.
     * flexGrow>0 children only claim their flexBasis (0 if unset);
     * their remaining allocation comes from the grow distribution. */
    int managed = 0;
    int total_grow = 0;
    int total_fixed = 0;
    int last_bw = 0;
    Dimension max_cross = 1;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!IswIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        int bw = (int)child->core.border_width;
        int bw2 = 2 * bw;
        Dimension cross = ChildCrossPreferred(child, horiz);

        /* Border-overlap accounting: X11 positions are at the outer corner
         * of the border, so a child at x spans [x, x + 2*bw + size].  Placing
         * the next child at (x + bw + size) makes its leading-border column
         * coincide with the previous child's trailing-border column, producing
         * a single shared 1px line instead of a 2px double line.
         * Each child therefore contributes (size + bw) to the main flow,
         * plus one trailing bw contributed once after the loop for the last
         * child's outer edge. */
        if (fc->flexBox.flex_grow > 0) {
            if (fc->flexBox.flex_basis > 0)
                total_fixed += (int)fc->flexBox.flex_basis + bw;
            else
                total_fixed += bw;
            total_grow += fc->flexBox.flex_grow;
        } else {
            total_fixed += (int)ChildBasis(fw, child, horiz) + bw;
        }

        if ((int)cross + bw2 > (int)max_cross)
            max_cross = (Dimension)((int)cross + bw2);
        last_bw = bw;
        managed++;
    }
    /* One extra bw for the final child's trailing outer border edge. */
    total_fixed += last_bw;

    if (managed == 0) {
        fw->flexBox.preferred_width = 1;
        fw->flexBox.preferred_height = 1;
        return;
    }

    int total_spacing = (managed - 1) * (int)spacing;

    /* Preferred size: for the preferred calculation, include all
     * children's full preferred sizes (grow children want their
     * natural size when unconstrained). Borders overlap between
     * adjacent children (shared pixel column), so each child costs
     * size + bw, plus one extra bw for the first child's outer edge. */
    if (!set_children) {
        int preferred_base = 0;
        int last_bw = 0;
        for (int i = 0; i < n; i++) {
            Widget child = children[i];
            if (!IswIsManaged(child))
                continue;
            int bw = (int)child->core.border_width;
            preferred_base += (int)ChildBasis(fw, child, horiz) + bw;
            last_bw = bw;
        }
        /* One extra bw for the last child's trailing outer border edge. */
        preferred_base += last_bw;
        int preferred_main = preferred_base + total_spacing;
        fw->flexBox.preferred_width  = horiz ? (Dimension)preferred_main : max_cross;
        fw->flexBox.preferred_height = horiz ? max_cross : (Dimension)preferred_main;
        return;
    }

    /* Distribute space within the allocated container size */
    Dimension container_main  = horiz ? fw->core.width : fw->core.height;
    Dimension container_cross = horiz ? fw->core.height : fw->core.width;

    int remaining = (int)container_main - total_fixed - total_spacing;
    if (remaining < 0)
        remaining = 0;

    /* First child sits at pos=0; X11 places the child's outer border corner
     * at that coordinate, so the leading border is already inside the
     * container. Adjacent children's borders then overlap by one pixel
     * column as `pos` advances by (main_sz + bw) per child. */
    Position pos = 0;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!IswIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        int bw = (int)child->core.border_width;
        int bw2 = 2 * bw;

        /* Main-axis size */
        int main_sz;
        if (fc->flexBox.flex_grow > 0 && total_grow > 0) {
            /* Grow children: flexBasis + proportional share of remaining */
            int basis = 0;
            if (fc->flexBox.flex_basis > 0)
                basis = (int)fc->flexBox.flex_basis;
            main_sz = basis + (remaining * fc->flexBox.flex_grow) / total_grow;
        } else {
            /* Non-grow children: preferred/basis size */
            main_sz = (int)ChildBasis(fw, child, horiz);
        }
        if (main_sz < 1)
            main_sz = 1;

        /* Cross-axis size and position */
        Dimension cross_pref = ChildCrossPreferred(child, horiz);
        int cross_pos = 0;
        int cross_sz = (int)cross_pref;

        switch (fc->flexBox.flex_align) {
        case XtflexAlignStart:
            cross_pos = 0;
            break;
        case XtflexAlignEnd:
            cross_pos = (int)container_cross - cross_sz - bw2;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case XtflexAlignCenter:
            cross_pos = ((int)container_cross - cross_sz - bw2) / 2;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case XtflexAlignStretch:
            cross_pos = 0;
            cross_sz = (int)container_cross - bw2;
            break;
        }
        if (cross_sz < 1)
            cross_sz = 1;

        Position x, y;
        Dimension w, h;
        if (horiz) {
            x = pos;
            y = (Position)cross_pos;
            w = (Dimension)main_sz;
            h = (Dimension)cross_sz;
        } else {
            x = (Position)cross_pos;
            y = pos;
            w = (Dimension)cross_sz;
            h = (Dimension)main_sz;
        }

        IswConfigureWidget(child, x, y, w, h, child->core.border_width);

        /* Advance by main_sz + one border width so the next child's border
         * overlaps this child's trailing border by one pixel column. */
        pos += (Position)(main_sz + bw) + (Position)spacing;
    }
}

/* --- Widget methods --- */

static void
Resize(Widget w)
{
    DoLayout((FlexBoxWidget)w, TRUE);
}

static void
ChangeManaged(Widget w)
{
    FlexBoxWidget fw = (FlexBoxWidget)w;

    if (fw->flexBox.layout_in_progress)
        return;

    fw->flexBox.layout_in_progress = True;

    DoLayout(fw, FALSE);

    /* Try to get our preferred size */
    if (fw->flexBox.preferred_width != fw->core.width ||
        fw->flexBox.preferred_height != fw->core.height)
    {
        IswWidgetGeometry req, reply;
        req.request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        req.width = fw->flexBox.preferred_width;
        req.height = fw->flexBox.preferred_height;
        IswGeometryResult result = IswMakeGeometryRequest(w, &req, &reply);
        if (result == IswGeometryAlmost) {
            req.width = reply.width;
            req.height = reply.height;
            IswMakeGeometryRequest(w, &req, NULL);
        }
    }

    DoLayout(fw, TRUE);

    fw->flexBox.layout_in_progress = False;
}

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request,
                IswWidgetGeometry *reply)
{
    FlexBoxWidget fw = (FlexBoxWidget)IswParent(child);
    (void)reply;

    /* Reject position requests — we control placement */
    if (request->request_mode & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
        return IswGeometryNo;

    /* Accept size requests, then re-layout */
    if (request->request_mode & XCB_CONFIG_WINDOW_WIDTH)
        child->core.width = request->width;
    if (request->request_mode & XCB_CONFIG_WINDOW_HEIGHT)
        child->core.height = request->height;
    if (request->request_mode & XCB_CONFIG_WINDOW_BORDER_WIDTH)
        child->core.border_width = request->border_width;

    ChangeManaged((Widget)fw);

    return IswGeometryDone;
}

static IswGeometryResult
PreferredGeometry(Widget widget, IswWidgetGeometry *request,
                  IswWidgetGeometry *reply)
{
    FlexBoxWidget fw = (FlexBoxWidget)widget;

    DoLayout(fw, FALSE);

    reply->width = fw->flexBox.preferred_width;
    reply->height = fw->flexBox.preferred_height;
    reply->request_mode = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    if ((request->request_mode & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)) == (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
        && request->width == reply->width
        && request->height == reply->height)
        return IswGeometryYes;
    if (reply->width == fw->core.width && reply->height == fw->core.height)
        return IswGeometryNo;
    return IswGeometryAlmost;
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList args, Cardinal *num_args)
{
    FlexBoxWidget cur = (FlexBoxWidget)current;
    FlexBoxWidget nw  = (FlexBoxWidget)new;
    (void)request; (void)args; (void)num_args;

    if (cur->flexBox.orientation != nw->flexBox.orientation ||
        cur->flexBox.spacing     != nw->flexBox.spacing)
    {
        DoLayout(nw, TRUE);
    }
    return FALSE;
}

static Boolean
ConstraintSetValues(Widget current, Widget request, Widget new,
                    ArgList args, Cardinal *num_args)
{
    FlexBoxConstraints cfc = (FlexBoxConstraints)current->core.constraints;
    FlexBoxConstraints nfc = (FlexBoxConstraints)new->core.constraints;
    (void)request; (void)args; (void)num_args;

    if (cfc->flexBox.flex_grow  != nfc->flexBox.flex_grow  ||
        cfc->flexBox.flex_basis != nfc->flexBox.flex_basis ||
        cfc->flexBox.flex_align != nfc->flexBox.flex_align)
    {
        DoLayout((FlexBoxWidget)IswParent(new), TRUE);
    }
    return FALSE;
}
