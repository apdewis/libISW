/*
 * XawContext.c - Context management implementation
 * 
 * Uses uthash for efficient XID → data mapping, replacing Xlib's context
 * functions which are not available in XCB.
 *
 * Copyright (c) 2026 Xaw3d Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <ISW/ISWContext.h>
#include "uthash.h"

/*
 * Hash table entry structure
 * 
 * We use a composite key of {id, context} to allow multiple contexts
 * per XID. The hash table is keyed by this composite.
 */
typedef struct _XawContextKey {
    XID        id;       /* window or resource ID */
    XContext   context;  /* context identifier */
} XawContextKey;

typedef struct _XawContextEntry {
    XawContextKey  key;      /* composite key (must be first for HASH_FIND) */
    XtPointer       data;     /* associated data */
    UT_hash_handle hh;       /* uthash handle */
} XawContextEntry;

/* Global hash table */
static XawContextEntry *context_table = NULL;

/* Context ID counter for XawUniqueContext */
static XContext next_context_id = 1;

/*
 * Thread safety note: In a multithreaded environment, these functions
 * should be protected with mutexes. For now, we assume single-threaded
 * usage as is typical for X11 applications. If thread safety is needed,
 * add LOCK_PROCESS/UNLOCK_PROCESS around hash table operations.
 */

/*
 * XawUniqueContext - Generate a unique context identifier
 * 
 * Returns a new, unique context ID suitable for use with XawSaveContext.
 */
XContext
XawUniqueContext(void)
{
    return next_context_id++;
}

/*
 * Compute hash for composite key
 */
static inline unsigned int
context_key_hash(XawContextKey *key)
{
    /* Simple hash combining both values */
    return (unsigned int)key->id ^ ((unsigned int)key->context << 16);
}

/*
 * Compare composite keys for equality
 */
static inline int
context_key_equal(XawContextKey *key1, XawContextKey *key2)
{
    return (key1->id == key2->id) && (key1->context == key2->context);
}

/*
 * XawSaveContext - Associate data with a window/ID and context
 * 
 * Parameters:
 *   dpy     - Display connection (unused, for Xlib API compatibility)
 *   id      - Window or resource ID
 *   context - Context identifier (from XawUniqueContext)
 *   data    - Data to associate
 *
 * Returns:
 *   0 on success, non-zero on failure
 */
int
XawSaveContext(xcb_connection_t *dpy _X_UNUSED,
               XID id,
               XContext context,
               XtPointer data)
{
    XawContextEntry *entry;
    XawContextKey key;
    
    key.id = id;
    key.context = context;
    
    /* Check if entry already exists */
    HASH_FIND(hh, context_table, &key, sizeof(XawContextKey), entry);
    
    if (entry != NULL) {
        /* Update existing entry */
        entry->data = data;
        return 0;
    }
    
    /* Create new entry */
    entry = (XawContextEntry *) malloc(sizeof(XawContextEntry));
    if (entry == NULL) {
        return 1;  /* allocation failed */
    }
    
    entry->key = key;
    entry->data = data;
    
    /* Add to hash table */
    HASH_ADD(hh, context_table, key, sizeof(XawContextKey), entry);
    
    return 0;
}

/*
 * XawFindContext - Retrieve data associated with a window/ID and context
 * 
 * Parameters:
 *   dpy          - Display connection (unused, for Xlib API compatibility)
 *   id           - Window or resource ID
 *   context      - Context identifier
 *   data_return  - Pointer to receive the associated data
 *
 * Returns:
 *   0 if found, non-zero if not found
 */
int
XawFindContext(xcb_connection_t *dpy _X_UNUSED,
               XID id,
               XContext context,
               XtPointer *data_return)
{
    XawContextEntry *entry;
    XawContextKey key;
    
    key.id = id;
    key.context = context;
    
    /* Search hash table */
    HASH_FIND(hh, context_table, &key, sizeof(XawContextKey), entry);
    
    if (entry != NULL) {
        *data_return = entry->data;
        return 0;  /* found */
    }
    
    return 1;  /* not found */
}

/*
 * XawDeleteContext - Remove association for a window/ID and context
 * 
 * Parameters:
 *   dpy     - Display connection (unused, for Xlib API compatibility)
 *   id      - Window or resource ID
 *   context - Context identifier
 *
 * Returns:
 *   0 if deleted, non-zero if not found
 */
int
XawDeleteContext(xcb_connection_t *dpy _X_UNUSED,
                 XID id,
                 XContext context)
{
    XawContextEntry *entry;
    XawContextKey key;
    
    key.id = id;
    key.context = context;
    
    /* Find entry */
    HASH_FIND(hh, context_table, &key, sizeof(XawContextKey), entry);
    
    if (entry != NULL) {
        /* Remove from hash table */
        HASH_DEL(context_table, entry);
        free(entry);
        return 0;
    }
    
    return 1;  /* not found */
}
