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
#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
#include "ResConfigP.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h>


#ifdef XTHREADS
void (*_XtProcessLock) (void) = NULL;
void (*_XtProcessUnlock) (void) = NULL;
void (*_XtInitAppLock) (XtAppContext) = NULL;
#endif

static _Xconst _XtString XtNnoPerDisplay = "noPerDisplay";

ProcessContext
_XtGetProcessContext(void)
{
    static ProcessContextRec processContextRec = {
        (XtAppContext) NULL,
        (XtAppContext) NULL,
        (ConverterTable) NULL,
        {(XtLanguageProc) NULL, (XtPointer) NULL}
    };

    return &processContextRec;
}

XtAppContext
_XtDefaultAppContext(void)
{
    ProcessContext process = _XtGetProcessContext();
    XtAppContext app;

    LOCK_PROCESS;
    if (process->defaultAppContext == NULL) {
        process->defaultAppContext = XtCreateApplicationContext();
    }
    app = process->defaultAppContext;
    UNLOCK_PROCESS;
    return app;
}

static void
AddToAppContext(xcb_connection_t *d, XtAppContext app)
{
#define DISPLAYS_TO_ADD 4

    if (app->count >= app->max) {
        app->max = (short) (app->max + DISPLAYS_TO_ADD);
        app->list = XtReallocArray(app->list,
                                   (Cardinal) app->max,
                                   (Cardinal) sizeof(xcb_connection_t *));
    }

    app->list[app->count++] = d;
    app->rebuild_fdlist = TRUE;
#ifdef USE_POLL
    app->fds.nfds++;
#else
    if (ConnectionNumber(d) + 1 > app->fds.nfds) {
        app->fds.nfds = ConnectionNumber(d) + 1;
    }
#endif
#undef DISPLAYS_TO_ADD
}

static void
XtDeleteFromAppContext(const xcb_connection_t *d, register XtAppContext app)
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
    if ((ConnectionNumber(d) + 1) == app->fds.nfds)
        app->fds.nfds--;
    else                        /* Unnecessary, just to be fool-proof */
        FD_CLR(ConnectionNumber(d), &app->fds.rmask);
#endif
}

static XtPerDisplay
NewPerDisplay(xcb_connection_t *dpy)
{
    PerDisplayTablePtr pd;

    pd = XtNew(PerDisplayTable);

    LOCK_PROCESS;
    pd->dpy = dpy;
    pd->next = _XtperDisplayList;
    _XtperDisplayList = pd;
    UNLOCK_PROCESS;
    return &(pd->perDpy);
}

static XtPerDisplay
InitPerDisplay(xcb_connection_t *dpy,
               int defaultScreen,
               XtAppContext app,
               _Xconst char *name,
               _Xconst char *classname)
{
    XtPerDisplay pd;

    AddToAppContext(dpy, app);

    pd = NewPerDisplay(dpy);
    _XtHeapInit(&pd->heap);
    pd->destroy_callbacks = NULL;
    /* Store the default screen number from xcb_connect() so that
     * _XtGetDefaultScreen() can later retrieve the correct xcb_screen_t*.
     * NOTE: Do NOT use the Xlib DefaultScreenOfDisplay() macro here — it
     * reads ((Display*)dpy)->default_screen which is meaningless for an
     * xcb_connection_t* and returns NULL/garbage. */
    pd->defaultScreen = defaultScreen;
    //pd->region = XCreateRegion();
    pd->region = xcb_generate_id(dpy);
    
    // Create the empty region
    xcb_void_cookie_t cookie = xcb_xfixes_create_region(dpy, pd->region, 0, 0);
    
    // Check for errors
    xcb_generic_error_t *error = xcb_request_check(dpy, cookie);
    if (error) {
        fprintf(stderr, "Error creating region: %d\n", error->error_code);
        free(error);
    }

    // Initialize null_region (empty region for clearing operations)
    pd->null_region = xcb_generate_id(dpy);
    cookie = xcb_xfixes_create_region(dpy, pd->null_region, 0, 0);
    error = xcb_request_check(dpy, cookie);
    if (error) {
        fprintf(stderr, "Error creating null_region: %d\n", error->error_code);
        free(error);
    }

    pd->case_cvt = NULL;
    pd->defaultKeycodeTranslator = XtTranslateKey;
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
    _XtAllocTMContext(pd);
    pd->mapping_callbacks = NULL;

    pd->PerWidgetContext = NULL;    /* uthash head must be NULL before first use */
    pd->pdi.grabList = NULL;
    pd->pdi.trace = NULL;
    pd->pdi.traceDepth = 0;
    pd->pdi.traceMax = 0;
    pd->pdi.focusWidget = NULL;
    pd->pdi.activatingKey = 0;
    pd->pdi.keyboard.grabType = XtNoServerGrab;
    pd->pdi.pointer.grabType = XtNoServerGrab;

    _XtAllocWWTable(pd);
    pd->per_screen_db = (xcb_xrm_database_t **) __XtCalloc(
        (Cardinal) xcb_setup_roots_length(xcb_get_setup(dpy)),
        (Cardinal) sizeof(xcb_xrm_database_t *));
    pd->cmd_db = (xcb_xrm_database_t *) NULL;
    pd->server_db = (xcb_xrm_database_t *) NULL;
    pd->dispatcher_list = NULL;
    pd->ext_select_list = NULL;
    pd->ext_select_count = 0;
    pd->hook_object = NULL;
#if 0
    /* NOTE: DefaultScreenOfDisplay(dpy) must NOT be used with xcb_connection_t*.
     * Use _XtGetDefaultScreen(dpy) instead. See _XtGetDefaultScreen() for details. */
    pd->hook_object = _XtCreate("hooks", "Hooks", hookObjectClass,
                                (Widget) NULL,
                                _XtGetDefaultScreen(dpy),
                                (ArgList) NULL, 0, (XtTypedArgList) NULL, 0,
                                (ConstraintWidgetClass) NULL);
#endif

//#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
//    pd->rcm_init = XInternAtom(dpy, RCM_INIT, 0);
//    pd->rcm_data = XInternAtom(dpy, RCM_DATA, 0);
//#endif

    return pd;
}

#define THIS_FUNC "XtOpenDisplay"
xcb_connection_t *
XtOpenDisplay(XtAppContext app,
              _Xconst _XtString displayName,
              _Xconst _XtString applName,
              _Xconst _XtString className,
              XrmOptionDescRec *urlist,
              Cardinal num_urs,
              int *argc,
              _XtString *argv)
{
    xcb_connection_t *d;
    int defaultScreen = 0;
    xcb_xrm_database_t *db = NULL;
    String language = NULL;

    LOCK_APP(app);
    LOCK_PROCESS;

    /* parse the command line for name, display, and/or language */
    db = _XtPreparseCommandLine(urlist, num_urs, *argc, argv,
                                (String *) &applName,
                                (String *) (displayName ? NULL : &displayName),
                                (app->process->globalLangProcRec.proc ?
                                 &language : NULL));
    UNLOCK_PROCESS;
    d = xcb_connect(displayName, &defaultScreen); //XOpenDisplay(displayName);
    if (xcb_connection_has_error(d) == 0) {
        int numScr = xcb_setup_roots_length(xcb_get_setup(d));
        if (numScr <= 0) {
            XtErrorMsg("nullDisplay",
                       THIS_FUNC, XtCXtToolkitError,
                       THIS_FUNC " requires a non-NULL display",
                       NULL, NULL);
        }
        if (defaultScreen < 0 || defaultScreen >= numScr) {
            XtWarningMsg("nullDisplay",
                         THIS_FUNC, XtCXtToolkitError,
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

    if (d) {
        XtPerDisplay pd;

        pd = InitPerDisplay(d, defaultScreen, app, applName, className);
        pd->language = language;
        _XtDisplayInitialize(d, pd, applName, num_urs, argc, argv);
    }
    else {
        int len;

        /* XDisplayName: return displayName if set, else $DISPLAY env var */
        if (!displayName) displayName = getenv("DISPLAY");
        if (!displayName) displayName = "";
        len = (int) strlen(displayName);
        app->display_name_tried = (_XtString) __XtMalloc((Cardinal) (len + 1));
        strncpy((char *) app->display_name_tried, displayName,
                (size_t) (len + 1));
        app->display_name_tried[len] = '\0';
    }
    if (db)
        xcb_xrm_database_free(db);
    UNLOCK_APP(app);
    return d;
}

/*
 * _XtGetDefaultScreen - XCB replacement for the Xlib DefaultScreenOfDisplay()
 * macro.
 *
 * DefaultScreenOfDisplay() is an Xlib macro defined as:
 *   ScreenOfDisplay(dpy, DefaultScreen(dpy))
 * which reads ((Display*)dpy)->default_screen — meaningless for an
 * xcb_connection_t* and returns NULL/garbage, causing a SIGSEGV when the
 * result is dereferenced.
 *
 * This function retrieves the default screen number stored in XtPerDisplay
 * (set from the screen-number output of xcb_connect()) and walks
 * xcb_setup_roots_iterator() to return the correct xcb_screen_t*.
 */
xcb_screen_t *
_XtGetDefaultScreen(xcb_connection_t *dpy)
{
    XtPerDisplay pd = _XtGetPerDisplay(dpy);
    int screen_num = pd ? pd->defaultScreen : 0;
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(dpy));
    for (int i = 0; i < screen_num; i++) {
        if (iter.rem == 0)
            break;
        xcb_screen_next(&iter);
    }
    return iter.data;
}

xcb_connection_t *
_XtAppInit(XtAppContext *app_context_return,
           String application_class,
           XrmOptionDescRec *options,
           Cardinal num_options,
           int *argc_in_out,
           _XtString **argv_in_out,
           String *fallback_resources)
{
    _XtString *saved_argv;
    int i;
    xcb_connection_t *dpy;

    /*
     * Save away argv and argc so we can set the properties later
     */
    saved_argv = XtMallocArray((Cardinal) *argc_in_out + 1,
                               (Cardinal) sizeof(_XtString));

    for (i = 0; i < *argc_in_out; i++)
        saved_argv[i] = (*argv_in_out)[i];
    saved_argv[i] = NULL;       /* NULL terminate that sucker. */

    *app_context_return = XtCreateApplicationContext();

    LOCK_APP((*app_context_return));
    if (fallback_resources)     /* save a procedure call */
        XtAppSetFallbackResources(*app_context_return, fallback_resources);

    dpy = XtOpenDisplay(*app_context_return, NULL, NULL,
                        application_class,
                        options, num_options, argc_in_out, *argv_in_out);

    if (!dpy) {
        String param = (*app_context_return)->display_name_tried;
        Cardinal param_count = 1;

        XtErrorMsg("invalidDisplay", "xtInitialize", XtCXtToolkitError,
                   "Can't open display: %s", &param, &param_count);
        XtFree((char *) (*app_context_return)->display_name_tried);
    }
    *argv_in_out = saved_argv;
    UNLOCK_APP((*app_context_return));
    return dpy;
}

void
XtDisplayInitialize(XtAppContext app,
                    xcb_connection_t *dpy,
                    _Xconst _XtString name,
                    _Xconst _XtString classname,
                    //XrmOptionDescRec *urlist,
                    Cardinal num_urs,
                    int *argc,
                    _XtString *argv)
{
    XtPerDisplay pd;
    //XrmDatabase db = NULL;

    LOCK_APP(app);
    /* XtDisplayInitialize doesn't receive a screen number; default to 0.
     * If the caller needs a specific screen, they should use XtOpenDisplay
     * which captures the screen number from xcb_connect(). */
    pd = InitPerDisplay(dpy, 0, app, name, classname);
    LOCK_PROCESS;
    //if (app->process->globalLangProcRec.proc)
    //    /* pre-parse the command line for the language resource */
    //    db = _XtPreparseCommandLine(urlist, num_urs, *argc, argv, NULL, NULL,
    //                                &pd->language);
    UNLOCK_PROCESS;
    _XtDisplayInitialize(dpy, pd, name, num_urs, argc, argv);
    //if (db)
    //    XrmDestroyDatabase(db);
    UNLOCK_APP(app);
}

XtAppContext
XtCreateApplicationContext(void)
{
    XtAppContext app = XtNew(XtAppStruct);

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
    app->process = _XtGetProcessContext();
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
    //app->errorDB = NULL;
    _XtSetDefaultErrorHandlers(&app->errorMsgHandler,
                               &app->warningMsgHandler, &app->errorHandler,
                               &app->warningHandler);
    app->action_table = NULL;
    _XtSetDefaultSelectionTimeout(&app->selectionTimeout);
    _XtSetDefaultConverterTable(&app->converterTable);
    app->sync = app->being_destroyed = app->error_inited = FALSE;
    app->in_phase2_destroy = NULL;
#ifndef USE_POLL
    FD_ZERO(&app->fds.rmask);
    FD_ZERO(&app->fds.wmask);
    FD_ZERO(&app->fds.emask);
#endif
    app->fds.nfds = 0;
    app->input_count = app->input_max = 0;
    _XtHeapInit(&app->heap);
    app->fallback_resources = NULL;
    _XtPopupInitialize(app);
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
XtAppSetExitFlag(XtAppContext app)
{
    LOCK_APP(app);
    app->exit_flag = TRUE;
    UNLOCK_APP(app);
}

Boolean
XtAppGetExitFlag(XtAppContext app)
{
    Boolean retval;

    LOCK_APP(app);
    retval = app->exit_flag;
    UNLOCK_APP(app);
    return retval;
}

static void
DestroyAppContext(XtAppContext app)
{
    XtAppContext *prev_app;

    prev_app = &app->process->appContextList;
    while (app->count > 0)
        XtCloseDisplay(app->list[app->count - 1]);
    if (app->list != NULL)
        XtFree((char *) app->list);
    _XtFreeConverterTable(app->converterTable);
    _XtCacheFlushTag(app, (XtPointer) &app->heap);
    _XtFreeActions(app->action_table);
    if (app->destroy_callbacks != NULL) {
        XtCallCallbackList((Widget) NULL,
                           (XtCallbackList) app->destroy_callbacks,
                           (XtPointer) app);
        _XtRemoveAllCallbacks(&app->destroy_callbacks);
    }
    while (app->timerQueue)
        XtRemoveTimeOut((XtIntervalId) app->timerQueue);
    while (app->workQueue)
        XtRemoveWorkProc((XtWorkProcId) app->workQueue);
    while (app->signalQueue)
        XtRemoveSignal((XtSignalId) app->signalQueue);
    if (app->input_list)
        _XtRemoveAllInputs(app);
    XtFree((char *) app->destroy_list);
    _XtHeapFree(&app->heap);
    while (*prev_app != app)
        prev_app = &(*prev_app)->next;
    *prev_app = app->next;
    if (app->process->defaultAppContext == app)
        app->process->defaultAppContext = NULL;
    if (app->free_bindings)
        _XtDoFreeBindings(app);
    FREE_APP_LOCK(app);
    XtFree((char *) app);
}

static XtAppContext *appDestroyList = NULL;
int _XtAppDestroyCount = 0;

void
XtDestroyApplicationContext(XtAppContext app)
{
    LOCK_APP(app);
    if (app->being_destroyed) {
        UNLOCK_APP(app);
        return;
    }

    if (_XtSafeToDestroy(app)) {
        LOCK_PROCESS;
        DestroyAppContext(app);
        UNLOCK_PROCESS;
    }
    else {
        app->being_destroyed = TRUE;
        LOCK_PROCESS;
        _XtAppDestroyCount++;
        appDestroyList = XtReallocArray(appDestroyList,
                                        (Cardinal) _XtAppDestroyCount,
                                        (Cardinal) sizeof(XtAppContext));
        appDestroyList[_XtAppDestroyCount - 1] = app;
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
    }
}

void
_XtDestroyAppContexts(void)
{
    int i, ii;
    XtAppContext apps[8];
    XtAppContext *pApps;

    pApps =
        XtStackAlloc(sizeof(XtAppContext) * (size_t) _XtAppDestroyCount, apps);

    for (i = ii = 0; i < _XtAppDestroyCount; i++) {
        if (_XtSafeToDestroy(appDestroyList[i]))
            DestroyAppContext(appDestroyList[i]);
        else
            pApps[ii++] = appDestroyList[i];
    }
    _XtAppDestroyCount = ii;
    if (_XtAppDestroyCount == 0) {
        XtFree((char *) appDestroyList);
        appDestroyList = NULL;
    }
    else {
        for (i = 0; i < ii; i++)
            appDestroyList[i] = pApps[i];
    }
    XtStackFree((XtPointer) pApps, apps);
}

XrmDatabase
XtDatabase(xcb_connection_t *dpy)
{
    xcb_xrm_database_t *db;
    xcb_screen_t *screen;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    
    screen = _XtGetDefaultScreen(dpy);
    
    /* Return the merged database for the default screen */
    db = XtScreenDatabase(screen);
    
    UNLOCK_APP(app);
    return db;
}

PerDisplayTablePtr _XtperDisplayList = NULL;

XtPerDisplay
_XtSortPerDisplayList(xcb_connection_t *dpy)
{
    register PerDisplayTablePtr pd, opd = NULL;
    XtPerDisplay result = NULL;

    LOCK_PROCESS;
    for (pd = _XtperDisplayList; pd != NULL && pd->dpy != dpy; pd = pd->next) {
        opd = pd;
    }

    if (pd == NULL) {
        XtErrorMsg(XtNnoPerDisplay, "getPerDisplay", XtCXtToolkitError,
                   "Couldn't find per display information", NULL, NULL);
    }
    else {
        if (pd != _XtperDisplayList) {  /* move it to the front */
            /* opd points to the previous one... */

            opd->next = pd->next;
            pd->next = _XtperDisplayList;
            _XtperDisplayList = pd;
        }
        result = &(pd->perDpy);
    }
    UNLOCK_PROCESS;
    return result;
}

XtAppContext
XtDisplayToApplicationContext(xcb_connection_t *dpy)
{
    XtAppContext retval;

    retval = _XtGetPerDisplay(dpy)->appContext;
    return retval;
}

static void
CloseDisplay(xcb_connection_t *dpy)
{
    register XtPerDisplay xtpd = NULL;
    register PerDisplayTablePtr pd, opd = NULL;
    //XrmDatabase db;

    XtDestroyWidget(XtHooksOfDisplay(dpy));

    LOCK_PROCESS;
    for (pd = _XtperDisplayList; pd != NULL && pd->dpy != dpy; pd = pd->next) {
        opd = pd;
    }

    if (pd == NULL) {
        XtErrorMsg(XtNnoPerDisplay, "closeDisplay", XtCXtToolkitError,
                   "Couldn't find per display information", NULL, NULL);
    }
    else {

        if (pd == _XtperDisplayList)
            _XtperDisplayList = pd->next;
        else
            opd->next = pd->next;

        xtpd = &(pd->perDpy);
    }

    if (xtpd != NULL) {
        int i;

        if (xtpd->destroy_callbacks != NULL) {
            XtCallCallbackList((Widget) NULL,
                               (XtCallbackList) xtpd->destroy_callbacks,
                               (XtPointer) xtpd);
            _XtRemoveAllCallbacks(&xtpd->destroy_callbacks);
        }
        if (xtpd->mapping_callbacks != NULL)
            _XtRemoveAllCallbacks(&xtpd->mapping_callbacks);
        /* Flush the converter cache and GC list before removing the
         * display from the app context.  FreePixel (called from the
         * cache flush) uses _XtConnectionOfScreen() which walks
         * app->list[] — the display must still be in that list. */
        _XtCacheFlushTag(xtpd->appContext, (XtPointer) &xtpd->heap);
        XtDeleteFromAppContext(dpy, xtpd->appContext);
        //if (xtpd->keysyms)
        //    xcb_key_symbols_free(xtpd->keysyms); //causes linker error even with xcb-xkb linked
            //XtFree((char *) xtpd->keysyms);
        XtFree((char *) xtpd->modKeysyms);
        XtFree((char *) xtpd->modsToKeysyms);
        xtpd->keysyms_per_keycode = 0;
        xtpd->being_destroyed = FALSE;
        xtpd->keysyms = NULL;
        xtpd->modKeysyms = NULL;
        xtpd->modsToKeysyms = NULL;
        //XDestroyRegion(xtpd->region);
        xcb_xfixes_destroy_region(dpy, xtpd->region);
        XtFree((char *) xtpd->pdi.trace);
        _XtHeapFree(&xtpd->heap);
        _XtFreeWWTable(xtpd);
        if (xtpd->per_screen_db) {
            int nscreens = xcb_setup_roots_length(xcb_get_setup(dpy));
            for (i = 0; i < nscreens; i++) {
                if (xtpd->per_screen_db[i])
                    xcb_xrm_database_free(xtpd->per_screen_db[i]);
            }
            XtFree((char *) xtpd->per_screen_db);
            xtpd->per_screen_db = NULL;
        }
        if (xtpd->cmd_db) {
            xcb_xrm_database_free(xtpd->cmd_db);
            xtpd->cmd_db = NULL;
        }
        if (xtpd->server_db) {
            xcb_xrm_database_free(xtpd->server_db);
            xtpd->server_db = NULL;
        }
        XtFree((_XtString) xtpd->language);
        if (xtpd->dispatcher_list != NULL)
            XtFree((char *) xtpd->dispatcher_list);
        if (xtpd->ext_select_list != NULL)
            XtFree((char *) xtpd->ext_select_list);
    }
    XtFree((char *) pd);
    /* No need to clear database on connection - we manage our own databases */
    xcb_disconnect(dpy);
    UNLOCK_PROCESS;
}

void
XtCloseDisplay(xcb_connection_t *dpy)
{
    XtPerDisplay pd;
    XtAppContext app = XtDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);
    if (pd->being_destroyed) {
        UNLOCK_APP(app);
        return;
    }

    if (_XtSafeToDestroy(app))
        CloseDisplay(dpy);
    else {
        pd->being_destroyed = TRUE;
        app->dpy_destroy_count++;
        app->dpy_destroy_list = XtReallocArray(app->dpy_destroy_list,
                                               (Cardinal) app->dpy_destroy_count,
                                               (Cardinal) sizeof(xcb_connection_t *));
        app->dpy_destroy_list[app->dpy_destroy_count - 1] = dpy;
    }
    UNLOCK_APP(app);
}

void
_XtCloseDisplays(XtAppContext app)
{
    int i;

    LOCK_APP(app);
    for (i = 0; i < app->dpy_destroy_count; i++) {
        CloseDisplay(app->dpy_destroy_list[i]);
    }
    app->dpy_destroy_count = 0;
    XtFree((char *) app->dpy_destroy_list);
    app->dpy_destroy_list = NULL;
    UNLOCK_APP(app);
}

XtAppContext
XtWidgetToApplicationContext(Widget w)
{
    XtAppContext retval;

    retval = _XtGetPerDisplay(XtDisplayOfObject(w))->appContext;
    return retval;
}

void
XtGetApplicationNameAndClass(xcb_connection_t *dpy,
                             String *name_return,
                             String *class_return)
{
    XtPerDisplay pd;

    pd = _XtGetPerDisplay(dpy);
    *name_return = pd->name;
    *class_return = pd->class;
}

XtPerDisplay
_XtGetPerDisplay(xcb_connection_t *display)
{
    XtPerDisplay retval;

    LOCK_PROCESS;
    retval = ((_XtperDisplayList != NULL && _XtperDisplayList->dpy == display)
              ? &_XtperDisplayList->perDpy : _XtSortPerDisplayList(display));

    UNLOCK_PROCESS;
    return retval;
}

XtPerDisplayInputRec *
_XtGetPerDisplayInput(xcb_connection_t *display)
{
    XtPerDisplayInputRec *retval;

    LOCK_PROCESS;
    retval = ((_XtperDisplayList != NULL && _XtperDisplayList->dpy == display)
              ? &_XtperDisplayList->perDpy.pdi
              : &_XtSortPerDisplayList(display)->pdi);
    UNLOCK_PROCESS;
    return retval;
}

void
XtGetDisplays(XtAppContext app_context,
              xcb_connection_t ***dpy_return,
              Cardinal *num_dpy_return)
{
    int ii;

    LOCK_APP(app_context);
    *num_dpy_return = (Cardinal) app_context->count;
    *dpy_return = XtMallocArray((Cardinal) app_context->count,
                                (Cardinal) sizeof(xcb_connection_t *));
    for (ii = 0; ii < app_context->count; ii++)
        (*dpy_return)[ii] = app_context->list[ii];
    UNLOCK_APP(app_context);
}

xcb_connection_t *
_XtConnectionOfScreen(xcb_screen_t *screen)
{
    XtAppContext app;

    LOCK_PROCESS;
    /* Walk the process-level display list */
    app = _XtDefaultAppContext();
    if (app != NULL) {
        int i;
        for (i = 0; i < app->count; i++) {
            xcb_connection_t *dpy = app->list[i];
            /* Check each screen of this connection */
            const xcb_setup_t *setup = xcb_get_setup(dpy);
            xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
            for (; iter.rem; xcb_screen_next(&iter)) {
                if (iter.data == screen) {
                    UNLOCK_PROCESS;
                    return dpy;
                }
            }
        }
    }
    UNLOCK_PROCESS;
    return NULL;
}
