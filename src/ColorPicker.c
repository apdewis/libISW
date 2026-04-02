/*
 * ColorPicker.c - ColorPicker widget implementation
 *
 * A Form subclass with three Scale sliders (R/G/B 0-255), labels,
 * and a preview swatch showing the selected color.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ColorPickerP.h>
#include <ISW/Scale.h>
#include <ISW/Label.h>
#include <ISW/Simple.h>

#include <stdio.h>

#define superclass (&formClassRec)

#define Offset(field) XtOffsetOf(ColorPickerRec, field)

static XtResource resources[] = {
    {XtNcolorRed, XtCColorRed, XtRInt, sizeof(int),
        Offset(colorPicker.red), XtRImmediate, (XtPointer) 0},
    {XtNcolorGreen, XtCColorGreen, XtRInt, sizeof(int),
        Offset(colorPicker.green), XtRImmediate, (XtPointer) 0},
    {XtNcolorBlue, XtCColorBlue, XtRInt, sizeof(int),
        Offset(colorPicker.blue), XtRImmediate, (XtPointer) 0},
    {XtNcolorChanged, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(colorPicker.color_changed), XtRCallback, NULL},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
};

#undef Offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SliderChanged(Widget, XtPointer, XtPointer);
static void UpdateSwatch(ColorPickerWidget);

ColorPickerClassRec colorPickerClassRec = {
  { /* core */
    (WidgetClass) superclass,
    "ColorPicker",
    sizeof(ColorPickerRec),
    IswInitializeWidgetSet,
    NULL,
    FALSE,
    Initialize,
    NULL,
    XtInheritRealize,
    NULL,
    0,
    resources,
    XtNumber(resources),
    NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    NULL,
    XtInheritResize,
    XtInheritExpose,
    SetValues,
    NULL,
    XtInheritSetValuesAlmost,
    NULL,
    NULL,
    XtVersion,
    NULL,
    NULL,
    XtInheritQueryGeometry,
    XtInheritDisplayAccelerator,
    NULL
  },
  { /* composite */
    XtInheritGeometryManager,
    XtInheritChangeManaged,
    XtInheritInsertChild,
    XtInheritDeleteChild,
    NULL
  },
  { /* constraint */
    NULL, 0,
    sizeof(ColorPickerConstraintsRec),
    NULL, NULL, NULL, NULL
  },
  { /* form */
    XtInheritLayout
  },
  { /* colorPicker */
    0
  }
};

WidgetClass colorPickerWidgetClass = (WidgetClass)&colorPickerClassRec;

/* --- Swatch expose callback --- */

static void
SwatchExpose(Widget w, XtPointer client_data, xcb_generic_event_t *event, Boolean *cont)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    (void)event; (void)cont;

    ISWRenderContext *ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    if (ctx) {
        ISWRenderBegin(ctx);
        ISWRenderSetColorRGBA(ctx,
            (double)cpw->colorPicker.red / 255.0,
            (double)cpw->colorPicker.green / 255.0,
            (double)cpw->colorPicker.blue / 255.0,
            1.0);
        ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
        ISWRenderEnd(ctx);
        ISWRenderDestroy(ctx);
    }
}

static void
UpdateSwatch(ColorPickerWidget cpw)
{
    if (cpw->colorPicker.swatchW && XtIsRealized(cpw->colorPicker.swatchW)) {
        /* Trigger a redraw by clearing and exposing */
        SwatchExpose(cpw->colorPicker.swatchW, (XtPointer)cpw, NULL, NULL);
    }
}

/* --- Slider callback --- */

static void
SliderChanged(Widget w, XtPointer client_data, XtPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    IswScaleCallbackData *sd = (IswScaleCallbackData *) call_data;
    (void)w;

    if (w == cpw->colorPicker.redScale)
        cpw->colorPicker.red = sd->value;
    else if (w == cpw->colorPicker.greenScale)
        cpw->colorPicker.green = sd->value;
    else if (w == cpw->colorPicker.blueScale)
        cpw->colorPicker.blue = sd->value;

    UpdateSwatch(cpw);

    IswColorPickerCallbackData cb;
    cb.red = cpw->colorPicker.red;
    cb.green = cpw->colorPicker.green;
    cb.blue = cpw->colorPicker.blue;
    XtCallCallbacks((Widget)cpw, XtNcolorChanged, (XtPointer)&cb);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ColorPickerWidget cpw = (ColorPickerWidget) new;
    Arg a[10];
    Cardinal n;
    Dimension slider_w = ISWScaleDim(new, 150);
    Dimension slider_h = ISWScaleDim(new, 30);
    Dimension swatch_sz = ISWScaleDim(new, 60);
    Dimension label_w = ISWScaleDim(new, 20);
    (void)request; (void)args; (void)num_args;

    /* Clamp */
    if (cpw->colorPicker.red < 0) cpw->colorPicker.red = 0;
    if (cpw->colorPicker.red > 255) cpw->colorPicker.red = 255;
    if (cpw->colorPicker.green < 0) cpw->colorPicker.green = 0;
    if (cpw->colorPicker.green > 255) cpw->colorPicker.green = 255;
    if (cpw->colorPicker.blue < 0) cpw->colorPicker.blue = 0;
    if (cpw->colorPicker.blue > 255) cpw->colorPicker.blue = 255;

    /* R label */
    n = 0;
    XtSetArg(a[n], XtNlabel, "R"); n++;
    XtSetArg(a[n], XtNborderWidth, 0); n++;
    XtSetArg(a[n], XtNwidth, label_w); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    cpw->colorPicker.redLabel = XtCreateManagedWidget(
        "redLabel", labelWidgetClass, new, a, n);

    /* R slider */
    n = 0;
    XtSetArg(a[n], XtNminimumValue, 0); n++;
    XtSetArg(a[n], XtNmaximumValue, 255); n++;
    XtSetArg(a[n], XtNscaleValue, cpw->colorPicker.red); n++;
    XtSetArg(a[n], XtNshowValue, False); n++;
    XtSetArg(a[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(a[n], XtNwidth, slider_w); n++;
    XtSetArg(a[n], XtNheight, slider_h); n++;
    XtSetArg(a[n], XtNfromHoriz, cpw->colorPicker.redLabel); n++;
    cpw->colorPicker.redScale = XtCreateManagedWidget(
        "redScale", scaleWidgetClass, new, a, n);
    XtAddCallback(cpw->colorPicker.redScale, XtNvalueChanged,
                  SliderChanged, (XtPointer)cpw);

    /* G label */
    n = 0;
    XtSetArg(a[n], XtNlabel, "G"); n++;
    XtSetArg(a[n], XtNborderWidth, 0); n++;
    XtSetArg(a[n], XtNwidth, label_w); n++;
    XtSetArg(a[n], XtNfromVert, cpw->colorPicker.redLabel); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    cpw->colorPicker.greenLabel = XtCreateManagedWidget(
        "greenLabel", labelWidgetClass, new, a, n);

    /* G slider */
    n = 0;
    XtSetArg(a[n], XtNminimumValue, 0); n++;
    XtSetArg(a[n], XtNmaximumValue, 255); n++;
    XtSetArg(a[n], XtNscaleValue, cpw->colorPicker.green); n++;
    XtSetArg(a[n], XtNshowValue, False); n++;
    XtSetArg(a[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(a[n], XtNwidth, slider_w); n++;
    XtSetArg(a[n], XtNheight, slider_h); n++;
    XtSetArg(a[n], XtNfromHoriz, cpw->colorPicker.greenLabel); n++;
    XtSetArg(a[n], XtNfromVert, cpw->colorPicker.redScale); n++;
    cpw->colorPicker.greenScale = XtCreateManagedWidget(
        "greenScale", scaleWidgetClass, new, a, n);
    XtAddCallback(cpw->colorPicker.greenScale, XtNvalueChanged,
                  SliderChanged, (XtPointer)cpw);

    /* B label */
    n = 0;
    XtSetArg(a[n], XtNlabel, "B"); n++;
    XtSetArg(a[n], XtNborderWidth, 0); n++;
    XtSetArg(a[n], XtNwidth, label_w); n++;
    XtSetArg(a[n], XtNfromVert, cpw->colorPicker.greenLabel); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    cpw->colorPicker.blueLabel = XtCreateManagedWidget(
        "blueLabel", labelWidgetClass, new, a, n);

    /* B slider */
    n = 0;
    XtSetArg(a[n], XtNminimumValue, 0); n++;
    XtSetArg(a[n], XtNmaximumValue, 255); n++;
    XtSetArg(a[n], XtNscaleValue, cpw->colorPicker.blue); n++;
    XtSetArg(a[n], XtNshowValue, False); n++;
    XtSetArg(a[n], XtNorientation, XtorientHorizontal); n++;
    XtSetArg(a[n], XtNwidth, slider_w); n++;
    XtSetArg(a[n], XtNheight, slider_h); n++;
    XtSetArg(a[n], XtNfromHoriz, cpw->colorPicker.blueLabel); n++;
    XtSetArg(a[n], XtNfromVert, cpw->colorPicker.greenScale); n++;
    cpw->colorPicker.blueScale = XtCreateManagedWidget(
        "blueScale", scaleWidgetClass, new, a, n);
    XtAddCallback(cpw->colorPicker.blueScale, XtNvalueChanged,
                  SliderChanged, (XtPointer)cpw);

    /* Color swatch preview */
    n = 0;
    XtSetArg(a[n], XtNwidth, swatch_sz); n++;
    XtSetArg(a[n], XtNheight, swatch_sz); n++;
    XtSetArg(a[n], XtNborderWidth, 1); n++;
    XtSetArg(a[n], XtNfromHoriz, cpw->colorPicker.redScale); n++;
    cpw->colorPicker.swatchW = XtCreateManagedWidget(
        "swatch", simpleWidgetClass, new, a, n);
    XtAddEventHandler(cpw->colorPicker.swatchW, XCB_EVENT_MASK_EXPOSURE, False,
                      SwatchExpose, (XtPointer)cpw);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired,
          ArgList args, Cardinal *num_args)
{
    ColorPickerWidget ccpw = (ColorPickerWidget) current;
    ColorPickerWidget dcpw = (ColorPickerWidget) desired;
    (void)request; (void)args; (void)num_args;

    Boolean changed = FALSE;

    if (ccpw->colorPicker.red != dcpw->colorPicker.red) {
        IswScaleSetValue(dcpw->colorPicker.redScale, dcpw->colorPicker.red);
        changed = TRUE;
    }
    if (ccpw->colorPicker.green != dcpw->colorPicker.green) {
        IswScaleSetValue(dcpw->colorPicker.greenScale, dcpw->colorPicker.green);
        changed = TRUE;
    }
    if (ccpw->colorPicker.blue != dcpw->colorPicker.blue) {
        IswScaleSetValue(dcpw->colorPicker.blueScale, dcpw->colorPicker.blue);
        changed = TRUE;
    }

    if (changed)
        UpdateSwatch(dcpw);

    return FALSE;
}

/* --- Public API --- */

void
IswColorPickerGetColor(Widget w, int *r, int *g, int *b)
{
    ColorPickerWidget cpw = (ColorPickerWidget) w;
    if (r) *r = cpw->colorPicker.red;
    if (g) *g = cpw->colorPicker.green;
    if (b) *b = cpw->colorPicker.blue;
}

void
IswColorPickerSetColor(Widget w, int r, int g, int b)
{
    Arg args[3];
    XtSetArg(args[0], XtNcolorRed, r);
    XtSetArg(args[1], XtNcolorGreen, g);
    XtSetArg(args[2], XtNcolorBlue, b);
    XtSetValues(w, args, 3);
}
