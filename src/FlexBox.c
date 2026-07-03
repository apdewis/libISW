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

/* --- Forward declarations --- */

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static void Resize(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static IswGeometryResult PreferredGeometry(Widget, IswWidgetGeometry *, IswWidgetGeometry *);
static void ChangeManaged(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void DoLayout(FlexBoxWidget fw, Boolean set_children);

/* --- Resources --- */

static IswFlexAlign defAlign = IswFlexAlignStretch;

#define Offset(field) IswOffsetOf(FlexBoxRec, flexBox.field)
static IswResource resources[] = {
    {IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
     Offset(orientation), IswRImmediate, (IswPointer)IswOrientVertical},
    {IswNspacing, IswCSpacing, IswRDimension, sizeof(Dimension),
     Offset(spacing), IswRImmediate, (IswPointer)0},
};
#undef Offset

#define COffset(field) IswOffsetOf(FlexBoxConstraintsRec, flexBox.field)
static IswResource constraintResources[] = {
    {IswNfillWeight, IswCFillWeight, IswRInt, sizeof(int),
     COffset(fill_weight), IswRImmediate, (IswPointer)0},
    {IswNfixedSize, IswCFixedSize, IswRDimension, sizeof(Dimension),
     COffset(fixed_size), IswRImmediate, (IswPointer)0},
    {IswNalign, IswCFlexAlign, IswRFlexAlign, sizeof(IswFlexAlign),
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
    /* xrm_class          */ ISW_NULLQUARK,
    /* compress_motion    */ TRUE,
    /* compress_exposure  */ TRUE,
    /* compress_enterleave*/ TRUE,
    /* visible_interest   */ FALSE,
    /* destroy            */ Destroy,
    /* resize             */ Resize,
    /* expose             */ Redisplay,
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

static IswQuark QStart, QEnd, QCenter, QStretch;

static void
_CvtStringToFlexAlign(IswValuePtr args, Cardinal *num_args,
                      IswValuePtr fromVal, IswValuePtr toVal)
{
    static IswFlexAlign align;
    IswQuark q;
    char lower[40];
    (void)args; (void)num_args;

    if (strlen((char *)fromVal->addr) < sizeof(lower)) {
        ISWCopyISOLatin1Lowered(lower, (char *)fromVal->addr);
        q = IswStringToQuark(lower);
        if      (q == QStart)   align = IswFlexAlignStart;
        else if (q == QEnd)     align = IswFlexAlignEnd;
        else if (q == QCenter)  align = IswFlexAlignCenter;
        else if (q == QStretch) align = IswFlexAlignStretch;
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
    QStart   = IswPermStringToQuark("start");
    QEnd     = IswPermStringToQuark("end");
    QCenter  = IswPermStringToQuark("center");
    QStretch = IswPermStringToQuark("stretch");
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
    fw->flexBox.render_ctx = NULL;
}

static void
Destroy(Widget w)
{
    FlexBoxWidget fw = (FlexBoxWidget)w;
    if (fw->flexBox.render_ctx != NULL) {
        ISWRenderDestroy(fw->flexBox.render_ctx);
        fw->flexBox.render_ctx = NULL;
    }
}

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    FlexBoxWidget fw = (FlexBoxWidget)w;
    ISWRenderContext *ctx;

    (void)event; (void)region;

    if (!IswIsRealized(w) || w->core.width == 0 || w->core.height == 0)
        return;

    ctx = fw->flexBox.render_ctx;
    if (ctx == NULL)
        ctx = fw->flexBox.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx == NULL)
        return;

    ISWRenderBegin(ctx);
    _IswCoreDrawBackground(w, ctx);
    if (w->core.border_width > 0) {
        ISWRenderSetLineWidth(ctx, (double) w->core.border_width);
        ISWRenderStrokeRectangle(ctx, 0, 0, w->core.width, w->core.height);
    }
    ISWRenderEnd(ctx);
}

/* --- Layout engine --- */

/* Child's main-axis size: fixedSize if set, else query preferred geometry. */
static Dimension
ChildPreferred(FlexBoxWidget fw, Widget child, Boolean is_horizontal)
{
    FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
    (void)fw;

    if (fc->flexBox.fixed_size > 0)
        return fc->flexBox.fixed_size;

    IswWidgetGeometry preferred;
    IswQueryGeometry(child, NULL, &preferred);

    if (is_horizontal) {
        if (preferred.request_mode & IswCWWidth)
            return preferred.width;
        return child->core.width;
    } else {
        if (preferred.request_mode & IswCWHeight)
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
        if (preferred.request_mode & IswCWHeight)
            return preferred.height;
        return child->core.height;
    } else {
        if (preferred.request_mode & IswCWWidth)
            return preferred.width;
        return child->core.width;
    }
}

/*
 * Core layout algorithm.
 *
 * Native fill model — NOT CSS flexbox.  Children are either fixed or fill:
 *   fixedSize > 0          → child is exactly that size (fill_weight ignored)
 *   fill_weight > 0, no fixedSize → proportional share of remaining space
 *   neither                → child's own preferred size (treated as fixed)
 *
 * Preferred size reflects what the layout would actually produce — fill
 * children contribute their preferred size, not zero.
 */
static void
DoLayout(FlexBoxWidget fw, Boolean set_children)
{
    int n = fw->composite.num_children;
    WidgetList children = fw->composite.children;
    Boolean horiz = (fw->flexBox.orientation == IswOrientHorizontal);
    Dimension spacing = ((int)fw->flexBox.spacing);

    /*
     * Classify children.  fixedSize means fixed — period.  fill_weight
     * is only honoured when fixedSize is unset.
     *
     * Windowless model: each child's border is part of its own surface
     * footprint and does NOT overlap with neighbours.  Each child
     * contributes (content + leading_border + trailing_border) to the
     * main axis.  Per-side border widths are used throughout.
     */
    int managed = 0;
    int total_weight = 0;
    int total_fixed = 0;
    Dimension max_cross = 1;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!IswIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);
        int main_border = horiz ? _IswBorderHoriz(bs) : _IswBorderVert(bs);
        int cross_border = horiz ? _IswBorderVert(bs) : _IswBorderHoriz(bs);
        Dimension cross = ChildCrossPreferred(child, horiz);

        if (fc->flexBox.fixed_size > 0) {
            /* Explicit fixed size — fill_weight ignored. */
            total_fixed += (int)fc->flexBox.fixed_size + main_border;
        } else if (fc->flexBox.fill_weight > 0) {
            /* Fill child — only border counts as fixed; content comes
             * from proportional distribution. */
            total_fixed += main_border;
            total_weight += fc->flexBox.fill_weight;
        } else {
            /* No fixedSize, no fill — use child's own preferred size. */
            total_fixed += (int)ChildPreferred(fw, child, horiz) + main_border;
        }

        if ((int)cross + cross_border > (int)max_cross)
            max_cross = (Dimension)((int)cross + cross_border);
        managed++;
    }

    if (managed == 0) {
        fw->flexBox.preferred_width = 1;
        fw->flexBox.preferred_height = 1;
        return;
    }

    int total_spacing = (managed - 1) * (int)spacing;

    /* Preferred size: every child at its natural size.  Fill children
     * contribute their preferred size so the preferred total matches
     * what the layout would actually produce — no mismatch between
     * what we ask for and what we draw. */
    if (!set_children) {
        int preferred_base = 0;
        for (int i = 0; i < n; i++) {
            Widget child = children[i];
            if (!IswIsManaged(child))
                continue;
            IswBorderSides bs = _IswGetBorderSides(child);
            int main_border = horiz ? _IswBorderHoriz(bs) : _IswBorderVert(bs);
            preferred_base += (int)ChildPreferred(fw, child, horiz) + main_border;
        }
        int preferred_main = preferred_base + total_spacing;
        fw->flexBox.preferred_width  = horiz ? (Dimension)preferred_main : max_cross;
        fw->flexBox.preferred_height = horiz ? max_cross : (Dimension)preferred_main;
        return;
    }

    /* Distribute space within the allocated container size.
     * Fill children split the remaining space proportionally by weight. */
    Dimension container_main  = horiz ? fw->core.width : fw->core.height;
    Dimension container_cross = horiz ? fw->core.height : fw->core.width;

    int remaining = (int)container_main - total_fixed - total_spacing;
    if (remaining < 0)
        remaining = 0;

    Position pos = 0;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!IswIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        IswBorderSides bs = _IswGetBorderSides(child);
        int main_border = horiz ? _IswBorderHoriz(bs) : _IswBorderVert(bs);
        int cross_border = horiz ? _IswBorderVert(bs) : _IswBorderHoriz(bs);

        int main_sz;
        if (fc->flexBox.fixed_size > 0) {
            main_sz = (int)fc->flexBox.fixed_size;
        } else if (fc->flexBox.fill_weight > 0 && total_weight > 0) {
            main_sz = (remaining * fc->flexBox.fill_weight) / total_weight;
        } else {
            main_sz = (int)ChildPreferred(fw, child, horiz);
        }
        if (main_sz < 1)
            main_sz = 1;

        /* Cross-axis size and position */
        Dimension cross_pref = ChildCrossPreferred(child, horiz);
        int cross_pos = 0;
        int cross_sz = (int)cross_pref;

        switch (fc->flexBox.flex_align) {
        case IswFlexAlignStart:
            cross_pos = 0;
            break;
        case IswFlexAlignEnd:
            cross_pos = (int)container_cross - cross_sz - cross_border;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case IswFlexAlignCenter:
            cross_pos = ((int)container_cross - cross_sz - cross_border) / 2;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case IswFlexAlignStretch:
            cross_pos = 0;
            cross_sz = (int)container_cross - cross_border;
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

        pos += (Position)(main_sz + main_border) + (Position)spacing;
    }
}

/* --- Widget methods --- */

static void
Resize(Widget w)
{
    FlexBoxWidget fw = (FlexBoxWidget)w;
    if (fw->flexBox.layout_in_progress)
        return;
    fw->flexBox.layout_in_progress = True;
    DoLayout(fw, TRUE);
    fw->flexBox.layout_in_progress = False;
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
        req.request_mode = IswCWWidth | IswCWHeight;
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
    FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
    (void)reply;

    /* During layout the container is the sole authority on child geometry.
     * Accepting requests here while ChangeManaged is blocked would leave
     * core.width/height out of sync with the configured size. */
    if (fw->flexBox.layout_in_progress)
        return IswGeometryNo;

    if (request->request_mode & (IswCWX | IswCWY))
        return IswGeometryNo;

    /* Fixed and fill children are sized by the layout, not by their own
     * requests.  Refusing tells the child to fit its content into the
     * size it already has (a Paned refits its panes and answers its own
     * child with IswGeometryAlmost). */
    if ((request->request_mode & (IswCWWidth | IswCWHeight)) &&
        (fc->flexBox.fixed_size > 0 || fc->flexBox.fill_weight > 0))
        return IswGeometryNo;

    /* A query must not change any state. */
    if (request->request_mode & IswCWQueryOnly)
        return IswGeometryYes;

    if (request->request_mode & IswCWWidth)
        child->core.width = request->width;
    if (request->request_mode & IswCWHeight)
        child->core.height = request->height;
    if (request->request_mode & IswCWBorderWidth)
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
    reply->request_mode = IswCWWidth | IswCWHeight;

    if ((request->request_mode & (IswCWWidth | IswCWHeight)) == (IswCWWidth | IswCWHeight)
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

    if (cfc->flexBox.fill_weight != nfc->flexBox.fill_weight ||
        cfc->flexBox.fixed_size  != nfc->flexBox.fixed_size  ||
        cfc->flexBox.flex_align  != nfc->flexBox.flex_align)
    {
        /* Relayout all children for the new constraints.  But DoLayout also
           moves `new` (the child being set) to its new slot, and IswSetValues,
           on return from here, turns any geometry change it sees on `new` into
           a single geometry request back to our GeometryManager.  If that
           request carries an X/Y move, GeometryManager rejects it wholesale
           (we reject all position requests), and IswSetValues then reverts
           `new` to its pre-SetValues geometry — leaving it frozen at its old
           slot while its siblings reflow.

           Restore `new`'s position after laying out so the request IswSetValues
           emits is size-only (no X/Y): GeometryManager accepts it and the
           ensuing ChangeManaged relayout places `new` at its correct slot. */
        Position keep_x = new->core.x;
        Position keep_y = new->core.y;
        DoLayout((FlexBoxWidget)IswParent(new), TRUE);
        new->core.x = keep_x;
        new->core.y = keep_y;
    }
    return FALSE;
}
