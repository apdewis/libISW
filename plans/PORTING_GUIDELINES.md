# Isw3d XCB Porting Guidelines

**Purpose:** Systematic replacement of Xlib types with XCB equivalents  
**Strategy:** Complete type replacement, not compatibility layer  
**Last Updated:** 2026-03-02 (Revised to reflect original intent)

---

## Core Principle: Complete Type Replacement

**The Goal:** Replace ALL Xlib types and function calls with XCB equivalents throughout the Isw3d codebase.

**NOT a compatibility layer:** We are not wrapping XCB in Xlib-style functions. We are directly replacing Xlib with XCB.

**What this means:**
- Every `Display*` becomes `xcb_connection_t*`
- Every `XEvent*` becomes `xcb_generic_event_t*`
- Every `XDrawLine()` becomes `xcb_poly_line()`
- Every `XFillRectangle()` becomes `xcb_poly_fill_rectangle()`
- No wrappers, no typedefs, direct XCB usage

---

## Quick Reference Decision Tree

```
Encountered Xlib Type or Function
│
├─ Is it a basic X11 type? (Display*, Window, GC, etc.)
│  └─ Replace with XCB equivalent (see Type Replacement Table)
│
├─ Is it a drawing primitive? (XRectangle, XPoint, etc.)
│  └─ Replace with xcb_rectangle_t, xcb_point_t, etc.
│
├─ Is it an Xlib function? (XDrawLine, XFillRectangle, etc.)
│  └─ Replace with direct XCB call (see Function Replacement Table)
│
├─ Is it a deprecated API? (XFontSet, XIM, XIC)
│  ├─ XFontSet → Replace with Xft (see p15_xft_migration_plan.md)
│  └─ XIM/XIC → Stub out (see IMPLEMENTATION_PLAN.md Phase 2)
│
└─ Is it an event type? (XEvent, XKeyEvent, XButtonEvent, etc.)
   └─ Replace with xcb_generic_event_t* and specific event types
```

---

## Type Replacement Table

### Basic X11 Types

| Xlib Type | XCB Replacement | Notes |
|-----------|----------------|-------|
| `Display*` | `xcb_connection_t*` | **Replace everywhere** |
| `Window` | `xcb_window_t` | Scalar type (uint32_t) |
| `Drawable` | `xcb_drawable_t` | Scalar type (uint32_t) |
| `Pixmap` | `xcb_pixmap_t` | Scalar type (uint32_t) |
| `GC` | `xcb_gcontext_t` | Scalar type (uint32_t) |
| `Atom` | `xcb_atom_t` | Scalar type (uint32_t) |
| `Colormap` | `xcb_colormap_t` | Scalar type (uint32_t) |
| `Cursor` | `xcb_cursor_t` | Scalar type (uint32_t) |
| `VisualID` | `xcb_visualid_t` | Scalar type (uint32_t) |
| `Time` | `xcb_timestamp_t` | Scalar type (uint32_t) |
| `KeySym` | `xcb_keysym_t` | Scalar type (uint32_t) |
| `KeyCode` | `xcb_keycode_t` | Scalar type (uint8_t) |
| `Button` | `xcb_button_t` | Scalar type (uint8_t) |

### Drawing Primitives

| Xlib Type | XCB Replacement | Structure |
|-----------|----------------|-----------|
| `XRectangle` | `xcb_rectangle_t` | `{int16_t x, y; uint16_t width, height}` |
| `XPoint` | `xcb_point_t` | `{int16_t x, y}` |
| `XSegment` | `xcb_segment_t` | `{int16_t x1, y1, x2, y2}` |
| `XArc` | `xcb_arc_t` | `{int16_t x, y; uint16_t width, height; int16_t angle1, angle2}` |

### Event Types

| Xlib Type | XCB Replacement | Notes |
|-----------|----------------|-------|
| `XEvent` | `xcb_generic_event_t*` | Base event type |
| `XKeyEvent` | `xcb_key_press_event_t*` | Cast from generic |
| `XButtonEvent` | `xcb_button_press_event_t*` | Cast from generic |
| `XMotionEvent` | `xcb_motion_notify_event_t*` | Cast from generic |
| `XExposeEvent` | `xcb_expose_event_t*` | Cast from generic |
| `XConfigureEvent` | `xcb_configure_notify_event_t*` | Cast from generic |

### Special Cases

| Xlib Type | XCB Replacement | Notes |
|-----------|----------------|-------|
| `Region` | `xcb_xfixes_region_t` | **Scalar XID**, not pointer! |
| `Screen*` | `xcb_screen_t*` | From setup iterator |
| `XFontSet` | `XftFont*` | **No XCB equivalent** - use Xft |
| `XIM` | `void*` (stub) | **No XCB equivalent** - stub out |
| `XIC` | `void*` (stub) | **No XCB equivalent** - stub out |

---

## Function Replacement Table

### Drawing Functions

| Xlib Function | XCB Replacement | Notes |
|--------------|----------------|-------|
| `XDrawLine(dpy, d, gc, x1, y1, x2, y2)` | `xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, d, gc, 2, points)` | Need points array |
| `XDrawLines(dpy, d, gc, points, n, mode)` | `xcb_poly_line(conn, mode, d, gc, n, points)` | Direct mapping |
| `XDrawPoint(dpy, d, gc, x, y)` | `xcb_poly_point(conn, XCB_COORD_MODE_ORIGIN, d, gc, 1, &point)` | Need point struct |
| `XDrawPoints(dpy, d, gc, points, n, mode)` | `xcb_poly_point(conn, mode, d, gc, n, points)` | Direct mapping |
| `XDrawRectangle(dpy, d, gc, x, y, w, h)` | `xcb_poly_rectangle(conn, d, gc, 1, &rect)` | Need rect struct |
| `XDrawRectangles(dpy, d, gc, rects, n)` | `xcb_poly_rectangle(conn, d, gc, n, rects)` | Direct mapping |
| `XFillRectangle(dpy, d, gc, x, y, w, h)` | `xcb_poly_fill_rectangle(conn, d, gc, 1, &rect)` | Need rect struct |
| `XFillRectangles(dpy, d, gc, rects, n)` | `xcb_poly_fill_rectangle(conn, d, gc, n, rects)` | Direct mapping |
| `XFillPolygon(dpy, d, gc, points, n, shape, mode)` | `xcb_fill_poly(conn, d, gc, shape, mode, n, points)` | Direct mapping |
| `XDrawArc(dpy, d, gc, x, y, w, h, a1, a2)` | `xcb_poly_arc(conn, d, gc, 1, &arc)` | Need arc struct |
| `XFillArc(dpy, d, gc, x, y, w, h, a1, a2)` | `xcb_poly_fill_arc(conn, d, gc, 1, &arc)` | Need arc struct |
| `XCopyArea(dpy, src, dst, gc, sx, sy, w, h, dx, dy)` | `xcb_copy_area(conn, src, dst, gc, sx, sy, dx, dy, w, h)` | Direct mapping |
| `XCopyPlane(dpy, src, dst, gc, sx, sy, w, h, dx, dy, plane)` | `xcb_copy_plane(conn, src, dst, gc, sx, sy, dx, dy, w, h, plane)` | Direct mapping |

### Window Functions

| Xlib Function | XCB Replacement | Notes |
|--------------|----------------|-------|
| `XClearWindow(dpy, w)` | `xcb_clear_area(conn, 0, w, 0, 0, 0, 0)` | Width/height=0 means entire window |
| `XClearArea(dpy, w, x, y, width, height, exposures)` | `xcb_clear_area(conn, exposures, w, x, y, width, height)` | Direct mapping |
| `XMoveWindow(dpy, w, x, y)` | `xcb_configure_window(conn, w, XCB_CONFIG_WINDOW_X\|Y, values)` | Need values array |
| `XResizeWindow(dpy, w, width, height)` | `xcb_configure_window(conn, w, XCB_CONFIG_WINDOW_WIDTH\|HEIGHT, values)` | Need values array |
| `XMoveResizeWindow(dpy, w, x, y, width, height)` | `xcb_configure_window(conn, w, mask, values)` | Need values array |
| `XRaiseWindow(dpy, w)` | `xcb_configure_window(conn, w, XCB_CONFIG_WINDOW_STACK_MODE, &above)` | above=XCB_STACK_MODE_ABOVE |
| `XLowerWindow(dpy, w)` | `xcb_configure_window(conn, w, XCB_CONFIG_WINDOW_STACK_MODE, &below)` | below=XCB_STACK_MODE_BELOW |
| `XMapWindow(dpy, w)` | `xcb_map_window(conn, w)` | Direct mapping |
| `XUnmapWindow(dpy, w)` | `xcb_unmap_window(conn, w)` | Direct mapping |
| `XMapRaised(dpy, w)` | `xcb_configure_window() + xcb_map_window()` | Two calls |

### GC Functions

| Xlib Function | XCB Replacement | Notes |
|--------------|----------------|-------|
| `XCreateGC(dpy, d, mask, values)` | `xcb_generate_id() + xcb_create_gc()` | Need to generate ID first |
| `XFreeGC(dpy, gc)` | `xcb_free_gc(conn, gc)` | Direct mapping |
| `XChangeGC(dpy, gc, mask, values)` | `xcb_change_gc(conn, gc, mask, values)` | Direct mapping |
| `XSetForeground(dpy, gc, pixel)` | `xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &pixel)` | Single value |
| `XSetBackground(dpy, gc, pixel)` | `xcb_change_gc(conn, gc, XCB_GC_BACKGROUND, &pixel)` | Single value |
| `XSetLineAttributes(dpy, gc, w, style, cap, join)` | `xcb_change_gc(conn, gc, mask, values)` | Multiple values |
| `XSetClipRectangles(dpy, gc, x, y, rects, n, ordering)` | `xcb_set_clip_rectangles(conn, ordering, gc, x, y, n, rects)` | Direct mapping |
| `XSetClipMask(dpy, gc, pixmap)` | `xcb_change_gc(conn, gc, XCB_GC_CLIP_MASK, &pixmap)` | Single value |

### Utility Functions

| Xlib Function | XCB Replacement | Notes |
|--------------|----------------|-------|
| `XFlush(dpy)` | `xcb_flush(conn)` | **Call after every drawing operation** |
| `XSync(dpy, discard)` | `xcb_flush(conn)` | XCB doesn't support discard |
| `XBell(dpy, percent)` | `xcb_bell(conn, percent)` | Direct mapping |

---

## Replacement Patterns

### Pattern 1: Simple Drawing Function

**Before (Xlib):**
```c
void DrawBox(Widget w, int x, int y, int width, int height)
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = GetGC(w);
    
    XDrawRectangle(dpy, win, gc, x, y, width, height);
}
```

**After (XCB):**
```c
void DrawBox(Widget w, int x, int y, int width, int height)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc = GetGC(w);
    
    xcb_rectangle_t rect = {x, y, width, height};
    xcb_poly_rectangle(conn, win, gc, 1, &rect);
    xcb_flush(conn);
}
```

**Key changes:**
- `Display*` → `xcb_connection_t*`
- `Window` → `xcb_window_t`
- `GC` → `xcb_gcontext_t`
- `XDrawRectangle()` → `xcb_poly_rectangle()` with rect struct
- Added `xcb_flush()` at end

### Pattern 2: Event Handling

**Before (Xlib):**
```c
static void HandleButtonPress(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XButtonEvent *be = &event->xbutton;
    
    if (be->button == Button1) {
        ProcessClick(w, be->x, be->y, be->time);
    }
}
```

**After (XCB):**
```c
static void HandleButtonPress(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    xcb_button_press_event_t *be = (xcb_button_press_event_t*)event;
    
    if (be->detail == XCB_BUTTON_INDEX_1) {
        ProcessClick(w, be->event_x, be->event_y, be->time);
    }
}
```

**Key changes:**
- `XEvent*` → `xcb_generic_event_t*`
- `XButtonEvent` → `xcb_button_press_event_t*` (cast)
- `event->xbutton` → cast entire event
- `button` → `detail`
- `Button1` → `XCB_BUTTON_INDEX_1`
- `x, y` → `event_x, event_y`

### Pattern 3: Widget Redisplay

**Before (Xlib):**
```c
static void Redisplay(Widget w, XEvent *event, Region region)
{
    LabelWidget lw = (LabelWidget)w;
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = lw->label.normal_GC;
    
    XFillRectangle(dpy, win, gc, 0, 0, lw->core.width, lw->core.height);
    XDrawString(dpy, win, gc, x, y, lw->label.label, strlen(lw->label.label));
}
```

**After (XCB):**
```c
static void Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    LabelWidget lw = (LabelWidget)w;
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc = lw->label.normal_GC;
    
    xcb_rectangle_t rect = {0, 0, lw->core.width, lw->core.height};
    xcb_poly_fill_rectangle(conn, win, gc, 1, &rect);
    
    // Text rendering uses Xft (no XCB text drawing)
    XftDrawStringUtf8(xft_draw, &color, font, x, y, 
                      (FcChar8*)lw->label.label, strlen(lw->label.label));
    
    xcb_flush(conn);
}
```

**Key changes:**
- `XEvent*` → `xcb_generic_event_t*`
- `Region` → `xcb_xfixes_region_t` (scalar, not pointer!)
- `Display*` → `xcb_connection_t*`
- `Window` → `xcb_window_t`
- `GC` → `xcb_gcontext_t`
- `XFillRectangle()` → `xcb_poly_fill_rectangle()` with rect struct
- `XDrawString()` → `XftDrawStringUtf8()` (Xft, not XCB)
- Added `xcb_flush()` at end

### Pattern 4: Window Configuration

**Before (Xlib):**
```c
void ResizeWidget(Widget w, int width, int height)
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    
    XResizeWindow(dpy, win, width, height);
}
```

**After (XCB):**
```c
void ResizeWidget(Widget w, int width, int height)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    
    uint32_t values[2] = {width, height};
    uint16_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    
    xcb_configure_window(conn, win, mask, values);
    xcb_flush(conn);
}
```

**Key changes:**
- `Display*` → `xcb_connection_t*`
- `Window` → `xcb_window_t`
- `XResizeWindow()` → `xcb_configure_window()` with mask and values
- Values packed into uint32_t array
- Added `xcb_flush()` at end

### Pattern 5: GC Creation

**Before (Xlib):**
```c
GC CreateGC(Widget w, unsigned long foreground, unsigned long background)
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    XGCValues values;
    
    values.foreground = foreground;
    values.background = background;
    
    return XCreateGC(dpy, win, GCForeground | GCBackground, &values);
}
```

**After (XCB):**
```c
xcb_gcontext_t CreateGC(Widget w, unsigned long foreground, unsigned long background)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc;
    uint32_t values[2];
    uint32_t mask;
    
    gc = xcb_generate_id(conn);
    values[0] = foreground;
    values[1] = background;
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
    
    xcb_create_gc(conn, gc, win, mask, values);
    xcb_flush(conn);
    
    return gc;
}
```

**Key changes:**
- `Display*` → `xcb_connection_t*`
- `Window` → `xcb_window_t`
- `GC` → `xcb_gcontext_t`
- `XGCValues` → `uint32_t values[]` array
- `XCreateGC()` → `xcb_generate_id()` + `xcb_create_gc()`
- Values packed in mask bit order
- Added `xcb_flush()` at end

---

## Special Cases

### Font Rendering (No XCB Equivalent)

**XFontSet has no XCB equivalent** - use Xft instead

**See:** [`plans/p15_xft_migration_plan.md`](p15_xft_migration_plan.md) for complete migration guide

**Summary:**
- Replace `XFontSet` with `XftFont*`
- Replace `XmbDrawString()` with `XftDrawStringUtf8()`
- Replace `XwcDrawString()` with `XftDrawStringUtf8()`
- Replace `XmbTextEscapement()` with `XftTextExtentsUtf8()`
- Replace `XExtentsOfFontSet()` with Xft font metrics

### Input Methods (No XCB Equivalent)

**XIM/XIC have no XCB equivalent** - stub out

**See:** [`plans/IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) Phase 2 for XIM stubbing plan

**Summary:**
- Wrap XIM code in `#ifdef ISW_HAS_XIM` (undefined by default)
- Provide stub typedefs: `typedef void *XIM; typedef void *XIC;`
- Stub out IM functions to no-ops
- Document that CJK input requires external input framework

### Region Handling

**Region type changed from pointer to scalar**

**Xlib:** `Region` is `struct _XRegion*` (pointer)  
**XCB:** `xcb_xfixes_region_t` is `uint32_t` (scalar XID)

**Implications:**
- Pass by value, not by reference
- NULL check: `region == 0` instead of `region == NULL`
- May need XFixes extension for region operations

**Pragmatic approach for now:**
- Accept `xcb_xfixes_region_t` in widget expose methods
- Pass `0` (no clipping) to internal helpers
- Document with `/* FIXME: XCB region conversion */`

---

## Common Mistakes to Avoid

### ❌ Mistake 1: Using Xlib Type Names

```c
// WRONG!
void MyFunction(Widget w) {
    Display *dpy = XtDisplay(w);  // Should be xcb_connection_t*
    Window win = XtWindow(w);      // Should be xcb_window_t
}
```

**Why wrong:** Perpetuates Xlib types instead of replacing them

**Correct:**
```c
// RIGHT!
void MyFunction(Widget w) {
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
}
```

### ❌ Mistake 2: Creating Compatibility Wrappers

```c
// WRONG!
int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
    xcb_connection_t *conn = (xcb_connection_t*)dpy;
    xcb_point_t points[2] = {{x1, y1}, {x2, y2}};
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, d, gc, 2, points);
    xcb_flush(conn);
    return 1;
}
```

**Why wrong:** Hides XCB calls, perpetuates Xlib API

**Correct:**
```c
// RIGHT!
// Call xcb_poly_line() directly in widget code, no wrapper
void DrawLine(Widget w, int x1, int y1, int x2, int y2)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc = GetGC(w);
    
    xcb_point_t points[2] = {{x1, y1}, {x2, y2}};
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, win, gc, 2, points);
    xcb_flush(conn);
}
```

### ❌ Mistake 3: Commenting Out Code

```c
// WRONG!
/* FIXME: XCB - XIM types don't exist */
/* #ifdef ISW_INTERNATIONALIZATION
#include <X11/Isw3d/IswImP.h>
#endif */
```

**Why wrong:** Leaves functionality broken without solution

**Correct:**
```c
// RIGHT!
#ifdef ISW_INTERNATIONALIZATION
#include <X11/Isw3d/IswImP.h>  // XIM stubbed out when ISW_HAS_XIM undefined
#endif
```

### ❌ Mistake 4: Forgetting xcb_flush()

```c
// WRONG!
void DrawBox(Widget w, int x, int y, int width, int height)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc = GetGC(w);
    
    xcb_rectangle_t rect = {x, y, width, height};
    xcb_poly_rectangle(conn, win, gc, 1, &rect);
    // Missing xcb_flush()!
}
```

**Why wrong:** Drawing commands may not be sent to server

**Correct:**
```c
// RIGHT!
void DrawBox(Widget w, int x, int y, int width, int height)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    xcb_gcontext_t gc = GetGC(w);
    
    xcb_rectangle_t rect = {x, y, width, height};
    xcb_poly_rectangle(conn, win, gc, 1, &rect);
    xcb_flush(conn);  // Always flush after drawing!
}
```

---

## File-by-File Replacement Process

### Step 1: Choose a File

Start with simple widgets, progress to complex ones:
1. Simple widgets: Label, Command, Toggle
2. Container widgets: Box, Form, Paned
3. Complex widgets: Text, List, Menu

### Step 2: Replace Types

Search and replace (carefully!):
- `Display *` → `xcb_connection_t *`
- `Window ` → `xcb_window_t `
- `GC ` → `xcb_gcontext_t `
- `XEvent *` → `xcb_generic_event_t *`
- `Region ` → `xcb_xfixes_region_t `

**Note:** Be careful with variable names (e.g., don't replace "window" in comments)

### Step 3: Replace Function Calls

For each Xlib function call:
1. Find it in the Function Replacement Table above
2. Replace with XCB equivalent
3. Create necessary structs (xcb_rectangle_t, xcb_point_t, etc.)
4. Add xcb_flush() if it's a drawing/configuration operation

### Step 4: Fix Event Handling

For each event handler:
1. Change parameter type to `xcb_generic_event_t*`
2. Cast to specific event type (e.g., `xcb_button_press_event_t*`)
3. Update field names (e.g., `button` → `detail`, `x` → `event_x`)
4. Update event type constants (e.g., `ButtonPress` → `XCB_BUTTON_PRESS`)

### Step 5: Test Compilation

```bash
cd /home/adam/Isw3d
make 2>&1 | grep -E "error|warning" | head -50
```

Fix errors one by one, referring to this guide.

### Step 6: Move to Next File

Repeat for all widget files.

---

## Checklist for Each File

- [ ] All `Display*` replaced with `xcb_connection_t*`
- [ ] All `Window` replaced with `xcb_window_t`
- [ ] All `GC` replaced with `xcb_gcontext_t`
- [ ] All `XEvent*` replaced with `xcb_generic_event_t*`
- [ ] All `Region` replaced with `xcb_xfixes_region_t`
- [ ] All drawing primitives replaced (XRectangle, XPoint, etc.)
- [ ] All Xlib function calls replaced with XCB equivalents
- [ ] `xcb_flush()` added after drawing/configuration operations
- [ ] Event handlers updated for XCB event types
- [ ] File compiles without errors
- [ ] No Xlib types or functions remaining (except XFontSet → Xft, XIM → stub)

---

## Getting Help

### When to Ask

1. **Unclear XCB equivalent** - Function not in replacement table
2. **Complex transformation** - Multiple Xlib calls need coordination
3. **Performance concerns** - XCB pattern significantly different
4. **Breaking change** - API compatibility concerns

### How to Ask

**Good question:**
```
Context: Replacing XDrawLines() in Widget X
Issue: XDrawLines() takes XPoint array, xcb_poly_line() takes xcb_point_t array
Question: Are XPoint and xcb_point_t binary compatible, or do I need to convert?
Checked: Type definitions, both are {int16_t x, y}
```

**Poor question:**
```
How do I fix this error?
[paste error message]
```

---

## Resources

- **XCB API Documentation:** https://xcb.freedesktop.org/manual/
- **XCB Tutorial:** https://www.x.org/releases/X11R7.7/doc/libxcb/tutorial/
- **Type replacement strategy:** [`plans/p7_full_xcb_port_analysis.md`](p7_full_xcb_port_analysis.md)
- **Font migration:** [`plans/p15_xft_migration_plan.md`](p15_xft_migration_plan.md)
- **Strategy drift analysis:** [`plans/p17_strategy_drift_analysis.md`](p17_strategy_drift_analysis.md)

---

## Summary

**Remember:**
1. **Replace, don't wrap** - Direct XCB calls, no compatibility layer
2. **Complete replacement** - All Xlib types become XCB types
3. **Flush after drawing** - `xcb_flush()` is required
4. **Xft for fonts** - XFontSet has no XCB equivalent
5. **Stub XIM** - Input methods have no XCB equivalent

**The goal:** Pure XCB codebase with no Xlib dependencies (except Xft for fonts, which internally uses Xlib but provides a clean API).
