/***********************************************************

Copyright 1987, 1988, 1994, 1998  The Open Group

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

#ifndef _IswinitialI_h
#define _IswinitialI_h

/****************************************************************
 *
 * Displays
 *
 ****************************************************************/

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif
#ifndef PATH_MAX
#ifdef WIN32
#define PATH_MAX 512
#else
#include <sys/param.h>
#endif
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef USE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xcb_keysyms.h>

#include "uthash.h"
_XFUNCPROTOBEGIN

typedef struct _TimerEventRec {
        struct timeval        te_timer_value;
	struct _TimerEventRec *te_next;
	IswTimerCallbackProc   te_proc;
	IswAppContext	      app;
	IswPointer	      te_closure;
} TimerEventRec;

typedef struct _InputEvent {
	IswInputCallbackProc   ie_proc;
	IswPointer	      ie_closure;
	struct _InputEvent    *ie_next;
	struct _InputEvent    *ie_oq;
	IswAppContext	      app;
	int		      ie_source;
	IswInputMask	      ie_condition;
} InputEvent;

typedef struct _SignalEventRec {
	IswSignalCallbackProc  se_proc;
	IswPointer	      se_closure;
	struct _SignalEventRec *se_next;
	IswAppContext	      app;
	Boolean		      se_notice;
} SignalEventRec;

typedef struct _WorkProcRec {
	IswWorkProc proc;
	IswPointer closure;
	struct _WorkProcRec *next;
	IswAppContext app;
} WorkProcRec;


typedef struct
{
#ifndef USE_POLL
  	fd_set rmask;
	fd_set wmask;
	fd_set emask;
#endif
	int	nfds;
} FdStruct;

typedef struct _LangProcRec {
    IswLanguageProc	proc;
    IswPointer		closure;
} LangProcRec;

typedef struct _ProcessContextRec {
    IswAppContext	defaultAppContext;
    IswAppContext	appContextList;
    ConverterTable	globalConverterTable;
    LangProcRec		globalLangProcRec;
} ProcessContextRec, *ProcessContext;

typedef struct {
    char*	start;
    char*	current;
    int		bytes_remaining;
} Heap;

typedef struct _DestroyRec DestroyRec;

typedef struct _IswEventQueue  IswEventQueue;
typedef struct _IswEventQueue {
    xcb_generic_event_t *event;
    xcb_connection_t *display;
    IswEventQueue *next;
} IswEventQueue;

typedef struct _IswAppStruct {
    IswAppContext next;		/* link to next app in process context */
    ProcessContext process;	/* back pointer to our process context */
    InternalCallbackList destroy_callbacks;
    xcb_connection_t **list;
    TimerEventRec *timerQueue;
    WorkProcRec *workQueue;
    InputEvent **input_list;
    InputEvent *outstandingQueue;
    SignalEventRec *signalQueue;
    IswDatabaseHandle errorDB;
    IswErrorMsgHandler errorMsgHandler, warningMsgHandler;
    IswErrorHandler errorHandler, warningHandler;
    struct _ActionListRec *action_table;
    ConverterTable converterTable;
    unsigned long selectionTimeout;
    FdStruct fds;
    short count;			/* num of assigned entries in list */
    short max;				/* allocate size of list */
    short last;
    short input_count;
    short input_max;			/* elts input_list init'd with */
    Boolean sync, being_destroyed, error_inited;
#ifndef NO_IDENTIFY_WINDOWS
    Boolean identify_windows;		/* debugging hack */
#endif
    Heap heap;
    String * fallback_resources;	/* Set by IswAppSetFallbackResources. */
    struct _ActionHookRec* action_hook_list;
    struct _BlockHookRec* block_hook_list;
    int destroy_list_size;		/* state data for 2-phase destroy */
    int destroy_count;
    int dispatch_level;
    DestroyRec* destroy_list;
    Widget in_phase2_destroy;
    LangProcRec langProcRec;
    struct _TMBindCacheRec * free_bindings;
    _IswString display_name_tried;
    xcb_connection_t **dpy_destroy_list;
    IswEventQueue *event_front, *event_back;
    int dpy_destroy_count;
    Boolean exit_flag;
    Boolean rebuild_fdlist;
#ifdef XTHREADS
    LockPtr lock_info;
    ThreadAppProc lock;
    ThreadAppProc unlock;
    ThreadAppYieldLockProc yield_lock;
    ThreadAppRestoreLockProc restore_lock;
    ThreadAppProc free_lock;
#endif
} IswAppStruct;

extern void _IswHeapInit(Heap* heap);
extern void _IswHeapFree(Heap* heap);

#ifdef XTTRACEMEMORY


extern char *_IswHeapMalloc(
    Heap*	/* heap */,
    Cardinal	/* size */,
    const char */* file */,
    int		/* line */
);

#define _IswHeapAlloc(heap,bytes) _IswHeapMalloc(heap, bytes, __FILE__, __LINE__)

#else /* XTTRACEMEMORY */

extern char* _IswHeapAlloc(
    Heap*	/* heap */,
    Cardinal	/* size */
);

#endif /* XTTRACEMEMORY */

extern void _IswSetDefaultErrorHandlers(
    IswErrorMsgHandler*	/* errMsg */,
    IswErrorMsgHandler*	/* warnMsg */,
    IswErrorHandler*	/* err */,
    IswErrorHandler*	/* warn */
);

extern void _IswSetDefaultSelectionTimeout(
    unsigned long* /* timeout */
);

extern IswAppContext _IswDefaultAppContext(
    void
);

extern ProcessContext _IswGetProcessContext(
    void
);

xcb_connection_t *
_IswAppInit(
    IswAppContext*	/* app_context_return */,
    String		/* application_class */,
    XrmOptionDescRec*	/* options */,
    Cardinal		/* num_options */,
    int*		/* argc_in_out */,
    _IswString**		/* argv_in_out */,
    String*		/* fallback_resources */
);

extern void _IswDestroyAppContexts(
    void
);

extern void _IswCloseDisplays(
    IswAppContext	/* app */
);

extern int _IswAppDestroyCount;

extern int _IswWaitForSomething(
    IswAppContext	/* app */,
    _IswBoolean 		/* ignoreEvents */,
    _IswBoolean 		/* ignoreTimers */,
    _IswBoolean 		/* ignoreInputs */,
    _IswBoolean		/* ignoreSignals */,
    _IswBoolean 		/* block */,
//#ifdef XTHREADS
    _IswBoolean		/* drop_lock */,
//#endif
    unsigned long*	/* howlong */
);

typedef struct _CaseConverterRec *CaseConverterPtr;
typedef struct _CaseConverterRec {
    xcb_keysym_t		start;		/* first xcb_keysym_t valid in converter */
    xcb_keysym_t		stop;		/* last xcb_keysym_t valid in converter */
    IswCaseProc		proc;		/* case converter function */
    CaseConverterPtr	next;		/* next converter record */
} CaseConverterRec;

typedef struct _ExtensionSelectorRec {
    IswExtensionSelectProc proc;
    int min, max;
    IswPointer client_data;
} ExtSelectRec;

typedef struct _IswPixmapStruct {
    unsigned char depth;
    xcb_pixmap_t pixmap;
    UT_hash_handle hh;
} IswPixmapStruct, *IswPixmapStructPtr;

typedef struct _IswScreenPixmapStruct {
    xcb_screen_t *screen;
    IswPixmapStructPtr pixmaps;
    UT_hash_handle hh;
} IswScreenPixmapStruct, *IswScreenPixmapStructPtr;

typedef struct _IswPerDisplayStruct {
    InternalCallbackList destroy_callbacks;
    int defaultScreen;             /* default screen number from xcb_connect() */
    CaseConverterPtr case_cvt;		/* user-registered case converters */
    IswKeyProc defaultKeycodeTranslator;
    IswAppContext appContext;
    unsigned long keysyms_serial;      /* for tracking MappingNotify events */
    xcb_key_symbols_t *keysyms;                   /* keycode to keysym table */
    int keysyms_per_keycode;           /* number of keysyms for each keycode*/
    int min_keycode, max_keycode;      /* range of keycodes */
    IswKeySym *modKeysyms;                     /* keysym values for modToKeysysm */
    ModToKeysymTable *modsToKeysyms;   /* modifiers to Keysysms index table*/
    unsigned char isModifier[32];      /* key-is-modifier-p bit table */
    IswKeySym lock_meaning;	       /* Lock modifier meaning */
    Modifiers mode_switch;	       /* keyboard group modifiers */
    Modifiers num_lock;		       /* keyboard numlock modifiers */
    Boolean being_destroyed;
    Boolean rv;			       /* reverse_video resource */
    //XrmName name;		       /* resolved app name */
    String name;
    //XrmClass class;		       /* application class */
    String class;
    Heap heap;
    IswScreenPixmapStructPtr pixmap_tab;   /* pixmap cache */
    String language;		       /* XPG language string */
    xcb_generic_event_t last_event;		       /* last event dispatched */
    IswTime last_timestamp;	       /* from last event dispatched */
    int multi_click_time;	       /* for IswSetMultiClickTime */
    struct _TMKeyContextRec* tm_context;     /* for IswGetActionKeysym */
    InternalCallbackList mapping_callbacks;  /* special case for TM */
    IswPerDisplayInputRec pdi;	       /* state for modal grabs & kbd focus */
    const struct _IswPlatformOps *ops; /* injected backend ops table for this connection */
    xcb_connection_t *native;	       /* owned native connection (Phase 10a seam);
					  _IswXcbConn resolves this rather than
					  casting the handle */
    struct _WWTable *WWtable;	       /* window to widget table */
    IswDatabaseHandle *per_screen_db;  /* per screen resource databases */
    IswDatabaseHandle cmd_db;	       /* db from command line, if needed */
    IswDatabaseHandle server_db;       /* resource property else .Xdefaults */
    IswEventDispatchProc* dispatcher_list;
    ExtSelectRec* ext_select_list;
    int ext_select_count;
    Widget hook_object;
    IswPerWidgetInput PerWidgetContext;
    double scale_factor;	       /* HiDPI scale factor (1.0 = 96 DPI) */
    Atom net_wm_user_time;       /* _NET_WM_USER_TIME atom (0 = not yet interned) */
    Atom net_wm_user_time_window; /* _NET_WM_USER_TIME_WINDOW atom */
#ifndef X_NO_RESOURCE_CONFIGURATION_MANAGEMENT
    Atom rcm_init;			/* ResConfig - initialize */
    Atom rcm_data;			/* ResConfig - data Atom */
#endif
} IswPerDisplayStruct, *IswPerDisplay;

typedef struct _PerDisplayTable {
	xcb_connection_t *dpy;
	IswPerDisplayStruct perDpy;
	struct _PerDisplayTable *next;
} PerDisplayTable, *PerDisplayTablePtr;

extern PerDisplayTablePtr _IswperDisplayList;

extern IswPerDisplay _IswSortPerDisplayList(
    xcb_connection_t * /* dpy */
);

extern IswPerDisplay _IswGetPerDisplay(
    IswDisplay		/* dpy */
);

/*
 * _IswGetDefaultScreen - XCB replacement for the Xlib DefaultScreenOfDisplay()
 * macro.  DefaultScreenOfDisplay() reads ((Display*)dpy)->default_screen which
 * is meaningless for an xcb_connection_t*.  This function uses the screen
 * number stored in IswPerDisplay (set from xcb_connect's screen-number output)
 * and walks xcb_setup_roots_iterator() to return the correct xcb_screen_t*.
 */
extern xcb_screen_t *_IswGetDefaultScreen(
    xcb_connection_t *		/* dpy */
);

/* Look up the display owning a given screen.  Searches the per-display table.
 * Returns NULL if not found.  (Phase 12a: neutral handles; the backend resolves
 * them to xcb_connection_t/xcb_screen_t internally.) */
extern IswDisplay _IswConnectionOfScreen(IswScreen screen);

extern IswPerDisplayInputRec* _IswGetPerDisplayInput(
    IswDisplay 		/* dpy */
);

#if 0
#ifdef DEBUG
#define _IswGetPerDisplay(display) \
    ((_IswperDisplayList != NULL && (_IswperDisplayList->dpy == (display))) \
     ? &_IswperDisplayList->perDpy \
     : _IswSortPerDisplayList(display))
#define _IswGetPerDisplayInput(display) \
    ((_IswperDisplayList != NULL && (_IswperDisplayList->dpy == (display))) \
     ? &_IswperDisplayList->perDpy.pdi \
     : &_IswSortPerDisplayList(display)->pdi)
#else
#define _IswGetPerDisplay(display) \
    ((_IswperDisplayList->dpy == (display)) \
     ? &_IswperDisplayList->perDpy \
     : _IswSortPerDisplayList(display))
#define _IswGetPerDisplayInput(display) \
    ((_IswperDisplayList->dpy == (display)) \
     ? &_IswperDisplayList->perDpy.pdi \
     : &_IswSortPerDisplayList(display)->pdi)
#endif /*DEBUG*/
#endif

extern void _IswDisplayInitialize(
    xcb_connection_t *		/* dpy */,
    IswPerDisplay	/* pd */,
    _Xconst char*	/* name */,
    //XrmOptionDescRec*	/* urlist */,
    Cardinal 		/* num_urs */,
    int*		/* argc */,
    _IswString* 		/* argv */
);

extern void _IswCacheFlushTag(
    IswAppContext /* app */,
    IswPointer	 /* tag */
);

extern void _IswFreeActions(
    struct _ActionListRec* /* action_table */
);

extern void _IswDoPhase2Destroy(
    IswAppContext /* app */,
    int		 /* dispatch_level */
);

extern void _IswDoFreeBindings(
    IswAppContext /* app */
);

extern void _IswExtensionSelect(
    Widget /* widget */
);

#define _IswSafeToDestroy(app) ((app)->dispatch_level == 0)

extern void _IswAllocWWTable(
    IswPerDisplay pd
);

extern void _IswFreeWWTable(
    IswPerDisplay pd
);

extern String _IswGetUserName(_IswString dest, int len);
extern IswDatabaseHandle _IswPreparseCommandLine(XrmOptionDescRec *urlist,
			Cardinal num_urs, int argc, _IswString *argv,
			String *applName, String *displayName,
			String *language);

extern double _IswGetScaleFactor(IswDisplay dpy);

/* XCB → neutral IswEvent translation (ISWPlatformEventXCB.c).  Fills *out and
 * returns True for toolkit-semantic events; returns False for X11 protocol
 * events the toolkit does not see as IswEvents. */
extern Boolean _IswEventFromXcb(IswDisplay dpy,
                                xcb_generic_event_t *xev, IswEvent *out);

_XFUNCPROTOEND

#endif /* _IswinitialI_h */
