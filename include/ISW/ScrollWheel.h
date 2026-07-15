/*
 * ScrollWheel.h - Continuous scroll-axis dispatch for ISW widgets
 *
 * Installs a custom IswEventDispatchProc for the IswScroll event kind.
 * When a scroll event arrives — continuous trackpad motion (sub-pixel
 * delta_x/delta_y, smooth=1) or a discrete wheel notch (discrete_x/y = +/-1,
 * smooth=0) — the dispatcher walks up the widget tree from the pointer target
 * to find the nearest scrollable container (Viewport, Text, or a standalone
 * Scrollbar) and forwards it as a scrollProc callback carrying an
 * IswScrollData payload (see IswScroll.h).
 *
 * Discrete-to-pixel scaling: a discrete wheel step is scaled by the target
 * Scrollbar's scrollWheelIncrement resource (default
 * ISW_SCROLL_WHEEL_DEFAULT_INCREMENT) into the pixel delta passed to
 * scrollProc, so the per-scrollbar increment still governs wheel speed.
 * Smooth (continuous) scrolls pass delta_x/delta_y through unchanged — the
 * increment resource is irrelevant for smooth input.
 *
 * Scroll stickiness: once a scrollable container is targeted, subsequent
 * scroll events within ISW_SCROLL_STICKY_MS continue scrolling the same
 * container even if the pointer has moved over a different scrollable.
 *
 * Shift+vertical-wheel switches to the horizontal axis (legacy behaviour,
 * applied in the XCB backend).  Buttons 6/7 (horizontal wheel) scroll
 * horizontally.
 */

#ifndef _ISW_ScrollWheel_h
#define _ISW_ScrollWheel_h

/* Resource names for scroll wheel configuration */
#define IswNscrollWheelIncrement "scrollWheelIncrement"
#define IswCScrollWheelIncrement "ScrollWheelIncrement"

/* Default scroll increment in pixels per wheel notch */
#define ISW_SCROLL_WHEEL_DEFAULT_INCREMENT 50

_XFUNCPROTOBEGIN

/*
 * Install the scroll wheel event dispatcher. Call once per connection.
 * Safe to call multiple times; subsequent calls are no-ops.
 */
extern void ISWScrollWheelInit(IswDisplay conn);

_XFUNCPROTOEND

#endif /* _ISW_ScrollWheel_h */
