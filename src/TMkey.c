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

#define XK_MISCELLANY
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include <X11/keysymdef.h>


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
        XtTranslateKeycode(dpy, (xcb_keycode_t) key, mod, &mod_ret, &sym_ret); \
        (ctx)->keycache.keycode[_i_] = (xcb_keycode_t) (key); \
        (ctx)->keycache.modifiers[_i_] = (unsigned char)(mod); \
        (ctx)->keycache.keysym[_i_] = sym_ret; \
        MOD_RETURN(ctx, key) = (unsigned char)mod_ret; \
    } \
}

#define UPDATE_CACHE(ctx, pd, key, mod, mod_ret, sym_ret) \
{ \
    int _i_ = (((key) - (TMLongCard) (pd)->min_keycode + modmix[(mod) & 0xff]) & \
               (TMKEYCACHESIZE-1)); \
    (ctx)->keycache.keycode[_i_] = (xcb_keycode_t) (key); \
    (ctx)->keycache.modifiers[_i_] = (unsigned char)(mod); \
    (ctx)->keycache.keysym[_i_] = sym_ret; \
    MOD_RETURN(ctx, key) = (unsigned char)(mod_ret); \
}

/* usual number of expected keycodes in XtKeysymToKeycodeList */
#define KEYCODE_ARRAY_SIZE 10

Boolean
_XtComputeLateBindings(xcb_connection_t *dpy,
                       LateBindingsPtr lateModifiers,
                       uint16_t *computed,
                       uint16_t *computedMask)
{
    int i, j, ref;
    ModToKeysymTable *temp;
    XtPerDisplay perDisplay;
    xcb_keysym_t tempKeysym = NoSymbol;

    perDisplay = _XtGetPerDisplay(dpy);
    if (perDisplay == NULL) {
        XtAppWarningMsg(XtDisplayToApplicationContext(dpy),
                        "displayError", "invalidDisplay", XtCXtToolkitError,
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
_XtAllocTMContext(XtPerDisplay pd)
{
    TMKeyContext ctx;

    ctx = (TMKeyContext) _XtHeapAlloc(&pd->heap, sizeof(TMKeyContextRec));
    ctx->event = NULL;
    ctx->serial = 0;
    ctx->keysym = NoSymbol;
    ctx->modifiers = 0;
    FLUSHKEYCACHE(ctx);
    pd->tm_context = ctx;
}

static unsigned int
num_bits(unsigned long mask)
{
    register unsigned long y;

    y = (mask >> 1) & 033333333333;
    y = mask - y - ((y >> 1) & 033333333333);
    return ((unsigned int) (((y + (y >> 3)) & 030707070707) % 077));
}

Boolean
_XtMatchUsingDontCareMods(TMTypeMatch typeMatch,
                          TMModifierMatch modMatch,
                          TMEventPtr eventSeq)
{
    Modifiers modifiers_return;
    xcb_keysym_t keysym_return;
    Modifiers useful_mods;
    Modifiers computed = 0;
    Modifiers computedMask = 0;
    Boolean resolved = TRUE;
    xcb_connection_t *dpy = eventSeq->dpy;
    XtPerDisplay pd;

    if (modMatch->lateModifiers != NULL)
        resolved = _XtComputeLateBindings(dpy, modMatch->lateModifiers,
                                          &computed, &computedMask);
    if (!resolved)
        return FALSE;
    computed = (Modifiers) (computed | modMatch->modifiers);
    computedMask = (Modifiers) (computedMask | modMatch->modifierMask); /* gives do-care mask */

    if ((computed & computedMask) == (eventSeq->event.modifiers & computedMask)) {
        TMKeyContext tm_context;
        int num_modbits;
        int i;
        xcb_keysym_t lower, upper;

        pd = _XtGetPerDisplay(dpy);
        tm_context = pd->tm_context;
        TRANSLATE(tm_context, pd, dpy, (xcb_keycode_t) eventSeq->event.eventCode,
                  (unsigned) 0, modifiers_return, keysym_return);

        XtConvertCase(dpy, keysym_return, &lower, &upper);
        if ((upper & typeMatch->eventCodeMask) == typeMatch->eventCode
            || (lower & typeMatch->eventCodeMask) == typeMatch->eventCode) {
            tm_context->event = eventSeq->xev;
            tm_context->serial = eventSeq->xev->full_sequence;
            tm_context->keysym = keysym_return;
            tm_context->modifiers = (Modifiers) 0;
            return TRUE;
        }
        useful_mods = ~computedMask & modifiers_return;
        if (useful_mods == 0)
            return FALSE;

        switch (num_modbits = (int) num_bits(useful_mods)) {
        case 1:
        case 8:
            /*
             * one modbit should never happen, in fact the implementation
             * of XtTranslateKey and XmTranslateKey guarantee that it
             * won't, so don't care if the loop is set up for the case
             * when one modbit is set.
             * The performance implications of all eight modbits being
             * set is horrendous. This isn't a problem with Xt/Xaw based
             * applications. We can only hope that Motif's virtual
             * modifiers won't result in all eight modbits being set.
             */
            for (i = (int) useful_mods; i > 0; i--) {
                TRANSLATE(tm_context, pd, dpy, eventSeq->event.eventCode,
                          (Modifiers) i, modifiers_return, keysym_return);
                XtConvertCase(dpy, keysym_return, &lower, &upper);
                if ((upper & typeMatch->eventCodeMask) ==
                        (typeMatch->eventCode & typeMatch->eventCodeMask)
                    || (lower & typeMatch->eventCodeMask) ==
                        (typeMatch->eventCode & typeMatch->eventCodeMask)) {
                    tm_context->event = eventSeq->xev;
                    tm_context->serial = eventSeq->xev->full_sequence;
                    tm_context->keysym = keysym_return;
                    tm_context->modifiers = (Modifiers) i;
                    return TRUE;
                }
            }
            break;
        default:               /* (2..7) */
        {
            /*
             * Only translate using combinations of the useful modifiers.
             * to minimize the chance of invalidating the cache.
             */
            static char pows[] = { 0, 1, 3, 7, 15, 31, 63, 127 };
            Modifiers tmod, mod_masks[8];
            int j;

            for (tmod = 1, i = 0; tmod <= (XCB_MOD_MASK_5 << 1); tmod <<= 1)
                if (tmod & useful_mods)
                    mod_masks[i++] = tmod;
            for (j = (int) pows[num_modbits]; j > 0; j--) {
                tmod = 0;
                for (i = 0; i < num_modbits; i++)
                    if (j & (1 << i))
                        tmod |= mod_masks[i];
                TRANSLATE(tm_context, pd, dpy, eventSeq->event.eventCode,
                          tmod, modifiers_return, keysym_return);
                XtConvertCase(dpy, keysym_return, &lower, &upper);
                if ((upper & typeMatch->eventCodeMask) ==
                        (typeMatch->eventCode & typeMatch->eventCodeMask)
                    || (lower & typeMatch->eventCodeMask) ==
                        (typeMatch->eventCode & typeMatch->eventCodeMask)) {
                    tm_context->event = eventSeq->xev;
                    tm_context->serial = eventSeq->xev->full_sequence;
                    tm_context->keysym = keysym_return;
                    tm_context->modifiers = (Modifiers) i;
                    return TRUE;
                }
            }
        }
            break;
        }                       /* switch (num_modbits) */
    }
    return FALSE;
}

void
XtConvertCase(xcb_connection_t *dpy,
              xcb_keysym_t keysym,
              xcb_keysym_t *lower_return,
              xcb_keysym_t *upper_return)
{
    XtPerDisplay pd;
    CaseConverterPtr ptr;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);

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

Boolean
_XtMatchUsingStandardMods(TMTypeMatch typeMatch,
                          TMModifierMatch modMatch,
                          TMEventPtr eventSeq)
{
    uint16_t modifiers_return;
    xcb_keysym_t keysym_return;
    uint16_t computed = 0;
    uint16_t computedMask = 0;
    xcb_connection_t *dpy = eventSeq->dpy;
    XtPerDisplay pd = _XtGetPerDisplay(dpy);
    TMKeyContext tm_context = pd->tm_context;
    uint16_t translateModifiers;

    /* To maximize cache utilization, we mask off nonstandard modifiers
       before cache lookup.  For a given key translator, standard modifiers
       are constant per xcb_keycode_t.  If a key translator uses no standard
       modifiers this implementation will never reference the cache.
     */

    modifiers_return = MOD_RETURN(tm_context, eventSeq->event.eventCode);
    if (!modifiers_return) {
        XtTranslateKeycode(dpy, (xcb_keycode_t) eventSeq->event.eventCode,
                           (Modifiers) eventSeq->event.modifiers,
                           &modifiers_return, &keysym_return);
        translateModifiers =
            (uint16_t) (eventSeq->event.modifiers & modifiers_return);
        UPDATE_CACHE(tm_context, pd, eventSeq->event.eventCode,
                     translateModifiers, modifiers_return, keysym_return);
    }
    else {
        translateModifiers =
            (uint16_t) (eventSeq->event.modifiers & modifiers_return);
        TRANSLATE(tm_context, pd, dpy, (xcb_keycode_t) eventSeq->event.eventCode,
                  translateModifiers, modifiers_return, keysym_return);
    }

    {
        xcb_keysym_t lower, upper;
        XtConvertCase(dpy, keysym_return, &lower, &upper);
        if ((typeMatch->eventCode & typeMatch->eventCodeMask) ==
                (upper & typeMatch->eventCodeMask)
            || (typeMatch->eventCode & typeMatch->eventCodeMask) ==
                (lower & typeMatch->eventCodeMask)) {
        Boolean resolved = TRUE;

        if (modMatch->lateModifiers != NULL)
            resolved = _XtComputeLateBindings(dpy, modMatch->lateModifiers,
                                              &computed, &computedMask);
        if (!resolved)
            return FALSE;
        computed = (uint16_t) (computed | modMatch->modifiers);
        computedMask = (uint16_t) (computedMask | modMatch->modifierMask);

        if ((computed & computedMask) ==
            (eventSeq->event.modifiers & ~modifiers_return & computedMask)) {
            tm_context->event = eventSeq->xev;
            tm_context->serial = eventSeq->xev->full_sequence;
            tm_context->keysym = keysym_return;
            tm_context->modifiers = translateModifiers;
            return TRUE;
        }
    }
    }
    return FALSE;
}

void
_XtBuildKeysymTables(xcb_connection_t *dpy, register XtPerDisplay pd)
{
    xcb_get_modifier_mapping_cookie_t mod_cookie;
    xcb_get_modifier_mapping_reply_t *mod_mapping;
    xcb_keycode_t *modmap;
    int i, j, k;
    int max_keys_per_mod;
    int keysyms_count = 0;
    
    if (pd->keysyms == NULL)
        pd->keysyms = xcb_key_symbols_alloc(dpy);
    
    /* Build modifier to keysym mapping tables if not already built */
    if (pd->modsToKeysyms != NULL)
        return;  /* Already initialized */
    
    /* Get modifier mapping from X server */
    mod_cookie = xcb_get_modifier_mapping(dpy);
    mod_mapping = xcb_get_modifier_mapping_reply(dpy, mod_cookie, NULL);
    
    if (mod_mapping == NULL)
        return;  /* Failed to get modifier mapping */
    
    max_keys_per_mod = mod_mapping->keycodes_per_modifier;
    modmap = xcb_get_modifier_mapping_keycodes(mod_mapping);
    
    /* Allocate the modsToKeysyms table (8 modifiers) */
    pd->modsToKeysyms = (ModToKeysymTable *) __XtCalloc(8, sizeof(ModToKeysymTable));
    
    /* Count total keysyms needed across all modifiers */
    for (i = 0; i < 8; i++) {
        for (j = 0; j < max_keys_per_mod; j++) {
            xcb_keycode_t keycode = modmap[i * max_keys_per_mod + j];
            if (keycode != 0)
                keysyms_count++;
        }
    }
    
    /* Allocate array for all modifier keysyms */
    if (keysyms_count > 0) {
        pd->modKeysyms = (xcb_keysym_t *) __XtMalloc(keysyms_count * sizeof(xcb_keysym_t));
        
        /* Build the tables */
        k = 0;  /* Index into modKeysyms array */
        for (i = 0; i < 8; i++) {
            pd->modsToKeysyms[i].mask = (1 << i);
            pd->modsToKeysyms[i].idx = k;
            pd->modsToKeysyms[i].count = 0;
            
            for (j = 0; j < max_keys_per_mod; j++) {
                xcb_keycode_t keycode = modmap[i * max_keys_per_mod + j];
                if (keycode != 0 && pd->keysyms != NULL) {
                    /* Get the keysym for this keycode (column 0) */
                    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(pd->keysyms, keycode, 0);
                    if (keysym != XCB_NO_SYMBOL) {
                        pd->modKeysyms[k++] = keysym;
                        pd->modsToKeysyms[i].count++;
                    }
                }
            }
        }
    }
    
    free(mod_mapping);
}

void
XtTranslateKey(xcb_connection_t *dpy, _XtKeyCode keycode,
               Modifiers modifiers, Modifiers *modifiers_return,
               xcb_keysym_t *keysym_return)
{
    XtPerDisplay pd;
    xcb_keysym_t sym;
    int col;
    Modifiers mods_consumed = 0;

    DPY_TO_APPCON(dpy);
    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);

    if (pd->keysyms == NULL) {
        *modifiers_return = 0;
        *keysym_return = XK_VoidSymbol;
        UNLOCK_APP(app);
        return;
    }

    /* col 0 = unshifted, col 1 = shifted */
    col = (modifiers & XCB_MOD_MASK_SHIFT) ? 1 : 0;
    if (col == 1)
        mods_consumed |= XCB_MOD_MASK_SHIFT;
    
    sym = xcb_key_symbols_get_keysym(pd->keysyms, keycode, col);

    /* CapsLock: for lowercase alphabetic keys, return uppercase */
    if ((modifiers & XCB_MOD_MASK_LOCK) && col == 0 && sym >= XK_a && sym <= XK_z) {
        xcb_keysym_t usym = xcb_key_symbols_get_keysym(pd->keysyms, keycode, 1);
        if (usym != XCB_NO_SYMBOL) {
            sym = usym;
            mods_consumed |= XCB_MOD_MASK_LOCK;
        }
    }

    /* Fall back to unshifted if the selected column has no symbol */
    if (sym == XCB_NO_SYMBOL) {
        sym = xcb_key_symbols_get_keysym(pd->keysyms, keycode, 0);
        mods_consumed &= ~XCB_MOD_MASK_SHIFT;  /* Shift wasn't actually used */
    }
    if (sym == XCB_NO_SYMBOL)
        sym = XK_VoidSymbol;

    *modifiers_return = mods_consumed;
    *keysym_return = sym;
    UNLOCK_APP(app);
}

void
XtTranslateKeycode(xcb_connection_t *dpy,
                   _XtKeyCode keycode,
                   Modifiers modifiers,
                   Modifiers *modifiers_return,
                   xcb_keysym_t *keysym_return)
{
    XtPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);
    (*pd->defaultKeycodeTranslator) (dpy, keycode, modifiers, modifiers_return,
                                     keysym_return);
    UNLOCK_APP(app);
}

void
XtSetKeyTranslator(xcb_connection_t *dpy, XtKeyProc translator)
{
    XtPerDisplay pd;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);

    pd->defaultKeycodeTranslator = translator;
    FLUSHKEYCACHE(pd->tm_context);
    /* XXX should now redo grabs */
    UNLOCK_APP(app);
}

void
XtRegisterCaseConverter(xcb_connection_t *dpy,
                        XtCaseProc proc,
                        xcb_keysym_t start,
                        xcb_keysym_t stop)
{
    XtPerDisplay pd;
    CaseConverterPtr ptr, prev;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);

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
            XtFree((char *) ptr);
        }
        else
            prev = ptr;
    }
    FLUSHKEYCACHE(pd->tm_context);
    /* XXX should now redo grabs */
    UNLOCK_APP(app);
}

xcb_key_symbols_t *
XtGetKeysymTable(xcb_connection_t *dpy,
                 xcb_keycode_t *min_keycode_return,
                 int *keysyms_per_keycode_return)
{
    XtPerDisplay pd;
    xcb_key_symbols_t *retval;

    DPY_TO_APPCON(dpy);

    LOCK_APP(app);
    pd = _XtGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);
    *min_keycode_return = (xcb_keycode_t) pd->min_keycode;    /* %%% */
    *keysyms_per_keycode_return = pd->keysyms_per_keycode;
    retval = pd->keysyms;
    UNLOCK_APP(app);
    return retval;
}

void
XtKeysymToKeycodeList(xcb_connection_t *dpy,
                      xcb_keysym_t keysym,
                      xcb_keycode_t **keycodes_return,
                      unsigned int *keycount_return)
{
    XtPerDisplay pd;
    unsigned int keycode;
    int per;
    register xcb_keysym_t *syms;
    register int i, j;
    xcb_keysym_t lsym, usym;
    unsigned int maxcodes = 0;
    unsigned int ncodes = 0;
    xcb_keycode_t *keycodes, *codeP = NULL;
    xcb_get_keyboard_mapping_cookie_t cookie;
    xcb_get_keyboard_mapping_reply_t *reply;
    
    DPY_TO_APPCON(dpy);
    LOCK_APP(app);
    
    pd = _XtGetPerDisplay(dpy);
    _InitializeKeysymTables(dpy, pd);
    
    keycodes = NULL;
    
    // Get keyboard mapping using XCB
    cookie = xcb_get_keyboard_mapping(dpy, pd->min_keycode, 
                                       pd->max_keycode - pd->min_keycode + 1);
    reply = xcb_get_keyboard_mapping_reply(dpy, cookie, NULL);
    
    if (!reply) {
        *keycodes_return = NULL;
        *keycount_return = 0;
        UNLOCK_APP(app);
        return;
    }
    
    syms = xcb_get_keyboard_mapping_keysyms(reply);
    per = reply->keysyms_per_keycode;
    
    for (keycode = (unsigned int) pd->min_keycode;
         (int) keycode <= pd->max_keycode; 
         syms += per, keycode++) {
        int match = 0;
        
        for (j = 0; j < per; j++) {
            if (syms[j] == keysym) {
                match = 1;
                break;
            }
        }
        
        if (!match) {
            for (i = 1; i < 5; i += 2) {
                if ((per == i) || ((per > i) && (syms[i] == XCB_NO_SYMBOL))) {
                    XtConvertCase(dpy, syms[i - 1], &lsym, &usym);
                    if ((lsym == keysym) || (usym == keysym)) {
                        match = 1;
                        break;
                    }
                }
            }
        }
        
        if (match) {
            if (ncodes == maxcodes) {
                xcb_keycode_t *old = keycodes;
                maxcodes += KEYCODE_ARRAY_SIZE;
                keycodes = XtMallocArray(maxcodes, sizeof(xcb_keycode_t));
                if (ncodes) {
                    (void) memcpy(keycodes, old, ncodes * sizeof(xcb_keycode_t));
                    XtFree((char *) old);
                }
                codeP = &keycodes[ncodes];
            }
            *codeP++ = (xcb_keycode_t) keycode;
            ncodes++;
        }
    }
    
    free(reply);  // Free the XCB reply
    
    *keycodes_return = keycodes;
    *keycount_return = ncodes;
    UNLOCK_APP(app);
}
