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
 * Per-category dispatch wrappers
 * =================================================================
 *
 * Toolkit and widget code calls these thin wrappers instead of walking
 * _IswPlatformGetOps()->cat->op at the call site.  Each hides the lookup and
 * null-guards a backend that hasn't filled the op (so a missing op degrades to
 * a no-op / failure rather than a crash).  Same convention as
 * _IswPlatformConnectionFd (IntrinsicI.h).  Implemented in
 * src/ISWPlatformDisplayXCB.c.
 */

/* Color (Phase 4) */
Boolean   _IswPlatformQueryColor(IswDisplay dpy, IswColormap cmap,
                                 unsigned long pixel, IswColor *out);
Boolean   _IswPlatformAllocColor(IswDisplay dpy, IswColormap cmap,
                                 unsigned short red, unsigned short green,
                                 unsigned short blue, unsigned long *pixel_out);
Boolean   _IswPlatformAllocNamedColor(IswDisplay dpy, IswColormap cmap,
                                      const char *name, unsigned long *pixel_out);
Boolean   _IswPlatformLookupColor(IswDisplay dpy, IswColormap cmap,
                                  const char *name);
void      _IswPlatformFreeColors(IswDisplay dpy, IswColormap cmap,
                                 unsigned long pixel);
Boolean   _IswPlatformMatchVisualInfo(IswDisplay dpy, IswScreen screen,
                                      int depth, int visual_class,
                                      IswVisualInfo *out);

/* Font (Phase 4) */
IswFontId _IswPlatformLoadFont(IswDisplay dpy, const char *name);
void      _IswPlatformFreeFont(IswDisplay dpy, IswFontId fid);

/* Cursor (Phase 5) */
IswCursor _IswPlatformLoadNamedCursor(IswDisplay dpy, IswScreen screen,
                                      const char *name,
                                      unsigned int fallback_shape);
void      _IswPlatformSetWindowCursor(IswDisplay dpy, IswWindow win,
                                      IswCursor cursor);
void      _IswPlatformFreeCursor(IswDisplay dpy, IswCursor cursor);

/* Grabs (Phase 5) */
int  _IswPlatformGrabPointer(IswDisplay dpy, IswWindow grab_window,
                             Boolean owner_events, unsigned int event_mask,
                             int pointer_mode, int keyboard_mode,
                             IswWindow confine_to, IswCursor cursor, IswTime time);
void _IswPlatformUngrabPointer(IswDisplay dpy, IswTime time);
int  _IswPlatformGrabKeyboard(IswDisplay dpy, IswWindow grab_window,
                              Boolean owner_events, int pointer_mode,
                              int keyboard_mode, IswTime time);
void _IswPlatformUngrabKeyboard(IswDisplay dpy, IswTime time);
void _IswPlatformGrabButton(IswDisplay dpy, IswWindow grab_window, int button,
                            unsigned int modifiers, Boolean owner_events,
                            unsigned int event_mask, int pointer_mode,
                            int keyboard_mode, IswWindow confine_to,
                            IswCursor cursor);
void _IswPlatformGrabKey(IswDisplay dpy, IswWindow grab_window, IswKeyCode keycode,
                         unsigned int modifiers, Boolean owner_events,
                         int pointer_mode, int keyboard_mode);

/* Selection (Phase 5) */
void      _IswPlatformSetSelectionOwner(IswDisplay dpy, IswWindow owner,
                                        xcb_atom_t selection, IswTime time);
IswWindow _IswPlatformGetSelectionOwner(IswDisplay dpy, xcb_atom_t selection);
void      _IswPlatformConvertSelection(IswDisplay dpy, IswWindow requestor,
                                       xcb_atom_t selection, xcb_atom_t target,
                                       xcb_atom_t property, IswTime time);

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
