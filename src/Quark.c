/*
 * Quark.c - Standalone quark (string interning) implementation for libXt
 *
 * This provides a thread-safe string interning system that maps strings
 * to integer quark values for fast comparison. It replaces the quark
 * functionality previously provided by Xlib's XRM (X Resource Manager).
 *
 * Implementation uses uthash for the string-to-quark hash table and
 * a dynamically growing array for the quark-to-string reverse mapping.
 *
 * Copyright (c) 2024 libXt contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "IntrinsicI.h"
#include "uthash.h"

/*
 * Hash table entry: maps a string to its quark value.
 */
typedef struct _QuarkEntry {
    const char  *string;        /* The interned string (key) */
    XtQuark     quark;          /* The quark value */
    int         is_permanent;   /* If true, string is not owned by us */
    UT_hash_handle hh;          /* uthash handle */
} QuarkEntry;

/*
 * Module state - protected by LOCK_PROCESS/UNLOCK_PROCESS.
 */
static QuarkEntry   *quark_table = NULL;    /* hash table: string -> quark */
static const char   **quark_strings = NULL; /* array: quark -> string */
static int          *quark_permanent = NULL;/* array: quark -> is_permanent flag */
static XtQuark      next_quark = 1;         /* next quark to assign (0 = NULLQUARK) */
static int          quark_table_size = 0;   /* allocated size of quark_strings array */

#define INITIAL_QUARK_TABLE_SIZE 512
#define QUARK_TABLE_GROW_FACTOR  2

/*
 * _XtQuarkGrow - Grow the quark-to-string reverse mapping array.
 * Must be called with LOCK_PROCESS held.
 */
static void
_XtQuarkGrow(void)
{
    int new_size;

    if (quark_table_size == 0) {
        new_size = INITIAL_QUARK_TABLE_SIZE;
    } else {
        new_size = quark_table_size * QUARK_TABLE_GROW_FACTOR;
    }

    quark_strings = realloc(quark_strings, (size_t)new_size * sizeof(const char *));
    quark_permanent = realloc(quark_permanent, (size_t)new_size * sizeof(int));

    if (quark_strings == NULL || quark_permanent == NULL) {
        /* Fatal allocation failure */
        _XtAllocError("quark table");
        return; /* not reached if _XtAllocError exits */
    }

    /* Initialize new entries */
    for (int i = quark_table_size; i < new_size; i++) {
        quark_strings[i] = NULL;
        quark_permanent[i] = 0;
    }

    quark_table_size = new_size;
}

/*
 * _XtInternString - Core interning function.
 * If permanent is true, the string pointer is stored directly.
 * If permanent is false, a copy is made via strdup().
 * Must be called with LOCK_PROCESS held.
 */
static XtQuark
_XtInternString(const char *string, int permanent)
{
    QuarkEntry *entry;
    XtQuark quark;

    if (string == NULL)
        return XT_NULLQUARK;

    /* Look up existing entry */
    HASH_FIND_STR(quark_table, string, entry);
    if (entry != NULL)
        return entry->quark;

    /* Grow reverse mapping array if needed */
    if (next_quark >= quark_table_size)
        _XtQuarkGrow();

    /* Assign new quark */
    quark = next_quark++;

    /* Create hash table entry */
    entry = malloc(sizeof(QuarkEntry));
    if (entry == NULL) {
        _XtAllocError("quark entry");
        return XT_NULLQUARK; /* not reached */
    }

    if (permanent) {
        entry->string = string;
        entry->is_permanent = 1;
    } else {
        entry->string = strdup(string);
        if (entry->string == NULL) {
            free(entry);
            _XtAllocError("quark string");
            return XT_NULLQUARK; /* not reached */
        }
        entry->is_permanent = 0;
    }

    entry->quark = quark;
    HASH_ADD_KEYPTR(hh, quark_table, entry->string,
                    strlen(entry->string), entry);

    /* Store in reverse mapping */
    quark_strings[quark] = entry->string;
    quark_permanent[quark] = entry->is_permanent;

    return quark;
}

/*
 * XtStringToQuark - Intern a string, making a copy.
 *
 * The string is copied internally; the caller may free the original
 * after this call returns. If the string was previously interned
 * (by either XtStringToQuark or XtPermStringToQuark), the existing
 * quark is returned without making a new copy.
 */
XtQuark
XtStringToQuark(const char *string)
{
    XtQuark result;

    LOCK_PROCESS;
    result = _XtInternString(string, 0);
    UNLOCK_PROCESS;

    return result;
}

/*
 * XtPermStringToQuark - Intern a permanent string without copying.
 *
 * The caller guarantees the string will remain valid for the lifetime
 * of the program (e.g., string literals). This avoids an internal copy.
 * If the string was previously interned, the existing quark is returned.
 */
XtQuark
XtPermStringToQuark(const char *string)
{
    XtQuark result;

    LOCK_PROCESS;
    result = _XtInternString(string, 1);
    UNLOCK_PROCESS;

    return result;
}

/*
 * XtQuarkToString - Look up the string for a quark.
 *
 * Returns the interned string, or NULL if the quark is invalid
 * (out of range or NULLQUARK). The returned string must not be
 * freed or modified by the caller.
 */
const char *
XtQuarkToString(XtQuark quark)
{
    const char *result;

    if (quark == XT_NULLQUARK)
        return NULL;

    LOCK_PROCESS;
    if (quark > 0 && quark < next_quark)
        result = quark_strings[quark];
    else
        result = NULL;
    UNLOCK_PROCESS;

    return result;
}

/*
 * Backward compatibility wrapper functions.
 * These provide actual function symbols for XrmStringToQuark,
 * XrmPermStringToQuark, and XrmQuarkToString so they can be
 * used as function pointers.
 */

#undef XrmStringToQuark
XtQuark
XrmStringToQuark(const char *string)
{
    return XtStringToQuark(string);
}

#undef XrmPermStringToQuark
XtQuark
XrmPermStringToQuark(const char *string)
{
    return XtPermStringToQuark(string);
}

#undef XrmQuarkToString
const char *
XrmQuarkToString(XtQuark quark)
{
    return XtQuarkToString(quark);
}

#undef XrmStringToBindingQuarkList
void
XrmStringToBindingQuarkList(const char *name,
                            XtBindingType *bindings_return,
                            XtQuark *quarks_return)
{
    XtStringToBindingQuarkList(name, bindings_return, quarks_return);
}

/*
 * XtStringToBindingQuarkList - Parse a resource path string.
 *
 * Parses a string like "name.name*name" into parallel arrays of
 * bindings and quarks. The '.' separator produces XtBindTightly,
 * the '*' separator produces XtBindLoosely.
 *
 * The first binding in the output corresponds to the binding
 * *before* the first component (conventionally XtBindTightly).
 *
 * The quarks array is terminated with XT_NULLQUARK.
 *
 * Both arrays must be pre-allocated by the caller with enough
 * space for all components.
 */
void
XtStringToBindingQuarkList(const char *name,
                           XtBindingType *bindings_return,
                           XtQuark *quarks_return)
{
    const char *p;
    const char *start;
    int idx = 0;
    char buf[256];

    if (name == NULL || *name == '\0') {
        quarks_return[0] = XT_NULLQUARK;
        return;
    }

    p = name;

    /* Handle leading binding character */
    if (*p == '.' || *p == '*') {
        bindings_return[0] = (*p == '*') ? XtBindLoosely : XtBindTightly;
        p++;
    } else {
        bindings_return[0] = XtBindTightly;
    }

    while (*p != '\0') {
        /* Find the start of the next component */
        start = p;

        /* Scan to next separator or end */
        while (*p != '\0' && *p != '.' && *p != '*')
            p++;

        /* Extract component name */
        if (p - start > 0) {
            size_t len = (size_t)(p - start);
            char *component;

            if (len < sizeof(buf)) {
                memcpy(buf, start, len);
                buf[len] = '\0';
                component = buf;
            } else {
                component = malloc(len + 1);
                if (component == NULL) {
                    quarks_return[idx] = XT_NULLQUARK;
                    return;
                }
                memcpy(component, start, len);
                component[len] = '\0';
            }

            quarks_return[idx] = XtStringToQuark(component);

            if (component != buf)
                free(component);

            idx++;
        }

        /* Process separator */
        if (*p == '.' || *p == '*') {
            bindings_return[idx] = (*p == '*') ? XtBindLoosely : XtBindTightly;
            p++;
        }
    }

    quarks_return[idx] = XT_NULLQUARK;
}
