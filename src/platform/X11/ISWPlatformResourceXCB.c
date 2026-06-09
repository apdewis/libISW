/*
 * ISWPlatformResourceXCB.c - Resource resolution (XCB platform backend)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The X11 backend's implementation of the resource-resolution ops
 * (struct _IswPlatformResourceOps, ISW/ISWPlatform.h).  This is the ONLY
 * translation unit that names libxcb-util-xrm: the toolkit holds an opaque
 * IswDatabaseHandle and reaches the store / .Xdefaults parser / RESOURCE_MANAGER
 * source / name-class precedence matcher through these verbs.  Xrm is X11's
 * particular answer to "what is the configured value of this resource?"; it is
 * confined here so a non-X backend can answer the same question from its own
 * source with no RESOURCE_MANAGER.
 *
 * The neutral IswDatabaseHandle IS an xcb_xrm_database_t* reinterpreted — the
 * casts below are the seam, exactly like _IswXcbConn / _IswXcbWindow elsewhere.
 *
 * Phase 15 (docs/ISWPLATFORM_PLAN.md).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>

#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"

/* Seam: the opaque handle reinterpreted as the native xrm database. */
#define DB(h)   ((xcb_xrm_database_t *) (h))
#define PDB(h)  ((xcb_xrm_database_t **) (h))
#define WRAP(p) ((IswDatabaseHandle) (p))

static IswDatabaseHandle
xcb_res_from_string(const char *str)
{
    return WRAP(xcb_xrm_database_from_string(str));
}

static IswDatabaseHandle
xcb_res_from_file(const char *filename)
{
    return WRAP(xcb_xrm_database_from_file(filename));
}

static IswDatabaseHandle
xcb_res_from_resource_manager(IswDisplay dpy, IswScreen screen)
{
    return WRAP(xcb_xrm_database_from_resource_manager(_IswXcbConn(dpy),
                                                       _IswXcbScreen(screen)));
}

static void
xcb_res_combine(IswDatabaseHandle source, IswDatabaseHandle *target,
                Boolean override)
{
    xcb_xrm_database_combine(DB(source), PDB(target), (bool) override);
}

static void
xcb_res_put_resource(IswDatabaseHandle *db, const char *resource,
                     const char *value)
{
    xcb_xrm_database_put_resource(PDB(db), resource, value);
}

static void
xcb_res_put_resource_line(IswDatabaseHandle *db, const char *line)
{
    xcb_xrm_database_put_resource_line(PDB(db), line);
}

static char *
xcb_res_to_string(IswDatabaseHandle db)
{
    return xcb_xrm_database_to_string(DB(db));
}

static void
xcb_res_free(IswDatabaseHandle db)
{
    xcb_xrm_database_free(DB(db));
}

static int
xcb_res_get_string(IswDatabaseHandle db, const char *res_name,
                   const char *res_class, char **out)
{
    return xcb_xrm_resource_get_string(DB(db), res_name, res_class, out);
}

const IswPlatformResourceOps isw_platform_xcb_resource_ops = {
    .from_string           = xcb_res_from_string,
    .from_file             = xcb_res_from_file,
    .from_resource_manager = xcb_res_from_resource_manager,
    .combine               = xcb_res_combine,
    .put_resource          = xcb_res_put_resource,
    .put_resource_line     = xcb_res_put_resource_line,
    .to_string             = xcb_res_to_string,
    .free                  = xcb_res_free,
    .get_string            = xcb_res_get_string,
};
