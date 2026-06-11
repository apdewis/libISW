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

#include <ISW/ISWPlatform.h>

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

/* Reverse widget<->window lookup (window -> owning widget), used by event
   translation to resolve an event's dispatch target to its root widget. */
Widget            _IswXcbWidgetForWindow(IswDisplay dpy, xcb_window_t window);

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
 *
 * All the neutral per-category dispatch wrappers (_IswPlatformSelectBackend,
 * color/font/cursor/grab/pointer, atom/property, window lifecycle/attributes,
 * root surface, resources, WM hints, and the selection wrappers) are declared
 * in the public neutral header ISW/ISWPlatform.h beside the ops vtable they
 * dispatch through — toolkit and widget code reaches them there, never via this
 * backend-private header.  Only the raw _IswXcb* seam bridges (above), the DnD
 * wrappers (below, used solely by the X11 DnD backend), and the backend vtable
 * externs remain backend private.
 */

 /* this file is to NEVER be included in core widget code */
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
