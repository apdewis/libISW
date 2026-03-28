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
#define THUMB_BAR_THICK 8   /* cross-track thickness of the knob */
#define TRACK_THICKNESS 4
#define TICK_LENGTH  6
#define VALUE_MARGIN 4

#define Offset(field) XtOffsetOf(ScaleRec, field)

static XtResource resources[] = {
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
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
    {XtNvaluePosition, XtCValuePosition, XtRInt, sizeof(IswScaleValuePosition),
        Offset(scale.value_pos), XtRImmediate, (XtPointer) IswScaleValueTop},
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

static void StartDrag(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void Drag(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void EndDrag(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void JumpToPosition(Widget, xcb_generic_event_t *, String *, Cardinal *);

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
static int ValueZoneWidth(ScaleWidget sw);

/*
 * How many pixels the track zone is offset from the widget origin.
 * For top/left value positions, the label zone pushes the track over/down.
 */
static int
TrackZoneOffsetX(ScaleWidget sw)
{
    if (!sw->scale.show_value)
        return 0;
    if (sw->scale.value_pos == IswScaleValueLeft)
        return ValueZoneWidth(sw);
    return 0;
}

static int
TrackZoneOffsetY(ScaleWidget sw)
{
    if (!sw->scale.show_value)
        return 0;
    if (sw->scale.value_pos == IswScaleValueTop)
        return ValueZoneHeight(sw);
    return 0;
}

/* The usable track length in pixels (thumb center can travel this range) */
static int
TrackLength(ScaleWidget sw)
{
    Dimension tw = ThumbW(sw);
    if (sw->scale.orientation == XtorientHorizontal)
        return (int)sw->core.width - TrackZoneOffsetX(sw) - (int)tw;
    else
        return (int)sw->core.height - TrackZoneOffsetY(sw) - (int)tw;
}

/* Convert a value to a pixel position (thumb center along the track axis) */
static Position
ValueToPixel(ScaleWidget sw, int value)
{
    int range = sw->scale.maximum - sw->scale.minimum;
    int track = TrackLength(sw);
    Dimension half_thumb = ThumbW(sw) / 2;
    int offset = (sw->scale.orientation == XtorientHorizontal)
                 ? TrackZoneOffsetX(sw) : TrackZoneOffsetY(sw);

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
    int offset = (sw->scale.orientation == XtorientHorizontal)
                 ? TrackZoneOffsetX(sw) : TrackZoneOffsetY(sw);

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
ExtractPosition(xcb_generic_event_t *event, Position *x, Position *y)
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

    /* Compute minimum cross-axis size from content:
     * thumb + value label zone + tick marks */
    Dimension thumb_cross = ISWScaleDim(new, THUMB_HEIGHT);
    Dimension tick_zone = 0;
    if (sw->scale.tick_interval > 0)
        tick_zone = ISWScaleDim(new, TICK_LENGTH) + 2;
    Dimension value_zone = 0;
    if (sw->scale.show_value && sw->scale.font) {
        value_zone = ISWScaledFontHeight(new, sw->scale.font)
                   + ISWScaleDim(new, VALUE_MARGIN);
    }
    Dimension min_cross = thumb_cross + tick_zone + value_zone;

    /* Default geometry */
    if (sw->core.width == 0)
        sw->core.width = (sw->scale.orientation == XtorientHorizontal)
            ? sw->scale.length : min_cross;
    if (sw->core.height == 0)
        sw->core.height = (sw->scale.orientation == XtorientHorizontal)
            ? min_cross : sw->scale.length;
    /* Enforce minimum so nothing gets clipped */
    if (sw->scale.orientation == XtorientHorizontal) {
        if (sw->core.height < min_cross)
            sw->core.height = min_cross;
    } else {
        if (sw->core.width < min_cross)
            sw->core.width = min_cross;
    }

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

static int
ValueZoneHeight(ScaleWidget sw)
{
    if (!sw->scale.show_value || !sw->scale.font)
        return 0;
    if (sw->scale.value_pos != IswScaleValueTop &&
        sw->scale.value_pos != IswScaleValueBottom)
        return 0;
    return ISWScaledFontHeight((Widget)sw, sw->scale.font)
         + (int)ISWScaleDim((Widget)sw, VALUE_MARGIN);
}

static int
ValueZoneWidth(ScaleWidget sw)
{
    if (!sw->scale.show_value || !sw->scale.font)
        return 0;
    if (sw->scale.value_pos != IswScaleValueLeft &&
        sw->scale.value_pos != IswScaleValueRight)
        return 0;
    /* Reserve space for widest possible value string */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", sw->scale.maximum);
    int w_max = ISWRenderTextWidth(sw->scale.render_ctx, buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "%d", sw->scale.minimum);
    int w_min = ISWRenderTextWidth(sw->scale.render_ctx, buf, (int)strlen(buf));
    int wid = (w_max > w_min) ? w_max : w_min;
    return wid + (int)ISWScaleDim((Widget)sw, VALUE_MARGIN);
}

/*
 * DrawValueLabel - draw the value text outside the slider area.
 * area_x/y/w/h is the bounding box of the full slider zone (thumb extent),
 * not just the thin track line.
 */
static void
DrawValueLabel(Widget w, ScaleWidget sw, ISWRenderContext *ctx,
               int area_x, int area_y, int area_w, int area_h)
{
    if (!sw->scale.show_value || !sw->scale.font)
        return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", sw->scale.value);
    int text_w = ISWRenderTextWidth(ctx, buf, (int)strlen(buf));
    int font_asc = ISWScaledFontAscent(w, sw->scale.font);
    int margin = (int)ISWScaleDim(w, VALUE_MARGIN);
    int lx, ly;

    switch (sw->scale.value_pos) {
    case IswScaleValueTop:
        lx = area_x;
        ly = area_y - margin;  /* baseline sits above the slider area */
        break;
    case IswScaleValueBottom:
        lx = area_x;
        ly = area_y + area_h + margin + font_asc;
        break;
    case IswScaleValueLeft:
        lx = area_x - margin - text_w;
        ly = area_y + area_h / 2 + font_asc / 2;
        break;
    case IswScaleValueRight:
        lx = area_x + area_w + margin;
        ly = area_y + area_h / 2 + font_asc / 2;
        break;
    default:
        return;
    }

    /* Clamp to widget bounds */
    if (lx < 0) lx = 0;
    if (ly < font_asc) ly = font_asc;
    if (lx + text_w > (int)sw->core.width)
        lx = (int)sw->core.width - text_w;
    if (ly > (int)sw->core.height)
        ly = (int)sw->core.height;

    ISWRenderDrawString(ctx, buf, (int)strlen(buf), lx, ly);
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
    int off_x = TrackZoneOffsetX(sw);
    int off_y = TrackZoneOffsetY(sw);

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, sw->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, sw->core.width, sw->core.height);

    ISWRenderSetColor(ctx, sw->scale.foreground);
    if (sw->scale.font)
        ISWRenderSetFont(ctx, sw->scale.font);

    if (sw->scale.orientation == XtorientHorizontal) {
        /* --- Horizontal layout --- */
        int track_zone_h = (int)sw->core.height - off_y;
        int track_center_y = off_y + track_zone_h / 2;

        /* Track */
        int track_y = track_center_y - (int)track_thick / 2;
        int track_x = off_x + (int)half_thumb;
        int track_w = (int)sw->core.width - off_x - (int)thumb_w;
        ISWRenderFillRectangle(ctx, track_x, track_y, track_w, track_thick);

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

        /* Thumb — thin bar perpendicular to track, radiused on the ends */
        {
            Dimension bar_thick = ISWScaleDim(w, THUMB_BAR_THICK);
            Position tx = sw->scale.thumb_pos - (Position)(bar_thick / 2);
            Position ty = track_center_y - (int)thumb_h / 2;
            ISWRenderFillRoundedRectangle(ctx, tx, ty, bar_thick, thumb_h,
                                          bar_thick / 2.0);
        }

        /* Value label — area is the full thumb extent */
        int area_y = track_center_y - (int)thumb_h / 2;
        DrawValueLabel(w, sw, ctx, track_x, area_y, track_w, (int)thumb_h);

    } else {
        /* --- Vertical layout --- */
        int track_zone_w = (int)sw->core.width - off_x;
        int track_center_x = off_x + track_zone_w / 2;

        /* Track */
        int track_x = track_center_x - (int)track_thick / 2;
        int track_top = off_y + (int)half_thumb;
        int track_h = (int)sw->core.height - off_y - (int)thumb_w;
        ISWRenderFillRectangle(ctx, track_x, track_top, track_thick, track_h);

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

        /* Thumb — same shape as horizontal, rotated 90 degrees */
        {
            Dimension bar_thick = ISWScaleDim(w, THUMB_BAR_THICK);
            Position tx = track_center_x - (int)thumb_h / 2;
            Position ty = sw->scale.thumb_pos - (Position)(bar_thick / 2);
            ISWRenderFillRoundedRectangle(ctx, tx, ty, thumb_h, bar_thick,
                                          bar_thick / 2.0);
        }

        /* Value label — area is the full thumb extent */
        int area_x = track_center_x - (int)thumb_w / 2;
        DrawValueLabel(w, sw, ctx, area_x, track_top, (int)thumb_w, track_h);
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
        csw->scale.show_value != dsw->scale.show_value ||
        csw->scale.value_pos != dsw->scale.value_pos) {
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
StartDrag(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
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
Drag(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
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
EndDrag(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ScaleWidget sw = (ScaleWidget) w;
    (void)event; (void)params; (void)num_params;
    sw->scale.dragging = False;
}

static void
JumpToPosition(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
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
