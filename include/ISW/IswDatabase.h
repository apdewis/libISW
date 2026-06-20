/*
 * IswDatabase.h - Resource database type definitions
 *
 * The neutral, opaque resource database handle and its query function.
 * Backward compatibility typedefs map the old Xrm names.
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

#ifndef _IswDatabase_h
#define _IswDatabase_h

/*
 * The resource database type — a neutral, opaque toolkit handle.
 *
 * Phase 15: the database is no longer typed on xcb-util-xrm's
 * xcb_xrm_database_t.  Resource resolution is X11's particular answer to a
 * general question ("what is the configured value of this resource for this
 * widget?"), so the toolkit holds only an opaque handle and reaches the
 * store/parser/matcher through the platform resource ops (ISW/ISWPlatform.h).
 * The XCB backend's resource-ops implementation casts this handle to
 * xcb_xrm_database_t* internally — Xrm is confined to that one translation
 * unit; this header carries no xcb dependency.
 */
typedef struct _IswResourceDb *IswDatabaseHandle;

/*
 * Backward compatibility - the Xlib XrmDatabase name, per CLAUDE.md, backed by
 * the same neutral handle.
 */
typedef struct _IswResourceDb *XrmDatabase;

/*
 * XrmHashTable and XrmSearchList are no longer used: resolution passes a full
 * name/class path string to the resource-resolution ops, not a search list.
 * Kept as void* so legacy/commented-out code still compiles; not for new use.
 */
typedef void *XrmHashTable;
typedef XrmHashTable *XrmSearchList;

/* IswQGetResource - query resource database by quark name/class arrays */
extern Bool IswQGetResource(
    IswDatabaseHandle   /* db */,
    IswQuarkList        /* names */,
    IswQuarkList        /* classes */,
    IswRepresentation * /* type_return */,
    IswValueRec *       /* value_return */
);

/* Backward compatibility */
#define XrmQGetResource IswQGetResource

#endif /* _IswDatabase_h */
