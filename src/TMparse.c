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
#include <ISW/ISWPlatform.h>
#include "StringDefs.h"
#include <ctype.h>
#include <stdlib.h>

#ifdef CACHE_TRANSLATIONS
#ifdef REFCNT_TRANSLATIONS
#define CACHED IswCacheAll | IswCacheRefCount
#else
#define CACHED IswCacheAll
#endif
#else
#define CACHED IswCacheNone
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

static _Xconst char *IswNtranslationParseError = "translationParseError";

/* The "Any" modifier wildcard ("Any<Btn1Down>") matches regardless of the
   modifier state.  It is a translation-manager-private sentinel that must not
   collide with the real IswModMask bits (0..12) or the other TM-private bits
   (KeysymModMask 1<<27, AnyButtonMask 1<<28); bit 15 is free. */
#define TM_ANY_MODIFIER (1L << 15)

typedef int EventType;

#define PARSE_PROC_DECL String, Opaque, EventPtr, Boolean*

typedef String(*ParseProc) (String /* str; */ ,
                            Opaque /* closure; */ ,
                            EventPtr /* event; */ ,
                            Boolean * /* error */ );

typedef TMShortCard Value;
typedef void (*ModifierProc) (Value, LateBindingsPtr *, Boolean, Value *);

typedef struct _ModifierRec {
    const char *name;
    IswQuark signature;
    ModifierProc modifierParseProc;
    Value value;
} ModifierRec, *ModifierKeys;

typedef struct _EventKey {
    const char *event;
    IswQuark signature;
    EventType eventType;
    ParseProc parseDetail;
    Opaque closure;
} EventKey, *EventKeys;

typedef struct {
    const char *name;
    IswQuark signature;
    Value value;
} NameValueRec, *NameValueTable;

static void ParseModImmed(Value, LateBindingsPtr *, Boolean, Value *);
static Boolean _IswLookupModifier(IswQuark, LateBindingsPtr *, Boolean, Value *, Bool);
static String PanicModeRecovery(String);
static String CheckForPoundSign(String, _IswTranslateOp, _IswTranslateOp *);
static uint32_t StringToKeySym(String, Boolean *);
/* *INDENT-OFF* */
static ModifierRec modifiers[] = {
    {"Shift",   0,      ParseModImmed, IswModShift},
    {"Lock",    0,      ParseModImmed, IswModLock},
    {"Ctrl",    0,      ParseModImmed, IswModControl},
    {"Mod1",    0,      ParseModImmed, IswModMod1},
    {"Mod2",    0,      ParseModImmed, IswModMod2},
    {"Mod3",    0,      ParseModImmed, IswModMod3},
    {"Mod4",    0,      ParseModImmed, IswModMod4},
    {"Mod5",    0,      ParseModImmed, IswModMod5},
    {"Meta",    0,      ParseModImmed, IswModMeta},
    {"m",       0,      ParseModImmed, IswModMeta},
    {"h",       0,      ParseModImmed, IswModHyper},
    {"su",      0,      ParseModImmed, IswModSuper},
    {"a",       0,      ParseModImmed, IswModAlt},
    {"Hyper",   0,      ParseModImmed, IswModHyper},
    {"Super",   0,      ParseModImmed, IswModSuper},
    {"Alt",     0,      ParseModImmed, IswModAlt},
    {"Button1", 0,      ParseModImmed, IswModButton1},
    {"Button2", 0,      ParseModImmed, IswModButton2},
    {"Button3", 0,      ParseModImmed, IswModButton3},
    {"Button4", 0,      ParseModImmed, IswModButton4},
    {"Button5", 0,      ParseModImmed, IswModButton5},
    {"c",       0,      ParseModImmed, IswModControl},
    {"s",       0,      ParseModImmed, IswModShift},
    {"l",       0,      ParseModImmed, IswModLock},
};

static NameValueRec motionDetails[] = {
    {"Normal",              0,         0}, /* neutral motion has no Hint detail */
    {NULL,                  ISW_NULLQUARK, 0},
};

static NameValueRec notifyModes[] = {
    {"Normal",              0,         IswNotifyNormal},
    {"Grab",                0,         IswNotifyGrab},
    {"Ungrab",              0,         IswNotifyUngrab},
    {NULL,                  ISW_NULLQUARK, 0},
};

/* *INDENT-ON* */

static String ParseKeySym(PARSE_PROC_DECL);
static String ParseKeyAndModifiers(PARSE_PROC_DECL);
static String ParseTable(PARSE_PROC_DECL);
static String ParseButton(PARSE_PROC_DECL);
static String ParseImmed(PARSE_PROC_DECL);
static String ParseAddModifier(PARSE_PROC_DECL);
static String ParseNone(PARSE_PROC_DECL);
static String ParseProtocolName(PARSE_PROC_DECL);

/* *INDENT-OFF* */
static EventKey events[] = {

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

{"KeyPress",        ISW_NULLQUARK, IswKeyDown,          ParseKeySym,    NULL},
{"Key",             ISW_NULLQUARK, IswKeyDown,          ParseKeySym,    NULL},
{"KeyDown",         ISW_NULLQUARK, IswKeyDown,          ParseKeySym,    NULL},
{"Ctrl",            ISW_NULLQUARK, IswKeyDown,          ParseKeyAndModifiers, (Opaque)IswModControl},
{"Shift",           ISW_NULLQUARK, IswKeyDown,          ParseKeyAndModifiers, (Opaque)IswModShift},
{"Meta",            ISW_NULLQUARK, IswKeyDown,          ParseKeyAndModifiers, (Opaque)NULL},
{"KeyUp",           ISW_NULLQUARK, IswKeyUp,            ParseKeySym,    NULL},
{"KeyRelease",      ISW_NULLQUARK, IswKeyUp,            ParseKeySym,    NULL},

{"ButtonPress",     ISW_NULLQUARK, IswButtonDown,       ParseButton, NULL },
{"BtnDown",         ISW_NULLQUARK, IswButtonDown,       ParseButton, NULL },
{"Btn1Down",        ISW_NULLQUARK, IswButtonDown,       ParseImmed, (Opaque)IswButtonLeft},
{"Btn2Down",        ISW_NULLQUARK, IswButtonDown,       ParseImmed, (Opaque)IswButtonMiddle},
{"Btn3Down",        ISW_NULLQUARK, IswButtonDown,       ParseImmed, (Opaque)IswButtonRight},
{"Btn4Down",        ISW_NULLQUARK, IswButtonDown,       ParseImmed, (Opaque)IswButtonWheelUp},
{"Btn5Down",        ISW_NULLQUARK, IswButtonDown,       ParseImmed, (Opaque)IswButtonWheelDown},

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

{"ButtonRelease",   ISW_NULLQUARK, IswButtonUp,         ParseButton, NULL },
{"BtnUp",           ISW_NULLQUARK, IswButtonUp,         ParseButton, NULL },
{"Btn1Up",          ISW_NULLQUARK, IswButtonUp,         ParseImmed, (Opaque)IswButtonLeft},
{"Btn2Up",          ISW_NULLQUARK, IswButtonUp,         ParseImmed, (Opaque)IswButtonMiddle},
{"Btn3Up",          ISW_NULLQUARK, IswButtonUp,         ParseImmed, (Opaque)IswButtonRight},
{"Btn4Up",          ISW_NULLQUARK, IswButtonUp,         ParseImmed, (Opaque)IswButtonWheelUp},
{"Btn5Up",          ISW_NULLQUARK, IswButtonUp,         ParseImmed, (Opaque)IswButtonWheelDown},

{"MotionNotify",    ISW_NULLQUARK, IswMotion,           ParseTable, (Opaque)motionDetails},
{"PtrMoved",        ISW_NULLQUARK, IswMotion,           ParseTable, (Opaque)motionDetails},
{"Motion",          ISW_NULLQUARK, IswMotion,           ParseTable, (Opaque)motionDetails},
{"MouseMoved",      ISW_NULLQUARK, IswMotion,           ParseTable, (Opaque)motionDetails},
{"BtnMotion",       ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)AnyButtonMask},
{"Btn1Motion",      ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)IswModButton1},
{"Btn2Motion",      ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)IswModButton2},
{"Btn3Motion",      ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)IswModButton3},
{"Btn4Motion",      ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)IswModButton4},
{"Btn5Motion",      ISW_NULLQUARK, IswMotion,           ParseAddModifier, (Opaque)IswModButton5},

{"EnterNotify",     ISW_NULLQUARK, IswEnter,            ParseTable, (Opaque)notifyModes},
{"Enter",           ISW_NULLQUARK, IswEnter,            ParseTable, (Opaque)notifyModes},
{"EnterWindow",     ISW_NULLQUARK, IswEnter,            ParseTable, (Opaque)notifyModes},

{"LeaveNotify",     ISW_NULLQUARK, IswLeave,            ParseTable, (Opaque)notifyModes},
{"LeaveWindow",     ISW_NULLQUARK, IswLeave,            ParseTable, (Opaque)notifyModes},
{"Leave",           ISW_NULLQUARK, IswLeave,            ParseTable, (Opaque)notifyModes},

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

{"FocusIn",         ISW_NULLQUARK, IswFocusIn,          ParseTable, (Opaque)notifyModes},

{"FocusOut",        ISW_NULLQUARK, IswFocusOut,         ParseTable, (Opaque)notifyModes},

{"Expose",          ISW_NULLQUARK, IswRedraw,           ParseNone,      NULL},

{"VisibilityNotify",ISW_NULLQUARK, IswVisibility,       ParseNone,      NULL},
{"Visible",         ISW_NULLQUARK, IswVisibility,       ParseNone,      NULL},

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

{"DestroyNotify",   ISW_NULLQUARK, IswDestroy,          ParseNone,      NULL},
{"Destroy",         ISW_NULLQUARK, IswDestroy,          ParseNone,      NULL},

{"UnmapNotify",     ISW_NULLQUARK, IswUnmap,            ParseNone,      NULL},
{"Unmap",           ISW_NULLQUARK, IswUnmap,            ParseNone,      NULL},

{"MapNotify",       ISW_NULLQUARK, IswMap,              ParseNone,      NULL},
{"Map",             ISW_NULLQUARK, IswMap,              ParseNone,      NULL},

{"ReparentNotify",  ISW_NULLQUARK, IswReparent,         ParseNone,      NULL},
{"Reparent",        ISW_NULLQUARK, IswReparent,         ParseNone,      NULL},

{"ConfigureNotify", ISW_NULLQUARK, IswGeometry,         ParseNone,      NULL},
{"Configure",       ISW_NULLQUARK, IswGeometry,         ParseNone,      NULL},

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

{"ClientMessage",   ISW_NULLQUARK, IswProtocol,         ParseProtocolName,      NULL},
{"Message",         ISW_NULLQUARK, IswProtocol,         ParseProtocolName,      NULL},

{"WindowClose",     ISW_NULLQUARK, IswWindowClose,      ParseNone,      NULL},

{"MappingNotify",   ISW_NULLQUARK, IswMappingChanged,   ParseNone,      NULL},
{"Mapping",         ISW_NULLQUARK, IswMappingChanged,   ParseNone,      NULL},


#ifdef DEBUG
# ifdef notdef
{"Timer",           ISW_NULLQUARK, _IswTimerEventType, ParseNone,     NULL},
{"EventTimer",      ISW_NULLQUARK, _IswEventTimerEventType, ParseNone,NULL},
# endif /* notdef */
#endif /* DEBUG */

/* Event Name,    Quark, Event Type,    Detail Parser, Closure */

};
/* *INDENT-ON* */

#define IsNewline(str) ((str) == '\n')

#define ScanFor(str, ch) \
    while ((*(str) != (ch)) && (*(str) != '\0') && !IsNewline(*(str))) (str)++

#define ScanNumeric(str)  while ('0' <= *(str) && *(str) <= '9') (str)++

#define ScanAlphanumeric(str) \
    while (('A' <= *(str) && *(str) <= 'Z') || \
           ('a' <= *(str) && *(str) <= 'z') || \
           ('0' <= *(str) && *(str) <= '9')) (str)++

#define ScanWhitespace(str) \
    while (*(str) == ' ' || *(str) == '\t') (str)++

static Boolean initialized = FALSE;
static IswQuark QMeta;
static IswQuark QCtrl;
static IswQuark QNone;
static IswQuark QAny;

static void
FreeEventSeq(EventSeqPtr eventSeq)
{
    register EventSeqPtr evs = eventSeq;

    while (evs != NULL) {
        evs->state = (StatePtr) evs;
        if (evs->next != NULL && evs->next->state == (StatePtr) evs->next)
            evs->next = NULL;
        evs = evs->next;
    }

    evs = eventSeq;
    while (evs != NULL) {
        register EventPtr event = evs;

        evs = evs->next;
        if (evs == event)
            evs = NULL;
        IswFree((char *) event);
    }
}

static void
CompileNameValueTable(NameValueTable table)
{
    register int i;

    for (i = 0; table[i].name; i++)
        table[i].signature = IswPermStringToQuark(table[i].name);
}

static int
OrderEvents(_Xconst void *a, _Xconst void *b)
{
    return ((((_Xconst EventKey *) a)->signature <
             ((_Xconst EventKey *) b)->signature) ? -1 : 1);
}

static void
Compile_XtEventTable(EventKeys table, Cardinal count)
{
    register int i;
    register EventKeys entry = table;

    for (i = (int) count; --i >= 0; entry++)
        entry->signature = IswPermStringToQuark(entry->event);
    qsort(table, count, sizeof(EventKey), OrderEvents);
}

static int
OrderModifiers(_Xconst void *a, _Xconst void *b)
{
    return ((((_Xconst ModifierRec *) a)->signature <
             ((_Xconst ModifierRec *) b)->signature) ? -1 : 1);
}

static void
Compile_XtModifierTable(ModifierKeys table, Cardinal count)
{
    register int i;
    register ModifierKeys entry = table;

    for (i = (int) count; --i >= 0; entry++)
        entry->signature = IswPermStringToQuark(entry->name);
    qsort(table, count, sizeof(ModifierRec), OrderModifiers);
}

static String
PanicModeRecovery(String str)
{
    ScanFor(str, '\n');
    if (*str == '\n')
        str++;
    return str;

}

static void
Syntax(_Xconst char *str0, _Xconst char *str1)
{
    Cardinal num_params = 2;
    String params[2];

    params[0] = (String) str0;
    params[1] = (String) str1;
    IswWarningMsg(IswNtranslationParseError, "parseError", IswCIswToolkitError,
                 "translation table syntax error: %s %s", params, &num_params);
}

static Cardinal
LookupTMEventType(String eventStr, Boolean *error)
{
    register int i = 0, left, right;
    register IswQuark signature;
    static int previous = 0;

    LOCK_PROCESS;
    if ((signature = StringToQuark(eventStr)) == events[previous].signature) {
        UNLOCK_PROCESS;
        return (Cardinal) previous;
    }

    left = 0;
    right = IswNumber(events) - 1;
    while (left <= right) {
        i = (left + right) >> 1;
        if (signature < events[i].signature)
            right = i - 1;
        else if (signature > events[i].signature)
            left = i + 1;
        else {
            previous = i;
            UNLOCK_PROCESS;
            return (Cardinal) i;
        }
    }

    Syntax("Unknown event type :  ", eventStr);
    *error = TRUE;
    UNLOCK_PROCESS;
    return (Cardinal) i;
}

/*
 * The "@keysym" modifier syntax historically resolved an arbitrary keysym to
 * a modifier mask at match time via the keyboard map (late bindings).  In the
 * neutral model the toolkit no longer carries keysyms, and the late-binding
 * resolver (_IswComputeLateBindings) has no neutral keysym to map.  We
 * therefore resolve "@name" directly to its IswModMask bit through the
 * modifiers[] table, the same way a bare modifier name resolves; an unknown
 * name is a parse error.  (This drops the rarely-used ability to name a raw
 * keysym that is not one of the known modifier names.)
 */
static void
_IswParseKeysymMod(String name,
                  LateBindingsPtr *lateBindings,
                  Boolean notFlag,
                  Value *valueP,
                  Boolean *error)
{
    IswQuark signature = IswStringToQuark(name);

    *valueP = 0;
    if (!_IswLookupModifier(signature, lateBindings, notFlag, valueP, FALSE)) {
        Syntax("Unknown modifier keysym name:  ", name);
        *error = TRUE;
    }
}

static Boolean
_IswLookupModifier(IswQuark signature,
                  LateBindingsPtr *lateBindings,
                  Boolean notFlag,
                  Value *valueP,
                  Bool constMask)
{
    int left, right;
    static int previous = 0;

    LOCK_PROCESS;
    if (signature == modifiers[previous].signature) {
        if (constMask)
            *valueP = modifiers[previous].value;
        else                    /* if (modifiers[previous].modifierParseProc) always true */
            (*modifiers[previous].modifierParseProc)
                (modifiers[previous].value, lateBindings, notFlag, valueP);
        UNLOCK_PROCESS;
        return TRUE;
    }

    left = 0;
    right = IswNumber(modifiers) - 1;
    while (left <= right) {
        int i = (left + right) >> 1;

        if (signature < modifiers[i].signature)
            right = i - 1;
        else if (signature > modifiers[i].signature)
            left = i + 1;
        else {
            previous = i;
            if (constMask)
                *valueP = modifiers[i].value;
            else                /* if (modifiers[i].modifierParseProc) always true */
                (*modifiers[i].modifierParseProc)
                    (modifiers[i].value, lateBindings, notFlag, valueP);
            UNLOCK_PROCESS;
            return TRUE;
        }
    }
    UNLOCK_PROCESS;
    return FALSE;
}

static String
ScanIdent(register String str)
{
    ScanAlphanumeric(str);
    while (('A' <= *str && *str <= 'Z')
           || ('a' <= *str && *str <= 'z')
           || ('0' <= *str && *str <= '9')
           || (*str == '-')
           || (*str == '_')
           || (*str == '$')
        )
        str++;
    return str;
}

static String
FetchModifierToken(String str, IswQuark *token_return)
{
    String start = str;

    if (*str == '$') {
        *token_return = QMeta;
        str++;
    }
    else if (*str == '^') {
        *token_return = QCtrl;
        str++;
    }
    else {
        str = ScanIdent(str);
        if (start != str) {
            char modStrbuf[100];
            char *modStr;

            modStr = IswStackAlloc((size_t) (str - start + 1), modStrbuf);
            if (modStr == NULL)
                _IswAllocError(NULL);
            (void) memcpy(modStr, start, (size_t) (str - start));
            modStr[str - start] = '\0';
            *token_return = IswStringToQuark(modStr);
            IswStackFree(modStr, modStrbuf);
        }
    }
    return str;
}

static String
ParseModifiers(register String str, EventPtr event, Boolean *error)
{
    register String start;
    Boolean notFlag, exclusive, keysymAsMod;
    Value maskBit;
    IswQuark Qmod = QNone;

    ScanWhitespace(str);
    start = str;
    str = FetchModifierToken(str, &Qmod);
    exclusive = FALSE;
    if (start != str) {
        if (Qmod == QNone) {
            event->event.modifierMask = (unsigned long) (~0);
            event->event.modifiers = 0;
            ScanWhitespace(str);
            return str;
        }
        else if (Qmod == QAny) {        /*backward compatibility */
            event->event.modifierMask = 0;
            event->event.modifiers = TM_ANY_MODIFIER;
            ScanWhitespace(str);
            return str;
        }
        str = start;            /*if plain modifier, reset to beginning */
    }
    else
        while (*str == '!' || *str == ':') {
            if (*str == '!') {
                exclusive = TRUE;
                str++;
                ScanWhitespace(str);
            }
            if (*str == ':') {
                event->event.standard = TRUE;
                str++;
                ScanWhitespace(str);
            }
        }

    while (*str != '<') {
        if (*str == '~') {
            notFlag = TRUE;
            str++;
        }
        else
            notFlag = FALSE;
        if (*str == '@') {
            keysymAsMod = TRUE;
            str++;
        }
        else
            keysymAsMod = FALSE;
        start = str;
        str = FetchModifierToken(str, &Qmod);
        if (start == str) {
            Syntax("Modifier or '<' expected", "");
            *error = TRUE;
            return PanicModeRecovery(str);
        }
        if (keysymAsMod) {
            _IswParseKeysymMod(IswQuarkToString(Qmod),
                              &event->event.lateModifiers,
                              notFlag, &maskBit, error);
            if (*error)
                return PanicModeRecovery(str);

        }
        else if (!_IswLookupModifier(Qmod, &event->event.lateModifiers,
                                    notFlag, &maskBit, FALSE)) {
            Syntax("Unknown modifier name:  ", IswQuarkToString(Qmod));
            *error = TRUE;
            return PanicModeRecovery(str);
        }
        event->event.modifierMask |= maskBit;
        if (notFlag)
            event->event.modifiers =
                (event->event.modifiers & (TMLongCard) (~maskBit));
        else
            event->event.modifiers |= maskBit;
        ScanWhitespace(str);
    }
    if (exclusive)
        event->event.modifierMask = (unsigned long) (~0);
    return str;
}

static String
ParseIswEventType(register String str,
                 EventPtr event,
                 Cardinal *tmEventP,
                 Boolean *error)
{
    String start = str;
    char eventTypeStrbuf[100];
    char *eventTypeStr;

    ScanAlphanumeric(str);
    eventTypeStr = IswStackAlloc((size_t) (str - start + 1), eventTypeStrbuf);
    if (eventTypeStr == NULL)
        _IswAllocError(NULL);
    (void) memcpy(eventTypeStr, start, (size_t) (str - start));
    eventTypeStr[str - start] = '\0';
    *tmEventP = LookupTMEventType(eventTypeStr, error);
    IswStackFree(eventTypeStr, eventTypeStrbuf);
    if (*error)
        return PanicModeRecovery(str);
    event->event.eventType = (TMLongCard) events[*tmEventP].eventType;
    return str;
}

static unsigned long
StrToHex(String str)
{
    register char c;
    register unsigned long val = 0;

    while ((c = *str)) {
        if ('0' <= c && c <= '9')
            val = (unsigned long) (val * 16 + (unsigned long) c - '0');
        else if ('a' <= c && c <= 'z')
            val = (unsigned long) (val * 16 + (unsigned long) c - 'a' + 10);
        else if ('A' <= c && c <= 'Z')
            val = (unsigned long) (val * 16 + (unsigned long) c - 'A' + 10);
        else
            return 0;
        str++;
    }

    return val;
}

static unsigned long
StrToOct(String str)
{
    register char c;
    register unsigned long val = 0;

    while ((c = *str)) {
        if ('0' <= c && c <= '7')
            val = val * 8 + (unsigned long) c - '0';
        else
            return 0;
        str++;
    }

    return val;
}

static unsigned long
StrToNum(String str)
{
    register char c;
    register unsigned long val = 0;

    if (*str == '0') {
        str++;
        if (*str == 'x' || *str == 'X')
            return StrToHex(++str);
        else
            return StrToOct(str);
    }

    while ((c = *str)) {
        if ('0' <= c && c <= '9')
            val = val * 10 + (unsigned long) c - '0';
        else
            return 0;
        str++;
    }

    return val;
}

/* Resolve a key name to its neutral key identity (IswKey enum value or a
   Unicode code point for printable keys) — the same vocabulary a dispatched
   IswEvent carries in key.key.  Leading-digit names stay numeric for
   backward-compatible "<Key>0x..." forms. */
static uint32_t
StringToKeySym(String str, Boolean *error)
{
    uint32_t key;

    if (str == NULL || *str == '\0')
        return IswKeyNone;

    if ('0' <= *str && *str <= '9')
        return (uint32_t) StrToNum(str);

    key = _IswPlatformKeyFromName(str);
    if (key == IswKeyNone) {
        Syntax("Unknown keysym name: ", str);
        *error = TRUE;
        return IswKeyNone;
    }
    return key;
}

static void
ParseModImmed(Value value,
              LateBindingsPtr *lateBindings _X_UNUSED,
              Boolean notFlag _X_UNUSED,
              Value *valueP)
{
    *valueP = value;
}

#ifdef sparc
/*
 * The stupid optimizer in SunOS 4.0.3 and below generates bogus code that
 * causes the value of the most recently used variable to be returned instead
 * of the value passed in.
 */
static String stupid_optimizer_kludge;

#define BROKEN_OPTIMIZER_HACK(val) stupid_optimizer_kludge = (val)
#else
#define BROKEN_OPTIMIZER_HACK(val) val
#endif

static String
ParseImmed(register String str,
           register Opaque closure,
           register EventPtr event,
           Boolean *error _X_UNUSED)
{
    event->event.eventCode = (unsigned long) closure;
    event->event.eventCodeMask = (unsigned long) (~0UL);

    return BROKEN_OPTIMIZER_HACK(str);
}

static String
ParseAddModifier(register String str,
                 register Opaque closure,
                 register EventPtr event,
                 Boolean *error _X_UNUSED)
{
    register unsigned long modval = (unsigned long) closure;

    event->event.modifiers |= modval;
    if (modval != AnyButtonMask)        /* AnyButtonMask is don't-care mask */
        event->event.modifierMask |= modval;

    return BROKEN_OPTIMIZER_HACK(str);
}

static String
ParseKeyAndModifiers(String str,
                     Opaque closure,
                     EventPtr event,
                     Boolean *error)
{
    str = ParseKeySym(str, closure, event, error);
    if ((unsigned long) closure == 0) {
        /* "Meta<Key>..." — resolve Meta to its neutral modifier bit. */
        Value metaMask = 0;

        if (_IswLookupModifier(QMeta, &event->event.lateModifiers, FALSE,
                              &metaMask, FALSE)) {
            event->event.modifiers |= metaMask;
            event->event.modifierMask |= metaMask;
        }
    }
    else {
        event->event.modifiers |= (unsigned long) closure;
        event->event.modifierMask |= (unsigned long) closure;
    }
    return str;
}

static String
ParseKeySym(register String str,
            Opaque closure _X_UNUSED,
            EventPtr event,
            Boolean *error)
{
    String start;
    char keySymNamebuf[100];
    char *keySymName = NULL;

    ScanWhitespace(str);

    if (*str == '\\') {
        keySymName = keySymNamebuf;
        str++;
        keySymName[0] = *str;
        if (*str != '\0' && !IsNewline(*str))
            str++;
        keySymName[1] = '\0';
        event->event.eventCode = StringToKeySym(keySymName, error);
        event->event.eventCodeMask = (unsigned long) (~0L);
    }
    else if (*str == ',' || *str == ':' ||
             /* allow leftparen to be single char symbol,
              * for backwards compatibility
              */
             (*str == '(' && *(str + 1) >= '0' && *(str + 1) <= '9')) {
        keySymName = keySymNamebuf;     /* just so we can stackfree it later */
        keySymName[0] = '\0';
        /* no detail */
        event->event.eventCode = 0L;
        event->event.eventCodeMask = 0L;
    }
    else {
        start = str;
        while (*str != ','
               && *str != ':' && *str != ' ' && *str != '\t' && !IsNewline(*str)
               && (*str != '(' || *(str + 1) <= '0' || *(str + 1) >= '9')
               && *str != '\0')
            str++;
        keySymName = IswStackAlloc((size_t) (str - start + 1), keySymNamebuf);
        (void) memcpy(keySymName, start, (size_t) (str - start));
        keySymName[str - start] = '\0';
        event->event.eventCode = StringToKeySym(keySymName, error);
        event->event.eventCodeMask = (unsigned long) (~0L);
    }
    if (*error && keySymName) {
        /* We never get here when keySymName hasn't been allocated */
        if (keySymName[0] == '<') {
            /* special case for common error */
            IswWarningMsg(IswNtranslationParseError, "missingComma",
                         IswCIswToolkitError,
                         "... possibly due to missing ',' in event sequence.",
                         (String *) NULL, (Cardinal *) NULL);
        }
        IswStackFree(keySymName, keySymNamebuf);
        return PanicModeRecovery(str);
    }
    /* Neutral key identity is already resolved at translate time, so there is
       no keycode->keysym resolution to do at match time.  The key matcher still
       case-folds letters so "<Key>A" matches a lowercase 'a' key event, the way
       the X standard-mods matcher did. */
    event->event.matchEvent = _IswMatchUsingStandardMods;

    IswStackFree(keySymName, keySymNamebuf);

    return str;
}

static String
ParseTable(register String str, Opaque closure, EventPtr event, Boolean *error)
{
    register String start = str;
    register IswQuark signature;
    NameValueTable table = (NameValueTable) closure;
    char tableSymName[100];

    event->event.eventCode = 0L;
    ScanAlphanumeric(str);
    if (str == start) {
        event->event.eventCodeMask = 0L;
        return str;
    }
    if (str - start >= 99) {
        Syntax("Invalid Detail Type (string is too long).", "");
        *error = TRUE;
        return str;
    }
    (void) memcpy(tableSymName, start, (size_t) (str - start));
    tableSymName[str - start] = '\0';
    signature = StringToQuark(tableSymName);
    for (; table->signature != ISW_NULLQUARK; table++)
        if (table->signature == signature) {
            event->event.eventCode = table->value;
            event->event.eventCodeMask = (unsigned long) (~0L);
            return str;
        }

    Syntax("Unknown Detail Type:  ", tableSymName);
    *error = TRUE;
    return PanicModeRecovery(str);
}

static String
ParseButton(String str, Opaque closure _X_UNUSED, EventPtr event, Boolean *error)
{
    String start = str;
    char buttonStr[7];
    size_t len;
    static const char buttonPrefix[] = "Button";
    unsigned long button;

    event->event.eventCode = 0L;
    if (strncmp(str, buttonPrefix, sizeof(buttonPrefix)-1) != 0) {
	event->event.eventCodeMask = 0L;
	return str;
    }
    str += sizeof(buttonPrefix)-1;
    start = str;
    ScanNumeric(str);
    if (str == start) {
	Syntax("Missing button number", "");
	*error = TRUE;
	return PanicModeRecovery(str);
    }
    len = (size_t) (str - start);
    if (len >= sizeof buttonStr) {
	Syntax("Button number too long", "");
	*error = TRUE;
	return PanicModeRecovery(str);
    }
    (void) memcpy(buttonStr, start, len);
    buttonStr[len] = '\0';
    button = StrToNum(buttonStr);
    if (button < 1 || 255 < button) {
	Syntax("Invalid button number", buttonStr);
	*error = TRUE;
	return PanicModeRecovery(str);
    }
    event->event.eventCode = button;
    event->event.eventCodeMask = (unsigned long) (~0L);
    return str;
}

static String
ParseNone(String str,
          Opaque closure _X_UNUSED,
          EventPtr event,
          Boolean *error _X_UNUSED)
{
    event->event.eventCode = 0;
    event->event.eventCodeMask = 0;

    return BROKEN_OPTIMIZER_HACK(str);
}

static String
ParseProtocolName(String str, Opaque closure _X_UNUSED, EventPtr event, Boolean *error)
{
    ScanWhitespace(str);

    if (*str == ',' || *str == ':') {
        /* no detail */
        event->event.eventCode = 0L;
        event->event.eventCodeMask = 0L;
    }
    else {
        String start;
        char protocolName[1000];

        start = str;
        while (*str != ','
               && *str != ':' && *str != ' ' && *str != '\t' && !IsNewline(*str)
               && *str != '\0')
            str++;
        if (str - start >= 999) {
            Syntax("Protocol name must be less than 1000 characters long.", "");
            *error = TRUE;
            return str;
        }
        (void) memcpy(protocolName, start, (size_t) (str - start));
        protocolName[str - start] = '\0';
        event->event.eventCode = (TMLongCard) IswStringToQuark(protocolName);
        event->event.matchEvent = _IswMatchProtocolName;
    }
    return str;
}

static ModifierMask buttonModifierMasks[] = {
    0, IswModButton1, IswModButton2, IswModButton3, IswModButton4, IswModButton5
};

static String ParseRepeat(String, int *, Boolean *, Boolean *);

static String
ParseEvent(register String str,
           EventPtr event, int *reps,
           Boolean *plus,
           Boolean *error)
{
    Cardinal tmEvent;

    str = ParseModifiers(str, event, error);
    if (*error)
        return str;
    if (*str != '<') {
        Syntax("Missing '<' while parsing event type.", "");
        *error = TRUE;
        return PanicModeRecovery(str);
    }
    else
        str++;
    str = ParseIswEventType(str, event, &tmEvent, error);
    if (*error)
        return str;
    if (*str != '>') {
        Syntax("Missing '>' while parsing event type", "");
        *error = TRUE;
        return PanicModeRecovery(str);
    }
    else
        str++;
    if (*str == '(') {
        str = ParseRepeat(str, reps, plus, error);
        if (*error)
            return str;
    }
    str =
        (*(events[tmEvent].parseDetail)) (str, events[tmEvent].closure, event,
                                          error);
    if (*error)
        return str;

/* gross hack! ||| this kludge is related to the X11 protocol deficiency w.r.t.
 * modifiers in grabs.
 */
    if ((event->event.eventType == IswButtonUp)
        && (event->event.modifiers | event->event.modifierMask) /* any */
        &&(event->event.modifiers != TM_ANY_MODIFIER)) {
        event->event.modifiers = (event->event.modifiers
                                  | (TMLongCard) buttonModifierMasks[event->
                                                                     event.
                                                                     eventCode]);
        /* the button that is going up will always be in the modifiers... */
    }

    return str;
}

static String
ParseQuotedStringEvent(register String str,
                       register EventPtr event,
                       Boolean *error)
{
    Value metaMask;
    char s[2];

    if (*str == '^') {
        str++;
        event->event.modifiers = IswModControl;
    }
    else if (*str == '$') {
        str++;
        if (_IswLookupModifier(QMeta, &event->event.lateModifiers, FALSE,
                              &metaMask, FALSE)) {
            event->event.modifiers |= metaMask;
            event->event.modifierMask |= metaMask;
        }
    }
    if (*str == '\\')
        str++;
    s[0] = *str;
    s[1] = '\0';
    if (*str != '\0' && !IsNewline(*str))
        str++;
    event->event.eventType = IswKeyDown;
    event->event.eventCode = StringToKeySym(s, error);
    if (*error)
        return PanicModeRecovery(str);
    event->event.eventCodeMask = (unsigned long) (~0L);
    event->event.matchEvent = _IswMatchUsingStandardMods;
    event->event.standard = TRUE;

    return str;
}

static EventSeqRec timerEventRec = {
    {0, 0, NULL, _IswEventTimerEventType, 0L, 0L, NULL, False},
    /* (StatePtr) -1 */ NULL,
    NULL,
    NULL
};

static void
RepeatDown(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    EventRec upEventRec;
    register EventPtr event, downEvent;
    EventPtr upEvent = &upEventRec;
    register int i;

    downEvent = event = *eventP;
    *upEvent = *downEvent;
    upEvent->event.eventType = ((event->event.eventType == IswButtonDown) ?
                                IswButtonUp : IswKeyUp);
    if ((upEvent->event.eventType == IswButtonUp)
        && (upEvent->event.modifiers != TM_ANY_MODIFIER)
        && (upEvent->event.modifiers | upEvent->event.modifierMask))
        upEvent->event.modifiers = (upEvent->event.modifiers
                                    | (TMLongCard) buttonModifierMasks[event->
                                                                       event.
                                                                       eventCode]);

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count +
                              (reps - 1) * 2);

    for (i = 1; i < reps; i++) {

        /* up */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *upEvent;

        /* timer */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = timerEventRec;

        /* down */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *downEvent;

    }

    event->next = NULL;
    *eventP = event;
    *actionsP = &event->actions;
}

static void
RepeatDownPlus(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    EventRec upEventRec;
    register EventPtr event, downEvent, lastDownEvent = NULL;
    EventPtr upEvent = &upEventRec;
    register int i;

    downEvent = event = *eventP;
    *upEvent = *downEvent;
    upEvent->event.eventType = ((event->event.eventType == IswButtonDown) ?
                                IswButtonUp : IswKeyUp);
    if ((upEvent->event.eventType == IswButtonUp)
        && (upEvent->event.modifiers != TM_ANY_MODIFIER)
        && (upEvent->event.modifiers | upEvent->event.modifierMask))
        upEvent->event.modifiers = (upEvent->event.modifiers
                                    | (TMLongCard) buttonModifierMasks[event->
                                                                       event.
                                                                       eventCode]);

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count + reps * 2 -
                              1);

    for (i = 0; i < reps; i++) {

        if (i > 0) {
            /* down */
            event->next = IswNew(EventSeqRec);
            event = event->next;
            *event = *downEvent;
        }
        lastDownEvent = event;

        /* up */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *upEvent;

        /* timer */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = timerEventRec;

    }

    event->next = lastDownEvent;
    *eventP = event;
    *actionsP = &lastDownEvent->actions;
}

static void
RepeatUp(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    EventRec upEventRec;
    register EventPtr event, downEvent;
    EventPtr upEvent = &upEventRec;
    register int i;

    /* the event currently sitting in *eventP is an "up" event */
    /* we want to make it a "down" event followed by an "up" event, */
    /* so that sequence matching on the "state" side works correctly. */

    downEvent = event = *eventP;
    *upEvent = *downEvent;
    downEvent->event.eventType = ((event->event.eventType == IswButtonUp) ?
                                  IswButtonDown : IswKeyDown);
    if ((downEvent->event.eventType == IswButtonDown)
        && (downEvent->event.modifiers != TM_ANY_MODIFIER)
        && (downEvent->event.modifiers | downEvent->event.modifierMask))
        downEvent->event.modifiers = (downEvent->event.modifiers
                                      &
                                      (TMLongCard) (~buttonModifierMasks
                                                    [event->event.eventCode]));

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count + reps * 2 -
                              1);

    /* up */
    event->next = IswNew(EventSeqRec);
    event = event->next;
    *event = *upEvent;

    for (i = 1; i < reps; i++) {

        /* timer */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = timerEventRec;

        /* down */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *downEvent;

        /* up */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *upEvent;

    }

    event->next = NULL;
    *eventP = event;
    *actionsP = &event->actions;
}

static void
RepeatUpPlus(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    EventRec upEventRec;
    register EventPtr event, downEvent, lastUpEvent = NULL;
    EventPtr upEvent = &upEventRec;
    register int i;

    /* the event currently sitting in *eventP is an "up" event */
    /* we want to make it a "down" event followed by an "up" event, */
    /* so that sequence matching on the "state" side works correctly. */

    downEvent = event = *eventP;
    *upEvent = *downEvent;
    downEvent->event.eventType = ((event->event.eventType == IswButtonUp) ?
                                  IswButtonDown : IswKeyDown);
    if ((downEvent->event.eventType == IswButtonDown)
        && (downEvent->event.modifiers != TM_ANY_MODIFIER)
        && (downEvent->event.modifiers | downEvent->event.modifierMask))
        downEvent->event.modifiers = (downEvent->event.modifiers
                                      &
                                      (TMLongCard) (~buttonModifierMasks
                                                    [event->event.eventCode]));

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count + reps * 2);

    for (i = 0; i < reps; i++) {

        /* up */
        event->next = IswNew(EventSeqRec);
        lastUpEvent = event = event->next;
        *event = *upEvent;

        /* timer */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = timerEventRec;

        /* down */
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *downEvent;

    }

    event->next = lastUpEvent;
    *eventP = event;
    *actionsP = &lastUpEvent->actions;
}

static void
RepeatOther(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    register EventPtr event, tempEvent;
    register int i;

    tempEvent = event = *eventP;

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count + reps - 1);

    for (i = 1; i < reps; i++) {
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *tempEvent;
    }

    *eventP = event;
    *actionsP = &event->actions;
}

static void
RepeatOtherPlus(EventPtr *eventP, int reps, ActionPtr **actionsP)
{
    register EventPtr event, tempEvent;
    register int i;

    tempEvent = event = *eventP;

    if (event->event.lateModifiers)
        event->event.lateModifiers->ref_count =
            (unsigned short) (event->event.lateModifiers->ref_count + reps - 1);

    for (i = 1; i < reps; i++) {
        event->next = IswNew(EventSeqRec);
        event = event->next;
        *event = *tempEvent;
    }

    event->next = event;
    *eventP = event;
    *actionsP = &event->actions;
}

static void
RepeatEvent(EventPtr *eventP, int reps, Boolean plus, ActionPtr **actionsP)
{
    switch ((*eventP)->event.eventType) {

    case IswButtonDown:
    case IswKeyDown:
        if (plus)
            RepeatDownPlus(eventP, reps, actionsP);
        else
            RepeatDown(eventP, reps, actionsP);
        break;

    case IswButtonUp:
    case IswKeyUp:
        if (plus)
            RepeatUpPlus(eventP, reps, actionsP);
        else
            RepeatUp(eventP, reps, actionsP);
        break;

    default:
        if (plus)
            RepeatOtherPlus(eventP, reps, actionsP);
        else
            RepeatOther(eventP, reps, actionsP);
    }
}

static String
ParseRepeat(register String str, int *reps, Boolean *plus, Boolean *error)
{

    /*** Parse the repetitions, for double click etc... ***/
    if (*str != '(' ||
        !(isdigit((unsigned char) str[1]) || str[1] == '+' || str[1] == ')'))
        return str;
    str++;
    if (isdigit((unsigned char) *str)) {
        String start = str;
        char repStr[7];
        size_t len;

        ScanNumeric(str);
        len = (size_t) (str - start);
        if (len < sizeof repStr) {
            (void) memcpy(repStr, start, len);
            repStr[len] = '\0';
            *reps = (int) StrToNum(repStr);
        }
        else {
            Syntax("Repeat count too large.", "");
            *error = TRUE;
            return str;
        }
    }
    if (*reps == 0) {
        Syntax("Missing repeat count.", "");
        *error = True;
        return str;
    }

    if (*str == '+') {
        *plus = TRUE;
        str++;
    }
    if (*str == ')')
        str++;
    else {
        Syntax("Missing ')'.", "");
        *error = TRUE;
    }

    return str;
}

/***********************************************************************
 * ParseEventSeq
 * Parses the left hand side of a translation table production
 * up to, and consuming the ":".
 * Takes a pointer to a char* (where to start parsing) and returns an
 * event seq (in a passed in variable), having updated the String
 **********************************************************************/

static String
ParseEventSeq(register String str,
              EventSeqPtr *eventSeqP,
              ActionPtr ** actionsP,
              Boolean *error)
{
    EventSeqPtr *nextEvent = eventSeqP;

    *eventSeqP = NULL;

    while (*str != '\0' && !IsNewline(*str)) {
        static Event nullEvent =
            { 0, 0, NULL, 0, 0L, 0L, _IswRegularMatch, FALSE };
        EventPtr event;

        ScanWhitespace(str);

        if (*str == '"') {
            str++;
            while (*str != '"' && *str != '\0' && !IsNewline(*str)) {
                event = IswNew(EventRec);
                event->event = nullEvent;
                event->state = /* (StatePtr) -1 */ NULL;
                event->next = NULL;
                event->actions = NULL;
                str = ParseQuotedStringEvent(str, event, error);
                if (*error) {
                    IswWarningMsg(IswNtranslationParseError, "nonLatin1",
                                 IswCIswToolkitError,
                                 "... probably due to non-Latin1 character in quoted string",
                                 (String *) NULL, (Cardinal *) NULL);
                    IswFree((char *) event);
                    return PanicModeRecovery(str);
                }
                *nextEvent = event;
                *actionsP = &event->actions;
                nextEvent = &event->next;
            }
            if (*str != '"') {
                Syntax("Missing '\"'.", "");
                *error = TRUE;
                return PanicModeRecovery(str);
            }
            else
                str++;
        }
        else {
            int reps = 0;
            Boolean plus = FALSE;

            event = IswNew(EventRec);
            event->event = nullEvent;
            event->state = /* (StatePtr) -1 */ NULL;
            event->next = NULL;
            event->actions = NULL;

            str = ParseEvent(str, event, &reps, &plus, error);
            if (*error)
                return str;
            *nextEvent = event;
            *actionsP = &event->actions;
            if (reps > 1 || plus)
                RepeatEvent(&event, reps, plus, actionsP);
            nextEvent = &event->next;
        }
        ScanWhitespace(str);
        if (*str == ':')
            break;
        else {
            if (*str != ',') {
                Syntax("',' or ':' expected while parsing event sequence.", "");
                *error = TRUE;
                return PanicModeRecovery(str);
            }
            else
                str++;
        }
    }

    if (*str != ':') {
        Syntax("Missing ':'after event sequence.", "");
        *error = TRUE;
        return PanicModeRecovery(str);
    }
    else
        str++;

    return str;
}

static String
ParseActionProc(register String str, IswQuark *actionProcNameP, Boolean *error)
{
    register String start = str;
    char procName[200];

    str = ScanIdent(str);
    if (str - start >= 199) {
        Syntax("Action procedure name is longer than 199 chars", "");
        *error = TRUE;
        return str;
    }
    (void) memcpy(procName, start, (size_t) (str - start));
    procName[str - start] = '\0';
    *actionProcNameP = IswStringToQuark(procName);
    return str;
}

static String
ParseString(register String str, _IswString *strP)
{
    register String start;

    if (*str == '"') {
        register unsigned prev_len, len;

        str++;
        start = str;
        *strP = NULL;
        prev_len = 0;

        while (*str != '"' && *str != '\0') {
            /* \"  produces double quote embedded in a quoted parameter
             * \\" produces backslash as last character of a quoted parameter
             */
            if (*str == '\\' &&
                (*(str + 1) == '"' ||
                 (*(str + 1) == '\\' && *(str + 2) == '"'))) {
                len = (unsigned) (prev_len + (str - start + 2));
                *strP = IswRealloc(*strP, len);
                (void) memcpy(*strP + prev_len, start, (size_t) (str - start));
                prev_len = len - 1;
                str++;
                (*strP)[prev_len - 1] = *str;
                (*strP)[prev_len] = '\0';
                start = str + 1;
            }
            str++;
        }
        len = (unsigned) (prev_len + (str - start + 1));
        *strP = IswRealloc(*strP, len);
        (void) memcpy(*strP + prev_len, start, (size_t) (str - start));
        (*strP)[len - 1] = '\0';
        if (*str == '"')
            str++;
        else
            IswWarningMsg(IswNtranslationParseError, "parseString",
                         IswCIswToolkitError, "Missing '\"'.",
                         (String *) NULL, (Cardinal *) NULL);
    }
    else {
        /* scan non-quoted string, stop on whitespace, ',' or ')' */
        start = str;
        while (*str != ' '
               && *str != '\t' && *str != ',' && *str != ')' && !IsNewline(*str)
               && *str != '\0')
            str++;
        *strP = __XtMalloc((unsigned) (str - start + 1));
        (void) memcpy(*strP, start, (size_t) (str - start));
        (*strP)[str - start] = '\0';
    }
    return str;
}

static String
ParseParamSeq(register String str, String **paramSeqP, Cardinal *paramNumP)
{
    typedef struct _ParamRec *ParamPtr;
    typedef struct _ParamRec {
        ParamPtr next;
        String param;
    } ParamRec;

    ParamPtr params = NULL;
    Cardinal num_params = 0;

    ScanWhitespace(str);
    while (*str != ')' && *str != '\0' && !IsNewline(*str)) {
        _IswString newStr;

        str = ParseString(str, &newStr);
        if (newStr != NULL) {
            ParamPtr temp = (ParamRec *)
                ALLOCATE_LOCAL((unsigned) sizeof(ParamRec));

            if (temp == NULL)
                _IswAllocError(NULL);

            num_params++;
            temp->next = params;
            params = temp;
            temp->param = newStr;
            ScanWhitespace(str);
            if (*str == ',') {
                str++;
                ScanWhitespace(str);
            }
        }
    }

    if (num_params != 0) {
        String *paramP = IswMallocArray(num_params + 1, (Cardinal)sizeof(String));
        Cardinal i;

        *paramSeqP = paramP;
        *paramNumP = num_params;
        paramP += num_params;   /* list is LIFO right now */
        *paramP-- = NULL;
        for (i = 0; i < num_params; i++) {
            ParamPtr next = params->next;

            *paramP-- = params->param;
            DEALLOCATE_LOCAL((char *) params);
            params = next;
        }
    }
    else {
        *paramSeqP = NULL;
        *paramNumP = 0;
    }

    return str;
}

static String
ParseAction(String str, ActionPtr actionP, IswQuark *quarkP, Boolean *error)
{
    str = ParseActionProc(str, quarkP, error);
    if (*error)
        return str;

    if (*str == '(') {
        str++;
        str = ParseParamSeq(str, &actionP->params, &actionP->num_params);
    }
    else {
        Syntax("Missing '(' while parsing action sequence", "");
        *error = TRUE;
        return str;
    }
    if (*str == ')')
        str++;
    else {
        Syntax("Missing ')' while parsing action sequence", "");
        *error = TRUE;
        return str;
    }
    return str;
}

static String
ParseActionSeq(TMParseStateTree parseTree,
               String str,
               ActionPtr *actionsP,
               Boolean *error)
{
    ActionPtr *nextActionP;

    if ((nextActionP = actionsP) != NULL)
        *actionsP = NULL;

    while (*str != '\0' && !IsNewline(*str)) {
        register ActionPtr action;
        IswQuark quark = ISW_NULLQUARK;

        action = IswNew(ActionRec);
        action->params = NULL;
        action->num_params = 0;
        action->next = NULL;

        str = ParseAction(str, action, &quark, error);
        if (*error) {
            IswFree((char *) action);
            return PanicModeRecovery(str);
        }

        action->idx = _IswGetQuarkIndex(parseTree, quark);
        ScanWhitespace(str);
        if (nextActionP) {
            *nextActionP = action;
            nextActionP = &action->next;
        }
    }
    if (IsNewline(*str))
        str++;
    ScanWhitespace(str);
    return str;
}

static void
ShowProduction(String currentProduction)
{
    Cardinal num_params = 1;
    String params[1];
    size_t len;
    char *eol, *production, productionbuf[500];

    eol = strchr(currentProduction, '\n');
    if (eol)
        len = (size_t) (eol - currentProduction);
    else
        len = strlen(currentProduction);
    production = IswStackAlloc(len + 1, productionbuf);
    if (production == NULL)
        _IswAllocError(NULL);
    (void) memcpy(production, currentProduction, len);
    production[len] = '\0';

    params[0] = production;
    IswWarningMsg(IswNtranslationParseError, "showLine", IswCIswToolkitError,
                 "... found while parsing '%s'", params, &num_params);

    IswStackFree(production, productionbuf);
}

/***********************************************************************
 * ParseTranslationTableProduction
 * Parses one line of event bindings.
 ***********************************************************************/

static String
ParseTranslationTableProduction(TMParseStateTree parseTree,
                                register String str,
                                Boolean *error)
{
    EventSeqPtr eventSeq = NULL;
    ActionPtr *actionsP;
    String production = str;

    actionsP = NULL;
    str = ParseEventSeq(str, &eventSeq, &actionsP, error);
    if (*error == TRUE) {
        ShowProduction(production);
    }
    else {
        ScanWhitespace(str);
        str = ParseActionSeq(parseTree, str, actionsP, error);
        if (*error == TRUE) {
            ShowProduction(production);
        }
        else {
            _IswAddEventSeqToStateTree(eventSeq, parseTree);
        }
    }
    FreeEventSeq(eventSeq);
    return (str);
}

static String
CheckForPoundSign(String str,
                  _IswTranslateOp defaultOp,
                  _IswTranslateOp *actualOpRtn)
{
    _IswTranslateOp opType;

    opType = defaultOp;
    ScanWhitespace(str);

    if (*str == '#') {
        String start;
        char operation[20];
        int len;

        str++;
        start = str;
        str = ScanIdent(str);
        len = MIN(19, (int) (str - start));
        (void) memcpy(operation, start, (size_t) len);
        operation[len] = '\0';
        if (!strcmp(operation, "replace"))
            opType = IswTableReplace;
        else if (!strcmp(operation, "augment"))
            opType = IswTableAugment;
        else if (!strcmp(operation, "override"))
            opType = IswTableOverride;
        ScanWhitespace(str);
        if (IsNewline(*str)) {
            str++;
            ScanWhitespace(str);
        }
    }
    *actualOpRtn = opType;
    return str;
}

static IswTranslations
ParseTranslationTable(String source,
                      Boolean isAccelerator,
                      _IswTranslateOp defaultOp,
                      Boolean *error)
{
    IswTranslations xlations;
    TMStateTree stateTrees[8];
    TMParseStateTreeRec parseTreeRec, *parseTree = &parseTreeRec;
    IswQuark stackQuarks[200];
    TMBranchHeadRec stackBranchHeads[200];
    StatePtr stackComplexBranchHeads[200];
    _IswTranslateOp actualOp;

    if (source == NULL)
        return (IswTranslations) NULL;

    source = CheckForPoundSign(source, defaultOp, &actualOp);
    if (isAccelerator && actualOp == IswTableReplace)
        actualOp = defaultOp;

    parseTree->isSimple = TRUE;
    parseTree->mappingNotifyInterest = FALSE;
    IswSetBit(parseTree->isAccelerator, isAccelerator);
    parseTree->isStackBranchHeads =
        parseTree->isStackQuarks = parseTree->isStackComplexBranchHeads = TRUE;

    parseTree->numQuarks =
        parseTree->numBranchHeads = parseTree->numComplexBranchHeads = 0;

    parseTree->quarkTblSize =
        parseTree->branchHeadTblSize =
        parseTree->complexBranchHeadTblSize = 200;

    parseTree->quarkTbl = stackQuarks;
    parseTree->branchHeadTbl = stackBranchHeads;
    parseTree->complexBranchHeadTbl = stackComplexBranchHeads;

    while (source != NULL && *source != '\0') {
        source = ParseTranslationTableProduction(parseTree, source, error);
        if (*error == TRUE)
            break;
    }
    stateTrees[0] = _IswParseTreeToStateTree(parseTree);

    if (!parseTree->isStackQuarks)
        IswFree((char *) parseTree->quarkTbl);
    if (!parseTree->isStackBranchHeads)
        IswFree((char *) parseTree->branchHeadTbl);
    if (!parseTree->isStackComplexBranchHeads)
        IswFree((char *) parseTree->complexBranchHeadTbl);

    xlations = _IswCreateXlations(stateTrees, 1, NULL, NULL);
    xlations->operation = (unsigned char) actualOp;

#ifdef notdef
    IswFree(stateTrees);
#endif                          /* notdef */
    return xlations;
}

/*** public procedures ***/

Boolean
IswCvtStringToAcceleratorTable(IswDisplay dpy,
                              IswValuePtr args _X_UNUSED,
                              Cardinal *num_args,
                              IswValuePtr from,
                              IswValuePtr to,
                              IswPointer *closure _X_UNUSED)
{
    String str;
    Boolean error = FALSE;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "wrongParameters", "cvtStringToAcceleratorTable",
                        IswCIswToolkitError,
                        "String to AcceleratorTable conversion needs no extra arguments",
                        (String *) NULL, (Cardinal *) NULL);
    str = (String) (from->addr);
    if (str == NULL) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "badParameters", "cvtStringToAcceleratorTable",
                        IswCIswToolkitError,
                        "String to AcceleratorTable conversion needs string",
                        (String *) NULL, (Cardinal *) NULL);
        return FALSE;
    }
    if (to->addr != NULL) {
        if (to->size < sizeof(IswAccelerators)) {
            to->size = sizeof(IswAccelerators);
            return FALSE;
        }
        *(IswAccelerators *) to->addr =
            (IswAccelerators) ParseTranslationTable(str, TRUE, IswTableAugment,
                                                   &error);
    }
    else {
        static IswAccelerators staticStateTable;

        staticStateTable =
            (IswAccelerators) ParseTranslationTable(str, TRUE, IswTableAugment,
                                                   &error);
        to->addr = (IswPointer) &staticStateTable;
        to->size = sizeof(IswAccelerators);
    }
    if (error == TRUE)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "parseError", "cvtStringToAcceleratorTable",
                        IswCIswToolkitError,
                        "String to AcceleratorTable conversion encountered errors",
                        (String *) NULL, (Cardinal *) NULL);
    return (error != TRUE);
}

Boolean
IswCvtStringToTranslationTable(IswDisplay dpy,
                              IswValuePtr args _X_UNUSED,
                              Cardinal *num_args,
                              IswValuePtr from,
                              IswValuePtr to,
                              IswPointer *closure_ret _X_UNUSED)
{
    String str;
    Boolean error = FALSE;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "wrongParameters", "cvtStringToTranslationTable",
                        IswCIswToolkitError,
                        "String to TranslationTable conversion needs no extra arguments",
                        (String *) NULL, (Cardinal *) NULL);
    str = (String) (from->addr);
    if (str == NULL) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "badParameters", "cvtStringToTranslation",
                        IswCIswToolkitError,
                        "String to TranslationTable conversion needs string",
                        (String *) NULL, (Cardinal *) NULL);
        return FALSE;
    }
    if (to->addr != NULL) {
        if (to->size < sizeof(IswTranslations)) {
            to->size = sizeof(IswTranslations);
            return FALSE;
        }
        *(IswTranslations *) to->addr =
            ParseTranslationTable(str, FALSE, IswTableReplace, &error);
    }
    else {
        static IswTranslations staticStateTable;

        staticStateTable =
            ParseTranslationTable(str, FALSE, IswTableReplace, &error);
        to->addr = (IswPointer) &staticStateTable;
        to->size = sizeof(IswTranslations);
    }
    if (error == TRUE)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "parseError", "cvtStringToTranslationTable",
                        IswCIswToolkitError,
                        "String to TranslationTable conversion encountered errors",
                        (String *) NULL, (Cardinal *) NULL);
    return (error != TRUE);
}

/*
 * Parses a user's or applications translation table
 */
IswAccelerators
IswParseAcceleratorTable(_Xconst char *source)
{
    Boolean error = FALSE;
    IswAccelerators ret =
        (IswAccelerators) ParseTranslationTable(source, TRUE, IswTableAugment,
                                               &error);

    if (error == TRUE)
        IswWarningMsg("parseError", "cvtStringToAcceleratorTable",
                     IswCIswToolkitError,
                     "String to AcceleratorTable conversion encountered errors",
                     (String *) NULL, (Cardinal *) NULL);
    return ret;
}

IswTranslations
IswParseTranslationTable(_Xconst char *source)
{
    Boolean error = FALSE;
    IswTranslations ret =
        ParseTranslationTable(source, FALSE, IswTableReplace, &error);
    if (error == TRUE)
        IswWarningMsg("parseError",
                     "cvtStringToTranslationTable", IswCIswToolkitError,
                     "String to TranslationTable conversion encountered errors",
                     (String *) NULL, (Cardinal *) NULL);
    return ret;
}

void
_IswTranslateInitialize(void)
{
    LOCK_PROCESS;
    if (initialized) {
        IswWarningMsg("translationError", "xtTranslateInitialize",
                     IswCIswToolkitError,
                     "Initializing Translation manager twice.", (String *) NULL,
                     (Cardinal *) NULL);
        UNLOCK_PROCESS;
        return;
    }

    initialized = TRUE;
    UNLOCK_PROCESS;
    QMeta = IswPermStringToQuark("Meta");
    QCtrl = IswPermStringToQuark("Ctrl");
    QNone = IswPermStringToQuark("None");
    QAny = IswPermStringToQuark("Any");

    Compile_XtEventTable(events, IswNumber(events));
    Compile_XtModifierTable(modifiers, IswNumber(modifiers));
    CompileNameValueTable(notifyModes);
    CompileNameValueTable(motionDetails);
}

void
_IswAddTMConverters(ConverterTable table)
{
    _IswTableAddConverter(table,
                         _IswQString,
                         IswPermStringToQuark(IswRTranslationTable),
                         IswCvtStringToTranslationTable, (IswConvertArgList) NULL,
                         (Cardinal) 0, True, CACHED, _IswFreeTranslations, True);
    _IswTableAddConverter(table, _IswQString,
                         IswPermStringToQuark(IswRAcceleratorTable),
                         IswCvtStringToAcceleratorTable, (IswConvertArgList) NULL,
                         (Cardinal) 0, True, CACHED, _IswFreeTranslations, True);
    _IswTableAddConverter(table,
                         IswPermStringToQuark(_IswRStateTablePair),
                         IswPermStringToQuark(IswRTranslationTable),
                         _IswCvtMergeTranslations, (IswConvertArgList) NULL,
                         (Cardinal) 0, True, CACHED, _IswFreeTranslations, True);
}
