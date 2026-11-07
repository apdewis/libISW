/*
 * ISWCursor.c - Live cursor reload
 *
 * Copyright (c) 2026 ISW Project
 *
 * ISWReloadCursors reloads every cursor ISW loaded by name, so a change to
 * the desktop's cursor theme or size reaches a running application.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/IntrinsicI.h>
#include <ISW/CompositeP.h>
#include <ISW/SimpleP.h>
#include <ISW/ShellP.h>
#include <ISW/HookObjI.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWPlatform.h>
#include <ISW/ISWRender.h>
#include <ISW/ResourceI.h>
#include <string.h>

/* An old String-converted cursor and the name it was converted from. */
typedef struct {
    IswCursor cursor;
    String    name;
} CursorName;

typedef struct {
    CursorName *list;
    Cardinal    n;
    Cardinal    cap;
} CursorNameMap;

static void
_isw_note_cursor(IswValuePtr from, IswValuePtr to,
                 IswValuePtr args _X_UNUSED, Cardinal num_args _X_UNUSED,
                 IswPointer closure)
{
    CursorNameMap *map = (CursorNameMap *) closure;
    IswCursor cursor = *(IswCursor *) to->addr;

    if (cursor == IswCursorNone)
        return;
    if (map->n == map->cap) {
        map->cap = map->cap ? map->cap * 2 : 16;
        map->list = (CursorName *) IswRealloc((char *) map->list,
                                              map->cap * sizeof(CursorName));
    }
    map->list[map->n].cursor = cursor;
    map->list[map->n].name = IswNewString((String) from->addr);
    map->n++;
}

static String
_isw_cursor_name(CursorNameMap *map, IswCursor cursor)
{
    Cardinal i;

    for (i = 0; i < map->n; i++)
        if (map->list[i].cursor == cursor)
            return map->list[i].name;
    return NULL;
}

/* Re-convert every cursor-typed resource in `res` (stored at `base`) whose
   value is an old String-converted cursor, and hand the new cursor to the
   widget through IswSetValues so its class applies it. */
static void
_isw_reload_resources(Widget w, char *base, IswResourceList res, Cardinal nres,
                      CursorNameMap *map)
{
    Cardinal i;

    for (i = 0; i < nres; i++) {
        IswCursor old, cursor;
        IswValueRec from, to;
        String name;

        if (strcmp(res[i].resource_type, IswRCursor) != 0 ||
            res[i].resource_size != sizeof(IswCursor))
            continue;
        memcpy(&old, base + res[i].resource_offset, sizeof(IswCursor));
        if ((name = _isw_cursor_name(map, old)) == NULL)
            continue;

        from.addr = (IswPointer) name;
        from.size = (unsigned) strlen(name) + 1;
        to.addr = (IswPointer) &cursor;
        to.size = sizeof(IswCursor);
        if (!IswConvertAndStore(w, IswRString, &from, IswRCursor, &to))
            continue;
        /* Still a named cursor, not one the application set. */
        _IswSuppressAppSetMarking(True);
        IswVaSetValues(w, res[i].resource_name, cursor, NULL);
        _IswSuppressAppSetMarking(False);
    }
}

static void
_isw_reload_tree(Widget w, CursorNameMap *map)
{
    IswResourceList res;
    Cardinal nres, i;

    if (w == NULL || w->core.being_destroyed)
        return;

    if (IswIsComposite(w)) {
        CompositeWidget cw = (CompositeWidget) w;
        for (i = 0; i < cw->composite.num_children; i++)
            _isw_reload_tree(cw->composite.children[i], map);
    }
    if (IswIsSubclass(w, simpleWidgetClass)) {
        SimpleWidgetClass sc = (SimpleWidgetClass) w->core.widget_class;
        if (sc->simple_class.nth_windowless_child != NULL) {
            int k = 0;
            Widget child;
            while ((child = (*sc->simple_class.nth_windowless_child)(w, k++))
                   != NULL)
                _isw_reload_tree(child, map);
        }
    }
    if (IswIsWidget(w)) {
        for (i = 0; i < w->core.num_popups; i++)
            _isw_reload_tree(w->core.popup_list[i], map);
    }

    /* A realized shell's window carries the themed default cursor (shells
       realized without one are left alone). */
    if (IswIsShell(w) && IswIsRealized(w) &&
        ((ShellWidget) w)->shell.theme_cursor != IswCursorNone) {
        ShellWidget sh = (ShellWidget) w;
        IswCursor old = sh->shell.theme_cursor;

        sh->shell.theme_cursor = _IswLoadThemedCursor(IswDisplayOf(w),
                                                      w->core.screen,
                                                      "left_ptr");
        _IswSetWindowCursor(w, sh->shell.theme_cursor);
        _IswFreeCursor(w, old);
    }

    if (map->n == 0)
        return;

    IswGetResourceList(IswClass(w), &res, &nres);
    _isw_reload_resources(w, (char *) w, res, nres, map);
    IswFree((char *) res);

    if (w->core.constraints != NULL && IswParent(w) != NULL &&
        IswIsConstraint(IswParent(w))) {
        IswGetConstraintResourceList(IswClass(IswParent(w)), &res, &nres);
        if (res != NULL) {
            _isw_reload_resources(w, (char *) w->core.constraints, res, nres,
                                  map);
            IswFree((char *) res);
        }
    }
}

void
ISWReloadCursors(IswDisplay dpy)
{
    _ISWReloadCursorStage(dpy);
}

void
_ISWReloadCursorStage(IswDisplay dpy)
{
    IswPerDisplay pd;
    HookObject ho;
    Widget *roots;
    Cardinal nroots, i;
    CursorNameMap map = { NULL, 0, 0 };

    if (dpy == NULL)
        return;
    pd = _IswGetPerDisplay(dpy);

    /* Backend drops its theme state; internally held cursors (ListViewHeader
       resize cursor) see the new generation and load again on next use. */
    _IswPlatformCursorSettingsChanged(dpy);
    pd->cursor_generation++;

    /* Drop every cached String to Cursor conversion on this display, noting
       which name each old cursor came from.  Old cursors are released here;
       windows still showing one keep it until it is replaced below. */
    _IswCacheFlushConverter(pd->appContext, (IswPointer) &pd->heap,
                            IswCvtStringToCursor, _isw_note_cursor,
                            (IswPointer) &map);

    /* Walk a snapshot of the application shells: a set_values may create
       widgets and change the hook object's list. */
    ho = (HookObject) IswHooksOfDisplay(dpy);
    nroots = ho->hooks.num_shells;
    if (nroots > 0) {
        roots = (Widget *) IswMalloc(nroots * sizeof(Widget));
        memcpy(roots, ho->hooks.shells, nroots * sizeof(Widget));
        for (i = 0; i < nroots; i++)
            _isw_reload_tree(roots[i], &map);
        IswFree((char *) roots);
    }

    /* Windowless widgets show their cursor on the ancestor's window only
       while the pointer is over them; re-apply for the current one. */
    if (pd->pdi.pointerWidget != NULL)
        _IswSimpleApplyCursor(pd->pdi.pointerWidget);

    for (i = 0; i < map.n; i++)
        IswFree(map.list[i].name);
    IswFree((char *) map.list);

    _IswPlatformFlush(dpy);
}
