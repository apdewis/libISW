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

/* IswKeyCode / IswKeySym / IswNoSymbol are declared in ISW/Intrinsic.h
   (included above) so the public key APIs there can use them without a cycle. */

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

/* Grabs — passive/active pointer/keyboard/button/key grabs.  Filled in Phase 5. */
typedef struct _IswPlatformGrabOps      IswPlatformGrabOps;

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
 * Input ops (Phase 3)
 * =================================================================
 *
 * Keysym table, keyboard mapping, modifier set, keycode<->keysym translation,
 * keysym-by-name (for the translation parser), case folding, mapping refresh,
 * and pointer query.  The backend owns a per-display keysym/modifier cache.
 * All neutral: IswKeyCode / IswKeySym / IswModMask, no xcb key types.
 */
struct _IswPlatformInputOps {
    /* keycode -> keysym for column `col` (0 = unshifted, 1 = shifted, ...). */
    IswKeySym (*keycode_to_keysym)(IswDisplay dpy, IswKeyCode kc, int col);
    /* All keycodes that produce `ks`.  Caller frees *out via free(). */
    void (*keysym_to_keycodes)(IswDisplay dpy, IswKeySym ks,
                               IswKeyCode **out, int *count);
    /* Resolve a keysym name ("Left", "a", "Escape") to a key identity, or
       IswNoSymbol.  Used by the "Ctrl<Key>a" translation parser. */
    IswKeySym (*keysym_from_name)(const char *name);
    /* Keysym -> its name (static buffer), or NULL. */
    const char *(*keysym_to_name)(IswKeySym ks);
    /* Lower/upper case forms of a keysym (either out pointer may be NULL). */
    void (*convert_case)(IswKeySym ks, IswKeySym *lower, IswKeySym *upper);
    /* Full translate: keycode + neutral modifier state -> keysym (+ the
       modifiers that were consumed).  Mirrors X's keycode translation. */
    void (*translate_keycode)(IswDisplay dpy, IswKeyCode kc, IswModMask state,
                              IswModMask *mods_return, IswKeySym *keysym_return);
    /* Rebuild the keysym/modifier cache after a mapping change
       (XCB_MAPPING_NOTIFY). */
    void (*refresh_mapping)(IswDisplay dpy);
    /* Query the pointer relative to `win`.  Returns False if unavailable. */
    Boolean (*query_pointer)(IswDisplay dpy, IswWindow win,
                             int *root_x, int *root_y,
                             int *win_x, int *win_y,
                             IswModMask *mods, IswWindow *child);
};

/*
 * =================================================================
 * Color ops (Phase 4)
 * =================================================================
 *
 * Colormap-based pixel<->RGB allocation, named-color allocation/lookup, pixel
 * release, and visual matching.  Neutral: IswColormap / IswColor /
 * IswVisualInfo, no xcb color/visual types.  Pixel values are the server's
 * numeric pixel (unsigned long), X11-compatible, carried opaquely.
 */
struct _IswPlatformColorOps {
    /* Pixel -> RGB via the colormap.  Fills out->{pixel,red,green,blue,flags}.
       Returns False if the query fails. */
    Boolean (*query_color)(IswDisplay dpy, IswColormap cmap,
                           unsigned long pixel, IswColor *out);
    /* Allocate the closest available cell for an RGB triple (16-bit each).
       *pixel_out receives the allocated pixel.  Returns False on failure. */
    Boolean (*alloc_color)(IswDisplay dpy, IswColormap cmap,
                           unsigned short red, unsigned short green,
                           unsigned short blue, unsigned long *pixel_out);
    /* Allocate a cell for a named color.  Returns False on failure. */
    Boolean (*alloc_named_color)(IswDisplay dpy, IswColormap cmap,
                                 const char *name, unsigned long *pixel_out);
    /* Is `name` a color the server knows (independent of allocation success)? */
    Boolean (*lookup_color)(IswDisplay dpy, IswColormap cmap, const char *name);
    /* Release a previously allocated pixel. */
    void (*free_colors)(IswDisplay dpy, IswColormap cmap, unsigned long pixel);
    /* Find a visual of `depth` and `visual_class` on `screen`.  Fills `out`.
       Returns False if none matches. */
    Boolean (*match_visual_info)(IswDisplay dpy, IswScreen screen,
                                 int depth, int visual_class,
                                 IswVisualInfo *out);
};

/*
 * =================================================================
 * Font ops (Phase 4)
 * =================================================================
 *
 * Core server-font open/close by name (the legacy IswRFont representation).
 * The toolkit's real text path is fontconfig/FreeType metrics (no server
 * coupling) and lives in the converters; these ops cover only the core
 * xcb_font_t handle still carried by IswFontStruct.fid.
 */
struct _IswPlatformFontOps {
    /* Open a core server font by name; returns 0 if not found. */
    IswFontId (*load_font)(IswDisplay dpy, const char *name);
    /* Close a core server font id (no-op for 0). */
    void (*free_font)(IswDisplay dpy, IswFontId fid);
};

/*
 * =================================================================
 * Cursor ops (Phase 5)
 * =================================================================
 *
 * Symbolic / themed cursor creation, application to a window, and release.
 * Neutral: IswCursor handle; the `shape` is the standard X cursor-font glyph
 * index (numerically compatible), so widgets keep using the XC_* values.
 */
struct _IswPlatformCursorOps {
    /* Glyph cursor from the standard cursor font for `shape`. */
    IswCursor (*create_glyph)(IswDisplay dpy, unsigned int shape);
    /* Theme-aware named cursor ("hand2", "watch", …); falls back to the glyph
       cursor for `fallback_shape` if the theme lookup fails. */
    IswCursor (*load_named)(IswDisplay dpy, IswScreen screen,
                            const char *name, unsigned int fallback_shape);
    /* Set the pointer cursor on a window (0 = revert to parent's). */
    void (*set_window_cursor)(IswDisplay dpy, IswWindow win, IswCursor cursor);
    /* Release a cursor (no-op for 0). */
    void (*free_cursor)(IswDisplay dpy, IswCursor cursor);
};

/*
 * =================================================================
 * Grab ops (Phase 5)
 * =================================================================
 *
 * Passive and active pointer / keyboard / button / key grabs.  Backends without
 * a grab concept may stub these (return failure / no-op).  pointer_mode /
 * keyboard_mode are the numeric async/sync constants; event_mask / modifiers are
 * the neutral mask values.
 */
struct _IswPlatformGrabOps {
    int  (*grab_pointer)(IswDisplay dpy, IswWindow grab_window,
                         Boolean owner_events, unsigned int event_mask,
                         int pointer_mode, int keyboard_mode,
                         IswWindow confine_to, IswCursor cursor, IswTime time);
    void (*ungrab_pointer)(IswDisplay dpy, IswTime time);
    int  (*grab_keyboard)(IswDisplay dpy, IswWindow grab_window,
                          Boolean owner_events, int pointer_mode,
                          int keyboard_mode, IswTime time);
    void (*ungrab_keyboard)(IswDisplay dpy, IswTime time);
    void (*grab_button)(IswDisplay dpy, IswWindow grab_window, int button,
                        unsigned int modifiers, Boolean owner_events,
                        unsigned int event_mask, int pointer_mode,
                        int keyboard_mode, IswWindow confine_to,
                        IswCursor cursor);
    void (*ungrab_button)(IswDisplay dpy, IswWindow grab_window, int button,
                          unsigned int modifiers);
    void (*grab_key)(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                     unsigned int modifiers, Boolean owner_events,
                     int pointer_mode, int keyboard_mode);
    void (*ungrab_key)(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                       unsigned int modifiers);
    void (*allow_events)(IswDisplay dpy, int mode, IswTime time);
};

/*
 * =================================================================
 * Selection ops (Phase 5)
 * =================================================================
 *
 * The three pure-selection protocol verbs: take/release ownership, query the
 * owner, and request a conversion.  The selection/target/property identifiers
 * are atoms, which Phase 6 abstracts; until then they stay xcb_atom_t.  The
 * property-exchange machinery the convert drives stays in Selection.c on the
 * seam until Phase 6.
 */
struct _IswPlatformSelectionOps {
    void      (*set_owner)(IswDisplay dpy, IswWindow owner,
                           xcb_atom_t selection, IswTime time);
    IswWindow (*get_owner)(IswDisplay dpy, xcb_atom_t selection);
    void      (*convert)(IswDisplay dpy, IswWindow requestor,
                         xcb_atom_t selection, xcb_atom_t target,
                         xcb_atom_t property, IswTime time);
};

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
    const IswPlatformGrabOps      *grab;
} IswPlatformOps;

#endif /* _ISWPlatform_h */
