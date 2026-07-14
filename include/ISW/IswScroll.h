/*
 * IswScroll.h - Continuous scroll axis payload for ISW
 *
 * IswScrollData is the call_data payload delivered to scrollProc callbacks
 * (Scrollbar, Viewport, Text) and to the scroll dispatcher.  It carries a
 * sub-pixel continuous delta plus a signed discrete (click-wheel) step count,
 * so consumers can accumulate smooth trackpad motion without quantizing to
 * whole pixels while still honouring discrete-wheel line/page semantics.
 *
 * dx/dy        : continuous pixel delta (negative = up/left).
 * discrete_x/y : signed click-wheel step count (one notch = +/-1).
 * smooth       : 1 when the source is a continuous valuator (trackpad),
 *                0 for a discrete wheel — consumers may use this to choose
 *                pixel-accurate vs. line/page quantization.
 */

#ifndef _ISW_IswScroll_h
#define _ISW_IswScroll_h

#include <stdint.h>

typedef struct {
    float    dx, dy;
    int32_t  discrete_x, discrete_y;
    uint8_t  smooth;
} IswScrollData;

#endif /* _ISW_IswScroll_h */
