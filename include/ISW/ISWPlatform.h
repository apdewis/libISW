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
#include <ISW/IswDragDrop.h>   /* IswDragSourceDesc / IswDndAction for the DnD ops */

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

/* Window-creation attributes the lifecycle needs.  visual/colormap/depth carry
   the (opaque) rendering target selection a window create needs; bit_gravity_nw
   requests NorthWest bit gravity (shells set it).  A zero visual/colormap/depth
   means "inherit from parent / screen default". */
typedef struct {
    uint32_t background_pixel;
    uint32_t border_pixel;
    uint32_t event_mask;        /* neutral IswEvent mask, backend-translated */
    Boolean  override_redirect;
    Boolean  save_under;
    IswVisualId visual;         /* visual id; 0 = COPY_FROM_PARENT */
    IswColormap colormap;       /* opaque; 0 = none */
    uint32_t    depth;          /* 0 = COPY_FROM_PARENT */
    Boolean     bit_gravity_nw; /* set NorthWest bit gravity */
} IswWindowAttributes;

/* Window stacking for configure. */
typedef enum {
    ISW_STACK_NONE = 0,
    ISW_STACK_ABOVE,
    ISW_STACK_BELOW
} IswStackMode;

/* Window class passed to the window-create op (values match the X11 wire
   protocol so the XCB backend forwards them directly). */
#define ISW_WINDOW_CLASS_COPY_FROM_PARENT  0u
#define ISW_WINDOW_CLASS_INPUT_OUTPUT      1u
#define ISW_WINDOW_CLASS_INPUT_ONLY        2u

/* Property change mode (Phase 6).  Numerically the XCB_PROP_MODE_* values. */
typedef enum {
    ISW_PROP_MODE_REPLACE = 0,
    ISW_PROP_MODE_PREPEND = 1,
    ISW_PROP_MODE_APPEND  = 2
} IswPropMode;

/* Standard atom values (Phase 6).  Numerically X11-compatible; a neutral name
   for the common predefined atoms callers pass as a property TYPE. */
#define ISW_ATOM_NONE     ((Atom) 0)
#define ISW_ATOM_ATOM     ((Atom) 4)
#define ISW_ATOM_CARDINAL ((Atom) 6)
#define ISW_ATOM_STRING   ((Atom) 31)
#define ISW_ATOM_WINDOW   ((Atom) 33)

/* Result of a property fetch (Phase 6).  Backends fill this from their native
   query so callers never touch xcb reply structs.  `value` is malloc'd; release
   with _IswPlatformFreeProperty (or free(value)). */
typedef struct {
    Atom          type;          /* actual type atom of the stored property   */
    int           format;        /* 8 / 16 / 32                               */
    uint32_t      num_items;     /* element count (in `format`-bit units)     */
    uint32_t      bytes_after;   /* remaining unread bytes (for INCR/chunking) */
    void         *value;         /* malloc'd payload, num_items*format/8 bytes */
} IswProperty;

/* Window-type hint (Phase 6).  Maps to _NET_WM_WINDOW_TYPE_* on X. */
typedef enum {
    ISW_WINDOW_TYPE_NORMAL = 0,
    ISW_WINDOW_TYPE_DIALOG,
    ISW_WINDOW_TYPE_TOOLTIP,
    ISW_WINDOW_TYPE_MENU,
    ISW_WINDOW_TYPE_POPUP_MENU,
    ISW_WINDOW_TYPE_UTILITY
} IswWindowType;

/*
 * Neutral WM-hints vocabulary.  The flag bit values and the WM_HINTS / size-hint
 * struct layout are numerically X11-compatible (ICCCM), so an X backend forwards
 * them unchanged and the toolkit (Shell) names no xcb type.  A non-X backend maps
 * the meaningful fields to its own surface-role mechanism.
 */

/* WM_HINTS flag bits (ICCCM values). */
#define IswWmHintInput          (1u << 0)
#define IswWmHintState          (1u << 1)
#define IswWmHintIconPixmap     (1u << 2)
#define IswWmHintIconWindow     (1u << 3)
#define IswWmHintIconPosition   (1u << 4)
#define IswWmHintIconMask       (1u << 5)
#define IswWmHintWindowGroup    (1u << 6)
#define IswWmHintUrgency        (1u << 8)

/* WM_NORMAL_HINTS (size-hint) flag bits (ICCCM values). */
#define IswSizeHintUSPosition   (1u << 0)
#define IswSizeHintUSSize       (1u << 1)
#define IswSizeHintPPosition    (1u << 2)
#define IswSizeHintPSize        (1u << 3)
#define IswSizeHintPMinSize     (1u << 4)
#define IswSizeHintPMaxSize     (1u << 5)
#define IswSizeHintPResizeInc   (1u << 6)
#define IswSizeHintPAspect      (1u << 7)
#define IswSizeHintBaseSize     (1u << 8)
#define IswSizeHintPWinGravity  (1u << 9)

/* WM initial-state values (ICCCM WM_STATE). */
#define IswWmStateWithdrawn     0
#define IswWmStateNormalState   1
#define IswWmStateIconicState   3

/* Neutral WM_NORMAL_HINTS scratch record (flat ICCCM size-hints layout).  Used
   by the toolkit to assemble size hints before handing them to set_normal_hints. */
typedef struct _IswSizeHints {
    int32_t flags;            /* IswSizeHint* bits */
    int32_t x, y, width, height;
    int32_t min_width, min_height, max_width, max_height;
    int32_t width_inc, height_inc;
    int32_t min_aspect_num, min_aspect_den, max_aspect_num, max_aspect_den;
    int32_t base_width, base_height;
    int32_t win_gravity;
} IswSizeHints;

/* Neutral WM_HINTS record (mirrors ICCCM WM_HINTS; handle fields are neutral). */
typedef struct _IswWmHints {
    int32_t   flags;          /* IswWmHint* bits */
    uint32_t  input;          /* app relies on the WM for keyboard focus */
    int32_t   initial_state;  /* IswWmState* */
    IswPixmap icon_pixmap;
    IswWindow icon_window;
    int32_t   icon_x, icon_y;
    IswPixmap icon_mask;
    IswWindow window_group;
} IswWmHints;

/* IswKeyCode / IswKeySym / IswNoSymbol are declared in ISW/Intrinsic.h
   (included above) so the public key APIs there can use them without a cycle. */

/*
 * Portable integer point (IswPoint) is defined in ISW/IswTypes.h, shared by the
 * render and platform headers.
 */

/*
 * Portable integer rectangle.  Replaces xcb_rectangle_t in platform-neutral
 * geometry (region-overlap math, expose/clip areas, cursor bounds).  Field
 * layout matches xcb_rectangle_t so the value semantics are identical.
 */
typedef struct {
    int16_t  x, y;
    uint16_t width, height;
} IswRectangle;

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

/* Root surface — the OS/WM-managed top-level window that owns a shell's render
 * surface and presents it.  A windowless shell holds the root window handle but
 * never touches it directly; the toolkit maps/configures it through the window
 * ops and presents through these. */
typedef struct _IswPlatformRootOps      IswPlatformRootOps;

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

/* Atoms — intern by name, name by atom.  Filled in Phase 6. */
typedef struct _IswPlatformAtomOps      IswPlatformAtomOps;

/* Properties — change/get/delete window properties.  Filled in Phase 6. */
typedef struct _IswPlatformPropertyOps  IswPlatformPropertyOps;

/* Window-manager hints — title, class, protocols, transient, type, size hints.
 * A backend maps each to its own mechanism (X properties; Wayland xdg_*).
 * Filled in Phase 6. */
typedef struct _IswPlatformHintOps      IswPlatformHintOps;

/* Drag-and-drop — the whole DnD engine. A backend implements the protocol its
 * own way (X11: XDND v5 client messages + selection transfers; Wayland:
 * wl_data_device).  Filled in Phase 7. */
typedef struct _IswPlatformDndOps       IswPlatformDndOps;

/* Resource resolution — the configured-value lookup the toolkit asks for. A
 * backend supplies it from its own source (X11: Xrm over RESOURCE_MANAGER /
 * .Xdefaults; another: config file / app-supplied).  Filled in Phase 15. */
typedef struct _IswPlatformResourceOps  IswPlatformResourceOps;

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
    /* Flush and round-trip: block until the server has processed all prior
       requests (used as a barrier, e.g. after mapping a shell). */
    void       (*sync)(IswDisplay dpy);
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
    IswColormap (*screen_default_colormap)(IswScreen screen);
    int        (*screen_depth)(IswScreen screen);
    /* Default black/white pixel values of a screen. */
    unsigned long (*screen_black_pixel)(IswScreen screen);
    unsigned long (*screen_white_pixel)(IswScreen screen);
    /* Ring the server bell (percent -100..100). */
    void       (*bell)(IswDisplay dpy, int percent);
    /* Server vendor string (static storage), or "" if unavailable.  Used only
       for diagnostic messages. */
    const char *(*vendor)(IswDisplay dpy);
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
    /* Create a child window of `parent` with geometry + attributes.  `attrs`
       carries visual/colormap/depth (0 = inherit from parent/screen) and the
       window_class.  Returns the created window. */
    IswWindow (*create)(IswDisplay dpy, IswWindow parent,
                        const IswWindowGeometry *geom,
                        const IswWindowAttributes *attrs,
                        unsigned int window_class);
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
    /* True if the window is currently viewable (mapped and all ancestors
       mapped) — the toolkit skips repainting a not-yet-viewable window. */
    Boolean     (*window_viewable)(IswDisplay dpy, IswWindow win);
};

/*
 * =================================================================
 * Root surface ops
 * =================================================================
 *
 * The OS/WM-managed top-level window that backs a (now windowless) shell.
 * create_root makes the top-level window — a child of the screen root, with the
 * shell's visual/colormap/depth/event-mask — and returns its opaque handle.
 * The shell maps/configures/destroys it through the generic window ops; only
 * presentation is here, since it couples the render surface to the window:
 * present_root blits a finished composite IswSurface to the root window.  The
 * IswSurface argument is the opaque render surface (ISW/Intrinsic.h); the
 * backend reads its back buffer through the render layer's accessors.
 */
struct _IswPlatformRootOps {
    IswWindow (*create_root)(IswDisplay dpy, IswScreen screen,
                             const IswWindowGeometry *geom,
                             const IswWindowAttributes *attrs);
    void      (*present_root)(IswDisplay dpy, IswWindow win,
                              IswSurface surface, int width, int height);
};

/*
 * =================================================================
 * Event ops (Phase 11a)
 * =================================================================
 *
 * Non-blocking event fetch for the toolkit's poll-based loop (the toolkit
 * selects/polls on the connection fd, never blocks on a single event, so no
 * wait() op).  Events are returned as opaque `void *` (the backend's native
 * event) to keep this public header xcb-free; the toolkit-internal loop casts
 * them while the IswEvent translation bridge is retired (Phase 11b / 13).
 */
struct _IswPlatformEventOps {
    /* Next pending event, reading the socket if needed; NULL if none.
       Caller frees with free(). */
    void *(*poll)(IswDisplay dpy);
    /* Next event already in the client-side queue WITHOUT reading the socket;
       NULL if the queue is empty.  Caller frees with free(). */
    void *(*poll_queued)(IswDisplay dpy);
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
#define ISW_ATTR_COLORMAP       (1u << 5)
#define ISW_ATTR_BIT_GRAVITY    (1u << 6)

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

/* One modifier's slice of the late-binding keysym pool: which modifier bit
   (`mask`), how many keysyms map to it (`count`), and the offset (`idx`) into
   the keysym pool returned alongside.  Layout-compatible with the translation
   manager's internal ModToKeysymTable. */
typedef struct _IswModKeysymEntry {
    Modifiers mask;
    int       count;
    int       idx;
} IswModKeysymEntry;

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
    /* Build the modifier->keysym late-binding tables for `dpy`.  The backend
       reads the server's modifier mapping and fills `mods_return` (8 entries,
       caller-allocated) and a freshly-malloc'd keysym pool in `*keysyms_return`
       (caller frees via free()); `*count_return` receives the pool size.  Used
       by the translation manager's late-binding resolver. */
    void (*build_mod_map)(IswDisplay dpy, IswModKeysymEntry *mods_return,
                          IswKeySym **keysyms_return, int *count_return);
    /* Release the backend-owned keysym table held in the per-display record
       (called at display teardown). */
    void (*free_keysyms)(IswDisplay dpy);
    /* Query the pointer relative to `win`.  Returns False if unavailable. */
    Boolean (*query_pointer)(IswDisplay dpy, IswWindow win,
                             int *root_x, int *root_y,
                             int *win_x, int *win_y,
                             IswModMask *mods, IswWindow *child);
    /* Warp the pointer to (x, y) relative to the origin of `dst_win`. */
    void (*warp_pointer)(IswDisplay dpy, IswWindow dst_win, int x, int y);
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
    /* Last-resort font when the resource converters yield no font: open a
       platform default and return a populated IswFontStruct (caller frees),
       or NULL if even the fallback is unavailable. */
    IswFontStruct *(*load_fallback_font)(IswDisplay dpy);
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
    void (*change_active_pointer_grab)(IswDisplay dpy, IswCursor cursor,
                                       IswTime time, unsigned int event_mask);
};

/*
 * =================================================================
 * Selection ops
 * =================================================================
 *
 * The selection engine (Selection.c) and its consumers are platform-neutral by
 * construction: a selection, a conversion target, a conversion type and an
 * exchange property are all named by an opaque IswSelectionId, never an X11
 * atom.  The backend owns the name<->id mapping and the id<->native identifier
 * mapping; on X11 an IswSelectionId maps numerically to an interned atom, but a
 * non-X11 backend assigns its own stable ids.
 *
 * The ops cover the whole selection protocol seam the engine needs:
 *   - intern_name / name_of: the name<->id mapping;
 *   - set_owner / get_owner / convert: the ownership + conversion-request verbs;
 *   - decode_event: turn an opaque native event into a neutral IswSelectionEvent
 *     so the engine never inspects a backend event struct;
 *   - send_notify: emit the protocol's "conversion ready / refused" reply;
 *   - max_transfer_bytes: the largest single-property payload the transport
 *     accepts (drives INCR chunking).
 *
 * The neutral selection types (IswSelectionId, IswSelectionRequest,
 * IswSelectionEvent and IswSelectionEventKind) are declared in ISW/Intrinsic.h
 * — the public selection API and its callbacks use them too — so they are
 * available here via the include above.
 */
struct _IswPlatformSelectionOps {
    /* name <-> id mapping. intern_name returns ISW_SELECTION_NONE when
       only_if_exists and the name is unknown; name_of copies the id's name into
       buf (NUL-terminated, truncated) and returns False if the id is unknown. */
    IswSelectionId (*intern_name)(IswDisplay dpy, const char *name,
                                  Boolean only_if_exists);
    Boolean        (*name_of)(IswDisplay dpy, IswSelectionId id,
                              char *buf, size_t buflen);
    void      (*set_owner)(IswDisplay dpy, IswWindow owner,
                           IswSelectionId selection, IswTime time);
    IswWindow (*get_owner)(IswDisplay dpy, IswSelectionId selection);
    void      (*convert)(IswDisplay dpy, IswWindow requestor,
                         IswSelectionId selection, IswSelectionId target,
                         IswSelectionId property, IswTime time);
    /* Decode an opaque native event into *out.  Returns False (and sets
       out->kind = ISW_SEL_EVENT_OTHER) for events that are not selection
       protocol events. */
    Boolean   (*decode_event)(IswDisplay dpy, const void *native,
                              IswSelectionEvent *out);
    /* Send the "conversion ready" reply for `req`; property == ISW_SELECTION_NONE
       signals refusal. */
    void      (*send_notify)(IswDisplay dpy, const IswSelectionRequest *req,
                             IswSelectionId property);
    /* Largest single-property payload (in bytes) the transport accepts. */
    unsigned long (*max_transfer_bytes)(IswDisplay dpy);
};

/*
 * =================================================================
 * Atom ops (Phase 6)
 * =================================================================
 *
 * Intern a name to an atom and recover a name from an atom.  The backend owns
 * the per-display atom cache.  Atoms are neutral (Atom == uint32_t,
 * X11-compatible); a non-X backend assigns its own stable ids.
 */
struct _IswPlatformAtomOps {
    /* Intern `name`; if only_if_exists and unknown, returns ISW_ATOM_NONE. */
    Atom    (*intern)(IswDisplay dpy, const char *name, Boolean only_if_exists);
    /* Copy the atom's name into buf (NUL-terminated, truncated to buflen).
       Returns False if the atom is unknown. */
    Boolean (*get_name)(IswDisplay dpy, Atom atom, char *buf, size_t buflen);
};

/*
 * =================================================================
 * Property ops (Phase 6)
 * =================================================================
 *
 * Generic window-property change/get/delete.  The toolkit builds the bytes; the
 * backend transfers them.  get() returns a neutral IswProperty (no xcb reply
 * structs leak out).
 */
struct _IswPlatformPropertyOps {
    void (*change)(IswDisplay dpy, IswWindow win, Atom property, Atom type,
                   int format, IswPropMode mode,
                   const void *data, uint32_t num_elements);
    /* Fetch up to long_length 32-bit words starting at long_offset.  Fills
       `out` (out->value malloc'd, may be NULL/0-length).  Returns False on
       failure. */
    Boolean (*get)(IswDisplay dpy, IswWindow win, Atom property, Atom type,
                   uint32_t long_offset, uint32_t long_length,
                   IswProperty *out);
    void (*delete_)(IswDisplay dpy, IswWindow win, Atom property);
};

/*
 * =================================================================
 * Window-manager hint ops (Phase 6)
 * =================================================================
 *
 * Semantic ICCCM/EWMH hints.  A backend maps each to its own mechanism — X sets
 * the corresponding properties; a Wayland backend would drive xdg_toplevel etc.
 * The niche EWMH long tail stays on the generic property ops (see
 * docs/PHASE6_SCOPE.md), so only the cross-platform-meaningful hints are here.
 */
struct _IswPlatformHintOps {
    void (*set_window_title)(IswDisplay dpy, IswWindow win, const char *utf8);
    void (*set_icon_title)(IswDisplay dpy, IswWindow win, const char *utf8);
    void (*set_wm_class)(IswDisplay dpy, IswWindow win,
                         const char *name, const char *class_name);
    void (*set_wm_protocols)(IswDisplay dpy, IswWindow win,
                             const Atom *protocols, int num_protocols);
    void (*set_transient_for)(IswDisplay dpy, IswWindow win, IswWindow leader);
    void (*set_window_type)(IswDisplay dpy, IswWindow win, IswWindowType type);
    void (*set_pid)(IswDisplay dpy, IswWindow win, uint32_t pid);
    /* Normal (size) hints the toolkit actually sets.  flags mirrors the ICCCM
       size-hint flag bits the caller populates; a backend honours what it can. */
    void (*set_normal_hints)(IswDisplay dpy, IswWindow win,
                             uint32_t flags,
                             int x, int y, int width, int height,
                             int min_width, int min_height,
                             int max_width, int max_height,
                             int width_inc, int height_inc,
                             int min_aspect_num, int min_aspect_den,
                             int max_aspect_num, int max_aspect_den,
                             int base_width, int base_height,
                             int win_gravity);
    /* The full WM_HINTS record (focus model, initial state, icon, group,
       urgency).  The backend marshals it to its native representation. */
    void (*set_wm_hints)(IswDisplay dpy, IswWindow win, const IswWmHints *hints);
};

/*
 * =================================================================
 * Drag-and-drop ops (Phase 7)
 * =================================================================
 *
 * The whole drag-and-drop engine lives behind these verbs.  On X11 the backend
 * implements them via the XDND v5 protocol (client messages + a selection
 * transfer of XdndSelection) in ISWPlatformDndXCB.c; a Wayland backend would
 * drive wl_data_device/wl_data_source instead.  The verbs mirror the public
 * IswDragDrop service one-for-one — the service entry points are thin
 * dispatchers over these — so nothing XDND-specific reaches widget code.
 *
 * Types come from <ISW/IswDragDrop.h>, included for that purpose; it pulls in
 * only Intrinsic.h + IswEvent.h, so there is no include cycle.
 */
struct _IswPlatformDndOps {
    void    (*enable)(Widget shell);
    void    (*widget_accept_drops)(Widget w);
    void    (*start_drag)(Widget source, IswEvent *trigger,
                          IswDragSourceDesc *desc);
    void    (*set_accepted_types)(Widget w, Atom *types, int num_types);
    void    (*set_accepted_actions)(Widget w, IswDndAction actions);
    void    (*set_drop_callback)(Widget w, IswCallbackProc proc,
                                 IswPointer closure);
    void    (*set_drag_motion_callback)(Widget w, IswCallbackProc proc,
                                        IswPointer closure);
    void    (*set_drag_leave_callback)(Widget w, IswCallbackProc proc,
                                        IswPointer closure);
    Atom    (*intern_type)(Widget w, const char *mime_type);
    Boolean (*is_dragging)(Widget w);
};

/*
 * =================================================================
 * Resource-resolution ops (Phase 15)
 * =================================================================
 *
 * The resource subsystem abstracted at the *question* it answers — "what is the
 * configured value of this resource (by full name/class path) for this widget?"
 * — not at any one platform's mechanism.  On X11 the backend implements these
 * over libxcb-util-xrm (RESOURCE_MANAGER / .Xdefaults / XENVIRONMENT, matched by
 * Xrm's tight/loose precedence rules), in ISWPlatformResourceXCB.c; Xrm is a
 * private detail of that TU.  Another backend supplies resources from its own
 * source (config file / app-supplied / none) with no RESOURCE_MANAGER.
 *
 * IswDatabaseHandle (ISW/IswDatabase.h) is the neutral opaque database handle;
 * the toolkit never inspects it.  combine/put take IswDatabaseHandle* because a
 * backend may reallocate the store on mutation.  The portable quark interning
 * (Quark.c) stays in the toolkit and is not part of this seam.
 */
struct _IswPlatformResourceOps {
    /* Build a database from a source. */
    IswDatabaseHandle (*from_string)(const char *str);
    IswDatabaseHandle (*from_file)(const char *filename);
    IswDatabaseHandle (*from_resource_manager)(IswDisplay dpy, IswScreen screen);
    /* Merge source into *target (override decides precedence on collision). */
    void (*combine)(IswDatabaseHandle source, IswDatabaseHandle *target,
                    Boolean override);
    /* Add a single "name: value" resource / one resource line to *db. */
    void (*put_resource)(IswDatabaseHandle *db, const char *resource,
                         const char *value);
    void (*put_resource_line)(IswDatabaseHandle *db, const char *line);
    /* Serialise the whole database (malloc'd string the caller frees). */
    char *(*to_string)(IswDatabaseHandle db);
    void (*free)(IswDatabaseHandle db);
    /* Resolve a fully-qualified name/class path to a string value.  Returns >= 0
       and sets *out (malloc'd) on a hit, < 0 on miss — mirrors Xrm's contract. */
    int (*get_string)(IswDatabaseHandle db, const char *res_name,
                      const char *res_class, char **out);
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
/* Rendering — the render system is platform-variant (cairo-on-xcb today,
   cairo-egl / a Wayland renderer tomorrow), so the backend's render vtable is a
   member of the platform ops table like every other capability.  Its concrete
   layout (the drawing + surface sub-vtables and backend detection) is render-
   internal and lives in ISWRenderPrivate.h; the platform table holds it by
   opaque pointer so this header pulls in no cairo/xcb render types. */
typedef struct _IswPlatformRenderOps IswPlatformRenderOps;

typedef struct _IswPlatformOps {
    const IswPlatformDisplayOps   *display;
    const IswPlatformWindowOps    *window;
    const IswPlatformRootOps      *root;
    const IswPlatformEventOps     *event;
    const IswPlatformInputOps     *input;
    const IswPlatformSelectionOps *selection;
    const IswPlatformColorOps     *color;
    const IswPlatformFontOps      *font;
    const IswPlatformCursorOps    *cursor;
    const IswPlatformGrabOps      *grab;
    const IswPlatformAtomOps      *atom;
    const IswPlatformPropertyOps  *property;
    const IswPlatformHintOps      *hint;
    const IswPlatformDndOps       *dnd;
    const IswPlatformResourceOps  *resource;
    const IswPlatformRenderOps    *render;
} IswPlatformOps;

/* Release the malloc'd payload of an IswProperty (safe on a zeroed struct). */
extern void _IswPlatformFreeProperty(IswProperty *prop);

/*
 * =================================================================
 * Per-category dispatch wrappers
 * =================================================================
 *
 * Thin neutral wrappers over the ops vtable above.  Each recovers the injected
 * backend ops from the display/widget it is handed, hides the vtable lookup,
 * and null-guards a missing op (degrading to a no-op / failure rather than a
 * crash).  Toolkit and widget code calls these instead of walking the vtable
 * or reaching into any backend-private header.  Implemented in the backend
 * dispatch TU (X11: ISWPlatformDisplayXCB.c and the per-category backend TUs).
 */

/* Backend selection (called once at IswOpenDisplay, before any connection). */
extern const IswPlatformOps *_IswPlatformSelectBackend(void);

/* The active backend's render ops (IswPlatformOps.render).  Opaque to non-render
   code; the render dispatcher casts it to the concrete IswPlatformRenderOps in
   ISWRenderPrivate.h.  Returns NULL if the backend exports no render ops. */
extern const IswPlatformRenderOps *_IswPlatformRenderOpsActive(void);

/* Display / event loop */
extern int       _IswPlatformConnectionFd(IswDisplay dpy);
/* Open/close a server connection by display-name (NULL = default). */
extern IswDisplay _IswPlatformOpenDisplay(const char *display_name,
                                          int *default_screen);
extern void      _IswPlatformCloseDisplay(IswDisplay dpy);
/* Server vendor string (diagnostic use), or "". */
extern const char *_IswPlatformDisplayVendor(IswDisplay dpy);
extern IswScreen _IswDefaultScreenOf(IswDisplay dpy);
extern IswWindow _IswDefaultRootWindow(IswDisplay dpy);
extern uint32_t  _IswPlatformScreenWidth(IswDisplay dpy, IswScreen screen);
extern uint32_t  _IswPlatformScreenHeight(IswDisplay dpy, IswScreen screen);
extern IswColormap _IswPlatformScreenDefaultColormap(IswDisplay dpy, IswScreen screen);
extern int       _IswPlatformScreenDepth(IswDisplay dpy, IswScreen screen);
extern unsigned long _IswPlatformScreenBlackPixel(IswDisplay dpy, IswScreen screen);
extern unsigned long _IswPlatformScreenWhitePixel(IswDisplay dpy, IswScreen screen);
extern void     *_IswPlatformPollEvent(IswDisplay dpy);
extern void     *_IswPlatformPollQueuedEvent(IswDisplay dpy);
extern Boolean   _IswPlatformDisplayHasError(IswDisplay dpy);
extern void      _IswPlatformFlush(IswDisplay dpy);
extern void      _IswPlatformSync(IswDisplay dpy);

/* Selection */
extern IswSelectionId _IswPlatformSelectionInternName(IswDisplay dpy,
                                                      const char *name,
                                                      Boolean only_if_exists);
extern Boolean   _IswPlatformSelectionName(IswDisplay dpy, IswSelectionId id,
                                           char *buf, size_t buflen);
extern void      _IswPlatformSetSelectionOwner(IswDisplay dpy, IswWindow owner,
                                              IswSelectionId selection,
                                              IswTime time);
extern IswWindow _IswPlatformGetSelectionOwner(IswDisplay dpy,
                                              IswSelectionId selection);
extern void      _IswPlatformConvertSelection(IswDisplay dpy, IswWindow requestor,
                                             IswSelectionId selection,
                                             IswSelectionId target,
                                             IswSelectionId property,
                                             IswTime time);
extern Boolean   _IswPlatformSelectionDecodeEvent(IswDisplay dpy,
                                                 const void *native,
                                                 IswSelectionEvent *out);
extern void      _IswPlatformSelectionSendNotify(IswDisplay dpy,
                                                const IswSelectionRequest *req,
                                                IswSelectionId property);
extern unsigned long _IswPlatformSelectionMaxTransfer(IswDisplay dpy);
extern IswSelectionId _IswPlatformSelectionStdType(IswDisplay dpy,
                                                   IswSelectionStdType which);

/* Color */
extern Boolean   _IswPlatformQueryColor(IswDisplay dpy, IswColormap cmap,
                                        unsigned long pixel, IswColor *out);
extern Boolean   _IswPlatformAllocColor(IswDisplay dpy, IswColormap cmap,
                                        unsigned short red, unsigned short green,
                                        unsigned short blue, unsigned long *pixel_out);
extern Boolean   _IswPlatformAllocNamedColor(IswDisplay dpy, IswColormap cmap,
                                             const char *name, unsigned long *pixel_out);
extern Boolean   _IswPlatformLookupColor(IswDisplay dpy, IswColormap cmap,
                                         const char *name);
extern void      _IswPlatformFreeColors(IswDisplay dpy, IswColormap cmap,
                                        unsigned long pixel);
extern Boolean   _IswPlatformMatchVisualInfo(IswDisplay dpy, IswScreen screen,
                                             int depth, int visual_class,
                                             IswVisualInfo *out);

/* Font */
extern IswFontId _IswPlatformLoadFont(IswDisplay dpy, const char *name);
extern void      _IswPlatformFreeFont(IswDisplay dpy, IswFontId fid);
extern IswFontStruct *_IswPlatformLoadFallbackFont(IswDisplay dpy);

/* Cursor */
extern IswCursor _IswPlatformLoadNamedCursor(IswDisplay dpy, IswScreen screen,
                                             const char *name,
                                             unsigned int fallback_shape);
extern void      _IswPlatformSetWindowCursor(IswDisplay dpy, IswWindow win,
                                             IswCursor cursor);
extern void      _IswPlatformFreeCursor(IswDisplay dpy, IswCursor cursor);

/* Grabs */
extern int  _IswPlatformGrabPointer(IswDisplay dpy, IswWindow grab_window,
                                    Boolean owner_events, unsigned int event_mask,
                                    int pointer_mode, int keyboard_mode,
                                    IswWindow confine_to, IswCursor cursor, IswTime time);
extern void _IswPlatformUngrabPointer(IswDisplay dpy, IswTime time);
extern int  _IswPlatformGrabKeyboard(IswDisplay dpy, IswWindow grab_window,
                                     Boolean owner_events, int pointer_mode,
                                     int keyboard_mode, IswTime time);
extern void _IswPlatformUngrabKeyboard(IswDisplay dpy, IswTime time);
extern void _IswPlatformGrabButton(IswDisplay dpy, IswWindow grab_window, int button,
                                   unsigned int modifiers, Boolean owner_events,
                                   unsigned int event_mask, int pointer_mode,
                                   int keyboard_mode, IswWindow confine_to,
                                   IswCursor cursor);
extern void _IswPlatformGrabKey(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                                unsigned int modifiers, Boolean owner_events,
                                int pointer_mode, int keyboard_mode);
/* Release a passive key/button grab installed with the above. */
extern void _IswPlatformUngrabKey(IswDisplay dpy, IswWindow grab_window,
                                  IswKeyCode keycode, unsigned int modifiers);
extern void _IswPlatformUngrabButton(IswDisplay dpy, IswWindow grab_window,
                                     int button, unsigned int modifiers);
/* Change the cursor / event mask of the active pointer grab in place. */
extern void _IswPlatformChangeActivePointerGrab(IswDisplay dpy, IswCursor cursor,
                                                IswTime time,
                                                unsigned int event_mask);

/* Pointer query */
extern Boolean _IswPlatformQueryPointer(IswDisplay dpy, IswWindow win,
                                        int *root_x, int *root_y,
                                        int *win_x, int *win_y,
                                        IswModMask *mods, IswWindow *child);

/* Resolve a key name ("a", "Return", "Escape") to a neutral key identity:
   an IswKey enum value, or a Unicode code point for printable keys — the same
   vocabulary carried in IswKeyEvent.key.  Returns IswKeyNone (0) if unknown.
   Used by the translation-table parser so "<Key>Return" matches the neutral
   key identity in a dispatched IswEvent. */
extern uint32_t _IswPlatformKeyFromName(const char *name);

/* Rebuild the backend keymap/modifier cache after a keyboard mapping change. */
extern void _IswPlatformRefreshMapping(IswDisplay dpy);

/* keycode -> keysym for column `col` (0 = unshifted, 1 = shifted). */
extern IswKeySym _IswPlatformKeycodeToKeysym(IswDisplay dpy, IswKeyCode kc,
                                             int col);
/* All keycodes producing `ks` (caller frees *out via free()). */
extern void _IswPlatformKeysymToKeycodes(IswDisplay dpy, IswKeySym ks,
                                         IswKeyCode **out, int *count);
/* Lower/upper case forms of `ks` (either out pointer may be NULL). */
extern void _IswPlatformConvertCase(IswKeySym ks, IswKeySym *lower,
                                    IswKeySym *upper);
/* keycode + neutral modifier state -> keysym (+ consumed modifiers). */
extern void _IswPlatformTranslateKeycode(IswDisplay dpy, IswKeyCode kc,
                                         IswModMask state,
                                         IswModMask *mods_return,
                                         IswKeySym *keysym_return);
/* Build the modifier->keysym late-binding tables (see input op). */
extern void _IswPlatformBuildModMap(IswDisplay dpy,
                                    IswModKeysymEntry *mods_return,
                                    IswKeySym **keysyms_return,
                                    int *count_return);
/* Release the backend-owned keysym table (display teardown). */
extern void _IswPlatformFreeKeysyms(IswDisplay dpy);

/* Warp the pointer to (x, y) relative to the origin of dst_win. */
extern void _IswPlatformWarpPointer(IswDisplay dpy, IswWindow dst_win,
                                    int x, int y);

/* Atom */
extern Atom    _IswPlatformInternAtomOp(IswDisplay dpy, const char *name,
                                        Boolean only_if_exists);
extern Boolean _IswPlatformGetAtomName(IswDisplay dpy, Atom atom,
                                       char *buf, size_t buflen);

/* Property */
extern void    _IswPlatformChangeProperty(IswDisplay dpy, IswWindow win, Atom property,
                                          Atom type, int format, IswPropMode mode,
                                          const void *data, uint32_t num_elements);
extern Boolean _IswPlatformGetProperty(IswDisplay dpy, IswWindow win, Atom property,
                                       Atom type, uint32_t long_offset,
                                       uint32_t long_length, IswProperty *out);
extern void    _IswPlatformDeleteProperty(IswDisplay dpy, IswWindow win, Atom property);

/* Window lifecycle */
extern IswWindow _IswPlatformAllocWindowId(IswDisplay dpy);
extern IswWindow _IswPlatformCreateWindow(IswDisplay dpy, IswWindow parent,
                                          const IswWindowGeometry *geom,
                                          const IswWindowAttributes *attrs,
                                          unsigned int window_class);
extern void    _IswPlatformDestroyWindow(IswDisplay dpy, IswWindow win);
extern void    _IswPlatformMapWindow(IswDisplay dpy, IswWindow win);
extern void    _IswPlatformUnmapWindow(IswDisplay dpy, IswWindow win);
extern void    _IswPlatformReparentWindow(IswDisplay dpy, IswWindow win,
                                          IswWindow new_parent, int32_t x, int32_t y);
extern void    _IswPlatformConfigureWindow(IswDisplay dpy, IswWindow win,
                                           const IswWindowGeometry *geom,
                                           unsigned int mask, IswStackMode stack,
                                           IswWindow sibling);
extern void    _IswPlatformClearArea(IswDisplay dpy, IswWindow win,
                                     int16_t x, int16_t y, uint16_t w, uint16_t h,
                                     Boolean generate_expose);
/* The single top-level window the platform owns for a widget's display.  The
   toolkit holds no window handle; boundary code (event routing, grabs, DnD)
   that must name a window resolves it here from the widget. */
extern IswWindow _IswPlatformWidgetWindow(IswDisplay dpy, Widget w);

/* Register (win) or clear (win==0) the platform window backing a specific
   widget — used by widgets that own a distinct top-level (tooltip popups).
   The toolkit holds no window handle; the association lives in the platform. */
extern void _IswPlatformSetWidgetWindow(IswDisplay dpy, Widget w, IswWindow win);

/* Reverse of _IswPlatformWidgetWindow: the widget associated with a window, or
   NULL.  Backend-protocol code (selections, tray, menu/scroll native paths)
   that must resolve a raw window to its widget uses this; core dispatch never
   does — events arrive already carrying their target widget. */
extern Widget   _IswPlatformWidgetForWindow(IswDisplay dpy, IswWindow win);

extern Boolean   _IswPlatformWindowViewable(IswDisplay dpy, IswWindow win);
extern IswWindowId _IswPlatformWindowId(IswWindow win);
extern IswWindow   _IswPlatformWindowFromId(IswDisplay dpy, IswWindowId id);

/* Window attributes */
extern void    _IswPlatformChangeAttributes(IswDisplay dpy, IswWindow win,
                                            const IswWindowAttributes *attrs,
                                            unsigned int mask);

/* Root surface */
extern IswWindow _IswPlatformCreateRoot(IswDisplay dpy, IswScreen screen,
                                        const IswWindowGeometry *geom,
                                        const IswWindowAttributes *attrs);
extern void    _IswPlatformPresentRoot(IswDisplay dpy, IswWindow win,
                                       IswSurface surface, int width, int height);

/* Resource resolution */
extern IswDatabaseHandle _IswPlatformResourceFromString(const char *str);
extern IswDatabaseHandle _IswPlatformResourceFromFile(const char *filename);
extern IswDatabaseHandle _IswPlatformResourceFromManager(IswDisplay dpy,
                                                         IswScreen screen);
extern void _IswPlatformResourceCombine(IswDatabaseHandle source,
                                        IswDatabaseHandle *target, Boolean override);
extern void _IswPlatformResourcePut(IswDatabaseHandle *db, const char *resource,
                                    const char *value);
extern void _IswPlatformResourcePutLine(IswDatabaseHandle *db, const char *line);
extern char *_IswPlatformResourceToString(IswDatabaseHandle db);
extern void _IswPlatformResourceFree(IswDatabaseHandle db);
extern int  _IswPlatformResourceGetString(IswDatabaseHandle db, const char *res_name,
                                          const char *res_class, char **out);

/* WM hints */
extern void _IswPlatformSetWindowTitle(IswDisplay dpy, IswWindow win, const char *utf8);
extern void _IswPlatformSetIconTitle(IswDisplay dpy, IswWindow win, const char *utf8);
extern void _IswPlatformSetWmClass(IswDisplay dpy, IswWindow win,
                                   const char *name, const char *class_name);
extern void _IswPlatformSetWmProtocols(IswDisplay dpy, IswWindow win,
                                       const Atom *protocols, int num_protocols);
extern void _IswPlatformSetTransientFor(IswDisplay dpy, IswWindow win, IswWindow leader);
extern void _IswPlatformSetWindowType(IswDisplay dpy, IswWindow win, IswWindowType type);
extern void _IswPlatformSetPid(IswDisplay dpy, IswWindow win, uint32_t pid);
extern void _IswPlatformSetNormalHints(IswDisplay dpy, IswWindow win, uint32_t flags,
                                       int x, int y, int width, int height,
                                       int min_width, int min_height,
                                       int max_width, int max_height,
                                       int width_inc, int height_inc,
                                       int min_aspect_num, int min_aspect_den,
                                       int max_aspect_num, int max_aspect_den,
                                       int base_width, int base_height, int win_gravity);
extern void _IswPlatformSetWmHints(IswDisplay dpy, IswWindow win,
                                   const IswWmHints *hints);

/* Send a protocol/client message about `win` (X ClientMessage and equivalents).
   `type` is the message-type atom; `format` is 8/16/32; `data` points at the
   message payload (up to 20 bytes — five 32-bit words, or 20 bytes for format 8)
   and is copied into the message.  The message is delivered to `target` (often
   the root for EWMH _NET_WM_STATE / restack / startup broadcasts) with
   `event_mask`; `propagate` follows the window tree. */
extern void _IswPlatformSendMessage(IswDisplay dpy, IswWindow target,
                                    IswWindow win, Atom type, int format,
                                    const void *data,
                                    Boolean propagate, unsigned int event_mask);

/* Translate (x,y) in `src`'s coordinate space to the root window.  Returns
   False if the translation fails. */
extern Boolean _IswPlatformTranslateToRoot(IswDisplay dpy, IswWindow src,
                                           int x, int y,
                                           int *root_x, int *root_y);

/* Create a 1x1 input-only child of `parent` (used for the focus-proxy /
   user-time window).  Returns the new window. */
extern IswWindow _IswPlatformCreateInputOnly(IswDisplay dpy, IswWindow parent);

/* Block (up to `timeout_ms`) for the WM's response to a geometry/map request on
   `win`: returns when a ConfigureNotify for `win` arrives (fills *new_x/y/w/h),
   tracking ReparentNotify into `*reparented`.  Returns False on timeout. */
extern Boolean _IswPlatformWaitForConfigure(IswDisplay dpy, IswWindow win,
                                            unsigned long timeout_ms,
                                            int *new_x, int *new_y,
                                            int *new_w, int *new_h,
                                            int *new_border,
                                            Boolean *reparented);

#endif /* _ISWPlatform_h */
