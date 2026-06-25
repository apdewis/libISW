/*
 * ColorPicker.c - ColorPicker widget implementation
 *
 * A Form subclass with a hue strip, saturation/value 2D area,
 * three SpinBox widgets (R/G/B 0-255), and a preview swatch.
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
#include <ISW/SpinBox.h>
#include <ISW/Label.h>
#include <ISW/Simple.h>
#include <ISW/DrawingArea.h>
#include <ISW/EventI.h>
#include <ISW/IswArgMacros.h>
#include <ISW/IswEvent.h>
#include <ISW/Text.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SpinBoxChanged(Widget, IswPointer, IswPointer);
static void SwatchExpose(Widget, IswPointer, IswPointer);
static void HueExpose(Widget, IswPointer, IswPointer);
static void HueInput(Widget, IswPointer, IswPointer);
static void SVExpose(Widget, IswPointer, IswPointer);
static void SVInput(Widget, IswPointer, IswPointer);
static void UpdateSwatch(ColorPickerWidget);
static void RGBtoHSV(int r, int g, int b, float *h, float *s, float *v);
static void HSVtoRGB(float h, float s, float v, int *r, int *g, int *b);
static void SyncHSVFromRGB(ColorPickerWidget cpw);
static void SyncRGBFromHSV(ColorPickerWidget cpw);
static void FireCallback(ColorPickerWidget cpw);
static void RebuildHuePixels(ColorPickerWidget cpw, int w, int h);
static void RebuildSVPixels(ColorPickerWidget cpw, int w, int h);
static void UpdateHexFromRGB(ColorPickerWidget cpw);
static void HexCommitAction(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actionsList[] = {
    {"HexCommit", HexCommitAction},
};

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
    actionsList,
    IswNumber(actionsList),
    resources,
    IswNumber(resources),
    ISW_NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
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

/* --- HSV <-> RGB conversion --- */

static void
RGBtoHSV(int r, int g, int b, float *h, float *s, float *v)
{
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float cmax = rf > gf ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
    float cmin = rf < gf ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
    float delta = cmax - cmin;

    *v = cmax;
    *s = (cmax > 0.0f) ? delta / cmax : 0.0f;

    if (delta < 1e-6f) {
        *h = 0.0f;
    } else if (cmax == rf) {
        *h = fmodf((gf - bf) / delta, 6.0f);
        if (*h < 0.0f) *h += 6.0f;
        *h /= 6.0f;
    } else if (cmax == gf) {
        *h = ((bf - rf) / delta + 2.0f) / 6.0f;
    } else {
        *h = ((rf - gf) / delta + 4.0f) / 6.0f;
    }
}

static void
HSVtoRGB(float h, float s, float v, int *r, int *g, int *b)
{
    float c = v * s;
    float hp = h * 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;

    if (hp < 1.0f)      { rf = c; gf = x; bf = 0; }
    else if (hp < 2.0f) { rf = x; gf = c; bf = 0; }
    else if (hp < 3.0f) { rf = 0; gf = c; bf = x; }
    else if (hp < 4.0f) { rf = 0; gf = x; bf = c; }
    else if (hp < 5.0f) { rf = x; gf = 0; bf = c; }
    else                { rf = c; gf = 0; bf = x; }

    *r = (int)((rf + m) * 255.0f + 0.5f);
    *g = (int)((gf + m) * 255.0f + 0.5f);
    *b = (int)((bf + m) * 255.0f + 0.5f);
    if (*r > 255) *r = 255;
    if (*g > 255) *g = 255;
    if (*b > 255) *b = 255;
}

static void
SyncHSVFromRGB(ColorPickerWidget cpw)
{
    RGBtoHSV(cpw->colorPicker.red, cpw->colorPicker.green,
             cpw->colorPicker.blue,
             &cpw->colorPicker.hue, &cpw->colorPicker.sat,
             &cpw->colorPicker.val);
}

static void
SyncRGBFromHSV(ColorPickerWidget cpw)
{
    HSVtoRGB(cpw->colorPicker.hue, cpw->colorPicker.sat,
             cpw->colorPicker.val,
             &cpw->colorPicker.red, &cpw->colorPicker.green,
             &cpw->colorPicker.blue);
}

static void
FireCallback(ColorPickerWidget cpw)
{
    IswColorPickerCallbackData cb;
    cb.red = cpw->colorPicker.red;
    cb.green = cpw->colorPicker.green;
    cb.blue = cpw->colorPicker.blue;
    IswCallCallbacks((Widget)cpw, IswNcolorChanged, (IswPointer)&cb);
}

/* --- Hex text field --- */

static void
UpdateHexFromRGB(ColorPickerWidget cpw)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X",
             cpw->colorPicker.red, cpw->colorPicker.green,
             cpw->colorPicker.blue);
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgString(&ab, buf);
    IswSetValues(cpw->colorPicker.hexW, ab.args, ab.count);
}

static void
HexCommitAction(Widget w, IswEvent *ev, String *params, Cardinal *num_params)
{
    (void)ev; (void)params; (void)num_params;
    Widget parent = IswParent(w);
    while (parent && IswClass(parent) != colorPickerWidgetClass)
        parent = IswParent(parent);
    if (!parent) return;

    ColorPickerWidget cpw = (ColorPickerWidget) parent;

    String str = NULL;
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgString(&ab, (IswArgVal)&str);
    IswGetValues(w, ab.args, ab.count);

    if (!str || *str == '\0') return;

    const char *p = str;
    if (*p == '#') p++;
    unsigned int hex;
    if (sscanf(p, "%6x", &hex) != 1) return;
    int len = 0;
    for (const char *c = p; *c && len < 7; c++, len++) {
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') ||
              (*c >= 'A' && *c <= 'F')))
            return;
    }

    if (len == 6) {
        cpw->colorPicker.red   = (hex >> 16) & 0xFF;
        cpw->colorPicker.green = (hex >> 8) & 0xFF;
        cpw->colorPicker.blue  = hex & 0xFF;
    } else if (len == 3) {
        int r = (hex >> 8) & 0xF;
        int g = (hex >> 4) & 0xF;
        int b = hex & 0xF;
        cpw->colorPicker.red   = r | (r << 4);
        cpw->colorPicker.green = g | (g << 4);
        cpw->colorPicker.blue  = b | (b << 4);
    } else {
        return;
    }

    SyncHSVFromRGB(cpw);
    IswSpinBoxSetValue(cpw->colorPicker.redSpinBox, cpw->colorPicker.red);
    IswSpinBoxSetValue(cpw->colorPicker.greenSpinBox, cpw->colorPicker.green);
    IswSpinBoxSetValue(cpw->colorPicker.blueSpinBox, cpw->colorPicker.blue);
    UpdateHexFromRGB(cpw);
    UpdateSwatch(cpw);

    Widget hueW = cpw->colorPicker.hueArea;
    if (hueW && (IswIsRealized(hueW) || hueW->core.windowless_realized))
        _IswRepaintWindowless(hueW);
    Widget svW = cpw->colorPicker.svArea;
    if (svW && (IswIsRealized(svW) || svW->core.windowless_realized))
        _IswRepaintWindowless(svW);

    FireCallback(cpw);
}

/* --- Pixel buffer builders --- */

static void
RebuildHuePixels(ColorPickerWidget cpw, int w, int h)
{
    free(cpw->colorPicker.hue_pixels);
    cpw->colorPicker.hue_pixels = malloc(w * h * 4);
    if (!cpw->colorPicker.hue_pixels) return;

    unsigned char *p = cpw->colorPicker.hue_pixels;
    for (int y = 0; y < h; y++) {
        float hue = (float)y / (float)(h - 1);
        int r, g, b;
        HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
        for (int x = 0; x < w; x++) {
            *p++ = (unsigned char)r;
            *p++ = (unsigned char)g;
            *p++ = (unsigned char)b;
            *p++ = 255;
        }
    }
}

static void
RebuildSVPixels(ColorPickerWidget cpw, int w, int h)
{
    free(cpw->colorPicker.sv_pixels);
    cpw->colorPicker.sv_pixels = malloc(w * h * 4);
    if (!cpw->colorPicker.sv_pixels) return;

    float hue = cpw->colorPicker.hue;
    unsigned char *p = cpw->colorPicker.sv_pixels;
    for (int y = 0; y < h; y++) {
        float val = 1.0f - (float)y / (float)(h - 1);
        for (int x = 0; x < w; x++) {
            float sat = (float)x / (float)(w - 1);
            int r, g, b;
            HSVtoRGB(hue, sat, val, &r, &g, &b);
            *p++ = (unsigned char)r;
            *p++ = (unsigned char)g;
            *p++ = (unsigned char)b;
            *p++ = 255;
        }
    }
}

/* --- Swatch expose callback --- */

static void
SwatchExpose(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *) call_data;
    ISWRenderContext *ctx = cb->render_ctx;

    ISWRenderSetColorRGBA(ctx,
        (double)cpw->colorPicker.red / 255.0,
        (double)cpw->colorPicker.green / 255.0,
        (double)cpw->colorPicker.blue / 255.0,
        1.0);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);
}

static void
UpdateSwatch(ColorPickerWidget cpw)
{
    Widget sw = cpw->colorPicker.swatchW;
    if (sw && (IswIsRealized(sw) || sw->core.windowless_realized))
        _IswRepaintWindowless(sw);
}

/* --- Hue strip callbacks --- */

static void
HueExpose(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *) call_data;
    ISWRenderContext *ctx = cb->render_ctx;
    int width = w->core.width;
    int height = w->core.height;

    if (width <= 0 || height <= 0) return;

    RebuildHuePixels(cpw, width, height);
    if (!cpw->colorPicker.hue_pixels) return;

    ISWRenderDrawImageRGBA(ctx, cpw->colorPicker.hue_pixels,
                           width, height, 0, 0, width, height);

    /* Draw indicator line at current hue */
    int hy = (int)(cpw->colorPicker.hue * (height - 1) + 0.5f);
    ISWRenderSetColorRGBA(ctx, 1.0, 1.0, 1.0, 1.0);
    ISWRenderSetLineWidth(ctx, 2.0);
    ISWRenderDrawLine(ctx, 0, hy, width, hy);
    ISWRenderSetColorRGBA(ctx, 0.0, 0.0, 0.0, 1.0);
    ISWRenderSetLineWidth(ctx, 1.0);
    ISWRenderDrawLine(ctx, 0, hy - 1, width, hy - 1);
    ISWRenderDrawLine(ctx, 0, hy + 1, width, hy + 1);
}

static void
HueInput(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *) call_data;
    IswEvent *ev = cb->event;
    if (!ev) return;

    if (ev->kind == IswButtonDown) {
        cpw->colorPicker.dragging_hue = True;
    } else if (ev->kind == IswButtonUp) {
        cpw->colorPicker.dragging_hue = False;
    }

    if (ev->kind == IswButtonDown || ev->kind == IswMotion) {
        if (ev->kind == IswMotion && !cpw->colorPicker.dragging_hue)
            return;

        int height = w->core.height;
        if (height <= 1) return;
        int y = IswEventY(ev);
        if (y < 0) y = 0;
        if (y >= height) y = height - 1;

        cpw->colorPicker.hue = (float)y / (float)(height - 1);
        SyncRGBFromHSV(cpw);

        IswSpinBoxSetValue(cpw->colorPicker.redSpinBox, cpw->colorPicker.red);
        IswSpinBoxSetValue(cpw->colorPicker.greenSpinBox, cpw->colorPicker.green);
        IswSpinBoxSetValue(cpw->colorPicker.blueSpinBox, cpw->colorPicker.blue);

        UpdateSwatch(cpw);
        UpdateHexFromRGB(cpw);
        _IswRepaintWindowless(w);
        Widget svw = cpw->colorPicker.svArea;
        if (svw && (IswIsRealized(svw) || svw->core.windowless_realized))
            _IswRepaintWindowless(svw);

        FireCallback(cpw);
    }
}

/* --- SV area callbacks --- */

static void
SVExpose(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *) call_data;
    ISWRenderContext *ctx = cb->render_ctx;
    int width = w->core.width;
    int height = w->core.height;

    if (width <= 0 || height <= 0) return;

    RebuildSVPixels(cpw, width, height);
    if (!cpw->colorPicker.sv_pixels) return;

    ISWRenderDrawImageRGBA(ctx, cpw->colorPicker.sv_pixels,
                           width, height, 0, 0, width, height);

    /* Draw crosshair at current sat/val position */
    int cx = (int)(cpw->colorPicker.sat * (width - 1) + 0.5f);
    int cy = (int)((1.0f - cpw->colorPicker.val) * (height - 1) + 0.5f);

    ISWRenderSetLineWidth(ctx, 2.0);
    ISWRenderSetColorRGBA(ctx, 1.0, 1.0, 1.0, 1.0);
    ISWRenderDrawLine(ctx, cx - 5, cy, cx + 5, cy);
    ISWRenderDrawLine(ctx, cx, cy - 5, cx, cy + 5);
    ISWRenderSetColorRGBA(ctx, 0.0, 0.0, 0.0, 1.0);
    ISWRenderSetLineWidth(ctx, 1.0);
    ISWRenderDrawLine(ctx, cx - 6, cy, cx - 5, cy);
    ISWRenderDrawLine(ctx, cx + 5, cy, cx + 6, cy);
    ISWRenderDrawLine(ctx, cx, cy - 6, cx, cy - 5);
    ISWRenderDrawLine(ctx, cx, cy + 5, cx, cy + 6);
}

static void
SVInput(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    ISWDrawingCallbackData *cb = (ISWDrawingCallbackData *) call_data;
    IswEvent *ev = cb->event;
    if (!ev) return;

    if (ev->kind == IswButtonDown) {
        cpw->colorPicker.dragging_sv = True;
    } else if (ev->kind == IswButtonUp) {
        cpw->colorPicker.dragging_sv = False;
    }

    if (ev->kind == IswButtonDown || ev->kind == IswMotion) {
        if (ev->kind == IswMotion && !cpw->colorPicker.dragging_sv)
            return;

        int width = w->core.width;
        int height = w->core.height;
        if (width <= 1 || height <= 1) return;

        int x = IswEventX(ev);
        int y = IswEventY(ev);
        if (x < 0) x = 0;
        if (x >= width) x = width - 1;
        if (y < 0) y = 0;
        if (y >= height) y = height - 1;

        cpw->colorPicker.sat = (float)x / (float)(width - 1);
        cpw->colorPicker.val = 1.0f - (float)y / (float)(height - 1);
        SyncRGBFromHSV(cpw);

        IswSpinBoxSetValue(cpw->colorPicker.redSpinBox, cpw->colorPicker.red);
        IswSpinBoxSetValue(cpw->colorPicker.greenSpinBox, cpw->colorPicker.green);
        IswSpinBoxSetValue(cpw->colorPicker.blueSpinBox, cpw->colorPicker.blue);

        UpdateSwatch(cpw);
        UpdateHexFromRGB(cpw);
        _IswRepaintWindowless(w);

        FireCallback(cpw);
    }
}

/* --- SpinBox callback --- */

static void
SpinBoxChanged(Widget w, IswPointer client_data, IswPointer call_data)
{
    ColorPickerWidget cpw = (ColorPickerWidget) client_data;
    IswSpinBoxCallbackData *sd = (IswSpinBoxCallbackData *) call_data;
    (void)w;

    if (w == cpw->colorPicker.redSpinBox)
        cpw->colorPicker.red = sd->value;
    else if (w == cpw->colorPicker.greenSpinBox)
        cpw->colorPicker.green = sd->value;
    else if (w == cpw->colorPicker.blueSpinBox)
        cpw->colorPicker.blue = sd->value;

    SyncHSVFromRGB(cpw);
    UpdateSwatch(cpw);
    UpdateHexFromRGB(cpw);

    Widget hueW = cpw->colorPicker.hueArea;
    if (hueW && (IswIsRealized(hueW) || hueW->core.windowless_realized))
        _IswRepaintWindowless(hueW);
    Widget svW = cpw->colorPicker.svArea;
    if (svW && (IswIsRealized(svW) || svW->core.windowless_realized))
        _IswRepaintWindowless(svW);

    FireCallback(cpw);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ColorPickerWidget cpw = (ColorPickerWidget) new;
    IswArgBuilder ab = IswArgBuilderInit();
    Dimension sv_sz   = 150;
    Dimension hue_w   = 20;
    Dimension hue_h   = sv_sz;
    Dimension swatch_w = 40;
    Dimension swatch_h = 30;
    Dimension spin_w  = 70;
    Dimension spin_h  = 26;
    Dimension label_w = 20;
    (void)request; (void)args; (void)num_args;

    cpw->colorPicker.hue_pixels = NULL;
    cpw->colorPicker.sv_pixels = NULL;
    cpw->colorPicker.dragging_hue = False;
    cpw->colorPicker.dragging_sv = False;

    /* Clamp */
    if (cpw->colorPicker.red < 0) cpw->colorPicker.red = 0;
    if (cpw->colorPicker.red > 255) cpw->colorPicker.red = 255;
    if (cpw->colorPicker.green < 0) cpw->colorPicker.green = 0;
    if (cpw->colorPicker.green > 255) cpw->colorPicker.green = 255;
    if (cpw->colorPicker.blue < 0) cpw->colorPicker.blue = 0;
    if (cpw->colorPicker.blue > 255) cpw->colorPicker.blue = 255;

    SyncHSVFromRGB(cpw);

    /* SV area (top-left) */
    IswArgWidth(&ab, sv_sz);
    IswArgHeight(&ab, sv_sz);
    IswArgBorderWidth(&ab, 1);
    IswArgLeft(&ab, IswChainLeft);
    IswArgTop(&ab, IswChainTop);
    cpw->colorPicker.svArea = IswCreateManagedWidget(
        "svArea", drawingAreaWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.svArea, IswNexposeCallback,
                  SVExpose, (IswPointer)cpw);
    IswAddCallback(cpw->colorPicker.svArea, IswNinputCallback,
                  SVInput, (IswPointer)cpw);

    /* Hue strip (right of SV area) */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, hue_w);
    IswArgHeight(&ab, hue_h);
    IswArgBorderWidth(&ab, 1);
    IswArgFromHoriz(&ab, cpw->colorPicker.svArea);
    IswArgTop(&ab, IswChainTop);
    cpw->colorPicker.hueArea = IswCreateManagedWidget(
        "hueArea", drawingAreaWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.hueArea, IswNexposeCallback,
                  HueExpose, (IswPointer)cpw);
    IswAddCallback(cpw->colorPicker.hueArea, IswNinputCallback,
                  HueInput, (IswPointer)cpw);

    /* R label (right of hue strip) */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "R");
    IswArgBorderWidth(&ab, 0);
    IswArgWidth(&ab, label_w);
    IswArgFromHoriz(&ab, cpw->colorPicker.hueArea);
    IswArgTop(&ab, IswChainTop);
    cpw->colorPicker.redLabel = IswCreateManagedWidget(
        "redLabel", labelWidgetClass, new, ab.args, ab.count);

    /* R spinbox */
    IswArgBuilderReset(&ab);
    IswArgSpinMinimum(&ab, 0);
    IswArgSpinMaximum(&ab, 255);
    IswArgSpinValue(&ab, cpw->colorPicker.red);
    IswArgWidth(&ab, spin_w);
    IswArgHeight(&ab, spin_h);
    IswArgFromHoriz(&ab, cpw->colorPicker.redLabel);
    IswArgTop(&ab, IswChainTop);
    cpw->colorPicker.redSpinBox = IswCreateManagedWidget(
        "redSpin", spinBoxWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.redSpinBox, IswNvalueChanged,
                  SpinBoxChanged, (IswPointer)cpw);

    /* G label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "G");
    IswArgBorderWidth(&ab, 0);
    IswArgWidth(&ab, label_w);
    IswArgFromHoriz(&ab, cpw->colorPicker.hueArea);
    IswArgFromVert(&ab, cpw->colorPicker.redLabel);
    cpw->colorPicker.greenLabel = IswCreateManagedWidget(
        "greenLabel", labelWidgetClass, new, ab.args, ab.count);

    /* G spinbox */
    IswArgBuilderReset(&ab);
    IswArgSpinMinimum(&ab, 0);
    IswArgSpinMaximum(&ab, 255);
    IswArgSpinValue(&ab, cpw->colorPicker.green);
    IswArgWidth(&ab, spin_w);
    IswArgHeight(&ab, spin_h);
    IswArgFromHoriz(&ab, cpw->colorPicker.greenLabel);
    IswArgFromVert(&ab, cpw->colorPicker.redSpinBox);
    cpw->colorPicker.greenSpinBox = IswCreateManagedWidget(
        "greenSpin", spinBoxWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.greenSpinBox, IswNvalueChanged,
                  SpinBoxChanged, (IswPointer)cpw);

    /* B label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "B");
    IswArgBorderWidth(&ab, 0);
    IswArgWidth(&ab, label_w);
    IswArgFromHoriz(&ab, cpw->colorPicker.hueArea);
    IswArgFromVert(&ab, cpw->colorPicker.greenLabel);
    cpw->colorPicker.blueLabel = IswCreateManagedWidget(
        "blueLabel", labelWidgetClass, new, ab.args, ab.count);

    /* B spinbox */
    IswArgBuilderReset(&ab);
    IswArgSpinMinimum(&ab, 0);
    IswArgSpinMaximum(&ab, 255);
    IswArgSpinValue(&ab, cpw->colorPicker.blue);
    IswArgWidth(&ab, spin_w);
    IswArgHeight(&ab, spin_h);
    IswArgFromHoriz(&ab, cpw->colorPicker.blueLabel);
    IswArgFromVert(&ab, cpw->colorPicker.greenSpinBox);
    cpw->colorPicker.blueSpinBox = IswCreateManagedWidget(
        "blueSpin", spinBoxWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.blueSpinBox, IswNvalueChanged,
                  SpinBoxChanged, (IswPointer)cpw);

    /* Color swatch preview (below SV area) */
    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, swatch_w);
    IswArgHeight(&ab, swatch_h);
    IswArgBorderWidth(&ab, 1);
    IswArgFromVert(&ab, cpw->colorPicker.svArea);
    IswArgLeft(&ab, IswChainLeft);
    cpw->colorPicker.swatchW = IswCreateManagedWidget(
        "swatch", drawingAreaWidgetClass, new, ab.args, ab.count);
    IswAddCallback(cpw->colorPicker.swatchW, IswNexposeCallback,
                  SwatchExpose, (IswPointer)cpw);

    /* Hex text field (right of swatch) */
    char hex_init[8];
    snprintf(hex_init, sizeof(hex_init), "#%02X%02X%02X",
             cpw->colorPicker.red, cpw->colorPicker.green,
             cpw->colorPicker.blue);
    IswArgBuilderReset(&ab);
    IswArgString(&ab, hex_init);
    IswArgEditType(&ab, IswtextEdit);
    IswArgWidth(&ab, 70);
    IswArgHeight(&ab, swatch_h);
    IswArgBorderWidth(&ab, 1);
    IswArgFromVert(&ab, cpw->colorPicker.svArea);
    IswArgFromHoriz(&ab, cpw->colorPicker.swatchW);
    cpw->colorPicker.hexW = IswCreateManagedWidget(
        "hexField", textWidgetClass, new, ab.args, ab.count);
    {
        static char hex_translations[] = "<Key>Return: HexCommit()";
        IswOverrideTranslations(cpw->colorPicker.hexW,
            IswParseTranslationTable(hex_translations));
    }
}

static void
Destroy(Widget w)
{
    ColorPickerWidget cpw = (ColorPickerWidget) w;
    free(cpw->colorPicker.hue_pixels);
    free(cpw->colorPicker.sv_pixels);
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
        IswSpinBoxSetValue(dcpw->colorPicker.redSpinBox, dcpw->colorPicker.red);
        changed = TRUE;
    }
    if (ccpw->colorPicker.green != dcpw->colorPicker.green) {
        IswSpinBoxSetValue(dcpw->colorPicker.greenSpinBox, dcpw->colorPicker.green);
        changed = TRUE;
    }
    if (ccpw->colorPicker.blue != dcpw->colorPicker.blue) {
        IswSpinBoxSetValue(dcpw->colorPicker.blueSpinBox, dcpw->colorPicker.blue);
        changed = TRUE;
    }

    if (changed) {
        SyncHSVFromRGB(dcpw);
        UpdateSwatch(dcpw);
    }

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
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgColorRed(&ab, r);
    IswArgColorGreen(&ab, g);
    IswArgColorBlue(&ab, b);
    IswSetValues(w, ab.args, ab.count);
}
