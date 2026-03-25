# Keyboard Navigation Plan

ISW currently has no keyboard navigation beyond what the Text widget provides. This plan covers adding full keyboard support.

## Current State

- **Xt keyboard infrastructure exists**: `XtSetKeyboardFocus`, `XtGetKeyboardFocusWidget`, key event dispatch, translation manager `<Key>` handling all work.
- **No Tab traversal**: Motif had this, Xaw never did. ISW inherits the gap.
- **No widget-level key bindings**: Interactive widgets (Command, Toggle, List, IconView, Scale, SpinBox, menus) have no keyboard activation or navigation.

## Work Items

### 1. Tab Traversal (focus manager)

- Add a focus manager at the Shell level that maintains an ordered list of focusable widgets.
- Tab / Shift+Tab cycles focus forward/backward.
- Each focusable widget draws a visible focus indicator (highlight ring or similar).
- Widgets opt in via a resource (e.g. `XtNtraversalOn`, Boolean, default True for interactive widgets).
- Focus order follows widget tree order by default, with an optional `XtNtabIndex` resource for explicit ordering.

### 2. Per-Widget Key Bindings

| Widget | Keys |
|--------|------|
| Command / Toggle | Space or Enter to activate |
| List | Up/Down arrow to move selection, Home/End, Page Up/Down, type-ahead search |
| IconView | Arrow keys (grid-aware: left/right/up/down), Home/End, Space to toggle selection in multi-select mode, Ctrl+A to select all |
| Scale | Left/Down to decrement, Right/Up to increment, Home/End for min/max, Page Up/Down for large steps |
| SpinBox | Up/Down arrows to increment/decrement (in addition to button clicks) |
| Scrollbar | Arrow keys, Page Up/Down |
| ComboBox | Up/Down to change selection, Enter to confirm, Escape to close dropdown |

### 3. Menu Keyboard Access

- Alt+key mnemonics to open menubar items (underlined character in label).
- Arrow keys to navigate within open menus (up/down within a menu, left/right between menubar items).
- Enter to activate, Escape to close.
- Accelerator keys (e.g. Ctrl+S) dispatched from the Shell level.
- `XtNmnemonic` resource on menu entries.

## Implementation Order

1. **Focus manager + Tab traversal** — foundation everything else depends on.
2. **Command/Toggle activation** — simplest widget key bindings, good test of focus system.
3. **List/ComboBox keyboard nav** — high value, commonly needed.
4. **IconView keyboard nav** — grid-aware arrow movement.
5. **Scale/SpinBox keyboard adjustment** — straightforward.
6. **Menu keyboard access** — most complex (mnemonics, accelerators, arrow navigation across menus).

## Design Notes

- The focus manager should live in Shell.c or a new FocusMgr.c, not in individual widgets.
- Focus indicator drawing should be handled by each widget's Redisplay, triggered by FocusIn/FocusOut events.
- Xt already dispatches FocusIn/FocusOut — widgets just need translations for `<FocusIn>` and `<FocusOut>` to set a flag and redraw.
- Avoid Motif's complexity (modal focus, exclusive grabs, traversal groups). A flat Tab-order list covering the shell's children is sufficient for now.
