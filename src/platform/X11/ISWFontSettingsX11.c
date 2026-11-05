/*
 * ISWFontSettingsX11.c - X11 font rendering settings (backend-private)
 *
 * Copyright (c) 2026 ISW Project
 *
 * Reads the Xft.* rendering resources and resolves fonts through fontconfig
 * with those resources overriding the fonts.conf result.  See
 * ISWFontSettingsX11.h.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "IntrinsicI.h"
#include "ISWPlatformPrivate.h"
#include "ISWFontSettingsX11.h"

#define XFT_UNSET (-1)

/* Xft.* resource values; XFT_UNSET where the resource is absent. */
typedef struct {
    int antialias;   /* FcBool */
    int hinting;     /* FcBool */
    int hintstyle;   /* FC_HINT_* */
    int rgba;        /* FC_RGBA_* */
    int lcdfilter;   /* FC_LCD_* */
} XftSettings;

static XftSettings _settings = {
    XFT_UNSET, XFT_UNSET, XFT_UNSET, XFT_UNSET, XFT_UNSET
};
static Boolean _settings_loaded = False;
static unsigned int _settings_serial = 1;

/* Boolean resource value, parsed as libXft's XftDefaultParseBool does:
   t/y/1 and "on" are true, f/n/0 and "off" are false. */
static int
XftParseBool(const char *v)
{
    char c0 = v[0], c1;

    if (c0 >= 'A' && c0 <= 'Z')
        c0 = (char) (c0 - 'A' + 'a');
    if (c0 == 't' || c0 == 'y' || c0 == '1')
        return FcTrue;
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
        return FcFalse;
    if (c0 == 'o') {
        c1 = v[1];
        if (c1 >= 'A' && c1 <= 'Z')
            c1 = (char) (c1 - 'A' + 'a');
        if (c1 == 'n')
            return FcTrue;
        if (c1 == 'f')
            return FcFalse;
    }
    return XFT_UNSET;
}

static int
XftGetBool(IswDatabaseHandle db, const char *name, const char *class)
{
    char *value = NULL;
    int result = XFT_UNSET;

    if (_IswPlatformResourceGetString(db, name, class, &value) >= 0 && value) {
        result = XftParseBool(value);
        free(value);
    }
    return result;
}

/* A fontconfig constant name (hintslight, rgb, lcddefault, ...) or a plain
   integer, as libXft accepts.  Values outside [lo, hi] are ignored. */
static int
XftGetInteger(IswDatabaseHandle db, const char *name, const char *class,
              int lo, int hi)
{
    char *value = NULL;
    int i, result = XFT_UNSET;

    if (_IswPlatformResourceGetString(db, name, class, &value) >= 0 && value) {
        if (FcNameConstant((const FcChar8 *) value, &i) ||
            sscanf(value, "%d", &i) == 1) {
            if (i >= lo && i <= hi)
                result = i;
        }
        free(value);
    }
    return result;
}

static void
XftRead(IswDisplay dpy, XftSettings *s)
{
    IswDatabaseHandle db = dpy ? IswDatabase(dpy) : NULL;

    s->antialias = s->hinting = s->hintstyle = s->rgba = s->lcdfilter =
        XFT_UNSET;
    if (!db)
        return;

    s->antialias = XftGetBool(db, "Xft.antialias", "Xft.Antialias");
    s->hinting   = XftGetBool(db, "Xft.hinting", "Xft.Hinting");
    s->hintstyle = XftGetInteger(db, "Xft.hintstyle", "Xft.HintStyle",
                                 FC_HINT_NONE, FC_HINT_FULL);
    s->rgba      = XftGetInteger(db, "Xft.rgba", "Xft.Rgba",
                                 FC_RGBA_UNKNOWN, FC_RGBA_NONE);
    s->lcdfilter = XftGetInteger(db, "Xft.lcdfilter", "Xft.LcdFilter",
                                 FC_LCD_NONE, FC_LCD_LEGACY);
}

void
_IswFontSettingsEnsure(IswDisplay dpy)
{
    if (_settings_loaded || !dpy)
        return;
    XftRead(dpy, &_settings);
    _settings_loaded = True;
}

Boolean
_IswFontSettingsReload(IswDisplay dpy)
{
    XftSettings s;

    if (!dpy)
        return False;
    XftRead(dpy, &s);
    _settings_loaded = True;
    if (memcmp(&s, &_settings, sizeof(s)) == 0)
        return False;
    _settings = s;
    _settings_serial++;
    return True;
}

unsigned int
_IswFontSettingsSerial(void)
{
    return _settings_serial;
}

FcPattern *
_IswFontMatch(const char *family, int weight, int slant)
{
    FcPattern *pattern, *match;
    FcResult result;
    FcChar8 *file = NULL;

    pattern = FcPatternCreate();
    if (!pattern)
        return NULL;
    FcPatternAddString(pattern, FC_FAMILY,
                       (const FcChar8 *) (family ? family : "Sans"));
    FcPatternAddInteger(pattern, FC_WEIGHT, weight);
    FcPatternAddInteger(pattern, FC_SLANT, slant);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    match = FcFontMatch(NULL, pattern, &result);
    FcPatternDestroy(pattern);
    if (match &&
        FcPatternGetString(match, FC_FILE, 0, &file) != FcResultMatch) {
        FcPatternDestroy(match);
        match = NULL;
    }
    return match;
}

FcPattern *
_IswFontFinalPattern(FcPattern *match, IswFontRenderParams *params)
{
    const XftSettings *s = &_settings;
    FcPattern *pat = FcPatternDuplicate(match);
    FcBool antialias = FcTrue, hinting = FcTrue, autohint = FcFalse;
    int hintstyle = FC_HINT_FULL, rgba = FC_RGBA_UNKNOWN,
        lcdfilter = FC_LCD_DEFAULT;

    if (!pat)
        return NULL;

    if (s->antialias != XFT_UNSET) {
        FcPatternDel(pat, FC_ANTIALIAS);
        FcPatternAddBool(pat, FC_ANTIALIAS, (FcBool) s->antialias);
    }
    if (s->hinting != XFT_UNSET) {
        FcPatternDel(pat, FC_HINTING);
        FcPatternAddBool(pat, FC_HINTING, (FcBool) s->hinting);
    }
    if (s->hintstyle != XFT_UNSET) {
        FcPatternDel(pat, FC_HINT_STYLE);
        FcPatternAddInteger(pat, FC_HINT_STYLE, s->hintstyle);
    }
    if (s->rgba != XFT_UNSET) {
        FcPatternDel(pat, FC_RGBA);
        FcPatternAddInteger(pat, FC_RGBA, s->rgba);
    }
    if (s->lcdfilter != XFT_UNSET) {
        FcPatternDel(pat, FC_LCD_FILTER);
        FcPatternAddInteger(pat, FC_LCD_FILTER, s->lcdfilter);
    }

    /* Defaults for absent properties are cairo-ft's. */
    FcPatternGetBool(pat, FC_ANTIALIAS, 0, &antialias);
    FcPatternGetBool(pat, FC_HINTING, 0, &hinting);
    FcPatternGetBool(pat, FC_AUTOHINT, 0, &autohint);
    FcPatternGetInteger(pat, FC_HINT_STYLE, 0, &hintstyle);
    FcPatternGetInteger(pat, FC_RGBA, 0, &rgba);
    FcPatternGetInteger(pat, FC_LCD_FILTER, 0, &lcdfilter);

    if (params) {
        params->antialias = antialias ? True : False;
        params->hinting   = hinting ? True : False;
        params->autohint  = autohint ? True : False;
        params->hintstyle = hintstyle;
        params->rgba      = rgba;
        params->lcdfilter = lcdfilter;
    }
    return pat;
}
