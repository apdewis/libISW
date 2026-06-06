/*
 * ISWPlatform.h - Platform abstraction vtable for ISW
 *
 * Copyright (c) 2026 ISW Project
 *
 * ISWRender abstracts drawing; ISWPlatform abstracts everything else that is
 * currently hardwired to X11/XCB: the display connection, shell windows,
 * top-level events, input, grabs, atoms, selections, colormaps, fonts and
 * cursors.  A platform backend (XCB today; Arcan/SHMIF or any EGL-providing
 * platform later) implements the IswPlatformOps vtable; widget code talks only
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
typedef struct _IswDisplay  *IswDisplay;   /* display / server connection */
typedef struct _IswWindow   *IswWindow;    /* a top-level / shell window  */
typedef struct _IswDrawable *IswDrawable;  /* anything that can be drawn into */

/*
 * Portable integer point.  Replaces xcb_point_t in platform-neutral
 * signatures (e.g. polygon vertex lists).
 */
typedef struct {
    int16_t x, y;
} IswPoint;

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
} IswCursorShape;

/*
 * =================================================================
 * Sub-vtables
 * =================================================================
 *
 * Phase 0 declares the grouping and the handle vocabulary only.  Operation
 * signatures are filled in per-phase as each category is abstracted; see the
 * phase plan.  Forward-declared here so IswPlatformOps can reference them.
 */

/* Display / connection — open, close, screen info, event-loop fd.
 * Filled in Phase 2. */
typedef struct _IswPlatformDisplayOps   IswPlatformDisplayOps;

/* Window lifecycle — create, configure, map, destroy, reparent.
 * Filled in Phase 2. */
typedef struct _IswPlatformWindowOps    IswPlatformWindowOps;

/* Event loop + dispatch — poll, translate to a portable event union,
 * modifier state.  Filled in Phase 1 (unblocks the rest). */
typedef struct _IswPlatformEventOps     IswPlatformEventOps;

/* Input — keysym table, keyboard mapping, modifier set, grabs.
 * Filled in Phase 3. */
typedef struct _IswPlatformInputOps     IswPlatformInputOps;

/* Selections / clipboard — own, convert, paste.  Filled in Phase 5. */
typedef struct _IswPlatformSelectionOps IswPlatformSelectionOps;

/* Colormap / visual — alloc by name / RGB, free.  Filled in Phase 4. */
typedef struct _IswPlatformColorOps     IswPlatformColorOps;

/* Fonts — open by pattern, metrics, close.  Filled in Phase 4. */
typedef struct _IswPlatformFontOps      IswPlatformFontOps;

/* Cursors — create from symbol, set on window, free.  Filled in Phase 5. */
typedef struct _IswPlatformCursorOps    IswPlatformCursorOps;

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
typedef struct _IswPlatformOps {
    const IswPlatformDisplayOps   *display;
    const IswPlatformWindowOps    *window;
    const IswPlatformEventOps     *event;
    const IswPlatformInputOps     *input;
    const IswPlatformSelectionOps *selection;
    const IswPlatformColorOps     *color;
    const IswPlatformFontOps      *font;
    const IswPlatformCursorOps    *cursor;
} IswPlatformOps;

#endif /* _ISWPlatform_h */
