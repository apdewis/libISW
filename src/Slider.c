/*
 * Slider.c - Slider widget implementation
 *
 * A slider control for selecting an integer value within a range.
 * Supports horizontal and vertical orientations, optional tick marks,
 * and optional value display.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/SliderP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/IswArgMacros.h>

#include "ISWXcbDraw.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/* Thumb dimensions (before HiDPI scaling) */
#define THUMB_SIZE 14
#define TRACK_THICKNESS 4
#define TICK_LENGTH  6
#define VALUE_MARGIN 4

#define Offset(field) IswOffsetOf(SliderRec, field)

static IswResource resources[] = {
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
        Offset(slider.foreground), IswRString, IswDefaultForeground},
    {IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
        Offset(slider.orientation), IswRImmediate, (IswPointer) IswOrientHorizontal},
    {IswNminimumValue, IswCMinimumValue, IswRInt, sizeof(int),
        Offset(slider.minimum), IswRImmediate, (IswPointer) 0},
    {IswNmaximumValue, IswCMaximumValue, IswRInt, sizeof(int),
        Offset(slider.maximum), IswRImmediate, (IswPointer) 100},
    {IswNsliderValue, IswCSliderValue, IswRInt, sizeof(int),
        Offset(slider.value), IswRImmediate, (IswPointer) 0},
    {IswNtickInterval, IswCTickInterval, IswRInt, sizeof(int),
        Offset(slider.tick_interval), IswRImmediate, (IswPointer) 0},
    {IswNshowValue, IswCShowValue, IswRBoolean, sizeof(Boolean),
        Offset(slider.show_value), IswRImmediate, (IswPointer) True},
    {IswNvaluePosition, IswCValuePosition, IswRInt, sizeof(IswSliderValuePosition),
        Offset(slider.value_pos), IswRImmediate, (IswPointer) IswSliderValueTop},
    {IswNlength, IswCLength, IswRDimension, sizeof(Dimension),
        Offset(slider.length), IswRImmediate, (IswPointer) 200},
    {IswNthickness, IswCThickness, IswRDimension, sizeof(Dimension),
        Offset(slider.thickness), IswRImmediate, (IswPointer) 30},
    {IswNvalueChanged, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(slider.value_changed), IswRCallback, NULL},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
        Offset(slider.font), IswRString, IswDefaultFont},
};

#undef Offset

/* Forward declarations */
static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void StartDrag(Widget, IswEvent *, String *, Cardinal *);
static void Drag(Widget, IswEvent *, String *, Cardinal *);
static void EndDrag(Widget, IswEvent *, String *, Cardinal *);
static void JumpToPosition(Widget, IswEvent *, String *, Cardinal *);
static void Increment(Widget, IswEvent *, String *, Cardinal *);
static void Decrement(Widget, IswEvent *, String *, Cardinal *);
static void PageIncrement(Widget, IswEvent *, String *, Cardinal *);
static void PageDecrement(Widget, IswEvent *, String *, Cardinal *);
static void SetMin(Widget, IswEvent *, String *, Cardinal *);
static void SetMax(Widget, IswEvent *, String *, Cardinal *);
static int ValueZoneHeight(SliderWidget sw);
static int ValueZoneWidth(SliderWidget sw);

static char defaultTranslations[] =
    "<Btn1Down>:   StartDrag()\n\
     <Btn1Motion>: Drag()\n\
     <Btn1Up>:     EndDrag()\n\
     <Key>Left:    Decrement()\n\
     <Key>Down:    Decrement()\n\
     <Key>Right:   Increment()\n\
     <Key>Up:      Increment()\n\
     <Key>Page_Up: PageIncrement()\n\
     <Key>Page_Down: PageDecrement()\n\
     <Key>Home:    SetMin()\n\
     <Key>End:     SetMax()";

static IswActionsRec actions[] = {
    {"StartDrag",      StartDrag},
    {"Drag",           Drag},
    {"EndDrag",        EndDrag},
    {"JumpToPosition", JumpToPosition},
    {"Increment",      Increment},
    {"Decrement",      Decrement},
    {"PageIncrement",  PageIncrement},
    {"PageDecrement",  PageDecrement},
    {"SetMin",         SetMin},
    {"SetMax",         SetMax},
};

SliderClassRec sliderClassRec = {
  { /* core_class fields */
    /* superclass         */ (WidgetClass) &simpleClassRec,
    /* class_name         */ "Slider",
    /* widget_size        */ sizeof(SliderRec),
    /* class_initialize   */ ClassInitialize,
    /* class_part_init    */ NULL,
    /* class_inited       */ FALSE,
    /* initialize         */ Initialize,
    /* initialize_hook    */ NULL,
    /* realize            */ Realize,
    /* actions            */ actions,
    /* num_actions        */ IswNumber(actions),
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
    /* get_values_hook    */ NULL,
    /* accept_focus       */ NULL,
    /* version            */ IswVersion,
    /* callback_private   */ NULL,
    /* tm_table           */ defaultTranslations,
    /* query_geometry     */ IswInheritQueryGeometry,
    /* display_accelerator*/ IswInheritDisplayAccelerator,
    /* extension          */ NULL
  },
  { /* simple_class fields */
    /* change_sensitive   */ IswInheritChangeSensitive
  },
  { /* slider_class fields */
    /* empty              */ 0
  }
};

WidgetClass sliderWidgetClass = (WidgetClass)&sliderClassRec;

/*
 * How many pixels the track zone is offset from the widget origin.
 * For top/left value positions, the label zone pushes the track over/down.
 */
static int
TrackZoneOffsetX(SliderWidget sw)
{
    if (!sw->slider.show_value)
        return 1;
    if (sw->slider.value_pos == IswSliderValueLeft)
        return ValueZoneWidth(sw);
    return 1;
}

static int
TrackZoneOffsetY(SliderWidget sw)
{
    if (!sw->slider.show_value)
        return 0;
    if (sw->slider.value_pos == IswSliderValueTop)
        return ValueZoneHeight(sw);
    return 0;
}

/* The usable track length in pixels (thumb center can travel this range) */
static int
TrackLength(SliderWidget sw)
{
    Dimension tw = THUMB_SIZE;
    if (sw->slider.orientation == IswOrientHorizontal)
        return (int)sw->core.width - TrackZoneOffsetX(sw) - (int)tw - 1;
    else
        return (int)sw->core.height - TrackZoneOffsetY(sw) - (int)tw - 1;
}

/* Convert a value to a pixel position (thumb center along the track axis) */
static Position
ValueToPixel(SliderWidget sw, int value)
{
    int range = sw->slider.maximum - sw->slider.minimum;
    int track = TrackLength(sw);
    Dimension half_thumb = THUMB_SIZE / 2;
    int offset = (sw->slider.orientation == IswOrientHorizontal)
                 ? TrackZoneOffsetX(sw) : TrackZoneOffsetY(sw);

    if (range <= 0 || track <= 0)
        return (Position)(offset + (int)half_thumb);

    int clamped = value;
    if (clamped < sw->slider.minimum) clamped = sw->slider.minimum;
    if (clamped > sw->slider.maximum) clamped = sw->slider.maximum;

    int frac = (int)((long)(clamped - sw->slider.minimum) * track / range);

    /* Vertical: minimum at bottom, maximum at top */
    if (sw->slider.orientation == IswOrientVertical)
        frac = track - frac;

    return (Position)(offset + (int)half_thumb + frac);
}

/* Convert a pixel position to a value */
static int
PixelToValue(SliderWidget sw, Position pixel)
{
    int range = sw->slider.maximum - sw->slider.minimum;
    int track = TrackLength(sw);
    Dimension half_thumb = THUMB_SIZE / 2;
    int offset = (sw->slider.orientation == IswOrientHorizontal)
                 ? TrackZoneOffsetX(sw) : TrackZoneOffsetY(sw);

    if (range <= 0 || track <= 0)
        return sw->slider.minimum;

    int pos = (int)pixel - offset - (int)half_thumb;
    if (pos < 0) pos = 0;
    if (pos > track) pos = track;

    /* Vertical: invert so bottom = minimum, top = maximum */
    if (sw->slider.orientation == IswOrientVertical)
        pos = track - pos;

    return sw->slider.minimum + (int)((long)pos * range / track);
}

static void
UpdateThumbPos(SliderWidget sw)
{
    sw->slider.thumb_pos = ValueToPixel(sw, sw->slider.value);
}

/* --- Extract position from events --- */

static void
ExtractPosition(IswEvent *iswev, Position *x, Position *y)
{
    switch (iswev->kind) {
        case IswMotion:
        case IswButtonDown:
        case IswButtonUp:
            *x = IswEventX(iswev); *y = IswEventY(iswev);
            break;
        default:
            *x = 0; *y = 0;
    }
}

/* --- Widget methods --- */

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter(IswRString, IswROrientation, ISWCvtStringToOrientation,
                       (IswConvertArgList)NULL, 0, IswCacheNone, (IswDestructor)NULL);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    SliderWidget sw = (SliderWidget) new;
    (void)request; (void)args; (void)num_args;

    new->core.windowless = True;

    ((SimpleWidget) new)->simple.traversal_on = True;

    /* Compute minimum cross-axis size from content:
     * thumb + value label zone + tick marks */
 
    Dimension tick_zone = 0;
    if (sw->slider.tick_interval > 0)
        tick_zone = (TICK_LENGTH) + 2;
    Dimension value_zone = 0;
    if (sw->slider.show_value && sw->slider.font) {
        value_zone = ISWScaledFontHeight(new, sw->slider.font)
                   + (VALUE_MARGIN);
    }
    Dimension min_cross = THUMB_SIZE + tick_zone + value_zone + 10;

    /* Default geometry */
    if (sw->core.width == 0)
        sw->core.width = (sw->slider.orientation == IswOrientHorizontal)
            ? sw->slider.length : min_cross;
    if (sw->core.height == 0)
        sw->core.height = (sw->slider.orientation == IswOrientHorizontal)
            ? min_cross : sw->slider.length;
    /* Enforce minimum so nothing gets clipped */
    if (sw->slider.orientation == IswOrientHorizontal) {
        if (sw->core.height < min_cross)
            sw->core.height = min_cross;
    } else {
        if (sw->core.width < min_cross)
            sw->core.width = min_cross;
    }

    /* Clamp value */
    if (sw->slider.value < sw->slider.minimum)
        sw->slider.value = sw->slider.minimum;
    if (sw->slider.value > sw->slider.maximum)
        sw->slider.value = sw->slider.maximum;

    sw->slider.dragging = False;
    sw->slider.drag_offset = 0;
    sw->slider.render_ctx = NULL;

    UpdateThumbPos(sw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
    SliderWidget sw = (SliderWidget) w;

    /* Chain up to Simple's realize */
    (*sliderWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    sw->slider.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

static void
Destroy(Widget w)
{
    SliderWidget sw = (SliderWidget) w;
    if (sw->slider.render_ctx)
        ISWRenderDestroy(sw->slider.render_ctx);
}

static void
Resize(Widget w)
{
    SliderWidget sw = (SliderWidget) w;
    UpdateThumbPos(sw);
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

static int
ValueZoneHeight(SliderWidget sw)
{
    if (!sw->slider.show_value || !sw->slider.font)
        return 0;
    if (sw->slider.value_pos != IswSliderValueTop &&
        sw->slider.value_pos != IswSliderValueBottom)
        return 0;
    return ISWScaledFontHeight((Widget)sw, sw->slider.font)
         + (int)(VALUE_MARGIN);
}

static int
ValueZoneWidth(SliderWidget sw)
{
    if (!sw->slider.show_value || !sw->slider.font)
        return 0;
    if (sw->slider.value_pos != IswSliderValueLeft &&
        sw->slider.value_pos != IswSliderValueRight)
        return 0;
    /* Reserve space for widest possible value string */
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", sw->slider.maximum);
    int w_max = ISWRenderTextWidth(sw->slider.render_ctx, buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "%d", sw->slider.minimum);
    int w_min = ISWRenderTextWidth(sw->slider.render_ctx, buf, (int)strlen(buf));
    int wid = (w_max > w_min) ? w_max : w_min;
    return wid + (int)(VALUE_MARGIN);
}

/*
 * DrawValueLabel - draw the value text outside the slider area.
 * area_x/y/w/h is the bounding box of the full slider zone (thumb extent),
 * not just the thin track line.
 */
static void
DrawValueLabel(Widget w, SliderWidget sw, ISWRenderContext *ctx,
               int area_x, int area_y, int area_w, int area_h)
{
    if (!sw->slider.show_value || !sw->slider.font)
        return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", sw->slider.value);
    int text_w = ISWRenderTextWidth(ctx, buf, (int)strlen(buf));
    int font_asc = ISWScaledFontAscent(w, sw->slider.font);
    int margin = (int)(VALUE_MARGIN);
    int lx, ly;

    switch (sw->slider.value_pos) {
        case IswSliderValueTop:
            lx = area_x;
            ly = area_y - margin;  /* baseline sits above the slider area */
            if(sw->slider.orientation == IswOrientVertical) {
                lx = lx + (area_w / 2) - (text_w / 2);
                ly = ly - (THUMB_SIZE / 2);
            }
            break;
        case IswSliderValueBottom:
            lx = area_x;
            ly = area_y + area_h + margin + font_asc;
            if(sw->slider.orientation == IswOrientVertical) {
                lx = lx + (area_w / 2) - (text_w / 2);
                ly = ly + (THUMB_SIZE / 2);
            }
            break;
        case IswSliderValueLeft:
            lx = area_x - margin - text_w;
            ly = area_y + area_h / 2 + font_asc / 2;
            break;
        case IswSliderValueRight:
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
Redisplay(Widget w, IswEvent *event, xcb_xfixes_region_t region)
{
    SliderWidget sw = (SliderWidget) w;
    ISWRenderContext *ctx = sw->slider.render_ctx;
    (void)event; (void)region;

    if (!ctx || !IswIsRealized(w))
        return;

    Dimension thumb_w = THUMB_SIZE;
    Dimension thumb_h = THUMB_SIZE;
    Dimension track_thick = TRACK_THICKNESS;
    Dimension half_thumb = THUMB_SIZE / 2;
    int off_x = TrackZoneOffsetX(sw);
    int off_y = TrackZoneOffsetY(sw);

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, sw->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, sw->core.width, sw->core.height);

    ISWRenderSetColor(ctx, sw->slider.foreground);
    if (sw->slider.font)
        ISWRenderSetFont(ctx, sw->slider.font);

    if (sw->slider.orientation == IswOrientHorizontal) {
        /* --- Horizontal layout --- */
        int track_zone_h = (int)sw->core.height - off_y;
        int track_center_y = off_y + track_zone_h / 2;

        /* Track */
        int track_y = track_center_y - (int)track_thick / 2;
        int track_x = off_x + (int)half_thumb;
        int track_w = (int)sw->core.width - off_x - (int)thumb_w;
        ISWRenderFillStrokeRoundedRectangle(ctx, track_x, track_y, track_w, track_thick,
                                          track_thick / 2.0, 0.3, 1);
        

        /* Tick marks (below track) */
        if (sw->slider.tick_interval > 0) {
            Dimension tick_len = (TICK_LENGTH);
            int tick_y = track_y + (int)track_thick + 2;

            for (int v = sw->slider.minimum; v <= sw->slider.maximum;
                 v += sw->slider.tick_interval) {
                Position px = ValueToPixel(sw, v);
                ISWRenderDrawLine(ctx, px, tick_y, px, tick_y + (int)tick_len);
            }
            if ((sw->slider.maximum - sw->slider.minimum) % sw->slider.tick_interval != 0) {
                Position px = ValueToPixel(sw, sw->slider.maximum);
                ISWRenderDrawLine(ctx, px, tick_y, px, tick_y + (int)tick_len);
            }
        }

        Dimension ts = (THUMB_SIZE);
        Position tx = sw->slider.thumb_pos - (Position)(ts / 2);
        Position ty = track_center_y - (int)ts / 2;
        ISWRenderSetColor(ctx, sw->core.background_pixel);
        ISWRenderFillRoundedRectangle(ctx, tx, ty, ts, ts, 3.0);
        ISWRenderSetColor(ctx, sw->slider.foreground);
        ISWRenderStrokeRoundedRectangle(ctx, tx, ty, ts, ts, 3.0, 1.0);

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
        ISWRenderFillStrokeRoundedRectangle(ctx, track_x, track_top, track_thick, track_h,
                                          track_thick / 2.0, 0.3, 1);

        /* Tick marks (right of track) */
        if (sw->slider.tick_interval > 0) {
            Dimension tick_len = (TICK_LENGTH);
            int tick_x = track_x + (int)track_thick + 2;

            for (int v = sw->slider.minimum; v <= sw->slider.maximum;
                 v += sw->slider.tick_interval) {
                Position py = ValueToPixel(sw, v);
                ISWRenderDrawLine(ctx, tick_x, py, tick_x + (int)tick_len, py);
            }
            if ((sw->slider.maximum - sw->slider.minimum) % sw->slider.tick_interval != 0) {
                Position py = ValueToPixel(sw, sw->slider.maximum);
                ISWRenderDrawLine(ctx, tick_x, py, tick_x + (int)tick_len, py);
            }
        }

        Dimension ts = (THUMB_SIZE);
        Position tx = track_center_x - (int)ts / 2;
        Position ty = sw->slider.thumb_pos - (Position)(ts / 2);
        ISWRenderSetColor(ctx, sw->core.background_pixel);
        ISWRenderFillRoundedRectangle(ctx, tx, ty, ts, ts, 3.0);
        ISWRenderSetColor(ctx, sw->slider.foreground);
        ISWRenderStrokeRoundedRectangle(ctx, tx, ty, ts, ts, 3.0, 1.0);
        
        /* Value label — area is the full thumb extent */
        int area_x = track_center_x - (int)thumb_w / 2;
        DrawValueLabel(w, sw, ctx, area_x, track_top, (int)thumb_w, track_h);
    }

    _IswFocusMgrDrawRing(w, ctx, sw->slider.foreground, 2.0);

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired, ArgList args, Cardinal *num_args)
{
    SliderWidget csw = (SliderWidget) current;
    SliderWidget dsw = (SliderWidget) desired;
    Boolean redraw = FALSE;
    (void)request; (void)args; (void)num_args;

    /* Clamp value */
    if (dsw->slider.value < dsw->slider.minimum)
        dsw->slider.value = dsw->slider.minimum;
    if (dsw->slider.value > dsw->slider.maximum)
        dsw->slider.value = dsw->slider.maximum;

    if (csw->slider.value != dsw->slider.value ||
        csw->slider.minimum != dsw->slider.minimum ||
        csw->slider.maximum != dsw->slider.maximum ||
        csw->slider.foreground != dsw->slider.foreground ||
        csw->core.background_pixel != dsw->core.background_pixel ||
        csw->slider.orientation != dsw->slider.orientation ||
        csw->slider.tick_interval != dsw->slider.tick_interval ||
        csw->slider.show_value != dsw->slider.show_value ||
        csw->slider.value_pos != dsw->slider.value_pos) {
        UpdateThumbPos(dsw);
        redraw = TRUE;
    }

    return redraw;
}

/* --- Action procedures --- */

static void
SetValueAndNotify(SliderWidget sw, int new_value)
{
    if (new_value < sw->slider.minimum) new_value = sw->slider.minimum;
    if (new_value > sw->slider.maximum) new_value = sw->slider.maximum;

    if (new_value != sw->slider.value) {
        sw->slider.value = new_value;
        UpdateThumbPos(sw);
        Redisplay((Widget)sw, NULL, 0);

        IswSliderCallbackData cb_data;
        cb_data.value = new_value;
        IswCallCallbacks((Widget)sw, IswNvalueChanged, (IswPointer)&cb_data);
    }
}

static void
StartDrag(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    SliderWidget sw = (SliderWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    ExtractPosition(iswev, &x, &y);

    Position pick = (sw->slider.orientation == IswOrientHorizontal) ? x : y;
    Dimension half_thumb = THUMB_SIZE / 2;

    /* Check if click is on the thumb */
    if (pick >= sw->slider.thumb_pos - (Position)half_thumb &&
        pick <= sw->slider.thumb_pos + (Position)half_thumb) {
        sw->slider.dragging = True;
        sw->slider.drag_offset = (int)pick - (int)sw->slider.thumb_pos;
    } else {
        /* Jump to clicked position */
        sw->slider.dragging = True;
        sw->slider.drag_offset = 0;
        SetValueAndNotify(sw, PixelToValue(sw, pick));
    }
}

static void
Drag(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    SliderWidget sw = (SliderWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    if (!sw->slider.dragging)
        return;

    ExtractPosition(iswev, &x, &y);
    Position pick = (sw->slider.orientation == IswOrientHorizontal) ? x : y;
    SetValueAndNotify(sw, PixelToValue(sw, pick - sw->slider.drag_offset));
}

static void
EndDrag(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)params; (void)num_params;
    sw->slider.dragging = False;
}

static void
JumpToPosition(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    SliderWidget sw = (SliderWidget) w;
    Position x, y;
    (void)params; (void)num_params;

    ExtractPosition(iswev, &x, &y);
    Position pick = (sw->slider.orientation == IswOrientHorizontal) ? x : y;
    sw->slider.dragging = False;
    SetValueAndNotify(sw, PixelToValue(sw, pick));
}

static int
PageStep(SliderWidget sw)
{
    int range = sw->slider.maximum - sw->slider.minimum;
    int step = range / 10;
    return step > 0 ? step : 1;
}

static void
Increment(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.value + 1);
}

static void
Decrement(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.value - 1);
}

static void
PageIncrement(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.value + PageStep(sw));
}

static void
PageDecrement(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.value - PageStep(sw));
}

static void
SetMin(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.minimum);
}

static void
SetMax(Widget w, IswEvent *iswev, String *p, Cardinal *np)
{
    SliderWidget sw = (SliderWidget) w;
    (void)iswev; (void)p; (void)np;
    SetValueAndNotify(sw, sw->slider.maximum);
}

/* --- Public API --- */

void
IswSliderSetValue(Widget w, int value)
{
    SliderWidget sw = (SliderWidget) w;
    IswArgBuilder ab = IswArgBuilderInit();

    IswArgSliderValue(&ab, value);
    IswSetValues(w, ab.args, ab.count);

    /* Fire callback if value actually changed */
    if (sw->slider.value == value) {
        IswSliderCallbackData cb_data;
        cb_data.value = value;
        IswCallCallbacks(w, IswNvalueChanged, (IswPointer)&cb_data);
    }
}

int
IswSliderGetValue(Widget w)
{
    SliderWidget sw = (SliderWidget) w;
    return sw->slider.value;
}
