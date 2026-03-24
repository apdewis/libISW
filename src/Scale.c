/*
 * Scale.c - Scale (slider) widget implementation
 *
 * A slider control for selecting an integer value within a range.
 * Supports horizontal and vertical orientations, optional tick marks,
 * and optional value display.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ScaleP.h>

#include "ISWXcbDraw.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/* Thumb dimensions (before HiDPI scaling) */
#define THUMB_WIDTH  12
#define THUMB_HEIGHT 20
#define TRACK_THICKNESS 4
#define TICK_LENGTH  6
#define VALUE_MARGIN 4

#define Offset(field) XtOffsetOf(ScaleRec, field)

static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        Offset(scale.foreground), XtRString, XtDefaultForeground},
    {XtNorientation, XtCOrientation, XtROrientation, sizeof(XtOrientation),
        Offset(scale.orientation), XtRImmediate, (XtPointer) XtorientHorizontal},
    {XtNminimumValue, XtCMinimumValue, XtRInt, sizeof(int),
        Offset(scale.minimum), XtRImmediate, (XtPointer) 0},
    {XtNmaximumValue, XtCMaximumValue, XtRInt, sizeof(int),
        Offset(scale.maximum), XtRImmediate, (XtPointer) 100},
    {XtNscaleValue, XtCScaleValue, XtRInt, sizeof(int),
        Offset(scale.value), XtRImmediate, (XtPointer) 0},
    {XtNtickInterval, XtCTickInterval, XtRInt, sizeof(int),
        Offset(scale.tick_interval), XtRImmediate, (XtPointer) 0},
    {XtNshowValue, XtCShowValue, XtRBoolean, sizeof(Boolean),
        Offset(scale.show_value), XtRImmediate, (XtPointer) True},
    {XtNlength, XtCLength, XtRDimension, sizeof(Dimension),
        Offset(scale.length), XtRImmediate, (XtPointer) 200},
    {XtNthickness, XtCThickness, XtRDimension, sizeof(Dimension),
        Offset(scale.thickness), XtRImmediate, (XtPointer) 30},
    {XtNvalueChanged, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(scale.value_changed), XtRCallback, NULL},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
        Offset(scale.font), XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
    {XtNfontSet, XtCFontSet, XtRFontSet, sizeof(ISWFontSet *),
        Offset(scale.fontset), XtRString, XtDefaultFontSet},
#endif
};

#undef Offset

/* Forward declarations */
static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void StartDrag(Widget, XEvent *, String *, Cardinal *);
static void Drag(Widget, XEvent *, String *, Cardinal *);
static void EndDrag(Widget, XEvent *, String *, Cardinal *);
static void JumpToPosition(Widget, XEvent *, String *, Cardinal *);

static char defaultTranslations[] =
    "<Btn1Down>:   StartDrag()\n\
     <Btn1Motion>: Drag()\n\
     <Btn1Up>:     EndDrag()";

static XtActionsRec actions[] = {
    {"StartDrag",      StartDrag},
    {"Drag",           Drag},
    {"EndDrag",        EndDrag},
    {"JumpToPosition", JumpToPosition},
};

ScaleClassRec scaleClassRec = {
  { /* core_class fields */
    /* superclass         */ (WidgetClass) &simpleClassRec,
    /* class_name         */ "Scale",
    /* widget_size        */ sizeof(ScaleRec),
    /* class_initialize   */ ClassInitialize,
    /* class_part_init    */ NULL,
    /* class_inited       */ FALSE,
    /* initialize         */ Initialize,
    /* initialize_hook    */ NULL,
    /* realize            */ Realize,
    /* actions            */ actions,
    /* num_actions        */ XtNumber(actions),
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
    /* get_values_hook    */ NULL,
    /* accept_focus       */ NULL,
    /* version            */ XtVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ defaultTranslations,
    /* query_geometry     */ XtInheritQueryGeometry,
    /* display_accelerator*/ XtInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* simple_class fields */
    /* change_sensitive   */ XtInheritChangeSensitive
  },
  { /* scale_class fields */
    /* empty              */ 0
  }
};

WidgetClass scaleWidgetClass = (WidgetClass)&scaleClassRec;

/* --- Geometry helpers --- */

/* Track margins: half the thumb size so the thumb center can reach the ends */
static Dimension
ThumbW(ScaleWidget sw)
{
    return ISWScaleDim((Widget)sw, THUMB_WIDTH);
}

static Dimension
ThumbH(ScaleWidget sw)
{
    return ISWScaleDim((Widget)sw, THUMB_HEIGHT);
}

static Dimension
TrackThick(ScaleWidget sw)
{
    return ISWScaleDim((Widget)sw, TRACK_THICKNESS);
}

static int ValueZoneHeight(ScaleWidget sw);

/* Pixel offset where the track zone begins */
static int
TrackZoneOffset(ScaleWidget sw)
{
    if (sw->scale.orientation == XtorientHorizontal)
        return 0;  /* horizontal: value is at top but track uses full width */
    /* vertical: value zone at top pushes track down */
    return ValueZoneHeight(sw);
}

/* The usable track length in pixels (thumb center can travel this range) */
static int
TrackLength(ScaleWidget sw)
{
    Dimension tw = ThumbW(sw);
    if (sw->scale.orientation == XtorientHorizontal)
        return (int)sw->core.width - (int)tw;
    else
        return (int)sw->core.height - TrackZoneOffset(sw) - (int)tw;
}

/* Convert a value to a pixel position (thumb center) */
static Position
ValueToPixel(ScaleWidget sw, int value)
{
    int range = sw->scale.maximum - sw->scale.minimum;
    int track = TrackLength(sw);
    Dimension half_thumb = ThumbW(sw) / 2;
    int offset = TrackZoneOffset(sw);

    if (range <= 0 || track <= 0)
        return (Position)(offset + (int)half_thumb);

    int clamped = value;
    if (clamped < sw->scale.minimum) clamped = sw->scale.minimum;
    if (clamped > sw->scale.maximum) clamped = sw->scale.maximum;

    int frac = (int)((long)(clamped - sw->scale.minimum) * track / range);

    /* Vertical: minimum at bottom, maximum at top */
    if (sw->scale.orientation == XtorientVertical)
        frac = track - frac;

    return (Position)(offset + (int)half_thumb + frac);
}

/* Convert a pixel position to a value */
static int
PixelToValue(ScaleWidget sw, Position pixel)
{
    int range = sw->scale.maximum - sw->scale.minimum;
    int track = TrackLength(sw);
    Dimension half_thumb = ThumbW(sw) / 2;
    int offset = TrackZoneOffset(sw);

    if (range <= 0 || track <= 0)
        return sw->scale.minimum;

    int pos = (int)pixel - offset - (int)half_thumb;
    if (pos < 0) pos = 0;
    if (pos > track) pos = track;

    /* Vertical: invert so bottom = minimum, top = maximum */
    if (sw->scale.orientation == XtorientVertical)
        pos = track - pos;

    return sw->scale.minimum + (int)((long)pos * range / track);
}

static void
UpdateThumbPos(ScaleWidget sw)
{
    sw->scale.thumb_pos = ValueToPixel(sw, sw->scale.value);
}

/* --- Extract position from XCB events --- */

static void
ExtractPosition(XEvent *event, Position *x, Position *y)
{
    uint8_t type = event->response_type & ~0x80;
    switch (type) {
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
        *x = ev->event_x; *y = ev->event_y;
        break;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *ev = (xcb_button_press_event_t *)event;
        *x = ev->event_x; *y = ev->event_y;
        break;
    }
    default:
        *x = 0; *y = 0;
    }
}

/* --- Widget methods --- */

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    XtSetTypeConverter(XtRString, XtROrientation, ISWCvtStringToOrientation,
                       (XtConvertArgList)NULL, 0, XtCacheNone, (XtDestructor)NULL);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ScaleWidget sw = (ScaleWidget) new;
    (void)request; (void)args; (void)num_args;

    /* HiDPI scaling */
    sw->scale.length = ISWScaleDim(new, sw->scale.length);
    sw->scale.thickness = ISWScaleDim(new, sw->scale.thickness);

    /* Default geometry */
    if (sw->core.width == 0)
        sw->core.width = (sw->scale.orientation == XtorientHorizontal)
            ? sw->scale.length : sw->scale.thickness;
    if (sw->core.height == 0)
        sw->core.height = (sw->scale.orientation == XtorientHorizontal)
            ? sw->scale.thickness : sw->scale.length;

    /* Clamp value */
    if (sw->scale.value < sw->scale.minimum)
        sw->scale.value = sw->scale.minimum;
    if (sw->scale.value > sw->scale.maximum)
        sw->scale.value = sw->scale.maximum;

    sw->scale.dragging = False;
    sw->scale.drag_offset = 0;
    sw->scale.render_ctx = NULL;

    UpdateThumbPos(sw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    ScaleWidget sw = (ScaleWidget) w;

    /* Chain up to Simple's realize */
    (*scaleWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    sw->scale.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

static void
Destroy(Widget w)
{
    ScaleWidget sw = (ScaleWidget) w;
    if (sw->scale.render_ctx)
        ISWRenderDestroy(sw->scale.render_ctx);
}

static void
Resize(Widget w)
{
    ScaleWidget sw = (ScaleWidget) w;
    UpdateThumbPos(sw);
    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);
}

/*
 * Layout zones (horizontal):
 *   Top zone:    value label (centered horizontally)
 *   Bottom zone: thumb + track + ticks
 *
 * Layout zones (vertical):
 *   Top zone:    value label (centered over track)
 *   Bottom zone: thumb + track + ticks
 */

static int
ValueZoneHeight(ScaleWidget sw)
{
    if (!sw->scale.show_value || !sw->scale.font)
        return 0;
    return ISWScaledFontHeight((Widget)sw, sw->scale.font)
         + (int)ISWScaleDim((Widget)sw, VALUE_MARGIN);
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    ScaleWidget sw = (ScaleWidget) w;
    ISWRenderContext *ctx = sw->scale.render_ctx;
    (void)event; (void)region;

    if (!ctx || !XtIsRealized(w))
        return;

    Dimension thumb_w = ThumbW(sw);
    Dimension thumb_h = ThumbH(sw);
    Dimension track_thick = TrackThick(sw);
    Dimension half_thumb = thumb_w / 2;
    int val_h = ValueZoneHeight(sw);

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, sw->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, sw->core.width, sw->core.height);

    ISWRenderSetColor(ctx, sw->scale.foreground);
    if (sw->scale.font)
        ISWRenderSetFont(ctx, sw->scale.font);

    if (sw->scale.orientation == XtorientHorizontal) {
        /* --- Horizontal layout --- */
        /* Track zone sits below the value zone */
        int track_zone_top = val_h;
        int track_zone_h = (int)sw->core.height - val_h;
        int track_center_y = track_zone_top + track_zone_h / 2;

        /* Track */
        int track_y = track_center_y - (int)track_thick / 2;
        ISWRenderFillRectangle(ctx, half_thumb, track_y,
                               sw->core.width - thumb_w, track_thick);

        /* Tick marks (below track) */
        if (sw->scale.tick_interval > 0) {
            Dimension tick_len = ISWScaleDim(w, TICK_LENGTH);
            int tick_y = track_y + (int)track_thick + 2;

            for (int v = sw->scale.minimum; v <= sw->scale.maximum;
                 v += sw->scale.tick_interval) {
                Position px = ValueToPixel(sw, v);
                ISWRenderDrawLine(ctx, px, tick_y, px, tick_y + (int)tick_len);
            }
            if ((sw->scale.maximum - sw->scale.minimum) % sw->scale.tick_interval != 0) {
                Position px = ValueToPixel(sw, sw->scale.maximum);
                ISWRenderDrawLine(ctx, px, tick_y, px, tick_y + (int)tick_len);
            }
        }

        /* Thumb (centered on track) */
        Position tx = sw->scale.thumb_pos - (Position)(thumb_w / 2);
        Position ty = track_center_y - (int)thumb_h / 2;
        ISWRenderFillRectangle(ctx, tx, ty, thumb_w, thumb_h);

        /* Value label (top-left) */
        if (sw->scale.show_value && sw->scale.font) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", sw->scale.value);
            int ly = ISWScaledFontAscent(w, sw->scale.font);
            ISWRenderDrawString(ctx, buf, (int)strlen(buf),
                                (int)half_thumb, ly);
        }
    } else {
        /* --- Vertical layout --- */
        /* Value zone at top, track zone below */
        int track_zone_top = val_h;
        int track_center_x = (int)sw->core.width / 2;

        /* Track */
        int track_x = track_center_x - (int)track_thick / 2;
        ISWRenderFillRectangle(ctx, track_x, track_zone_top + (int)half_thumb,
                               track_thick,
                               sw->core.height - track_zone_top - thumb_w);

        /* Tick marks (right of track) */
        if (sw->scale.tick_interval > 0) {
            Dimension tick_len = ISWScaleDim(w, TICK_LENGTH);
            int tick_x = track_x + (int)track_thick + 2;

            for (int v = sw->scale.minimum; v <= sw->scale.maximum;
                 v += sw->scale.tick_interval) {
                Position py = ValueToPixel(sw, v);
                ISWRenderDrawLine(ctx, tick_x, py, tick_x + (int)tick_len, py);
            }
            if ((sw->scale.maximum - sw->scale.minimum) % sw->scale.tick_interval != 0) {
                Position py = ValueToPixel(sw, sw->scale.maximum);
                ISWRenderDrawLine(ctx, tick_x, py, tick_x + (int)tick_len, py);
            }
        }

        /* Thumb (centered on track) */
        Position tx = track_center_x - (int)thumb_w / 2;
        Position ty = sw->scale.thumb_pos - (Position)(thumb_h / 2);
        ISWRenderFillRectangle(ctx, tx, ty, thumb_w, thumb_h);

        /* Value label (top, centered over track) */
        if (sw->scale.show_value && sw->scale.font) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", sw->scale.value);
            int text_w = ISWRenderTextWidth(ctx, buf, (int)strlen(buf));
            int lx = ((int)sw->core.width - text_w) / 2;
            int ly = ISWScaledFontAscent(w, sw->scale.font);
            ISWRenderDrawString(ctx, buf, (int)strlen(buf), lx, ly);
        }
    }

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired, ArgList args, Cardinal *num_args)
{
    ScaleWidget csw = (ScaleWidget) current;
    ScaleWidget dsw = (ScaleWidget) desired;
    Boolean redraw = FALSE;
    (void)request; (void)args; (void)num_args;

    /* Clamp value */
    if (dsw->scale.value < dsw->scale.minimum)
        dsw->scale.value = dsw->scale.minimum;
    if (dsw->scale.value > dsw->scale.maximum)
        dsw->scale.value = dsw->scale.maximum;

    if (csw->scale.value != dsw->scale.value ||
        csw->scale.minimum != dsw->scale.minimum ||
        csw->scale.maximum != dsw->scale.maximum ||
        csw->scale.foreground != dsw->scale.foreground ||
        csw->core.background_pixel != dsw->core.background_pixel ||
        csw->scale.orientation != dsw->scale.orientation ||
        csw->scale.tick_interval != dsw->scale.tick_interval ||
        csw->scale.show_value != dsw->scale.show_value) {
        UpdateThumbPos(dsw);
        redraw = TRUE;
    }

    return redraw;
}

/* --- Action procedures --- */

static void
SetValueAndNotify(ScaleWidget sw, int new_value)
{
    if (new_value < sw->scale.minimum) new_value = sw->scale.minimum;
    if (new_value > sw->scale.maximum) new_value = sw->scale.maximum;

    if (new_value != sw->scale.value) {
        sw->scale.value = new_value;
        UpdateThumbPos(sw);
        Redisplay((Widget)sw, NULL, 0);

        IswScaleCallbackData cb_data;
        cb_data.value = new_value;
        XtCallCallbacks((Widget)sw, XtNvalueChanged, (XtPointer)&cb_data);
    }
}

static void
StartDrag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ScaleWidget sw = (ScaleWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    ExtractPosition(event, &x, &y);

    Position pick = (sw->scale.orientation == XtorientHorizontal) ? x : y;
    Dimension half_thumb = ThumbW(sw) / 2;

    /* Check if click is on the thumb */
    if (pick >= sw->scale.thumb_pos - (Position)half_thumb &&
        pick <= sw->scale.thumb_pos + (Position)half_thumb) {
        sw->scale.dragging = True;
        sw->scale.drag_offset = (int)pick - (int)sw->scale.thumb_pos;
    } else {
        /* Jump to clicked position */
        sw->scale.dragging = True;
        sw->scale.drag_offset = 0;
        SetValueAndNotify(sw, PixelToValue(sw, pick));
    }
}

static void
Drag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ScaleWidget sw = (ScaleWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    if (!sw->scale.dragging)
        return;

    ExtractPosition(event, &x, &y);
    Position pick = (sw->scale.orientation == XtorientHorizontal) ? x : y;
    SetValueAndNotify(sw, PixelToValue(sw, pick - sw->scale.drag_offset));
}

static void
EndDrag(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ScaleWidget sw = (ScaleWidget) w;
    (void)event; (void)params; (void)num_params;
    sw->scale.dragging = False;
}

static void
JumpToPosition(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    ScaleWidget sw = (ScaleWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    ExtractPosition(event, &x, &y);
    Position pick = (sw->scale.orientation == XtorientHorizontal) ? x : y;
    sw->scale.dragging = False;
    SetValueAndNotify(sw, PixelToValue(sw, pick));
}

/* --- Public API --- */

void
IswScaleSetValue(Widget w, int value)
{
    ScaleWidget sw = (ScaleWidget) w;
    Arg args[1];

    XtSetArg(args[0], XtNscaleValue, value);
    XtSetValues(w, args, 1);

    /* Fire callback if value actually changed */
    if (sw->scale.value == value) {
        IswScaleCallbackData cb_data;
        cb_data.value = value;
        XtCallCallbacks(w, XtNvalueChanged, (XtPointer)&cb_data);
    }
}

int
IswScaleGetValue(Widget w)
{
    ScaleWidget sw = (ScaleWidget) w;
    return sw->scale.value;
}
