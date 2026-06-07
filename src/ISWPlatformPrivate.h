/*
 * ISWPlatformPrivate.h - Internal declarations for ISWPlatform backends
 *
 * Copyright (c) 2026 ISW Project
 *
 * Internal counterpart to ISW/ISWPlatform.h, mirroring the public/private
 * split used by ISWRender.h / ISWRenderPrivate.h.  Concrete backends and the
 * platform dispatcher include this; widget code includes only the public
 * header.  This is where each backend's native handle structs, its exported
 * IswPlatformOps instance, and the active-backend selection live.
 *
 * SCAFFOLDING ONLY (Phase 0).  Declares the backend-extern hook and the
 * accessor for the active vtable; the XCB backend's concrete handle structs
 * and operation implementations are added per-phase (see
 * docs/ISWPLATFORM_PLAN.md).
 *
 * CRITICAL: XCB types are confined to the XCB backend translation unit.  They
 * MUST NOT leak into ISW/ISWPlatform.h.
 */

#ifndef _ISWPlatformPrivate_h
#define _ISWPlatformPrivate_h

#include "../include/ISW/ISWPlatform.h"

#include <xcb/xcb.h>

/*
 * =================================================================
 * Internal backend seam (Phase 2, temporary, src-only)
 * =================================================================
 *
 * NOT a public escape hatch.  Declared here in the src/-internal header and
 * never in any include/ISW/ header, so application code cannot reach native
 * XCB.  Toolkit/widget .c files for categories not yet abstracted (atoms→6,
 * color/font→4, selection/cursor/grab→5, input→3, resources, plus the XCB
 * drawing/XDND/tray backends) include this header and use these to convert an
 * opaque IswDisplay/IswScreen/IswWindow to the native XCB handle while they
 * await their phase.  The set of users shrinks phase by phase; the seam is
 * deleted after Phase 6.
 *
 * Implemented in src/ISWPlatformDisplayXCB.c.
 */
xcb_connection_t *_IswXcbConn(IswDisplay dpy);
xcb_screen_t     *_IswXcbScreen(IswScreen screen);
xcb_screen_t     *_IswXcbDefaultScreen(IswDisplay dpy);
xcb_window_t      _IswXcbWindow(IswWindow win);
IswWindow         _IswXcbWindowWrap(xcb_window_t id);

/* Color/font/visual value handles (Phase 4).  Plain casts: each handle IS the
   native id/pointer.  Implemented in src/ISWPlatformColorFontXCB.c. */
xcb_colormap_t    _IswXcbColormap(IswColormap cmap);
IswColormap       _IswXcbColormapWrap(xcb_colormap_t cmap);
xcb_font_t        _IswXcbFontId(IswFontId fid);
IswFontId         _IswXcbFontIdWrap(xcb_font_t fid);
xcb_visualtype_t *_IswXcbVisual(IswVisual vis);
IswVisual         _IswXcbVisualWrap(xcb_visualtype_t *vis);

/* Cursor value handle (Phase 5).  Plain cast: the handle IS the native id.
   Implemented in src/ISWPlatformGrabCursorXCB.c. */
xcb_cursor_t      _IswXcbCursor(IswCursor cursor);
IswCursor         _IswXcbCursorWrap(xcb_cursor_t cursor);

/*
 * =================================================================
 * Active platform backend
 * =================================================================
 *
 * The dispatcher resolves a single IswPlatformOps for the process at startup
 * (XCB today).  _IswPlatformGetOps returns it; widget-facing platform wrappers
 * dispatch through it.  Returns NULL before a backend is selected.
 */
const IswPlatformOps *_IswPlatformGetOps(void);

/*
 * =================================================================
 * XCB backend
 * =================================================================
 *
 * The pure-XCB platform backend.  Its exported vtable is wired up as
 * categories are abstracted; the sub-vtable members of this struct are filled
 * in their respective phases.
 */
extern const IswPlatformOps isw_platform_xcb_ops;

#endif /* _ISWPlatformPrivate_h */
