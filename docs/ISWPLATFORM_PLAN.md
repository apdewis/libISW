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

**Current phase:** 10b — retype the display off the connection. Phases 0–9, 11a,
12a, 13a, 14, 15 are done. **The real objective is a core `xcb_*` census of 0**
(see "The actual goal" below); as of 2026-06-09 the core still holds **1350**
`xcb_*` refs, so every remaining phase is measured against that number, not a
public-header grep.

## Phase table

| Phase | Scope | Status | Primary files |
|---|---|---|---|
| 0 | Scaffolding: vtable header skeleton + this status file | done | `include/ISW/ISWPlatform.h`, `src/ISWPlatformPrivate.h`, `docs/ISWPLATFORM_PLAN.md` |
| 1 | Portable event union + `IswEvent` (unblocks the rest) | done | `IswEvent.h`, `ISWPlatformEventXCB.c`, Event.c, TMstate.c, TMaction.c, +~30 widget files |
| 2 | `IswDisplay` + `IswWindow` (core widget lifecycle) | done | ISWPlatform.h, ISWPlatformDisplayXCB.c, ISWPlatformPrivate.h, CoreP.h, Intrinsic.h + ~80 files |
| 3 | `IswInput` + translation manager | done | Keyboard.c, Pointer.c, XtTypes.h, TMparse.c, TMstate.c, TMaction.c, TMgrab.c |
| 4 | `IswColor` + `IswFont` | done | ISWPlatform.h, ISWPlatformColorFontXCB.c, Converters.c, IswTypes.h, CoreP.h, Core.c |
| 5 | `IswSelection`, `IswCursor`, grabs | done | ISWPlatform.h, ISWPlatformGrabCursorXCB.c, PassivGrab.c, Simple.c, Converters.c, MenuBar.c, List.c, Selection.c |
| 6 | Atoms + properties | done | ISWPlatform.h, ISWPlatformAtomPropXCB.c, ISWAtoms.c, Shell.c, Selection.c, ResConfig.c, SetWMCW.c, Tip.c, SimpleMenu.c, Intrinsic.c, TMprint.c, TMstate.c, TextAction.c, Display.c, Converters.c |
| 7 | `IswDragDrop` — generic DnD service; XDND becomes the X11 backend | done | ISWPlatform.h, ISWPlatformDndXCB.c (new, from ISWXdnd.c), IswDragDrop.h (new), IswTypes.h, ISWPlatformPrivate.h, ISWPlatformDisplayXCB.c, CMakeLists.txt, Shell.c, IconView.c, isw_demo.c |
| 8 | Dependency-inject the ops table (kill the singleton) | done | ISWPlatformPrivate.h, ISWPlatformDisplayXCB.c + every `_IswPlatform*` wrapper + the per-display init path |
| 9 | Route connection setup through the vtable (open/close) | done | Display.c, ISWPlatformDisplayXCB.c |
| 10 | Break `IswDisplay == xcb_connection_t*` — 10a native seam done; **10b** retype display off the connection (census `xcb_connection_t` 189→0) | 10a done, 10b todo | InitialI.h, Display.c, Selection.c, TMkey.c, the I-headers, ISWPlatformDisplayXCB.c |
| 11 | Event loop + retire native-event bridge (census event-struct 342→0) — 11a poll ops done | 11a done, 11 todo | ISWPlatformEventXCB.c, NextEvent.c, Event.c, EventUtil.c, TMstate.c, +widgets |
| 12 | Per-display state — 12a field retypes done; **12b** relocate keysym machinery (census keysym 18→0) | 12a done, 12b todo | TMkey.c, FocusMgr.c, ISWPlatformInputXCB.c |
| 13 | Selection/property + tray + **13c** raw draw/window-call routing (census 168→0) — 13a done | 13a done, 13b/13c todo | Selection.c, IswTrayIcon.c, +widgets, ISWPlatform.h |
| 14 | Replace XFixes damage regions with client-side region | done | IswTypes.h, IntrinsicP.h, Event.c, Display.c, +~32 widgets |
| 15 | Resource resolution behind ops (Xrm → X11-backend detail) | done | IswDatabase.h, ISWPlatform.h, ISWPlatformResourceXCB.c, Initialize.c, Resources.c, +6 files |
| 16 | Purge `xcb_*` from the public API | todo | Intrinsic.h, IswEvent.h, IswTypes.h, ISWRender.h, IswTrayIcon.h, TextSink.h |
| 17 | Prove it: a second (stub/null) backend; **core census must be 0** | todo | new stub backend TU, CMakeLists.txt |

> Each phase, when started, gets its own scope manifest before edits. Approval
> of one phase is not approval of the next. **Every phase ends by re-running the
> core `xcb_*` census and recording before→after; the branch is done when it is
> 0.**

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

**Done** (build green, demo verified). Scope manifest: `docs/PHASE6_SCOPE.md`.

Decisions (with the user): **generic property ops; XDND/tray stay seam**, and
**semantic hint ops** for the cross-platform-meaningful WM hints.

- `Atom` was already a neutral `uint32_t`; flipped the 85 `xcb_atom_t`
  occurrences across 12 public headers to `Atom` (incl. the Phase-5 selection
  verbs). Added `IswPropMode`, `IswProperty` (+ `_IswPlatformFreeProperty`),
  `IswWindowType`, `ISW_ATOM_*` to ISWPlatform.h.
- Three sub-vtables in the new `src/ISWPlatformAtomPropXCB.c`, wired into
  `isw_platform_xcb_ops`, reached via thin `_IswPlatform*` dispatch wrappers
  (the Phase-8 convention, not `_IswPlatformGetOps()` at call sites):
  - `IswPlatformAtomOps` — `intern` / `get_name`.
  - `IswPlatformPropertyOps` — `change` / `get` (returns neutral `IswProperty`,
    no xcb reply structs leak) / `delete`.
  - `IswPlatformHintOps` — semantic `set_window_title` (WM_NAME + _NET_WM_NAME),
    `set_icon_title`, `set_wm_class`, `set_wm_protocols`, `set_transient_for`,
    `set_window_type` (incl. POPUP_MENU/TOOLTIP/DIALOG), `set_pid`,
    `set_normal_hints` (carries aspect — widened mid-phase so it isn't lossy).
- Routed: ISWAtoms (now delegates to the op), Shell.c (all ~120 sites: title/
  icon/class/protocols/transient/window-type/pid/normal-hints semantic; locale/
  client-leader/role/command/state/icon/user-time/startup-id generic),
  Selection.c (the Phase-5-deferred transfer machinery — change/get/delete +
  interns; fixed a latent use-after-free and avoided malloc-vs-IswFree
  allocator mismatches by copying into `__XtMalloc` where downstream `IswFree`s),
  ResConfig/Tip/SimpleMenu/Intrinsic/TMprint/TMstate/TextAction/Display/
  Converters.
- **On the seam by design / scope:** `xcb_send_event` client-messages (startup-id
  remove, _NET_WM_STATE, iconify), `xcb_create_window` for the user-time helper
  window, and `xcb_icccm_set_wm_hints` (reads a private xcb_icccm struct field
  the toolkit hasn't neutralised — not atom/property work). XDND (Phase 7) +
  tray + render backends keep their raw atom/property use.
- Verified live: window title in the WM (WM_NAME + _NET_WM_NAME), _NET_WM_PID,
  WM_PROTOCOLS=WM_DELETE_WINDOW, and a graceful WM_DELETE close (full atom
  round-trip: intern → advertise → match incoming → handle). Clipboard
  selection-conversion path exercised without crash. No `xcb_atom_t` in any
  `include/ISW/` header; no direct libX11 NEEDED.

### Phase 7 — `IswDragDrop` — generic DnD service; XDND becomes the X11 backend

**The correction.** Earlier drafts of this phase treated XDND as a thing to
*wrap*: keep `ISWXdnd.c` as the home, keep the public API XDND-shaped
(`ISWXdndStartDrag`, `XdndAware`, XDND action atoms), and expose a few entry
points through a vtable. That is wrong. XDND is **not** a neutral drag-and-drop
abstraction that happens to run on X — it *is* an X11 wire protocol: a fixed set
of `Xdnd*` atoms, a ClientMessage state machine (Enter/Position/Status/Leave/
Drop/Finished), ownership of `XdndSelection`, foreign-window discovery via the
`XdndAware` property, X cursors and an override-redirect drag-icon window. None
of that survives on Wayland (which has its own `wl_data_device` DnD) or on any
non-X target. So XDND belongs **inside the platform backend**, exactly like the
selection, atom, and grab machinery did — and the widget-/application-facing
surface must become a **generic, transport-agnostic drag-and-drop service** that
names nothing X-specific.

**The split.**

1. **Generic public service — `include/ISW/IswDragDrop.h` (new).** A
   protocol-neutral DnD API. Renames the `ISWXdnd*` entry points to `IswDnd*`
   and strips every XDND-ism from the *vocabulary*:
   - `IswDndEnableSource` / `IswDndStartDrag` — begin a drag from a widget,
     given a transport-neutral `IswDragSourceDesc`.
   - `IswDndRegisterDropTarget` / `IswDndUnregisterDropTarget`,
     `IswDndSetAcceptedTypes` / `IswDndSetAcceptedActions`, the drop / enter /
     motion / leave callbacks, `IswDndIsDragging`.
   - MIME types stay as the neutral `Atom` (an intern token, *not* an X concept
     by the time it reaches the backend — Phase 6 already neutralised `Atom`).
     `IswDndAction` (copy/move/link/ask/private) is already protocol-neutral and
     is kept. The drag-icon field flips `xcb_pixmap_t icon_pixmap` →
     a neutral handle (`IswPixmap`/`IswImage`) so the struct carries no xcb type.
   - No `XdndAware`, no `XdndSelection`, no XDND version, no ClientMessage in any
     public type or doc comment.

2. **X11 backend — `src/ISWPlatformDndXCB.c` (new, the body of ISWXdnd.c).** The
   entire ~2,100-line protocol engine moves here essentially intact: `InternAtoms`
   for the `Xdnd*` set, the source/target state machines, `XdndSelection`
   ownership, `FindXdndAwareWindow`, the drag cursors and the drag-icon window,
   URI-list parsing. It is reached through a `IswPlatformDndOps` sub-vtable on
   `IswPlatformOps`, wired into `isw_platform_xcb_ops` and called via thin
   `_IswPlatformDnd*` dispatch wrappers (the same convention as Phases 2/6). The
   generic service in (1) is a thin shim over those wrappers — it holds the
   per-shell DnD registration/callback bookkeeping (the part that *is* portable)
   and delegates all wire work to the backend.

3. **`include/ISW/ISWXdnd.h` — transitional compat shim, since removed.** Shipped
   as a thin header aliasing the old `ISWXdnd*` names to `IswDnd*` to keep the
   structural move reviewable, then deleted once all callers (Shell.c, IconView.c,
   the demo) were migrated to `IswDnd*`. The final state has no shim.

**`IswPlatformDndOps` (semantic, transport-neutral).** Verbs mirroring the
public service one-for-one: `enable`, `widget_accept_drops`,
`start_drag(widget, trigger_event, desc)`, `set_accepted_types`,
`set_accepted_actions`, `set_drop_callback`, `set_drag_motion_callback`,
`set_drag_leave_callback`, `intern_type`, `is_dragging`. The XCB backend maps
each to XDND; a future Wayland backend maps them to `wl_data_device` /
`wl_data_source`.

**Correction to the earlier draft — the engine moves whole.** This section once
proposed splitting "portable bookkeeping" (drop-target registration, type/action
filters, callback dispatch) into the service and leaving only the wire protocol
in the backend. The code does not support a clean split: that bookkeeping is
interwoven with the XDND state machine through one shared `XdndState` struct and
is driven from inside the XDND handlers (e.g. `FindDropTarget` itself calls
`xcb_translate_coordinates`). Splitting mid-protocol was judged unjustified
surgery on a working 2,100-line implementation. So the **whole engine moved into
the backend** (`ISWPlatformDndXCB.c`) behind `IswPlatformDndOps`; the generic
`IswDnd*` service is a thin dispatcher. The goal is met regardless — X11 DnD is
platform-specific code, the public surface is transport-neutral, a non-X backend
supplies its own ops. See `docs/PHASE7_SCOPE.md`.

Depends on Phases 1, 2, 5, 6.

Files: ISWPlatform.h, ISWPlatformDndXCB.c (new), IswDragDrop.h (new), IswTypes.h,
ISWPlatformPrivate.h, ISWPlatformDisplayXCB.c, CMakeLists.txt, Shell.c,
IconView.c, isw_demo.c.

**Done** (build green, demo verified). Scope manifest: `docs/PHASE7_SCOPE.md`.

- `src/ISWXdnd.c` → `src/ISWPlatformDndXCB.c`: the XDND v5 engine intact; public
  `ISWXdnd*` entry points became static `xcb_dnd_*` ops feeding the new
  `isw_platform_xcb_dnd_ops`. `start_drag` takes the neutral `IswEvent *` and
  recovers the native button event via `IswEventNative()` (the backend tracks
  drags in physical coords — threshold/icon/hit-test all want raw geometry).
- New public `include/ISW/IswDragDrop.h`: `IswDnd*` names, neutral `Atom` MIME
  types, new `IswPixmap` handle for the icon, `IswDndStartDrag(Widget, IswEvent*,
  desc)`. No `xcb_*` / `Xdnd*` in it.
- Shim removal completed (was the Phase-7 follow-on): the internal callers
  (Shell.c `IswDndEnable`, IconView.c `IswDndIsDragging`) and the demo now call
  `IswDnd*` directly; `include/ISW/ISWXdnd.h` is **deleted**. No `ISWXdnd*`
  references remain in any source. The app guide was updated to the new API.
- `IswPlatformDndOps *dnd` added to `IswPlatformOps`, reached via thin
  `_IswPlatformDnd*` wrappers (Phase-2/6 convention).
- Verified live: the demo shell advertises `XdndAware` — the drop-target path
  runs through the new vtable (`IswDndEnable → _IswPlatformDndEnable →
  ops->dnd->enable → xcb_dnd_enable`); demo runs without crashing. No
  `xcb_atom_t` in any `include/ISW/` header; no **direct** libX11 NEEDED.

### Phase 8 — Dependency-inject the ops table (kill the singleton)

**Why this phase exists.** Phases 0–5 reached the backend through
`_IswPlatformGetOps(void)` — a no-argument accessor returning a single,
process-global, compile-time-fixed `&isw_platform_xcb_ops`. That is a
**singleton**, and it defeats the stated goal of modular server targets: a
per-process global backend means one binary can host exactly one target, chosen
at build time, with no way to be told *which* backend at the call site. It was
introduced in Phase 2 scaffolding without being asked for and was never
surfaced as a decision. This phase removes it.

**The model: dependency injection.** The active `IswPlatformOps *` is
**established once at the injection point and passed where it is needed** —
never fetched from a global. Concretely:

- `_IswPlatformGetOps(void)` is **deleted**. No global accessor survives.
- The ops table is injected at display/connection setup (the one place a
  backend is actually selected) and **threaded down the call chain** to the
  toolkit/widget code that needs it. Every `_IswPlatform*` dispatch wrapper
  (the ~20 added in Phases 4–5 plus `_IswPlatformConnectionFd`) takes the ops
  table — or a handle that carries it — as an explicit input, and forwards it.
- Call sites obtain the ops from **what they already hold** (the `Widget` /
  `IswDisplay` already passed into the wrapper), not from process state. Because
  the backend now travels with the connection, two connections can carry two
  different backends — the actual definition of "modular server targets".
- A handful of ops have no display in hand at the call (`keysym_from_name`,
  `convert_case`): decide per-op whether to route them through an injected ops
  param too, or treat them as pure/stateless. No silent fallback to a global.

**Acceptance.**
- `grep -r _IswPlatformGetOps src/ include/` returns nothing. The singleton is
  gone, not merely hidden.
- The ops table reaches every dispatch site via parameter/handle passing, from a
  single injection point — verifiably no process-global backend state.
- Selecting a backend is data: a second (even stub) backend can be injected at
  init and exercised without recompiling the toolkit.
- Build green; demo verified (cursors, grabs, menu, clipboard, rendering) with
  the XCB ops injected exactly as the singleton used to supply them.

> NOTE (LLM accountability): this phase is remediation for an
> abstraction-defeating pattern introduced by an earlier automated pass. The
> design constraint was explicit — **pass the ops table where needed
> (dependency injection); do not reach for a global accessor.** Honour it.

Depends on Phases 0–5 (the wrappers + handles being in place). Best done before
Phase 6/7 add more dispatch sites that would otherwise also need rework.

---

# Phases 9–17 — finishing the abstraction (planned)

## The actual goal (read this before scoping any remaining phase)

**Zero `xcb_*` in core.** The branch exists to get *all X11 specifics out of the
core* — the toolkit/widget `.c` files. "Core" = every `src/*.c` that is **not** a
designated backend translation unit. The backend TUs, where X11 is allowed and
expected to live, are:

```
ISWPlatformDisplayXCB  ISWPlatformEventXCB    ISWPlatformInputXCB
ISWPlatformColorFontXCB ISWPlatformGrabCursorXCB ISWPlatformAtomPropXCB
ISWPlatformDndXCB      ISWPlatformResourceXCB
ISWRender  ISWRenderCairoXCB  ISWXcbDraw   (the render/draw backend)
```

The single objective metric is the count of `xcb_*` tokens in core `.c` files.
**It must reach 0.** Every remaining phase is defined as "drive category X of
that count to zero," and its acceptance is the census number for that category,
measured the same way each time:

```sh
# core xcb_* census — run from repo root.
# Uses `command grep` (not any grep alias/ugrep) and a case filter for the
# backend TUs, so it is reproducible regardless of shell grep wrapping.
census() {
  local f b n
  for f in src/*.c; do
    b=$(basename "$f" .c)
    case "$b" in ISWPlatform*XCB|ISWRender*|ISWXcbDraw) continue;; esac
    n=$(command grep -oE 'xcb_[a-z_]+' "$f" | wc -l)
    [ "$n" -gt 0 ] && printf '%5d  %s\n' "$n" "$b"
  done | sort -rn
  # total (computed outside the pipe so it isn't lost to a subshell):
  local total=0
  for f in src/*.c; do
    b=$(basename "$f" .c)
    case "$b" in ISWPlatform*XCB|ISWRender*|ISWXcbDraw) continue;; esac
    total=$((total + $(command grep -oE 'xcb_[a-z_]+' "$f" | wc -l)))
  done
  echo "TOTAL core xcb_* = $total"
}
# per-symbol breakdown:
for f in src/*.c; do b=$(basename "$f" .c); case "$b" in ISWPlatform*XCB|ISWRender*|ISWXcbDraw) continue;; esac; command grep -oE 'xcb_[a-z_]+' "$f"; done | sort | uniq -c | sort -rn
```

Baseline 2026-06-09: **TOTAL core `xcb_*` = 1350**.

**Why the earlier phases didn't get us here.** Phases 0–15 each accepted on "no
`xcb_*` in *public headers*" and "this category's *verbs* go through ops" — never
on the core-`.c` census. So every phase passed green while the core stayed
saturated. As of the 2026-06-09 census the core still holds **1350 `xcb_*`
references across ~60 files** (Event 149, IswTrayIcon 119, Shell 99, TextAction
71, Text 64, TMkey 60, Selection 57, IswDrawing 57, Display 48, …). That number,
not a public-header grep, is the truth. The remaining phases below are rewritten
to attack it head-on.

### Census by category (2026-06-09) — what the 1350 actually are

| Category | Count | What it is | How it leaves core |
|---|---|---|---|
| Native event structs | 342 | `xcb_generic_event_t` (84) + every `xcb_*_event_t` cast — core still **decodes native events** | neutral `IswEvent` must carry every field core reads; then the casts delete. The `IswEventNative` bridge is retired. **(Phase 11)** |
| `xcb_connection_t` | 189 | core functions hold the display **as a connection** | retype to `IswDisplay`; the few real connection uses (`DefaultRootWindow`→`xcb_get_setup`) go behind a setup/root op. Productions vanish as a side effect, not by casting. **(Phase 10)** |
| 1:1 value types | 318 | `xcb_atom_t`(114) `window_t`(58) `keysym_t`(51) `pixmap_t`(31) `keycode_t`(25) `cursor_t`(21) `timestamp_t`(16) `colormap_t`(3) — all have `Isw*`/`Atom` equivalents | mechanical type-name swap; cheap, but only after the call using them is itself off raw xcb |
| Raw draw/window calls | 168 | `xcb_flush`(60) `clear_area`(19) `bell`(19) `change_window_attributes`(13) `configure_window`(13) `copy_area`(6) `create/destroy/map/unmap_window` `generate_id` `*_pixmap` `put_image` | **the window ops already exist** (create/destroy/map/unmap/reparent/configure/change_attributes/clear_area/alloc_id) — core is **bypassing** them. Route through existing ops; add a `bell` op and a setup/root op; pixmap/copy_area/put_image go through the render/draw backend. **(Phase 13c — drawing/window routing)** |
| `xcb_screen_t`+iter | 65 | `screen_t`(54) `screen_iterator_t`(5) `screen_next`(5) | `IswScreen` + a screen-enumeration/root op |
| Geometry value structs | 52 | `xcb_point_t`(23) `rectangle_t`(19) `size_hints_t`(10) | neutral POD structs (`IswPoint`/`IswRect`/size-hint fields already on the hint op) |
| setup/root access | 21 | `xcb_get_setup`(9) `setup_roots_iterator`(7) `screen_next`(5) | one setup/screen-root op (kills the `DefaultRootWindow`/`xcb_get_setup` idiom everywhere) |
| keysym machinery | 18 | `xcb_key_symbols_*` in TMkey.c/FocusMgr.c | relocate into the input backend behind input ops **(Phase 12)** |
| visual types | 12 | `xcb_visualtype_t`/`visualid_t` | `IswVisual`/`IswVisualId` (mostly exist) |

The categories are not independent passes to "complete" in isolation — a single
core file (e.g. `Event.c`) usually mixes event-decode, raw calls, and value
types, and is only *done* when its census line hits 0. Phases below are ordered
so the enabling ops/types land first, then files are swept to zero.

**Conventions (unchanged).** Opaque `Isw*` handles; semantic ops sub-vtables in
the backend reached through thin `_IswPlatform*` wrappers; ops recovered from the
injected per-display table (Phase 8); no `xcb_*` in public `include/ISW/`
headers. Each phase ends **build green + demo verified**; a phase touching the
public API says so (ABI/API break). **Additionally, each phase now ends by
re-running the census and recording the before/after core count** — a phase that
does not reduce the number did not accomplish its purpose.

### Phase 9 — Route connection setup through the vtable (`open`/`close`)

**Why.** The vtable already has `display->open` (`xcb_disp_open`) and
`display->close` (`xcb_disp_close`), but they are **dead code**: `IswOpenDisplay`
calls `xcb_connect()` inline (`Display.c:324`, plus inline
`xcb_setup_roots_length` / `xcb_get_setup` / `xcb_connection_has_error`) and
`CloseDisplay` calls `xcb_disconnect()` inline (`Display.c:825`). The one seam
where a backend is *selected* is the one seam the entry points skip. Phase 8
even injects `pd->ops` *inside* `InitPerDisplay` — downstream of the hardcoded
`xcb_connect`. This is the prerequisite for every later phase.

**Scope.**
- Choose the ops table as the **first act of init**, before any connection
  exists (a backend-selection step: env var / build default → `IswPlatformOps *`).
- `IswOpenDisplay` / `_IswAppInit` call `ops->display->open`; `CloseDisplay`
  calls `ops->display->close`. Remove the inline `xcb_connect`/`xcb_disconnect`
  and the inline `xcb_setup_*` screen probing (move the screen-count/default
  validation behind `display->screen_count` / `display->screen`).
- `InitPerDisplay` receives the already-selected ops and the opaque display,
  not a raw `xcb_connection_t *` (signature change; full retype is Phase 10).

**Acceptance.** `grep -n 'xcb_connect\|xcb_disconnect' src/Display.c` → empty;
`ops->display->open`/`->close` are actually invoked; demo opens/closes its
display through the vtable; build green + demo verified.

**Depends on** Phase 8 (ops injection point exists).

### Phase 10 — Break the `IswDisplay == xcb_connection_t*` identity

**Why.** `IswDisplay` is documented as "the `xcb_connection_t*` reinterpreted"
(`ISWPlatformDisplayXCB.c:18-20`), so `_IswXcbConn` is a bare cast
(`:54-58`) relied on in 59 files. While the handle's *identity* is the XCB
connection, no other backend can supply a display object and every holder of an
`IswDisplay` may cast it back to xcb.

**Audit finding (surfaced before edits, refined 2026-06-09).** The identity
touches ~370 interlocked sites: ~86 that *produce* an `IswDisplay` by casting a
connection (`(IswDisplay) dpy`), ~283 `_IswXcbConn` consumers across 56 files,
plus the per-display table key, `core.display`, `hooks.display`, the WWtable,
`DPY_TO_APPCON`, and `app->list`. **Key correction:** the consumers continuing to
call `_IswXcbConn` is *fine* — that is the backend seam, and a core function may
legitimately resolve the native connection through it. What actually blocks the
representation flip is narrower: the ~86 productions exist only because those
functions are *typed* on `xcb_connection_t *` and cast at each neutral-API call.
The original "no `_IswXcbConn` callers outside the backend" acceptance was the
wrong target (it conflated the seam with the leak); the right target is the
**`xcb_connection_t` census category → 0**, which removes the productions, after
which the handle-representation flip is small. Split accordingly:

#### Phase 10a — introduce the native seam (no representation change)
- Add `xcb_connection_t *native` to the per-display record
  (`IswPerDisplayStruct`); set it at `InitPerDisplay` to the connection that
  opened the display.
- `_IswXcbConn` becomes a **field lookup** (`_IswGetPerDisplay(dpy)->native`)
  instead of a bare `(xcb_connection_t *) dpy` cast — **the handle's value is
  unchanged** (still equals the connection), so all ~370 sites keep working
  untouched; only the *mechanism* of the seam changes, proving the indirection.
- Acceptance: `_IswXcbConn` no longer casts; demo builds + runs + takes events
  without crashing; handle value still equals the connection (transitional).

#### Phase 10b — retype the display off the connection (kills the 189 `xcb_connection_t` + the productions)

**Corrected understanding (2026-06-09).** The earlier framing (split into a
"handle representation flip" gated on shrinking `_IswXcbConn`'s consumers, then
deferred behind 11b/12b/13b) was wrong. The real content of 10b is the
**`xcb_connection_t` census category (189)**: ~40 core functions hold their
display *as a connection* and cast `(IswDisplay) dpy` at every neutral-API call
(86 such productions). They make almost **no raw `xcb_*(dpy,…)` calls** — e.g.
`Selection.c` (36 productions) makes zero; its only real connection use is
`DefaultRootWindow(dpy)` → `xcb_get_setup`. So:

- Retype `xcb_connection_t *dpy` → `IswDisplay dpy` across those core
  functions/params/locals **and the connection-typed fields in the I-headers**
  (`SelectionI.h`, `TranslateI.h`, `ContextI.h`, `IswPerDisplayStruct`'s nested
  state). The `(IswDisplay)` casts then delete themselves — productions vanish as
  a *side effect* of the field/param being the right type, not by cast-renaming.
- The few genuine raw-connection idioms (`DefaultRootWindow`/`xcb_get_setup`/
  `setup_roots_iterator`, census "setup/root access" = 21) go behind **one new
  setup/screen-root op** so no core file calls `xcb_get_setup` either.
- This does **not** require 11b/12b first: a function can hold `IswDisplay` while
  its body still decodes a native event or touches keysyms — those are different
  census categories (event structs / keysym machinery), on a different axis.
- Once no core file is typed on the connection, finish the representation flip:
  make `IswDisplay` the per-display record pointer (`{ ops, native, state }`);
  retype `InitPerDisplay`/`NewPerDisplay`/the table key/`core.display`;
  `_IswXcbConn` reads `native` from the record the handle now *is*. With the
  productions already gone, this last step is small and safe.

**Progress (2026-06-09): census `xcb_connection_t` 189 → 101, build green, demo
smoke-verified.** Done so far, by connected component:
- **Realize chain**: `IswRealizeProc` + `IswCreateWindow` retyped to `IswDisplay`
  (IntrinsicP.h); all ~18 widget/Shell/Vendor/Core `Realize` procs + the
  `RealizeWidget` dispatch in Intrinsic.c. Raw window creation inside
  `IswCreateWindow`/`CoreRealize`/`IswTipRealize`/Text-GC reaches the connection
  via a `_IswXcbConn` seam local (those raw calls are Phase 13c).
- **Selection component**: `SelectionI.h` `dpy` fields → `IswDisplay`; all
  Selection.c internal fns/locals retyped; `DefaultRootWindow(dpy)` →
  `_IswXcbWindow(_IswDefaultRootWindow(dpy))`; `MAX_SELECTION_INCR` + the lone
  `xcb_send_event` seam-localised (13c). **Selection.c connection refs → 0.**
- **Atoms**: deleted dead orphan `IswAtoms.c`; `ISWAtoms.c` legacy wrappers
  retyped (refs → comments only).
- **Event-queue / mapping**: `IswEventQueue.display` + `_IswRefreshMapping`
  (EventI.h) → `IswDisplay`; NextEvent.c/Event.c callers fixed.
- **Resource/print/create consumers**: TMprint.c (all print fns), `_IswPrintEventSeq`
  (TranslateI.h), `_IswAppCreateShell`/`_IswCreateHookObj` (CreateI.h) + Hooks.c,
  `_IswAppInit` now returns `IswDisplay`, VarCreate/Initialize entry points.
- **New neutral helpers** (`_IswDefaultScreenOf` / `_IswDefaultRootWindow`, over
  the existing display screen/root ops) replace `_IswGetDefaultScreen(conn)` /
  `DefaultRootWindow` in the consumer paths.

**Remaining 101, partitioned (verified):**
- **~36 are Phase 13c, not 10b**: `_IswXcbConn(...)` *seam locals that feed raw
  draw/clear/copy/gc calls* (Event, TextAction, Text, Tree, Tip, Viewport,
  Panner, SimpleMenu, Geometry, Resources, TextSink). Retyping them now only
  moves `_IswXcbConn` up a line; they leave when their raw call becomes an op.
- **~14 are Phase 12b**: TMkey.c (10) + FocusMgr.c (4) — the connection feeds
  `xcb_key_symbols_*` / modifier-mapping.
- **~32 are the representation-flip core**: Display.c (20) + Initialize.c (12) —
  the per-display table key (`_PerDisplayTable.dpy`), `AddToAppContext`/
  `NewPerDisplay`/`InitPerDisplay`/`app->list`, and the `xcb_setup_roots_iterator`
  screen-enum loops. Best done as the focused final flip (make `IswDisplay` the
  record pointer; point `_IswXcbConn` at `native`), not piecemeal.
- A few true stragglers: Converters `_IswLoadThemedCursor` param, IswDrawing
  `GetDisplayFromScreen`, Vendor's `IswRDisplay` resource-size, and 3
  comment-only mentions in ISWAtoms.

**Acceptance.** Census `xcb_connection_t` in core → **0** (achieved jointly with
12b for the keysym share and 13c for the seam-local share); setup/root access in
core → **0** (behind the op); no `(IswDisplay) conn` production outside the
backend; `struct _IswDisplay` is a real owned object; build green + demo
verified. Record before/after core census.

**Depends on** Phase 9. The keysym share rides with 12b and the draw-seam share
with 13c; the representation flip is 10b's own final step.

### Phase 11 — Abstract the event loop (`IswPlatformEventOps`)

**Why.** The biggest single coupling. `NextEvent.c` polls raw xcb
(`xcb_poll_for_event`, `:570`), dispatch consumes `xcb_generic_event_t`
throughout `Event.c`, and the vtable's `.event` slot is literally `NULL`
(`ISWPlatformDisplayXCB.c:411`). Phase 1's `IswEvent` is only a *translation
bridge* with a `native` xcb pointer, not a replacement for the loop.

**Census target.** This phase owns the **native event-struct category = 342** (
`xcb_generic_event_t` 84 + every `xcb_*_event_t` cast) — the largest single
category, spread across `Event.c`, `NextEvent.c`, `EventUtil.c`, `TMstate.c`,
`Keyboard.c`, `Selection.c`, `Shell.c`, `PassivGrab.c`, `FocusMgr.c`, and ~10
widget files that still reach native fields through the `IswEventNative` bridge.

**Scope.**
- `IswPlatformEventOps`: `wait` (blocking), `poll`/`poll_queued` (non-blocking,
  done in 11a), `translate` (native → `IswEvent`), `flush`. The loop bodies in
  `NextEvent.c` move onto the ops; the toolkit enqueues/dispatches `IswEvent`.
- Make `IswEvent` carry **every field core code currently reads off a native
  struct** (the reason the bridge persists) — audit the 342 sites for any field
  the neutral union lacks and add it. Then delete the `xcb_*_event_t` casts in
  core and **retire `IswEventNative`** for widget/toolkit code (it survives only
  inside backend protocol handlers, which are being moved out by 13/14 anyway).
- X11 protocol *sends* that aren't event-decode (`xcb_send_event` ICCCM
  client-messages in `Shell.c`) are not this category — they belong to the
  draw/window-call routing (Phase 13c) via a messaging op, and are tracked there,
  not hidden here.

**Acceptance.** Census native-event-struct category in core → **0**;
`IswEventNative` no longer called by core; `.event` op drives the loop; build
green + demo verified (keyboard, mouse, expose, focus). Record before/after core
census.

**Depends on** Phase 10 (events carry the opaque display).

### Phase 12 — Neutralize the per-display state object

**Why.** The per-display record (`InitialI.h:323`) — the toolkit's core state,
the thing Phase 8 injected ops into — still stores raw xcb resources:
`xcb_key_symbols_t* keysyms` (`:332`), `xcb_xfixes_region_t region/null_region`
(`:326-327`, addressed structurally in Phase 14), `xcb_xrm_database_t*`
(`:358-360`, addressed in Phase 13), and the keysym/modifier tables.

**Audit finding (surfaced before edits).** Most of the struct's raw-xcb fields
are owned by other phases: `region`/`null_region` → 14, `per_screen_db`/`cmd_db`/
`server_db` → 15, `native` → the 10b seam, `last_event` (`xcb_generic_event_t`)
→ Phase 11 (event-struct category). What is *uniquely* Phase 12 is the keysym/modifier state
— and that is not a field rename: the keysym-table machinery (`xcb_key_symbols_t
keysyms`, `xcb_get_modifier_mapping`, `xcb_key_symbols_get_keysym`) lives in
toolkit `TMkey.c` across ~48 raw-xcb sites (`_IswBuildKeysymTables`,
`IswTranslateKey`, `_IswComputeLateBindings`, `_IswXcbKeysyms`). Neutralizing it
means relocating that keyboard-mapping logic into the input backend — a
Phase-7-DnD-scale move, not a typedef swap. The phase is therefore split:

#### Phase 12a — trivial field retypes (no behaviour change)
- `xcb_keysym_t modKeysyms`/`lock_meaning` (both `uint32`) → `IswKeySym`.
- `_IswConnectionOfScreen` and screen→connection lookups return `IswDisplay`
  rather than `xcb_connection_t *`.
- Acceptance: those fields/signatures carry neutral types; build green + demo
  verified. (`xcb_key_symbols_t* keysyms` stays — it's 12b.)

#### Phase 12b — relocate the keysym/modifier machinery into the input backend
Owns the **keysym-machinery census category = 18** (`xcb_key_symbols_t`,
`xcb_key_symbols_alloc/free/get_keysym`, `xcb_get_modifier_mapping`) plus the
`xcb_keysym_t`/`xcb_keycode_t` value types riding with it (census 51 + 25, which
neutralize to `IswKeySym`/`IswKeyCode` once the calls move).
- Move `_IswBuildKeysymTables` / `_IswXcbKeysyms` / the raw `xcb_key_symbols_*` +
  modifier-mapping calls out of `TMkey.c` into `ISWPlatformInputXCB.c`, behind
  input ops (the table becomes an opaque `IswKeysymTable` handle or stays
  backend-internal state reached only via ops). Route `FocusMgr.c`'s independent
  `xcb_key_symbols` cache through the same ops.
- Acceptance: census keysym-machinery category in core → **0** (no
  `xcb_key_symbols_*` / `xcb_get_modifier_mapping` outside the input backend);
  `pd->keysyms` opaque to the toolkit; `TMkey.c`/`FocusMgr.c`/`Keyboard.c` census
  lines for keysym/keycode types → 0; keyboard input + translations verified
  live. Record before/after core census.

**Acceptance (phase overall).** `IswPerDisplayStruct` holds no `xcb_*` resource
type (the remaining ones are owned by Phases 13/14/15 and the 10b `native` seam,
all of which also end at census 0 for their category); build green + demo
verified.

**Depends on** Phase 11 (12a). 12b is unfinished Phase-3 input work.

### Phase 13 — Abstract selection / property exchange and the tray

**Why.** The largest *unphased* protocol surfaces. Phase 5 cut selection at the
verb level but left the property exchange XCB; `Selection.c` still has 148 raw
`xcb_*` references (INCR transfers, property round-trips), and `IswTrayIcon.c`
(99) implements XEMBED directly.

**Scope.**
- Extend the Phase-6 property/atom ops (or add a selection-transfer op set) to
  cover the INCR / property-exchange state machine in `Selection.c`; the toolkit
  selection code drives it through ops.
- Move the XEMBED tray protocol behind a tray op set (or document it as an
  X11-only optional module that a non-X backend omits — a deliberate decision to
  surface, not a silent gap).

**Audit finding (surfaced before edits).** The hard part is already done:
`Selection.c`'s INCR / property-exchange state machine runs on the Phase-5/6
selection + property ops (`_IswPlatformChangeProperty`/`GetProperty`/selection
verbs). Of its raw-xcb refs, most are *type mentions* (`xcb_atom_t`,
`xcb_timestamp_t`, `xcb_window_t`, `xcb_connection_t` locals/params) and the rest
are: 3 `xcb_flush`, 2 `xcb_change_window_attributes`, 1 `xcb_send_event`, and
event-struct casts (`xcb_selection_request_event_t`, `property_notify`, …). These
split across census categories owned by other phases: the event-struct casts →
**Phase 11** (event-struct category), the `xcb_connection_t` locals → **Phase
10b**, `xcb_send_event` → **Phase 13c** (messaging op). So `Selection.c`'s core
census reaches 0 only across 13a + 11 + 10b + 13c — not 13a alone.

#### Phase 13a — neutralize the selection residuals
- Route the residual direct calls through ops: 3 `xcb_flush` →
  `_IswPlatformFlush`; 2 `xcb_change_window_attributes` → a new
  `_IswPlatformChangeAttributes` wrapper over the existing `change_attributes`
  window op (event-mask only).
- Neutralize the trivially-1:1 types (`xcb_atom_t` → `Atom`, `xcb_timestamp_t` →
  `IswTime`; identical underlying types, zero behaviour change).
- Deferred to their owning phases (not 13a): every `xcb_*_event_t` cast → Phase
  11; `xcb_send_event` (builds a native selection-notify event) → Phase 13c
  (messaging op); the `xcb_connection_t` local-plumbing retype → Phase 10b.
- Acceptance: no residual `xcb_flush`/`xcb_change_window_attributes` and no
  `xcb_atom_t`/`xcb_timestamp_t` in `Selection.c`; clipboard copy/paste verified
  live; build green. (Done.)

#### Phase 13b — the tray (`IswTrayIcon.c`)
- Self-contained XEMBED widget — census **119** (the single biggest core file
  after `Event.c`): client messages, reparent, visual selection, expose. Decide:
  abstract behind a tray op set, OR declare it an explicitly X11-only optional
  module a non-X backend omits (the plan's offered shortcut). **If declared
  X11-optional, the file is reclassified as a backend TU** and its 119 leave the
  *core* census legitimately (it is no longer core) — that decision must be
  recorded, not used as a silent census dodge. A separate decision, not blocking
  13a.

#### Phase 13c — route core's raw drawing/window calls through the ops

**Census target.** The **raw draw/window-call category = 168** —
`xcb_flush`(60), `xcb_clear_area`(19), `xcb_bell`(19),
`xcb_change_window_attributes`(13), `xcb_configure_window`(13),
`xcb_copy_area`(6), `xcb_create/destroy/map/unmap_window`,
`xcb_generate_id`(10), `xcb_*_pixmap`, `xcb_put_image` — scattered across core
widget/toolkit files that call raw xcb **even though the window ops already
exist** (`create`/`destroy`/`map`/`unmap`/`reparent`/`configure`/
`change_attributes`/`clear_area`/`alloc_id` are all in `ISWPlatform.h`). Plus the
setup/root and screen-enum access (census 21 + 65) and the geometry value structs
(`xcb_point_t`/`rectangle_t`, census 42).

- Route every core raw window/draw call through the **existing** window ops via
  `_IswPlatform*` wrappers (most need only a wrapper, the op exists). `xcb_flush`
  → `_IswPlatformFlush` (exists). 
- Add the **few genuinely missing ops**: a `bell` op; a `copy_area`/`put_image`/
  pixmap path through the **render/draw backend** (`ISWRender*`/`ISWXcbDraw` are
  backend TUs, so core calls them through `ISWRender*`/`_IswPlatform*`, never raw
  xcb); the setup/screen-root op shared with Phase 10b (`DefaultRootWindow`/
  `xcb_get_setup`/`setup_roots_iterator`/`screen_next`).
- Neutralize the geometry value structs to toolkit PODs (`IswPoint`/`IswRect`)
  or pass primitive fields.
- `xcb_send_event` ICCCM client-message *sends* in `Shell.c` go behind a
  messaging op here (this is where Phase 11 said they belong).

**Acceptance.** Census draw/window-call category, setup/root, screen-enum, and
geometry-struct categories in core → **0**; build green + demo verified (repaint,
resize, map/unmap, bell). Record before/after core census.

**Acceptance (phase overall).** `Selection.c` core census → 0 (13a + Phase 11's
event-struct removal + 10b's connection retype + 13c's `xcb_send_event`); tray
abstracted or explicitly reclassified backend-optional (13b); core raw
draw/window calls → 0 (13c); clipboard copy/paste + repaint verified live; build
green.

**Depends on** Phase 11 (13a, event-struct removal). 13c shares the setup/root op
with Phase 10b.

### Phase 14 — Replace XFixes damage regions with a client-side region type — **DONE**

**Why (as written).** Damage/redraw accounting was assumed to be built on the
XFixes *server* extension: `IswAddExposureToRegion` round-tripping
`xcb_xfixes_create_region` + `union_region` per expose, `pd->region`/
`null_region` as XFixes scratch regions, and — worst — the **`IswExpose` class
method typed `xcb_xfixes_region_t`** (`IntrinsicP.h:136`), so every widget's
expose contract named an X extension. A second `_IswRegion` existed client-side
(`ISWP.h:40`), defined twice (`ISWXcbDraw.c:851`, `IswXcbDraw.c:974`).

**Audit finding (before implementing).** The XFixes damage machinery was
**entirely vestigial**, not live:
- Every `core_class.expose` call site passed region `0`/`NULL` (Event.c, Geometry.c,
  ISWRender.c, Text.c) — widgets never received a real region.
- `IswAddExposureToRegion` and `get_region_bounding_box` (the only code making
  real `xcb_xfixes_*` server calls) had **zero live callers** — invoked only
  from commented-out lines (`Event.c:1710-1712`).
- `pd->region`/`null_region` were created in `Display.c` and **never read** by
  any live path, then destroyed at close.
- The "defined twice" `_IswRegion` was a dead, **uncompiled** orphan file
  (`IswXcbDraw.c`/`.h`, lowercase — only the uppercase pair is in CMake); the
  live client-side rectangle-set region (`struct _IswRegion` / `Region` /
  `ISWRegionPtr`) already existed and worked (Command.c, Text.c, Layout.c).

So the real work was a **dead-code removal + type swap**, not building a region
engine: the portable region type already existed; the X extension was doing no
real work.

**What was done (full phase, one pass — option "14 full").**
- Added the neutral handle `typedef struct _IswRegion *IswRegion;` in
  `IswTypes.h` (the value-handles home, "carry no xcb dependency"). Retyped the
  `IswExposeProc` contract (`IntrinsicP.h:133`) off `xcb_xfixes_region_t` → `IswRegion`.
- Renamed `xcb_xfixes_region_t` → `IswRegion` across all ~32 widget Redisplay
  signatures + the casts in `SmeBSB.c`/`Text.c` (value was always 0/NULL, so
  byte-identical behavior).
- Deleted the dead `IswAddExposureToRegion` + `get_region_bounding_box` (and
  their orphan `MAX`/`MIN` macros) from `Event.c`; deleted their public decls
  from `Intrinsic.h`; removed `pd->region`/`null_region` from `InitialI.h` and
  their create/destroy in `Display.c`.
- De-duplicated `_IswRegion` to one TU: dropped the value-typedef in
  `ISWXcbDraw.c` (struct tag only, so it no longer collides with the `IswRegion`
  handle) and **git-rm'd the orphan `IswXcbDraw.c`/`.h`**.
- Stripped now-unused `<xcb/xfixes.h>` from all 6 files; removed `xcb-xfixes`
  from the library's CMake deps (pkg-config + BSD fallback). The demo's
  `DEMO_DEPS` keeps it (the demo executable, not the library).

**Acceptance — met.** `IswExpose` no longer names `xcb_xfixes_region_t` (zero
in `IntrinsicP.h`); one `struct _IswRegion` definition; **zero `xcb_xfixes`
anywhere in `src`/`include`**; build green; **`xcb-xfixes` no longer a linked
dependency of `libISW.so`** (`ldd` clean) — the strongest proof the extension
was confined out, not merely renamed. Demo verified live: survived a resize
cycle, unmap/map full-window damage, and scroll-wheel expose with no crash and
no new stderr.

**Depends on** Phase 11 (expose flows through the abstracted event path).

### Phase 15 — Resource resolution behind ops (Xrm demoted to an X11-backend detail) — **DONE**

**Outcome (what shipped).** A coarse resource-resolution ops seam now sits
between the toolkit and Xrm. `IswDatabaseHandle` / `XrmDatabase` are the neutral
opaque `struct _IswResourceDb *` (IswDatabase.h, no xcb include); a new
`IswPlatformResourceOps` sub-vtable (from_string / from_file /
from_resource_manager / combine / put_resource / put_resource_line / to_string /
free / get_string) is wired into `isw_platform_xcb_ops.resource`. The XCB
backend's implementation (`ISWPlatformResourceXCB.c`, the only TU including
`<xcb/xcb_xrm.h>`) casts the handle to `xcb_xrm_database_t*` and delegates to
libxcb-util-xrm. All ~83 toolkit call sites across Initialize.c, Resources.c,
ResConfig.c, Intrinsic.c, Error.c, Display.c, Functions.c now call the
`_IswPlatformResource*` wrappers; per-display DB fields (`server_db`, `cmd_db`,
`per_screen_db`, `errorDB`) and `_IswPreparseCommandLine` / `_IswRefetchResources`
retyped off `xcb_xrm_database_t`. The portable quark interning (Quark.c) was left
untouched (not X11-specific). Resource ops use `_IswPlatformSelectBackend()`, not
the per-display record — the database is a free-standing store, and several calls
run before any per-display record exists (same rationale as
`_IswPlatformConnectionFd`).

**Proof (Xrm demoted, not renamed).** `nm` over every toolkit `.o`: exactly one
object — `ISWPlatformResourceXCB.c.o` — references `xcb_xrm_*` symbols (all 9);
zero elsewhere. No `xcb_xrm_database_t` or `<xcb/xcb_xrm.h>` in toolkit code or
public headers (comment mentions only). Live: demo ran with
`-xrm '*background: #112233'` (exercises the command-line parser →
put_resource_line/put_resource/from_string/combine path) and survived full
interaction with zero new stderr; an instrumented `get_string` showed **846 real
resolution hits** (e.g. resolved value `#D8D8E8`) flowing through the neutral op,
confirming the seam is the live resolution path, not a bypass. Instrumentation
reverted; build green.

**Why.** The resource subsystem — how every widget gets every value — is X11's
Resource Manager, and the toolkit is bound to it in both *type* and *engine*.
`IswDatabaseHandle` / `XrmDatabase` are `typedef xcb_xrm_database_t *`
(`IswDatabase.h:39,46`), and resolution is delegated wholesale to
**libxcb-util-xrm**: the database store, the `.Xdefaults`/string parser
(`xcb_xrm_database_from_string`/`_from_file`), merge/override
(`xcb_xrm_database_combine`), and — the genuine engine — the tight/loose
name-class wildcard precedence matcher (`xcb_xrm_resource_get_string`).
`Resources.c:1451` (`XrmQGetResource`) is explicitly "a compatibility wrapper
around `xcb_xrm_resource_get_string`". 10 distinct `xcb_xrm_*` functions across
~83 call sites in 8 files (`Initialize.c`, `Resources.c`, `ResConfig.c`,
`Intrinsic.c`, `Error.c`, `Display.c`, `Convert.c`/`Converters.c`).

**Reframing (corrected from the original plan).** Xrm is *X11's particular
answer* to a general question — "what is the configured value of this resource
for this widget?" — drawn from some source (on X11: `RESOURCE_MANAGER` /
`.Xdefaults` / `XENVIRONMENT`, matched by Xrm's precedence rules). The
platform-independence boundary therefore belongs at that **general question**,
not at Xrm's mechanics. The original scope ("keep the matching logic, swap the
type") was wrong: the matching logic is *not* the toolkit's to keep — it lives
inside libxcb-util-xrm, i.e. it *is* X11's approach. Reimplementing it in-repo
would just be rebuilding X11's resource manager inside the toolkit, which is
pointless. Instead, **confine Xrm to the XCB backend behind a resource-ops
seam**, exactly as XFixes (14), selection/property (5/6), and event-poll (11)
were confined.

**Scope.**
- Define a **coarse resource-resolution ops interface** (the abstracted
  *question*, not Xrm's mechanism): an opaque `IswResourceDb` handle the toolkit
  never inspects, plus ops to (a) build/load the platform resource source
  (X11: `RESOURCE_MANAGER` + `.Xdefaults`/`XENVIRONMENT`; another backend:
  config file / app-supplied / nothing), (b) merge an app- or command-line-
  supplied source into it, and (c) resolve a full name/class path to a string
  value or "none". The interface abstracts *resolve a resource*, never
  `combine`/`put`/`get_string` as such, so a non-X backend answers lookups
  without mimicking Xrm's operational model.
- Retype `IswDatabaseHandle` / `XrmDatabase` off `xcb_xrm_database_t *` to the
  neutral opaque handle. Route all ~83 `xcb_xrm_*` call sites through the ops
  (or through the thin toolkit wrappers over them).
- The **XCB backend's** resource-ops implementation keeps using libxcb-util-xrm
  internally — Xrm becomes a private detail of one translation unit, the only
  place `xcb_xrm_*` / `<xcb/xcb_xrm.h>` appears, and the only thing that pulls
  the `xcb-xrm` link. The portable quark interning (`Quark.c`) stays in the
  toolkit unchanged (it is not X11-specific).
- Keep the `Xrm*` *names* per CLAUDE.md, but back them with the neutral handle.

**Acceptance.** No `xcb_xrm_database_t` (and no `<xcb/xcb_xrm.h>`) in toolkit
code or public headers — confined to the XCB backend TU; toolkit resource code
names no X11 resource concept and reaches resolution only through the ops seam;
resource resolution and command-line option parsing verified live; build green +
demo verified. (Goal proven the way 14 was: `xcb-xrm` no longer a link
dependency of toolkit objects outside the backend — Xrm is demoted, not
renamed.) One phase.

**Depends on** Phase 10 (DB lookups keyed off the opaque display).

### Phase 16 — Purge `xcb_*` from the public API

**Why.** The abstraction never reached the API boundary: `xcb_*` appears in 8
public headers, and `Intrinsic.h` types ~10 entry points on
`xcb_generic_event_t *` (`IswLastEventProcessed`, the next-event / dispatch /
peek / handler signatures). An application must include and name XCB types.

**Scope.**
- Flip the remaining public signatures to neutral types: `xcb_generic_event_t *`
  → `IswEvent *` (Phase 11 makes this real), the `Xrm*`/`xcb_xrm_database_t`
  surface (Phase 15), the `xcb_xfixes_region_t` expose contract (Phase 14), and
  any residual `xcb_*` in `IswEvent.h` / `ISWRender.h` / `IswTrayIcon.h` /
  `IswTypes.h` / `TextSink.h`.
- This is an explicit **ABI/API break**; update the app guide and the X11
  compat shim headers (`include/X11/`).

**Acceptance.** `grep -rl 'xcb_' include/ISW/*.h | grep -vE 'I\.h$|P\.h$'` →
empty (the *header* half of the goal — note this is necessary but **not
sufficient**; the core `.c` census is the real gate, owned by 10–13); the demo
and downstream guide compile against the neutral API; build green.

**Depends on** Phases 11, 14, 15 (the neutral types those phases introduce).

### Phase 17 — Prove it: a second (stub/null) backend — the final census gate

**Why.** The abstraction is unproven until a non-XCB backend can be selected and
the toolkit links without pulling `xcb_*` from non-backend TUs. This phase is
the falsification test for all prior work, and the **executable form of the core
census**: a non-backend TU that still names `xcb_*` shows up as an undefined
`xcb_*` symbol when linking against the stub instead of the XCB backend.

**Scope.**
- Add a stub/null `IswPlatformOps` (every sub-vtable filled with minimal/no-op
  implementations) selectable at init via the Phase-9 backend-selection step.
- Build a variant that links the toolkit against the stub backend only; any
  `xcb_*` symbol pulled from a non-backend TU is a residual leak — i.e. a nonzero
  core census line that 10–16 missed — to fix.
- Run the demo (or a headless smoke harness) on the stub backend far enough to
  exercise init, resource resolution, event dispatch, and expose.

**Acceptance.** Core `xcb_*` census = **0** (the whole-branch goal); toolkit
links with the stub backend and **zero `xcb_*` symbols from non-backend
translation units** (verified via `nm`/link — the census made executable); a
non-XCB backend is selectable at init without recompiling the toolkit; the XCB
backend remains the default and demo-verified.

**Depends on** Phases 9–16 (every category driven to 0).

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
- Phase 6 done (build green, demo verified): neutral `Atom` (already a typedef)
  flipped across 12 public headers + `IswPlatformAtomOps`/`IswPlatformPropertyOps`/
  `IswPlatformHintOps` (`ISWPlatformAtomPropXCB.c`).  Generic property verbs
  (intern/get_name/change/get→`IswProperty`/delete) + semantic ICCCM/EWMH hints
  (title/icon/class/protocols/transient/window-type/pid/normal-hints).  Routed
  ISWAtoms/Shell/Selection/ResConfig/Tip/SimpleMenu/Intrinsic/TMprint/TMstate/
  TextAction/Display/Converters through the dispatch wrappers.  Selection.c's
  Phase-5-deferred transfer machinery now off raw xcb (fixed a latent UAF +
  allocator mismatches).  `xcb_send_event`/`xcb_create_window`/
  `xcb_icccm_set_wm_hints` stay on the seam by scope; XDND (Phase 7) + tray keep
  raw use.  WM title/PID/protocols/WM_DELETE verified live.  No `xcb_atom_t` in
  public headers.  No direct libX11 NEEDED entry.
- Phase 7 done (build green, demo verified): XDND recognised as an X11 wire
  protocol and moved whole into the platform backend.  `src/ISWXdnd.c` →
  `src/ISWPlatformDndXCB.c` behind a new `IswPlatformDndOps` vtable
  (`isw_platform_xcb_dnd_ops`, reached via thin `_IswPlatformDnd*` wrappers).
  New transport-neutral public service `include/ISW/IswDragDrop.h` (`IswDnd*`
  names, neutral `Atom` MIME types, new `IswPixmap` icon handle, `IswEvent *`
  trigger).  Earlier draft's service/backend bookkeeping split abandoned as
  unjustified mid-protocol surgery — the engine moves intact (see
  PHASE7_SCOPE.md).  Drop-target path verified live (demo advertises `XdndAware`
  through the new vtable).  Shim fully removed in the same phase: internal
  callers (Shell.c, IconView.c), the demo, and the app guide all use `IswDnd*`
  directly; `include/ISW/ISWXdnd.h` deleted; no `ISWXdnd*` left in any source.
  No `xcb_*`/`Xdnd*` in the public DnD header.  No direct libX11 NEEDED entry.
- Phase 8 done: ops table dependency-injected onto the per-display record
  (`pd->ops`), the process-global ops accessor removed; every `_IswPlatform*`
  wrapper recovers ops from the display/widget it is handed.
- Phase 9 done: `IswOpenDisplay`/`CloseDisplay` route through
  `display->open`/`close` via `_IswPlatformSelectBackend()`; connection setup is
  itself behind the vtable.
- Phase 10a done: `xcb_connection_t *native` on the per-display record;
  `_IswXcbConn` is a field lookup, not a cast (handle value unchanged).
- Phase 11a done: poll ops (`xcb_poll_for_event[_queued]`) +
  `_IswPlatformPollEvent`/`PollQueued`/`DisplayHasError`/`Flush`; `NextEvent.c`/
  `Shell.c` poll through them.
- Phase 12a done: `modKeysyms`/`lock_meaning` → `IswKeySym`;
  `_IswConnectionOfScreen` returns `IswDisplay`.
- Phase 13a done: selection residuals neutralized (`xcb_flush`→`_IswPlatformFlush`,
  `xcb_change_window_attributes`→`_IswPlatformChangeAttributes`,
  `xcb_atom_t`/`xcb_timestamp_t`→`Atom`/`IswTime` in `Selection.c`); clipboard
  round-trip verified live.
- Phase 14 done (build green, demo verified): XFixes damage machinery was
  vestigial (every expose passed region 0; the XFixes functions had no live
  callers); retyped `IswExpose` off `xcb_xfixes_region_t` to the neutral
  `IswRegion`, deleted the dead XFixes code + orphan draw file, **`xcb-xfixes`
  no longer a link dependency of `libISW.so`**.
- Phase 15 done (build green, demo verified): resource resolution behind a
  coarse ops seam; `IswDatabaseHandle`/`XrmDatabase` neutral opaque handle; all
  ~83 `xcb_xrm_*` sites routed through `_IswPlatformResource*`; Xrm confined to
  `ISWPlatformResourceXCB.c` (proven by `nm`: only that one object references
  `xcb_xrm_*`). 846 live resolution hits verified through the neutral op.
- **Plan rewrite 2026-06-09:** reanchored the whole 9–17 tail to the *real*
  objective — **core `xcb_*` census → 0** — after a 10b attempt exposed that
  prior phases accepted on public-header greps while the core still held **1350**
  `xcb_*` refs. Added the reproducible census command + baseline, a
  by-category census table, corrected Phase 10b (it is the `xcb_connection_t`
  category retype, not a 11b/12b-gated handle flip), re-anchored 11/12b/13's
  acceptance to census numbers, and added **Phase 13c** (route core's 168 raw
  draw/window calls through the already-existing window ops). Phase 17 is now the
  executable census gate (`nm`: zero `xcb_*` from non-backend TUs). No code
  changed in this rewrite — plan only.
