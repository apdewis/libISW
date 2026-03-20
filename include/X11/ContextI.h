/***********************************************************

Copyright 2025 The Kilo Code Project

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#ifndef _XtcontextI_h
#define _XtcontextI_h

#include "Intrinsic.h"
#include "uthash.h"

/* 
 * XCB-based replacement for Xlib context management functions.
 * This provides a hash table-based implementation to replace:
 * - XUniqueContext()
 * - XSaveContext()
 * - XFindContext()
 * - XDeleteContext()
 */

/* Context data structure for hash table */
typedef struct {
    xcb_connection_t *dpy;      /* Display connection */
    XID key;                    /* Key: window ID or xcb_atom_t */
    void *data;                 /* Value: context data */
    UT_hash_handle hh;          /* Hash handle for uthash */
} XtContextEntry;

/* Context type identifier */
typedef struct {
    int id;                     /* Unique context identifier */
    XtContextEntry *entries;    /* Hash table of entries */
} *XtContext;

/* Function prototypes */
extern XtContext XtUniqueContext(void);
extern int XtSaveContext(xcb_connection_t *dpy, XID key, XtContext context, const void *data);
extern int XtFindContext(xcb_connection_t *dpy, XID key, XtContext context, void **data);
extern int XtDeleteContext(xcb_connection_t *dpy, XID key, XtContext context);

/* Return codes */
#define XT_CONTEXT_SUCCESS      0
#define XT_CONTEXT_BAD_CONTEXT  1
#define XT_CONTEXT_BAD_KEY      2
#define XT_CONTEXT_BAD_DATA     3
#define XT_CONTEXT_NO_CONTEXT   4

#endif /* _XtcontextI_h */
/* DON'T ADD STUFF AFTER THIS #endif */