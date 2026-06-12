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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include <ISW/ISWPlatform.h>

#include <ctype.h>

#define FLUSHKEYCACHE(ctx) \
        memset((void *)&ctx->keycache, 0, sizeof(TMKeyCache))

/*
 * The following array reorders the modifier bits so that the most common ones
 * (used by a translator) are in the top-most bits with respect to the size of
 * the keycache.  The array currently just reverses the bits as a good guess.
 * This might be more trouble than it is worth, but it seems to help.
 */

#define FM(i) i >> (8 - TMKEYCACHELOG2)

/* *INDENT-OFF* */
static const unsigned char modmix[256] = {
FM(0x0f), FM(0x8f), FM(0x4f), FM(0xcf), FM(0x2f), FM(0xaf), FM(0x6f), FM(0xef),
FM(0x1f), FM(0x9f), FM(0x5f), FM(0xdf), FM(0x3f), FM(0xbf), FM(0x7f), FM(0xff),
FM(0x07), FM(0x87), FM(0x47), FM(0xc7), FM(0x27), FM(0xa7), FM(0x67), FM(0xe7),
FM(0x17), FM(0x97), FM(0x57), FM(0xd7), FM(0x37), FM(0xb7), FM(0x77), FM(0xf7),
FM(0x0b), FM(0x8b), FM(0x4b), FM(0xcb), FM(0x2b), FM(0xab), FM(0x6b), FM(0xeb),
FM(0x1b), FM(0x9b), FM(0x5b), FM(0xdb), FM(0x3b), FM(0xbb), FM(0x7b), FM(0xfb),
FM(0x03), FM(0x83), FM(0x43), FM(0xc3), FM(0x23), FM(0xa3), FM(0x63), FM(0xe3),
FM(0x13), FM(0x93), FM(0x53), FM(0xd3), FM(0x33), FM(0xb3), FM(0x73), FM(0xf3),
FM(0x0d), FM(0x8d), FM(0x4d), FM(0xcd), FM(0x2d), FM(0xad), FM(0x6d), FM(0xed),
FM(0x1d), FM(0x9d), FM(0x5d), FM(0xdd), FM(0x3d), FM(0xbd), FM(0x7d), FM(0xfd),
FM(0x05), FM(0x85), FM(0x45), FM(0xc5), FM(0x25), FM(0xa5), FM(0x65), FM(0xe5),
FM(0x15), FM(0x95), FM(0x55), FM(0xd5), FM(0x35), FM(0xb5), FM(0x75), FM(0xf5),
FM(0x09), FM(0x89), FM(0x49), FM(0xc9), FM(0x29), FM(0xa9), FM(0x69), FM(0xe9),
FM(0x19), FM(0x99), FM(0x59), FM(0xd9), FM(0x39), FM(0xb9), FM(0x79), FM(0xf9),
FM(0x01), FM(0x81), FM(0x41), FM(0xc1), FM(0x21), FM(0xa1), FM(0x61), FM(0xe1),
FM(0x11), FM(0x91), FM(0x51), FM(0xd1), FM(0x31), FM(0xb1), FM(0x71), FM(0xf1),
FM(0x00), FM(0x8e), FM(0x4e), FM(0xce), FM(0x2e), FM(0xae), FM(0x6e), FM(0xee),
FM(0x1e), FM(0x9e), FM(0x5e), FM(0xde), FM(0x3e), FM(0xbe), FM(0x7e), FM(0xfe),
FM(0x08), FM(0x88), FM(0x48), FM(0xc8), FM(0x28), FM(0xa8), FM(0x68), FM(0xe8),
FM(0x18), FM(0x98), FM(0x58), FM(0xd8), FM(0x38), FM(0xb8), FM(0x78), FM(0xf8),
FM(0x04), FM(0x84), FM(0x44), FM(0xc4), FM(0x24), FM(0xa4), FM(0x64), FM(0xe4),
FM(0x14), FM(0x94), FM(0x54), FM(0xd4), FM(0x34), FM(0xb4), FM(0x74), FM(0xf4),
FM(0x0c), FM(0x8c), FM(0x4c), FM(0xcc), FM(0x2c), FM(0xac), FM(0x6c), FM(0xec),
FM(0x1c), FM(0x9c), FM(0x5c), FM(0xdc), FM(0x3c), FM(0xbc), FM(0x7c), FM(0xfc),
FM(0x02), FM(0x82), FM(0x42), FM(0xc2), FM(0x22), FM(0xa2), FM(0x62), FM(0xe2),
FM(0x12), FM(0x92), FM(0x52), FM(0xd2), FM(0x32), FM(0xb2), FM(0x72), FM(0xf2),
FM(0x0a), FM(0x8a), FM(0x4a), FM(0xca), FM(0x2a), FM(0xaa), FM(0x6a), FM(0xea),
FM(0x1a), FM(0x9a), FM(0x5a), FM(0xda), FM(0x3a), FM(0xba), FM(0x7a), FM(0xfa),
FM(0x06), FM(0x86), FM(0x46), FM(0xc6), FM(0x26), FM(0xa6), FM(0x66), FM(0xe6),
FM(0x16), FM(0x96), FM(0x56), FM(0xd6), FM(0x36), FM(0xb6), FM(0x76), FM(0xf6),
FM(0x0e), FM(0x8e), FM(0x4e), FM(0xce), FM(0x2e), FM(0xae), FM(0x6e), FM(0xee),
FM(0x1e), FM(0x9e), FM(0x5e), FM(0xde), FM(0x3e), FM(0xbe), FM(0x7e), FM(0xfe)
};
/* *INDENT-ON* */
#undef FM

#define MOD_RETURN(ctx, key) (ctx)->keycache.modifiers_return[key]

#define TRANSLATE(ctx,pd,dpy,key,mod,mod_ret,sym_ret) \
{ \
    int _i_ = (((key) - (TMLongCard) (pd)->min_keycode + modmix[(mod) & 0xff]) & \
               (TMKEYCACHESIZE-1)); \
    if ((key) == 0) { /* Xlib XIM composed input */ \
        mod_ret = 0; \
        sym_ret = 0; \
    } else if (   /* not Xlib XIM composed input */ \
        (ctx)->keycache.keycode[_i_] == (key) && \
        (ctx)->keycache.modifiers[_i_] == (mod)) { \
        mod_ret = MOD_RETURN(ctx, key); \
        sym_ret = (ctx)->keycache.keysym[_i_]; \
    } else { \
        IswTranslateKeycode((IswDisplay) (dpy), (IswKeyCode) key, mod, &mod_ret, &sym_ret); \
        (ctx)->keycache.keycode[_i_] = (IswKeyCode) (key); \
        (ctx)->keycache.modifiers[_i_] = (unsigned char)(mod); \
        (ctx)->keycache.keysym[_i_] = sym_ret; \
        MOD_RETURN(ctx, key) = (unsigned char)mod_ret; \
    } \
}

#define UPDATE_CACHE(ctx, pd, key, mod, mod_ret, sym_ret) \
{ \
    int _i_ = (((key) - (TMLongCard) (pd)->min_keycode + modmix[(mod) & 0xff]) & \
               (TMKEYCACHESIZE-1)); \
    (ctx)->keycache.keycode[_i_] = (IswKeyCode) (key); \
    (ctx)->keycache.modifiers[_i_] = (unsigned char)(mod); \
    (ctx)->keycache.keysym[_i_] = sym_ret; \
    MOD_RETURN(ctx, key) = (unsigned char)(mod_ret); \
}

/* usual number of expected keycodes in IswKeysymToKeycodeList */
#define KEYCODE_ARRAY_SIZE 10

Boolean
_IswComputeLateBindings(IswDisplay dpy,
                       LateBindingsPtr lateModifiers,
                       uint16_t *computed,
                       uint16_t *computedMask)
{
    int i, j, ref;
    ModToKeysymTable *temp;
    IswPerDisplay perDisplay;
    IswKeySym tempKeysym = NoSymbol;

    perDisplay = _IswGetPerDisplay(dpy);
    if (perDisplay == NULL) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "displayError", "invalidDisplay", IswCIswToolkitError,
                        "Can't find display structure", NULL, NULL);
        return FALSE;
    }
    _InitializeKeysymTables(dpy, perDisplay);
    
    for (ref = 0; lateModifiers[ref].keysym; ref++) {
        Boolean found = FALSE;

        for (i = 0; i < 8; i++) {
            temp = &(perDisplay->modsToKeysyms[i]);
            for (j = 0; j < temp->count; j++) {
                if (perDisplay->modKeysyms[temp->idx + j] ==
                    lateModifiers[ref].keysym) {
                    *computedMask = *computedMask | temp->mask;
                    if (!lateModifiers[ref].knot)
                        *computed |= temp->mask;
                    tempKeysym = lateModifiers[ref].keysym;
                    found = TRUE;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found && !lateModifiers[ref].knot)
            if (!lateModifiers[ref].pair && (tempKeysym == NoSymbol))
                return FALSE;
        /* if you didn't find the modifier and the modifier must be
           asserted then return FALSE. If you didn't find the modifier
           and the modifier must be off, then it is OK . Don't
           return FALSE if this is the first member of a pair or if
           it is the second member of a pair when the first member
           was bound to a modifier */
        if (!lateModifiers[ref].pair)
            tempKeysym = NoSymbol;
    }
    return TRUE;
}

void
_IswAllocTMContext(IswPerDisplay pd)
{
    TMKeyContext ctx;

    ctx = (TMKeyContext) _IswHeapAlloc(&pd->heap, sizeof(TMKeyContextRec));
    //ctx->event = NULL;
    ctx->serial = 0;
    ctx->keysym = NoSymbol;
    ctx->modifiers = 0;
    FLUSHKEYCACHE(ctx);
    pd->tm_context = ctx;
}



void
IswConvertCase(IswDisplay dpy,
              IswKeySym keysym,
              IswKeySym *lower_return,
              IswKeySym *upper_return)
{
    IswPerDisplay pd;
    CaseConverterPtr ptr;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _IswGetPerDisplay((IswDisplay) dpy);

    *lower_return = *upper_return = keysym;
    for (ptr = pd->case_cvt; ptr; ptr = ptr->next)
        if (ptr->start <= keysym && keysym <= ptr->stop) {
            (*ptr->proc) (dpy, keysym, lower_return, upper_return);
            return;
        }
    
    if (keysym >= 32 && keysym < 128) {
        *lower_return = tolower((char)keysym);
        *upper_return = toupper((char)keysym);
    } else {
        *lower_return = keysym;
        *upper_return = keysym;
    }
    UNLOCK_APP(app);
}


/* Build the modifier->keysym late-binding tables on demand.  The native read
   of the server's modifier mapping lives in the platform input backend
   (_IswPlatformBuildModMap); this just owns the per-display table storage. */
void
_IswInitKeysymTables(IswDisplay dpy, register IswPerDisplay pd)
{
    IswModKeysymEntry mods[8];
    IswKeySym *pool = NULL;
    int count = 0;
    int i;

    if (pd->modsToKeysyms != NULL)
        return;  /* Already initialized */

    _IswPlatformBuildModMap(dpy, mods, &pool, &count);

    pd->modsToKeysyms = (ModToKeysymTable *) __XtCalloc(8, sizeof(ModToKeysymTable));
    for (i = 0; i < 8; i++) {
        pd->modsToKeysyms[i].mask  = mods[i].mask;
        pd->modsToKeysyms[i].idx   = mods[i].idx;
        pd->modsToKeysyms[i].count = mods[i].count;
    }
    pd->modKeysyms = pool;   /* malloc'd by the backend; freed in Display.c */
}

void
IswTranslateKey(IswDisplay dpy, IswKeyCode keycode,
               Modifiers modifiers, Modifiers *modifiers_return,
               IswKeySym *keysym_return)
{
    IswPerDisplay pd;
    IswKeySym sym;
    int col;
    Modifiers mods_consumed = 0;

    DPY_TO_APPCON(dpy);
    LOCK_APP(app);
    pd = _IswGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);

    /* col 0 = unshifted, col 1 = shifted */
    col = (modifiers & IswModShift) ? 1 : 0;
    if (col == 1)
        mods_consumed |= IswModShift;

    sym = _IswPlatformKeycodeToKeysym(dpy, keycode, col);

    /* CapsLock: for lowercase alphabetic keys, return uppercase */
    if ((modifiers & IswModLock) && col == 0 && sym >= IswKey_a && sym <= IswKey_z) {
        IswKeySym usym = _IswPlatformKeycodeToKeysym(dpy, keycode, 1);
        if (usym != IswNoSymbol) {
            sym = usym;
            mods_consumed |= IswModLock;
        }
    }

    /* Fall back to unshifted if the selected column has no symbol */
    if (sym == IswNoSymbol) {
        sym = _IswPlatformKeycodeToKeysym(dpy, keycode, 0);
        mods_consumed &= ~IswModShift;  /* Shift wasn't actually used */
    }
    if (sym == IswNoSymbol)
        sym = IswKeyVoidSymbol;

    *modifiers_return = mods_consumed;
    *keysym_return = sym;
    UNLOCK_APP(app);
}

void
IswTranslateKeycode(IswDisplay dpy,
                   IswKeyCode keycode,
                   Modifiers modifiers,
                   Modifiers *modifiers_return,
                   IswKeySym *keysym_return)
{
    IswPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _IswGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);
    (*pd->defaultKeycodeTranslator) (dpy, keycode, modifiers, modifiers_return,
                                     keysym_return);
    UNLOCK_APP(app);
}

void
IswSetKeyTranslator(IswDisplay dpy, IswKeyProc translator)
{
    IswPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _IswGetPerDisplay(dpy);

    pd->defaultKeycodeTranslator = translator;
    FLUSHKEYCACHE(pd->tm_context);
    /* XXX should now redo grabs */
    UNLOCK_APP(app);
}

void
IswRegisterCaseConverter(IswDisplay dpy,
                        IswCaseProc proc,
                        IswKeySym start,
                        IswKeySym stop)
{
    IswPerDisplay pd;
    CaseConverterPtr ptr, prev;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _IswGetPerDisplay(dpy);

    ptr = (CaseConverterPtr) __XtMalloc(sizeof(CaseConverterRec));
    ptr->start = start;
    ptr->stop = stop;
    ptr->proc = proc;
    ptr->next = pd->case_cvt;
    pd->case_cvt = ptr;

    /* Remove obsolete case converters from the list */
    prev = ptr;
    for (ptr = ptr->next; ptr; ptr = prev->next) {
        if (start <= ptr->start && stop >= ptr->stop) {
            prev->next = ptr->next;
            IswFree((char *) ptr);
        }
        else
            prev = ptr;
    }
    FLUSHKEYCACHE(pd->tm_context);
    /* XXX should now redo grabs */
    UNLOCK_APP(app);
}

void
IswKeysymToKeycodeList(IswDisplay dpy,
                      IswKeySym keysym,
                      IswKeyCode **keycodes_return,
                      unsigned int *keycount_return)
{
    IswKeyCode *codes = NULL;
    int count = 0;

    /* The case-folding keycode lookup is keymap work; the backend owns it. */
    _IswPlatformKeysymToKeycodes(dpy, keysym, &codes, &count);

    *keycodes_return = codes;
    *keycount_return = (unsigned int) count;
}
