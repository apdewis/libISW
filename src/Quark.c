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
    IswQuark     quark;          /* The quark value */
    int         is_permanent;   /* If true, string is not owned by us */
    UT_hash_handle hh;          /* uthash handle */
} QuarkEntry;

/*
 * Module state - protected by LOCK_PROCESS/UNLOCK_PROCESS.
 */
static QuarkEntry   *quark_table = NULL;    /* hash table: string -> quark */
static const char   **quark_strings = NULL; /* array: quark -> string */
static int          *quark_permanent = NULL;/* array: quark -> is_permanent flag */
static IswQuark      next_quark = 1;         /* next quark to assign (0 = ISW_NULLQUARK) */
static int          quark_table_size = 0;   /* allocated size of quark_strings array */

#define INITIAL_QUARK_TABLE_SIZE 512
#define QUARK_TABLE_GROW_FACTOR  2

/*
 * _IswQuarkGrow - Grow the quark-to-string reverse mapping array.
 * Must be called with LOCK_PROCESS held.
 */
static void
_IswQuarkGrow(void)
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
        _IswAllocError("quark table");
        return; /* not reached if _IswAllocError exits */
    }

    /* Initialize new entries */
    for (int i = quark_table_size; i < new_size; i++) {
        quark_strings[i] = NULL;
        quark_permanent[i] = 0;
    }

    quark_table_size = new_size;
}

/*
 * _IswInternString - Core interning function.
 * If permanent is true, the string pointer is stored directly.
 * If permanent is false, a copy is made via strdup().
 * Must be called with LOCK_PROCESS held.
 */
static IswQuark
_IswInternString(const char *string, int permanent)
{
    QuarkEntry *entry;
    IswQuark quark;

    if (string == NULL)
        return ISW_NULLQUARK;

    /* Look up existing entry */
    HASH_FIND_STR(quark_table, string, entry);
    if (entry != NULL)
        return entry->quark;

    /* Grow reverse mapping array if needed */
    if (next_quark >= quark_table_size)
        _IswQuarkGrow();

    /* Assign new quark */
    quark = next_quark++;

    /* Create hash table entry */
    entry = malloc(sizeof(QuarkEntry));
    if (entry == NULL) {
        _IswAllocError("quark entry");
        return ISW_NULLQUARK; /* not reached */
    }

    if (permanent) {
        entry->string = string;
        entry->is_permanent = 1;
    } else {
        entry->string = strdup(string);
        if (entry->string == NULL) {
            free(entry);
            _IswAllocError("quark string");
            return ISW_NULLQUARK; /* not reached */
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
 * IswStringToQuark - Intern a string, making a copy.
 *
 * The string is copied internally; the caller may free the original
 * after this call returns. If the string was previously interned
 * (by either IswStringToQuark or IswPermStringToQuark), the existing
 * quark is returned without making a new copy.
 */
IswQuark
IswStringToQuark(const char *string)
{
    IswQuark result;

    LOCK_PROCESS;
    result = _IswInternString(string, 0);
    UNLOCK_PROCESS;

    return result;
}

/*
 * IswPermStringToQuark - Intern a permanent string without copying.
 *
 * The caller guarantees the string will remain valid for the lifetime
 * of the program (e.g., string literals). This avoids an internal copy.
 * If the string was previously interned, the existing quark is returned.
 */
IswQuark
IswPermStringToQuark(const char *string)
{
    IswQuark result;

    LOCK_PROCESS;
    result = _IswInternString(string, 1);
    UNLOCK_PROCESS;

    return result;
}

/*
 * IswQuarkToString - Look up the string for a quark.
 *
 * Returns the interned string, or NULL if the quark is invalid
 * (out of range or ISW_NULLQUARK). The returned string must not be
 * freed or modified by the caller.
 */
const char *
IswQuarkToString(IswQuark quark)
{
    const char *result;

    if (quark == ISW_NULLQUARK)
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
 * These provide the Xrm-named function symbols so legacy code that takes
 * the address of XrmStringToQuark etc. still links.
 */

IswQuark
XrmStringToQuark(const char *string)
{
    return IswStringToQuark(string);
}

IswQuark
XrmPermStringToQuark(const char *string)
{
    return IswPermStringToQuark(string);
}

const char *
XrmQuarkToString(IswQuark quark)
{
    return IswQuarkToString(quark);
}

void
XrmStringToBindingQuarkList(const char *name,
                            IswBindingType *bindings_return,
                            IswQuark *quarks_return)
{
    IswStringToBindingQuarkList(name, bindings_return, quarks_return);
}

/*
 * IswStringToBindingQuarkList - Parse a resource path string.
 *
 * Parses a string like "name.name*name" into parallel arrays of
 * bindings and quarks. The '.' separator produces IswBindTightly,
 * the '*' separator produces IswBindLoosely.
 *
 * The first binding in the output corresponds to the binding
 * *before* the first component (conventionally IswBindTightly).
 *
 * The quarks array is terminated with ISW_NULLQUARK.
 *
 * Both arrays must be pre-allocated by the caller with enough
 * space for all components.
 */
void
IswStringToBindingQuarkList(const char *name,
                           IswBindingType *bindings_return,
                           IswQuark *quarks_return)
{
    const char *p;
    const char *start;
    int idx = 0;
    char buf[256];

    if (name == NULL || *name == '\0') {
        quarks_return[0] = ISW_NULLQUARK;
        return;
    }

    p = name;

    /* Handle leading binding character */
    if (*p == '.' || *p == '*') {
        bindings_return[0] = (*p == '*') ? IswBindLoosely : IswBindTightly;
        p++;
    } else {
        bindings_return[0] = IswBindTightly;
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
                    quarks_return[idx] = ISW_NULLQUARK;
                    return;
                }
                memcpy(component, start, len);
                component[len] = '\0';
            }

            quarks_return[idx] = IswStringToQuark(component);

            if (component != buf)
                free(component);

            idx++;
        }

        /* Process separator */
        if (*p == '.' || *p == '*') {
            bindings_return[idx] = (*p == '*') ? IswBindLoosely : IswBindTightly;
            p++;
        }
    }

    quarks_return[idx] = ISW_NULLQUARK;
}
