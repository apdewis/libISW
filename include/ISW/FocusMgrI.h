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

/* True while the Alt modifier is held (tracked by the dispatcher for
 * rendering menu-mnemonic underlines). */
extern Boolean _IswFocusMgrAltHeld(void);

/* True if mnemonic underlines should be visible for this menu right now.
 * That's the case when Alt is held, OR the menu was opened via a
 * mnemonic (in which case underlines persist until it's dismissed). */
extern Boolean _IswFocusMgrShowMnemonicsForMenu(Widget menu);

/* Find the first index in 'label' whose lowercase character matches
 * the lowercase letter of 'mnemonic' keysym. Returns -1 if no match or
 * mnemonic is 0 / not a printable letter. */
extern int _IswFocusMgrFindMnemonicIndex(const char *label,
                                         xcb_keysym_t mnemonic);

/* Register a SimpleMenu shell so mnemonic dispatch knows when it opens
 * and closes. Safe to call multiple times. */
extern void _IswFocusMgrRegisterMenu(Widget menu);

#endif /* _ISW_FocusMgrI_h */
