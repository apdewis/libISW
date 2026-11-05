/*
 * ISWFontSettingsX11.h - X11 font rendering settings (backend-private)
 *
 * Copyright (c) 2026 ISW Project
 *
 * The system font rendering settings — antialias, hinting, hint style,
 * subpixel order, LCD filter — resolved per font in the same precedence as
 * libXft / cairo-xlib, with resources authoritative:
 *
 *   1. Xft.* resources from the display's resource database (RESOURCE_MANAGER,
 *      read through IswDatabase) win;
 *   2. fontconfig's per-font result (fonts.conf) fills in anything not set
 *      there.
 *
 * Shared by the Cairo-XCB and EGL render backends so both resolve a font to
 * the same file and the same rendering parameters.
 */

#ifndef _ISWFontSettingsX11_h
#define _ISWFontSettingsX11_h

#include <ISW/Intrinsic.h>
#include <fontconfig/fontconfig.h>

/* Rendering parameters of one resolved font. */
typedef struct {
    Boolean antialias;
    Boolean hinting;
    Boolean autohint;    /* fonts.conf FC_AUTOHINT: force FreeType's autohinter */
    int     hintstyle;   /* FC_HINT_NONE .. FC_HINT_FULL */
    int     rgba;        /* FC_RGBA_*; subpixel when RGB/BGR/VRGB/VBGR */
    int     lcdfilter;   /* FC_LCD_NONE .. FC_LCD_LEGACY */
} IswFontRenderParams;

/* True when the parameters select subpixel (LCD) antialiasing. */
#define _IswFontParamsSubpixel(p) \
    ((p)->antialias && \
     ((p)->rgba == FC_RGBA_RGB || (p)->rgba == FC_RGBA_BGR || \
      (p)->rgba == FC_RGBA_VRGB || (p)->rgba == FC_RGBA_VBGR))

/* Load the Xft.* settings from dpy's resource database on first use; no-op
   afterwards.  The settings are process-wide, like the backends' font caches. */
extern void _IswFontSettingsEnsure(IswDisplay dpy);

/* Re-read the Xft.* settings from dpy's resource database.  Returns True and
   bumps the settings serial if anything changed. */
extern Boolean _IswFontSettingsReload(IswDisplay dpy);

/* Serial of the current settings; changes whenever _IswFontSettingsReload
   finds a difference.  Font state resolved under an older serial is stale. */
extern unsigned int _IswFontSettingsSerial(void);

/* fontconfig match for a font description (scalable, after FcMatchPattern
   substitution and defaults), with the fonts.conf rendering properties but no
   Xft overrides.  Returns NULL if nothing matched or the match has no file.
   Caller destroys. */
extern FcPattern *_IswFontMatch(const char *family, int weight, int slant);

/* Final pattern for a match: a duplicate of `match` with the current Xft.*
   overrides applied (FcPatternDel + FcPatternAdd*, so resources beat fonts.conf
   for every match target).  Fills *params from the final pattern, mirroring how
   cairo-ft reads it.  Caller destroys. */
extern FcPattern *_IswFontFinalPattern(FcPattern *match,
                                       IswFontRenderParams *params);

#endif /* _ISWFontSettingsX11_h */
