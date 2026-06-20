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
#include        <ISW/ISWP.h>          /* IswOrientation / IswJustify */
#include        <ISW/ISWPlatform.h>
#include        <stdio.h>
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

#define IswQCursor               IswPermStringToQuark(IswRCursor)
#define IswQDisplay              IswPermStringToQuark(IswRDisplay)
#define IswQFile                 IswPermStringToQuark(IswRFile)
#define IswQFloat                IswPermStringToQuark(IswRFloat)
#define IswQInitialState         IswPermStringToQuark(IswRInitialState)
#define IswQWindowType           IswPermStringToQuark(IswRWindowType)
#define IswQPixmap               IswPermStringToQuark(IswRPixmap)
#define IswQShort                IswPermStringToQuark(IswRShort)
#define IswQUnsignedChar         IswPermStringToQuark(IswRUnsignedChar)
#define IswQVisual               IswPermStringToQuark(IswRVisual)

static IswQuark IswQBool;
static IswQuark IswQBoolean;
static IswQuark IswQColor;
static IswQuark IswQDimension;
static IswQuark IswQFont;
static IswQuark IswQFontStruct;
static IswQuark IswQGravity;
static IswQuark IswQInt;
static IswQuark IswQPixel;
static IswQuark IswQPosition;
IswQuark _IswQString;

void
_IswConvertInitialize(void)
{
    IswQBool = IswPermStringToQuark(IswRBool);
    IswQBoolean = IswPermStringToQuark(IswRBoolean);
    IswQColor = IswPermStringToQuark(IswRColor);
    IswQDimension = IswPermStringToQuark(IswRDimension);
    IswQFont = IswPermStringToQuark(IswRFont);
    IswQFontStruct = IswPermStringToQuark(IswRFontStruct);
    IswQGravity = IswPermStringToQuark(IswRGravity);
    IswQInt = IswPermStringToQuark(IswRInt);
    IswQPixel = IswPermStringToQuark(IswRPixel);
    IswQPosition = IswPermStringToQuark(IswRPosition);
    _IswQString = IswPermStringToQuark(IswRString);
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
        IswDatabaseHandle rdb = IswDatabase(dpy);
        IswQuarkName xrm_name[2];
        IswQuarkClass xrm_class[2];
        IswRepresentation rep_type;
        IswValueRec value;

        xrm_name[0] = IswPermStringToQuark("stringConversionWarnings");
        xrm_name[1] = 0;
        xrm_class[0] = IswPermStringToQuark("StringConversionWarnings");
        xrm_class[1] = 0;
        if (IswQGetResource(rdb, xrm_name, xrm_class, &rep_type, &value)) {
            if (rep_type == IswQBoolean)
                report_it = *(Boolean *) value.addr ? Report : Ignore;
            else if (rep_type == _IswQString) {
                IswValueRec toVal;
                Boolean report = False;

                toVal.addr = (IswPointer) &report;
                toVal.size = sizeof(Boolean);
                if (IswCallConverter
                    (dpy, IswCvtStringToBoolean, (IswValuePtr) NULL,
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
                  IswValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  IswValuePtr fromVal,
                  IswValuePtr toVal,
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
                IswValuePtr args _X_UNUSED,
                Cardinal *num_args,
                IswValuePtr fromVal,
                IswValuePtr toVal,
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
                     IswValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     IswValuePtr fromVal,
                     IswValuePtr toVal,
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
               IswValuePtr args _X_UNUSED,
               Cardinal *num_args,
               IswValuePtr fromVal,
               IswValuePtr toVal,
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
                  IswValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  IswValuePtr fromVal,
                  IswValuePtr toVal,
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
     sizeof(IswScreen)},
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.colormap),
     sizeof(IswColormap)}
};
/* *INDENT-ON* */

Boolean
IswCvtIntToColor(IswDisplay dpy,
                IswValuePtr args,
                Cardinal *num_args,
                IswValuePtr fromVal,
                IswValuePtr toVal,
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
                   IswValuePtr args,
                   Cardinal *num_args,
                   IswValuePtr fromVal,
                   IswValuePtr toVal,
                   IswPointer *closure_ret)
{
    String str = (String) fromVal->addr;
    IswScreen screen;
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

    screen = *((IswScreen *) args[0].addr);
    colormap = *((IswColormap *) args[1].addr);

    if (CompareISOLatin1(str, IswDefaultBackground) == 0) {
        *closure_ret = NULL;
        if (pd->rv) {
            done_string(Pixel, _IswPlatformScreenBlackPixel(dpy, screen), IswRPixel);
        }
        else {
            done_string(Pixel, _IswPlatformScreenWhitePixel(dpy, screen), IswRPixel);
        }
    }
    if (CompareISOLatin1(str, IswDefaultForeground) == 0) {
        *closure_ret = NULL;
        if (pd->rv) {
            done_string(Pixel, _IswPlatformScreenWhitePixel(dpy, screen), IswRPixel);
        }
        else {
            done_string(Pixel, _IswPlatformScreenBlackPixel(dpy, screen), IswRPixel);
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
          IswValuePtr toVal,
          IswPointer closure,
          IswValuePtr args,
          Cardinal *num_args)
{
    IswScreen screen;
    IswColormap colormap;

    if (*num_args != 2) {
        IswAppWarningMsg(app, IswNwrongParameters, "freePixel", IswCIswToolkitError,
                        "Freeing a pixel requires screen and colormap arguments",
                        NULL, NULL);
        return;
    }

    screen = *((IswScreen *) args[0].addr);
    colormap = *((IswColormap *) args[1].addr);

    if (closure) {
        IswDisplay dpy = _IswConnectionOfScreen(screen);
        unsigned long pixel = *(uint32_t *) toVal->addr;
        _IswPlatformFreeColors(dpy, colormap, pixel);
    }
}

/* no longer used by Xt, but it's in the spec */
IswConvertArgRec const screenConvertArg[] = {
    {IswWidgetBaseOffset, (IswPointer) IswOffsetOf(WidgetRec, core.screen),
     sizeof(IswScreen)}
};

static void
FetchDisplayArg(Widget widget, Cardinal *size _X_UNUSED, IswValueRec *value)
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
     sizeof(IswScreen)},
};
/* *INDENT-ON* */

/* -----------------------------------------------------------------------
 * Load a theme-aware named cursor (glyph fallback).  Thin wrapper over the
 * platform cursor op; the glyph/themed creation now lives in the backend
 * (ISWPlatformGrabCursorXCB.c).  The raw conn/screen params are seam-internal
 * (this is an internal header, not a public ISW/ widget header).
 * ----------------------------------------------------------------------- */
IswCursor
_IswLoadThemedCursor(IswDisplay dpy, IswScreen screen, const char *name)
{
    return _IswPlatformLoadNamedCursor(dpy, screen, name);
}

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

Boolean
IswCvtStringToCursor(IswDisplay dpy,
                    IswValuePtr args,
                    Cardinal *num_args,
                    IswValuePtr fromVal,
                    IswValuePtr toVal,
                    IswPointer *closure_ret _X_UNUSED)
{
    /* *INDENT-OFF* */
    static const char *cursor_names[] = {
        "X_cursor",            "arrow",
        "based_arrow_down",    "based_arrow_up",
        "boat",                "bogosity",
        "bottom_left_corner",  "bottom_right_corner",
        "bottom_side",         "bottom_tee",
        "box_spiral",          "center_ptr",
        "circle",              "clock",
        "coffee_mug",          "cross",
        "cross_reverse",       "crosshair",
        "diamond_cross",       "dot",
        "dotbox",              "double_arrow",
        "draft_large",         "draft_small",
        "draped_box",          "exchange",
        "fleur",               "gobbler",
        "gumby",               "hand1",
        "hand2",               "heart",
        "icon",                "iron_cross",
        "left_ptr",            "left_side",
        "left_tee",            "leftbutton",
        "ll_angle",            "lr_angle",
        "man",                 "middlebutton",
        "mouse",               "pencil",
        "pirate",              "plus",
        "question_arrow",      "right_ptr",
        "right_side",          "right_tee",
        "rightbutton",         "rtl_logo",
        "sailboat",            "sb_down_arrow",
        "sb_h_double_arrow",   "sb_left_arrow",
        "sb_right_arrow",      "sb_up_arrow",
        "sb_v_double_arrow",   "shuttle",
        "sizing",              "spider",
        "spraycan",            "star",
        "target",              "tcross",
        "top_left_arrow",      "top_left_corner",
        "top_right_corner",    "top_side",
        "top_tee",             "trek",
        "ul_angle",            "umbrella",
        "ur_angle",            "watch",
        "xterm",
    };
    /* *INDENT-ON* */
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

    for (i = 0; i < IswNumber(cursor_names); i++) {
        if (strcmp(name, cursor_names[i]) == 0) {
            IswScreen screen = *(IswScreen *) args[1].addr;
            IswCursor cursor = _IswLoadThemedCursor(dpy, screen,
                                                    cursor_names[i]);

            done_string(IswCursor, cursor, IswRCursor);
        }
    }
    IswDisplayStringConversionWarning(dpy, name, IswRCursor);
    return False;
}

static void
FreeCursor(IswAppContext app,
           IswValuePtr toVal,
           IswPointer closure _X_UNUSED,
           IswValuePtr args,
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
                     IswValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     IswValuePtr fromVal,
                     IswValuePtr toVal,
                     IswPointer *closure_ret _X_UNUSED)
{
    IswDisplay d;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToDisplay",
                        IswCIswToolkitError,
                        "String to Display conversion needs no extra arguments",
                        NULL, NULL);

    {
        int screen_num = 0;
        d = _IswPlatformOpenDisplay((char *) fromVal->addr, &screen_num);
        if (d != NULL && !_IswPlatformDisplayHasError(d))
            done_string(IswDisplay, d, IswRDisplay);
        if (d != NULL)
            _IswPlatformCloseDisplay(d);
    }

    IswDisplayStringConversionWarning(dpy, (char *) fromVal->addr, IswRDisplay);
    return False;
}

Boolean
IswCvtStringToFile(IswDisplay dpy,
                  IswValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  IswValuePtr fromVal,
                  IswValuePtr toVal,
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
         IswValuePtr toVal,
         IswPointer closure _X_UNUSED,
         IswValuePtr args _X_UNUSED,
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
                IswValuePtr args _X_UNUSED,
                Cardinal *num_args,
                IswValuePtr fromVal,
                IswValuePtr toVal,
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
                   IswValuePtr args _X_UNUSED,
                   Cardinal *num_args,
                   IswValuePtr fromVal,
                   IswValuePtr toVal,
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
                  IswValuePtr args,
                  Cardinal *num_args,
                  IswValuePtr fromVal,
                  IswValuePtr toVal,
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
        IswQuarkName xrm_name[2];
        IswQuarkClass xrm_class[2];
        IswRepresentation rep_type;
        IswValueRec value;

        xrm_name[0] = IswPermStringToQuark("xtDefaultFont");
        xrm_name[1] = 0;
        xrm_class[0] = IswPermStringToQuark("IswDefaultFont");
        xrm_class[1] = 0;
        if (IswQGetResource(IswDatabase((IswDisplay) display), xrm_name, xrm_class,
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
         IswValuePtr toVal,
         IswPointer closure _X_UNUSED,
         IswValuePtr args,
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
               IswValuePtr args _X_UNUSED,
               Cardinal *num_args,
               IswValuePtr fromVal,
               IswValuePtr toVal,
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
                        IswValuePtr args,
                        Cardinal *num_args,
                        IswValuePtr fromVal,
                        IswValuePtr toVal,
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
        IswQuarkName xrm_name[2];
        IswQuarkClass xrm_class[2];
        IswRepresentation rep_type;
        IswValueRec value;

        xrm_name[0] = IswPermStringToQuark("xtDefaultFont");
        xrm_name[1] = 0;
        xrm_class[0] = IswPermStringToQuark("IswDefaultFont");
        xrm_class[1] = 0;
        if (IswQGetResource(IswDatabase((IswDisplay) display), xrm_name, xrm_class,
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
               IswValuePtr toVal,
               IswPointer closure _X_UNUSED,
               IswValuePtr args,
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
                 IswValuePtr args _X_UNUSED,
                 Cardinal *num_args,
                 IswValuePtr fromVal,
                 IswValuePtr toVal,
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
                   IswValuePtr args _X_UNUSED,
                   Cardinal *num_args,
                   IswValuePtr fromVal,
                   IswValuePtr toVal,
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
                       IswValuePtr args _X_UNUSED,
                       Cardinal *num_args,
                       IswValuePtr fromVal,
                       IswValuePtr toVal,
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
                       IswValuePtr args _X_UNUSED,
                       Cardinal *num_args,
                       IswValuePtr fromVal,
                       IswValuePtr toVal,
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
                          IswValuePtr args _X_UNUSED,
                          Cardinal *num_args,
                          IswValuePtr fromVal,
                          IswValuePtr toVal,
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
                  IswValuePtr args _X_UNUSED,
                  Cardinal *num_args,
                  IswValuePtr fromVal,
                  IswValuePtr toVal,
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
                IswValuePtr args _X_UNUSED,
                Cardinal *num_args,
                IswValuePtr fromVal,
                IswValuePtr toVal,
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
                 IswValuePtr args _X_UNUSED,
                 Cardinal *num_args,
                 IswValuePtr fromVal,
                 IswValuePtr toVal,
                 IswPointer *closure_ret _X_UNUSED)
{
    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtIntToPixmap", IswCIswToolkitError,
                        "Integer to Pixmap conversion needs no extra arguments",
                        NULL, NULL);
    done(IswPixmap, *(int *) fromVal->addr);
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
                          IswValuePtr args _X_UNUSED,
                          Cardinal *num_args,
                          IswValuePtr fromVal,
                          IswValuePtr toVal,
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
        done_string(int, IswWmStateNormal, IswRInitialState);

    if (CompareISOLatin1(str, "IconicState") == 0)
        done_string(int, IswWmStateIconic, IswRInitialState);

    {
        int val;

        if (IsInteger(str, &val))
            done_string(int, val, IswRInitialState);
    }
    IswDisplayStringConversionWarning(dpy, str, IswRInitialState);
    return False;
}

static Boolean
IswCvtStringToWindowType(IswDisplay dpy,
                         IswValuePtr args _X_UNUSED,
                         Cardinal *num_args,
                         IswValuePtr fromVal,
                         IswValuePtr toVal,
                         IswPointer *closure_ret _X_UNUSED)
{
    String str = (String) fromVal->addr;

    if (*num_args != 0)
        IswAppWarningMsg(IswDisplayToApplicationContext(dpy),
                        IswNwrongParameters, "cvtStringToWindowType",
                        IswCIswToolkitError,
                        "String to WindowType conversion needs no extra arguments",
                        NULL, NULL);

    if (CompareISOLatin1(str, "Normal") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_NORMAL, IswRWindowType);
    if (CompareISOLatin1(str, "Dialog") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_DIALOG, IswRWindowType);
    if (CompareISOLatin1(str, "Tooltip") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_TOOLTIP, IswRWindowType);
    if (CompareISOLatin1(str, "Menu") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_MENU, IswRWindowType);
    if (CompareISOLatin1(str, "PopupMenu") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_POPUP_MENU, IswRWindowType);
    if (CompareISOLatin1(str, "Utility") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_UTILITY, IswRWindowType);
    if (CompareISOLatin1(str, "Dock") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_DOCK, IswRWindowType);
    if (CompareISOLatin1(str, "Desktop") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_DESKTOP, IswRWindowType);
    if (CompareISOLatin1(str, "Toolbar") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_TOOLBAR, IswRWindowType);
    if (CompareISOLatin1(str, "Splash") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_SPLASH, IswRWindowType);
    if (CompareISOLatin1(str, "Notification") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_NOTIFICATION, IswRWindowType);
    if (CompareISOLatin1(str, "DropdownMenu") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_DROPDOWN_MENU, IswRWindowType);
    if (CompareISOLatin1(str, "Combo") == 0)
        done_string(IswWindowType, ISW_WINDOW_TYPE_COMBO, IswRWindowType);

    IswDisplayStringConversionWarning(dpy, str, IswRWindowType);
    return False;
}

/* *INDENT-OFF* */
static IswConvertArgRec const visualConvertArgs[] = {
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.screen),
     sizeof(IswScreen)},
    {IswWidgetBaseOffset, (IswPointer)IswOffsetOf(WidgetRec, core.depth),
     sizeof(Cardinal)}
};
/* *INDENT-ON* */

Boolean
IswCvtStringToVisual(IswDisplay dpy, IswValuePtr args,     /* Screen, depth */
                    Cardinal *num_args,        /* 2 */
                    IswValuePtr fromVal,
                    IswValuePtr toVal,
                    IswPointer *closure_ret _X_UNUSED)
{
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
        vc = IswVisualStaticGray;
    else if (CompareISOLatin1(str, "StaticColor") == 0)
        vc = IswVisualStaticColor;
    else if (CompareISOLatin1(str, "TrueColor") == 0)
        vc = IswVisualTrueColor;
    else if (CompareISOLatin1(str, "GrayScale") == 0)
        vc = IswVisualGrayScale;
    else if (CompareISOLatin1(str, "PseudoColor") == 0)
        vc = IswVisualPseudoColor;
    else if (CompareISOLatin1(str, "DirectColor") == 0)
        vc = IswVisualDirectColor;
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
            const char *vendor = _IswPlatformDisplayVendor(dpy);

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
IswCvtStringToGravity(IswDisplay dpy,
                     IswValuePtr args _X_UNUSED,
                     Cardinal *num_args,
                     IswValuePtr fromVal,
                     IswValuePtr toVal,
                     IswPointer *closure_ret _X_UNUSED)
{
    /* *INDENT-OFF* */
    static struct _namepair {
        IswQuark quark;
        const char *name;
        int gravity;
    } names[] = {
        { ISW_NULLQUARK, "forget",          IswGravityForget },
        { ISW_NULLQUARK, "northwest",       IswGravityNorthWest },
        { ISW_NULLQUARK, "north",           IswGravityNorth },
        { ISW_NULLQUARK, "northeast",       IswGravityNorthEast },
        { ISW_NULLQUARK, "west",            IswGravityWest },
        { ISW_NULLQUARK, "center",          IswGravityCenter },
        { ISW_NULLQUARK, "east",            IswGravityEast },
        { ISW_NULLQUARK, "southwest",       IswGravitySouthWest },
        { ISW_NULLQUARK, "south",           IswGravitySouth },
        { ISW_NULLQUARK, "southeast",       IswGravitySouthEast },
        { ISW_NULLQUARK, "static",          IswGravityStatic },
        { ISW_NULLQUARK, "unmap",           IswGravityUnmap },
        { ISW_NULLQUARK, "0",               IswGravityForget },
        { ISW_NULLQUARK, "1",               IswGravityNorthWest },
        { ISW_NULLQUARK, "2",               IswGravityNorth },
        { ISW_NULLQUARK, "3",               IswGravityNorthEast },
        { ISW_NULLQUARK, "4",               IswGravityWest },
        { ISW_NULLQUARK, "5",               IswGravityCenter },
        { ISW_NULLQUARK, "6",               IswGravityEast },
        { ISW_NULLQUARK, "7",               IswGravitySouthWest },
        { ISW_NULLQUARK, "8",               IswGravitySouth },
        { ISW_NULLQUARK, "9",               IswGravitySouthEast },
        { ISW_NULLQUARK, "10",              IswGravityStatic },
        { ISW_NULLQUARK, NULL,              IswGravityForget }
    };
    /* *INDENT-ON* */
    static Boolean haveQuarks = FALSE;
    char lowerName[40];
    IswQuark q;
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
            np->quark = IswPermStringToQuark(np->name);
        }
        haveQuarks = TRUE;
    }
    s = (char *) fromVal->addr;
    if (strlen(s) < sizeof lowerName) {
        CopyISOLatin1Lowered(lowerName, s);
        q = IswStringToQuark(lowerName);
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
    Add(_IswQString, IswQWindowType, IswCvtStringToWindowType, NULL, 0,
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

/*
 * Widget-set value converters (libXmu replacements).  Pure neutral resource
 * conversions; no windowing-system dependency.
 */

/* String -> IswOrientation ("horizontal" / "vertical", case-insensitive). */
Boolean
ISWCvtStringToOrientation(
    IswDisplay display,
    IswValuePtr args,
    Cardinal *num_args,
    IswValuePtr from,
    IswValuePtr to,
    IswPointer *converter_data)
{
    static IswOrientation orientation;
    char lowerName[64];
    const char *str = (const char *)from->addr;

    (void)display; (void)args; (void)num_args; (void)converter_data;

    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;

    ISWCopyISOLatin1Lowered(lowerName, str);

    if (strcmp(lowerName, "horizontal") == 0) {
        orientation = IswOrientHorizontal;
    } else if (strcmp(lowerName, "vertical") == 0) {
        orientation = IswOrientVertical;
    } else {
        return False;
    }

    if (to->addr == NULL) {
        to->addr = (IswPointer)&orientation;
    } else if (to->size < sizeof(IswOrientation)) {
        to->size = sizeof(IswOrientation);
        return False;
    } else {
        *(IswOrientation *)to->addr = orientation;
    }
    to->size = sizeof(IswOrientation);

    return True;
}

/* String -> IswJustify ("left" / "center" / "right", case-insensitive). */
Boolean
ISWCvtStringToJustify(
    IswDisplay display,
    IswValuePtr args,
    Cardinal *num_args,
    IswValuePtr from,
    IswValuePtr to,
    IswPointer *converter_data)
{
    static IswJustify justify;
    char lowerName[64];
    const char *str = (const char *)from->addr;

    (void)display; (void)args; (void)num_args; (void)converter_data;

    if (str == NULL || strlen(str) >= sizeof(lowerName))
        return False;

    ISWCopyISOLatin1Lowered(lowerName, str);

    if (strcmp(lowerName, "left") == 0) {
        justify = IswJustifyLeft;
    } else if (strcmp(lowerName, "center") == 0) {
        justify = IswJustifyCenter;
    } else if (strcmp(lowerName, "right") == 0) {
        justify = IswJustifyRight;
    } else {
        return False;
    }

    if (to->addr == NULL) {
        to->addr = (IswPointer)&justify;
    } else if (to->size < sizeof(IswJustify)) {
        to->size = sizeof(IswJustify);
        return False;
    } else {
        *(IswJustify *)to->addr = justify;
    }
    to->size = sizeof(IswJustify);

    return True;
}

/* String -> Widget by name, searching from the parent widget passed in args. */
Boolean
ISWCvtStringToWidget(
    IswDisplay display,
    IswValuePtr args,
    Cardinal *num_args,
    IswValuePtr from,
    IswValuePtr to,
    IswPointer *converter_data)
{
    static Widget widget;
    Widget parent;
    const char *name;

    (void)display; (void)converter_data;

    /* Need exactly one argument: the parent widget */
    if (*num_args != 1) {
        IswAppWarningMsg(
            IswWidgetToApplicationContext(*((Widget *)args[0].addr)),
            "wrongParameters", "cvtStringToWidget", "IswToolkitError",
            "String to Widget conversion requires parent argument",
            (String *)NULL, (Cardinal *)NULL);
        return False;
    }

    parent = *((Widget *)args[0].addr);
    name = (const char *)from->addr;

    if (name == NULL || *name == '\0') {
        return False;
    }

    widget = IswNameToWidget(parent, (String)name);

    if (widget == (Widget)NULL) {
        /* Widget not found - not an error, may be created later */
        return False;
    }

    if (to->addr == NULL) {
        to->addr = (IswPointer)&widget;
    } else if (to->size < sizeof(Widget)) {
        to->size = sizeof(Widget);
        return False;
    } else {
        *(Widget *)to->addr = widget;
    }
    to->size = sizeof(Widget);

    return True;
}
