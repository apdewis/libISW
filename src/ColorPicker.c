/*
 * ColorPicker.c - ColorPicker widget implementation
 *
 * A Form subclass with three Slider widgets (R/G/B 0-255), labels,
 * and a preview swatch showing the selected color.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ColorPickerP.h>
#include <ISW/Slider.h>
#include <ISW/Label.h>
#include <ISW/Simple.h>

#include <stdio.h>

#define superclass (&formClassRec)

#define Offset(field) IswOffsetOf(ColorPickerRec, field)

static IswResource resources[] = {
    {IswNcolorRed, IswCColorRed, IswRInt, sizeof(int),
        Offset(colorPicker.red), IswRImmediate, (IswPointer) 0},
    {IswNcolorGreen, IswCColorGreen, IswRInt, sizeof(int),
        Offset(colorPicker.green), IswRImmediate, (IswPointer) 0},
    {IswNcolorBlue, IswCColorBlue, IswRInt, sizeof(int),
        Offset(colorPicker.blue), IswRImmediate, (IswPointer) 0},
    {IswNcolorChanged, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(colorPicker.color_changed), IswRCallback, NULL},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
};

#undef Offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SliderChanged(Widget, IswPointer, IswPointer);
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
    IswInheritRealize,
    NULL,
    0,
    resources,
    IswNumber(resources),
    NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    NULL,
    IswInheritResize,
    IswInheritExpose,
    SetValues,
    NULL,
    IswInheritSetValuesAlmost,
    NULL,
    NULL,
    IswVersion,
    NULL,
    NULL,
    IswInheritQueryGeometry,
    IswInheritDisplayAccelerator,
    NULL
  },
  { /* composite */
    IswInheritGeometryManager,
    IswInheritChangeManaged,
    IswInheritInsertChild,
    IswInheritDeleteChild,
    NULL
  },
  { /* constraint */
    NULL, 0,
    sizeof(ColorPickerConstraintsRec),
    NULL, NULL, NULL, NULL
  },
  { /* form */
    IswInheritLayout
  },
  { /* colorPicker */
    0
  }
};

WidgetClass colorPickerWidgetClass = (WidgetClass)&colorPickerClassRec;

/* --- Swatch expose callback --- */

static void
SwatchExpose(Widget w, IswPointer client_data, xcb_generic_event_t *event, Boolean *cont)
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
    if (cpw->colorPicker.swatchW && IswIsRealized(cpw->colorPicker.swatchW)) {
        /* Trigger a redraw by clearing and exposing */
        SwatchExpose(cpw->colorPicker.swatchW, (IswPointer)cpw, NULL, NULL);
    }
}

/* --- Slider callback --- */

static void
SliderChanged(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    IswSliderCallbackData *sd = (IswSliderCallbackData *) call_data;
    (void)w;

    if (w == cpw->colorPicker.redSlider)
        cpw->colorPicker.red = sd->value;
    else if (w == cpw->colorPicker.greenSlider)
        cpw->colorPicker.green = sd->value;
    else if (w == cpw->colorPicker.blueSlider)
        cpw->colorPicker.blue = sd->value;

    UpdateSwatch(cpw);

    IswColorPickerCallbackData cb;
    cb.red = cpw->colorPicker.red;
    cb.green = cpw->colorPicker.green;
    cb.blue = cpw->colorPicker.blue;
    IswCallCallbacks((Widget)cpw, IswNcolorChanged, (IswPointer)&cb);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ColorPickerWidget cpw = (ColorPickerWidget) new;
    Arg a[10];
    Cardinal n;
    Dimension slider_w = (150);
    Dimension slider_h = (30);
    Dimension swatch_sz = (60);
    Dimension label_w = (20);
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
    IswSetArg(a[n], IswNlabel, "R"); n++;
    IswSetArg(a[n], IswNborderWidth, 0); n++;
    IswSetArg(a[n], IswNwidth, label_w); n++;
    IswSetArg(a[n], IswNleft, IswChainLeft); n++;
    cpw->colorPicker.redLabel = IswCreateManagedWidget(
        "redLabel", labelWidgetClass, new, a, n);

    /* R slider */
    n = 0;
    IswSetArg(a[n], IswNminimumValue, 0); n++;
    IswSetArg(a[n], IswNmaximumValue, 255); n++;
    IswSetArg(a[n], IswNsliderValue, cpw->colorPicker.red); n++;
    IswSetArg(a[n], IswNshowValue, False); n++;
    IswSetArg(a[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(a[n], IswNwidth, slider_w); n++;
    IswSetArg(a[n], IswNheight, slider_h); n++;
    IswSetArg(a[n], IswNfromHoriz, cpw->colorPicker.redLabel); n++;
    cpw->colorPicker.redSlider = IswCreateManagedWidget(
        "redSlider", sliderWidgetClass, new, a, n);
    IswAddCallback(cpw->colorPicker.redSlider, IswNvalueChanged,
                  SliderChanged, (IswPointer)cpw);

    /* G label */
    n = 0;
    IswSetArg(a[n], IswNlabel, "G"); n++;
    IswSetArg(a[n], IswNborderWidth, 0); n++;
    IswSetArg(a[n], IswNwidth, label_w); n++;
    IswSetArg(a[n], IswNfromVert, cpw->colorPicker.redLabel); n++;
    IswSetArg(a[n], IswNleft, IswChainLeft); n++;
    cpw->colorPicker.greenLabel = IswCreateManagedWidget(
        "greenLabel", labelWidgetClass, new, a, n);

    /* G slider */
    n = 0;
    IswSetArg(a[n], IswNminimumValue, 0); n++;
    IswSetArg(a[n], IswNmaximumValue, 255); n++;
    IswSetArg(a[n], IswNsliderValue, cpw->colorPicker.green); n++;
    IswSetArg(a[n], IswNshowValue, False); n++;
    IswSetArg(a[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(a[n], IswNwidth, slider_w); n++;
    IswSetArg(a[n], IswNheight, slider_h); n++;
    IswSetArg(a[n], IswNfromHoriz, cpw->colorPicker.greenLabel); n++;
    IswSetArg(a[n], IswNfromVert, cpw->colorPicker.redSlider); n++;
    cpw->colorPicker.greenSlider = IswCreateManagedWidget(
        "greenSlider", sliderWidgetClass, new, a, n);
    IswAddCallback(cpw->colorPicker.greenSlider, IswNvalueChanged,
                  SliderChanged, (IswPointer)cpw);

    /* B label */
    n = 0;
    IswSetArg(a[n], IswNlabel, "B"); n++;
    IswSetArg(a[n], IswNborderWidth, 0); n++;
    IswSetArg(a[n], IswNwidth, label_w); n++;
    IswSetArg(a[n], IswNfromVert, cpw->colorPicker.greenLabel); n++;
    IswSetArg(a[n], IswNleft, IswChainLeft); n++;
    cpw->colorPicker.blueLabel = IswCreateManagedWidget(
        "blueLabel", labelWidgetClass, new, a, n);

    /* B slider */
    n = 0;
    IswSetArg(a[n], IswNminimumValue, 0); n++;
    IswSetArg(a[n], IswNmaximumValue, 255); n++;
    IswSetArg(a[n], IswNsliderValue, cpw->colorPicker.blue); n++;
    IswSetArg(a[n], IswNshowValue, False); n++;
    IswSetArg(a[n], IswNorientation, XtorientHorizontal); n++;
    IswSetArg(a[n], IswNwidth, slider_w); n++;
    IswSetArg(a[n], IswNheight, slider_h); n++;
    IswSetArg(a[n], IswNfromHoriz, cpw->colorPicker.blueLabel); n++;
    IswSetArg(a[n], IswNfromVert, cpw->colorPicker.greenSlider); n++;
    cpw->colorPicker.blueSlider = IswCreateManagedWidget(
        "blueSlider", sliderWidgetClass, new, a, n);
    IswAddCallback(cpw->colorPicker.blueSlider, IswNvalueChanged,
                  SliderChanged, (IswPointer)cpw);

    /* Color swatch preview */
    n = 0;
    IswSetArg(a[n], IswNwidth, swatch_sz); n++;
    IswSetArg(a[n], IswNheight, swatch_sz); n++;
    IswSetArg(a[n], IswNborderWidth, 1); n++;
    IswSetArg(a[n], IswNfromHoriz, cpw->colorPicker.redSlider); n++;
    cpw->colorPicker.swatchW = IswCreateManagedWidget(
        "swatch", simpleWidgetClass, new, a, n);
    IswAddEventHandler(cpw->colorPicker.swatchW, XCB_EVENT_MASK_EXPOSURE, False,
                      SwatchExpose, (IswPointer)cpw);
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
        IswSliderSetValue(dcpw->colorPicker.redSlider, dcpw->colorPicker.red);
        changed = TRUE;
    }
    if (ccpw->colorPicker.green != dcpw->colorPicker.green) {
        IswSliderSetValue(dcpw->colorPicker.greenSlider, dcpw->colorPicker.green);
        changed = TRUE;
    }
    if (ccpw->colorPicker.blue != dcpw->colorPicker.blue) {
        IswSliderSetValue(dcpw->colorPicker.blueSlider, dcpw->colorPicker.blue);
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
    IswSetArg(args[0], IswNcolorRed, r);
    IswSetArg(args[1], IswNcolorGreen, g);
    IswSetArg(args[2], IswNcolorBlue, b);
    IswSetValues(w, args, 3);
}
