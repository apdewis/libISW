#ifndef _ISWPlatformPrivateXCB_h
#define _ISWPlatformPrivateXCB_h

#include <xcb/xcb.h>

/* Association of a widget to the platform-owned window backing it.  The toolkit
   never holds a window handle; the platform maps widget→window here for the few
   widgets that back a real window (the shell root, tooltip popups). */
typedef struct _IswWidgetWindowMap {
    void        *widget;        /* opaque Widget key (not dereferenced) */
    xcb_window_t window;
} IswWidgetWindowMap;

typedef struct _IswDisplayXCB {
    xcb_connection_t   *conn;
    xcb_window_t        root_window;  /* shell's top-level (the default) */
    IswWidgetWindowMap *wmap;         /* widget→window associations */
    int                 wmap_count;
    int                 wmap_cap;
    struct _WWTable    *wwtable;      /* window→widget table (incl. foreign) */
    xcb_gcontext_t      blit_gc;      /* lazy GC for back-pixmap→window copy_area */
    struct _IswFrameSync *frame_sync; /* _NET_WM_SYNC_REQUEST records, one per
                                         sync-enabled toplevel */
    Boolean             sync_ext_initialized; /* xcb_sync_initialize done */

    /* XInput2 (smooth/trackpad scroll).  xi_opcode is the extension's major
       opcode (0 if XI2 is absent); xi_present is 1 once XI2 ≥ 2.0 is up.
       scroll_valuators is an opaque per-display cache of master-pointer
       scroll valuator indices/resolution (ISWPlatformInputXCB.c owns it). */
    uint8_t             xi_opcode;
    uint8_t             xi_present;
    void               *scroll_valuators;
} IswDisplayXCB;

/* window→widget table lifecycle (ISWPlatformWWTableXCB.c) */
extern void _IswXcbAllocWWTable(IswDisplay display);
extern void _IswXcbFreeWWTable(IswDisplay display);

/* XInput2 lifecycle (ISWPlatformInputXCB.c).  Init probes the extension +
   version at display-open time; SelectForWindow requests XI button/motion
   events on a freshly-created window (so XI smooth scroll arrives on every
   window, including popups/menus).  Free releases the valuator cache.  All
   three are no-ops when xcb-xinput is absent or the server lacks XI2. */
extern void _IswXcbInputInit(IswDisplay display);
extern void _IswXcbInputSelectForWindow(IswDisplay display, xcb_window_t window);
extern void _IswXcbInputFree(IswDisplay display);
/* Translate an XI2 generic event into an IswEvent.  Returns True if the event
   was an XI2 event this backend translated (button press/release remap +
   scroll-valuator motion); False to let the core translator handle it. */
extern Boolean _IswXcbInputTranslateEvent(IswDisplay display,
                                          xcb_generic_event_t *xev,
                                          IswEvent *out);

/* Shared event-translation helpers (ISWPlatformEventXCB.c), used by the XI2
   translator too.  Resolve the widget target for a window and the pointer
   position relative to the top-level shell window (physical px — the dispatch
   core descales to logical). */
#include <ISW/IswEvent.h>
extern IswEventTarget _IswXcbTargetForWindow(IswDisplay dpy, xcb_window_t window);
extern void _IswXcbShellCoordsForEvent(IswDisplay dpy, xcb_window_t event_win,
                                       int16_t event_x, int16_t event_y,
                                       int16_t root_x, int16_t root_y,
                                       int16_t *shell_x, int16_t *shell_y);

/* True if `widget` is still registered in the window→widget table (its window
   has not been destroyed) — backs _IswPlatformWidgetIsLive. */
extern Boolean _IswXcbWidgetRegistered(IswDisplay display, Widget widget);

/* _NET_WM_SYNC_REQUEST frame-sync record access (ISWPlatformAtomPropXCB.c).
   Latch stores the 64-bit sync value from a WM sync-request client message;
   returns True if `window` has frame sync enabled (the message is consumed).
   WindowDestroyed destroys the window's sync counter and frees its record. */
extern Boolean _IswXcbFrameSyncLatch(IswDisplay display, xcb_window_t window,
                                     uint32_t lo, uint32_t hi);
extern void _IswXcbFrameSyncWindowDestroyed(IswDisplay display,
                                            xcb_window_t window);

#endif /* _ISWPlatformPrivateXCB_h */