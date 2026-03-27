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
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/FlexBoxP.h>
#include <ISW/ISWRender.h>
#include "ISWXcbDraw.h"

/* --- Forward declarations --- */

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Resize(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static XtGeometryResult PreferredGeometry(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
static void ChangeManaged(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static Boolean ConstraintSetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void DoLayout(FlexBoxWidget fw, Boolean set_children);

/* --- Resources --- */

static IswFlexAlign defAlign = XtflexAlignStretch;

#define Offset(field) XtOffsetOf(FlexBoxRec, flexBox.field)
static XtResource resources[] = {
    {XtNorientation, XtCOrientation, XtROrientation, sizeof(XtOrientation),
     Offset(orientation), XtRImmediate, (XtPointer)XtorientVertical},
    {XtNspacing, XtCSpacing, XtRDimension, sizeof(Dimension),
     Offset(spacing), XtRImmediate, (XtPointer)0},
};
#undef Offset

#define COffset(field) XtOffsetOf(FlexBoxConstraintsRec, flexBox.field)
static XtResource constraintResources[] = {
    {XtNflexGrow, XtCFlexGrow, XtRInt, sizeof(int),
     COffset(flex_grow), XtRImmediate, (XtPointer)0},
    {XtNflexBasis, XtCFlexBasis, XtRDimension, sizeof(Dimension),
     COffset(flex_basis), XtRImmediate, (XtPointer)0},
    {XtNflexAlign, XtCFlexAlign, XtRFlexAlign, sizeof(IswFlexAlign),
     COffset(flex_align), XtRFlexAlign, (XtPointer)&defAlign},
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
    /* realize            */ XtInheritRealize,
    /* actions            */ NULL,
    /* num_actions        */ 0,
    /* resources          */ resources,
    /* num_resources      */ XtNumber(resources),
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
    /* set_values_almost  */ XtInheritSetValuesAlmost,
    /* get_values_hook    */ NULL,
    /* accept_focus       */ NULL,
    /* version            */ XtVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ NULL,
    /* query_geometry     */ PreferredGeometry,
    /* display_accelerator*/ XtInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* composite_class */
    /* geometry_manager   */ GeometryManager,
    /* change_managed     */ ChangeManaged,
    /* insert_child       */ XtInheritInsertChild,
    /* delete_child       */ XtInheritDeleteChild,
    /* extension          */ NULL
  },
  { /* constraint_class */
    /* subresources       */ constraintResources,
    /* subresource_count  */ XtNumber(constraintResources),
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
    QStart   = XrmPermStringToQuark("start");
    QEnd     = XrmPermStringToQuark("end");
    QCenter  = XrmPermStringToQuark("center");
    QStretch = XrmPermStringToQuark("stretch");
    XtAddConverter(XtRString, XtRFlexAlign, _CvtStringToFlexAlign, NULL, 0);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    (void)request; (void)args; (void)num_args;
    FlexBoxWidget fw = (FlexBoxWidget)new;
    fw->flexBox.preferred_width = 0;
    fw->flexBox.preferred_height = 0;
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
        return ISWScaleDim((Widget)fw, (int)fc->flexBox.flex_basis);

    /* Ask the child what size it wants */
    XtWidgetGeometry preferred;
    XtQueryGeometry(child, NULL, &preferred);

    if (is_horizontal) {
        if (preferred.request_mode & CWWidth)
            return preferred.width;
        return child->core.width;
    } else {
        if (preferred.request_mode & CWHeight)
            return preferred.height;
        return child->core.height;
    }
}

static Dimension
ChildCrossPreferred(Widget child, Boolean is_horizontal)
{
    XtWidgetGeometry preferred;
    XtQueryGeometry(child, NULL, &preferred);

    if (is_horizontal) {
        if (preferred.request_mode & CWHeight)
            return preferred.height;
        return child->core.height;
    } else {
        if (preferred.request_mode & CWWidth)
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
    Dimension spacing = ISWScaleDim((Widget)fw, (int)fw->flexBox.spacing);

    /* Count managed children and gather base sizes */
    int managed = 0;
    int total_grow = 0;
    int total_base = 0;
    Dimension max_cross = 1;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!XtIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        Dimension base = ChildBasis(fw, child, horiz);
        Dimension cross = ChildCrossPreferred(child, horiz);

        total_base += (int)base;
        total_grow += fc->flexBox.flex_grow;
        if (cross > max_cross)
            max_cross = cross;
        managed++;
    }

    if (managed == 0) {
        fw->flexBox.preferred_width = 1;
        fw->flexBox.preferred_height = 1;
        return;
    }

    int total_spacing = (managed - 1) * (int)spacing;
    int preferred_main = total_base + total_spacing;
    Dimension preferred_cross = max_cross;

    fw->flexBox.preferred_width  = horiz ? (Dimension)preferred_main : preferred_cross;
    fw->flexBox.preferred_height = horiz ? preferred_cross : (Dimension)preferred_main;

    if (!set_children)
        return;

    /* Distribute space */
    Dimension container_main  = horiz ? fw->core.width : fw->core.height;
    Dimension container_cross = horiz ? fw->core.height : fw->core.width;

    int remaining = (int)container_main - total_base - total_spacing;
    if (remaining < 0)
        remaining = 0;

    Position pos = 0;

    for (int i = 0; i < n; i++) {
        Widget child = children[i];
        if (!XtIsManaged(child))
            continue;

        FlexBoxConstraints fc = (FlexBoxConstraints)child->core.constraints;
        Dimension base = ChildBasis(fw, child, horiz);

        /* Main-axis size: base + proportional share of remaining */
        int main_sz = (int)base;
        if (fc->flexBox.flex_grow > 0 && total_grow > 0)
            main_sz += (remaining * fc->flexBox.flex_grow) / total_grow;
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
            cross_pos = (int)container_cross - cross_sz;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case XtflexAlignCenter:
            cross_pos = ((int)container_cross - cross_sz) / 2;
            if (cross_pos < 0) cross_pos = 0;
            break;
        case XtflexAlignStretch:
            cross_pos = 0;
            cross_sz = (int)container_cross;
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

        XtConfigureWidget(child, x, y, w, h, child->core.border_width);

        pos += (Position)main_sz + (Position)spacing;
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

    DoLayout(fw, FALSE);

    /* Try to get our preferred size */
    if (fw->flexBox.preferred_width != fw->core.width ||
        fw->flexBox.preferred_height != fw->core.height)
    {
        XtWidgetGeometry req, reply;
        req.request_mode = CWWidth | CWHeight;
        req.width = fw->flexBox.preferred_width;
        req.height = fw->flexBox.preferred_height;
        XtGeometryResult result = XtMakeGeometryRequest(w, &req, &reply);
        if (result == XtGeometryAlmost) {
            req.width = reply.width;
            req.height = reply.height;
            XtMakeGeometryRequest(w, &req, NULL);
        }
    }

    DoLayout(fw, TRUE);
}

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request,
                XtWidgetGeometry *reply)
{
    FlexBoxWidget fw = (FlexBoxWidget)XtParent(child);
    (void)reply;

    /* Reject position requests — we control placement */
    if (request->request_mode & (CWX | CWY))
        return XtGeometryNo;

    /* Accept size requests, then re-layout */
    if (request->request_mode & CWWidth)
        child->core.width = request->width;
    if (request->request_mode & CWHeight)
        child->core.height = request->height;
    if (request->request_mode & CWBorderWidth)
        child->core.border_width = request->border_width;

    ChangeManaged((Widget)fw);

    return XtGeometryDone;
}

static XtGeometryResult
PreferredGeometry(Widget widget, XtWidgetGeometry *request,
                  XtWidgetGeometry *reply)
{
    FlexBoxWidget fw = (FlexBoxWidget)widget;

    DoLayout(fw, FALSE);

    reply->width = fw->flexBox.preferred_width;
    reply->height = fw->flexBox.preferred_height;
    reply->request_mode = CWWidth | CWHeight;

    if ((request->request_mode & (CWWidth | CWHeight)) == (CWWidth | CWHeight)
        && request->width == reply->width
        && request->height == reply->height)
        return XtGeometryYes;
    if (reply->width == fw->core.width && reply->height == fw->core.height)
        return XtGeometryNo;
    return XtGeometryAlmost;
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
        DoLayout((FlexBoxWidget)XtParent(new), TRUE);
    }
    return FALSE;
}
