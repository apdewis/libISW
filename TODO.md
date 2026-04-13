# TODO

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

## ISWRenderGL — native GL rendering backend (replace Cairo dependency)

Cairo is a bottleneck for multi-platform support. Cairo-XCB ties rendering to
X11. Cairo-EGL (declared but unimplemented) is unmaintained upstream and was
always a second-class Cairo backend. Meanwhile both X11 (via EGL) and Arcan
(via arcan_shmifext) provide native EGL contexts that GL can render into
directly.

Replace Cairo with a native GL 2D rendering backend so the render layer works
identically on any platform that provides an EGL surface.

### Architecture

```
ISWRender vtable (unchanged API)
       ↓
ISWRenderGL backend (new)
  ├─ 2D primitives: NanoVG or equivalent (rectangles, lines, arcs, polygons)
  ├─ Text: FreeType glyph atlas → GL textures
  ├─ Gradients, alpha, anti-aliasing: native GL
  └─ EGL surface from ISWPlatform (see next TODO)
       ↓
EGL context — provided by platform
  ├─ X11:  EGL_KHR_platform_xcb / EGL_EXT_platform_x11
  └─ Arcan: arcan_shmifext_egl_meta()
```

### What the GL backend replaces

| Current (Cairo) | GL backend |
|---|---|
| cairo_xcb_surface_create | EGL surface (platform-provided) |
| cairo_fill/stroke | NanoVG nvgFill/nvgStroke or raw GL |
| cairo_show_text + fontconfig | FreeType + glyph texture atlas |
| cairo_pattern_create_linear | GL shader gradient |
| pixman (transitive dep) | gone |

### ISWRenderOps vtable cleanup

The current vtable leaks XCB types that block non-X backends:

- `xcb_point_t *pts` in fill_polygon/stroke_polygon — replace with
  `ISWPoint` (typedef struct { int16_t x, y; })
- `xcb_pixmap_t` in draw_pixmap — replace with opaque `ISWDrawable` handle
  that each platform backend maps to its native drawable type
- `ISWRenderContext` embeds `xcb_connection_t*`, `xcb_window_t`,
  `xcb_screen_t*`, `xcb_colormap_t` — replace with opaque `ISWDisplay`
  and `ISWWindow` handles (dovetails with ISWPlatform vtable below)
- `get_cairo_context` op — remove; GL backend has no cairo_t to return.
  Any widget code reaching through this is backend-specific and needs
  fixing.

### Font rendering without Cairo

Cairo currently delegates to FreeType/Fontconfig. The GL backend needs its
own text path:

1. Font discovery: Fontconfig (keep — it's not tied to Cairo or X11)
2. Glyph rasterization: FreeType (keep — already an indirect dependency)
3. Glyph caching: texture atlas, packed per font size. Upload glyphs on
   first use, render as textured quads.
4. XFontStruct compatibility: ISWRender's set_font/text_width/text_height
   ops already abstract this. The GL backend implements them via FreeType
   metrics instead of xcb_query_font.

### NanoVG vs raw GL

NanoVG is ~4,500 lines, zlib licensed, designed exactly for this use case
(anti-aliased 2D vector graphics on GL). Provides path-based drawing,
gradients, text rendering (via stb_truetype, replaceable with FreeType),
scissoring. Can be vendored as a single .c/.h pair.

Alternative: raw GL with a small custom 2D layer. More control, more code
to maintain. NanoVG is the pragmatic choice unless its text rendering or
gradient model proves insufficient.

### Suggested order

1. Vendor NanoVG (or chosen 2D-on-GL library)
2. Implement ISWRenderGL backend behind ISWRenderOps vtable
3. Clean XCB types out of ISWRenderOps and ISWRenderContext
4. EGL context creation on X11 (ISW_RENDER_BACKEND_GL enum value)
5. Verify all widgets render correctly via GL path
6. Cairo-XCB backend becomes optional/legacy
7. Remove Cairo-EGL declarations (never implemented, now superseded)

### Dependencies

- EGL (already optional dep in CMakeLists.txt)
- GL ES 2.0+ or GL 2.1+ (NanoVG requirement)
- FreeType + Fontconfig (already transitive deps via Cairo — become direct)
- NanoVG or equivalent (vendored)

### Relationship to ISWPlatform vtable

This TODO handles the *rendering* side. The ISWPlatform vtable (below)
handles *everything else* (windows, events, input, etc.). The two connect
at EGL surface creation: ISWPlatform provides the EGL display/surface,
ISWRenderGL consumes it. Do this first — it removes the Cairo dependency
that would otherwise force every new platform to port Cairo.

## ISWPlatform vtable — abstract all X11/XCB platform dependencies

ISWRender already abstracts drawing, and the GL backend (above) removes the
Cairo dependency from rendering. The resource system is being abstracted
separately (above). Everything else — display, windows, events, input, grabs,
atoms, selections, colormaps, fonts, cursors — is still hardcoded to XCB. This
needs a platform vtable so backends other than X11 can be supported (Arcan/SHMIF,
or any platform that provides EGL and an event system).

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

**Fonts** — xcb_font_t, XLFD queries, XtFontStruct. The GL backend handles
rendering via FreeType+fontconfig; the XCB font path is legacy metrics queries.
Abstract to open-by-pattern + metrics.
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

### Drag-and-drop — abstract ISWXdnd behind platform vtable

Current state: ISWXdnd.c (~1,800 lines) implements full XDND v5 with both drag
source and drop target support, hardwired to XCB atoms, client messages,
selection transfers, and xcb_translate_coordinates. Feature-complete for the XCB
backend:

- Drag source via ISWXdndStartDrag — pointer grab, motion tracking, full
  XdndEnter/Position/Drop protocol, XdndStatus/XdndFinished handling
- Drag visual feedback — glyph cursor changes, drag icon window from pixmap
- Action negotiation — Copy, Move, Link with modifier-key selection
  (Ctrl=Copy, Shift=Move, Ctrl+Shift=Link) and source/target negotiation
- Multiple MIME types — arbitrary types via ISWXdndSetAcceptedTypes and
  ISWXdndInternType; text/uri-list and text/plain interned by default
- Drop position feedback — dragEnter/dragMotion/dragLeave callbacks with
  IswDragOverCallbackData for visual highlight on targets
- Incremental transfer — delegated to Xt selection mechanism (INCR for free)

**Minor gaps:**

- Ask and Private actions are declared in the IswDndAction enum but have no
  atom/protocol support

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

Files: ISWXdnd.c (~1,800 lines, refactor into vtable), ISWXdnd.h.
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
