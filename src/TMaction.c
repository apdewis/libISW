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

/* TMaction.c -- maintains the state table of actions for the translation
 *              manager.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "StringDefs.h"

#if defined(__STDC__) && !defined(NORCONST)
#define RConst const
#else
#define RConst /**/
#endif
static _Xconst _IswString IswNtranslationError = "translationError";

typedef struct _CompiledAction {
    XrmQuark signature;
    IswActionProc proc;
} CompiledAction, *CompiledActionTable;

#define GetClassActions(wc) \
      ((wc->core_class.actions) \
       ? (((TMClassCache)wc->core_class.actions)->actions) \
       : NULL)

static CompiledActionTable
CompileActionTable(register RConst struct _IswActionsRec *actions, register Cardinal count,      /* may be 0 */
                   Boolean stat,        /* if False, copy before compiling in place */
                   Boolean perm)        /* if False, use XrmStringToQuark */
{
    register CompiledActionTable cActions;
    register int i;
    CompiledActionTable cTableHold;
    XrmQuark (*func) (_Xconst char *);

    if (!count)
        return (CompiledActionTable) NULL;
    func = (perm ? XrmPermStringToQuark : XrmStringToQuark);

    if (!stat) {
        cTableHold = cActions = IswMallocArray(count,
                                              (Cardinal) sizeof(CompiledAction));

        for (i = (int) count; --i >= 0; cActions++, actions++) {
            cActions->proc = actions->proc;
            cActions->signature = (*func) (actions->string);
        }
    }
    else {
        cTableHold = (CompiledActionTable) actions;

        for (i = (int) count; --i >= 0; actions++)
            ((CompiledActionTable) actions)->signature =
                (*func) (actions->string);
    }
    cActions = cTableHold;

    /* Insertion sort.  Whatever sort is used, it must be stable. */
    for (i = 1; (Cardinal) i <= count - 1; i++) {
        CompiledAction hold;
        register Cardinal j;

        hold = cActions[i];
        j = (Cardinal) i;
        while (j && cActions[j - 1].signature > hold.signature) {
            cActions[j] = cActions[j - 1];
            j--;
        }
        cActions[j] = hold;
    }

    return cActions;
}

typedef struct _ActionListRec *ActionList;
typedef struct _ActionListRec {
    ActionList next;
    CompiledActionTable table;
    TMShortCard count;
} ActionListRec;

static void
ReportUnboundActions(IswTranslations xlations, TMBindData bindData)
{
    TMSimpleStateTree stateTree;
    Cardinal num_unbound = 0;
    Cardinal num_params = 1;
    char *message;
    char messagebuf[1000];
    register Cardinal num_chars = 0;
    register Cardinal i, j;
    IswActionProc *procs;

    for (i = 0; i < xlations->numStateTrees; i++) {
        if (bindData->simple.isComplex)
            procs = TMGetComplexBindEntry(bindData, i)->procs;
        else
            procs = TMGetSimpleBindEntry(bindData, i)->procs;

        stateTree = (TMSimpleStateTree) xlations->stateTreeTbl[i];
        for (j = 0; j < stateTree->numQuarks; j++) {
            if (procs[j] == NULL) {
                String s = XrmQuarkToString(stateTree->quarkTbl[j]);

                if (num_unbound != 0)
                    num_chars += 2;
                num_chars += (Cardinal) strlen(s);
                num_unbound++;
            }
        }
    }
    if (num_unbound == 0)
        return;
    message = IswStackAlloc(num_chars + 1, messagebuf);
    if (message != NULL) {
        String params[1];

        *message = '\0';
        num_unbound = 0;
        for (i = 0; i < xlations->numStateTrees; i++) {
            if (bindData->simple.isComplex)
                procs = TMGetComplexBindEntry(bindData, i)->procs;
            else
                procs = TMGetSimpleBindEntry(bindData, i)->procs;

            stateTree = (TMSimpleStateTree) xlations->stateTreeTbl[i];
            for (j = 0; j < stateTree->numQuarks; j++) {
                if (procs[j] == NULL) {
                    String s = XrmQuarkToString(stateTree->quarkTbl[j]);

                    if (num_unbound != 0)
                        (void) strcat(message, ", ");
                    (void) strcat(message, s);
                    num_unbound++;
                }
            }
        }
        message[num_chars] = '\0';
        params[0] = message;
        IswWarningMsg(IswNtranslationError, "unboundActions", IswCIswToolkitError,
                     "Actions not found: %s", params, &num_params);
        IswStackFree(message, messagebuf);
    }
}

static CompiledAction *
SearchActionTable(XrmQuark signature,
                  CompiledActionTable actionTable,
                  Cardinal numActions)
{
    int left, right;

    left = 0;
    right = (int) numActions - 1;
    while (left <= right) {
        int i = (left + right) >> 1;

        if (signature < actionTable[i].signature)
            right = i - 1;
        else if (signature > actionTable[i].signature)
            left = i + 1;
        else {
            while (i && actionTable[i - 1].signature == signature)
                i--;
            return &actionTable[i];
        }
    }
    return (CompiledAction *) NULL;
}

static int
BindActions(TMSimpleStateTree stateTree,
            IswActionProc *procs,
            CompiledActionTable compiledActionTable,
            TMShortCard numActions,
            Cardinal *ndxP)
{
    register int unbound = (int) (stateTree->numQuarks - *ndxP);
    CompiledAction *action;
    register Cardinal ndx;
    register Boolean savedNdx = False;

    for (ndx = *ndxP; ndx < stateTree->numQuarks; ndx++) {
        if (procs[ndx] == NULL) {
            /* attempt to bind it */
            XrmQuark q = stateTree->quarkTbl[ndx];

            action = SearchActionTable(q, compiledActionTable, numActions);
            if (action) {
                procs[ndx] = action->proc;
                unbound--;
            }
            else if (!savedNdx) {
                *ndxP = ndx;
                savedNdx = True;
            }
        }
        else {
            /* already bound, leave it alone */
            unbound--;
        }
    }
    return unbound;
}

typedef struct _TMBindCacheStatusRec {
    unsigned int boundInClass:1;
    unsigned int boundInHierarchy:1;
    unsigned int boundInContext:1;
    unsigned int notFullyBound:1;
    unsigned int refCount:28;
} TMBindCacheStatusRec, *TMBindCacheStatus;

typedef struct _TMBindCacheRec {
    struct _TMBindCacheRec *next;
    TMBindCacheStatusRec status;
    TMStateTree stateTree;
#ifdef TRACE_TM
    WidgetClass widgetClass;
#endif                          /* TRACE_TM */
    IswActionProc procs[1];      /* variable length */
} TMBindCacheRec, *TMBindCache;

typedef struct _TMClassCacheRec {
    CompiledActionTable actions;
    TMBindCacheRec *bindCache;
} TMClassCacheRec, *TMClassCache;

#define IsPureClassBind(bc) \
  (bc->status.boundInClass && \
   !(bc->status.boundInHierarchy || \
     bc->status.boundInContext || \
     bc->status.notFullyBound))

#define GetClassCache(w) \
  ((TMClassCache)w->core.widget_class->core_class.actions)

static int
BindProcs(Widget widget,
          TMSimpleStateTree stateTree,
          IswActionProc *procs,
          TMBindCacheStatus bindStatus)
{
    register WidgetClass class;
    register ActionList actionList;
    int unbound = -1, newUnbound = -1;
    Cardinal ndx = 0;
    Widget w = widget;

    LOCK_PROCESS;
    do {
        class = w->core.widget_class;
        do {
            if (class->core_class.actions != NULL)
                unbound =
                    BindActions(stateTree,
                                procs,
                                GetClassActions(class),
                                (TMShortCard) class->core_class.num_actions,
                                &ndx);
            class = class->core_class.superclass;
        } while (unbound != 0 && class != NULL);
        if (unbound < (int) stateTree->numQuarks)
            bindStatus->boundInClass = True;
        else
            bindStatus->boundInClass = False;
        if (newUnbound == -1)
            newUnbound = unbound;
        w = IswParent(w);
    } while (unbound != 0 && w != NULL);

    if (newUnbound > unbound)
        bindStatus->boundInHierarchy = True;
    else
        bindStatus->boundInHierarchy = False;

    if (unbound) {
        IswAppContext app = IswWidgetToApplicationContext(widget);

        newUnbound = unbound;
        for (actionList = app->action_table;
             unbound != 0 && actionList != NULL;
             actionList = actionList->next) {
            unbound = BindActions(stateTree,
                                  procs,
                                  actionList->table, actionList->count, &ndx);
        }
        if (newUnbound > unbound)
            bindStatus->boundInContext = True;
        else
            bindStatus->boundInContext = False;

    }
    else {
        bindStatus->boundInContext = False;
    }
    UNLOCK_PROCESS;
    return unbound;
}

static IswActionProc *
TryBindCache(Widget widget, TMStateTree stateTree)
{
    TMClassCache classCache;

    LOCK_PROCESS;
    classCache = GetClassCache(widget);

    if (classCache == NULL) {
        WidgetClass wc = IswClass(widget);

        wc->core_class.actions = (IswActionList)
            _IswInitializeActionData(NULL, 0, True);
    }
    else {
        TMBindCache bindCache = (TMBindCache) (classCache->bindCache);

        for (; bindCache; bindCache = bindCache->next) {
            if (IsPureClassBind(bindCache) &&
                (stateTree == bindCache->stateTree)) {
                bindCache->status.refCount++;
                UNLOCK_PROCESS;
                return &bindCache->procs[0];
            }
        }
    }
    UNLOCK_PROCESS;
    return NULL;
}

/*
 * The class record actions field will point to the bind cache header
 * after this call is made out of coreClassPartInit.
 */
IswPointer
_IswInitializeActionData(register struct _IswActionsRec *actions,
                        register Cardinal count,
                        _IswBoolean inPlace)
{
    TMClassCache classCache;

    classCache = IswNew(TMClassCacheRec);
    classCache->actions =
        CompileActionTable(actions, count, (Boolean) inPlace, True);
    classCache->bindCache = NULL;
    return (IswPointer) classCache;
}

#define TM_BIND_CACHE_REALLOC   2

static IswActionProc *
EnterBindCache(Widget w,
               TMSimpleStateTree stateTree,
               IswActionProc *procs,
               TMBindCacheStatus bindStatus)
{
    TMClassCache classCache;
    TMBindCache *bindCachePtr;
    TMShortCard procsSize;
    TMBindCache bindCache;

    LOCK_PROCESS;
    classCache = GetClassCache(w);
    bindCachePtr = &classCache->bindCache;
    procsSize = (TMShortCard) (stateTree->numQuarks * sizeof(IswActionProc));

    for (bindCache = *bindCachePtr;
         (*bindCachePtr);
         bindCachePtr = &(*bindCachePtr)->next, bindCache = *bindCachePtr) {
        TMBindCacheStatus cacheStatus = &bindCache->status;

        if ((bindStatus->boundInClass == cacheStatus->boundInClass) &&
            (bindStatus->boundInHierarchy == cacheStatus->boundInHierarchy) &&
            (bindStatus->boundInContext == cacheStatus->boundInContext) &&
            (bindCache->stateTree == (TMStateTree) stateTree) &&
            !IswMemcmp(&bindCache->procs[0], procs, procsSize)) {
            bindCache->status.refCount++;
            break;
        }
    }
    if (*bindCachePtr == NULL) {
        *bindCachePtr = bindCache = (TMBindCache)
            __XtMalloc((Cardinal) (sizeof(TMBindCacheRec) +
                                   (size_t) (procsSize -
                                             sizeof(IswActionProc))));
        bindCache->next = NULL;
        bindCache->status = *bindStatus;
        bindCache->status.refCount = 1;
        bindCache->stateTree = (TMStateTree) stateTree;
#ifdef TRACE_TM
        bindCache->widgetClass = IswClass(w);
        if (_IswGlobalTM.numBindCache == _IswGlobalTM.bindCacheTblSize) {
            _IswGlobalTM.bindCacheTblSize =
                (TMShortCard) (_IswGlobalTM.bindCacheTblSize + 16);
            _IswGlobalTM.bindCacheTbl =
                IswReallocArray(_IswGlobalTM.bindCacheTbl,
                               (Cardinal) _IswGlobalTM.bindCacheTblSize,
                               (Cardinal) sizeof(TMBindCache));
        }
        _IswGlobalTM.bindCacheTbl[_IswGlobalTM.numBindCache++] = bindCache;
#endif                          /* TRACE_TM */
        memcpy(&bindCache->procs[0], procs, procsSize);
    }
    UNLOCK_PROCESS;
    return &bindCache->procs[0];
}

static void
RemoveFromBindCache(Widget w, IswActionProc *procs)
{
    TMClassCache classCache;
    TMBindCache *bindCachePtr;
    TMBindCache bindCache;
    IswAppContext app = IswWidgetToApplicationContext(w);

    LOCK_PROCESS;
    classCache = GetClassCache(w);
    bindCachePtr = (TMBindCache *) &classCache->bindCache;

    for (bindCache = *bindCachePtr;
         *bindCachePtr;
         bindCachePtr = &(*bindCachePtr)->next, bindCache = *bindCachePtr) {
        if (&bindCache->procs[0] == procs) {
            if (--bindCache->status.refCount == 0) {
#ifdef TRACE_TM
                TMShortCard j;
                Boolean found = False;
                TMBindCache *tbl = _IswGlobalTM.bindCacheTbl;

                for (j = 0; j < _IswGlobalTM.numBindCache; j++) {
                    if (found)
                        tbl[j - 1] = tbl[j];
                    if (tbl[j] == bindCache)
                        found = True;
                }
                if (!found)
                    IswWarning("where's the action ??? ");
                else
                    _IswGlobalTM.numBindCache--;
#endif                          /* TRACE_TM */
                *bindCachePtr = bindCache->next;
                bindCache->next = app->free_bindings;
                app->free_bindings = bindCache;
            }
            break;
        }
    }
    UNLOCK_PROCESS;
}

static void
RemoveAccelerators(Widget widget, IswPointer closure, IswPointer data _X_UNUSED)
{
    Widget destination = (Widget) closure;
    TMComplexBindProcs bindProcs;
    IswTranslations stackXlations[16];
    IswTranslations *xlationsList, destXlations;
    TMShortCard i, numXlations = 0;

    if ((destXlations = destination->core.tm.translations) == NULL) {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        IswNtranslationError, "nullTable", IswCIswToolkitError,
                        "Can't remove accelerators from NULL table",
                        NULL, NULL);
        return;
    }

    xlationsList = (IswTranslations *)
        IswStackAlloc((destXlations->numStateTrees * sizeof(IswTranslations)),
                     stackXlations);

    for (i = 0, bindProcs =
         TMGetComplexBindEntry(destination->core.tm.proc_table, i);
         i < destXlations->numStateTrees; i++, bindProcs++) {
        if (bindProcs->widget == widget) {
            /*
             * if it's being destroyed don't do all the work
             */
            if (destination->core.being_destroyed) {
                bindProcs->procs = NULL;
            }
            else
                xlationsList[numXlations] = bindProcs->aXlations;
            numXlations++;
        }
    }

    if (numXlations == 0)
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        IswNtranslationError, "nullTable", IswCIswToolkitError,
                        "Tried to remove nonexistent accelerators", NULL, NULL);
    else {
        if (!destination->core.being_destroyed)
            for (i = 0; i < numXlations; i++)
                _IswUnmergeTranslations(destination, xlationsList[i]);
    }
    IswStackFree((char *) xlationsList, stackXlations);
}

void
_IswBindActions(Widget widget, IswTM tm)
{
    IswTranslations xlations = tm->translations;
    int globalUnbound = 0;
    Cardinal i;
    TMBindData bindData = (TMBindData) tm->proc_table;
    TMSimpleBindProcs simpleBindProcs = NULL;
    TMComplexBindProcs complexBindProcs = NULL;
    IswActionProc *newProcs;
    Widget bindWidget;

    if ((xlations == NULL) || widget->core.being_destroyed)
        return;

    for (i = 0; i < xlations->numStateTrees; i++) {
        TMSimpleStateTree stateTree;

        stateTree = (TMSimpleStateTree) xlations->stateTreeTbl[i];
        if (bindData->simple.isComplex) {
            complexBindProcs = TMGetComplexBindEntry(bindData, i);
            if (complexBindProcs->widget) {
                bindWidget = complexBindProcs->widget;

                if (bindWidget->core.destroy_callbacks != NULL)
                    _IswAddCallbackOnce((InternalCallbackList *)
                                       &bindWidget->core.destroy_callbacks,
                                       RemoveAccelerators, (IswPointer) widget);
                else
                    _IswAddCallback((InternalCallbackList *)
                                   &bindWidget->core.destroy_callbacks,
                                   RemoveAccelerators, (IswPointer) widget);
            }
            else
                bindWidget = widget;
        }
        else {
            simpleBindProcs = TMGetSimpleBindEntry(bindData, i);
            bindWidget = widget;
        }
        if ((newProcs =
             TryBindCache(bindWidget, (TMStateTree) stateTree)) == NULL) {
            IswActionProc *procs, stackProcs[256];
            int localUnbound;
            TMBindCacheStatusRec bcStatusRec;

            procs = (IswActionProc *)
                IswStackAlloc(stateTree->numQuarks * sizeof(IswActionProc),
                             stackProcs);
            IswBZero((IswPointer) procs,
                    stateTree->numQuarks * sizeof(IswActionProc));

            localUnbound = BindProcs(bindWidget,
                                     stateTree, procs, &bcStatusRec);

            if (localUnbound)
                bcStatusRec.notFullyBound = True;
            else
                bcStatusRec.notFullyBound = False;

            newProcs =
                EnterBindCache(bindWidget, stateTree, procs, &bcStatusRec);
            IswStackFree((IswPointer) procs, (IswPointer) stackProcs);
            globalUnbound += localUnbound;
        }
        if (bindData->simple.isComplex)
            complexBindProcs->procs = newProcs;
        else
            simpleBindProcs->procs = newProcs;
    }
    if (globalUnbound)
        ReportUnboundActions(xlations, (TMBindData) tm->proc_table);
}

void
_IswUnbindActions(Widget widget, IswTranslations xlations, TMBindData bindData)
{
    Cardinal i;
    Widget bindWidget;
    IswActionProc *procs;

    if ((xlations == NULL) || !IswIsRealized(widget))
        return;

    for (i = 0; i < xlations->numStateTrees; i++) {
        if (bindData->simple.isComplex) {
            TMComplexBindProcs complexBindProcs;

            complexBindProcs = TMGetComplexBindEntry(bindData, i);

            if (complexBindProcs->widget) {
                /*
                 * check for this being an accelerator binding whose
                 * source is gone ( set by RemoveAccelerators)
                 */
                if (complexBindProcs->procs == NULL)
                    continue;

                IswRemoveCallback(complexBindProcs->widget,
                                 IswNdestroyCallback,
                                 RemoveAccelerators, (IswPointer) widget);
                bindWidget = complexBindProcs->widget;
            }
            else
                bindWidget = widget;
            procs = complexBindProcs->procs;
            complexBindProcs->procs = NULL;
        }
        else {
            TMSimpleBindProcs simpleBindProcs;

            simpleBindProcs = TMGetSimpleBindEntry(bindData, i);
            procs = simpleBindProcs->procs;
            simpleBindProcs->procs = NULL;
            bindWidget = widget;
        }
        RemoveFromBindCache(bindWidget, procs);
    }
}

#ifdef notdef
void
_IswRemoveBindProcsByIndex(Widget w, TMBindData bindData, TMShortCard ndx)
{
    TMShortCard i = ndx;
    TMBindProcs bindProcs = (TMBindProcs) &bindData->bindTbl[0];

    RemoveFromBindCache(bindProcs->widget ? bindProcs->widget : w,
                        bindProcs[i].procs);

    for (; i < bindData->bindTblSize; i++)
        bindProcs[i] = bindProcs[i + 1];
}
#endif                          /* notdef */

/*
 * used to free all copied action tables, called from DestroyAppContext
 */
void
_IswFreeActions(ActionList actions)
{
    ActionList curr, next;

    for (curr = actions; curr;) {
        next = curr->next;
        IswFree((char *) curr->table);
        IswFree((char *) curr);
        curr = next;
    }
}

void
IswAppAddActions(IswAppContext app, IswActionList actions, Cardinal num_actions)
{
    register ActionList rec;

    LOCK_APP(app);
    rec = IswNew(ActionListRec);
    rec->next = app->action_table;
    app->action_table = rec;
    rec->table = CompileActionTable(actions, num_actions, False, False);
    rec->count = (TMShortCard) num_actions;
    UNLOCK_APP(app);
}

void
IswGetActionList(WidgetClass widget_class,
                IswActionList *actions_return,
                Cardinal *num_actions_return)
{
    CompiledActionTable table;

    *actions_return = NULL;
    *num_actions_return = 0;

    LOCK_PROCESS;
    if (!widget_class->core_class.class_inited) {
        UNLOCK_PROCESS;
        return;
    }
    if (!(widget_class->core_class.class_inited & WidgetClassFlag)) {
        UNLOCK_PROCESS;
        return;
    }
    *num_actions_return = widget_class->core_class.num_actions;
    if (*num_actions_return) {
        IswActionList list = *actions_return =
            IswMallocArray(*num_actions_return, (Cardinal) sizeof(IswActionsRec));

        table = GetClassActions(widget_class);

        if (table != NULL) {
            int i;

            for (i = (int) (*num_actions_return); --i >= 0; list++, table++) {
                list->string = XrmQuarkToString(table->signature);
                list->proc = table->proc;
            }
        }
    }
    UNLOCK_PROCESS;
}

/***********************************************************************
 *
 * Pop-up and Grab stuff
 *
 ***********************************************************************/

static Widget
_IswFindPopup(Widget widget, String name)
{
    register Cardinal i;
    register XrmQuark q;
    register Widget w;

    q = XrmStringToQuark(name);

    for (w = widget; w != NULL; w = w->core.parent)
        for (i = 0; i < w->core.num_popups; i++)
            if (w->core.popup_list[i]->core.xrm_name == q)
                return w->core.popup_list[i];

    return NULL;
}

void
IswMenuPopupAction(Widget widget,
                  xcb_generic_event_t *event,
                  String *params,
                  Cardinal *num_params)
{
    Boolean spring_loaded;
    register Widget popup_shell;
    IswAppContext app = IswWidgetToApplicationContext(widget);

    LOCK_APP(app);
    if (*num_params != 1) {
        IswAppWarningMsg(app,
                        "invalidParameters", "xtMenuPopupAction",
                        IswCIswToolkitError,
                        "MenuPopup wants exactly one argument", NULL, NULL);
        UNLOCK_APP(app);
        return;
    }

    if (event->response_type == XCB_BUTTON_PRESS)
        spring_loaded = True;
    else if (event->response_type == XCB_KEY_PRESS || event->response_type == XCB_ENTER_NOTIFY)
        spring_loaded = False;
    else {
        IswAppWarningMsg(app,
                        "invalidPopup", "unsupportedOperation",
                        IswCIswToolkitError,
                        "Pop-up menu creation is only supported on ButtonPress, KeyPress or EnterNotify events.",
                        NULL, NULL);
        UNLOCK_APP(app);
        return;
    }

    popup_shell = _IswFindPopup(widget, params[0]);
    if (popup_shell == NULL) {
        IswAppWarningMsg(app,
                        "invalidPopup", "xtMenuPopup", IswCIswToolkitError,
                        "Can't find popup widget \"%s\" in IswMenuPopup",
                        params, num_params);
        UNLOCK_APP(app);
        return;
    }

    if (spring_loaded)
        _IswPopup(popup_shell, IswGrabExclusive, TRUE);
    else
        _IswPopup(popup_shell, IswGrabNonexclusive, FALSE);
    UNLOCK_APP(app);
}

static void
_IswMenuPopdownAction(Widget widget,
                    xcb_generic_event_t *event _X_UNUSED,
                     String *params,
                     Cardinal *num_params)
{
    Widget popup_shell;

    if (*num_params == 0) {
        IswPopdown(widget);
    }
    else if (*num_params == 1) {
        popup_shell = _IswFindPopup(widget, params[0]);
        if (popup_shell == NULL) {
            IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                            "invalidPopup", "xtMenuPopdown", IswCIswToolkitError,
                            "Can't find popup widget \"%s\" in IswMenuPopdown",
                            params, num_params);
            return;
        }
        IswPopdown(popup_shell);
    }
    else {
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidParameters", "xtMenuPopdown", IswCIswToolkitError,
                        "IswMenuPopdown called with num_params != 0 or 1",
                        NULL, NULL);
    }
}

/* *INDENT-OFF* */
static IswActionsRec RConst tmActions[] = {
    {"IswMenuPopup",                    IswMenuPopupAction},
    {"IswMenuPopdown",                  _IswMenuPopdownAction},
    {"MenuPopup",                      IswMenuPopupAction},      /* old & obsolete */
    {"MenuPopdown",                    _IswMenuPopdownAction},   /* ditto */
#ifndef NO_MIT_HACKS
    {"IswDisplayTranslations",          _IswDisplayTranslations},
    {"IswDisplayAccelerators",          _IswDisplayAccelerators},
    {"IswDisplayInstalledAccelerators", _IswDisplayInstalledAccelerators},
#endif
};
/* *INDENT-ON* */

void
_IswPopupInitialize(IswAppContext app)
{
    register ActionList rec;

    /*
     * The _IswGlobalTM.newMatchSemantics flag determines whether
     * we support old or new matching
     * behavior. This is mainly an issue of whether subsequent lhs will
     * get pushed up in the match table if a lhs containing this initial
     * sequence has already been encountered. Currently inited to False;
     */
#ifdef NEW_TM
    _IswGlobalTM.newMatchSemantics = True;
#else
    _IswGlobalTM.newMatchSemantics = False;
#endif

    rec = IswNew(ActionListRec);
    rec->next = app->action_table;
    app->action_table = rec;
    LOCK_PROCESS;
    rec->table = CompileActionTable(tmActions, IswNumber(tmActions), False,
                                    True);
    rec->count = IswNumber(tmActions);
    UNLOCK_PROCESS;
    _IswGrabInitialize(app);
}

void
IswCallActionProc(Widget widget,
                 _Xconst char *action,
                xcb_generic_event_t *event,
                 String *params,
                 Cardinal num_params)
{
    CompiledAction *actionP;
    XrmQuark q = XrmStringToQuark(action);
    Widget w = widget;
    IswAppContext app = IswWidgetToApplicationContext(widget);
    ActionList actionList;
    Cardinal i;

    LOCK_APP(app);
    IswCheckSubclass(widget, coreWidgetClass,
                    "IswCallActionProc first argument is not a subclass of Core");
    LOCK_PROCESS;
    do {
        WidgetClass class = IswClass(w);

        do {
            if ((actionP = GetClassActions(class)) != NULL)
                for (i = 0; i < class->core_class.num_actions; i++, actionP++) {

                    if (actionP->signature == q) {
                        ActionHook hook = app->action_hook_list;

                        while (hook != NULL) {
                            (*hook->proc) (widget,
                                           hook->closure,
                                           (String) action,
                                           event,
                                           params,
                                           &num_params);
                            hook = hook->next;
                        }
                        (*(actionP->proc))
                            (widget, event, params, &num_params);
                        UNLOCK_PROCESS;
                        UNLOCK_APP(app);
                        return;
                    }
                }
            class = class->core_class.superclass;
        } while (class != NULL);
        w = IswParent(w);
    } while (w != NULL);
    UNLOCK_PROCESS;

    for (actionList = app->action_table;
         actionList != NULL; actionList = actionList->next) {

        for (i = 0, actionP = actionList->table;
             i < actionList->count; i++, actionP++) {
            if (actionP->signature == q) {
                ActionHook hook = app->action_hook_list;

                while (hook != NULL) {
                    (*hook->proc) (widget,
                                   hook->closure,
                                   (String) action,
                                   event,
                                   params,
                                   &num_params);
                    hook = hook->next;
                }
                (*(actionP->proc))
                    (widget, event, params, &num_params);
                UNLOCK_APP(app);
                return;
            }
        }

    }

    {
        String par[2];
        Cardinal num_par = 2;

        par[0] = (String) action;
        par[1] = IswName(widget);
        IswAppWarningMsg(app,
                        "noActionProc", "xtCallActionProc", IswCIswToolkitError,
                        "No action proc named \"%s\" is registered for widget \"%s\"",
                        par, &num_par);
    }
    UNLOCK_APP(app);
}

void
_IswDoFreeBindings(IswAppContext app)
{
    TMBindCache bcp;

    while (app->free_bindings) {
        bcp = app->free_bindings->next;
        IswFree((char *) app->free_bindings);
        app->free_bindings = bcp;
    }
}
