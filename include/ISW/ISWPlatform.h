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
/* IswDisplay / IswScreen / IswWindow are declared in ISW/Intrinsic.h (included
   above) so the public accessors there can reference them without a cycle. */
typedef struct _IswDrawable *IswDrawable;  /* anything that can be drawn into */

/* Window id as a portable value.  A window handle (IswWindow) is one of these
   reinterpreted by the backend; this is the on-the-wire/identity form used in
   neutral structs and where a bare id is needed (e.g. event targets). */
typedef uint32_t IswWindowId;

/* Geometry request for window create/configure, in physical pixels. */
typedef struct {
    int32_t  x, y;
    uint32_t width, height;
    uint32_t border_width;
} IswWindowGeometry;

/* Window-creation attributes the lifecycle needs.  Neutral subset; colormap,
   visual and pixmaps are added when Phase 4 abstracts them. */
typedef struct {
    uint32_t background_pixel;
    uint32_t border_pixel;
    uint32_t event_mask;        /* neutral IswEvent mask, backend-translated */
    Boolean  override_redirect;
    Boolean  save_under;
} IswWindowAttributes;

/* Window stacking for configure. */
typedef enum {
    ISW_STACK_NONE = 0,
    ISW_STACK_ABOVE,
    ISW_STACK_BELOW
} IswStackMode;

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
 * Display ops (Phase 2)
 * =================================================================
 *
 * Open/close the server connection, enumerate screens, expose the event-loop
 * file descriptor, flush, and report connection health.  All platform-neutral:
 * the XCB backend maps IswDisplay/IswScreen to xcb_connection_t/xcb_screen_t.
 */
struct _IswPlatformDisplayOps {
    /* Open a connection to `display_name` (NULL = default).  Returns a backend
       display handle, or NULL on failure.  *default_screen receives the
       preferred screen index. */
    IswDisplay (*open)(const char *display_name, int *default_screen);
    /* Flush pending requests, then close and free the handle. */
    void       (*close)(IswDisplay dpy);
    /* True if the connection has gone into an error/closed state. */
    Boolean    (*has_error)(IswDisplay dpy);
    /* Flush buffered requests to the server. */
    void       (*flush)(IswDisplay dpy);
    /* Event-loop file descriptor for select()/poll(). */
    int        (*connection_fd)(IswDisplay dpy);
    /* Number of screens, and the i-th screen handle. */
    int        (*screen_count)(IswDisplay dpy);
    IswScreen  (*screen)(IswDisplay dpy, int index);
    /* Root window of a screen. */
    IswWindow  (*root_window)(IswScreen screen);
    /* Screen geometry in physical pixels. */
    uint32_t   (*screen_width)(IswScreen screen);
    uint32_t   (*screen_height)(IswScreen screen);
    /* Ring the server bell (percent -100..100). */
    void       (*bell)(IswDisplay dpy, int percent);
};

/*
 * =================================================================
 * Window ops (Phase 2)
 * =================================================================
 *
 * Window lifecycle: create, configure (move/resize/restack), map, unmap,
 * reparent, destroy, attribute changes, and the localized repaint primitive
 * (clear-area) the toolkit uses to provoke a redraw.
 */
struct _IswPlatformWindowOps {
    /* Allocate a new window id (deferred creation backends may no-op). */
    IswWindow (*alloc_id)(IswDisplay dpy);
    /* Create a child window of `parent` with geometry + attributes; `depth`
       and a backend-resolved visual are taken from the screen default for now
       (Phase 4 generalises visual/depth).  Returns the created window. */
    IswWindow (*create)(IswDisplay dpy, IswWindow parent,
                        const IswWindowGeometry *geom,
                        const IswWindowAttributes *attrs);
    void (*destroy)(IswDisplay dpy, IswWindow win);
    void (*map)(IswDisplay dpy, IswWindow win);
    void (*unmap)(IswDisplay dpy, IswWindow win);
    void (*reparent)(IswDisplay dpy, IswWindow win, IswWindow new_parent,
                     int32_t x, int32_t y);
    /* Configure: move/resize and optional restack.  `mask` selects which of
       geom's fields apply (bitwise ISW_CONFIG_*). */
    void (*configure)(IswDisplay dpy, IswWindow win,
                      const IswWindowGeometry *geom, unsigned int mask,
                      IswStackMode stack, IswWindow sibling);
    /* Change the mutable attribute subset (event mask, bg pixel, …). */
    void (*change_attributes)(IswDisplay dpy, IswWindow win,
                              const IswWindowAttributes *attrs,
                              unsigned int mask);
    /* Clear a rectangle (0,0,0,0 = whole window), optionally generating an
       Expose for the toolkit's redraw path. */
    void (*clear_area)(IswDisplay dpy, IswWindow win,
                       int16_t x, int16_t y, uint16_t w, uint16_t h,
                       Boolean generate_expose);
    /* Window id <-> handle, for neutral structs / event targets. */
    IswWindowId (*window_id)(IswWindow win);
    IswWindow   (*window_from_id)(IswWindowId id);
};

/* configure() mask bits. */
#define ISW_CONFIG_X            (1u << 0)
#define ISW_CONFIG_Y            (1u << 1)
#define ISW_CONFIG_WIDTH        (1u << 2)
#define ISW_CONFIG_HEIGHT       (1u << 3)
#define ISW_CONFIG_BORDER       (1u << 4)
#define ISW_CONFIG_STACK        (1u << 5)

/* change_attributes() mask bits. */
#define ISW_ATTR_BACK_PIXEL     (1u << 0)
#define ISW_ATTR_BORDER_PIXEL   (1u << 1)
#define ISW_ATTR_EVENT_MASK     (1u << 2)
#define ISW_ATTR_OVERRIDE       (1u << 3)
#define ISW_ATTR_SAVE_UNDER     (1u << 4)

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
