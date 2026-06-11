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
} IswDisplayXCB;

/* window→widget table lifecycle (ISWPlatformWWTableXCB.c) */
extern void _IswXcbAllocWWTable(IswDisplay display);
extern void _IswXcbFreeWWTable(IswDisplay display);

#endif /* _ISWPlatformPrivateXCB_h */