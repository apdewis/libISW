/*
 * XtDatabase.h - Resource database type definitions for libXt
 *
 * This replaces XrmDatabase from <X11/Xresource.h> with the
 * xcb-util-xrm database type, and provides backward compatibility
 * typedefs.
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

#ifndef _XtDatabase_h
#define _XtDatabase_h

#include <xcb/xcb_xrm.h>

/*
 * The resource database type.
 * xcb-util-xrm provides xcb_xrm_database_t as an opaque type.
 * We use a pointer to it as our database handle.
 */
typedef xcb_xrm_database_t *XtDatabaseHandle;

/*
 * Backward compatibility - map old XrmDatabase to new type.
 * XrmDatabase was an opaque pointer in Xlib; xcb_xrm_database_t*
 * serves the same role.
 */
typedef xcb_xrm_database_t *XrmDatabase;

/*
 * XrmHashTable and XrmSearchList are no longer needed.
 * xcb-util-xrm does not use search lists; instead, full resource
 * name/class strings are passed to xcb_xrm_resource_get_string().
 * These are defined as void* to allow commented-out code to compile
 * without errors, but they should not be used in new code.
 */
typedef void *XrmHashTable;
typedef XrmHashTable *XrmSearchList;

/* XrmQGetResource - query resource database by quark name/class arrays */
extern Bool XrmQGetResource(
    XrmDatabase         /* db */,
    XrmNameList         /* names */,
    XrmClassList        /* classes */,
    XrmRepresentation * /* type_return */,
    XrmValue *          /* value_return */
);

#endif /* _XtDatabase_h */
