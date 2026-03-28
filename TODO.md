# TODO

## Remove vestigial GC management

Replace xcb_gcontext_t fields in widget private structs with direct color/state
tracking. All drawing already goes through ISWRender (Cairo); the GC fields
allocated via XtGetGC/XtReleaseGC are only used as identity comparisons to pick
colors. Removing them eliminates GCManager.c and the XCB GC allocation overhead
entirely.

Affected widgets: Command, Label, List, SmeBSB, Scrollbar, Panner, ProgressBar,
Tip, StripChart, SmeLine, Tree, AsciiSink, MultiSink, Paned.

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
