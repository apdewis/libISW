/*
 * ISWPlatform.h - Platform abstraction vtable for ISW
 *
 * Copyright (c) 2026 ISW Project
 *
 * ISWRender abstracts drawing; ISWPlatform abstracts everything else that is
 * currently hardwired to X11/XCB: the display connection, shell windows,
 * top-level events, input, grabs, atoms, selections, colormaps, fonts and
 * cursors.  A platform backend (XCB today; Arcan/SHMIF or any EGL-providing
 * platform later) implements the ISWPlatformOps vtable; widget code talks only
 * to the abstract handles and operations declared here.
 *
 * SCAFFOLDING ONLY (Phase 0).  This header declares the opaque handle types and
 * the vtable skeleton.  No backend implements it yet and no widget code routes
 * through it yet — see docs/ISWPLATFORM_PLAN.md for the phase plan.
 *
 * CRITICAL: This interface is platform-neutral by construction — NO XCB or
 * Xlib types appear in any signature below.  Concrete backends map the opaque
 * handles to their native types internally.
 */

#ifndef _ISWPlatform_h
#define _ISWPlatform_h

#include <ISW/Intrinsic.h>

/*
 * =================================================================
 * Opaque platform handles
 * =================================================================
 *
 * Each backend maps these to its native types internally (the XCB backend to
 * xcb_connection_t* / xcb_window_t / xcb_pixmap_t, etc.).  Widget code never
 * dereferences them.
 */
typedef struct _ISWDisplay  *ISWDisplay;   /* display / server connection */
typedef struct _ISWWindow   *ISWWindow;    /* a top-level / shell window  */
typedef struct _ISWDrawable *ISWDrawable;  /* anything that can be drawn into */

/*
 * Portable integer point.  Replaces xcb_point_t in platform-neutral
 * signatures (e.g. polygon vertex lists).
 */
typedef struct {
    int16_t x, y;
} ISWPoint;

/*
 * Symbolic cursor shapes.  Backends map each to their native cursor; X11 maps
 * to glyph cursors from the standard cursor font.
 */
typedef enum {
    ISW_CURSOR_ARROW = 0,
    ISW_CURSOR_HAND,
    ISW_CURSOR_CROSSHAIR,
    ISW_CURSOR_TEXT,
    ISW_CURSOR_WATCH,
    ISW_CURSOR_SIZE_H,
    ISW_CURSOR_SIZE_V
} ISWCursorShape;

/*
 * =================================================================
 * Sub-vtables
 * =================================================================
 *
 * Phase 0 declares the grouping and the handle vocabulary only.  Operation
 * signatures are filled in per-phase as each category is abstracted; see the
 * phase plan.  Forward-declared here so ISWPlatformOps can reference them.
 */

/* Display / connection — open, close, screen info, event-loop fd.
 * Filled in Phase 2. */
typedef struct _ISWPlatformDisplayOps   ISWPlatformDisplayOps;

/* Window lifecycle — create, configure, map, destroy, reparent.
 * Filled in Phase 2. */
typedef struct _ISWPlatformWindowOps    ISWPlatformWindowOps;

/* Event loop + dispatch — poll, translate to a portable event union,
 * modifier state.  Filled in Phase 1 (unblocks the rest). */
typedef struct _ISWPlatformEventOps     ISWPlatformEventOps;

/* Input — keysym table, keyboard mapping, modifier set, grabs.
 * Filled in Phase 3. */
typedef struct _ISWPlatformInputOps     ISWPlatformInputOps;

/* Selections / clipboard — own, convert, paste.  Filled in Phase 5. */
typedef struct _ISWPlatformSelectionOps ISWPlatformSelectionOps;

/* Colormap / visual — alloc by name / RGB, free.  Filled in Phase 4. */
typedef struct _ISWPlatformColorOps     ISWPlatformColorOps;

/* Fonts — open by pattern, metrics, close.  Filled in Phase 4. */
typedef struct _ISWPlatformFontOps      ISWPlatformFontOps;

/* Cursors — create from symbol, set on window, free.  Filled in Phase 5. */
typedef struct _ISWPlatformCursorOps    ISWPlatformCursorOps;

/*
 * =================================================================
 * Platform operations vtable
 * =================================================================
 *
 * The top-level vtable a platform backend exports.  Mirrors the structure of
 * ISWRenderOps in ISWRenderPrivate.h: one const instance per backend, selected
 * at init.  Sub-vtables are referenced by pointer so each category can be
 * populated independently as its phase lands.
 */
typedef struct _ISWPlatformOps {
    const ISWPlatformDisplayOps   *display;
    const ISWPlatformWindowOps    *window;
    const ISWPlatformEventOps     *event;
    const ISWPlatformInputOps     *input;
    const ISWPlatformSelectionOps *selection;
    const ISWPlatformColorOps     *color;
    const ISWPlatformFontOps      *font;
    const ISWPlatformCursorOps    *cursor;
} ISWPlatformOps;

#endif /* _ISWPlatform_h */
