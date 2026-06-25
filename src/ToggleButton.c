#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ToggleButtonP.h>
#include <ISW/ISWPlatform.h>

static char defaultTranslations[] =
    "<Btn1Down>,<Btn1Up>:   toggle() notify()\n\
     <Key>space:            toggle() notify()\n\
     <Key>Return:           toggle() notify()";

#define offset(field) IswOffsetOf(ToggleButtonRec, field)
static IswResource resources[] = {
    {IswNstate, IswCState, IswRBoolean, sizeof(Boolean),
       offset(command.set), IswRString, (IswPointer)"off"},
    {IswNimageOn, IswCImageOn, IswRString, sizeof(String),
       offset(toggle_button.image_on_source), IswRImmediate, (IswPointer)NULL},
    {IswNimageOff, IswCImageOff, IswRString, sizeof(String),
       offset(toggle_button.image_off_source), IswRImmediate, (IswPointer)NULL},
};
#undef offset

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void Toggle(Widget, IswEvent *, String *, Cardinal *);
static void Notify(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actionsList[] = {
    {"toggle", Toggle},
    {"notify", Notify},
};

#define SuperClass ((CommandWidgetClass)&commandClassRec)
#define LabelClass ((LabelWidgetClass)&labelClassRec)

ToggleButtonClassRec toggleButtonClassRec = {
  {
    (WidgetClass) SuperClass,
    "ToggleButton",
    sizeof(ToggleButtonRec),
    ClassInitialize,
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
    FALSE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
    IswInheritResize,
    Redisplay,
    SetValues,
    NULL,
    IswInheritSetValuesAlmost,
    NULL,
    NULL,
    IswVersion,
    NULL,
    defaultTranslations,
    IswInheritQueryGeometry,
    IswInheritDisplayAccelerator,
    NULL
  },
  { IswInheritChangeSensitive },
  { 0 },
  { 0 },
  { 0 },
};

WidgetClass toggleButtonWidgetClass = (WidgetClass) &toggleButtonClassRec;

static ISWImage *
LoadImage(Widget w, const char *source)
{
    LabelWidget lw = (LabelWidget) w;
    float dpi = (float)(96.0 * ISWScaleFactor(w));
    char fg_hex[8];
    IswDisplay dpy = w->core.display;
    IswColormap cmap = w->core.colormap;
    unsigned long pixel = (unsigned long)lw->label.foreground;
    const char *color = NULL;
    IswColor c;

    if (_IswPlatformQueryColor(dpy, cmap, pixel, &c)) {
        snprintf(fg_hex, sizeof(fg_hex), "#%02X%02X%02X",
                 c.red >> 8, c.green >> 8, c.blue >> 8);
        color = fg_hex;
    }
    return ISWImageLoad(source, (double)dpi, color);
}

static void
ApplyCurrentImage(ToggleButtonWidget tbw)
{
    ISWImage *target = tbw->command.set
        ? tbw->toggle_button.image_on
        : tbw->toggle_button.image_off;
    if (target)
        tbw->label.image = target;
}

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ToggleButtonWidget tbw = (ToggleButtonWidget) new;

    tbw->toggle_button.image_on_source = tbw->toggle_button.image_on_source
        ? IswNewString(tbw->toggle_button.image_on_source) : NULL;
    tbw->toggle_button.image_off_source = tbw->toggle_button.image_off_source
        ? IswNewString(tbw->toggle_button.image_off_source) : NULL;

    tbw->toggle_button.image_on = tbw->toggle_button.image_on_source
        ? LoadImage(new, tbw->toggle_button.image_on_source) : NULL;
    tbw->toggle_button.image_off = tbw->toggle_button.image_off_source
        ? LoadImage(new, tbw->toggle_button.image_off_source) : NULL;

    if (tbw->toggle_button.image_on || tbw->toggle_button.image_off) {
        if (tbw->label.image
            && tbw->label.image != tbw->toggle_button.image_on
            && tbw->label.image != tbw->toggle_button.image_off) {
            ISWImageDestroy(tbw->label.image);
        }
        tbw->label.image = NULL;
        ApplyCurrentImage(tbw);

        if (tbw->label.image) {
            double sf = ISWImageIsVector(tbw->label.image)
                           ? ISWScaleFactor(new) : 1.0;
            Dimension iw = (Dimension)(ISWImageGetWidth(tbw->label.image) / sf + 0.5);
            Dimension ih = (Dimension)(ISWImageGetHeight(tbw->label.image) / sf + 0.5);
            tbw->label.label_width = iw;
            tbw->label.label_height = ih;
            tbw->label.label_len = 0;
            if (request->core.width == 0)
                new->core.width = iw + 2 * tbw->label.internal_width;
            if (request->core.height == 0)
                new->core.height = ih + 2 * tbw->label.internal_height;
        }
    }

    tbw->command.highlighted = HighlightNone;
    ((SimpleWidget) new)->simple.traversal_on = True;
}

static void
Destroy(Widget w)
{
    ToggleButtonWidget tbw = (ToggleButtonWidget) w;

    if (tbw->label.image == tbw->toggle_button.image_on
        || tbw->label.image == tbw->toggle_button.image_off)
        tbw->label.image = NULL;

    if (tbw->toggle_button.image_on) {
        ISWImageDestroy(tbw->toggle_button.image_on);
        tbw->toggle_button.image_on = NULL;
    }
    if (tbw->toggle_button.image_off) {
        ISWImageDestroy(tbw->toggle_button.image_off);
        tbw->toggle_button.image_off = NULL;
    }
    if (tbw->toggle_button.image_on_source) {
        IswFree((char *)tbw->toggle_button.image_on_source);
        tbw->toggle_button.image_on_source = NULL;
    }
    if (tbw->toggle_button.image_off_source) {
        IswFree((char *)tbw->toggle_button.image_off_source);
        tbw->toggle_button.image_off_source = NULL;
    }
}

static void
Toggle(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ToggleButtonWidget tbw = (ToggleButtonWidget) w;

    tbw->command.set = !tbw->command.set;
    ApplyCurrentImage(tbw);

    if (IswIsRealized(w))
        Redisplay(w, iswev, 0);
}

static void
Notify(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ToggleButtonWidget tbw = (ToggleButtonWidget) w;
    long state = tbw->command.set;

    IswCallCallbacks(w, IswNcallback, (IswPointer) state);
}

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    (*SuperClass->core_class.expose)(w, event, region);
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList args, Cardinal *num_args)
{
    ToggleButtonWidget oldtbw = (ToggleButtonWidget) current;
    ToggleButtonWidget tbw = (ToggleButtonWidget) new;
    Boolean redisplay = False;

    if (oldtbw->toggle_button.image_on_source != tbw->toggle_button.image_on_source) {
        if (oldtbw->toggle_button.image_on_source)
            IswFree((char *)oldtbw->toggle_button.image_on_source);
        tbw->toggle_button.image_on_source = tbw->toggle_button.image_on_source
            ? IswNewString(tbw->toggle_button.image_on_source) : NULL;
        if (tbw->toggle_button.image_on)
            ISWImageDestroy(tbw->toggle_button.image_on);
        tbw->toggle_button.image_on = tbw->toggle_button.image_on_source
            ? LoadImage(new, tbw->toggle_button.image_on_source) : NULL;
        redisplay = True;
    }

    if (oldtbw->toggle_button.image_off_source != tbw->toggle_button.image_off_source) {
        if (oldtbw->toggle_button.image_off_source)
            IswFree((char *)oldtbw->toggle_button.image_off_source);
        tbw->toggle_button.image_off_source = tbw->toggle_button.image_off_source
            ? IswNewString(tbw->toggle_button.image_off_source) : NULL;
        if (tbw->toggle_button.image_off)
            ISWImageDestroy(tbw->toggle_button.image_off);
        tbw->toggle_button.image_off = tbw->toggle_button.image_off_source
            ? LoadImage(new, tbw->toggle_button.image_off_source) : NULL;
        redisplay = True;
    }

    if (oldtbw->command.set != tbw->command.set || redisplay) {
        ApplyCurrentImage(tbw);
        redisplay = True;
    }

    return redisplay;
}
