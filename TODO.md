# TODO

## Replace Xlib-compat constants with XCB native enums

XtTypes.h defines ~200 Xlib-named constants as hardcoded integers (e.g.
`#define KeyPressMask (1L<<0)`, `#define GXcopy 0x3`, `#define XA_PRIMARY 1`).
Nearly all of these have direct equivalents already defined in `xcb/xproto.h`
and `xcb/xcb_icccm.h` (`XCB_EVENT_MASK_KEY_PRESS`, `XCB_GX_COPY`,
`XCB_ATOM_PRIMARY`, etc.). The Xlib names should never have been recreated.

### Approach

1. Replace all bare-integer definitions in XtTypes.h with XCB_EQUIVALENT

### Groups (all have XCB equivalents unless noted)

- Event masks (25) → `XCB_EVENT_MASK_*`
- Event types (33) → `XCB_KEY_PRESS`, `XCB_BUTTON_PRESS`, etc.
  (`NoExpose` and `LASTEvent` have no XCB equivalent — define locally)
- CW attribute masks (15) → `XCB_CW_*`
- ConfigureWindow masks (7) → `XCB_CONFIG_WINDOW_*`
- GC functions (16) → removed (GC management deleted)
- GC value masks (23) → removed (GC management deleted)
- Line/cap/join/fill styles (14) → removed (GC management deleted)
- Fill/subwindow/arc rules (8) → removed (GC management deleted)
- Gravity (12) → `XCB_GRAVITY_*`
- Grab modes/status (7) → `XCB_GRAB_MODE_*`, `XCB_GRAB_STATUS_*`
- Prop modes (3) → `XCB_PROP_MODE_*`
- Stack modes (5) → `XCB_STACK_MODE_*`
- Window class (2) → `XCB_WINDOW_CLASS_*`
- Map state (3) → `XCB_MAP_STATE_*`
- Backing store (3) → `XCB_BACKING_STORE_*`
- Predefined atoms (59) → `XCB_ATOM_*`
- Modifier masks (8) → `XCB_MOD_MASK_*`
- Button masks (5) → `XCB_BUTTON_MASK_*`
- Button indices (5) → `XCB_BUTTON_INDEX_*`
- Notify modes/detail (12) → XCB notify enums
- Visibility/property/colormap state → `XCB_VISIBILITY_*` etc.
- Visual classes (6) → `XCB_VISUAL_CLASS_*`
- Image format (3) → `XCB_IMAGE_FORMAT_*`
- Byte order (2) → `XCB_IMAGE_ORDER_*`
- Circulation (2) → `XCB_PLACE_*`
- Mapping request (3) → `XCB_MAPPING_*`
- Focus revert (3) → `XCB_INPUT_FOCUS_*`

Also remove: `DisplayOfScreen` (NULL stub, unused), `XYBitmap`/`XYPixmap`/
`ZPixmap` (unused), `CoordModeOrigin`/`CoordModePrevious` (unused).

Files: XtTypes.h, and every src/ file that references these constants.

## Investigate animation hook architecture

Optional animation support when EGL or other accelerated backends are available.
Needs investigation before implementation to determine where hooks attach and how
widgets opt in.

### Candidate attachment points

- **ISWRender Begin/End frame cycle**: Animation state updates between
  `ISWRenderBegin()` and widget draw calls. The EGL backend would add vsync
  synchronisation via `eglSwapBuffers`. Cairo-XCB already has a double-buffered
  frame boundary in `cairo_xcb_begin()`/`cairo_xcb_end()`.
- **Timer-driven frame clock**: A single app-wide `XtAppAddTimeOut` at ~16ms
  driving all active animations. Scrollbar and Tip already use timers for
  movement/delay — this generalises the pattern.
- **SetValues intercept**: Property changes that currently trigger immediate
  redisplay could instead start animated transitions interpolating between old
  and new values.
- **Capability gating**: `ISWRenderGetCapabilities()` with
  `ISW_RENDER_CAP_HW_ACCEL` gates whether frame-driven animation is enabled or
  falls back to instant state changes on software backends.

### Open design questions

- Where does animation state live? Per-widget (CorePart field or extension
  record) vs external registry keyed by widget.
- Who owns the frame clock? Single app-wide timer ticking all active animations
  vs per-widget timers. Single clock avoids drift and redundant wakeups.
- How do widgets opt in? Class method (`animate` proc), callback list, or
  ISWRender-level API called during expose.
- Relationship to ISWRender Begin/End — hooks inside the render cycle can
  interpolate drawing parameters (opacity, position, color); hooks outside can
  only update resources and trigger redraws.

### Natural animation candidates

Tip (fade in/out), Scrollbar (smooth thumb movement), ProgressBar (indeterminate
animation), Command (hover/press feedback), menu popup transitions.

## Abstract resource system behind vtable

Decouple widget resource management from the Xrm/quark implementation so the
backend is pluggable. Motivating use case: replace X resource file format with
TOML configuration.

### Architecture

- **Resource schema**: Widget XtResource[] tables remain as declarative struct
  descriptions (name, type, size, offset, default). Already backend-agnostic in
  principle — no changes needed at the widget level.
- **Resolution vtable**: Pluggable lookup — given a widget path and resource
  name, return a value. Xrm quark-based resolution becomes one implementation,
  TOML table lookup another.
- **Converter vtable**: Pluggable type conversion — given a string and target
  type, produce a typed value. The 31 existing converters become one backend's
  converter set.
- **Storage vtable**: Pluggable database — load, merge, query, free. Replaces
  the hardcoded XrmDatabase/xcb_xrm_database_t.

### Key refactoring

The main difficulty is Resources.c _XtGetResources, which interleaves resolution
logic, quark manipulation, and converter dispatch in a single pass. These three
concerns need separating before a vtable can slot in cleanly. The quark system
(Quark.c) moves from a public dependency to an internal optimisation detail of
the Xrm backend.

### Affected files

Core: Resources.c, Converters.c, Convert.c, Initialize.c, Quark.c, Create.c,
SetValues.c, GetValues.c, GetResList.c, VarCreate.c, VarGet.c (~6,200 lines).
Public API: XtGetApplicationResources, XtSetValues, XtGetValues,
XtSetTypeConverter remain unchanged — backends implement them.

## Edge-specific borders

Replace the single uniform `border_width` / `border_pixel` with per-edge values
(top, right, bottom, left) so widgets can have asymmetric borders.

### Core problem

XCB `xcb_create_window` only supports a single uniform border width — the X
server draws it. Per-edge borders require abandoning X server borders entirely
and drawing all borders in software inside the widget window.

### Required changes

**CorePart / RectObjPart** — add four `Dimension` fields (`border_width_top`,
`_right`, `_bottom`, `_left`) and corresponding per-edge `Pixel` fields. The
existing `XtNborderWidth` / `XtNborderColor` become shorthand that sets all
four edges.
Files: CoreP.h, RectObjP.h, Core.c, RectObj.c.

**Eliminate X server borders** — all widgets must create windows with
`border_width=0` and incorporate border space into their own dimensions. This is
the largest single change. `XtCreateWindow()` in Intrinsic.c currently passes
`core.border_width` to `xcb_create_window`; this must become 0, with the
widget's width/height expanded to include the border area.
Files: Intrinsic.c, Core.c, Shell.c.

**Software border drawing** — new ISWRender function(s) for per-edge borders,
e.g. `ISWRenderStrokeBorder(ctx, widths[4], colors[4], w, h)`, or four
`ISWRenderDrawLine()` calls. Form.c and Box.c already draw borders manually via
`ISWRenderStrokeRectangle()` — these provide the pattern. Every widget's
Expose/Redisplay method needs to draw its own borders.
Files: ISWRender.h, ISWRender.c, ISWRenderXCB.c, ISWRenderCairoXCB.c,
Form.c, Box.c, and all widget Expose methods.

**Geometry management** — every `2 * border_width` calculation becomes
`border_left + border_right` (horizontal) or `border_top + border_bottom`
(vertical). The `XCB_CONFIG_WINDOW_BORDER_WIDTH` flag in geometry requests goes
away; border changes become internal resize + redraw operations.
Files: Geometry.c, Box.c, Form.c, Paned.c, Viewport.c.

**SetValues** — Core.c currently updates `XCB_CW_BORDER_PIXEL` /
`XCB_CW_BORDER_PIXMAP` on the X window. With software borders, these trigger a
redraw instead of an X attribute change. Must also handle partial edge updates
(e.g. only `border_top_width` changed).
Files: Core.c.

### Suggested order

1. Add per-edge fields to CorePart/RectObjPart with backward-compat defaults
2. Add ISWRender border drawing API
3. Convert Form and Box (already do manual drawing) as proof of concept
4. Move all widgets to `border_width=0` at the X level
5. Update geometry managers
6. Update SetValues and resource handling

## ISWPlatform vtable — abstract all X11/XCB platform dependencies

ISWRender already abstracts drawing. The resource system is being abstracted
separately (above). Everything else — display, windows, events, input, grabs,
atoms, selections, colormaps, fonts, cursors — is still hardcoded to XCB. This
needs a platform vtable so backends other than X11 can be supported (Arcan/SHMIF,
or anything Cairo can render to with a usable event system).

### Vtable structure

```
Widget code (unchanged)
       ↓
ISWPlatform vtable
  ├─ ISWPlatformDisplay    open, close, screen info, fd for event loop
  ├─ ISWPlatformWindow     create, configure, map, destroy, reparent
  ├─ ISWPlatformEvent      poll, translate to portable event union, modifier state
  ├─ ISWPlatformInput      keysym table, keyboard mapping, grabs
  ├─ ISWPlatformSelection  own, convert, paste
  ├─ ISWPlatformColor      alloc by name/RGB, free
  ├─ ISWPlatformFont       open by pattern, metrics, close
  └─ ISWPlatformCursor     create from symbol, set on window, free
       ↓
ISWRender vtable (already exists)
       ↓
Backend: XCB | Arcan/SHMIF | ...
```

### Categories to abstract

**Display/Connection** — xcb_connection_t, xcb_screen_t embedded in every
widget's core.display/core.screen. Wrap behind opaque ISWDisplay. XtTypes.h
macros (ConnectionNumber, DefaultRootWindow, etc.) route through vtable instead
of calling XCB directly.
Files: CoreP.h, Display.c, Initialize.c.

**Window lifecycle** — create, map, configure, destroy, reparent all use
xcb_window_t and xcb_create_window/xcb_configure_window etc. Largest single
surface area.
Files: Core.c, Shell.c, Geometry.c, Composite.c, Create.c, Popup.c.

**Event loop + dispatch** — xcb_generic_event_t* plus 14 specific event structs,
FD-based poll loop in NextEvent.c, type-switch dispatch in Event.c. Events flow
into every widget action proc — this is the hardest piece. Needs a portable
event union that XCB backend populates from xcb_generic_event_t and other
backends populate from their native events.
Files: Event.c (~1,200 lines), NextEvent.c, Keyboard.c, Pointer.c.

**Translation manager** — parses "Ctrl<Key>a" into XCB event types + keysyms
via xkbcommon. Needs backend-neutral keysym vocabulary and event type mapping.
Files: TMparse.c, TMstate.c, TMaction.c, TMgrab.c.

**Input (keyboard/pointer)** — xcb_keycode_t, xcb_keysym_t, modifier masks
(ShiftMask, ControlMask, Mod1-5), button constants. Abstract keysym table and
modifier set.
Files: Keyboard.c, Pointer.c, XtTypes.h.

**Grabs** — passive/active pointer+keyboard grabs. Can stub for backends without
grab support.
Files: PassivGrab.c, TMgrab.c.

**Atoms + properties** — xcb_atom_t, xcb_change_property, ICCCM/EWMH hints.
X11-specific concept; other platforms have their own metadata mechanisms.
Files: ISWAtoms.c, Shell.c, Vendor.c, SetWMCW.c.

**Selections/clipboard** — xcb_convert_selection, incremental transfer protocol.
Each platform has its own clipboard API.
Files: Selection.c (~800 lines).

**Colormap/Visual** — xcb_colormap_t, xcb_visualtype_t, color alloc/free.
True-color backends can simplify this massively to direct RGBA.
Files: Converters.c, Core.c, Display.c.

**Fonts** — xcb_font_t, XLFD queries, XtFontStruct. Cairo+fontconfig already
does the real rendering work; the XCB font path is mostly legacy metrics
queries. Abstract to open-by-pattern + metrics.
Files: AsciiSink.c, MultiSink.c, Converters.c.

**Cursors** — xcb_cursor_t, glyph cursor creation. Map to a symbolic cursor
enum (arrow, hand, crosshair, text, etc.).
Files: Tip.c, Panner.c, Simple.c.

### Suggested order of work

1. Portable event union + ISWPlatformEvent (unblocks everything else)
2. ISWPlatformDisplay + ISWPlatformWindow (core widget lifecycle)
3. ISWPlatformInput (keyboard/pointer, translation manager)
4. ISWPlatformColor + ISWPlatformFont (resource converters depend on these)
5. ISWPlatformSelection, ISWPlatformCursor, grabs (lower priority)
6. ISWPlatformDragDrop (depends on events, windows, selections)

### Drag-and-drop — complete and abstract ISWXdnd

Current state: ISWXdnd.c implements XDND v5 drop-target only, hardwired to XCB
atoms, client messages, selection transfers, and xcb_translate_coordinates. No
drag-source support exists.

**Missing functionality (complete XDND first, then abstract):**

- Drag source initiation — widget calls ISWDragStart, library handles
  XdndEnter/Position/Drop protocol toward the target window
- Drag visual feedback — cursor changes, drag icon/proxy window
- Action negotiation — XdndActionCopy is hardcoded; need Move, Link, Ask, Private
- Multiple MIME types — only text/uri-list supported; need text/plain,
  application-specific types, type negotiation
- Drop position feedback — visual highlight on potential targets (enter/leave
  callbacks on drop-aware widgets)
- Incremental transfer — large payloads via INCR property protocol

**Abstraction (ISWPlatformDragDrop vtable):**

The XDND protocol is X11-specific. Other platforms have completely different DnD
mechanisms (Arcan has its own data transfer model, Wayland has wl_data_device,
web has HTML5 drag events). The abstraction needs to be at the semantic level:

```
ISWPlatformDragDrop
  ├─ drag_start(widget, mime_types[], data_callback)
  ├─ drag_set_actions(allowed_actions)
  ├─ drag_set_icon(pixmap or render surface)
  ├─ drop_register(widget, accepted_types[], callback)
  ├─ drop_unregister(widget)
  └─ internal: enter/leave/motion/drop event routing
```

The XCB backend implements this via XDND atoms + client messages + selection
transfers. Other backends implement their native protocol. Widget code only sees
the abstract API.

Files: ISWXdnd.c (~470 lines, rewrite), ISWXdnd.h (expand public API).
Depends on: ISWPlatformEvent, ISWPlatformWindow, ISWPlatformSelection.

### Remove X Session Management (XSMP/ICE) support

SessionShell and associated API are dead code. The XSMP protocol (save/restore
app state across logout/login via libSM/libICE) is unused by any downstream
consumer — ISDE links libSM/libICE but calls zero SM/ICE functions, implementing
its own session management instead. Modern desktops use D-Bus or .desktop
autostart; Wayland doesn't use XSMP at all.

**Remove from ISW:**

- `SessionShell` widget class and `sessionShellWidgetClass` symbol
- Session-related resources: `XtNsessionID`, `XtNrestartCommand`,
  `XtNcloneCommand`, `XtNresignCommand`, `XtNshutdownCommand`,
  `XtNsaveCallback`, `XtNsaveCompleteCallback`, `XtNdieCallback`,
  `XtNerrorCallback`, `XtNcancelCallback`
- `XtSessionGetToken()`, `XtSessionReturnToken()`
- Any ICE transport integration in the event loop
- `XtIsSessionShell()` predicate
- Obsolete non-App-context functions that only exist for backward compat:
  `XtInitialize`, `XtMainLoop`, `XtAddConverter`, `XtPeekEvent`,
  `XtDestroyGC`, `XtAddInput`, `XtAddTimeOut`, `XtProcessEvent`,
  `XtAddActions`, `XtCreateApplicationShell`, `XtSetErrorHandler`,
  `XtSetWarningHandler`, `XtStringConversionWarning`

Files: ShellP.h, Shell.h (generated from string.list), Intrinsic.h,
util/string.list, and any Session-related source in src/.

Do this before the API rename so the new Isw namespace starts clean.

### Full API rename — Xt → Isw

Once the platform vtable is in place, the embedded libXt is no longer libXt. It
is a platform-agnostic toolkit intrinsics layer that happens to have an XCB
backend. The entire public API should be renamed at that point:

- Function prefix: Xt → Isw (XtCreateWidget → IswCreateWidget, etc.)
- Type prefix: Xt → Isw (XtResource → IswResource, XtCallbackProc →
  IswCallbackProc, XtAppContext → IswAppContext, etc.)
- Xrm prefix: Xrm → Isw (XrmValue → IswValue, XrmQuark → IswQuark, etc.)
- Header path: include/X11/ → include/ISW/ (Intrinsic.h, IntrinsicP.h, Core.h,
  Shell.h, StringDefs.h, etc. move into the ISW namespace)
- Macro aliases: XtTypes.h Xlib-compat macros (ConnectionNumber,
  DefaultRootWindow, BlackPixelOfScreen, etc.) replaced by ISWPlatform vtable
  calls
- Predefined atoms/constants: XA_PRIMARY, XA_STRING etc. become ISW-namespaced
  or backend-internal details

This is a mechanical but large rename. Do it as the final step after the vtable
is proven, not before — renaming first would create unnecessary churn during
development. Provide compat headers (include/X11/ → include/ISW/ forwarding)
during a transition period if needed for downstream consumers.
