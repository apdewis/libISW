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

**Current phase:** 2 — `IswDisplay` + `IswWindow` (not started)

## Phase table

| Phase | Scope | Status | Primary files |
|---|---|---|---|
| 0 | Scaffolding: vtable header skeleton + this status file | done | `include/ISW/ISWPlatform.h`, `src/ISWPlatformPrivate.h`, `docs/ISWPLATFORM_PLAN.md` |
| 1 | Portable event union + `IswEvent` (unblocks the rest) | done | `IswEvent.h`, `ISWPlatformEventXCB.c`, Event.c, TMstate.c, TMaction.c, +~30 widget files |
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

Define the toolkit's own event type, `IswEvent` (`include/ISW/IswEvent.h`),
and decouple the toolkit core and widgets from `xcb_generic_event_t`.

**Decoupling principle (this is the point of the phase):** `IswEvent` carries
ONLY events the toolkit consumes *semantically* — keyboard, pointer, crossing,
focus, redraw, geometry, the few structure transitions widgets observe, and a
window-close request. Everything that is X11 *protocol* stays entirely inside
the platform backend and is NEVER surfaced as an `IswEvent`:

| X11 protocol concern | Where it goes |
|---|---|
| Selection clear/request/notify, PropertyNotify (INCR) | inside Selection.c backend; exposed as the clipboard service API (`IswGetSelectionValue`/`IswOwnSelection`) — widgets already use these, never the events |
| ClientMessage: XDND | inside ISWXdnd.c backend; exposed as the drag-drop service API |
| ClientMessage: tray (`_NET_SYSTEM_TRAY`, `_XEMBED`) | inside IswTrayIcon.c backend |
| ClientMessage: WM_DELETE_WINDOW | backend decodes → `IswCloseRequest` semantic event |
| MappingNotify, keysym tables, raw keycodes | inside the keyboard/translation backend |
| Atoms | never reach toolkit/widget code |

Neutral vocabulary in `IswEvent`: `IswModMask` (Ctrl/Alt/Super… not X mod
bits), `IswKey` + Unicode + UTF-8 text (no keysyms/keycodes), `IswNotifyMode`
+ `IswFocusSource` (collapsing X notify detail), `IswButton`, logical
coordinates, opaque `IswEventTarget` (not a raw window id).

**Work items:**
1. `include/ISW/IswEvent.h` — the neutral union (done).
2. Translator at the poll/dispatch seam: native `xcb_generic_event_t` →
   `IswEvent` for the semantic kinds, folding in the keysym→IswKey/text
   resolution and HiDPI descale that Event.c does today; protocol events are
   routed to the backend's own handlers and never translated.
3. Migrate the toolkit core (Event.c dispatch, TM matching) and widget action
   procs / event handlers to read `IswEvent` fields instead of casting
   `xcb_*_event_t`.
4. Confirm `IswActionProc` / `IswEventHandler` public typedefs carry
   `IswEvent *`.

Files: IswEvent.h (new), Event.c (~2,500 lines), NextEvent.c, Keyboard.c,
Pointer.c, TMstate.c/TMparse.c/TMkey.c/GetActKey.c (TM matching on neutral key
identity + modifiers), plus every widget that reads event fields. Selection.c,
ISWXdnd.c, IswTrayIcon.c are touched only to keep their protocol handling
backend-internal, not to route IswEvents.

**Status: done (build green).** What landed:

- `include/ISW/IswEvent.h` — the neutral union: 17 toolkit-semantic kinds, the
  `IswModMask` / `IswKey` / `IswNotifyMode` / `IswFocusSource` / `IswButton`
  vocabularies, logical coordinates, opaque `IswEventTarget`. No xcb types,
  atoms, keysyms or keycodes.
- `src/ISWPlatformEventXCB.c` — `_IswEventFromXcb()` translates the native
  `xcb_generic_event_t` into `IswEvent` for the semantic kinds (folding in
  keysym→`IswKey`/UTF-8 resolution and HiDPI descale); returns False for X11
  protocol events so the dispatch core routes them to backend handlers without
  ever building an IswEvent. Moves behind the `IswPlatformEvent` sub-vtable in
  Phase 2.
- Public typedefs flipped to `IswEvent *`: `IswActionProc`, `IswEventHandler`,
  `IswExposeProc`, `IswActionHookProc`. Dispatch core (`IswDispatchEventToWidget`,
  `CallEventHandlers`) and the TM action path (`HandleActions`,
  `IswCallActionProc`) translate once and hand `IswEvent *` to widgets.
- **Widget bodies migrated to neutral fields.** Added neutral accessors
  (`IswEventX/Y`, `IswEventModifiers`, `IswEventButton`) and rewrote the
  widget action procs / handlers to read `iswev->kind` and neutral fields
  instead of casting `xcb_*_event_t`. Bridge sites dropped from ~190 to ~35.
- **Native-event escape hatch retained, by design** (`IswEventNative()` /
  `ISW_NATIVE_EVENT()`, re-documented in IswEvent.h). It is no longer a
  temporary shim — it is the deliberate seam for code that operates below the
  neutral layer, and the ~35 remaining uses are all one of:
  1. backend-internal X11 protocol handlers (Selection.c, Shell.c WM,
     ISWXdnd.c, IswTrayIcon.c, ResConfig.c);
  2. re-dispatch through `IswCallActionProc()` (still native — Scrollbar,
     SimpleMenu, MenuBar); retires when the action API goes neutral;
  3. X-only fields not yet abstracted — root coords, native event-window for
     `IswWindowToWidget()`, EnterNotify INFERIOR detail, same_screen; these get
     neutral forms in the Display/Window (Phase 2) and Input (Phase 3) phases;
  4. public callback contracts still exposing the native event
     (`ISWDrawingCallbackData`, Grip↔Paned).
- Library builds green; demo (`isw_demo`) builds and runs; no direct libX11
  NEEDED entry.

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

- Phase 0 done: created `ISWPlatform.h`, `ISWPlatformPrivate.h`, and this
  status file. Handle/op types use the `Isw*` prefix.
- Phase 1 done (build green): added neutral `IswEvent` (`IswEvent.h`) + the
  XCB translator (`ISWPlatformEventXCB.c`); flipped the public action/handler/
  expose/hook typedefs and the dispatch + TM core to `IswEvent *`; migrated
  ~30 widget files (via the `IswEventNative` bridge). X11 protocol events
  (selection / property / client-message / XDND / tray / mapping) stay
  backend-internal and are never surfaced as IswEvents. `libISW.so` links;
  no direct libX11 NEEDED entry. Follow-up: retire the native bridge by reading
  neutral fields in widget procs.
