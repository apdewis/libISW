/*
 * ResourceReload.c - Automatic reload when the desktop's settings change
 *
 * Copyright (c) 2026 ISW Project
 *
 * The platform reports a change to the desktop's resource settings as an
 * IswSettingsChanged event (X11: RESOURCE_MANAGER / SCREEN_RESOURCES on a
 * root window).  Bursts are coalesced into one reload that runs when the
 * application is idle: the screen databases are rebuilt, fonts and cursors
 * reload when their settings changed, database-driven resources are
 * re-applied, every shell is redrawn once, and the display's
 * resourcesChangedCallback runs.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicI.h>
#include <ISW/ResourceI.h>
#include <ISW/HookObjI.h>
#include <ISW/ISWPlatform.h>
#include <ISW/ISWRender.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

/* Quiet period after the last settings-change event before the reload is
   queued: xrdb and session managers often write the property several times
   in a row. */
#define RELOAD_DEBOUNCE_MS 200

static const char *const fontKeys[][2] = {
    { "Xft.antialias", "Xft.Antialias" },
    { "Xft.hinting",   "Xft.Hinting" },
    { "Xft.hintstyle", "Xft.HintStyle" },
    { "Xft.rgba",      "Xft.Rgba" },
    { "Xft.lcdfilter", "Xft.LcdFilter" },
    { "Xft.dpi",       "Xft.Dpi" },
};

static const char *const cursorKeys[][2] = {
    { "Xcursor.theme", "Xcursor.Theme" },
    { "Xcursor.size",  "Xcursor.Size" },
    { "Xft.dpi",       "Xft.Dpi" },
};

static char *
DbValue(IswDatabaseHandle db, const char *name, const char *class)
{
    char *value = NULL;

    if (db == NULL ||
        _IswPlatformResourceGetString(db, name, class, &value) < 0)
        return NULL;
    return value;
}

/* True if any of the keys has a different value in `old_db` and `new_db`. */
static Boolean
KeysDiffer(IswDatabaseHandle old_db, IswDatabaseHandle new_db,
           const char *const keys[][2], Cardinal nkeys)
{
    Cardinal i;
    Boolean differ = False;

    for (i = 0; i < nkeys && !differ; i++) {
        char *a = DbValue(old_db, keys[i][0], keys[i][1]);
        char *b = DbValue(new_db, keys[i][0], keys[i][1]);

        differ = (a == NULL) != (b == NULL) ||
                 (a != NULL && strcmp(a, b) != 0);
        free(a);
        free(b);
    }
    return differ;
}

/* The autoReloadResources resource: the user's opt-out. */
static Boolean
AutoReloadResource(IswDisplay dpy, IswPerDisplay pd)
{
    IswDatabaseHandle db;
    char name[256], class[256];
    char *value;
    Boolean on = True;

    db = IswScreenDatabase(pd->ops->display->screen(dpy, pd->defaultScreen));
    snprintf(name, sizeof(name), "%s.autoReloadResources",
             pd->name ? pd->name : "");
    snprintf(class, sizeof(class), "%s.AutoReloadResources",
             pd->class ? pd->class : "");
    value = DbValue(db, name, class);
    if (value != NULL) {
        on = !(strcasecmp(value, "false") == 0 || strcasecmp(value, "off") == 0 ||
               strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0);
        free(value);
    }
    return on;
}

static void
AutoReload(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    const IswPlatformOps *ops = pd->ops;
    int s, nscreens = ops->display->screen_count(dpy);
    IswDatabaseHandle *old_dbs, *new_dbs;
    HookObject ho = (HookObject) IswHooksOfDisplay(dpy);
    IswResourcesChangedDataRec data;
    Widget *roots;
    Cardinal nroots, i;

    /* Rebuild every screen's database, keeping the old one to compare. */
    old_dbs = (IswDatabaseHandle *) __IswCalloc((Cardinal) nscreens,
                                                sizeof(IswDatabaseHandle));
    new_dbs = (IswDatabaseHandle *) __IswCalloc((Cardinal) nscreens,
                                                sizeof(IswDatabaseHandle));
    for (s = 0; s < nscreens; s++) {
        IswScreen screen = ops->display->screen(dpy, s);
        old_dbs[s] = _IswDetachScreenDatabase(screen);
        new_dbs[s] = IswScreenDatabase(screen);
    }

    data.changed = IswResourcesChangedDatabase;
    s = pd->defaultScreen;
    if (KeysDiffer(old_dbs[s], new_dbs[s], fontKeys, IswNumber(fontKeys)))
        data.changed |= IswResourcesChangedFonts;
    if (KeysDiffer(old_dbs[s], new_dbs[s], cursorKeys, IswNumber(cursorKeys)))
        data.changed |= IswResourcesChangedCursors;

    /* Every stage repaints into one composite pass at the end. */
    ISWRenderBeginDeferComposite();

    if (data.changed & IswResourcesChangedFonts)
        _ISWRenderReloadFontStage(dpy);
    if (data.changed & IswResourcesChangedCursors)
        _ISWReloadCursorStage(dpy);

    /* Re-apply database-driven resources on every application shell tree,
       each against its own screen's databases. */
    nroots = ho->hooks.num_shells;
    if (nroots > 0) {
        roots = (Widget *) IswMalloc(nroots * sizeof(Widget));
        memcpy(roots, ho->hooks.shells, nroots * sizeof(Widget));
        for (i = 0; i < nroots; i++) {
            Widget root = roots[i];
            int scr = pd->defaultScreen;

            if (root->core.being_destroyed)
                continue;
            for (s = 0; s < nscreens; s++)
                if (ops->display->screen(dpy, s) == root->core.screen)
                    scr = s;
            _IswRefetchSubtree(root, old_dbs[scr], new_dbs[scr]);
        }
        IswFree((char *) roots);
    }

    _ISWRenderRedrawDisplay(dpy);
    ISWRenderFlushComposites();

    for (s = 0; s < nscreens; s++)
        if (old_dbs[s] != NULL)
            _IswPlatformResourceFree(old_dbs[s]);
    IswFree((char *) old_dbs);
    IswFree((char *) new_dbs);

    _IswPlatformFlush(dpy);

    IswCallCallbackList((Widget) ho, ho->hooks.resources_changed_callbacks,
                        (IswPointer) &data);
}

static Boolean
ReloadWorkProc(IswPointer closure)
{
    IswDisplay dpy = (IswDisplay) closure;

    _IswGetPerDisplay(dpy)->reload_work = 0;
    AutoReload(dpy);
    return True;
}

static void
ReloadTimer(IswPointer closure, IswIntervalId *id _X_UNUSED)
{
    IswDisplay dpy = (IswDisplay) closure;
    IswPerDisplay pd = _IswGetPerDisplay(dpy);

    pd->reload_timer = 0;
    if (pd->reload_work == 0)
        pd->reload_work = IswAppAddWorkProc(pd->appContext, ReloadWorkProc,
                                            (IswPointer) dpy);
}

/*
 * _IswScheduleSettingsReload - the desktop's resource settings changed
 * (IswSettingsChanged).  Restart the debounce timer; when it expires the
 * reload is queued as a work proc and runs once the application is idle.
 */
void
_IswScheduleSettingsReload(IswDisplay dpy)
{
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    IswAppContext app = pd->appContext;

    if (!app->auto_reload || !AutoReloadResource(dpy, pd))
        return;

    if (pd->reload_timer)
        IswRemoveTimeOut(pd->reload_timer);
    pd->reload_timer = IswAppAddTimeOut(app, RELOAD_DEBOUNCE_MS, ReloadTimer,
                                        (IswPointer) dpy);
}

void
IswAppSetAutoReload(IswAppContext app, _IswBoolean auto_reload)
{
    LOCK_APP(app);
    app->auto_reload = auto_reload ? True : False;
    UNLOCK_APP(app);
}

void
IswPutLineResource(IswDisplay dpy, _Xconst char *line)
{
    IswPerDisplay pd;
    int s, nscreens;

    if (dpy == NULL || line == NULL)
        return;
    pd = _IswGetPerDisplay(dpy);

    LOCK_PROCESS;
    pd->put_lines = (String *) IswRealloc((char *) pd->put_lines,
                                          (pd->num_put_lines + 1) *
                                          sizeof(String));
    pd->put_lines[pd->num_put_lines++] = IswNewString((String) line);

    /* Apply to the databases already built; a rebuilt one re-applies it. */
    if (pd->per_screen_db) {
        nscreens = pd->ops->display->screen_count(dpy);
        for (s = 0; s < nscreens; s++)
            if (pd->per_screen_db[s])
                _IswPlatformResourcePutLine(&pd->per_screen_db[s], line);
    }
    UNLOCK_PROCESS;
}
