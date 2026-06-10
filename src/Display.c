/***********************************************************
Copyright (c) 1993, Oracle and/or its affiliates.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*

Copyright 1987, 1988, 1998  The Open Group

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
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
#include "ResConfigP.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>


#ifdef XTHREADS
void (*_IswProcessLock) (void) = NULL;
void (*_IswProcessUnlock) (void) = NULL;
void (*_IswInitAppLock) (IswAppContext) = NULL;
#endif

static _Xconst _IswString IswNnoPerDisplay = "noPerDisplay";

ProcessContext
_IswGetProcessContext(void)
{
    static ProcessContextRec processContextRec = {
        (IswAppContext) NULL,
        (IswAppContext) NULL,
        (ConverterTable) NULL,
        {(IswLanguageProc) NULL, (IswPointer) NULL}
    };

    return &processContextRec;
}

IswAppContext
_IswDefaultAppContext(void)
{
    ProcessContext process = _IswGetProcessContext();
    IswAppContext app;

    LOCK_PROCESS;
    if (process->defaultAppContext == NULL) {
        process->defaultAppContext = IswCreateApplicationContext();
    }
    app = process->defaultAppContext;
    UNLOCK_PROCESS;
    return app;
}

static void
AddToAppContext(IswDisplay dpy, IswAppContext app)
{
#define DISPLAYS_TO_ADD 4

    if (app->count >= app->max) {
        app->max = (short) (app->max + DISPLAYS_TO_ADD);
        app->list = IswReallocArray(app->list,
                                   (Cardinal) app->max,
                                   (Cardinal) sizeof(IswDisplay));
    }

    app->list[app->count++] = dpy;
    app->rebuild_fdlist = TRUE;
#ifdef USE_POLL
    app->fds.nfds++;
#else
    if (_IswPlatformConnectionFd((IswDisplay)dpy) + 1 > app->fds.nfds) {
        app->fds.nfds = _IswPlatformConnectionFd((IswDisplay)dpy) + 1;
    }
#endif
#undef DISPLAYS_TO_ADD
}

static void
IswDeleteFromAppContext(IswDisplay d, register IswAppContext app)
{
    register int i;

    for (i = 0; i < app->count; i++)
        if (app->list[i] == d)
            break;

    if (i < app->count) {
        if (i <= app->last && app->last > 0)
            app->last--;
        for (i++; i < app->count; i++)
            app->list[i - 1] = app->list[i];
        app->count--;
    }
    app->rebuild_fdlist = TRUE;
#ifdef USE_POLL
    app->fds.nfds--;
#else
    if ((_IswPlatformConnectionFd((IswDisplay)d) + 1) == app->fds.nfds)
        app->fds.nfds--;
    else                        /* Unnecessary, just to be fool-proof */
        FD_CLR(_IswPlatformConnectionFd((IswDisplay)d), &app->fds.rmask);
#endif
}

static IswPerDisplay
NewPerDisplay(IswDisplay dpy)
{
    PerDisplayTablePtr pd;

    pd = IswNew(PerDisplayTable);

    LOCK_PROCESS;
    pd->dpy = dpy;
    pd->next = _IswperDisplayList;
    _IswperDisplayList = pd;
    UNLOCK_PROCESS;
    return &(pd->perDpy);
}

static IswPerDisplay
InitPerDisplay(IswDisplay dpy,
               int defaultScreen,
               IswAppContext app,
               _Xconst char *name,
               _Xconst char *classname,
               const IswPlatformOps *ops)
{
    IswPerDisplay pd;

    AddToAppContext(dpy, app);

    pd = NewPerDisplay(dpy);
    /* Carry the backend ops (selected at IswOpenDisplay, before the connection
     * existed) on the per-display record; every _IswPlatform* wrapper recovers
     * the ops from here rather than a process-global accessor. */
    pd->ops = ops;
    _IswHeapInit(&pd->heap);
    pd->destroy_callbacks = NULL;
    pd->defaultScreen = defaultScreen;
    pd->case_cvt = NULL;
    pd->defaultKeycodeTranslator = IswTranslateKey;
    pd->keysyms_serial = 0;
    pd->keysyms = NULL;

    //XDisplayKeycodes(dpy, &pd->min_keycode, &pd->max_keycode);
    pd->min_keycode = 8; //just assume the full legal range for X11
    pd->max_keycode = 255;
    pd->modKeysyms = NULL;
    pd->modsToKeysyms = NULL;
    pd->appContext = app;
    pd->name = (String)name;
    pd->class = (String)classname;
    pd->being_destroyed = False;
    pd->pixmap_tab = NULL;
    pd->language = NULL;
    pd->rv = False;
    pd->last_event.full_sequence = 0;
    pd->last_timestamp = 0;
    _IswAllocTMContext(pd);
    pd->mapping_callbacks = NULL;

    pd->PerWidgetContext = NULL;    /* uthash head must be NULL before first use */
    pd->pdi.grabList = NULL;
    pd->pdi.trace = NULL;
    pd->pdi.traceDepth = 0;
    pd->pdi.traceMax = 0;
    pd->pdi.focusWidget = NULL;
    pd->pdi.activatingKey = 0;
    pd->pdi.keyboard.grabType = IswNoServerGrab;
    pd->pdi.pointer.grabType = IswNoServerGrab;
    /* Windowless dispatch state: the per-display record is malloc'd (IswNew),
       not zeroed, so these must be initialised — otherwise the first motion
       event reads a garbage pointerWidget and crashes. */
    pd->pdi.pointerWidget = NULL;
    pd->pdi.windowlessButtonGrab = NULL;
    pd->pdi.buttonsDown = 0;
    pd->pdi.xdndDragActive = False;

    _IswAllocWWTable(pd);
    pd->per_screen_db = (IswDatabaseHandle *) __XtCalloc(
        (Cardinal) ops->display->screen_count(dpy),
        (Cardinal) sizeof(IswDatabaseHandle ));
    pd->cmd_db = (IswDatabaseHandle ) NULL;
    pd->server_db = (IswDatabaseHandle ) NULL;
    pd->dispatcher_list = NULL;
    pd->ext_select_list = NULL;
    pd->ext_select_count = 0;
    pd->hook_object = NULL;
#if 0
    /* NOTE: DefaultScreenOfDisplay(dpy) must NOT be used with xcb_connection_t*.
     * Use _IswGetDefaultScreen(dpy) instead. See _IswGetDefaultScreen() for details. */
    pd->hook_object = _IswCreate("hooks", "Hooks", hookObjectClass,
                                (Widget) NULL,
                                _IswGetDefaultScreen(dpy),
                                (ArgList) NULL, 0, (IswTypedArgList) NULL, 0,
                                (ConstraintWidgetClass) NULL);
#endif

#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
    {
        pd->rcm_init = _IswPlatformInternAtomOp((IswDisplay) dpy, RCM_INIT, False);
        pd->rcm_data = _IswPlatformInternAtomOp((IswDisplay) dpy, RCM_DATA, False);
    }
#endif

    return pd;
}

#define THIS_FUNC "IswOpenDisplay"
IswDisplay
IswOpenDisplay(IswAppContext app,
              _Xconst _IswString displayName,
              _Xconst _IswString applName,
              _Xconst _IswString className,
              XrmOptionDescRec *urlist,
              Cardinal num_urs,
              int *argc,
              _IswString *argv)
{
    IswDisplay dpy;
    int defaultScreen = 0;
    IswDatabaseHandle db = NULL;
    String language = NULL;

    /* Select the backend as the first act of init — before any connection
     * exists — so connection setup (open/screen probing/close) goes through
     * the vtable rather than calling xcb_connect directly here. */
    const IswPlatformOps *ops = _IswPlatformSelectBackend();

    LOCK_APP(app);
    LOCK_PROCESS;

    /* parse the command line for name, display, and/or language */
    db = _IswPreparseCommandLine(urlist, num_urs, *argc, argv,
                                (String *) &applName,
                                (String *) (displayName ? NULL : &displayName),
                                (app->process->globalLangProcRec.proc ?
                                 &language : NULL));
    UNLOCK_PROCESS;
    dpy = ops->display->open(displayName, &defaultScreen);
    if (dpy != NULL && !ops->display->has_error(dpy)) {
        int numScr = ops->display->screen_count(dpy);
        if (numScr <= 0) {
            IswErrorMsg("nullDisplay",
                       THIS_FUNC, IswCIswToolkitError,
                       THIS_FUNC " requires a non-NULL display",
                       NULL, NULL);
        }
        if (defaultScreen < 0 || defaultScreen >= numScr) {
            IswWarningMsg("nullDisplay",
                         THIS_FUNC, IswCIswToolkitError,
                         THIS_FUNC " default screen is invalid (ignoring)",
                         NULL, NULL);
            defaultScreen = 0;
        }
    } else {
        //#TODO implement retry logic rather than just exiting
        //we particularly want to consider than an X client can theoretically reconnect, potentially to a different display server entirely
        printf("Display connection error \n");
        exit(-1);
    }

    if (!applName && !(applName = getenv("RESOURCE_NAME"))) {
        if (*argc > 0 && argv[0] && *argv[0]) {
#ifdef WIN32
            char *ptr = strrchr(argv[0], '\\');
#else
            char *ptr = strrchr(argv[0], '/');
#endif

            if (ptr)
                applName = ++ptr;
            else
                applName = argv[0];
        }
        else
            applName = "main";
    }

    if (dpy) {
        IswPerDisplay pd;

        pd = InitPerDisplay(dpy, defaultScreen, app, applName, className, ops);
        pd->language = language;
        _IswDisplayInitialize(dpy, pd, applName, num_urs, argc, argv);
    }
    else {
        int len;

        /* XDisplayName: return displayName if set, else $DISPLAY env var */
        if (!displayName) displayName = getenv("DISPLAY");
        if (!displayName) displayName = "";
        len = (int) strlen(displayName);
        app->display_name_tried = (_IswString) __XtMalloc((Cardinal) (len + 1));
        strncpy((char *) app->display_name_tried, displayName,
                (size_t) (len + 1));
        app->display_name_tried[len] = '\0';
    }
    if (db)
        _IswPlatformResourceFree(db);
    UNLOCK_APP(app);
    return dpy;
}

IswDisplay
_IswAppInit(IswAppContext *app_context_return,
           String application_class,
           XrmOptionDescRec *options,
           Cardinal num_options,
           int *argc_in_out,
           _IswString **argv_in_out,
           String *fallback_resources)
{
    _IswString *saved_argv;
    int i;
    IswDisplay dpy;

    /*
     * Save away argv and argc so we can set the properties later
     */
    saved_argv = IswMallocArray((Cardinal) *argc_in_out + 1,
                               (Cardinal) sizeof(_IswString));

    for (i = 0; i < *argc_in_out; i++)
        saved_argv[i] = (*argv_in_out)[i];
    saved_argv[i] = NULL;       /* NULL terminate that sucker. */

    *app_context_return = IswCreateApplicationContext();

    LOCK_APP((*app_context_return));
    if (fallback_resources)     /* save a procedure call */
        IswAppSetFallbackResources(*app_context_return, fallback_resources);

    dpy = IswOpenDisplay(*app_context_return, NULL, NULL,
                        application_class,
                        options, num_options, argc_in_out, *argv_in_out);

    if (!dpy) {
        String param = (*app_context_return)->display_name_tried;
        Cardinal param_count = 1;

        IswErrorMsg("invalidDisplay", "xtInitialize", IswCIswToolkitError,
                   "Can't open display: %s", &param, &param_count);
        IswFree((char *) (*app_context_return)->display_name_tried);
    }
    *argv_in_out = saved_argv;
    UNLOCK_APP((*app_context_return));
    return dpy;
}

void
IswDisplayInitialize(IswAppContext app,
                    IswDisplay dpy,
                    _Xconst _IswString name,
                    _Xconst _IswString classname,
                    //XrmOptionDescRec *urlist,
                    Cardinal num_urs,
                    int *argc,
                    _IswString *argv)
{
    IswPerDisplay pd;
    //XrmDatabase db = NULL;

    LOCK_APP(app);
    /* IswDisplayInitialize doesn't receive a screen number; default to 0.
     * If the caller needs a specific screen, they should use IswOpenDisplay
     * which captures the screen number from xcb_connect(). */
    pd = InitPerDisplay(dpy, 0, app, name, classname,
                        _IswPlatformSelectBackend());
    LOCK_PROCESS;
    //if (app->process->globalLangProcRec.proc)
    //    /* pre-parse the command line for the language resource */
    //    db = _IswPreparseCommandLine(urlist, num_urs, *argc, argv, NULL, NULL,
    //                                &pd->language);
    UNLOCK_PROCESS;
    _IswDisplayInitialize(dpy, pd, name, num_urs, argc, argv);
    //if (db)
    //    XrmDestroyDatabase(db);
    UNLOCK_APP(app);
}

IswAppContext
IswCreateApplicationContext(void)
{
    IswAppContext app = IswNew(IswAppStruct);

#ifdef XTHREADS
    app->lock_info = NULL;
    app->lock = NULL;
    app->unlock = NULL;
    app->yield_lock = NULL;
    app->restore_lock = NULL;
    app->free_lock = NULL;
#endif
    INIT_APP_LOCK(app);
    LOCK_APP(app);
    LOCK_PROCESS;
    app->process = _IswGetProcessContext();
    app->next = app->process->appContextList;
    app->process->appContextList = app;
    app->langProcRec.proc = app->process->globalLangProcRec.proc;
    app->langProcRec.closure = app->process->globalLangProcRec.closure;
    app->destroy_callbacks = NULL;
    app->list = NULL;
    app->count = app->max = app->last = 0;
    app->timerQueue = NULL;
    app->workQueue = NULL;
    app->signalQueue = NULL;
    app->input_list = NULL;
    app->outstandingQueue = NULL;
    app->event_front = NULL;
    app->event_back = NULL;
    //app->errorDB = NULL;
    _IswSetDefaultErrorHandlers(&app->errorMsgHandler,
                               &app->warningMsgHandler, &app->errorHandler,
                               &app->warningHandler);
    app->action_table = NULL;
    _IswSetDefaultSelectionTimeout(&app->selectionTimeout);
    _IswSetDefaultConverterTable(&app->converterTable);
    app->sync = app->being_destroyed = app->error_inited = FALSE;
    app->in_phase2_destroy = NULL;
#ifndef USE_POLL
    FD_ZERO(&app->fds.rmask);
    FD_ZERO(&app->fds.wmask);
    FD_ZERO(&app->fds.emask);
#endif
    app->fds.nfds = 0;
    app->input_count = app->input_max = 0;
    _IswHeapInit(&app->heap);
    app->fallback_resources = NULL;
    _IswPopupInitialize(app);
    app->action_hook_list = NULL;
    app->block_hook_list = NULL;
    app->destroy_list_size = app->destroy_count = app->dispatch_level = 0;
    app->destroy_list = NULL;
#ifndef NO_IDENTIFY_WINDOWS
    app->identify_windows = False;
#endif
    app->free_bindings = NULL;
    app->display_name_tried = NULL;
    app->dpy_destroy_count = 0;
    app->dpy_destroy_list = NULL;
    app->exit_flag = FALSE;
    app->rebuild_fdlist = TRUE;
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
    return app;
}

void
IswAppSetExitFlag(IswAppContext app)
{
    LOCK_APP(app);
    app->exit_flag = TRUE;
    UNLOCK_APP(app);
}

Boolean
IswAppGetExitFlag(IswAppContext app)
{
    Boolean retval;

    LOCK_APP(app);
    retval = app->exit_flag;
    UNLOCK_APP(app);
    return retval;
}

static void
DestroyAppContext(IswAppContext app)
{
    IswAppContext *prev_app;

    prev_app = &app->process->appContextList;
    while (app->count > 0)
        IswCloseDisplay((IswDisplay) app->list[app->count - 1]);
    if (app->list != NULL)
        IswFree((char *) app->list);
    _IswFreeConverterTable(app->converterTable);
    _IswCacheFlushTag(app, (IswPointer) &app->heap);
    _IswFreeActions(app->action_table);
    if (app->destroy_callbacks != NULL) {
        IswCallCallbackList((Widget) NULL,
                           (IswCallbackList) app->destroy_callbacks,
                           (IswPointer) app);
        _IswRemoveAllCallbacks(&app->destroy_callbacks);
    }
    while (app->timerQueue)
        IswRemoveTimeOut((IswIntervalId) app->timerQueue);
    while (app->workQueue)
        IswRemoveWorkProc((IswWorkProcId) app->workQueue);
    while (app->signalQueue)
        IswRemoveSignal((IswSignalId) app->signalQueue);
    if (app->input_list)
        _IswRemoveAllInputs(app);
    IswFree((char *) app->destroy_list);
    _IswHeapFree(&app->heap);
    while (*prev_app != app)
        prev_app = &(*prev_app)->next;
    *prev_app = app->next;
    if (app->process->defaultAppContext == app)
        app->process->defaultAppContext = NULL;
    if (app->free_bindings)
        _IswDoFreeBindings(app);
    FREE_APP_LOCK(app);
    IswFree((char *) app);
}

static IswAppContext *appDestroyList = NULL;
int _IswAppDestroyCount = 0;

void
IswDestroyApplicationContext(IswAppContext app)
{
    LOCK_APP(app);
    if (app->being_destroyed) {
        UNLOCK_APP(app);
        return;
    }

    if (_IswSafeToDestroy(app)) {
        LOCK_PROCESS;
        DestroyAppContext(app);
        UNLOCK_PROCESS;
    }
    else {
        app->being_destroyed = TRUE;
        LOCK_PROCESS;
        _IswAppDestroyCount++;
        appDestroyList = IswReallocArray(appDestroyList,
                                        (Cardinal) _IswAppDestroyCount,
                                        (Cardinal) sizeof(IswAppContext));
        appDestroyList[_IswAppDestroyCount - 1] = app;
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
    }
}

void
_IswDestroyAppContexts(void)
{
    int i, ii;
    IswAppContext apps[8];
    IswAppContext *pApps;

    pApps =
        IswStackAlloc(sizeof(IswAppContext) * (size_t) _IswAppDestroyCount, apps);

    for (i = ii = 0; i < _IswAppDestroyCount; i++) {
        if (_IswSafeToDestroy(appDestroyList[i]))
            DestroyAppContext(appDestroyList[i]);
        else
            pApps[ii++] = appDestroyList[i];
    }
    _IswAppDestroyCount = ii;
    if (_IswAppDestroyCount == 0) {
        IswFree((char *) appDestroyList);
        appDestroyList = NULL;
    }
    else {
        for (i = 0; i < ii; i++)
            appDestroyList[i] = pApps[i];
    }
    IswStackFree((IswPointer) pApps, apps);
}

XrmDatabase
IswDatabase(IswDisplay dpy_opaque)
{
    xcb_connection_t *dpy = _IswXcbConn(dpy_opaque);
    IswDatabaseHandle db;
    xcb_screen_t *screen;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);

    screen = _IswXcbScreen(_IswDefaultScreenOf(dpy_opaque));

    /* Return the merged database for the default screen */
    db = IswScreenDatabase((IswScreen) screen);
    
    UNLOCK_APP(app);
    return db;
}

PerDisplayTablePtr _IswperDisplayList = NULL;

IswPerDisplay
_IswSortPerDisplayList(IswDisplay dpy)
{
    register PerDisplayTablePtr pd, opd = NULL;
    IswPerDisplay result = NULL;

    LOCK_PROCESS;
    for (pd = _IswperDisplayList; pd != NULL && pd->dpy != dpy; pd = pd->next) {
        opd = pd;
    }

    if (pd == NULL) {
        IswErrorMsg(IswNnoPerDisplay, "getPerDisplay", IswCIswToolkitError,
                   "Couldn't find per display information", NULL, NULL);
    }
    else {
        if (pd != _IswperDisplayList) {  /* move it to the front */
            /* opd points to the previous one... */

            opd->next = pd->next;
            pd->next = _IswperDisplayList;
            _IswperDisplayList = pd;
        }
        result = &(pd->perDpy);
    }
    UNLOCK_PROCESS;
    return result;
}

IswAppContext
IswDisplayToApplicationContext(IswDisplay dpy)
{
    IswAppContext retval;

    retval = _IswGetPerDisplay(dpy)->appContext;
    return retval;
}

static void
CloseDisplay(IswDisplay dpy)
{
    register IswPerDisplay xtpd = NULL;
    register PerDisplayTablePtr pd, opd = NULL;
    /* The display's own backend ops, captured from the per-display record while
     * it is still valid; used to close the connection through the vtable after
     * the record is freed. */
    const IswPlatformOps *ops = _IswPlatformSelectBackend();
    //XrmDatabase db;

    IswDestroyWidget(IswHooksOfDisplay((IswDisplay) dpy));

    LOCK_PROCESS;
    for (pd = _IswperDisplayList; pd != NULL && pd->dpy != dpy; pd = pd->next) {
        opd = pd;
    }

    if (pd == NULL) {
        IswErrorMsg(IswNnoPerDisplay, "closeDisplay", IswCIswToolkitError,
                   "Couldn't find per display information", NULL, NULL);
    }
    else {

        if (pd == _IswperDisplayList)
            _IswperDisplayList = pd->next;
        else
            opd->next = pd->next;

        xtpd = &(pd->perDpy);
    }

    if (xtpd != NULL) {
        int i;

        if (xtpd->ops != NULL)
            ops = xtpd->ops;

        if (xtpd->destroy_callbacks != NULL) {
            IswCallCallbackList((Widget) NULL,
                               (IswCallbackList) xtpd->destroy_callbacks,
                               (IswPointer) xtpd);
            _IswRemoveAllCallbacks(&xtpd->destroy_callbacks);
        }
        if (xtpd->mapping_callbacks != NULL)
            _IswRemoveAllCallbacks(&xtpd->mapping_callbacks);
        /* Flush the converter cache and GC list before removing the
         * display from the app context.  FreePixel (called from the
         * cache flush) uses _IswConnectionOfScreen() which walks
         * app->list[] — the display must still be in that list. */
        _IswCacheFlushTag(xtpd->appContext, (IswPointer) &xtpd->heap);
        IswDeleteFromAppContext(dpy, xtpd->appContext);
        //if (xtpd->keysyms)
        //    xcb_key_symbols_free(xtpd->keysyms); //causes linker error even with xcb-xkb linked
            //IswFree((char *) xtpd->keysyms);
        IswFree((char *) xtpd->modKeysyms);
        IswFree((char *) xtpd->modsToKeysyms);
        xtpd->keysyms_per_keycode = 0;
        xtpd->being_destroyed = FALSE;
        xtpd->keysyms = NULL;
        xtpd->modKeysyms = NULL;
        xtpd->modsToKeysyms = NULL;
        IswFree((char *) xtpd->pdi.trace);
        _IswHeapFree(&xtpd->heap);
        _IswFreeWWTable(xtpd);
        if (xtpd->per_screen_db) {
            int nscreens = ops->display->screen_count(dpy);
            for (i = 0; i < nscreens; i++) {
                if (xtpd->per_screen_db[i])
                    _IswPlatformResourceFree(xtpd->per_screen_db[i]);
            }
            IswFree((char *) xtpd->per_screen_db);
            xtpd->per_screen_db = NULL;
        }
        if (xtpd->cmd_db) {
            _IswPlatformResourceFree(xtpd->cmd_db);
            xtpd->cmd_db = NULL;
        }
        if (xtpd->server_db) {
            _IswPlatformResourceFree(xtpd->server_db);
            xtpd->server_db = NULL;
        }
        IswFree((_IswString) xtpd->language);
        if (xtpd->dispatcher_list != NULL)
            IswFree((char *) xtpd->dispatcher_list);
        if (xtpd->ext_select_list != NULL)
            IswFree((char *) xtpd->ext_select_list);
    }
    IswFree((char *) pd);
    /* No need to clear database on connection - we manage our own databases */
    /* Close the connection through the vtable (flush + disconnect). */
    ops->display->close((IswDisplay) dpy);
    UNLOCK_PROCESS;
}

void
IswCloseDisplay(IswDisplay dpy)
{
    
    IswPerDisplay pd;
    IswAppContext app = IswDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    pd = _IswGetPerDisplay(dpy);
    if (pd->being_destroyed) {
        UNLOCK_APP(app);
        return;
    }

    if (_IswSafeToDestroy(app))
        CloseDisplay(dpy);
    else {
        pd->being_destroyed = TRUE;
        app->dpy_destroy_count++;
        app->dpy_destroy_list = IswReallocArray(app->dpy_destroy_list,
                                               (Cardinal) app->dpy_destroy_count,
                                               (Cardinal) sizeof(IswDisplay));
        app->dpy_destroy_list[app->dpy_destroy_count - 1] = dpy;
    }
    UNLOCK_APP(app);
}

void
_IswCloseDisplays(IswAppContext app)
{
    int i;

    LOCK_APP(app);
    for (i = 0; i < app->dpy_destroy_count; i++) {
        CloseDisplay(app->dpy_destroy_list[i]);
    }
    app->dpy_destroy_count = 0;
    IswFree((char *) app->dpy_destroy_list);
    app->dpy_destroy_list = NULL;
    UNLOCK_APP(app);
}

IswAppContext
IswWidgetToApplicationContext(Widget w)
{
    IswAppContext retval;

    retval = _IswGetPerDisplay(IswDisplayOfObject(w))->appContext;
    return retval;
}

void
IswGetApplicationNameAndClass(IswDisplay dpy,
                             String *name_return,
                             String *class_return)
{
    IswPerDisplay pd;

    pd = _IswGetPerDisplay(dpy);
    *name_return = pd->name;
    *class_return = pd->class;
}

IswPerDisplay
_IswGetPerDisplay(IswDisplay display)
{
    IswPerDisplay retval;

    LOCK_PROCESS;
    retval = ((_IswperDisplayList != NULL && _IswperDisplayList->dpy == display)
              ? &_IswperDisplayList->perDpy : _IswSortPerDisplayList(display));

    UNLOCK_PROCESS;
    return retval;
}

IswPerDisplayInputRec *
_IswGetPerDisplayInput(IswDisplay dpy)
{
    IswPerDisplayInputRec *retval;

    LOCK_PROCESS;
    retval = ((_IswperDisplayList != NULL && _IswperDisplayList->dpy == dpy)
              ? &_IswperDisplayList->perDpy.pdi
              : &_IswSortPerDisplayList(dpy)->pdi);
    UNLOCK_PROCESS;
    return retval;
}

void
IswGetDisplays(IswAppContext app_context,
              IswDisplay **dpy_return,
              Cardinal *num_dpy_return)
{
    int ii;

    LOCK_APP(app_context);
    *num_dpy_return = (Cardinal) app_context->count;
    *dpy_return = IswMallocArray((Cardinal) app_context->count,
                                (Cardinal) sizeof(IswDisplay));
    for (ii = 0; ii < app_context->count; ii++)
        (*dpy_return)[ii] = (IswDisplay) app_context->list[ii];
    UNLOCK_APP(app_context);
}

IswDisplay
_IswConnectionOfScreen(IswScreen screen)
{
    IswAppContext app;
    const IswPlatformOps *ops = _IswPlatformSelectBackend();

    LOCK_PROCESS;
    /* Walk the process-level display list */
    app = _IswDefaultAppContext();
    if (app != NULL) {
        int i;
        for (i = 0; i < app->count; i++) {
            IswDisplay dpy = app->list[i];
            /* Check each screen of this connection */
            int nscreens = ops->display->screen_count(dpy);
            int s;
            for (s = 0; s < nscreens; s++) {
                if (ops->display->screen(dpy, s) == screen) {
                    UNLOCK_PROCESS;
                    return dpy;
                }
            }
        }
    }
    UNLOCK_PROCESS;
    return NULL;
}
