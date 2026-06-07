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

*/

/* TMstate.c -- maintains the state table of actions for the translation
 *              manager.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#ifndef TM_NO_MATCH
#define TM_NO_MATCH (-2)
#endif                          /* TM_NO_MATCH */
/* forward definitions */
static StatePtr NewState(TMParseStateTree, TMShortCard, TMShortCard);

static String IswNtranslationError = "translationError";

TMGlobalRec _IswGlobalTM;        /* initialized to zero K&R */

#define MatchIncomingEvent(tmEvent, typeMatch, modMatch) \
  (typeMatch->eventType == tmEvent->event.eventType && \
   (typeMatch->matchEvent != NULL) && \
   (*typeMatch->matchEvent)(typeMatch, modMatch, tmEvent))

#define NumStateTrees(xlations) \
  ((translateData->isSimple) ? 1 : (TMComplexXlations(xlations))->numTrees)

static TMShortCard
GetBranchHead(TMParseStateTree parseTree,
              TMShortCard typeIndex,
              TMShortCard modIndex,
              Boolean isDummy)
{
#define TM_BRANCH_HEAD_TBL_ALLOC        ((TMShortCard) 8)
#define TM_BRANCH_HEAD_TBL_REALLOC      ((TMShortCard) 8)

    TMBranchHead branchHead = parseTree->branchHeadTbl;

    /*
     * dummy is used as a place holder for later matching in old-style
     * matching behavior. If there's already an entry we don't need
     * another dummy.
     */
    if (isDummy) {
        TMShortCard i;

        for (i = 0; i < parseTree->numBranchHeads; i++, branchHead++) {
            if ((branchHead->typeIndex == typeIndex) &&
                (branchHead->modIndex == modIndex))
                return i;
        }
    }
    if (parseTree->numBranchHeads == parseTree->branchHeadTblSize) {

        if (parseTree->branchHeadTblSize == 0)
            parseTree->branchHeadTblSize = TM_BRANCH_HEAD_TBL_ALLOC;
        else
            parseTree->branchHeadTblSize += TM_BRANCH_HEAD_TBL_REALLOC;

        if (parseTree->isStackBranchHeads) {
            TMBranchHead oldBranchHeadTbl = parseTree->branchHeadTbl;

            parseTree->branchHeadTbl =
                IswMallocArray((Cardinal) parseTree->branchHeadTblSize,
                              (Cardinal) sizeof(TMBranchHeadRec));
            memcpy(parseTree->branchHeadTbl, oldBranchHeadTbl,
                   parseTree->branchHeadTblSize * sizeof(TMBranchHeadRec));
            parseTree->isStackBranchHeads = False;
        }
        else {
            parseTree->branchHeadTbl = (TMBranchHead)
                IswReallocArray(parseTree->branchHeadTbl,
                               (Cardinal) parseTree->branchHeadTblSize,
                               (Cardinal) sizeof(TMBranchHeadRec));
        }
    }
#ifdef TRACE_TM
    LOCK_PROCESS;
    _IswGlobalTM.numBranchHeads++;
    UNLOCK_PROCESS;
#endif                          /* TRACE_TM */
    branchHead = &parseTree->branchHeadTbl[parseTree->numBranchHeads++];
    branchHead->typeIndex = typeIndex;
    branchHead->modIndex = modIndex;
    branchHead->more = 0;
    branchHead->isSimple = True;
    branchHead->hasActions = False;
    branchHead->hasCycles = False;
    return (TMShortCard) (parseTree->numBranchHeads - 1);
}

TMShortCard
_IswGetQuarkIndex(TMParseStateTree parseTree, XrmQuark quark)
{
#define TM_QUARK_TBL_ALLOC      ((TMShortCard) 16)
#define TM_QUARK_TBL_REALLOC    ((TMShortCard) 16)
    TMShortCard i;

    for (i = 0; i < parseTree->numQuarks; i++)
        if (parseTree->quarkTbl[i] == quark)
            break;

    if (i == parseTree->numQuarks) {
        if (parseTree->numQuarks == parseTree->quarkTblSize) {

            if (parseTree->quarkTblSize == 0)
                parseTree->quarkTblSize = TM_QUARK_TBL_ALLOC;
            else
                parseTree->quarkTblSize += TM_QUARK_TBL_REALLOC;

            if (parseTree->isStackQuarks) {
                XrmQuark *oldquarkTbl = parseTree->quarkTbl;

                parseTree->quarkTbl =
                    IswMallocArray((Cardinal) parseTree->quarkTblSize,
                                  (Cardinal) sizeof(XrmQuark));
                memcpy(parseTree->quarkTbl, oldquarkTbl,
                       parseTree->quarkTblSize * sizeof(XrmQuark));
                parseTree->isStackQuarks = False;
            }
            else {
                parseTree->quarkTbl = (XrmQuark *)
                    IswReallocArray(parseTree->quarkTbl,
                                   (Cardinal) parseTree->quarkTblSize,
                                   (Cardinal) sizeof(XrmQuark));
            }
        }
        parseTree->quarkTbl[parseTree->numQuarks++] = quark;
    }
    return i;
}

/*
 * Get an entry from the parseTrees complex branchHead tbl. If there's none
 * there then allocate one
 */
static TMShortCard
GetComplexBranchIndex(TMParseStateTree parseTree,
                      TMShortCard typeIndex _X_UNUSED,
                      TMShortCard modIndex _X_UNUSED)
{
#define TM_COMPLEXBRANCH_HEAD_TBL_ALLOC 8
#define TM_COMPLEXBRANCH_HEAD_TBL_REALLOC 4

    if (parseTree->numComplexBranchHeads == parseTree->complexBranchHeadTblSize) {
        if (parseTree->complexBranchHeadTblSize == 0)
            parseTree->complexBranchHeadTblSize =
                (TMShortCard) (parseTree->complexBranchHeadTblSize +
                               TM_COMPLEXBRANCH_HEAD_TBL_ALLOC);
        else
            parseTree->complexBranchHeadTblSize =
                (TMShortCard) (parseTree->complexBranchHeadTblSize +
                               TM_COMPLEXBRANCH_HEAD_TBL_REALLOC);

        if (parseTree->isStackComplexBranchHeads) {
            StatePtr *oldcomplexBranchHeadTbl = parseTree->complexBranchHeadTbl;

            parseTree->complexBranchHeadTbl =
                IswMallocArray((Cardinal) parseTree->complexBranchHeadTblSize,
                              (Cardinal) sizeof(StatePtr));
            memcpy(parseTree->complexBranchHeadTbl, oldcomplexBranchHeadTbl,
                   parseTree->complexBranchHeadTblSize * sizeof(StatePtr));
            parseTree->isStackComplexBranchHeads = False;
        }
        else {
            parseTree->complexBranchHeadTbl = (StatePtr *)
                IswReallocArray(parseTree->complexBranchHeadTbl,
                               (Cardinal) parseTree->complexBranchHeadTblSize,
                               (Cardinal) sizeof(StatePtr));
        }
    }
    parseTree->complexBranchHeadTbl[parseTree->numComplexBranchHeads++] = NULL;
    return (TMShortCard) (parseTree->numComplexBranchHeads - 1);
}

TMShortCard
_IswGetTypeIndex(Event *event)
{
    TMShortCard i, j = TM_TYPE_SEGMENT_SIZE;
    TMShortCard typeIndex = 0;
    TMTypeMatch typeMatch;
    TMTypeMatch segment = NULL;

    LOCK_PROCESS;
    for (i = 0; i < _IswGlobalTM.numTypeMatchSegments; i++) {
        segment = _IswGlobalTM.typeMatchSegmentTbl[i];
        for (j = 0;
             typeIndex < _IswGlobalTM.numTypeMatches && j < TM_TYPE_SEGMENT_SIZE;
             j++, typeIndex++) {
            typeMatch = &(segment[j]);
            if (event->eventType == typeMatch->eventType &&
                event->eventCode == typeMatch->eventCode &&
                event->eventCodeMask == typeMatch->eventCodeMask &&
                event->matchEvent == typeMatch->matchEvent) {
                UNLOCK_PROCESS;
                return typeIndex;
            }
        }
    }

    if (j == TM_TYPE_SEGMENT_SIZE) {
        if (_IswGlobalTM.numTypeMatchSegments ==
            _IswGlobalTM.typeMatchSegmentTblSize) {
            _IswGlobalTM.typeMatchSegmentTblSize =
                (TMShortCard) (_IswGlobalTM.typeMatchSegmentTblSize + 4);
            _IswGlobalTM.typeMatchSegmentTbl = (TMTypeMatch *)
                IswReallocArray(_IswGlobalTM.typeMatchSegmentTbl,
                               (Cardinal) _IswGlobalTM.typeMatchSegmentTblSize,
                               (Cardinal) sizeof(TMTypeMatch));
        }
        _IswGlobalTM.typeMatchSegmentTbl[_IswGlobalTM.numTypeMatchSegments++] =
            segment = IswMallocArray(TM_TYPE_SEGMENT_SIZE,
                                    (Cardinal) sizeof(TMTypeMatchRec));
        j = 0;
    }
    typeMatch = &segment[j];
    typeMatch->eventType = event->eventType;
    typeMatch->eventCode = event->eventCode;
    typeMatch->eventCodeMask = event->eventCodeMask;
    typeMatch->matchEvent = event->matchEvent;
    _IswGlobalTM.numTypeMatches++;
    UNLOCK_PROCESS;
    return typeIndex;
}

static Boolean
CompareLateModifiers(LateBindingsPtr lateBind1P, LateBindingsPtr lateBind2P)
{
    LateBindingsPtr late1P = lateBind1P;
    LateBindingsPtr late2P = lateBind2P;

    if (late1P != NULL || late2P != NULL) {
        int i = 0;
        int j = 0;

        if (late1P != NULL)
            for (; late1P->keysym != NoSymbol; i++)
                late1P++;
        if (late2P != NULL)
            for (; late2P->keysym != NoSymbol; j++)
                late2P++;
        if (i != j)
            return FALSE;
        late1P--;
        while (late1P >= lateBind1P) {
            Boolean last = True;

            for (late2P = lateBind2P + i - 1; late2P >= lateBind2P; late2P--) {
                if (late1P->keysym == late2P->keysym
                    && late1P->knot == late2P->knot) {
                    j--;
                    if (last)
                        i--;
                    break;
                }
                last = False;
            }
            late1P--;
        }
        if (j != 0)
            return FALSE;
    }
    return TRUE;
}

TMShortCard
_IswGetModifierIndex(Event *event)
{
    TMShortCard i, j = TM_MOD_SEGMENT_SIZE;
    TMShortCard modIndex = 0;
    TMModifierMatch modMatch;
    TMModifierMatch segment = NULL;

    LOCK_PROCESS;
    for (i = 0; i < _IswGlobalTM.numModMatchSegments; i++) {
        segment = _IswGlobalTM.modMatchSegmentTbl[i];
        for (j = 0;
             modIndex < _IswGlobalTM.numModMatches && j < TM_MOD_SEGMENT_SIZE;
             j++, modIndex++) {
            modMatch = &(segment[j]);
            if (event->modifiers == modMatch->modifiers &&
                event->modifierMask == modMatch->modifierMask &&
                event->standard == modMatch->standard &&
                ((!event->lateModifiers && !modMatch->lateModifiers) ||
                 CompareLateModifiers(event->lateModifiers,
                                      modMatch->lateModifiers))) {
                /*
                 * if we found a match then we can free the parser's
                 * late modifiers. If there isn't a match we use the
                 * parser's copy
                 */
                if (event->lateModifiers &&
                    --event->lateModifiers->ref_count == 0) {
                    IswFree((char *) event->lateModifiers);
                    event->lateModifiers = NULL;
                }
                UNLOCK_PROCESS;
                return modIndex;
            }
        }
    }

    if (j == TM_MOD_SEGMENT_SIZE) {
        if (_IswGlobalTM.numModMatchSegments ==
            _IswGlobalTM.modMatchSegmentTblSize) {
            _IswGlobalTM.modMatchSegmentTblSize =
                (TMShortCard) (_IswGlobalTM.modMatchSegmentTblSize + 4);
            _IswGlobalTM.modMatchSegmentTbl = (TMModifierMatch *)
                IswReallocArray(_IswGlobalTM.modMatchSegmentTbl,
                               (Cardinal) _IswGlobalTM.modMatchSegmentTblSize,
                               (Cardinal) sizeof(TMModifierMatch));
        }
        _IswGlobalTM.modMatchSegmentTbl[_IswGlobalTM.numModMatchSegments++] =
            segment = IswMallocArray(TM_MOD_SEGMENT_SIZE,
                                    (Cardinal) sizeof(TMModifierMatchRec));
        j = 0;
    }
    modMatch = &segment[j];
    modMatch->modifiers = event->modifiers;
    modMatch->modifierMask = event->modifierMask;
    modMatch->standard = event->standard;
    /*
     * We use the parser's copy of the late binding array
     */
#ifdef TRACE_TM
    if (event->lateModifiers)
        _IswGlobalTM.numLateBindings++;
#endif                          /* TRACE_TM */
    modMatch->lateModifiers = event->lateModifiers;
    _IswGlobalTM.numModMatches++;
    UNLOCK_PROCESS;
    return modIndex;
}

/*
 * This is called from the SimpleStateHandler to match a stateTree
 * entry to the event coming in
 */
static int
MatchBranchHead(TMSimpleStateTree stateTree, int startIndex, TMEventPtr event)
{
    TMBranchHead branchHead = &stateTree->branchHeadTbl[startIndex];
    int i;

    LOCK_PROCESS;
    for (i = startIndex; i < (int) stateTree->numBranchHeads; i++, branchHead++) {
        TMTypeMatch typeMatch;
        TMModifierMatch modMatch;

        typeMatch = TMGetTypeMatch(branchHead->typeIndex);
        modMatch = TMGetModifierMatch(branchHead->modIndex);

        if (MatchIncomingEvent(event, typeMatch, modMatch)) {
            UNLOCK_PROCESS;
            return i;
        }
    }
    UNLOCK_PROCESS;
    return (TM_NO_MATCH);
}

Boolean
_IswRegularMatch(TMTypeMatch typeMatch,
                TMModifierMatch modMatch,
                TMEventPtr eventSeq)
{
    Modifiers computed = 0;
    Modifiers computedMask = 0;
    Boolean resolved = TRUE;

    if (typeMatch->eventCode != (eventSeq->event.eventCode &
                                 typeMatch->eventCodeMask))
        return FALSE;
    if (modMatch->lateModifiers != NULL)
        resolved = _IswComputeLateBindings(eventSeq->dpy,
                                          modMatch->lateModifiers,
                                          &computed, &computedMask);
    if (!resolved)
        return FALSE;
    computed = (Modifiers) (computed | modMatch->modifiers);
    computedMask = (Modifiers) (computedMask | modMatch->modifierMask);

    return ((computed & computedMask) ==
            (eventSeq->event.modifiers & computedMask));
}

Boolean
_IswMatchAtom(TMTypeMatch typeMatch,
             TMModifierMatch modMatch _X_UNUSED,
             TMEventPtr eventSeq)
{
    Atom atom;
    const char *atom_name;

    atom_name = XrmQuarkToString((XrmQuark) (typeMatch->eventCode));
    atom = _IswPlatformInternAtomOp((IswDisplay) eventSeq->dpy, atom_name, False);

    if (atom == ISW_ATOM_NONE)
        return False;

    return (atom == eventSeq->event.eventCode);
}

#define IsOn(vec,idx) ((vec)[(idx)>>3] & (1 << ((idx) & 7)))

/*
 * there are certain cases where you want to ignore the event and stay
 * in the same state.
 */
static Boolean
Ignore(Widget widget, TMEventPtr event)
{
    xcb_connection_t *dpy;
    IswPerDisplay pd;

    if (event->event.eventType == XCB_MOTION_NOTIFY)
        return TRUE;
    if (!(event->event.eventType == XCB_KEY_PRESS ||
          event->event.eventType == XCB_KEY_RELEASE))
        return FALSE;
    dpy = _IswXcbConn(IswDisplayOf(widget));

    pd = _IswGetPerDisplay(IswDisplayOf(widget));
    _InitializeKeysymTables(dpy, pd);
    return IsOn(pd->isModifier, event->event.eventCode) ? TRUE : FALSE;
}

static void
XEventToTMEvent(xcb_generic_event_t *event, TMEventPtr tmEvent)
{
    tmEvent->xev = event;
    tmEvent->event.eventCodeMask = 0;
    tmEvent->event.modifierMask = 0;
    tmEvent->event.eventType = (TMLongCard) (event->response_type & ~0x80);
    tmEvent->event.lateModifiers = NULL;
    tmEvent->event.matchEvent = NULL;
    tmEvent->event.standard = FALSE;

    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        {
            xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)event;
            tmEvent->event.eventCode = key_event->detail;
            tmEvent->event.modifiers = key_event->state;
        }
        break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        {
            xcb_button_press_event_t *button_event = (xcb_button_press_event_t *)event;
            tmEvent->event.eventCode = button_event->detail;
            tmEvent->event.modifiers = button_event->state;
        }
        break;
    case XCB_MOTION_NOTIFY:
        {
            xcb_motion_notify_event_t *motion_event = (xcb_motion_notify_event_t *)event;
            tmEvent->event.eventCode = (TMLongCard) motion_event->detail; /* XCB_NOTIFY_MODE_NORMAL or NotifyHint */
            tmEvent->event.modifiers = motion_event->state;
        }
        break;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        {
            xcb_enter_notify_event_t *crossing_event = (xcb_enter_notify_event_t *)event;
            tmEvent->event.eventCode = (TMLongCard) crossing_event->mode;
            tmEvent->event.modifiers = crossing_event->state;
        }
        break;
    case XCB_PROPERTY_NOTIFY:
        {
            xcb_property_notify_event_t *prop_event = (xcb_property_notify_event_t *)event;
            tmEvent->event.eventCode = prop_event->atom;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_SELECTION_CLEAR:
        {
            xcb_selection_clear_event_t *sel_clear_event = (xcb_selection_clear_event_t *)event;
            tmEvent->event.eventCode = sel_clear_event->selection;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_SELECTION_REQUEST:
        {
            xcb_selection_request_event_t *sel_req_event = (xcb_selection_request_event_t *)event;
            tmEvent->event.eventCode = sel_req_event->selection;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_SELECTION_NOTIFY:
        {
            xcb_selection_notify_event_t *sel_notify_event = (xcb_selection_notify_event_t *)event;
            tmEvent->event.eventCode = sel_notify_event->selection;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_CLIENT_MESSAGE:
        {
            xcb_client_message_event_t *client_event = (xcb_client_message_event_t *)event;
            tmEvent->event.eventCode = client_event->type;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_MAPPING_NOTIFY:
        {
            xcb_mapping_notify_event_t *mapping_event = (xcb_mapping_notify_event_t *)event;
            tmEvent->event.eventCode = (TMLongCard) mapping_event->request;
            tmEvent->event.modifiers = 0;
        }
        break;
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
        {
            xcb_focus_in_event_t *focus_event = (xcb_focus_in_event_t *)event;
            tmEvent->event.eventCode = (TMLongCard) focus_event->mode;
            tmEvent->event.modifiers = 0;
        }
        break;
    default:
        tmEvent->event.eventCode = 0;
        tmEvent->event.modifiers = 0;
        break;
    }
}

static unsigned long
GetTime(IswTM tm, xcb_generic_event_t *event)
{
    switch (event->response_type & 0x7f) {  // Mask out the "reply" bit
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
            xcb_key_press_event_t *key_event = (xcb_key_press_event_t *)event;
            return key_event->time;
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
            xcb_button_press_event_t *button_event = (xcb_button_press_event_t *)event;
            return button_event->time;
        default:
            return tm->lastEventTime;
    }
}

static void
HandleActions(Widget w,
              xcb_generic_event_t *event,
              TMSimpleStateTree stateTree,
              Widget accelWidget,
              IswActionProc *procs,
              ActionRec *actions)
{
    ActionHook actionHookList;
    Widget bindWidget;

    bindWidget = accelWidget ? accelWidget : w;
    if (accelWidget && !IswIsSensitive(accelWidget) &&
    (event->response_type == XCB_KEY_PRESS || event->response_type == XCB_KEY_RELEASE ||
     event->response_type == XCB_BUTTON_PRESS || event->response_type == XCB_BUTTON_RELEASE ||
     event->response_type == XCB_MOTION_NOTIFY || event->response_type == XCB_ENTER_NOTIFY ||
     event->response_type == XCB_LEAVE_NOTIFY || event->response_type == XCB_FOCUS_IN ||
     event->response_type == XCB_FOCUS_OUT))
    return;

    actionHookList = IswWidgetToApplicationContext(w)->action_hook_list;

    /* Action procs and action hooks receive the toolkit-neutral event; the
       native event is reachable through IswEventNative(&nev) for code not yet
       migrated. */
    IswEvent nev;
    (void) _IswEventFromXcb(IswDisplayOf(w), event, &nev);

    while (actions != NULL) {
        /* perform any actions */
        if (procs[actions->idx] != NULL) {
            if (actionHookList) {
                ActionHook hook;
                ActionHook next_hook;
                String procName =
                    XrmQuarkToString(stateTree->quarkTbl[actions->idx]);

                for (hook = actionHookList; hook != NULL;) {
                    /*
                     * Need to cache hook->next because the following action
                     * proc may free hook via IswRemoveActionHook making
                     * hook->next invalid upon return from the action proc.
                     */
                    next_hook = hook->next;
                    (*hook->proc) (bindWidget,
                                   hook->closure,
                                   procName,
                                   &nev,
                                   actions->params, &actions->num_params);
                    hook = next_hook;
                }
            }
            (*(procs[actions->idx]))
                (bindWidget, &nev, actions->params, &actions->num_params);
        }
        actions = actions->next;
    }
}

typedef struct {
    unsigned int isCycleStart:1;
    unsigned int isCycleEnd:1;
    TMShortCard typeIndex;
    TMShortCard modIndex;
} MatchPairRec, *MatchPair;

typedef struct TMContextRec {
    TMShortCard numMatches;
    TMShortCard maxMatches;
    MatchPair matches;
} TMContextRec, *TMContext;

static TMContextRec contextCache[2];

#define GetContextPtr(tm) ((TMContext *)&(tm->current_state))

#define TM_CONTEXT_MATCHES_ALLOC 4
#define TM_CONTEXT_MATCHES_REALLOC 2

static void
PushContext(TMContext *contextPtr, StatePtr newState)
{
    TMContext context = *contextPtr;

    LOCK_PROCESS;
    if (context == NULL) {
        if (contextCache[0].numMatches == 0)
            context = &contextCache[0];
        else if (contextCache[1].numMatches == 0)
            context = &contextCache[1];
        if (!context) {
            context = IswNew(TMContextRec);
            context->matches = NULL;
            context->numMatches = context->maxMatches = 0;
        }
    }
    if (context->numMatches &&
        context->matches[context->numMatches - 1].isCycleEnd) {
        TMShortCard i;

        for (i = 0;
             i < context->numMatches &&
             !(context->matches[i].isCycleStart); i++) {
        };
        if (i < context->numMatches)
            context->numMatches = (TMShortCard) (i + 1);
#ifdef DEBUG
        else
            IswWarning("pushing cycle end with no cycle start");
#endif                          /* DEBUG */
    }
    else {
        if (context->numMatches == context->maxMatches) {
            if (context->maxMatches == 0)
                context->maxMatches =
                    (TMShortCard) (context->maxMatches +
                                   TM_CONTEXT_MATCHES_ALLOC);
            else
                context->maxMatches =
                    (TMShortCard) (context->maxMatches +
                                   TM_CONTEXT_MATCHES_REALLOC);
            context->matches = (MatchPairRec *)
                IswReallocArray(context->matches,
                               (Cardinal) context->maxMatches,
                               sizeof(MatchPairRec));
        }
        context->matches[context->numMatches].isCycleStart =
            newState->isCycleStart;
        context->matches[context->numMatches].isCycleEnd = newState->isCycleEnd;
        context->matches[context->numMatches].typeIndex = newState->typeIndex;
        context->matches[context->numMatches++].modIndex = newState->modIndex;
        *contextPtr = context;
    }
    UNLOCK_PROCESS;
}

static void
FreeContext(TMContext *contextPtr)
{
    TMContext context = NULL;

    LOCK_PROCESS;

    if (&contextCache[0] == *contextPtr)
        context = &contextCache[0];
    else if (&contextCache[1] == *contextPtr)
        context = &contextCache[1];

    if (context)
        context->numMatches = 0;
    else if (*contextPtr) {
        IswFree((char *) ((*contextPtr)->matches));
        IswFree((char *) *contextPtr);
    }

    *contextPtr = NULL;
    UNLOCK_PROCESS;
}

static int
MatchExact(TMSimpleStateTree stateTree,
           int startIndex,
           TMShortCard typeIndex,
           TMShortCard modIndex)
{
    TMBranchHead branchHead = &(stateTree->branchHeadTbl[startIndex]);
    int i;

    for (i = startIndex; i < (int) stateTree->numBranchHeads; i++, branchHead++) {
        if ((branchHead->typeIndex == typeIndex) &&
            (branchHead->modIndex == modIndex))
            return i;
    }
    return (TM_NO_MATCH);
}

static void
HandleSimpleState(Widget w, IswTM tmRecPtr, TMEventRec *curEventPtr)
{
    IswTranslations xlations = tmRecPtr->translations;
    TMContext *contextPtr = GetContextPtr(tmRecPtr);
    TMShortCard i;
    ActionRec *actions = NULL;
    Boolean matchExact = False;
    Boolean match = False;
    StatePtr complexMatchState = NULL;
    TMShortCard typeIndex = 0, modIndex = 0;
    int matchTreeIndex = TM_NO_MATCH;

    LOCK_PROCESS;
    for (i = 0;
         ((!match || !complexMatchState) && (i < xlations->numStateTrees));
         i++) {
        int currIndex = -1;
        TMSimpleStateTree stateTree =
            (TMSimpleStateTree) xlations->stateTreeTbl[i];

        /*
         * don't process this tree if we're only looking for a
         * complexMatchState and there are no complex states
         */
        while (!(match && stateTree->isSimple) &&
               ((!match || !complexMatchState) && (currIndex != TM_NO_MATCH))) {
            currIndex++;
            if (matchExact)
                currIndex =
                    MatchExact(stateTree, currIndex, typeIndex, modIndex);
            else
                currIndex = MatchBranchHead(stateTree, currIndex, curEventPtr);
            if (currIndex != TM_NO_MATCH) {
                TMBranchHead branchHead;
                StatePtr currState;

                branchHead = &stateTree->branchHeadTbl[currIndex];
                if (branchHead->isSimple)
                    currState = NULL;
                else
                    currState = ((TMComplexStateTree) stateTree)
                        ->complexBranchHeadTbl[TMBranchMore(branchHead)];

                /*
                 * first check for a complete match
                 */
                if (!match) {
                    if (branchHead->hasActions) {
                        if (branchHead->isSimple) {
                            static ActionRec dummyAction;

                            dummyAction.idx = TMBranchMore(branchHead);
                            actions = &dummyAction;
                        }
                        else
                            actions = currState->actions;
                        tmRecPtr->lastEventTime =
                            GetTime(tmRecPtr, curEventPtr->xev);
                        FreeContext((TMContext *) &tmRecPtr->current_state);
                        match = True;
                        matchTreeIndex = i;
                    }
                    /*
                     * if it doesn't have actions and
                     * it's bc mode then it's a potential match node that is
                     * used to match later sequences.
                     */
                    if (!TMNewMatchSemantics() && !matchExact) {
                        matchExact = True;
                        typeIndex = branchHead->typeIndex;
                        modIndex = branchHead->modIndex;
                    }
                }
                /*
                 * check for it being an event sequence which can be
                 * a future match
                 */
                if (!branchHead->isSimple &&
                    !branchHead->hasActions && !complexMatchState)
                    complexMatchState = currState;
            }
        }
    }
    if (match) {
        TMBindData bindData = (TMBindData) tmRecPtr->proc_table;
        IswActionProc *procs;
        Widget accelWidget;

        if (bindData->simple.isComplex) {
            TMComplexBindProcs bindProcs =
                TMGetComplexBindEntry(bindData, matchTreeIndex);
            procs = bindProcs->procs;
            accelWidget = bindProcs->widget;
        }
        else {
            TMSimpleBindProcs bindProcs =
                TMGetSimpleBindEntry(bindData, matchTreeIndex);
            procs = bindProcs->procs;
            accelWidget = NULL;
        }
        HandleActions
            (w,
             curEventPtr->xev,
             (TMSimpleStateTree) xlations->stateTreeTbl[matchTreeIndex],
             accelWidget, procs, actions);
    }
    if (complexMatchState)
        PushContext(contextPtr, complexMatchState);
    UNLOCK_PROCESS;
}

static int
MatchComplexBranch(TMComplexStateTree stateTree,
                   int startIndex,
                   TMContext context,
                   StatePtr *leafStateRtn)
{
    TMShortCard i;

    LOCK_PROCESS;
    for (i = (TMShortCard) startIndex; i < stateTree->numComplexBranchHeads;
         i++) {
        StatePtr candState;
        TMShortCard numMatches = context->numMatches;
        MatchPair statMatch = context->matches;

        for (candState = stateTree->complexBranchHeadTbl[i];
             numMatches && candState;
             numMatches--, statMatch++, candState = candState->nextLevel) {
            if ((statMatch->typeIndex != candState->typeIndex) ||
                (statMatch->modIndex != candState->modIndex))
                break;
        }
        if (numMatches == 0) {
            *leafStateRtn = candState;
            UNLOCK_PROCESS;
            return i;
        }
    }
    *leafStateRtn = NULL;
    UNLOCK_PROCESS;
    return (TM_NO_MATCH);
}

static StatePtr
TryCurrentTree(Widget widget,
               TMComplexStateTree *stateTreePtr,
               IswTM tmRecPtr,
               TMEventRec *curEventPtr)
{
    StatePtr candState = NULL, matchState = NULL;
    TMContext *contextPtr = GetContextPtr(tmRecPtr);
    TMTypeMatch typeMatch;
    TMModifierMatch modMatch;
    int currIndex = -1;

    /*
     * we want the first sequence that both matches and has actions.
     * we keep on looking till we find both
     */
    LOCK_PROCESS;
    while ((currIndex =
            MatchComplexBranch(*stateTreePtr,
                               ++currIndex, (*contextPtr), &candState))
           != TM_NO_MATCH) {
        if (candState != NULL) {
            typeMatch = TMGetTypeMatch(candState->typeIndex);
            modMatch = TMGetModifierMatch(candState->modIndex);

            /* does this state's index match? --> done */
            if (MatchIncomingEvent(curEventPtr, typeMatch, modMatch)) {
                if (candState->actions) {
                    UNLOCK_PROCESS;
                    return candState;
                }
                else
                    matchState = candState;
            }
            /* is this an event timer? */
            if (typeMatch->eventType == _IswEventTimerEventType) {
                StatePtr nextState = candState->nextLevel;

                /* does the succeeding state match? */
                if (nextState != NULL) {
                    TMTypeMatch nextTypeMatch;
                    TMModifierMatch nextModMatch;

                    nextTypeMatch = TMGetTypeMatch(nextState->typeIndex);
                    nextModMatch = TMGetModifierMatch(nextState->modIndex);

                    /* is it within the timeout? */
                    if (MatchIncomingEvent(curEventPtr,
                                           nextTypeMatch, nextModMatch)) {
                        xcb_generic_event_t *xev = curEventPtr->xev;
                        unsigned long time = GetTime(tmRecPtr, xev);
                        IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf(widget));
                        unsigned long delta =
                            (unsigned long) pd->multi_click_time;

                        if ((tmRecPtr->lastEventTime + delta) >= time) {
                            if (nextState->actions) {
                                UNLOCK_PROCESS;
                                return candState;
                            }
                            else
                                matchState = candState;
                        }
                    }
                }
            }
        }
    }
    UNLOCK_PROCESS;
    return matchState;
}

static void
HandleComplexState(Widget w, IswTM tmRecPtr, TMEventRec *curEventPtr)
{
    IswTranslations xlations = tmRecPtr->translations;
    TMContext *contextPtr = GetContextPtr(tmRecPtr);
    TMShortCard i, matchTreeIndex = 0;
    StatePtr matchState = NULL, candState;
    TMComplexStateTree *stateTreePtr =
        (TMComplexStateTree *) &xlations->stateTreeTbl[0];

    LOCK_PROCESS;
    for (i = 0; i < xlations->numStateTrees; i++, stateTreePtr++) {
        /*
         * some compilers sign extend Boolean bit fields so test for
         * false |||
         */
        if (((*stateTreePtr)->isSimple == False) &&
            (candState = TryCurrentTree(w, stateTreePtr, tmRecPtr, curEventPtr))) {
            if (!matchState || candState->actions) {
                matchTreeIndex = i;
                matchState = candState;
                if (candState->actions)
                    break;
            }
        }
    }
    if (matchState == NULL) {
        /* couldn't find it... */
        if (!Ignore(w, curEventPtr)) {
            FreeContext(contextPtr);
            HandleSimpleState(w, tmRecPtr, curEventPtr);
        }
    }
    else {
        TMBindData bindData = (TMBindData) tmRecPtr->proc_table;
        IswActionProc *procs;
        Widget accelWidget;
        TMTypeMatch typeMatch;

        typeMatch = TMGetTypeMatch(matchState->typeIndex);

        PushContext(contextPtr, matchState);
        if (typeMatch->eventType == _IswEventTimerEventType) {
            matchState = matchState->nextLevel;
            PushContext(contextPtr, matchState);
        }
        tmRecPtr->lastEventTime = GetTime(tmRecPtr, curEventPtr->xev);

        if (bindData->simple.isComplex) {
            TMComplexBindProcs bindProcs =
                TMGetComplexBindEntry(bindData, matchTreeIndex);
            procs = bindProcs->procs;
            accelWidget = bindProcs->widget;
        }
        else {
            TMSimpleBindProcs bindProcs =
                TMGetSimpleBindEntry(bindData, matchTreeIndex);
            procs = bindProcs->procs;
            accelWidget = NULL;
        }
        HandleActions(w, curEventPtr->xev, (TMSimpleStateTree)
                      xlations->stateTreeTbl[matchTreeIndex],
                      accelWidget, procs, matchState->actions);
    }
    UNLOCK_PROCESS;
}

void
_IswTranslateEvent(Widget w, xcb_generic_event_t *event)
{
    IswTM tmRecPtr = &w->core.tm;
    TMEventRec curEvent;
    StatePtr current_state = tmRecPtr->current_state;

    XEventToTMEvent(event, &curEvent);
    curEvent.dpy = _IswXcbConn(IswDisplayOf(w));  /* dpy not set by XEventToTMEvent; needed by _IswMatchUsingStandardMods */

    if (!tmRecPtr->translations) {
        IswAppWarningMsg(IswWidgetToApplicationContext(w),
                        IswNtranslationError, "nullTable", IswCIswToolkitError,
                        "Can't translate event through NULL table", NULL, NULL);
        return;
    }
    if (current_state == NULL)
        HandleSimpleState(w, tmRecPtr, &curEvent);
    else
        HandleComplexState(w, tmRecPtr, &curEvent);
}

static StatePtr
NewState(TMParseStateTree stateTree _X_UNUSED,
         TMShortCard typeIndex,
         TMShortCard modIndex)
{
    StatePtr state = IswNew(StateRec);

#ifdef TRACE_TM
    LOCK_PROCESS;
    _IswGlobalTM.numComplexStates++;
    UNLOCK_PROCESS;
#endif                          /* TRACE_TM */
    state->typeIndex = typeIndex;
    state->modIndex = modIndex;
    state->nextLevel = NULL;
    state->actions = NULL;
    state->isCycleStart = state->isCycleEnd = False;
    return state;
}

/*
 * This routine is an iterator for state trees. If the func returns
 * true then iteration is over.
 */
void
_IswTraverseStateTree(TMStateTree tree, _IswTraversalProc func, IswPointer data)
{
    TMComplexStateTree stateTree = (TMComplexStateTree) tree;
    TMBranchHead currBH;
    TMShortCard i;
    StateRec dummyStateRec, *dummyState = &dummyStateRec;
    ActionRec dummyActionRec, *dummyAction = &dummyActionRec;
    Boolean firstSimple = True;
    StatePtr currState;

    /* first traverse the complex states */
    if (stateTree->isSimple == False)
        for (i = 0; i < stateTree->numComplexBranchHeads; i++) {
            currState = stateTree->complexBranchHeadTbl[i];
            for (; currState; currState = currState->nextLevel) {
                if (func(currState, data))
                    return;
                if (currState->isCycleEnd)
                    break;
            }
        }

    /* now traverse the simple ones */
    for (i = 0, currBH = stateTree->branchHeadTbl;
         i < stateTree->numBranchHeads; i++, currBH++) {
        if (currBH->isSimple && currBH->hasActions) {
            if (firstSimple) {
                IswBZero((char *) dummyState, sizeof(StateRec));
                IswBZero((char *) dummyAction, sizeof(ActionRec));
                dummyState->actions = dummyAction;
                firstSimple = False;
            }
            dummyState->typeIndex = currBH->typeIndex;
            dummyState->modIndex = currBH->modIndex;
            dummyAction->idx = currBH->more;
            if (func(dummyState, data))
                return;
        }
    }
}

static EventMask
EventToMask(TMTypeMatch typeMatch, TMModifierMatch modMatch)
{
    EventMask returnMask;
    unsigned long eventType = typeMatch->eventType;

    if (eventType == XCB_MOTION_NOTIFY) {
        Modifiers modifierMask = (Modifiers) modMatch->modifierMask;
        Modifiers tempMask;

        returnMask = 0;
        if (modifierMask == 0) {
            if (modMatch->modifiers == AnyButtonMask)
                return XCB_EVENT_MASK_BUTTON_MOTION;
            else
                return XCB_EVENT_MASK_POINTER_MOTION;
        }
        tempMask = modifierMask &
            (XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3
             | XCB_BUTTON_MASK_4 | XCB_BUTTON_MASK_5);
        if (tempMask == 0)
            return XCB_EVENT_MASK_POINTER_MOTION;
        if (tempMask & XCB_BUTTON_MASK_1)
            returnMask |= XCB_EVENT_MASK_BUTTON_1_MOTION;
        if (tempMask & XCB_BUTTON_MASK_2)
            returnMask |= XCB_EVENT_MASK_BUTTON_2_MOTION;
        if (tempMask & XCB_BUTTON_MASK_3)
            returnMask |= XCB_EVENT_MASK_BUTTON_3_MOTION;
        if (tempMask & XCB_BUTTON_MASK_4)
            returnMask |= XCB_EVENT_MASK_BUTTON_4_MOTION;
        if (tempMask & XCB_BUTTON_MASK_5)
            returnMask |= XCB_EVENT_MASK_BUTTON_5_MOTION;
        return returnMask;
    }
    returnMask = _IswConvertTypeToMask((int) eventType);
    if (returnMask == (XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY))
        returnMask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    return returnMask;
}

static void
DispatchMappingNotify(Widget widget _X_UNUSED,  /* will be NULL from _RefreshMapping */
                      IswPointer closure,        /* real Widget */
                      IswPointer call_data)      /* xcb_generic_event_t* */
{
    _IswTranslateEvent((Widget) closure, (xcb_generic_event_t *) call_data);
}

static void
RemoveFromMappingCallbacks(Widget widget,
                           IswPointer closure,    /* target widget */
                           IswPointer call_data _X_UNUSED)
{
    _IswRemoveCallback(&_IswGetPerDisplay(IswDisplayOf(widget))->mapping_callbacks,
                      DispatchMappingNotify, closure);
}

static Boolean
AggregateEventMask(StatePtr state, IswPointer data)
{
    LOCK_PROCESS;
    *((EventMask *) data) |= EventToMask(TMGetTypeMatch(state->typeIndex),
                                         TMGetModifierMatch(state->modIndex));
    UNLOCK_PROCESS;
    return False;
}

void
_IswInstallTranslations(Widget widget)
{
    IswTranslations xlations;
    Cardinal i;
    Boolean mappingNotifyInterest = False;

    xlations = widget->core.tm.translations;
    if (xlations == NULL)
        return;

    /*
     * check for somebody stuffing the translations directly into the
     * instance structure. We will end up being called again out of
     * ComposeTranslations but we *should* have bindings by then
     */
    if (widget->core.tm.proc_table == NULL) {
        _IswMergeTranslations(widget, NULL, IswTableReplace);
        /*
         * if we're realized then we'll be called out of
         * ComposeTranslations
         */
        if (IswIsRealized(widget))
            return;
    }

    xlations->eventMask = 0;
    for (i = 0; i < xlations->numStateTrees; i++) {
        TMStateTree stateTree = xlations->stateTreeTbl[i];

        _IswTraverseStateTree(stateTree,
                             AggregateEventMask,
                             (IswPointer) &xlations->eventMask);
        mappingNotifyInterest =
            (Boolean) (mappingNotifyInterest |
                       stateTree->simple.mappingNotifyInterest);
    }
    /* double click needs to make sure that you have selected on both
       button down and up. */

    if (xlations->eventMask & XCB_EVENT_MASK_BUTTON_PRESS)
        xlations->eventMask |= XCB_EVENT_MASK_BUTTON_RELEASE;
    if (xlations->eventMask & XCB_EVENT_MASK_BUTTON_RELEASE)
        xlations->eventMask |= XCB_EVENT_MASK_BUTTON_PRESS;


    if (mappingNotifyInterest) {
        IswPerDisplay pd = _IswGetPerDisplay(IswDisplayOf(widget));

        if (pd->mapping_callbacks)
            _IswAddCallbackOnce(&(pd->mapping_callbacks),
                               DispatchMappingNotify, (IswPointer) widget);
        else
            _IswAddCallback(&(pd->mapping_callbacks),
                           DispatchMappingNotify, (IswPointer) widget);

        if (widget->core.destroy_callbacks != NULL)
            _IswAddCallbackOnce((InternalCallbackList *)
                               &widget->core.destroy_callbacks,
                               RemoveFromMappingCallbacks, (IswPointer) widget);
        else
            _IswAddCallback((InternalCallbackList *)
                           &widget->core.destroy_callbacks,
                           RemoveFromMappingCallbacks, (IswPointer) widget);
    }
    _IswBindActions(widget, (IswTM) &widget->core.tm);
    _IswRegisterGrabs(widget);
}

void
_IswRemoveTranslations(Widget widget)
{
    Cardinal i;
    Boolean mappingNotifyInterest = False;
    IswTranslations xlations = widget->core.tm.translations;

    if (xlations == NULL)
        return;

    for (i = 0; i < xlations->numStateTrees; i++) {
        TMSimpleStateTree stateTree =
            (TMSimpleStateTree) xlations->stateTreeTbl[i];
        mappingNotifyInterest =
            (Boolean) (mappingNotifyInterest |
                       stateTree->mappingNotifyInterest);
    }
    if (mappingNotifyInterest)
        RemoveFromMappingCallbacks(widget, (IswPointer) widget, NULL);
}

static void
_IswUninstallTranslations(Widget widget)
{
    IswTranslations xlations = widget->core.tm.translations;

    _IswUnbindActions(widget, xlations, (TMBindData) widget->core.tm.proc_table);
    _IswRemoveTranslations(widget);
    widget->core.tm.translations = NULL;
    FreeContext((TMContext *) &widget->core.tm.current_state);
}

void
_IswDestroyTMData(Widget widget)
{
    TMComplexBindData cBindData;

    _IswUninstallTranslations(widget);

    if ((cBindData = (TMComplexBindData) widget->core.tm.proc_table)) {
        if (cBindData->isComplex) {
            ATranslations nXlations = (ATranslations) cBindData->accel_context;

            while (nXlations) {
                ATranslations aXlations = nXlations;

                nXlations = nXlations->next;
                IswFree((char *) aXlations);
            }
        }
        IswFree((char *) cBindData);
    }
}

/*** Public procedures ***/

void
IswUninstallTranslations(Widget widget)
{
    EventMask oldMask;
    Widget hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    if (!widget->core.tm.translations) {
        UNLOCK_APP(app);
        return;
    }
    oldMask = widget->core.tm.translations->eventMask;
    _IswUninstallTranslations(widget);
    if (IswIsRealized(widget) && oldMask) {
        uint32_t event_mask = (uint32_t) IswBuildEventMask(widget);
        (void)xcb_change_window_attributes(
            _IswXcbConn(IswDisplayOf(widget)),  /* your XCB connection */
            _IswXcbWindow(IswWindowOf(widget)),  /* window XID */
            XCB_CW_EVENT_MASK,
            &event_mask
        );
    }
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHuninstallTranslations;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_APP(app);
}

IswTranslations
_IswCreateXlations(TMStateTree *stateTrees,
                  TMShortCard numStateTrees,
                  IswTranslations first,
                  IswTranslations second)
{
    IswTranslations xlations;
    TMShortCard i;

    xlations = (IswTranslations)
        __XtMalloc((Cardinal) (sizeof(TranslationData) +
                               (size_t) (numStateTrees -
                                         1) * sizeof(TMStateTree)));
#ifdef TRACE_TM
    LOCK_PROCESS;
    if (_IswGlobalTM.numTms == _IswGlobalTM.tmTblSize) {
        _IswGlobalTM.tmTblSize = (TMShortCard) (_IswGlobalTM.tmTblSize + 16);
        _IswGlobalTM.tmTbl = (IswTranslations *)
            IswReallocArray(_IswGlobalTM.tmTbl,
                           (Cardinal) _IswGlobalTM.tmTblSize,
                           (Cardinal) sizeof(IswTranslations));
    }
    _IswGlobalTM.tmTbl[_IswGlobalTM.numTms++] = xlations;
    UNLOCK_PROCESS;
#endif                          /* TRACE_TM */

    xlations->composers[0] = first;
    xlations->composers[1] = second;
    xlations->hasBindings = False;
    xlations->operation = IswTableReplace;

    for (i = 0; i < numStateTrees; i++) {
        xlations->stateTreeTbl[i] = (TMStateTree) stateTrees[i];
        stateTrees[i]->simple.refCount++;
    }
    xlations->numStateTrees = numStateTrees;
    xlations->eventMask = 0;
    return xlations;
}

TMStateTree
_IswParseTreeToStateTree(TMParseStateTree parseTree)
{
    TMSimpleStateTree simpleTree;

    if (parseTree->numComplexBranchHeads) {
        TMComplexStateTree complexTree;

        complexTree = IswNew(TMComplexStateTreeRec);
        complexTree->isSimple = False;
        complexTree->complexBranchHeadTbl =
            IswMallocArray((Cardinal) parseTree->numComplexBranchHeads,
                          (Cardinal) sizeof(StatePtr));
        memcpy(complexTree->complexBranchHeadTbl,
               parseTree->complexBranchHeadTbl,
               parseTree->numComplexBranchHeads * sizeof(StatePtr));
        complexTree->numComplexBranchHeads = parseTree->numComplexBranchHeads;
        simpleTree = (TMSimpleStateTree) complexTree;
    }
    else {
        simpleTree = IswNew(TMSimpleStateTreeRec);
        simpleTree->isSimple = True;
    }
    simpleTree->isAccelerator = parseTree->isAccelerator;
    simpleTree->refCount = 0;
    simpleTree->mappingNotifyInterest = parseTree->mappingNotifyInterest;

    simpleTree->branchHeadTbl =
        IswMallocArray((Cardinal) parseTree->numBranchHeads,
                      (Cardinal) sizeof(TMBranchHeadRec));
    memcpy(simpleTree->branchHeadTbl, parseTree->branchHeadTbl,
           parseTree->numBranchHeads * sizeof(TMBranchHeadRec));
    simpleTree->numBranchHeads = parseTree->numBranchHeads;

    simpleTree->quarkTbl = IswMallocArray((Cardinal) parseTree->numQuarks,
                                         (Cardinal) sizeof(XrmQuark));
    memcpy(simpleTree->quarkTbl, parseTree->quarkTbl,
           parseTree->numQuarks * sizeof(XrmQuark));
    simpleTree->numQuarks = parseTree->numQuarks;

    return (TMStateTree) simpleTree;
}

static void
FreeActions(ActionPtr actions)
{
    ActionPtr action;
    TMShortCard i;

    for (action = actions; action;) {
        ActionPtr nextAction = action->next;

        for (i = (TMShortCard) action->num_params; i;) {
            IswFree((_IswString) action->params[--i]);
        }
        IswFree((char *) action->params);
        IswFree((char *) action);
        action = nextAction;
    }
}

static void
AmbigActions(EventSeqPtr initialEvent,
             StatePtr *state,
             TMParseStateTree stateTree)
{
    String params[3];
    Cardinal numParams = 0;

    params[numParams++] = _IswPrintEventSeq(initialEvent, NULL);
    params[numParams++] = _IswPrintActions((*state)->actions,
                                          stateTree->quarkTbl);
    IswWarningMsg(IswNtranslationError, "oldActions", IswCIswToolkitError,
                 "Previous entry was: %s %s", params, &numParams);
    IswFree((char *) params[0]);
    IswFree((char *) params[1]);
    numParams = 0;
    params[numParams++] = _IswPrintActions(initialEvent->actions,
                                          stateTree->quarkTbl);
    IswWarningMsg(IswNtranslationError, "newActions", IswCIswToolkitError,
                 "New actions are:%s", params, &numParams);
    IswFree((char *) params[0]);
    IswWarningMsg(IswNtranslationError, "ambiguousActions",
                 IswCIswToolkitError,
                 "Overriding earlier translation manager actions.", NULL, NULL);

    FreeActions((*state)->actions);
    (*state)->actions = NULL;
}

void
_IswAddEventSeqToStateTree(EventSeqPtr eventSeq, TMParseStateTree stateTree)
{
    StatePtr *state;
    EventSeqPtr initialEvent = eventSeq;
    TMBranchHead branchHead;
    TMShortCard idx, modIndex, typeIndex;

    if (eventSeq == NULL)
        return;

    /* note that all states in the event seq passed in start out null */
    /* we fill them in with the matching state as we traverse the list */

    /*
     * We need to free the parser data structures !!!
     */

    typeIndex = _IswGetTypeIndex(&eventSeq->event);
    modIndex = _IswGetModifierIndex(&eventSeq->event);
    idx = GetBranchHead(stateTree, typeIndex, modIndex, False);
    branchHead = &stateTree->branchHeadTbl[idx];

    /*
     * Need to check for pre-existing actions with same lhs |||
     */

    /*
     * Check for optimized case. Don't assume that the eventSeq has actions.
     */
    if (!eventSeq->next &&
        eventSeq->actions &&
        !eventSeq->actions->next && !eventSeq->actions->num_params) {
        if (eventSeq->event.eventType == XCB_MAPPING_NOTIFY)
            stateTree->mappingNotifyInterest = True;
        branchHead->hasActions = True;
        IswSetBits(branchHead->more, eventSeq->actions->idx, 13);
        FreeActions(eventSeq->actions);
        eventSeq->actions = NULL;
        return;
    }

    branchHead->isSimple = False;
    if (!eventSeq->next)
        branchHead->hasActions = True;
    IswSetBits(branchHead->more,
              GetComplexBranchIndex(stateTree, typeIndex, modIndex), 13);
    state = &stateTree->complexBranchHeadTbl[TMBranchMore(branchHead)];

    for (;;) {
        *state = NewState(stateTree, typeIndex, modIndex);

        if (eventSeq->event.eventType == XCB_MAPPING_NOTIFY)
            stateTree->mappingNotifyInterest = True;

        /* *state now points at state record matching event */
        eventSeq->state = *state;

        if (eventSeq->actions != NULL) {
            if ((*state)->actions != NULL)
                AmbigActions(initialEvent, state, stateTree);
            (*state)->actions = eventSeq->actions;
#ifdef TRACE_TM
            LOCK_PROCESS;
            _IswGlobalTM.numComplexActions++;
            UNLOCK_PROCESS;
#endif                          /* TRACE_TM */
        }

        if (((eventSeq = eventSeq->next) == NULL) || (eventSeq->state))
            break;

        state = &(*state)->nextLevel;
        typeIndex = _IswGetTypeIndex(&eventSeq->event);
        modIndex = _IswGetModifierIndex(&eventSeq->event);
        LOCK_PROCESS;
        if (!TMNewMatchSemantics()) {
            /*
             * force a potential empty entry into the branch head
             * table in order to emulate old matching behavior
             */
            (void) GetBranchHead(stateTree, typeIndex, modIndex, True);
        }
        UNLOCK_PROCESS;
    }

    if (eventSeq && eventSeq->state) {
        /* we've been here before... must be a cycle in the event seq. */
        branchHead->hasCycles = True;
        (*state)->nextLevel = eventSeq->state;
        eventSeq->state->isCycleStart = True;
        (*state)->isCycleEnd = TRUE;
    }
}

/*
 * Internal Converter for merging. Old and New must both be valid xlations
 */
Boolean
_IswCvtMergeTranslations(IswDisplay dpy _X_UNUSED,
                        XrmValuePtr args _X_UNUSED,
                        Cardinal *num_args,
                        XrmValuePtr from,
                        XrmValuePtr to,
                        IswPointer *closure_ret _X_UNUSED)
{
    IswTranslations first, second, xlations;
    TMStateTree *stateTrees, stackStateTrees[16];
    TMShortCard numStateTrees, i;

    if (*num_args != 0)
        IswWarningMsg("invalidParameters", "mergeTranslations",
                     IswCIswToolkitError,
                     "MergeTM to TranslationTable needs no extra arguments",
                     NULL, NULL);

    if (to->addr != NULL && to->size < sizeof(IswTranslations)) {
        to->size = sizeof(IswTranslations);
        return False;
    }

    first = ((TMConvertRec *) from->addr)->old;
    second = ((TMConvertRec *) from->addr)->new;

    numStateTrees =
        (TMShortCard) (first->numStateTrees + second->numStateTrees);

    stateTrees = (TMStateTree *)
        IswStackAlloc(numStateTrees * sizeof(TMStateTree), stackStateTrees);

    for (i = 0; i < first->numStateTrees; i++)
        stateTrees[i] = first->stateTreeTbl[i];
    for (i = 0; i < second->numStateTrees; i++)
        stateTrees[i + first->numStateTrees] = second->stateTreeTbl[i];

    xlations = _IswCreateXlations(stateTrees, numStateTrees, first, second);

    if (to->addr != NULL) {
        *(IswTranslations *) to->addr = xlations;
    }
    else {
        static IswTranslations staticStateTable;

        staticStateTable = xlations;
        to->addr = (IswPointer) &staticStateTable;
        to->size = sizeof(IswTranslations);
    }

    IswStackFree((IswPointer) stateTrees, (IswPointer) stackStateTrees);
    return True;
}

static IswTranslations
MergeThem(Widget dest, IswTranslations first, IswTranslations second)
{
    IswCacheRef cache_ref;
    static XrmQuark from_type = NULLQUARK, to_type;
    XrmValue from, to;
    TMConvertRec convert_rec;
    IswTranslations newTable;

    LOCK_PROCESS;
    if (from_type == NULLQUARK) {
        from_type = XrmPermStringToQuark(_IswRStateTablePair);
        to_type = XrmPermStringToQuark(IswRTranslationTable);
    }
    UNLOCK_PROCESS;
    from.addr = (IswPointer) &convert_rec;
    from.size = sizeof(TMConvertRec);
    to.addr = (IswPointer) &newTable;
    to.size = sizeof(IswTranslations);
    convert_rec.old = first;
    convert_rec.new = second;

    LOCK_PROCESS;
    if (!_IswConvert(dest, from_type, &from, to_type, &to, &cache_ref)) {
        UNLOCK_PROCESS;
        return NULL;
    }
    UNLOCK_PROCESS;

#ifndef REFCNT_TRANSLATIONS

    if (cache_ref)
        IswAddCallback(dest, IswNdestroyCallback,
                      IswCallbackReleaseCacheRef, (IswPointer) cache_ref);

#endif

    return newTable;
}

/*
 * Unmerge will recursively traverse the xlation compose tree and
 * generate a new xlation that is the result of all instances of
 * xlations being removed. It currently doesn't differentiate between
 * the potential that an xlation will be both an accelerator and
 * normal. This is not supported by the spec anyway.
 */
static IswTranslations
UnmergeTranslations(Widget widget,
                    IswTranslations xlations,
                    IswTranslations unmergeXlations,
                    TMShortCard currIndex,
                    TMComplexBindProcs oldBindings,
                    TMShortCard numOldBindings,
                    TMComplexBindProcs newBindings,
                    TMShortCard *numNewBindingsRtn)
{
    IswTranslations first, second, result;

    if (!xlations || (xlations == unmergeXlations))
        return NULL;

    if (xlations->composers[0]) {
        first = UnmergeTranslations(widget, xlations->composers[0],
                                    unmergeXlations, currIndex,
                                    oldBindings, numOldBindings,
                                    newBindings, numNewBindingsRtn);
    }
    else
        first = NULL;

    if (xlations->composers[0]
        && xlations->composers[1]) {
        second = UnmergeTranslations(widget, xlations->composers[1],
                                     unmergeXlations, (TMShortCard)
                                     (currIndex +
                                      xlations->composers[0]->numStateTrees),
                                     oldBindings,
                                     numOldBindings, newBindings,
                                     numNewBindingsRtn);
    }
    else
        second = NULL;

    if (first || second) {
        if (first && second) {
            if ((first != xlations->composers[0]) ||
                (second != xlations->composers[1]))
                result = MergeThem(widget, first, second);
            else
                result = xlations;
        }
        else {
            if (first)
                result = first;
            else
                result = second;
        }
    }
    else {                      /* only update for leaf nodes */
        if (numOldBindings) {
            Cardinal i;

            for (i = 0; i < xlations->numStateTrees; i++) {
                if (xlations->stateTreeTbl[i]->simple.isAccelerator)
                    newBindings[*numNewBindingsRtn] =
                        oldBindings[currIndex + i];
                (*numNewBindingsRtn)++;
            }
        }
        result = xlations;
    }
    return result;
}

typedef struct {
    IswTranslations xlations;
    TMComplexBindProcs bindings;
} MergeBindRec, *MergeBind;

static IswTranslations
MergeTranslations(Widget widget,
                  IswTranslations oldXlations,
                  IswTranslations newXlations,
                  _IswTranslateOp operation,
                  Widget source,
                  TMComplexBindProcs oldBindings,
                  TMComplexBindProcs newBindings,
                  TMShortCard *numNewRtn)
{
    IswTranslations newTable = NULL, xlations;
    TMComplexBindProcs bindings;
    TMShortCard i, j;
    TMStateTree *treePtr;
    TMShortCard numNew;
    MergeBindRec bindPair[2];

    /* If the new translation has an accelerator context then pull it
     * off and pass it and the real xlations in to the caching merge
     * routine.
     */
    if (newXlations->hasBindings) {
        xlations = ((ATranslations) newXlations)->xlations;
        bindings = (TMComplexBindProcs)
            &((ATranslations) newXlations)->bindTbl[0];
    }
    else {
        xlations = newXlations;
        bindings = NULL;
    }
    switch (operation) {
    default:
    case IswTableReplace:
        newTable = bindPair[0].xlations = xlations;
        bindPair[0].bindings = bindings;
        bindPair[1].xlations = NULL;
        bindPair[1].bindings = NULL;
        break;
    case IswTableAugment:
        bindPair[0].xlations = oldXlations;
        bindPair[0].bindings = oldBindings;
        bindPair[1].xlations = xlations;
        bindPair[1].bindings = bindings;
        newTable = NULL;
        break;
    case IswTableOverride:
        bindPair[0].xlations = xlations;
        bindPair[0].bindings = bindings;
        bindPair[1].xlations = oldXlations;
        bindPair[1].bindings = oldBindings;
        newTable = NULL;
        break;
    }
    if (!newTable)
        newTable =
            MergeThem(widget, bindPair[0].xlations, bindPair[1].xlations);

    for (i = 0, numNew = 0; i < 2; i++) {
        if (bindPair[i].xlations)
            for (j = 0; j < bindPair[i].xlations->numStateTrees; j++, numNew++) {
                if (bindPair[i].xlations->stateTreeTbl[j]->simple.isAccelerator) {
                    if (bindPair[i].bindings)
                        newBindings[numNew] = bindPair[i].bindings[j];
                    else {
                        newBindings[numNew].widget = source;
                        newBindings[numNew].aXlations = bindPair[i].xlations;
                    }
                }
            }
    }
    *numNewRtn = numNew;
    treePtr = &newTable->stateTreeTbl[0];
    for (i = 0; i < newTable->numStateTrees; i++, treePtr++)
        (*treePtr)->simple.refCount++;
    return newTable;
}

static TMBindData
MakeBindData(TMComplexBindProcs bindings,
             TMShortCard numBindings,
             TMBindData oldBindData)
{
    TMLongCard bytes;
    TMShortCard i;
    Boolean isComplex;
    TMBindData bindData;

    if (numBindings == 0)
        return NULL;
    for (i = 0; i < numBindings; i++)
        if (bindings[i].widget)
            break;
    isComplex = (i < numBindings);
    if (isComplex)
        bytes = (sizeof(TMComplexBindDataRec) +
                 ((TMLongCard) (numBindings - 1) *
                  sizeof(TMComplexBindProcsRec)));
    else
        bytes = (sizeof(TMSimpleBindDataRec) +
                 ((TMLongCard) (numBindings - 1) *
                  sizeof(TMSimpleBindProcsRec)));

    bindData =
        (TMBindData) __XtCalloc((Cardinal) sizeof(char), (Cardinal) bytes);
    IswSetBit(bindData->simple.isComplex, isComplex);
    if (isComplex) {
        TMComplexBindData cBindData = (TMComplexBindData) bindData;

        /*
         * If there were any accelerator contexts in the old bindData
         * then propagate them to the new one.
         */
        if (oldBindData && oldBindData->simple.isComplex)
            cBindData->accel_context =
                ((TMComplexBindData) oldBindData)->accel_context;
        memcpy(&cBindData->bindTbl[0], bindings,
               numBindings * sizeof(TMComplexBindProcsRec));
    }
    return bindData;
}

/*
 * This routine is the central clearinghouse for merging translations
 * into a widget. It takes care of preping the action bindings for
 * realize time and calling the converter or doing a straight merge if
 * the destination is empty.
 */
static Boolean
ComposeTranslations(Widget dest,
                    _IswTranslateOp operation,
                    Widget source,
                    IswTranslations newXlations)
{
    IswTranslations newTable, oldXlations;
    IswTranslations accNewXlations;
    EventMask oldMask = 0;
    TMBindData bindData;
    TMComplexBindProcs oldBindings = NULL;
    TMShortCard numOldBindings = 0, numNewBindings = 0, numBytes;
    TMComplexBindProcsRec stackBindings[16], *newBindings;

    /*
     * how should we be handling the refcount decrement for the
     * replaced translation table ???
     */
    if (!newXlations) {
        IswAppWarningMsg(IswWidgetToApplicationContext(dest),
                        IswNtranslationError, "nullTable", IswCIswToolkitError,
                        "table to (un)merge must not be null", NULL, NULL);
        return False;
    }

    accNewXlations = newXlations;
    newXlations = ((newXlations->hasBindings)
                   ? ((ATranslations) newXlations)->xlations : newXlations);

    if (!(oldXlations = dest->core.tm.translations))
        operation = IswTableReplace;

    /*
     * try to avoid generation of duplicate state trees. If the source
     * isn't simple (1 state Tree) then it's too much hassle
     */
    if (((operation == IswTableAugment) ||
         (operation == IswTableOverride)) && (newXlations->numStateTrees == 1)) {
        Cardinal i;

        for (i = 0; i < oldXlations->numStateTrees; i++)
            if (oldXlations->stateTreeTbl[i] == newXlations->stateTreeTbl[0])
                break;
        if (i < oldXlations->numStateTrees) {
            if (operation == IswTableAugment) {
                /*
                 * we don't need to do anything since it's already
                 * there
                 */
                return True;
            }
            else {              /* operation == IswTableOverride */
                /*
                 * We'll get rid of the duplicate trees throughout the
                 * and leave it with a pruned translation table. This
                 * will only work if the same table has been merged
                 * into this table (or one of it's composers
                 */
                _IswUnmergeTranslations(dest, newXlations);
                /*
                 * reset oldXlations so we're back in sync
                 */
                if (!(oldXlations = dest->core.tm.translations))
                    operation = IswTableReplace;
            }
        }
    }

    bindData = (TMBindData) dest->core.tm.proc_table;
    if (bindData) {
        numOldBindings = (oldXlations ? oldXlations->numStateTrees : 0);
        if (bindData->simple.isComplex)
            oldBindings = &((TMComplexBindData) bindData)->bindTbl[0];
        else
            oldBindings = (TMComplexBindProcs)
                (&((TMSimpleBindData) bindData)->bindTbl[0]);
    }

    numBytes =
        (TMShortCard) ((size_t) ((oldXlations ? oldXlations->numStateTrees : 0)
                                 +
                                 newXlations->numStateTrees) *
                       sizeof(TMComplexBindProcsRec));
    newBindings = (TMComplexBindProcs) IswStackAlloc(numBytes, stackBindings);
    IswBZero((char *) newBindings, numBytes);

    if (operation == IswTableUnmerge) {
        newTable = UnmergeTranslations(dest,
                                       oldXlations,
                                       newXlations,
                                       0,
                                       oldBindings, numOldBindings,
                                       newBindings, &numNewBindings);
#ifdef DEBUG
        /* check for no match for unmerge */
        if (newTable == oldXlations) {
            IswWarning("attempt to unmerge invalid table");
            IswStackFree((char *) newBindings, (char *) stackBindings);
            return (newTable != NULL);
        }
#endif                          /* DEBUG */
    }
    else {
        newTable = MergeTranslations(dest,
                                     oldXlations,
                                     accNewXlations,
                                     operation,
                                     source,
                                     oldBindings, newBindings, &numNewBindings);
    }
    if (IswIsRealized(dest)) {
        oldMask = 0;
        if (oldXlations)
            oldMask = oldXlations->eventMask;
        _IswUninstallTranslations(dest);
    }

    dest->core.tm.proc_table =
        (IswActionProc *) MakeBindData(newBindings, numNewBindings, bindData);

    IswFree((char *) bindData);

    dest->core.tm.translations = newTable;

    if (IswIsRealized(dest)) {
        EventMask mask = 0;

        _IswInstallTranslations(dest);
        if (newTable)
            mask = newTable->eventMask;
        if (mask != oldMask){
            uint32_t event_mask = (uint32_t) IswBuildEventMask(dest);
            (void)xcb_change_window_attributes(
                _IswXcbConn(IswDisplayOf(dest)),  /* your XCB connection */
                _IswXcbWindow(IswWindowOf(dest)),  /* window XID */
                XCB_CW_EVENT_MASK,
                &event_mask
            );
        }
            //XSelectInput(IswDisplayOf(dest), IswWindowOf(dest),
            //             (long) IswBuildEventMask(dest));
    }
    IswStackFree((IswPointer) newBindings, (IswPointer) stackBindings);
    return (newTable != NULL);
}

/*
 * If a GetValues is done on a translation resource that contains
 * accelerators we need to return the accelerator context in addition
 * to the pure translations.  Since this means returning memory that
 * the client controls but we still own, we will track the "headers"
 * that we return (via a linked list pointed to from the bindData) and
 * free it at destroy time.
 */
IswTranslations
_IswGetTranslationValue(Widget w)
{
    IswTM tmRecPtr = (IswTM) &w->core.tm;
    ATranslations *aXlationsPtr;
    TMComplexBindData cBindData = (TMComplexBindData) tmRecPtr->proc_table;
    IswTranslations xlations = tmRecPtr->translations;

    if (!xlations || !cBindData || !cBindData->isComplex)
        return xlations;

    /* Walk the list looking to see if we already have generated a
     * header for the currently installed translations.  If we have,
     * just return that header.  Otherwise create a new header.
     */
    for (aXlationsPtr = (ATranslations *) &cBindData->accel_context;
         *aXlationsPtr && (*aXlationsPtr)->xlations != xlations;
         aXlationsPtr = &(*aXlationsPtr)->next);
    if (*aXlationsPtr)
        return (IswTranslations) *aXlationsPtr;
    else {
        /* create a new aXlations context */
        ATranslations aXlations;
        Cardinal numBindings = xlations->numStateTrees;

        (*aXlationsPtr) = aXlations = (ATranslations)
            __XtMalloc((Cardinal) (sizeof(ATranslationData) +
                                   (numBindings -
                                    1) * sizeof(TMComplexBindProcsRec)));

        aXlations->hasBindings = True;
        aXlations->xlations = xlations;
        aXlations->next = NULL;
        memcpy(&aXlations->bindTbl[0],
               &cBindData->bindTbl[0],
               numBindings * sizeof(TMComplexBindProcsRec));
        return (IswTranslations) aXlations;
    }
}

static void
RemoveStateTree(TMStateTree tree _X_UNUSED)
{
#ifdef REFCNT_TRANSLATIONS
    TMComplexStateTree stateTree = (TMComplexStateTree) tree;

    if (--stateTree->refCount == 0) {
        /*
         * should we free/refcount the match recs ?
         */
        if (!stateTree->isSimple) {
            StatePtr currState, nextState;
            TMShortCard i;

            for (i = 0; i < stateTree->numComplexBranchHeads; i++) {
                currState = nextState = stateTree->complexBranchHeadTbl[i];
                for (; nextState;) {
                    FreeActions(currState->actions);
                    currState->actions = NULL;
                    if (!currState->isCycleEnd)
                        nextState = currState->nextLevel;
                    else
                        nextState = NULL;
                    IswFree((char *) currState);
                }
            }
            IswFree((char *) stateTree->complexBranchHeadTbl);
        }
        IswFree((char *) stateTree->branchHeadTbl);
        IswFree((char *) stateTree);
    }
#endif                          /* REFCNT_TRANSLATIONS */
}

void
_IswRemoveStateTreeByIndex(IswTranslations xlations, TMShortCard i)
{
    TMStateTree *stateTrees = xlations->stateTreeTbl;

    RemoveStateTree(stateTrees[i]);
    xlations->numStateTrees--;

    for (; i < xlations->numStateTrees; i++) {
        stateTrees[i] = stateTrees[i + 1];
    }
}

void
_IswFreeTranslations(IswAppContext app,
                    XrmValuePtr toVal,
                    IswPointer closure _X_UNUSED,
                    XrmValuePtr args _X_UNUSED,
                    Cardinal *num_args)
{
    IswTranslations xlations;
    int i;

    if (*num_args != 0)
        IswAppWarningMsg(app,
                        "invalidParameters", "freeTranslations",
                        IswCIswToolkitError,
                        "Freeing IswTranslations requires no extra arguments",
                        NULL, NULL);

    xlations = *(IswTranslations *) toVal->addr;
    for (i = 0; i < (int) xlations->numStateTrees; i++)
        RemoveStateTree(xlations->stateTreeTbl[i]);
    IswFree((char *) xlations);
}

/*  The spec is not clear on when actions specified in accelerators are bound;
 *  Bind them at Realize the same as translations
 */
void
IswInstallAccelerators(Widget destination, Widget source)
{
    IswTranslations aXlations;
    _IswTranslateOp op;

    WIDGET_TO_APPCON(destination);

    /*
     * test that it was parsed as an accelarator table. Even though
     * there doesn't need to be a distinction it makes life easier if
     * we honor the spec implication that aXlations is an accelerator
     */
    LOCK_APP(app);
    LOCK_PROCESS;
    if ((!IswIsWidget(source)) ||
        ((aXlations = source->core.accelerators) == NULL) ||
        (aXlations->stateTreeTbl[0]->simple.isAccelerator == False)) {
        UNLOCK_PROCESS;
        UNLOCK_APP(app);
        return;
    }

    aXlations = source->core.accelerators;
    op = aXlations->operation;

    if (ComposeTranslations(destination, op, source, aXlations) &&
        (IswClass(source)->core_class.display_accelerator != NULL)) {
        _IswString buf = _IswPrintXlations(destination, aXlations, source, False);

        (*(IswClass(source)->core_class.display_accelerator)) (source, buf);
        IswFree(buf);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
IswInstallAllAccelerators(Widget destination, Widget source)
{
    Cardinal i;

    WIDGET_TO_APPCON(destination);

    /* Recurse down normal children */
    LOCK_APP(app);
    LOCK_PROCESS;
    if (IswIsComposite(source)) {
        CompositeWidget cw = (CompositeWidget) source;

        for (i = 0; i < cw->composite.num_children; i++) {
            IswInstallAllAccelerators(destination, cw->composite.children[i]);
        }
    }

    /* Recurse down popup children */
    if (IswIsWidget(source)) {
        for (i = 0; i < source->core.num_popups; i++) {
            IswInstallAllAccelerators(destination, source->core.popup_list[i]);
        }
    }
    /* Finally, apply procedure to this widget */
    IswInstallAccelerators(destination, source);
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

#if 0                           /* dead code */
static _IswTranslateOp
_IswGetTMOperation(IswTranslations xlations)
{
    return ((xlations->hasBindings)
            ? ((ATranslations) xlations)->xlations->operation
            : xlations->operation);
}
#endif

void
IswAugmentTranslations(Widget widget, IswTranslations new)
{
    Widget hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    (void) ComposeTranslations(widget, IswTableAugment, (Widget) NULL, new);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHaugmentTranslations;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
IswOverrideTranslations(Widget widget, IswTranslations new)
{
    Widget hookobj;

    WIDGET_TO_APPCON(widget);

    LOCK_APP(app);
    LOCK_PROCESS;
    (void) ComposeTranslations(widget, IswTableOverride, (Widget) NULL, new);
    hookobj = IswHooksOfDisplay(IswDisplayOfObject(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHoverrideTranslations;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

void
_IswMergeTranslations(Widget widget,
                     IswTranslations newXlations,
                     _IswTranslateOp op)
{
    if (!newXlations) {
        if (!widget->core.tm.translations)
            return;
        else {
            newXlations = widget->core.tm.translations;
            widget->core.tm.translations = NULL;
        }
    }
    (void) ComposeTranslations(widget, op, (Widget) NULL, newXlations);
}

void
_IswUnmergeTranslations(Widget widget, IswTranslations xlations)
{
    ComposeTranslations(widget, IswTableUnmerge, (Widget) NULL, xlations);
}
