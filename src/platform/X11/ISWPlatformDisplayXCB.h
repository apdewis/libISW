#ifndef _ISWPlatformPrivateXCB_h
#define _ISWPlatformPrivateXCB_h

#include <xcb/xcb.h>

typedef struct _IswDisplayXCB {
    xcb_connection_t *conn;
} IswDisplayXCB;

#endif /* _ISWPlatformPrivateXCB_h */