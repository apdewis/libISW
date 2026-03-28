/*
 * SpinBox.c - SpinBox widget implementation
 *
 * A numeric entry with up/down arrow buttons. Subclasses Form and
 * creates three internal children: an AsciiText field and two Repeater
 * buttons for increment/decrement with auto-repeat on hold.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/SpinBoxP.h>
#include <ISW/ISWRender.h>
#include <ISW/AsciiText.h>
#include <ISW/Repeater.h>
#include <ISW/CommandP.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Offset(field) XtOffsetOf(SpinBoxRec, field)

static XtResource resources[] = {
    {XtNspinMinimum, XtCSpinMinimum, XtRInt, sizeof(int),
        Offset(spinBox.minimum), XtRImmediate, (XtPointer) 0},
    {XtNspinMaximum, XtCSpinMaximum, XtRInt, sizeof(int),
        Offset(spinBox.maximum), XtRImmediate, (XtPointer) 100},
    {XtNspinValue, XtCSpinValue, XtRInt, sizeof(int),
        Offset(spinBox.value), XtRImmediate, (XtPointer) 0},
    {XtNspinIncrement, XtCSpinIncrement, XtRInt, sizeof(int),
        Offset(spinBox.increment), XtRImmediate, (XtPointer) 1},
    {XtNspinWrap, XtCSpinWrap, XtRBoolean, sizeof(Boolean),
        Offset(spinBox.wrap), XtRImmediate, (XtPointer) False},
    {XtNvalueChanged, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(spinBox.value_changed), XtRCallback, NULL},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 1},
};

#undef Offset

/* Forward declarations */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void GetValuesHook(Widget, ArgList, Cardinal *);

static void UpCallback(Widget, XtPointer, XtPointer);
static void DownCallback(Widget, XtPointer, XtPointer);
static void LayoutChildren(SpinBoxWidget);
static void ChangeManaged(Widget);
static XtGeometryResult GeometryManager(Widget, XtWidgetGeometry *, XtWidgetGeometry *);

SpinBoxClassRec spinBoxClassRec = {
  { /* core_class fields */
    /* superclass         */ (WidgetClass) &formClassRec,
    /* class_name         */ "SpinBox",
    /* widget_size        */ sizeof(SpinBoxRec),
    /* class_initialize   */ IswInitializeWidgetSet,
    /* class_part_init    */ NULL,
    /* class_inited       */ FALSE,
    /* initialize         */ Initialize,
    /* initialize_hook    */ NULL,
    /* realize            */ Realize,
    /* actions            */ NULL,
    /* num_actions        */ 0,
    /* resources          */ resources,
    /* num_resources      */ XtNumber(resources),
    /* xrm_class          */ NULLQUARK,
    /* compress_motion    */ TRUE,
    /* compress_exposure  */ TRUE,
    /* compress_enterleave*/ TRUE,
    /* visible_interest   */ FALSE,
    /* destroy            */ Destroy,
    /* resize             */ Resize,
    /* expose             */ Redisplay,
    /* set_values         */ SetValues,
    /* set_values_hook    */ NULL,
    /* set_values_almost  */ XtInheritSetValuesAlmost,
    /* get_values_hook    */ GetValuesHook,
    /* accept_focus       */ NULL,
    /* version            */ XtVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ NULL,
    /* query_geometry     */ XtInheritQueryGeometry,
    /* display_accelerator*/ XtInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */ GeometryManager,
    /* change_managed     */ ChangeManaged,
    /* insert_child       */ XtInheritInsertChild,
    /* delete_child       */ XtInheritDeleteChild,
    /* extension          */ NULL
  },
  { /* constraint_class fields */
    /* subresources       */ NULL,
    /* subresource_count  */ 0,
    /* constraint_size    */ sizeof(SpinBoxConstraintsRec),
    /* initialize         */ NULL,
    /* destroy            */ NULL,
    /* set_values         */ NULL,
    /* extension          */ NULL
  },
  { /* form_class fields */
    /* layout             */ XtInheritLayout
  },
  { /* spinBox_class fields */
    /* empty              */ 0
  }
};

WidgetClass spinBoxWidgetClass = (WidgetClass)&spinBoxClassRec;

/* --- Internal helpers --- */

static void
SyncTextFromValue(SpinBoxWidget sbw)
{
    char buf[32];
    Arg args[1];

    snprintf(buf, sizeof(buf), "%d", sbw->spinBox.value);
    XtSetArg(args[0], XtNstring, buf);
    XtSetValues(sbw->spinBox.textW, args, 1);
}

static int
ReadTextValue(SpinBoxWidget sbw)
{
    String str = NULL;
    Arg args[1];

    XtSetArg(args[0], XtNstring, &str);
    XtGetValues(sbw->spinBox.textW, args, 1);

    if (str == NULL || *str == '\0')
        return sbw->spinBox.value;

    return atoi(str);
}

static void
ClampAndNotify(SpinBoxWidget sbw, int new_value)
{
    if (sbw->spinBox.wrap) {
        int range = sbw->spinBox.maximum - sbw->spinBox.minimum + 1;
        if (range > 0) {
            while (new_value > sbw->spinBox.maximum)
                new_value -= range;
            while (new_value < sbw->spinBox.minimum)
                new_value += range;
        }
    } else {
        if (new_value < sbw->spinBox.minimum)
            new_value = sbw->spinBox.minimum;
        if (new_value > sbw->spinBox.maximum)
            new_value = sbw->spinBox.maximum;
    }

    if (new_value != sbw->spinBox.value) {
        sbw->spinBox.value = new_value;
        SyncTextFromValue(sbw);

        IswSpinBoxCallbackData cb_data;
        cb_data.value = new_value;
        XtCallCallbacks((Widget)sbw, XtNvalueChanged, (XtPointer)&cb_data);
    }
}

/* --- Button callbacks --- */

static void
UpCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    SpinBoxWidget sbw = (SpinBoxWidget) client_data;
    (void)w; (void)call_data;
    ClampAndNotify(sbw, sbw->spinBox.value + sbw->spinBox.increment);
}

static void
DownCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    SpinBoxWidget sbw = (SpinBoxWidget) client_data;
    (void)w; (void)call_data;
    ClampAndNotify(sbw, sbw->spinBox.value - sbw->spinBox.increment);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SpinBoxWidget sbw = (SpinBoxWidget) new;
    Arg arglist[12];
    Cardinal n;
    char buf[32];
    (void)request; (void)args; (void)num_args;

    /* Clamp initial value */
    if (sbw->spinBox.value < sbw->spinBox.minimum)
        sbw->spinBox.value = sbw->spinBox.minimum;
    if (sbw->spinBox.value > sbw->spinBox.maximum)
        sbw->spinBox.value = sbw->spinBox.maximum;

    snprintf(buf, sizeof(buf), "%d", sbw->spinBox.value);

    /* All children borderless — SpinBox draws its own border */

    /* Text field */
    n = 0;
    XtSetArg(arglist[n], XtNstring, buf); n++;
    XtSetArg(arglist[n], XtNeditType, IswtextEdit); n++;
    XtSetArg(arglist[n], XtNborderWidth, 0); n++;
    sbw->spinBox.textW = XtCreateManagedWidget("text", asciiTextWidgetClass,
                                                new, arglist, n);

    /* Up button — highlight forced to 0 after creation to bypass
       Command's Initialize override of the sentinel default */
    n = 0;
    XtSetArg(arglist[n], XtNlabel, "+"); n++;
    XtSetArg(arglist[n], XtNborderWidth, 0); n++;
    XtSetArg(arglist[n], XtNborderStrokeWidth, 0); n++;
    sbw->spinBox.upW = XtCreateManagedWidget("up", repeaterWidgetClass,
                                              new, arglist, n);
    ((CommandWidget)sbw->spinBox.upW)->command.border_stroke_width = 0;
    XtAddCallback(sbw->spinBox.upW, XtNcallback, UpCallback, (XtPointer)sbw);

    /* Down button */
    n = 0;
    XtSetArg(arglist[n], XtNlabel, "-"); n++;
    XtSetArg(arglist[n], XtNborderWidth, 0); n++;
    XtSetArg(arglist[n], XtNborderStrokeWidth, 0); n++;
    sbw->spinBox.downW = XtCreateManagedWidget("down", repeaterWidgetClass,
                                                new, arglist, n);
    ((CommandWidget)sbw->spinBox.downW)->command.border_stroke_width = 0;
    XtAddCallback(sbw->spinBox.downW, XtNcallback, DownCallback, (XtPointer)sbw);

    /* Set default size if not specified */
    if (sbw->core.width == 0)
        sbw->core.width = ISWScaleDim(new, 120);
    if (sbw->core.height == 0) {
        /* Derive height from font metrics so the text line fits */
        XFontStruct *font = NULL;
        XtVaGetValues(sbw->spinBox.textW, XtNfont, &font, NULL);
        int font_h = font ? ISWScaledFontHeight(new, font) : ISWScaleDim(new, 14);
        int margin = 4;  /* Text widget default VMargins (top=2 + bottom=2) */
        sbw->core.height = (Dimension)(font_h + margin + ISWScaleDim(new, 4));
    }

    sbw->spinBox.render_ctx = NULL;
    LayoutChildren(sbw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    SpinBoxWidget sbw = (SpinBoxWidget) w;

    /* Chain up to Form's realize */
    (*spinBoxWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    sbw->spinBox.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

static void
Destroy(Widget w)
{
    SpinBoxWidget sbw = (SpinBoxWidget) w;
    if (sbw->spinBox.render_ctx)
        ISWRenderDestroy(sbw->spinBox.render_ctx);
}

static void
ChangeManaged(Widget w)
{
    LayoutChildren((SpinBoxWidget) w);
}

static XtGeometryResult
GeometryManager(Widget child, XtWidgetGeometry *request, XtWidgetGeometry *reply)
{
    (void)child; (void)request; (void)reply;
    /* Deny all child geometry requests — we control layout */
    return XtGeometryNo;
}

static void
LayoutChildren(SpinBoxWidget sbw)
{
    Dimension btn_w = ISWScaleDim((Widget)sbw, 18);
    Dimension w = sbw->core.width;
    Dimension h = sbw->core.height;
    /* Leave 1px for vertical divider, 1px for horizontal divider */
    Dimension text_w = (w > btn_w + 1) ? (w - btn_w - 1) : 1;
    Position btn_x = (Position)(text_w + 1);  /* 1px gap for vertical divider */
    Dimension up_h = h / 2;
    Position down_y = (Position)(up_h + 1);   /* 1px gap for horizontal divider */
    Dimension down_h = (h > (Dimension)down_y) ? (h - (Dimension)down_y) : 1;

    /* All children borderless — SpinBox draws divider lines in the gaps */
    XtConfigureWidget(sbw->spinBox.textW, 0, 0, text_w, h, 0);
    XtConfigureWidget(sbw->spinBox.upW, btn_x, 0, btn_w, up_h, 0);
    XtConfigureWidget(sbw->spinBox.downW, btn_x, down_y, btn_w, down_h, 0);
}

static void
Resize(Widget w)
{
    SpinBoxWidget sbw = (SpinBoxWidget) w;
    LayoutChildren(sbw);
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    SpinBoxWidget sbw = (SpinBoxWidget) w;
    ISWRenderContext *ctx = sbw->spinBox.render_ctx;
    (void)event; (void)region;

    if (!ctx || !XtIsRealized(w))
        return;

    Dimension btn_w = ISWScaleDim(w, 18);
    int text_w = (int)sbw->core.width - (int)btn_w - 1;
    int gap_x = text_w;
    int gap_y = (int)sbw->core.height / 2;

    ISWRenderBegin(ctx);
    ISWRenderSetColor(ctx, sbw->core.border_pixel);
    ISWRenderSetLineWidth(ctx, 1.0);
    /* Vertical divider between text and buttons */
    ISWRenderDrawLine(ctx, gap_x, 0, gap_x, (int)sbw->core.height);
    /* Horizontal divider between up and down buttons */
    ISWRenderDrawLine(ctx, gap_x, gap_y, (int)sbw->core.width, gap_y);
    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired,
          ArgList args, Cardinal *num_args)
{
    SpinBoxWidget csw = (SpinBoxWidget) current;
    SpinBoxWidget dsw = (SpinBoxWidget) desired;
    (void)request; (void)args; (void)num_args;

    /* Clamp value */
    if (dsw->spinBox.value < dsw->spinBox.minimum)
        dsw->spinBox.value = dsw->spinBox.minimum;
    if (dsw->spinBox.value > dsw->spinBox.maximum)
        dsw->spinBox.value = dsw->spinBox.maximum;

    if (csw->spinBox.value != dsw->spinBox.value ||
        csw->spinBox.minimum != dsw->spinBox.minimum ||
        csw->spinBox.maximum != dsw->spinBox.maximum) {
        SyncTextFromValue(dsw);
    }

    return FALSE;  /* Form handles its own redraw */
}

static void
GetValuesHook(Widget w, ArgList args, Cardinal *num_args)
{
    SpinBoxWidget sbw = (SpinBoxWidget) w;
    Cardinal i;

    /* Sync value from text field before returning */
    for (i = 0; i < *num_args; i++) {
        if (strcmp(args[i].name, XtNspinValue) == 0) {
            sbw->spinBox.value = ReadTextValue(sbw);
            *(int *)args[i].value = sbw->spinBox.value;
        }
    }
}

/* --- Public API --- */

void
IswSpinBoxSetValue(Widget w, int value)
{
    Arg args[1];
    XtSetArg(args[0], XtNspinValue, value);
    XtSetValues(w, args, 1);
}

int
IswSpinBoxGetValue(Widget w)
{
    int value;
    Arg args[1];
    XtSetArg(args[0], XtNspinValue, &value);
    XtGetValues(w, args, 1);
    return value;
}
