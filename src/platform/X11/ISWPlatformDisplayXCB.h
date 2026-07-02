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
} IswDisplayXCB;

/* window→widget table lifecycle (ISWPlatformWWTableXCB.c) */
extern void _IswXcbAllocWWTable(IswDisplay display);
extern void _IswXcbFreeWWTable(IswDisplay display);

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