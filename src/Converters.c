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

/* Conversion.c - implementations of resource type conversion procs */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include        "IntrinsicI.h"
#include        "StringDefs.h"
#include        "Shell.h"
#include        "ISWPlatformPrivate.h"
#include        <stdio.h>
#include        <X11/cursorfont.h>
#include        <xcb/xcb_cursor.h>
#include        <X11/keysym.h>
#include        <X11/Xlocale.h>
#include        <fontconfig/fontconfig.h>
#include        <ft2build.h>
#include        FT_FREETYPE_H

#define IsNewline(str) ((str) == '\n')
#define IsWhitespace(str) ((str)== ' ' || (str) == '\t')

static _Xconst _IswString IswNwrongParameters = "wrongParameters";
static _Xconst _IswString IswNconversionError = "conversionError";

/* Representation types */

#define IswQAtom                 XrmPermStringToQuark(IswRAtom)
#define IswQCursor               XrmPermStringToQuark(IswRCursor)
#define IswQDisplay              XrmPermStringToQuark(IswRDisplay)
#define IswQFile                 XrmPermStringToQuark(IswRFile)
#define IswQFloat                XrmPermStringToQuark(IswRFloat)
#define IswQInitialState         XrmPermStringToQuark(IswRInitialState)
#define IswQPixmap               XrmPermStringToQuark(IswRPixmap)
#define IswQShort                XrmPermStringToQuark(IswRShort)
#define IswQUnsignedChar         XrmPermStringToQuark(IswRUnsignedChar)
#define IswQVisual               XrmPermStringToQuark(IswRVisual)

static XrmQuark IswQBool;
static XrmQuark IswQBoolean;
static XrmQuark IswQColor;
static XrmQuark IswQDimension;
static XrmQuark IswQFont;
static XrmQuark IswQFontStruct;
static XrmQuark IswQGravity;
static XrmQuark IswQInt;
static XrmQuark IswQPixel;
static XrmQuark IswQPosition;
XrmQuark _IswQString;

void
_IswConvertInitialize(void)
{
    IswQBool = XrmPermStringToQuark(IswRBool);
    IswQBoolean = XrmPermStringToQuark(IswRBoolean);
    IswQColor = XrmPermStringToQuark(IswRColor);
    IswQDimension = XrmPermStringToQuark(IswRDimension);
    IswQFont = XrmPermStringToQuark(IswRFont);
    IswQFontStruct = XrmPermStringToQuark(IswRFontStruct);
    IswQGravity = XrmPermStringToQuark(IswRGravity);
    IswQInt = XrmPermStringToQuark(IswRInt);
    IswQPixel = XrmPermStringToQuark(IswRPixel);
    IswQPosition = XrmPermStringToQuark(IswRPosition);
    _IswQString = XrmPermStringToQuark(IswRString);
}

#define done_typed_string(type, typed_value, tstr) \
        {                                                       \
            if (toVal->addr != NULL) {                          \
                if (toVal->size < sizeof(type)) {               \
                    toVal->size = sizeof(type);                 \
                    IswDisplayStringConversionWarning(dpy,       \
                        (char*) fromVal->addr, tstr);           \
                    return False;                               \
                }                                               \
                *(type*)(toVal->addr) = typed_value;            \
            }                                                   \
            else {                                              \
                static type static_val;                         \
                static_val = typed_value;                       \
                toVal->addr = (IswPointer)&static_val;            \
            }                                                   \
            toVal->size = sizeof(type);                         \
            return True;                                        \
        }

#define done_string(type, value, tstr) \
        done_typed_string(type, (type) (value), tstr)

#define done_typed(type, typed_value) \
        {                                                       \
            if (toVal->addr != NULL) {                          \
                if (toVal->size < sizeof(type)) {               \
                    toVal->size = sizeof(type);                 \
                    return False;                               \
                }                                               \
                *(type*)(toVal->addr) = typed_value;            \
            }                                                   \
            else {                                              \
                static type static_val;                         \
                static_val = typed_value;                       \
                toVal->addr = (IswPointer)&static_val;            \
            }                                                   \
            toVal->size = sizeof(type);                         \
            return True;                                        \
        }

#define done(type, value) \
        done_typed(type, (type) (value))

void
IswDisplayStringConversionWarning(IswDisplay dpy,
                                 _Xconst char *from,
                                 _Xconst char *toType)
{
#ifndef NO_MIT_HACKS
    /* Allow suppression of conversion warnings. %%%  Not specified. */

    static enum { Check, Report, Ignore } report_it = Check;
    IswAppContext app = IswDisplayToApplicationContext(dpy);

    LOCK_APP(app);
    LOCK_PROCESS;
    if (report_it == Check) {
        XrmDatabase rdb = IswDatabase(dpy);
        XrmName xrm_name[2];
        XrmClass xrm_class[2];
        XrmRepresentation rep_type;
        XrmValue value;

        xrm_name[0] = XrmPermStringToQuark("stringConversionWarnings");
        xrm_name[1] = 0;
        xrm_class[0] = XrmPermStringToQuark("StringConversionWarnings");
        xrm_class[1] = 0;
        if (XrmQGetResource(rdb, xrm_name, xrm_class, &rep_type, &value)) {
            if (rep_type == IswQBoolean)
                report_it = *(Boolean *) value.addr ? Report : Ignore;
            else if (rep_type == _IswQString) {
                XrmValue toVal;
                Boolean report = False;

                toVal.addr = (IswPointer) &report;
                toVal.size = sizeof(Boolean);
                if (IswCallConverter
                    (dpy, IswCvtStringToBoolean, (XrmValuePtr) NULL,
                     (Cardinal) 0, &value, &toVal, (IswCacheRef *) NULL))
                    report_it = report ? Report : Ignore;
            }
            else
                report_it = Report;
        }
        else
            report_it = Report;
    }

    if (report_it == Report) {
#endif                          /* ifndef NO_MIT_HACKS */
        String params[2];
        Cardinal num_params = 2;

        params[0] = (String) from;
        params[1] = (String) toType;
        IswAppWarningMsg(app,
                        IswNconversionError, "string", IswCIswToolkitError,
                        "Cannot convert string \"%s\" to type %s",
                        params, &num_params);
#ifndef NO_MIT_HACKS
    }
#endif                          /* ifndef NO_MIT_HACKS */
    UNLOCK_PROCESS;
    UNLOCK_APP(app);
}

static int CompareISOLatin1(const char *, const char *);

static Boolean
IsInteger(String string, int *value)
{
    Boolean foundDigit = False;
    Boolean isNegative = False;
    Boolean isPositive = False;
    int val = 0;
    char ch;

    /* skip leading whitespace */
    while ((ch = *string) == ' ' || ch == '\t')
        string++;
    while ((ch = *string++)) {
        if (ch >= '0' && ch <= '9') {
            val *= 10;
            val += ch - '0';
            foundDigit = True;
            continue;
        }
        if (IsWhitespace(ch)) {
            if (!foundDigit)
                return False;
            /* make sure only trailing whitespace */
            while ((ch = *string++)) {
                if (!IsWhitespace(ch))
                    return False;
            }
            break;
        }
        if (ch == '-' && !foundDigit && !isNegative && !isPositive) {
            isNegative = True;
            continue;
        }
        if (ch == '+' && !foundDigit && !isNegative && !isPositive) {
            isPositive = True;
            continue;
        }
        return False;
    }
    if (ch == '\0') {
        if (isNegative)
            *value = -val;
        else
            *value = val;
        return True;
    }
    return False;
}

Boolean
IswCvtIntToBoolean(IswDisplay dpy,
                  XrmValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToBoolean",
                        IswCIswToolkitError,
                        "Integer to Boolean conversion needs no extra arguments",
                        NULL, NULL);
    done(Boolean, (*(int *) fromVal->addr != 0));
}

Boolean
IswCvtIntToShort(IswDisplay dpy,
                XrmValuePtr args _X_UNUSED,
                Cardinal *num_args,
                XrmValuePtr fromVal,
                XrmValuePtr toVal,
                IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToShort", IswCIswToolkitError,
                        "Integer to Short conversion needs no extra arguments",
                        NULL, NULL);
    done(short, (*(int *) fromVal->addr));
}

Boolean
IswCvtStringToBoolean(IswDisplay dpy,
                     XrmValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     XrmValuePtr fromVal,
                     XrmValuePtr toVal,
                     IswPointer *closure_ret _X_UNUSED)
{
    String str = (String) fromVal->addr;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToBoolean",
                        IswCIswToolkitError,
                        "String to Boolean conversion needs no extra arguments",
                        NULL, NULL);

    if ((CompareISOLatin1(str, "true") == 0)
        || (CompareISOLatin1(str, "yes") == 0)
        || (CompareISOLatin1(str, "on") == 0)
        || (CompareISOLatin1(str, "1") == 0))
        done_string(Boolean, True, IswRBoolean);

    if ((CompareISOLatin1(str, "false") == 0)
        || (CompareISOLatin1(str, "no") == 0)
        || (CompareISOLatin1(str, "off") == 0)
        || (CompareISOLatin1(str, "0") == 0))
        done_string(Boolean, False, IswRBoolean);

    IswDisplayStringConversionWarning(dpy, str, IswRBoolean);
    return False;
}

Boolean
IswCvtIntToBool(IswDisplay dpy,
               XrmValuePtr args _X_UNUSED,
               Cardinal *num_args,
               XrmValuePtr fromVal,
               XrmValuePtr toVal,
               IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToBool", IswCIswToolkitError,
                        "Integer to Bool conversion needs no extra arguments",
                        NULL, NULL);
    done(Bool, (*(int *) fromVal->addr != 0));
}

Boolean
IswCvtStringToBool(IswDisplay dpy,
                  XrmValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    String str = (String) fromVal->addr;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToBool",
                        IswCIswToolkitError,
                        "String to Bool conversion needs no extra arguments",
                        NULL, NULL);

    if ((CompareISOLatin1(str, "true") == 0)
        || (CompareISOLatin1(str, "yes") == 0)
        || (CompareISOLatin1(str, "on") == 0)
        || (CompareISOLatin1(str, "1") == 0))
        done_string(Bool, True, IswRBool);

    if ((CompareISOLatin1(str, "false") == 0)
        || (CompareISOLatin1(str, "no") == 0)
        || (CompareISOLatin1(str, "off") == 0)
        || (CompareISOLatin1(str, "0") == 0))
        done_string(Bool, False, IswRBool);

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRBool);
    return False;
}

/* *INDENT-OFF* */
IswConvertArgRec const colorConvertArgs[] = {
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
     sizeof(xcb_screen_t *)},
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.colormap),
     sizeof(IswColormap)}
};
/* *INDENT-ON* */

Boolean
IswCvtIntToColor(IswDisplay dpy,
                XrmValuePtr args,
                Cardinal *num_args,
                XrmValuePtr fromVal,
                XrmValuePtr toVal,
                IswPointer *closure_ret _X_UNUSED)
{
    IswColormap colormap;

    if (*num_args != 2) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntOrPixelToXColor",
                        IswCIswToolkitError,
                        "Pixel to color conversion needs screen and colormap arguments",
                        NULL, NULL);
        return False;
    }
    colormap = *((IswColormap *) args[1].addr);

    {
        unsigned long pixel = (unsigned long) (uint32_t) (*(int *) fromVal->addr);
        IswColor c;

        if (!_IswPlatformQueryColor(dpy, colormap, pixel, &c))
            return False;
        done_typed(IswColor, c);
    }
}

Boolean
IswCvtStringToPixel(IswDisplay dpy,
                   XrmValuePtr args,
                   Cardinal *num_args,
                   XrmValuePtr fromVal,
                   XrmValuePtr toVal,
                   IswPointer *closure_ret)
{
    String str = (String) fromVal->addr;
    xcb_screen_t *screen;
    IswPerDisplay pd = _IswGetPerDisplay(dpy);
    IswColormap colormap;
    Cardinal num_params = 1;

    if (*num_args != 2) {
        IswAppWarningMsg(pd->appContext, IswNwrongParameters, "cvtStringToPixel",
                        IswCIswToolkitError,
                        "String to pixel conversion needs screen and colormap arguments",
                        NULL, NULL);
        return False;
    }

    screen = *((xcb_screen_t **) args[0].addr);
    colormap = *((IswColormap *) args[1].addr);

    if (CompareISOLatin1(str, IswDefaultBackground) == 0) {
        *closure_ret = NULL;
        if (pd->rv) {
            done_string(Pixel, BlackPixelOfScreen(screen), IswRPixel);
        }
        else {
            done_string(Pixel, WhitePixelOfScreen(screen), IswRPixel);
        }
    }
    if (CompareISOLatin1(str, IswDefaultForeground) == 0) {
        *closure_ret = NULL;
        if (pd->rv) {
            done_string(Pixel, WhitePixelOfScreen(screen), IswRPixel);
        }
        else {
            done_string(Pixel, BlackPixelOfScreen(screen), IswRPixel);
        }
    }

    /* Handle #RGB, #RRGGBB, #RRRRGGGGBBBB hex color specifications */
    if (str[0] == '#') {
        size_t len = strlen(str + 1);
        unsigned int r = 0, g = 0, b = 0;
        uint16_t red, green, blue;

        if (len == 3 &&
            sscanf(str + 1, "%1x%1x%1x", &r, &g, &b) == 3) {
            red   = (uint16_t)(r * 0x1111);
            green = (uint16_t)(g * 0x1111);
            blue  = (uint16_t)(b * 0x1111);
        } else if (len == 6 &&
                   sscanf(str + 1, "%2x%2x%2x", &r, &g, &b) == 3) {
            red   = (uint16_t)(r << 8 | r);
            green = (uint16_t)(g << 8 | g);
            blue  = (uint16_t)(b << 8 | b);
        } else if (len == 12 &&
                   sscanf(str + 1, "%4x%4x%4x", &r, &g, &b) == 3) {
            red   = (uint16_t)r;
            green = (uint16_t)g;
            blue  = (uint16_t)b;
        } else {
            String params[1];
            params[0] = str;
            IswAppWarningMsg(pd->appContext, "badValue", "cvtStringToPixel",
                            IswCIswToolkitError,
                            "Color name \"%s\" is not defined",
                            params, &num_params);
            *closure_ret = NULL;
            return False;
        }

        unsigned long result_pixel;
        if (_IswPlatformAllocColor(dpy, colormap, red, green, blue,
                                   &result_pixel)) {
            *closure_ret = (char *) True;
            done_string(Pixel, (Pixel) result_pixel, IswRPixel);
        }

        String params[1];
        params[0] = str;
        IswAppWarningMsg(pd->appContext, "noColormap", "cvtStringToPixel",
                        IswCIswToolkitError,
                        "Cannot allocate colormap entry for \"%s\"",
                        params, &num_params);
        *closure_ret = NULL;
        return False;
    }

    {
        unsigned long result_pixel;
        if (_IswPlatformAllocNamedColor(dpy, colormap, str, &result_pixel)) {
            *closure_ret = (char *) True;
            done_string(Pixel, (Pixel) result_pixel, IswRPixel);
        }

        /* Allocation failed — check if name is valid */
        _Xconst _IswString msg;
        _Xconst _IswString type;
        String params[1];

        params[0] = str;
        if (_IswPlatformLookupColor(dpy, colormap, str)) {
            type = "noColormap";
            msg = "Cannot allocate colormap entry for \"%s\"";
        } else {
            type = "badValue";
            msg = "Color name \"%s\" is not defined";
        }

        IswAppWarningMsg(pd->appContext, type, "cvtStringToPixel",
                        IswCIswToolkitError, msg, params, &num_params);
        *closure_ret = NULL;
        return False;
    }
}

static void
FreePixel(IswAppContext app,
          XrmValuePtr toVal,
          IswPointer closure,
          XrmValuePtr args,
          Cardinal *num_args)
{
    xcb_screen_t *screen;
    IswColormap colormap;

    if (*num_args != 2) {
        IswAppWarningMsg(app, IswNwrongParameters, "freePixel", IswCIswToolkitError,
                        "Freeing a pixel requires screen and colormap arguments",
                        NULL, NULL);
        return;
    }

    screen = *((xcb_screen_t **) args[0].addr);
    colormap = *((IswColormap *) args[1].addr);

    if (closure) {
        IswDisplay dpy = _IswConnectionOfScreen((IswScreen) screen);
        unsigned long pixel = *(uint32_t *) toVal->addr;
        _IswPlatformFreeColors(dpy, colormap, pixel);
    }
}

/* no longer used by Xt, but it's in the spec */
IswConvertArgRec const screenConvertArg[] = {
    {IswWidgetBaseOffset, (IswPointer) IswOffsetOf(WidgetRec, core.screen),
     sizeof(xcb_screen_t *)}
};

static void
FetchDisplayArg(Widget widget, Cardinal *size _X_UNUSED, XrmValue *value)
{
    if (widget == NULL) {
        IswErrorMsg("missingWidget", "fetchDisplayArg", IswCIswToolkitError,
                   "FetchDisplayArg called without a widget to reference",
                   NULL, NULL);
        /* can't return any useful Display and caller will de-ref NULL,
           so aborting is the only useful option */
    }
    else {
        static IswDisplay _fetch_dpy;
        Boolean isWidget = IswIsWidget(widget);
        if (!isWidget) {
            Widget parent = IswParent(widget);
            if (parent && IswIsWidget(parent)) {
            }
        } else {
        }
        _fetch_dpy = IswDisplayOfObject(widget);
        value->size = sizeof(IswDisplay);
        value->addr = (IswPointer) &_fetch_dpy;
    }
}

/* *INDENT-OFF* */
static IswConvertArgRec const displayConvertArg[] = {
    {IswProcedureArg, (IswPointer)FetchDisplayArg, 0},
};

static IswConvertArgRec const cursorConvertArgs[] = {
    {IswProcedureArg, (IswPointer)FetchDisplayArg, 0},
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
     sizeof(xcb_screen_t *)},
};
/* *INDENT-ON* */

/* -----------------------------------------------------------------------
 * Load a theme-aware named cursor (glyph fallback).  Thin wrapper over the
 * platform cursor op; the glyph/themed creation now lives in the backend
 * (ISWPlatformGrabCursorXCB.c).  The raw conn/screen params are seam-internal
 * (this is an internal header, not a public ISW/ widget header).
 * ----------------------------------------------------------------------- */
IswCursor
_IswLoadThemedCursor(IswDisplay dpy, IswScreen screen,
                    const char *name, unsigned int shape)
{
    return _IswPlatformLoadNamedCursor(dpy, screen, name, shape);
}

/* -----------------------------------------------------------------------
 * Resolve a fontconfig family name (optionally with "-size" suffix) into
 * an IswFontStruct with font_family set for Cairo rendering.  fid is 0
 * since rendering goes through Cairo, not core X11 fonts.
 * ----------------------------------------------------------------------- */
static IswFontStruct *
_IswLoadFontconfigFont(const char *name)
{
    double pt_size = 10.0;
    FcPattern *pattern = NULL, *match = NULL;
    FcResult result;
    FcChar8 *font_file = NULL;
    FcChar8 *matched_family = NULL;
    int weight = FC_WEIGHT_NORMAL;
    int slant = FC_SLANT_ROMAN;
    FT_Library ft_lib = NULL;
    FT_Face ft_face = NULL;
    IswFontStruct *fs = NULL;

    /* Parse fontconfig name format: "Family-Size:weight=bold:slant=italic" */
    pattern = FcNameParse((const FcChar8 *)name);
    if (!pattern) return NULL;
    /* Prefer scalable (outline) fonts — bitmap fonts like "fixed" become
     * fuzzy when scaled to non-native sizes under HiDPI. */
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    match = FcFontMatch(NULL, pattern, &result);
    if (!match) goto cleanup;

    if (FcPatternGetString(match, FC_FILE, 0, &font_file) != FcResultMatch)
        goto cleanup;

    /* Extract resolved properties from the matched pattern */
    if (FcPatternGetString(match, FC_FAMILY, 0, &matched_family) != FcResultMatch)
        matched_family = (FcChar8 *)"Sans";
    if (FcPatternGetInteger(match, FC_WEIGHT, 0, &weight) != FcResultMatch)
        weight = FC_WEIGHT_NORMAL;
    if (FcPatternGetInteger(match, FC_SLANT, 0, &slant) != FcResultMatch)
        slant = FC_SLANT_ROMAN;

    /* Extract size from the parsed pattern (default 10pt) */
    if (FcPatternGetDouble(match, FC_SIZE, 0, &pt_size) != FcResultMatch)
        pt_size = 10.0;

    /* Load with FreeType to get metrics at the requested size */
    if (FT_Init_FreeType(&ft_lib) != 0) goto cleanup;
    if (FT_New_Face(ft_lib, (const char *)font_file, 0, &ft_face) != 0)
        goto cleanup;

    /* Set character size: FreeType uses 1/64th of a point units */
    FT_Set_Char_Size(ft_face, 0, (FT_F26Dot6)(pt_size * 64), 96, 96);

    fs = IswNew(IswFontStruct);
    fs->fid       = 0;
    fs->ascent    = (int)(ft_face->size->metrics.ascender >> 6);
    fs->descent   = (int)(-(ft_face->size->metrics.descender >> 6));
    fs->min_char_or_byte2 = 0;
    fs->max_char_or_byte2 = 0;
    fs->min_byte1 = 0;
    fs->max_byte1 = 0;
    fs->font_family = IswNewString((const char *)matched_family);
    fs->font_weight = weight;
    fs->font_slant  = slant;
    fs->pt_size     = pt_size;

cleanup:
    if (ft_face) FT_Done_Face(ft_face);
    if (ft_lib) FT_Done_FreeType(ft_lib);
    if (match) FcPatternDestroy(match);
    if (pattern) FcPatternDestroy(pattern);
    return fs;
}

/* -----------------------------------------------------------------------
 * XCB replacement for XLoadFont(display, name)
 * ----------------------------------------------------------------------- */
static IswFontId
_IswLoadFont(IswDisplay dpy, const char *name)
{
    return _IswPlatformLoadFont(dpy, name);
}

/* -----------------------------------------------------------------------
 * XCB replacement for XFreeFont(display, fontstruct)
 * ----------------------------------------------------------------------- */
static void
_IswFreeFont(IswDisplay dpy, IswFontStruct *fs)
{
    if (fs == NULL) return;
    if (fs->fid != 0)
        _IswPlatformFreeFont(dpy, fs->fid);
    if (fs->font_family)
        IswFree(fs->font_family);
    IswFree((char *) fs);
}

/* XMatchVisualInfo equivalent now lives in the color/font backend
   (ISWPlatformColorFontXCB.c) behind the match_visual_info op.  Phase 4. */

Boolean
IswCvtStringToCursor(IswDisplay dpy,
                    XrmValuePtr args,
                    Cardinal *num_args,
                    XrmValuePtr fromVal,
                    XrmValuePtr toVal,
                    IswPointer *closure_ret _X_UNUSED)
{
    /* *INDENT-OFF* */
    static const struct _CursorName {
        const char      *name;
        unsigned int    shape;
    } cursor_names[] = {
        {"X_cursor",            XC_X_cursor},
        {"arrow",               XC_arrow},
        {"based_arrow_down",    XC_based_arrow_down},
        {"based_arrow_up",      XC_based_arrow_up},
        {"boat",                XC_boat},
        {"bogosity",            XC_bogosity},
        {"bottom_left_corner",  XC_bottom_left_corner},
        {"bottom_right_corner", XC_bottom_right_corner},
        {"bottom_side",         XC_bottom_side},
        {"bottom_tee",          XC_bottom_tee},
        {"box_spiral",          XC_box_spiral},
        {"center_ptr",          XC_center_ptr},
        {"circle",              XC_circle},
        {"clock",               XC_clock},
        {"coffee_mug",          XC_coffee_mug},
        {"cross",               XC_cross},
        {"cross_reverse",       XC_cross_reverse},
        {"crosshair",           XC_crosshair},
        {"diamond_cross",       XC_diamond_cross},
        {"dot",                 XC_dot},
        {"dotbox",              XC_dotbox},
        {"double_arrow",        XC_double_arrow},
        {"draft_large",         XC_draft_large},
        {"draft_small",         XC_draft_small},
        {"draped_box",          XC_draped_box},
        {"exchange",            XC_exchange},
        {"fleur",               XC_fleur},
        {"gobbler",             XC_gobbler},
        {"gumby",               XC_gumby},
        {"hand1",               XC_hand1},
        {"hand2",               XC_hand2},
        {"heart",               XC_heart},
        {"icon",                XC_icon},
        {"iron_cross",          XC_iron_cross},
        {"left_ptr",            XC_left_ptr},
        {"left_side",           XC_left_side},
        {"left_tee",            XC_left_tee},
        {"leftbutton",          XC_leftbutton},
        {"ll_angle",            XC_ll_angle},
        {"lr_angle",            XC_lr_angle},
        {"man",                 XC_man},
        {"middlebutton",        XC_middlebutton},
        {"mouse",               XC_mouse},
        {"pencil",              XC_pencil},
        {"pirate",              XC_pirate},
        {"plus",                XC_plus},
        {"question_arrow",      XC_question_arrow},
        {"right_ptr",           XC_right_ptr},
        {"right_side",          XC_right_side},
        {"right_tee",           XC_right_tee},
        {"rightbutton",         XC_rightbutton},
        {"rtl_logo",            XC_rtl_logo},
        {"sailboat",            XC_sailboat},
        {"sb_down_arrow",       XC_sb_down_arrow},
        {"sb_h_double_arrow",   XC_sb_h_double_arrow},
        {"sb_left_arrow",       XC_sb_left_arrow},
        {"sb_right_arrow",      XC_sb_right_arrow},
        {"sb_up_arrow",         XC_sb_up_arrow},
        {"sb_v_double_arrow",   XC_sb_v_double_arrow},
        {"shuttle",             XC_shuttle},
        {"sizing",              XC_sizing},
        {"spider",              XC_spider},
        {"spraycan",            XC_spraycan},
        {"star",                XC_star},
        {"target",              XC_target},
        {"tcross",              XC_tcross},
        {"top_left_arrow",      XC_top_left_arrow},
        {"top_left_corner",     XC_top_left_corner},
        {"top_right_corner",    XC_top_right_corner},
        {"top_side",            XC_top_side},
        {"top_tee",             XC_top_tee},
        {"trek",                XC_trek},
        {"ul_angle",            XC_ul_angle},
        {"umbrella",            XC_umbrella},
        {"ur_angle",            XC_ur_angle},
        {"watch",               XC_watch},
        {"xterm",               XC_xterm},
    };
    /* *INDENT-ON* */
    const struct _CursorName *nP;
    char *name = (char *) fromVal->addr;
    register Cardinal i;

    if (*num_args != 2) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToCursor",
                        IswCIswToolkitError,
                        "String to cursor conversion needs display and screen arguments",
                        NULL, NULL);
        return False;
    }

    for (i = 0, nP = cursor_names; i < IswNumber(cursor_names); i++, nP++) {
        if (strcmp(name, nP->name) == 0) {
            IswScreen screen = *(IswScreen *) args[1].addr;
            IswCursor cursor = _IswLoadThemedCursor(dpy, screen,
                                                    nP->name, nP->shape);

            done_string(IswCursor, cursor, IswRCursor);
        }
    }
    IswDisplayStringConversionWarning(dpy, name, IswRCursor);
    return False;
}

static void
FreeCursor(IswAppContext app,
           XrmValuePtr toVal,
           IswPointer closure _X_UNUSED,
           XrmValuePtr args,
           Cardinal *num_args)
{
    IswDisplay display;

    if (*num_args != 1) {
        IswAppWarningMsg(app,
                        IswNwrongParameters, "freeCursor", IswCIswToolkitError,
                        "Free Cursor requires display argument", NULL, NULL);
        return;
    }

    display = *(IswDisplay *) args[0].addr;
    _IswPlatformFreeCursor(display, *(IswCursor *) toVal->addr);
}

Boolean
IswCvtStringToDisplay(IswDisplay dpy,
                     XrmValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     XrmValuePtr fromVal,
                     XrmValuePtr toVal,
                     IswPointer *closure_ret _X_UNUSED)
{
    xcb_connection_t *d;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToDisplay",
                        IswCIswToolkitError,
                        "String to Display conversion needs no extra arguments",
                        NULL, NULL);

    {
        int screen_num = 0;
        d = xcb_connect((char *) fromVal->addr, &screen_num);
        if (d != NULL && xcb_connection_has_error(d) == 0)
            done_string(xcb_connection_t *, d, IswRDisplay);
        if (d != NULL)
            xcb_disconnect(d);
    }

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRDisplay);
    return False;
}

Boolean
IswCvtStringToFile(IswDisplay dpy,
                  XrmValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    FILE *f;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToFile",
                        IswCIswToolkitError,
                        "String to File conversion needs no extra arguments",
                        NULL, NULL);

    f = fopen((char *) fromVal->addr, "r");
    if (f != NULL)
        done_string(FILE *, f, IswRFile);

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRFile);
    return False;
}

static void
FreeFile(IswAppContext app,
         XrmValuePtr toVal,
         IswPointer closure _X_UNUSED,
         XrmValuePtr args _X_UNUSED,
         Cardinal *num_args)
{
    if (*num_args != 0)
        IswAppWarningMsg(app,
                        IswNwrongParameters, "freeFile", IswCIswToolkitError,
                        "Free File requires no extra arguments", NULL, NULL);

    fclose(*(FILE **) toVal->addr);
}

Boolean
IswCvtIntToFloat(IswDisplay dpy,
                XrmValuePtr args _X_UNUSED,
                Cardinal *num_args,
                XrmValuePtr fromVal,
                XrmValuePtr toVal,
                IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToFloat", IswCIswToolkitError,
                        "Integer to Float conversion needs no extra arguments",
                        NULL, NULL);
    done(float, (*(int *) fromVal->addr));
}

Boolean
IswCvtStringToFloat(IswDisplay dpy,
                   XrmValuePtr args _X_UNUSED,
                   Cardinal *num_args,
                   XrmValuePtr fromVal,
                   XrmValuePtr toVal,
                   IswPointer *closure_ret _X_UNUSED)
{
    int ret;
    float f, nan = 0.0;

    (void) sscanf("NaN", "%g",
                  toVal->addr != NULL ? (float *) toVal->addr : &nan);

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToFloat",
                        IswCIswToolkitError,
                        "String to Float conversion needs no extra arguments",
                        NULL, NULL);

    ret = sscanf(fromVal->addr, "%g", &f);
    if (ret == 0) {
        if (toVal->addr != NULL && toVal->size == sizeof nan)
            *(float *) toVal->addr = nan;
        IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRFloat);
        return False;
    }
    done_string(float, f, IswRFloat);
}

Boolean
IswCvtStringToFont(IswDisplay dpy,
                  XrmValuePtr args,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    IswFontId f;
    IswDisplay display;

    if (*num_args != 1) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToFont",
                        IswCIswToolkitError,
                        "String to font conversion needs display argument",
                        NULL, NULL);
        return False;
    }

    display = *(IswDisplay *) args[0].addr;


    if (CompareISOLatin1((String) fromVal->addr, IswDefaultFont) != 0) {
        f = _IswLoadFont(display, (char *) fromVal->addr);

        if (f != 0) {
 Done:     done_string(IswFontId, f, IswRFont);
        }
        IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRFont);
    } else {
    }
    /* try and get the default font */

    {
        XrmName xrm_name[2];
        XrmClass xrm_class[2];
        XrmRepresentation rep_type;
        XrmValue value;

        xrm_name[0] = XrmPermStringToQuark("xtDefaultFont");
        xrm_name[1] = 0;
        xrm_class[0] = XrmPermStringToQuark("IswDefaultFont");
        xrm_class[1] = 0;
        if (XrmQGetResource(IswDatabase((IswDisplay) display), xrm_name, xrm_class,
                            &rep_type, &value)) {
            if (rep_type == _IswQString) {
                f = _IswLoadFont(display, (char *) value.addr);

                if (f != 0)
                    goto Done;
                else
                    IswDisplayStringConversionWarning(dpy, (char *) value.addr,
                                                     IswRFont);
            }
            else if (rep_type == IswQFont) {
                f = *(IswFontId *) value.addr;
                goto Done;
            }
            else if (rep_type == IswQFontStruct) {
                f = ((IswFontStruct *) value.addr)->fid;
                goto Done;
            }
        } else {
        }
    }
    /* Should really do XListFonts, but most servers support this */
    f = _IswLoadFont(display, "-*-*-*-R-*-*-*-120-*-*-*-*-ISO8859-*");

    if (f != 0) {
        goto Done;
    }

    IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                    "noFont", "cvtStringToFont", IswCIswToolkitError,
                    "Unable to load any usable ISO8859 font", NULL, NULL);

    return False;
}

static void
FreeFont(IswAppContext app,
         XrmValuePtr toVal,
         IswPointer closure _X_UNUSED,
         XrmValuePtr args,
         Cardinal *num_args)
{
    IswDisplay display;

    if (*num_args != 1) {
        IswAppWarningMsg(app,
                        IswNwrongParameters, "freeFont", IswCIswToolkitError,
                        "Free Font needs display argument", NULL, NULL);
        return;
    }

    display = *(IswDisplay *) args[0].addr;
    _IswPlatformFreeFont(display, *(IswFontId *) toVal->addr);
}

Boolean
IswCvtIntToFont(IswDisplay dpy,
               XrmValuePtr args _X_UNUSED,
               Cardinal *num_args,
               XrmValuePtr fromVal,
               XrmValuePtr toVal,
               IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToFont", IswCIswToolkitError,
                        "Integer to Font conversion needs no extra arguments",
                        NULL, NULL);
    done(IswFontId, *(int *) fromVal->addr);
}

Boolean
IswCvtStringToFontStruct(IswDisplay dpy,
                        XrmValuePtr args,
                        Cardinal *num_args,
                        XrmValuePtr fromVal,
                        XrmValuePtr toVal,
                        IswPointer *closure_ret _X_UNUSED)
{
    IswFontStruct *f;
    IswDisplay display;

    if (*num_args != 1) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToFontStruct",
                        IswCIswToolkitError,
                        "String to font conversion needs display argument",
                        NULL, NULL);
        return False;
    }

    display = *(IswDisplay *) args[0].addr;

    if (CompareISOLatin1((String) fromVal->addr, IswDefaultFont) != 0) {
        f = _IswLoadFontconfigFont((const char *) fromVal->addr);
        if (f != NULL) {
 Done:      done_string(IswFontStruct *, f, IswRFontStruct);
        }
        IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr,
                                         IswRFontStruct);
    }

    /* IswDefaultFont or explicit name failed — check xtDefaultFont resource */
    {
        XrmName xrm_name[2];
        XrmClass xrm_class[2];
        XrmRepresentation rep_type;
        XrmValue value;

        xrm_name[0] = XrmPermStringToQuark("xtDefaultFont");
        xrm_name[1] = 0;
        xrm_class[0] = XrmPermStringToQuark("IswDefaultFont");
        xrm_class[1] = 0;
        if (XrmQGetResource(IswDatabase((IswDisplay) display), xrm_name, xrm_class,
                            &rep_type, &value)) {
            if (rep_type == _IswQString) {
                f = _IswLoadFontconfigFont((const char *) value.addr);
                if (f != NULL) goto Done;
                IswDisplayStringConversionWarning(dpy, (char *) value.addr,
                                                 IswRFontStruct);
            }
            else if (rep_type == IswQFontStruct) {
                f = (IswFontStruct *) value.addr;
                goto Done;
            }
        }
    }

    /* Default: Sans 10pt */
    f = _IswLoadFontconfigFont("Sans-10");
    if (f != NULL)
        goto Done;

    IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                    "noFont", "cvtStringToFontStruct", IswCIswToolkitError,
                    "Unable to load any usable font via fontconfig", NULL, NULL);

    return False;
}

static void
FreeFontStruct(IswAppContext app,
               XrmValuePtr toVal,
               IswPointer closure _X_UNUSED,
               XrmValuePtr args,
               Cardinal *num_args)
{
    IswDisplay display;

    if (*num_args != 1) {
        IswAppWarningMsg(app,
                        IswNwrongParameters, "freeFontStruct", IswCIswToolkitError,
                        "Free FontStruct requires display argument",
                        NULL, NULL);
        return;
    }

    display = *(IswDisplay *) args[0].addr;
    _IswFreeFont(display, *(IswFontStruct **) toVal->addr);
}

Boolean
IswCvtStringToInt(IswDisplay dpy,
                 XrmValuePtr args _X_UNUSED,
                 Cardinal *num_args,
                 XrmValuePtr fromVal,
                 XrmValuePtr toVal,
                 IswPointer *closure_ret _X_UNUSED)
{
    int i;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToInt", IswCIswToolkitError,
                        "String to Integer conversion needs no extra arguments",
                        NULL, NULL);
    if (IsInteger((String) fromVal->addr, &i))
        done_string(int, i, IswRInt);

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRInt);
    return False;
}

Boolean
IswCvtStringToShort(IswDisplay dpy,
                   XrmValuePtr args _X_UNUSED,
                   Cardinal *num_args,
                   XrmValuePtr fromVal,
                   XrmValuePtr toVal,
                   IswPointer *closure_ret _X_UNUSED)
{
    int i;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToShort",
                        IswCIswToolkitError,
                        "String to Integer conversion needs no extra arguments",
                        NULL, NULL);
    if (IsInteger((String) fromVal->addr, &i))
        done_string(short, (short) i, IswRShort);

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRShort);
    return False;
}

Boolean
IswCvtStringToDimension(IswDisplay dpy,
                       XrmValuePtr args _X_UNUSED,
                       Cardinal *num_args,
                       XrmValuePtr fromVal,
                       XrmValuePtr toVal,
                       IswPointer *closure_ret _X_UNUSED)
{
    int i;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToDimension",
                        IswCIswToolkitError,
                        "String to Dimension conversion needs no extra arguments",
                        NULL, NULL);
    if (IsInteger((String) fromVal->addr, &i)) {
        if (i < 0)
            IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr,
                                             IswRDimension);
        done_string(Dimension, (Dimension) i, IswRDimension);
    }
    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRDimension);
    return False;
}

Boolean
IswCvtIntToUnsignedChar(IswDisplay dpy,
                       XrmValuePtr args _X_UNUSED,
                       Cardinal *num_args,
                       XrmValuePtr fromVal,
                       XrmValuePtr toVal,
                       IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToUnsignedChar",
                        IswCIswToolkitError,
                        "Integer to UnsignedChar conversion needs no extra arguments",
                        NULL, NULL);
    done(unsigned char, (*(int *) fromVal->addr));
}

Boolean
IswCvtStringToUnsignedChar(IswDisplay dpy,
                          XrmValuePtr args _X_UNUSED,
                          Cardinal *num_args,
                          XrmValuePtr fromVal,
                          XrmValuePtr toVal,
                          IswPointer *closure_ret _X_UNUSED)
{
    int i;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToUnsignedChar",
                        IswCIswToolkitError,
                        "String to Integer conversion needs no extra arguments",
                        NULL, NULL);
    if (IsInteger((String) fromVal->addr, &i)) {
        if (i < 0 || i > 255)
            IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr,
                                             IswRUnsignedChar);
        done_string(unsigned char, i, IswRUnsignedChar);
    }
    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr,
                                     IswRUnsignedChar);
    return False;
}

Boolean
IswCvtColorToPixel(IswDisplay dpy,
                  XrmValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtXColorToPixel",
                        IswCIswToolkitError,
                        "Color to Pixel conversion needs no extra arguments",
                        NULL, NULL);
    done(Pixel, ((IswColor *) fromVal->addr)->pixel);
}

Boolean
IswCvtIntToPixel(IswDisplay dpy,
                XrmValuePtr args _X_UNUSED,
                Cardinal *num_args,
                XrmValuePtr fromVal,
                XrmValuePtr toVal,
                IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToPixel", IswCIswToolkitError,
                        "Integer to Pixel conversion needs no extra arguments",
                        NULL, NULL);
    done(Pixel, *(int *) fromVal->addr);
}

Boolean
IswCvtIntToPixmap(IswDisplay dpy,
                 XrmValuePtr args _X_UNUSED,
                 Cardinal *num_args,
                 XrmValuePtr fromVal,
                 XrmValuePtr toVal,
                 IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToPixmap", IswCIswToolkitError,
                        "Integer to Pixmap conversion needs no extra arguments",
                        NULL, NULL);
    done(xcb_pixmap_t, *(int *) fromVal->addr);
}

#ifdef MOTIFBC
void
LowerCase(register char *source, register *dest)
{
    register char ch;
    int i;

    for (i = 0; (ch = *source) != 0 && i < 999; source++, dest++, i++) {
        if ('A' <= ch && ch <= 'Z')
            *dest = ch - 'A' + 'a';
        else
            *dest = ch;
    }
    *dest = 0;
}
#endif

static int
CompareISOLatin1(const char *first, const char *second)
{
    register const unsigned char *ap, *bp;

    for (ap = (const unsigned char *) first,
         bp = (const unsigned char *) second; *ap && *bp; ap++, bp++) {
        register unsigned char a, b;

        if ((a = *ap) != (b = *bp)) {
            /* try lowercasing and try again */

            if ((a >= XK_A) && (a <= XK_Z))
                a = (unsigned char) (a + (XK_a - XK_A));
            else if ((a >= XK_Agrave) && (a <= XK_Odiaeresis))
                a = (unsigned char) (a + (XK_agrave - XK_Agrave));
            else if ((a >= XK_Ooblique) && (a <= XK_Thorn))
                a = (unsigned char) (a + (XK_oslash - XK_Ooblique));

            if ((b >= XK_A) && (b <= XK_Z))
                b = (unsigned char) (b + (XK_a - XK_A));
            else if ((b >= XK_Agrave) && (b <= XK_Odiaeresis))
                b = (unsigned char) (b + (XK_agrave - XK_Agrave));
            else if ((b >= XK_Ooblique) && (b <= XK_Thorn))
                b = (unsigned char) (b + (XK_oslash - XK_Ooblique));

            if (a != b)
                break;
        }
    }
    return (((int) *bp) - ((int) *ap));
}

static void
CopyISOLatin1Lowered(char *dst, const char *src)
{
    unsigned char *dest = (unsigned char *) dst;
    const unsigned char *source = (const unsigned char *) src;

    for (; *source; source++, dest++) {
        if (*source >= XK_A && *source <= XK_Z)
            *dest = (unsigned char) (*source + (XK_a - XK_A));
        else if (*source >= XK_Agrave && *source <= XK_Odiaeresis)
            *dest = (unsigned char) (*source + (XK_agrave - XK_Agrave));
        else if (*source >= XK_Ooblique && *source <= XK_Thorn)
            *dest = (unsigned char) (*source + (XK_oslash - XK_Ooblique));
        else
            *dest = *source;
    }
    *dest = '\0';
}

Boolean
IswCvtStringToInitialState(IswDisplay dpy,
                          XrmValuePtr args _X_UNUSED,
                          Cardinal *num_args,
                          XrmValuePtr fromVal,
                          XrmValuePtr toVal,
                          IswPointer *closure_ret _X_UNUSED)
{
    String str = (String) fromVal->addr;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToInitialState",
                        IswCIswToolkitError,
                        "String to InitialState conversion needs no extra arguments",
                        NULL, NULL);

    if (CompareISOLatin1(str, "NormalState") == 0)
        done_string(int, XCB_ICCCM_WM_STATE_NORMAL, IswRInitialState);

    if (CompareISOLatin1(str, "IconicState") == 0)
        done_string(int, XCB_ICCCM_WM_STATE_ICONIC, IswRInitialState);

    {
        int val;

        if (IsInteger(str, &val))
            done_string(int, val, IswRInitialState);
    }
    IswDisplayStringConversionWarning(dpy, str, IswRInitialState);
    return False;
}

/* *INDENT-OFF* */
static IswConvertArgRec const visualConvertArgs[] = {
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
     sizeof(xcb_screen_t *)},
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.depth),
     sizeof(Cardinal)}
};
/* *INDENT-ON* */

Boolean
IswCvtStringToVisual(IswDisplay dpy, XrmValuePtr args,     /* Screen, depth */
                    Cardinal *num_args,        /* 2 */
                    XrmValuePtr fromVal,
                    XrmValuePtr toVal,
                    IswPointer *closure_ret _X_UNUSED)
{
    xcb_connection_t *conn = _IswXcbConn(dpy);
    String str = (String) fromVal->addr;
    int vc;
    IswVisualInfo vinfo;

    if (*num_args != 2) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToVisual",
                        IswCIswToolkitError,
                        "String to Visual conversion needs screen and depth arguments",
                        NULL, NULL);
        return False;
    }

    if (CompareISOLatin1(str, "StaticGray") == 0)
        vc = XCB_VISUAL_CLASS_STATIC_GRAY;
    else if (CompareISOLatin1(str, "StaticColor") == 0)
        vc = XCB_VISUAL_CLASS_STATIC_COLOR;
    else if (CompareISOLatin1(str, "TrueColor") == 0)
        vc = XCB_VISUAL_CLASS_TRUE_COLOR;
    else if (CompareISOLatin1(str, "GrayScale") == 0)
        vc = XCB_VISUAL_CLASS_GRAY_SCALE;
    else if (CompareISOLatin1(str, "PseudoColor") == 0)
        vc = XCB_VISUAL_CLASS_PSEUDO_COLOR;
    else if (CompareISOLatin1(str, "DirectColor") == 0)
        vc = XCB_VISUAL_CLASS_DIRECT_COLOR;
    else if (!IsInteger(str, &vc)) {
        IswDisplayStringConversionWarning(dpy, str, "Visual class name");
        return False;
    }

    {
        IswScreen screen = *(IswScreen *) args[0].addr;
        if (_IswPlatformMatchVisualInfo(
                dpy, screen, (int) *(int *) args[1].addr, vc, &vinfo)) {
            done_string(IswVisual, vinfo.visual, IswRVisual);
        }
        else {
            String params[2];
            Cardinal num_params = 2;
            const xcb_setup_t *setup = xcb_get_setup(conn);
            const char *vendor = (setup != NULL)
                ? xcb_setup_vendor(setup) : "";

            params[0] = str;
            params[1] = (String) vendor;
            IswAppWarningMsg(IswDisplayToApplicationContext(dpy), IswNconversionError,
                            "stringToVisual", IswCIswToolkitError,
                            "Cannot find Visual of class %s for display %s", params,
                            &num_params);
            return False;
        }
    }
}

Boolean
IswCvtStringToAtom(IswDisplay dpy,
                  XrmValuePtr args,
                  Cardinal *num_args,
                  XrmValuePtr fromVal,
                  XrmValuePtr toVal,
                  IswPointer *closure_ret _X_UNUSED)
{
    Atom atom;

    if (*num_args != 1) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToAtom",
                        IswCIswToolkitError,
                        "String to Atom conversion needs Display argument",
                        NULL, NULL);
        return False;
    }

    {
        IswDisplay adpy = *(IswDisplay *) args->addr;
        const char *name = (char *) fromVal->addr;
        atom = _IswPlatformInternAtomOp(adpy, name, False);
        if (atom == ISW_ATOM_NONE) {
            IswDisplayStringConversionWarning(dpy, name, IswRAtom);
            return False;
        }
    }
    done_string(Atom, atom, IswRAtom);
}


Boolean
IswCvtStringToGravity(IswDisplay dpy,
                     XrmValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     XrmValuePtr fromVal,
                     XrmValuePtr toVal,
                     IswPointer *closure_ret _X_UNUSED)
{
    /* *INDENT-OFF* */
    static struct _namepair {
        XrmQuark quark;
        const char *name;
        int gravity;
    } names[] = {
        { NULLQUARK, "forget",          XCB_GRAVITY_BIT_FORGET },
        { NULLQUARK, "northwest",       XCB_GRAVITY_NORTH_WEST },
        { NULLQUARK, "north",           XCB_GRAVITY_NORTH },
        { NULLQUARK, "northeast",       XCB_GRAVITY_NORTH_EAST },
        { NULLQUARK, "west",            XCB_GRAVITY_WEST },
        { NULLQUARK, "center",          XCB_GRAVITY_CENTER },
        { NULLQUARK, "east",            XCB_GRAVITY_EAST },
        { NULLQUARK, "southwest",       XCB_GRAVITY_SOUTH_WEST },
        { NULLQUARK, "south",           XCB_GRAVITY_SOUTH },
        { NULLQUARK, "southeast",       XCB_GRAVITY_SOUTH_EAST },
        { NULLQUARK, "static",          XCB_GRAVITY_STATIC },
        { NULLQUARK, "unmap",           XCB_GRAVITY_WIN_UNMAP },
        { NULLQUARK, "0",               XCB_GRAVITY_BIT_FORGET },
        { NULLQUARK, "1",               XCB_GRAVITY_NORTH_WEST },
        { NULLQUARK, "2",               XCB_GRAVITY_NORTH },
        { NULLQUARK, "3",               XCB_GRAVITY_NORTH_EAST },
        { NULLQUARK, "4",               XCB_GRAVITY_WEST },
        { NULLQUARK, "5",               XCB_GRAVITY_CENTER },
        { NULLQUARK, "6",               XCB_GRAVITY_EAST },
        { NULLQUARK, "7",               XCB_GRAVITY_SOUTH_WEST },
        { NULLQUARK, "8",               XCB_GRAVITY_SOUTH },
        { NULLQUARK, "9",               XCB_GRAVITY_SOUTH_EAST },
        { NULLQUARK, "10",              XCB_GRAVITY_STATIC },
        { NULLQUARK, NULL,              XCB_GRAVITY_BIT_FORGET }
    };
    /* *INDENT-ON* */
    static Boolean haveQuarks = FALSE;
    char lowerName[40];
    XrmQuark q;
    char *s;
    struct _namepair *np;

    if (*num_args != 0) {
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        "wrongParameters", "cvtStringToGravity",
                        "IswToolkitError",
                        "String to Gravity conversion needs no extra arguments",
                        NULL, NULL);
        return False;
    }
    if (!haveQuarks) {
        for (np = names; np->name; np++) {
            np->quark = XrmPermStringToQuark(np->name);
        }
        haveQuarks = TRUE;
    }
    s = (char *) fromVal->addr;
    if (strlen(s) < sizeof lowerName) {
        CopyISOLatin1Lowered(lowerName, s);
        q = XrmStringToQuark(lowerName);
        for (np = names; np->name; np++)
            if (np->quark == q)
                done_string(int, np->gravity, IswRGravity);
    }
    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRGravity);
    return False;
}

void
_IswAddDefaultConverters(ConverterTable table)
{
    
#define Add(from, to, proc, convert_args, num_args, cache) \
    _IswTableAddConverter(table, from, to, proc, \
            (IswConvertArgRec const*) convert_args, (Cardinal)num_args, \
            True, cache, (IswDestructor)NULL, True)

#define Add2(from, to, proc, convert_args, num_args, cache, destructor) \
    _IswTableAddConverter(table, from, to, proc, \
            (IswConvertArgRec const *) convert_args, (Cardinal)num_args, \
            True, cache, destructor, True)

    Add(IswQColor, IswQPixel, IswCvtColorToPixel, NULL, 0, IswCacheNone);

    Add(IswQInt, IswQBool, IswCvtIntToBool, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQBoolean, IswCvtIntToBoolean, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQColor, IswCvtIntToColor,
        colorConvertArgs, IswNumber(colorConvertArgs), IswCacheByDisplay);
    Add(IswQInt, IswQDimension, IswCvtIntToShort, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQFloat, IswCvtIntToFloat, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQFont, IswCvtIntToFont, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQPixel, IswCvtIntToPixel, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQPixmap, IswCvtIntToPixmap, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQPosition, IswCvtIntToShort, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQShort, IswCvtIntToShort, NULL, 0, IswCacheNone);
    Add(IswQInt, IswQUnsignedChar, IswCvtIntToUnsignedChar, NULL, 0, IswCacheNone);

    Add(IswQPixel, IswQColor, IswCvtIntToColor,
        colorConvertArgs, IswNumber(colorConvertArgs), IswCacheByDisplay);

    Add(_IswQString, IswQAtom, IswCvtStringToAtom,
        displayConvertArg, IswNumber(displayConvertArg), IswCacheNone);
    Add(_IswQString, IswQBool, IswCvtStringToBool, NULL, 0, IswCacheNone);
    Add(_IswQString, IswQBoolean, IswCvtStringToBoolean, NULL, 0, IswCacheNone);
    Add2(_IswQString, IswQCursor, IswCvtStringToCursor,
         cursorConvertArgs, IswNumber(cursorConvertArgs),
         IswCacheByDisplay, FreeCursor);
    Add(_IswQString, IswQDimension, IswCvtStringToDimension, NULL, 0, IswCacheNone);
    Add(_IswQString, IswQDisplay, IswCvtStringToDisplay, NULL, 0, IswCacheAll);
    Add2(_IswQString, IswQFile, IswCvtStringToFile, NULL, 0,
         IswCacheAll | IswCacheRefCount, FreeFile);
    Add(_IswQString, IswQFloat, IswCvtStringToFloat, NULL, 0, IswCacheNone);

    Add2(_IswQString, IswQFont, IswCvtStringToFont,
         displayConvertArg, IswNumber(displayConvertArg),
         IswCacheByDisplay, FreeFont);

    Add2(_IswQString, IswQFontStruct, IswCvtStringToFontStruct,
         displayConvertArg, IswNumber(displayConvertArg),
         IswCacheByDisplay, FreeFontStruct);

    Add(_IswQString, IswQGravity, IswCvtStringToGravity, NULL, 0, IswCacheNone);
    Add(_IswQString, IswQInitialState, IswCvtStringToInitialState, NULL, 0,
        IswCacheNone);
    Add(_IswQString, IswQInt, IswCvtStringToInt, NULL, 0, IswCacheAll);
    Add2(_IswQString, IswQPixel, IswCvtStringToPixel,
         colorConvertArgs, IswNumber(colorConvertArgs),
         IswCacheByDisplay, FreePixel);
    Add(_IswQString, IswQPosition, IswCvtStringToShort, NULL, 0, IswCacheAll);
    Add(_IswQString, IswQShort, IswCvtStringToShort, NULL, 0, IswCacheAll);
    Add(_IswQString, IswQUnsignedChar, IswCvtStringToUnsignedChar,
        NULL, 0, IswCacheAll);
    Add2(_IswQString, IswQVisual, IswCvtStringToVisual,
         visualConvertArgs, IswNumber(visualConvertArgs),
         IswCacheByDisplay, NULL);

    _IswAddTMConverters(table);
}
