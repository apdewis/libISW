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

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef WIN32
#define X_INCLUDE_PWD_H
#define XOS_USE_XT_LOCKING
#include <X11/Xos_r.h>
#endif

static String
XcbGetRootDirName(_IswString dest, int len)
{
#ifdef WIN32
    char *ptr1, *ptr2 = NULL;
    int len1 = 0, len2 = 0;
    if ((ptr1 = getenv("HOME"))) {
        len1 = strlen(ptr1);
    } else if ((ptr1 = getenv("HOMEDRIVE")) && (ptr2 = getenv("HOMEDIR"))) {
        len1 = strlen(ptr1);
        len2 = strlen(ptr2);
    } else if ((ptr2 = getenv("USERNAME"))) {
        len1 = strlen(ptr1 = "/users/");
        len2 = strlen(ptr2);
    }
    if ((len1 + len2 + 1) < len)
        sprintf(dest, "%s%s", ptr1, (ptr2) ? ptr2 : "");
    else
        *dest = '\0';
#else
#ifdef X_NEEDS_PWPARAMS
    _Xgetpwparams pwparams;
#endif
    char *ptr;
    if (len <= 0 || dest == NULL)
        return NULL;
    if ((ptr = getenv("HOME"))) {
        (void) strncpy(dest, ptr, (size_t)(len - 1));
        dest[len - 1] = '\0';
    } else {
        struct passwd *pw;
        if ((ptr = getenv("USER")))
            pw = _XGetpwnam(ptr, pwparams);
        else
            pw = _XGetpwuid(getuid(), pwparams);
        if (pw != NULL) {
            (void) strncpy(dest, pw->pw_dir, (size_t)(len - 1));
            dest[len - 1] = '\0';
        } else
            *dest = '\0';
    }
#endif
    return dest;
}

static void
XcbGetHostname(char *buf, int maxlen)
{
    if (maxlen <= 0 || buf == NULL)
        return;
    buf[0] = '\0';
    (void) gethostname(buf, (size_t) maxlen);
    buf[maxlen - 1] = '\0';
}

static IswDatabaseHandle
xcb_res_build_user_db(IswDisplay dpy, IswScreen screen)
{
    IswDatabaseHandle db = NULL;

    /* 1. RESOURCE_MANAGER property (or ~/.Xdefaults fallback) */
    IswDatabaseHandle rdb =
        WRAP(xcb_xrm_database_from_resource_manager(_IswXcbConn(dpy),
                                                     _IswXcbScreen(screen)));
    if (rdb) {
        xcb_xrm_database_combine(DB(rdb), PDB(&db), (bool) False);
    } else {
        const char *slashDotXdefaults = "/.Xdefaults";
        char filename[PATH_MAX];
        (void) XcbGetRootDirName(filename,
                                 PATH_MAX - (int) strlen(slashDotXdefaults) - 1);
        (void) strcat(filename, slashDotXdefaults);
        IswDatabaseHandle fdb = WRAP(xcb_xrm_database_from_file(filename));
        if (fdb)
            xcb_xrm_database_combine(DB(fdb), PDB(&db), (bool) False);
    }

    /* 2. XENVIRONMENT or ~/.Xdefaults-hostname */
    {
        char filenamebuf[PATH_MAX];
        char *filename;
        if (!(filename = getenv("XENVIRONMENT"))) {
            const char *slashDotXdefaultsDash = "/.Xdefaults-";
            int len;
            (void) XcbGetRootDirName(filename = filenamebuf,
                                     PATH_MAX -
                                     (int) strlen(slashDotXdefaultsDash) - 1);
            (void) strcat(filename, slashDotXdefaultsDash);
            len = (int) strlen(filename);
            XcbGetHostname(filename + len, PATH_MAX - len);
        }
        IswDatabaseHandle envdb = WRAP(xcb_xrm_database_from_file(filename));
        if (envdb)
            xcb_xrm_database_combine(DB(envdb), PDB(&db), (bool) False);
    }

    /* 3. XUSERFILESEARCHPATH / XAPPLRESDIR user app-defaults */
    {
        char *path = NULL;
        Boolean deallocate = False;
        if (!(path = getenv("XUSERFILESEARCHPATH"))) {
#if !defined(WIN32) || !defined(__MINGW32__)
            char *old_path;
            char homedir[PATH_MAX];
            XcbGetRootDirName(homedir, PATH_MAX);
            if (!(old_path = getenv("XAPPLRESDIR"))) {
                IswAsprintf(&path,
                    "%s/%%L/%%N%%C:%s/%%l/%%N%%C:%s/%%N%%C:%s/%%L/%%N:%s/%%l/%%N:%s/%%N",
                    homedir, homedir, homedir, homedir, homedir, homedir);
            } else {
                IswAsprintf(&path,
                    "%s/%%L/%%N%%C:%s/%%l/%%N%%C:%s/%%N%%C:%s/%%N%%C:%s/%%L/%%N:%s/%%l/%%N:%s/%%N:%s/%%N",
                    old_path, old_path, old_path, homedir,
                    old_path, old_path, old_path, homedir);
            }
            deallocate = True;
#endif
        }
        char *resolved = IswResolvePathname(dpy, NULL, NULL, NULL, path, NULL, 0, NULL);
        if (resolved) {
            IswDatabaseHandle fdb = WRAP(xcb_xrm_database_from_file(resolved));
            if (fdb)
                xcb_xrm_database_combine(DB(fdb), PDB(&db), (bool) False);
            IswFree(resolved);
        }
        if (deallocate)
            IswFree(path);
    }

    /* 4. System app-defaults directory */
    {
        char *resolved = IswResolvePathname(dpy, "app-defaults",
                                            NULL, NULL, NULL, NULL, 0, NULL);
        if (resolved) {
            IswDatabaseHandle fdb = WRAP(xcb_xrm_database_from_file(resolved));
            if (fdb)
                xcb_xrm_database_combine(DB(fdb), PDB(&db), (bool) False);
            IswFree(resolved);
        }
    }

    return db;
}

const IswPlatformResourceOps isw_platform_xcb_resource_ops = {
    .from_string           = xcb_res_from_string,
    .from_file             = xcb_res_from_file,
    .combine               = xcb_res_combine,
    .put_resource          = xcb_res_put_resource,
    .put_resource_line     = xcb_res_put_resource_line,
    .to_string             = xcb_res_to_string,
    .free                  = xcb_res_free,
    .get_string            = xcb_res_get_string,
    .build_user_db         = xcb_res_build_user_db,
};
