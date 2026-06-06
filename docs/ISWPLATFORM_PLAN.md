# ISWPlatform vtable — execution status

Phase-tracking status file for the **ISWPlatform vtable** work: abstracting all
X11/XCB platform dependencies (display, windows, events, input, grabs, atoms,
selections, colormaps, fonts, cursors, drag-and-drop) behind a pluggable
backend vtable.

**Design lives in [TODO.md](../TODO.md)** under "ISWPlatform vtable — abstract
all X11/XCB platform dependencies." **This file holds execution state only** —
which phase is current, what is done, what is deferred and why.

## How to use this file

- Each phase has a row in the table and a section below.
- Statuses: `todo` · `in-progress` · `done` · `deferred`.
- On completing a phase: flip its row to `done`, add a one-line entry to the
  changelog, and advance **Current phase** to the next row.
- A phase abandoned mid-stream is marked `deferred` with the reason — never
  silently skipped.
- Any future session reads this file first to know exactly where the
  abstraction stands.

**Current phase:** 0 — scaffolding (in-progress)

## Phase table

| Phase | Scope | Status | Primary files |
|---|---|---|---|
| 0 | Scaffolding: vtable header skeleton + this status file | in-progress | `include/ISW/ISWPlatform.h`, `src/ISWPlatformPrivate.h`, `docs/ISWPLATFORM_PLAN.md` |
| 1 | Portable event union + `IswEvent` (unblocks the rest) | todo | Event.c, NextEvent.c, Keyboard.c, Pointer.c |
| 2 | `IswDisplay` + `IswWindow` (core widget lifecycle) | todo | CoreP.h, Display.c, Initialize.c, Core.c, Shell.c, Geometry.c, Composite.c, Create.c, Popup.c |
| 3 | `IswInput` + translation manager | todo | Keyboard.c, Pointer.c, XtTypes.h, TMparse.c, TMstate.c, TMaction.c, TMgrab.c |
| 4 | `IswColor` + `IswFont` | todo | Converters.c, Core.c, Display.c, TextSink.c |
| 5 | `IswSelection`, `IswCursor`, grabs | todo | Selection.c, PassivGrab.c, TMgrab.c, Tip.c, Panner.c, Simple.c |
| 6 | Atoms + properties | todo | ISWAtoms.c, Shell.c, Vendor.c, SetWMCW.c |
| 7 | `IswDragDrop` (XDND refactor) | todo | ISWXdnd.c, ISWXdnd.h |

> Each phase, when started, gets its own scope manifest before edits. Approval
> of one phase is not approval of the next.

## Phases

### Phase 0 — Scaffolding (in-progress)

Introduce the abstraction's file scaffolding and this status tracker **without
altering any runtime path**.

- `include/ISW/ISWPlatform.h` — opaque handles (`IswDisplay`, `IswWindow`,
  `IswDrawable`, `IswPoint`, `IswCursorShape`) and the `IswPlatformOps` struct
  with its eight forward-declared sub-vtables. Declarations only; no XCB types.
- `src/ISWPlatformPrivate.h` — `_ISWPlatformGetOps()` accessor + the
  `isw_platform_xcb_ops` backend extern. Mirrors the ISWRenderPrivate.h split.
- `docs/ISWPLATFORM_PLAN.md` — this file.

No `.c` file, no backend implementation, no widget rewiring. CMake needs no
change: headers install via the `include/ISW/` glob and compile-check on first
`#include`.

### Phase 1 — Portable event union + `IswEvent`

Define a backend-neutral event union the XCB backend populates from
`xcb_generic_event_t` (and other backends from their native events), plus the
poll/translate/modifier-state ops. Hardest piece — events flow into every
widget action proc — so it goes first to unblock everything else.

Files: Event.c (~2,500 lines), NextEvent.c, Keyboard.c, Pointer.c.

### Phase 2 — `IswDisplay` + `IswWindow`

Wrap the display connection (`xcb_connection_t` / `xcb_screen_t` embedded in
`core.display` / `core.screen`) behind `ISWDisplay`, and window lifecycle
(create/configure/map/destroy/reparent) behind `ISWWindow`. Largest single
surface area.

Files: CoreP.h, Display.c, Initialize.c, Core.c, Shell.c, Geometry.c,
Composite.c, Create.c, Popup.c.

### Phase 3 — `IswInput` + translation manager

Abstract the keysym table, keyboard mapping, modifier set, and the
`"Ctrl<Key>a"` parser's event-type/keysym vocabulary behind a backend-neutral
input interface.

Files: Keyboard.c, Pointer.c, XtTypes.h, TMparse.c, TMstate.c, TMaction.c,
TMgrab.c.

### Phase 4 — `IswColor` + `IswFont`

Color alloc/free by name/RGB and colormap/visual handling behind
`IswColor`; font open-by-pattern + metrics behind `IswFont`.

> **Correction to TODO.md:** the TODO lists `AsciiSink.c` / `MultiSink.c` under
> Fonts. Both were deleted in the i18n work; the font-metrics path is now
> `TextSink.c`.

Files: Converters.c, Core.c, Display.c, TextSink.c.

### Phase 5 — `IswSelection`, `IswCursor`, grabs

Clipboard/selection transfer, symbolic-cursor creation/set/free, and
passive/active grabs (stubbable on backends without grab support).

Files: Selection.c (~2,460 lines), PassivGrab.c, TMgrab.c, Tip.c, Panner.c,
Simple.c.

### Phase 6 — Atoms + properties

`xcb_atom_t`, `xcb_change_property`, ICCCM/EWMH hints behind a metadata
mechanism other platforms can implement their own way.

Files: ISWAtoms.c, Shell.c, Vendor.c, SetWMCW.c.

### Phase 7 — `IswDragDrop` (XDND refactor)

Refactor the ~2,100-line XDND v5 implementation behind a semantic
drag/drop vtable (`drag_start`, `drag_set_actions`, `drag_set_icon`,
`drop_register`, `drop_unregister`, enter/leave/motion/drop routing). XCB
backend implements it via XDND atoms + client messages + selection transfers.

Depends on Phases 1, 2, 5. Files: ISWXdnd.c, ISWXdnd.h.

## Changelog

- Phase 0 started: created `ISWPlatform.h`, `ISWPlatformPrivate.h`, and this
  status file.
