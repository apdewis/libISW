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
 * The backend ops table is injected once per connection at display setup
 * (Display.c: InitPerDisplay sets IswPerDisplay->ops) and recovered from the
 * per-display record by each wrapper — there is no process-global accessor.
 * The concrete table for the XCB backend is isw_platform_xcb_ops (below).
 *
 * Backend selection: the active ops table is chosen here, as the first act of
 * init, BEFORE any connection exists — so that connection setup itself
 * (open/close) goes through the vtable.  This is not a global accessor: it is
 * called once at IswOpenDisplay and the result is carried on the per-display
 * record.  With a single backend it returns isw_platform_xcb_ops; a future
 * build/env selector would resolve a different table here.
 */
const IswPlatformOps *_IswPlatformSelectBackend(void);

/*
 * =================================================================
 * Per-category dispatch wrappers
 * =================================================================
 *
 * Toolkit and widget code calls these thin wrappers instead of walking
 * the ops vtable at the call site.  Each recovers the injected ops from the
 * display/widget it is handed, hides the lookup, and null-guards a backend
 * that hasn't filled the op (so a missing op degrades to a no-op / failure
 * rather than a crash).  Same convention as _IswPlatformConnectionFd
 * (IntrinsicI.h).  Implemented in src/ISWPlatformDisplayXCB.c.
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

/* Selection (Phase 5; atoms neutralised in Phase 6) */
void      _IswPlatformSetSelectionOwner(IswDisplay dpy, IswWindow owner,
                                        Atom selection, IswTime time);
IswWindow _IswPlatformGetSelectionOwner(IswDisplay dpy, Atom selection);
void      _IswPlatformConvertSelection(IswDisplay dpy, IswWindow requestor,
                                       Atom selection, Atom target,
                                       Atom property, IswTime time);

/* Atom (Phase 6) */
Atom    _IswPlatformInternAtomOp(IswDisplay dpy, const char *name,
                                 Boolean only_if_exists);
Boolean _IswPlatformGetAtomName(IswDisplay dpy, Atom atom,
                                char *buf, size_t buflen);

/* Property (Phase 6) */
void    _IswPlatformChangeProperty(IswDisplay dpy, IswWindow win, Atom property,
                                   Atom type, int format, IswPropMode mode,
                                   const void *data, uint32_t num_elements);
Boolean _IswPlatformGetProperty(IswDisplay dpy, IswWindow win, Atom property,
                                Atom type, uint32_t long_offset,
                                uint32_t long_length, IswProperty *out);
void    _IswPlatformDeleteProperty(IswDisplay dpy, IswWindow win, Atom property);

/* Window attributes (Phase 13a) */
void    _IswPlatformChangeAttributes(IswDisplay dpy, IswWindow win,
                                     const IswWindowAttributes *attrs,
                                     unsigned int mask);

/* Resource resolution (Phase 15).  Toolkit resource code calls these instead of
   any xcb_xrm_* function; the XCB backend's resource ops implement them over
   libxcb-util-xrm (the only TU that names Xrm). */
IswDatabaseHandle _IswPlatformResourceFromString(const char *str);
IswDatabaseHandle _IswPlatformResourceFromFile(const char *filename);
IswDatabaseHandle _IswPlatformResourceFromManager(IswDisplay dpy,
                                                  IswScreen screen);
void _IswPlatformResourceCombine(IswDatabaseHandle source,
                                 IswDatabaseHandle *target, Boolean override);
void _IswPlatformResourcePut(IswDatabaseHandle *db, const char *resource,
                             const char *value);
void _IswPlatformResourcePutLine(IswDatabaseHandle *db, const char *line);
char *_IswPlatformResourceToString(IswDatabaseHandle db);
void _IswPlatformResourceFree(IswDatabaseHandle db);
int  _IswPlatformResourceGetString(IswDatabaseHandle db, const char *res_name,
                                   const char *res_class, char **out);

/* WM hints (Phase 6) */
void _IswPlatformSetWindowTitle(IswDisplay dpy, IswWindow win, const char *utf8);
void _IswPlatformSetIconTitle(IswDisplay dpy, IswWindow win, const char *utf8);
void _IswPlatformSetWmClass(IswDisplay dpy, IswWindow win,
                            const char *name, const char *class_name);
void _IswPlatformSetWmProtocols(IswDisplay dpy, IswWindow win,
                                const Atom *protocols, int num_protocols);
void _IswPlatformSetTransientFor(IswDisplay dpy, IswWindow win, IswWindow leader);
void _IswPlatformSetWindowType(IswDisplay dpy, IswWindow win, IswWindowType type);
void _IswPlatformSetPid(IswDisplay dpy, IswWindow win, uint32_t pid);
void _IswPlatformSetNormalHints(IswDisplay dpy, IswWindow win, uint32_t flags,
                                int x, int y, int width, int height,
                                int min_width, int min_height,
                                int max_width, int max_height,
                                int width_inc, int height_inc,
                                int min_aspect_num, int min_aspect_den,
                                int max_aspect_num, int max_aspect_den,
                                int base_width, int base_height, int win_gravity);

/* Drag-and-drop (Phase 7).  Thin dispatchers over the platform DnD ops; the
 * generic IswDnd* service calls these.  The whole DnD engine lives in the
 * backend (X11: ISWPlatformDndXCB.c). */
void    _IswPlatformDndEnable(Widget shell);
void    _IswPlatformDndWidgetAcceptDrops(Widget w);
void    _IswPlatformDndStartDrag(Widget source, IswEvent *trigger,
                                 IswDragSourceDesc *desc);
void    _IswPlatformDndSetAcceptedTypes(Widget w, Atom *types, int num_types);
void    _IswPlatformDndSetAcceptedActions(Widget w, IswDndAction actions);
void    _IswPlatformDndSetDropCallback(Widget w, IswCallbackProc proc,
                                       IswPointer closure);
void    _IswPlatformDndSetDragMotionCallback(Widget w, IswCallbackProc proc,
                                             IswPointer closure);
void    _IswPlatformDndSetDragLeaveCallback(Widget w, IswCallbackProc proc,
                                            IswPointer closure);
Atom    _IswPlatformDndInternType(Widget w, const char *mime_type);
Boolean _IswPlatformDndIsDragging(Widget w);

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

/* The XCB backend's resource-resolution ops (Phase 15), wired into
   isw_platform_xcb_ops.resource.  Implemented in ISWPlatformResourceXCB.c —
   the only TU that includes <xcb/xcb_xrm.h>. */
extern const IswPlatformResourceOps isw_platform_xcb_resource_ops;

#endif /* _ISWPlatformPrivate_h */
