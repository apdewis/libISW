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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/SpinBoxP.h>
#include <ISW/ISWRender.h>
#include <ISW/AsciiText.h>
#include <ISW/Repeater.h>
#include <ISW/Label.h>
#include <ISW/CommandP.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Offset(field) IswOffsetOf(SpinBoxRec, field)

static IswResource resources[] = {
    {IswNspinMinimum, IswCSpinMinimum, IswRInt, sizeof(int),
        Offset(spinBox.minimum), IswRImmediate, (IswPointer) 0},
    {IswNspinMaximum, IswCSpinMaximum, IswRInt, sizeof(int),
        Offset(spinBox.maximum), IswRImmediate, (IswPointer) 100},
    {IswNspinValue, IswCSpinValue, IswRInt, sizeof(int),
        Offset(spinBox.value), IswRImmediate, (IswPointer) 0},
    {IswNspinIncrement, IswCSpinIncrement, IswRInt, sizeof(int),
        Offset(spinBox.increment), IswRImmediate, (IswPointer) 1},
    {IswNspinWrap, IswCSpinWrap, IswRBoolean, sizeof(Boolean),
        Offset(spinBox.wrap), IswRImmediate, (IswPointer) False},
    {IswNvalueChanged, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(spinBox.value_changed), IswRCallback, NULL},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 1},
};

#undef Offset

/* Forward declarations */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void GetValuesHook(Widget, ArgList, Cardinal *);

static void UpCallback(Widget, IswPointer, IswPointer);
static void DownCallback(Widget, IswPointer, IswPointer);
static void LayoutChildren(SpinBoxWidget);
static void ChangeManaged(Widget);
static IswGeometryResult GeometryManager(Widget, IswWidgetGeometry *, IswWidgetGeometry *);

static void IncrementAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void DecrementAction(Widget, xcb_generic_event_t *, String *, Cardinal *);

static char defaultTranslations[] =
    "<Key>Up:    Increment()\n"
    "<Key>Down:  Decrement()";

static IswActionsRec actionsList[] = {
    {"Increment", IncrementAction},
    {"Decrement", DecrementAction},
};

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
    /* actions            */ actionsList,
    /* num_actions        */ IswNumber(actionsList),
    /* resources          */ resources,
    /* num_resources      */ IswNumber(resources),
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
    /* set_values_almost  */ IswInheritSetValuesAlmost,
    /* get_values_hook    */ GetValuesHook,
    /* accept_focus       */ NULL,
    /* version            */ IswVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ defaultTranslations,
    /* query_geometry     */ IswInheritQueryGeometry,
    /* display_accelerator*/ IswInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */ GeometryManager,
    /* change_managed     */ ChangeManaged,
    /* insert_child       */ IswInheritInsertChild,
    /* delete_child       */ IswInheritDeleteChild,
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
    /* layout             */ IswInheritLayout
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
    IswSetArg(args[0], IswNstring, buf);
    IswSetValues(sbw->spinBox.textW, args, 1);
}

static int
ReadTextValue(SpinBoxWidget sbw)
{
    String str = NULL;
    Arg args[1];

    IswSetArg(args[0], IswNstring, &str);
    IswGetValues(sbw->spinBox.textW, args, 1);

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
        IswCallCallbacks((Widget)sbw, IswNvalueChanged, (IswPointer)&cb_data);
    }
}

/* --- Button callbacks --- */

static void
UpCallback(Widget w, IswPointer client_data, IswPointer call_data)
{
    SpinBoxWidget sbw = (SpinBoxWidget) client_data;
    (void)w; (void)call_data;
    ClampAndNotify(sbw, sbw->spinBox.value + sbw->spinBox.increment);
}

static void
DownCallback(Widget w, IswPointer client_data, IswPointer call_data)
{
    SpinBoxWidget sbw = (SpinBoxWidget) client_data;
    (void)w; (void)call_data;
    ClampAndNotify(sbw, sbw->spinBox.value - sbw->spinBox.increment);
}

/* Walk up to find the enclosing SpinBox — lets the same action procs
 * be bound on child widgets (e.g. the internal Text field). */
static SpinBoxWidget
FindSpinBox(Widget w)
{
    while (w && !IswIsSubclass(w, spinBoxWidgetClass))
        w = IswParent(w);
    return (SpinBoxWidget) w;
}

static void
IncrementAction(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    SpinBoxWidget sbw = FindSpinBox(w);
    (void)e; (void)p; (void)np;
    if (sbw) ClampAndNotify(sbw, sbw->spinBox.value + sbw->spinBox.increment);
}

static void
DecrementAction(Widget w, xcb_generic_event_t *e, String *p, Cardinal *np)
{
    SpinBoxWidget sbw = FindSpinBox(w);
    (void)e; (void)p; (void)np;
    if (sbw) ClampAndNotify(sbw, sbw->spinBox.value - sbw->spinBox.increment);
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
    IswSetArg(arglist[n], IswNstring, buf); n++;
    IswSetArg(arglist[n], IswNeditType, IswtextEdit); n++;
    IswSetArg(arglist[n], IswNborderWidth, 0); n++;
    IswSetArg(arglist[n], IswNconsumeTab, False); n++;  /* single-line: Tab traverses */
    sbw->spinBox.textW = IswCreateManagedWidget("text", asciiTextWidgetClass,
                                                new, arglist, n);

    /* Augment the Text child so Up/Down step the SpinBox while focused. */
    {
        static char spinbox_text_translations[] =
            "<Key>Up:   Increment()\n"
            "<Key>Down: Decrement()";
        IswOverrideTranslations(sbw->spinBox.textW,
            IswParseTranslationTable(spinbox_text_translations));
    }

    /* Up button with upward arrow SVG */
    static const char up_arrow_svg[] =
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='6'>"
        "<path d='M1,5 L5,1 L9,5' stroke='black' stroke-width='1.5' "
        "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";
    n = 0;
    IswSetArg(arglist[n], IswNlabel, ""); n++;
    IswSetArg(arglist[n], IswNimage, up_arrow_svg); n++;
    IswSetArg(arglist[n], IswNborderWidth, 0); n++;
    IswSetArg(arglist[n], IswNborderStrokeWidth, 0); n++;
    IswSetArg(arglist[n], IswNcornerRadius, 0); n++;
    IswSetArg(arglist[n], IswNinternalWidth, 0); n++;
    IswSetArg(arglist[n], IswNinternalHeight, 0); n++;
    sbw->spinBox.upW = IswCreateManagedWidget("up", repeaterWidgetClass,
                                              new, arglist, n);
    ((SimpleWidget) sbw->spinBox.upW)->simple.traversal_on = False;
    IswAddCallback(sbw->spinBox.upW, IswNcallback, UpCallback, (IswPointer)sbw);

    /* Down button with downward arrow SVG */
    static const char down_arrow_svg[] =
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='6'>"
        "<path d='M1,1 L5,5 L9,1' stroke='black' stroke-width='1.5' "
        "fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>";
    n = 0;
    IswSetArg(arglist[n], IswNlabel, ""); n++;
    IswSetArg(arglist[n], IswNimage, down_arrow_svg); n++;
    IswSetArg(arglist[n], IswNborderWidth, 0); n++;
    IswSetArg(arglist[n], IswNborderStrokeWidth, 0); n++;
    IswSetArg(arglist[n], IswNcornerRadius, 0); n++;
    IswSetArg(arglist[n], IswNinternalWidth, 0); n++;
    IswSetArg(arglist[n], IswNinternalHeight, 0); n++;
    sbw->spinBox.downW = IswCreateManagedWidget("down", repeaterWidgetClass,
                                                new, arglist, n);
    ((SimpleWidget) sbw->spinBox.downW)->simple.traversal_on = False;
    IswAddCallback(sbw->spinBox.downW, IswNcallback, DownCallback, (IswPointer)sbw);

    /* Set default size if not specified */
    if (sbw->core.width == 0)
        sbw->core.width = (120);
    if (sbw->core.height == 0) {
        /* Derive height from font metrics so the text line fits */
        IswFontStruct *font = NULL;
        IswVaGetValues(sbw->spinBox.textW, IswNfont, &font, NULL);
        int font_h = font ? ISWScaledFontHeight(new, font) : (14);
        int margin = 4;  /* Text widget default VMargins (top=2 + bottom=2) */
        sbw->core.height = (Dimension)(font_h + margin + (4));
    }

    sbw->spinBox.render_ctx = NULL;
    LayoutChildren(sbw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
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

static IswGeometryResult
GeometryManager(Widget child, IswWidgetGeometry *request, IswWidgetGeometry *reply)
{
    (void)child; (void)request; (void)reply;
    /* Deny all child geometry requests — we control layout */
    return IswGeometryNo;
}

static void
LayoutChildren(SpinBoxWidget sbw)
{
    Dimension btn_w = (27);
    Dimension w = sbw->core.width;
    Dimension h = sbw->core.height;
    /* Leave 1px for vertical divider, 1px for horizontal divider */
    Dimension text_w = (w > btn_w + 1) ? (w - btn_w - 1) : 1;
    Position btn_x = (Position)(text_w + 1);  /* 1px gap for vertical divider */
    Dimension up_h = h / 2;
    Position down_y = (Position)(up_h + 1);   /* 1px gap for horizontal divider */
    Dimension down_h = (h > (Dimension)down_y) ? (h - (Dimension)down_y) : 1;

    /* All children borderless — SpinBox draws divider lines in the gaps */
    IswConfigureWidget(sbw->spinBox.textW, 0, 0, text_w, h, 0);
    IswConfigureWidget(sbw->spinBox.upW, btn_x, 0, btn_w, up_h, 0);
    IswConfigureWidget(sbw->spinBox.downW, btn_x, down_y, btn_w, down_h, 0);
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

    if (!ctx || !IswIsRealized(w))
        return;

    Dimension btn_w = (27);
    int text_w = (int)sbw->core.width - (int)btn_w - 1;
    int gap_x = text_w;
    int gap_y = (int)sbw->core.height / 2;

    ISWRenderBegin(ctx);
    ISWRenderSetColor(ctx, sbw->core.border_pixel);
    ISWRenderSetLineWidth(ctx, 1.0);
    /* Outer border */
    ISWRenderStrokeRectangle(ctx, 0, 0, (int)sbw->core.width, (int)sbw->core.height);
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
        if (strcmp(args[i].name, IswNspinValue) == 0) {
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
    IswSetArg(args[0], IswNspinValue, value);
    IswSetValues(w, args, 1);
}

int
IswSpinBoxGetValue(Widget w)
{
    int value;
    Arg args[1];
    IswSetArg(args[0], IswNspinValue, &value);
    IswGetValues(w, args, 1);
    return value;
}
