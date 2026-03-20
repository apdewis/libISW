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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "ContextI.h"
#include <stdlib.h>

/* Global context ID counter */
static int context_id_counter = 0;

/*
 * XtUniqueContext - Create a new unique context
 * Returns: New context identifier
 */
XtContext
XtUniqueContext(void)
{
    XtContext context = (XtContext) malloc(sizeof(*context));
    if (context) {
        context->id = ++context_id_counter;
        context->entries = NULL;
    }
    return context;
}

/*
 * XtSaveContext - Save data in a context
 * Parameters:
 *   dpy     - Display connection
 *   key     - Key (window ID or atom)
 *   context - Context identifier
 *   data    - Data to save
 * Returns: XT_CONTEXT_SUCCESS on success, error code on failure
 */
int
XtSaveContext(xcb_connection_t *dpy, XID key, XtContext context, const void *data)
{
    XtContextEntry *entry;

    if (!context)
        return XT_CONTEXT_BAD_CONTEXT;
    
    if (!data)
        return XT_CONTEXT_BAD_DATA;

    /* Check if entry already exists */
    HASH_FIND(hh, context->entries, &key, sizeof(key), entry);
    if (entry) {
        /* Update existing entry */
        entry->data = (void *)data;
    } else {
        /* Create new entry */
        entry = (XtContextEntry *) malloc(sizeof(XtContextEntry));
        if (!entry)
            return XT_CONTEXT_BAD_DATA;
        
        entry->dpy = dpy;
        entry->key = key;
        entry->data = (void *)data;
        HASH_ADD(hh, context->entries, key, sizeof(key), entry);
    }

    return XT_CONTEXT_SUCCESS;
}

/*
 * XtFindContext - Find data in a context
 * Parameters:
 *   dpy     - Display connection
 *   key     - Key (window ID or atom)
 *   context - Context identifier
 *   data    - Pointer to store found data
 * Returns: XT_CONTEXT_SUCCESS on success, error code on failure
 */
int
XtFindContext(xcb_connection_t *dpy, XID key, XtContext context, void **data)
{
    XtContextEntry *entry;

    if (!context)
        return XT_CONTEXT_BAD_CONTEXT;
    
    if (!data)
        return XT_CONTEXT_BAD_DATA;

    HASH_FIND(hh, context->entries, &key, sizeof(key), entry);
    if (entry) {
        *data = entry->data;
        return XT_CONTEXT_SUCCESS;
    }

    return XT_CONTEXT_NO_CONTEXT;
}

/*
 * XtDeleteContext - Delete data from a context
 * Parameters:
 *   dpy     - Display connection
 *   key     - Key (window ID or atom)
 *   context - Context identifier
 * Returns: XT_CONTEXT_SUCCESS on success, error code on failure
 */
int
XtDeleteContext(xcb_connection_t *dpy, XID key, XtContext context)
{
    XtContextEntry *entry;

    if (!context)
        return XT_CONTEXT_BAD_CONTEXT;

    HASH_FIND(hh, context->entries, &key, sizeof(key), entry);
    if (entry) {
        HASH_DEL(context->entries, entry);
        free(entry);
        return XT_CONTEXT_SUCCESS;
    }

    return XT_CONTEXT_NO_CONTEXT;
}