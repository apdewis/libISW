/*
 * FocusMgrI.h - Internal focus manager API
 *
 * Per-shell Tab/Shift-Tab traversal between widgets that opt in via
 * the IswNtraversalOn resource on Simple.
 *
 * This header is internal to libISW and not exposed as part of the
 * public API.
 */

#ifndef _ISW_FocusMgrI_h
#define _ISW_FocusMgrI_h

#include <ISW/Intrinsic.h>

/* Lazy install: registers focus-next / focus-prev actions globally,
 * augments shell translations so Tab / Shift+Tab cycle focus. Safe to
 * call repeatedly; the shell-level setup happens once per shell.
 *
 * Called from VendorShell ChangeManaged when the first child appears.
 */
extern void _IswFocusMgrEnsureInstalled(Widget shell);

/* Called when a shell is being destroyed. Frees per-shell state. */
extern void _IswFocusMgrDestroyShell(Widget shell);

/* Called from the event dispatcher before a KeyPress is routed to the
 * focus descendant. If the key is Tab / Shift+Tab (and the shell has
 * traversable widgets), advances focus and returns True so the
 * dispatcher skips normal delivery. Otherwise returns False. */
extern Boolean _IswFocusMgrMaybeHandleKey(Widget widget,
                                          xcb_generic_event_t *event);

/* Draw a dashed focus ring inset 'pad' pixels from the widget's border,
 * using 'color' as the stroke color. No-ops if the widget doesn't own
 * focus (i.e. SimplePart.has_focus is False). 'ctx' must be an already-
 * begun render context for the widget. */
extern void _IswFocusMgrDrawRing(Widget w, void *ctx,
                                 unsigned long color, double pad);

#endif /* _ISW_FocusMgrI_h */
