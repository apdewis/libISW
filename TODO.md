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

## Restore internationalization — real UTF-8 text support

The ISW_INTERNATIONALIZATION build flag (on by default) gates code paths
inherited from Xaw3d's i18n support, but the actual functionality is gone.
The original Xlib calls (XwcDrawString, XmbTextExtents, XCreateFontSet,
XwcGetColumn) were removed during the XCB conversion and not replaced. The
text pipeline is ASCII end to end.

### Current state

- **MultiSink/MultiSrc**: exist and compile, but their wide-character paths
  route through the same single-byte rendering as AsciiSink. ISWFontSet is
  a thin wrapper around xcb_font_t with ascent/descent — no multibyte text
  measurement or rendering.
- **ISWFontSet**: defined in ISWXftCompat.h as {xcb_font_t, ascent, descent,
  height}. IswTextWidth and IswDrawString use xcb_image_text_8 — single-byte
  only.
- **XFontSet**: typedef'd to void* in XtTypes.h. Not functional.
- **Wchar conversion**: _ISWTextWCToMB in TextSrc.c and IswWcToUtf8 in
  MultiSink.c exist but nothing produces wchar input.
- **XIM**: ISWIm.c still calls Xlib XOpenIM/XCreateIC. Disabled via
  ISW_HAS_XIM=OFF. Non-functional.
- **Label, SmeBSB, List, etc.**: have #ifdef ISW_INTERNATIONALIZATION
  fontset resource paths that compile but do nothing useful — the fontset
  is the same xcb_font_t underneath.

### What needs to happen

**Text rendering** — replace xcb_image_text_8 with a rendering path that
handles UTF-8. Two options depending on where this falls relative to the
GL backend TODO:

- If before GL backend: use Cairo's cairo_show_text / pango, which handle
  UTF-8 natively. ISWRenderDrawString routes through Cairo-XCB already.
- If after GL backend: use FreeType glyph atlas with UTF-8 codepoint
  decoding. NanoVG has basic UTF-8 text support built in.

Either way, ISWRenderDrawString and ISWRenderTextWidth need to accept and
correctly measure UTF-8 strings. This is the minimum viable fix.

**Text measurement** — IswTextWidth must measure UTF-8 strings correctly.
Currently delegates to xcb_query_text_extents which is single-byte. Replace
with FreeType/Fontconfig metrics or Cairo text extents.

**ISWFontSet replacement** — the current ISWFontSet struct is not useful.
Replace with a font abstraction that wraps FreeType (or Cairo font) and
provides:
- Open by fontconfig pattern (already the discovery path)
- UTF-8 string width measurement
- Glyph rendering (via ISWRender)
- Ascent/descent/height metrics
This overlaps with the font refactor plan in FONT_REFACTOR_PLAN.md.

**MultiSink/MultiSrc** — once text rendering handles UTF-8, the
distinction between AsciiSink and MultiSink mostly disappears. MultiSink
becomes "text sink that uses ISWFontSet" rather than "text sink that uses
Xlib wide-char functions." The wchar conversion layer (_ISWTextWCToMB etc.)
can be removed — work in UTF-8 throughout, no wchar intermediate.

**XIM replacement** — lowest priority. Requires an XCB-native input method
protocol or integration with IBus/Fcitx via DBus. This is a large effort
and can be deferred. Without it, CJK input composition is unavailable.

### Suggested order

1. Make ISWRenderDrawString / ISWRenderTextWidth handle UTF-8 (smallest
   change with largest impact — most display widgets start working)
2. Replace ISWFontSet with FreeType/Fontconfig-backed font abstraction
3. Fix MultiSink to use new font abstraction for text entry
4. Remove wchar conversion layer, work in UTF-8 throughout
5. Clean up or remove dead #ifdef ISW_INTERNATIONALIZATION scaffolding
   that no longer serves a purpose
6. XIM replacement (deferred — separate effort)

### Files

Core: ISWXftCompat.h, ISWXcbDraw.c, IswXcbDraw.c, ISWRenderCairoXCB.c,
MultiSink.c, MultiSrc.c, AsciiSink.c, TextSrc.c, ISWI18n.c, ISWIm.c.
Display widgets: Label.c, SmeBSB.c, List.c, ListView.c, IconView.c, Tip.c,
Slider.c.

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

## Full API rename — Xt → Isw

The embedded libXt uses Xt/Xrm prefixes and lives under include/X11/. This
made sense when it was a fork of libXt, but the subsequent work — GL backend,
windowless widgets, platform vtable — will create substantial new code. If the
rename happens after those changes, every new function, type, and header gets
written with the old names and then mechanically rewritten. Do it first so all
new infrastructure uses the final namespace from the start.

### Scope

- Function prefix: Xt → Isw (XtCreateWidget → IswCreateWidget, etc.)
- Type prefix: Xt → Isw (XtResource → IswResource, XtCallbackProc →
  IswCallbackProc, XtAppContext → IswAppContext, etc.)
- Xrm prefix: Xrm → Isw (XrmValue → IswValue, XrmQuark → IswQuark, etc.)
- Header path: include/X11/ → include/ISW/ (Intrinsic.h, IntrinsicP.h,
  Core.h, Shell.h, StringDefs.h, etc. move into the ISW namespace)
- Macro aliases: XtTypes.h Xlib-compat macros (ConnectionNumber,
  DefaultRootWindow, BlackPixelOfScreen, etc.) get Isw-prefixed
  replacements
- Predefined atoms/constants: XA_PRIMARY, XA_STRING etc. become
  ISW-namespaced or backend-internal details
- Generated files: util/string.list, util/makestrs need updating so
  StringDefs.h generates Isw names

### What does NOT change

- ISWRender API — already Isw-namespaced
- ISW widget names (IswCommand, IswLabel, etc.) — already Isw-namespaced
- XCB types (xcb_connection_t, xcb_window_t, etc.) — these get abstracted
  later by the platform vtable, not this rename

### Approach

This is mechanical — sed/script-driven, not design work. But it touches
every file in the project (~150+ source and header files).

1. Write a rename script mapping old symbols → new symbols. Build the
   mapping from the public headers (include/X11/*.h) — every Xt/Xrm
   prefixed typedef, function, macro.
2. Move include/X11/*.h → include/ISW/ (many already live there)
3. Run the rename across all source and headers
4. Update CMakeLists.txt, pkg-config, util/makestrs, util/string.list
5. Provide compat headers: include/X11/Intrinsic.h that #includes
   include/ISW/Intrinsic.h with #define aliases, for downstream
   consumers during transition. Remove after one release cycle.
6. Update examples/isw_demo to use new names
7. Verify build, run demo

### Downstream impact

The compat headers mean existing code keeps compiling with deprecation
warnings. New code uses Isw names. The app developer guide
(docs/CLAUDE_APP_GUIDE.md) gets updated to reflect the new API.

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
ISWRenderGL consumes it.

**Depends on:** Xt → Isw rename (above) — new GL backend code uses Isw
names from the start.

**Sequencing:** Rename → GL backend → windowless widgets → platform vtable.

## Windowless widgets — eliminate window-per-widget model

The current architecture creates one X window per widget. The X server
handles clipping, background painting, cursor management, and event routing
by window ID. This model is incompatible with Arcan (one surface per client
segment, no window tree) and is unnecessary overhead on X11 for interior
widgets that don't need their own server-side resource.

Move to a model where only shells, popups, and menus have real platform
windows. All interior widgets are windowless — they draw into their parent's
window and receive events via hit-testing.

### What the X server currently does for free

| Service | Current (windowed) | Replacement (windowless) |
|---|---|---|
| Event routing | X sends event to window → XtWindowToWidget lookup | Hit-test widget tree at pointer coords |
| Clipping | Child window clips to parent bounds automatically | ISWRender set_clip_rectangle before each child |
| Background | X paints XCB_CW_BACK_PIXEL on Expose | Widget paints own background in expose method |
| Cursor | XCB_CW_CURSOR per window | Track pointer widget, update shell cursor on change |
| Scrolling | Move child window to negative coords, clip window masks | Coordinate translation + software clip |
| Enter/Leave | X generates crossing events at window boundaries | Synthesize from pointer motion across widget bounds |
| Event masks | Per-window xcb_change_window_attributes | Shell selects all types, filter in dispatch |
| Stacking | X server manages child window z-order | Widget tree order defines paint/hit-test order |

### What stays windowed

- **TopLevelShell / ApplicationShell** — real WM-managed windows
- **OverrideShell** — menus, tooltips (override-redirect)
- **TransientShell** — dialogs
- Optionally **Viewport clip windows** — performance optimization, can be
  eliminated later

### Phase 1: Windowless widget infrastructure

Add windowless realization path. RectObj (RectObj.c:100) already has
`realize = NULL` — extend this pattern to Core-derived widgets.

- Add `Boolean windowless` to CorePart, default True for non-shell widgets
- Windowless realize: skip xcb_create_window, inherit parent's window for
  drawing context
- `XtWindow(widget)` returns nearest windowed ancestor's window
- `XtIsRealized(widget)` returns True if nearest windowed ancestor is
  realized
- Modify XtCreateWindow (Intrinsic.c:527) to no-op for windowless widgets

Files: CoreP.h, Core.c, Intrinsic.c, Create.c.

### Phase 2: Hit-test event dispatch

Replace window-based event routing with spatial dispatch for windowless
widgets.

- Implement `_XtFindWidgetAtPoint(shell, x, y)` — recursive descent
  through widget tree, test point-in-rectangle, return deepest match.
  Walk children in reverse stacking order (topmost first).
- Modify XtDispatchEvent (Event.c:1735+) — when XtWindowToWidget returns
  a shell, hit-test to find the actual target widget, translate coords
  to widget-local.
- Keyboard events: route to focus widget (already logical —
  Keyboard.c:169 tracks focusWidget, no change needed).
- Synthesize Enter/Leave: track "pointer widget", on motion compare
  with hit-test result, generate crossing events on change.

Files: Event.c (~1,200 lines, major rework), Keyboard.c, Pointer.c.

### Phase 3: Expose and rendering

Parent's expose method must walk children and delegate rendering.

- Shell expose: iterate children in stacking order
- Before each child: push ISWRender clip rectangle to child bounds
- Translate coordinate origin to child's (x, y)
- Call child's expose method
- After: pop clip, restore origin
- Damage tracking: intersect expose region with each child's bounds,
  skip children outside damaged area
- Background painting: widgets already mostly paint their own backgrounds
  via ISWRender. Remove reliance on X server XCB_CW_BACK_PIXEL — move
  background fill to start of each widget's expose method.

Files: Core.c (expose dispatch), Shell.c, ISWRender.c (coordinate
transform helpers), every widget expose method (add background fill).

### Phase 4: Cursor management

- Shell tracks "current cursor widget" — the widget under the pointer
  from the most recent hit-test
- On pointer motion: if cursor widget changed, look up new widget's
  cursor resource, call xcb_change_window_attributes on the shell window
  (or platform equivalent)
- Simple.c currently sets cursor per-widget window in Realize — change
  to store cursor value in widget instance, apply lazily via shell

Files: Simple.c, Shell.c, Event.c (pointer motion hook).

### Phase 5: Viewport scrolling

Viewport.c currently scrolls by moving the child window to negative
coordinates (XtMoveWidget at Viewport.c:511). The clip window masks
overflow.

- Replace with scroll offset stored in Viewport instance
- During child rendering: apply offset as coordinate translation
- During hit-testing: apply inverse offset
- Use ISWRender clip rectangle instead of clip window
- Remove xcb_reparent_window calls (Viewport.c:353, 415-416, 428-429)

Files: Viewport.c (~500 lines, significant rework).

### Phase 6: Geometry management

Widget x, y coordinates are currently relative to parent window and
applied via xcb_configure_window. For windowless widgets:

- x, y remain relative to parent (no change to geometry negotiation)
- XtConfigureWidget (Geometry.c:650) skips xcb_configure_window for
  windowless widgets — just updates fields and triggers
  expose/
- XtTranslateCoords (Geometry.c:779) already walks the hierarchy
  accumulating offsets — continues to work unchanged
- XtMoveWidget, XtResizeWidget skip X calls for windowless widgets,
  invalidate affected region in parent instead

Files: Geometry.c.

### Migration strategy

Opt-in per widget class, not a flag day:

1. Build the infrastructure (phases 1-2) with all widgets still windowed
2. Convert leaf widgets first: Label, Command, Toggle (no children,
   simple expose)
3. Convert container widgets: Box, Form, Paned (need expose delegation)
4. Convert complex widgets: Text, List, Scrollbar (heavy event usage)
5. Convert Viewport last (scrolling rework)
6. Shells never convert — they own the real windows

Each step is independently testable via the demo app. A widget can be
toggled back to windowed by overriding `windowless = False` in its
class record if problems surface.

### Relationship to other TODOs

**Depends on:** Xt → Isw rename, ISWRenderGL (phase 3 needs render
clip/transform support that shouldn't be built on Cairo).

**Enables:** ISWPlatform vtable — with windowless widgets, the platform
only needs to create windows for shells and popups. ISWPlatformWindow
becomes a small interface instead of the largest abstraction surface.
ISWPlatformEvent only translates native events at the shell level;
interior dispatch is pure toolkit logic.

**Sequencing:** Rename → GL backend → windowless widgets → platform vtable.

## ISWPlatform vtable — abstract all X11/XCB platform dependencies

ISWRender already abstracts drawing, the GL backend (above) removes the Cairo
dependency, and windowless widgets (above) reduce the platform surface to
shells and popups only. The resource system is being abstracted separately
(above). What remains — display/connection, shell windows, top-level events,
input, grabs, atoms, selections, colormaps, fonts, cursors — is still hardcoded
to XCB. This needs a platform vtable so backends other than X11 can be
supported (Arcan/SHMIF, or any platform that provides EGL and an event system).

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

### Note on API names

The Xt → Isw rename (above) is sequenced before this work. All new
platform vtable code uses Isw-prefixed names from the start.
