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

**Current phase:** 6 — Atoms + properties (not started)

## Phase table

| Phase | Scope | Status | Primary files |
|---|---|---|---|
| 0 | Scaffolding: vtable header skeleton + this status file | done | `include/ISW/ISWPlatform.h`, `src/ISWPlatformPrivate.h`, `docs/ISWPLATFORM_PLAN.md` |
| 1 | Portable event union + `IswEvent` (unblocks the rest) | done | `IswEvent.h`, `ISWPlatformEventXCB.c`, Event.c, TMstate.c, TMaction.c, +~30 widget files |
| 2 | `IswDisplay` + `IswWindow` (core widget lifecycle) | done | ISWPlatform.h, ISWPlatformDisplayXCB.c, ISWPlatformPrivate.h, CoreP.h, Intrinsic.h + ~80 files |
| 3 | `IswInput` + translation manager | done | Keyboard.c, Pointer.c, XtTypes.h, TMparse.c, TMstate.c, TMaction.c, TMgrab.c |
| 4 | `IswColor` + `IswFont` | done | ISWPlatform.h, ISWPlatformColorFontXCB.c, Converters.c, IswTypes.h, CoreP.h, Core.c |
| 5 | `IswSelection`, `IswCursor`, grabs | done | ISWPlatform.h, ISWPlatformGrabCursorXCB.c, PassivGrab.c, Simple.c, Converters.c, MenuBar.c, List.c, Selection.c |
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

Wrap the display connection and window identity behind the opaque handles
`IswDisplay` / `IswScreen` / `IswWindow`, and the display + window lifecycle
behind the `IswPlatformDisplayOps` / `IswPlatformWindowOps` sub-vtables.
Full breaking change (per user direction): NO public escape hatch — the public
API is XCB-free.

**Status: done (build green, demo verified).** What landed:

- **Opaque handles** `IswDisplay` / `IswScreen` / `IswWindow` (typedef'd in
  `Intrinsic.h`).  In the XCB backend each is the native value reinterpreted —
  `IswDisplay` *is* `xcb_connection_t*`, `IswScreen` *is* `xcb_screen_t*`,
  `IswWindow` *is* the `xcb_window_t` id — so the seam conversions are plain
  casts (no wrapper alloc, no lookup, can't crash on a stale value) and
  `core.display` is interchangeable with the conn the dispatch/per-display
  layers carry.
- **Vtable + backend**: `IswPlatformDisplayOps` (open/close/has_error/flush/
  connection_fd/screen enumeration/root/bell) and `IswPlatformWindowOps`
  (alloc_id/create/destroy/map/unmap/reparent/configure/change_attributes/
  clear_area/id↔handle) in `ISWPlatform.h`, implemented over XCB in
  `src/ISWPlatformDisplayXCB.c`; `isw_platform_xcb_ops` + `_IswPlatformGetOps`.
- **Accessors renamed** `IswDisplay()`→`IswDisplayOf()`, `IswScreen()`→
  `IswScreenOf()`, `IswWindow()`→`IswWindowOf()` (a typedef and a same-named
  function-like macro cannot coexist); `…OfObject` variants kept their names,
  now returning opaque handles.  `core.display/screen/window` and
  `hooks.display/screen` fields are opaque handles.
- **Public headers are XCB-free**: Intrinsic.h, DrawingArea.h, IswTrayIcon.h,
  ISWContext.h, ScrollWheel.h carry only the opaque handles.  The old
  `ConnectionNumber()` XCB macro is retired in favour of
  `_IswPlatformConnectionFd()` (a display-op wrapper).  `libISW.so` has no
  direct libX11 NEEDED entry.
- **Internal seam** (`_IswXcbConn`/`_IswXcbScreen`/`_IswXcbDefaultScreen`/
  `_IswXcbWindow`/`_IswXcbWindowWrap`), declared ONLY in
  `src/ISWPlatformPrivate.h` (never a public header).  Used by ~57 src files
  whose categories await their own phase (atoms→6, color/font→4, selection/
  cursor/grab→5, input/keysym→3, resources, plus the XCB drawing/XDND/tray
  backends).  This is the Phase-2 seam: it shrinks phase by phase and is
  deleted after Phase 6.  See `docs/PHASE2_MIGRATION_SPEC.md`.

Verified: demo starts, renders, and survives menu-bar / scroll / combobox
open-close / resize / button interactions; rendering correct after resize+scroll.

Files: ISWPlatform.h, ISWPlatformDisplayXCB.c (new), ISWPlatformPrivate.h,
CoreP.h, HookObjI.h, Intrinsic.h + ~80 src files (accessor rename + seam).

### Phase 3 — `IswInput` + translation manager

Abstract the keysym table, keyboard mapping, modifier set, keycode↔keysym
translation, keysym-by-name, case folding, mapping refresh and pointer query
behind a backend-neutral input interface. Same contract as Phases 1–2: no
public escape hatch; public headers carry no xcb keysym/keycode types.

**Status: done (build green, demo verified).** What landed:

- **Neutral vocabulary**: `IswKeyCode` / `IswKeySym` (uint32_t, numerically
  X11-keysym/keycode compatible — existing 0xff.. constants and the TM tables
  keep working — but carrying NO xcb dependency), defined in IswTypes.h.
  `KeySym` is now `IswKeySym`, not `xcb_keysym_t`.  The obsolete `_IswKeyCode`
  widening macro is removed.
- **`IswPlatformInputOps`** (ISWPlatform.h) implemented over XCB in
  `src/ISWPlatformInputXCB.c`: keycode_to_keysym, keysym_to_keycodes,
  keysym_from_name, keysym_to_name, convert_case, translate_keycode,
  refresh_mapping, query_pointer.  Wired into `isw_platform_xcb_ops.input`.
  The per-display keysym/modifier cache stays in TMkey.c (single
  implementation); the backend reaches it via `_IswXcbKeysyms` /
  `_IswXcbRefreshKeysyms` bridges.
- **Public key APIs neutralised**: `IswTranslateKeycode` / `IswTranslateKey` /
  `IswKeysymToKeycodeList` / `IswConvertCase` / `IswRegisterCaseConverter` flip
  their xcb keysym/keycode params to `IswKeySym` / `IswKeyCode`.
  `IswGetKeysymTable` (returns the native table) moved OUT of the public API to
  TranslateI.h — it is backend-internal.
- **Public headers XCB-free** for input: no `xcb_keysym_t` / `xcb_keycode_t` /
  `xcb_key_symbols_t` in Intrinsic.h / IswTypes.h / the widget headers.
- Verified: demo starts, renders, and survives typing into fields, Tab focus
  traversal, arrow/Escape/Return keys, plus scroll + menu.  No direct libX11.
- **Seam users (Phase 3 retire list)**: ~16 src files still use xcb key types
  internally (TMkey/TMparse/TMstate/TMgrab/TMprint/GetActKey, Keyboard,
  PassivGrab, FocusMgr, SimpleMenu, MenuButton, SmeBSB, the XCB draw/XDND
  backends, plus the two platform backend TUs).  These are toolkit-internal /
  backend; the TYPE no longer leaves the public surface.  They shrink as later
  phases (grabs→5, XDND→7) land.

Files: ISWPlatform.h, ISWPlatformInputXCB.c (new), IswTypes.h, Intrinsic.h,
TranslateI.h, TMkey.c, TMgrab.c + the input/keysym src files.

### Phase 4 — `IswColor` + `IswFont`

Color alloc/free by name/RGB and colormap/visual handling behind
`IswColor`; font open-by-pattern + metrics behind `IswFont`.

> **Correction to TODO.md:** the TODO lists `AsciiSink.c` / `MultiSink.c` under
> Fonts. Both were deleted in the i18n work; the font-metrics path is now
> `TextSink.c`.

Files: Converters.c, Core.c, Display.c, TextSink.c.

**Done** (build green, demo verified). Scope manifest: `docs/PHASE4_SCOPE.md`.

- Neutral handles in `IswTypes.h`: `IswColormap`/`IswFontId` (value handles
  over the native id) and `IswVisual`/`IswVisualId`. `IswColor`/`IswVisualInfo`/
  `IswFontStruct` struct names were already neutral; their embedded xcb field
  types (`xcb_visualtype_t*`, `xcb_visualid_t`, `xcb_font_t`, `xcb_colormap_t`)
  are now the handles.
- `IswPlatformColorOps` (query/alloc/alloc-named/lookup/free color +
  match_visual_info) and `IswPlatformFontOps` (load/free core font) added to
  `ISWPlatform.h`; backend `src/ISWPlatformColorFontXCB.c` implements them and
  is wired into `isw_platform_xcb_ops.color`/`.font`. The
  `_IswXcbColormap`/`_IswXcbFontId`/`_IswXcbVisual` value-cast seam helpers join
  the internal seam.
- `Converters.c` color/named-color/visual/font converters route through the ops;
  `_IswMatchVisualInfo` moved into the backend. Pixel→RGB inline queries in
  `Label.c`/`ToggleButton.c`/`IconView.c` now call `color->query_color`.
- Type-only flips: `CoreP.h core.colormap` → `IswColormap` (with matching
  resource `sizeof` in `Core.c`/`Vendor.c`/`Converters.c` — these MUST move
  together or the resource copy corrupts the field), `ShellP.h visual` →
  `IswVisualId`, `IswCreateWindow` visual param → `IswVisual`
  (`IntrinsicP.h`/`Intrinsic.c`/`Core.c`/`Simple.c`), `Resources.c` default
  colormap.
- The fontconfig/FreeType metrics path (the real text loader) was already
  XCB-free and is unchanged. Cursor-font path and `xcb_create_colormap` in the
  tray/XDND backends stay on the seam (Phases 5/7). Pixmaps stay xcb (render
  layer). No xcb color/font/visual TYPE remains in any `include/ISW/` header;
  libISW.so keeps no direct libX11 NEEDED.

### Phase 5 — `IswSelection`, `IswCursor`, grabs

Clipboard/selection transfer, symbolic-cursor creation/set/free, and
passive/active grabs (stubbable on backends without grab support).

Files: Selection.c (~2,460 lines), PassivGrab.c, TMgrab.c, Tip.c, Panner.c,
Simple.c.

**Done** (build green, demo verified). Scope manifest: `docs/PHASE5_SCOPE.md`.

> **Scope boundary (decided with the user):** selections are atom-typed and
> `Selection.c` is property-exchange-heavy; **atoms + properties are Phase 6**.
> Phase 5 abstracted only the three pure-selection verbs (`set_owner`,
> `get_owner`, `convert`) — they keep `xcb_atom_t` params, which Phase 6 retypes
> to the neutral `Atom`. Selection.c's property plumbing, `xcb_send_event`, and
> `xcb_selection_request_event_t*` stay on the seam until Phase 6.

- Neutral handles in `IswTypes.h`: `IswCursor` (value handle over
  `xcb_cursor_t`, 0 = none) and `IswTime` (server timestamp; `ISW_CURRENT_TIME`
  = 0). `IswCursorShape` already existed.
- Three sub-vtables added to `ISWPlatform.h` and implemented in the new
  `src/ISWPlatformGrabCursorXCB.c`, wired into `isw_platform_xcb_ops`:
  - `IswPlatformCursorOps` — `create_glyph` / `load_named` (theme-aware, glyph
    fallback) / `set_window_cursor` / `free_cursor`. The glyph + themed cursor
    creation moved out of Converters.c into the backend.
  - `IswPlatformGrabOps` — pointer/keyboard/button/key grab + ungrab +
    `allow_events`.
  - `IswPlatformSelectionOps` — `set_owner` / `get_owner` / `convert`.
  - `_IswXcbCursor`/`_IswXcbCursorWrap` value-cast seam helpers added.
- Routing: `PassivGrab.c` active+passive grabs through the grab ops;
  `Keyboard.c`, `MenuBar.c` (menubar popup grab), `List.c` (combo/list popup
  grab) ungrab/grab via ops; `Simple.c` `_IswSetWindowCursor`/`_IswFreeCursor`
  via cursor ops (`_IswChangeActivePointerGrabCursor` stays seam — a narrow
  active-grab-cursor refresh); `Converters.c` cursor converter +
  `_IswLoadThemedCursor` delegate to the cursor op; `Selection.c` the three
  selection verbs via the selection ops.
- Type-only flips across the public surface: `xcb_cursor_t` → `IswCursor`
  (SimpleP/PanedP/ListViewP/SimpleMenP/PassivGraI/Scrollbar + grab APIs) and
  `xcb_timestamp_t` → `IswTime` (Intrinsic.h grab/selection APIs +
  IntrinsicP/ShellI/SelectionI/ListBoxP/TextP/InitialI). Cursor resource sizes
  (`Simple.c`) flipped to `sizeof(IswCursor)` in lockstep with the field.
- Verified live: pointer/menu grab (Edit menu opens under the menubar grab,
  renders, closes cleanly on item-click — the Phase-3 crash spot), combo/list
  popup grab, and widget rendering all work. XDND (Phase 7) and the tray icon
  keep their seam grab/selection use. No `xcb_cursor_t`/`xcb_timestamp_t` TYPE in
  any `include/ISW/` header; libISW.so keeps no direct libX11 NEEDED.

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
- Phase 2 done (build green, demo verified): opaque `IswDisplay`/`IswScreen`/
  `IswWindow` handles + `IswPlatformDisplayOps`/`IswPlatformWindowOps` vtables
  (`ISWPlatformDisplayXCB.c`).  Accessors renamed to `…Of`; `core`/`hooks`
  display/screen/window fields opaque.  Public headers XCB-free; `ConnectionNumber`
  retired for `_IswPlatformConnectionFd`.  Internal seam (`_IswXcb*`, src-only)
  carries the not-yet-abstracted categories (~57 files) and is retired through
  Phases 3–6.  No public escape hatch.  No direct libX11 NEEDED entry.
- Phase 3 done (build green, demo verified): neutral `IswKeyCode`/`IswKeySym`
  vocabulary + `IswPlatformInputOps` (`ISWPlatformInputXCB.c`).  Public key APIs
  (`IswTranslateKeycode`/`IswTranslateKey`/`IswKeysymToKeycodeList`/
  `IswConvertCase`) neutralised; `KeySym` is `IswKeySym`; `IswGetKeysymTable`
  moved to TranslateI.h (backend-internal).  No xcb keysym/keycode types in
  public headers.  Keysym cache stays in TMkey.c.  Typing/Tab/special-keys
  verified.  No direct libX11 NEEDED entry.
- Phase 4 done (build green, demo verified): neutral `IswColormap`/`IswFontId`/
  `IswVisual`/`IswVisualId` value handles + `IswPlatformColorOps`/
  `IswPlatformFontOps` (`ISWPlatformColorFontXCB.c`).  Converters.c color/
  named-color/visual/font paths and the `Label`/`Toggle`/`IconView` pixel→RGB
  queries route through the ops; `_IswMatchVisualInfo` moved into the backend.
  `core.colormap` and resource sizes flipped together (`CoreP.h`/`Core.c`/
  `Vendor.c`/`Converters.c`); `ShellP.h visual` → `IswVisualId`; `IswCreateWindow`
  visual param → `IswVisual`.  Fontconfig/FreeType metrics path unchanged
  (already XCB-free).  Cursors + `xcb_create_colormap` (tray/XDND) stay on the
  seam (Phases 5/7); pixmaps stay xcb (render layer).  No xcb color/font/visual
  types in public headers.  No direct libX11 NEEDED entry.
- Phase 5 done (build green, demo verified): neutral `IswCursor`/`IswTime`
  (`ISW_CURRENT_TIME`) + `IswPlatformCursorOps`/`IswPlatformGrabOps`/
  `IswPlatformSelectionOps` (`ISWPlatformGrabCursorXCB.c`).  Glyph/themed cursor
  creation moved into the backend; grabs (PassivGrab active+passive, MenuBar/
  List popup grabs, Keyboard ungrab) and the three selection verbs (set/get
  owner, convert) route through the ops.  Public surface flipped
  `xcb_cursor_t`→`IswCursor` (10 headers + grab APIs) and
  `xcb_timestamp_t`→`IswTime` (selection/grab/event time).  Scope cut at the
  user's choice: selection atoms + property exchange stay Phase 6 (selection
  verbs keep `xcb_atom_t`).  XDND (Phase 7) + tray keep seam grab/selection use.
  No xcb cursor/timestamp types in public headers.  No direct libX11 NEEDED
  entry.
