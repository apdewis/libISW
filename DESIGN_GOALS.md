# ISW Design Goals

## Platform Abstraction

ISW currently runs exclusively on XCB. The long-term goal is to support three
platform backends through a shared abstraction layer:

- **XCB** — current and primary target (X11 via pure XCB, no Xlib)
- **WebAssembly** — browser-based rendering via Emscripten (Canvas 2D or WebGL)
- **Arcan/SHMIF** — native Arcan client via shared-memory IPC

Wayland is explicitly a non-goal and will never be a supported target.

### ISWPlatform Layer

The existing ISWRender API demonstrates the pattern: an opaque context with a
vtable of function pointers, backend-specific implementations behind it,
widgets unaware of which backend is active. The same pattern needs to be pushed
down into the embedded libXt layer.

An `ISWPlatformOps` vtable would abstract the operations that currently call
XCB directly throughout the ~52 libXt source files: display/connection
management, window creation and mapping, event polling, atom interning,
property manipulation, selection/clipboard, and flush/sync.

Each platform backend implements the vtable:

| Operation | XCB | WASM | Arcan/SHMIF |
|---|---|---|---|
| Open display | `xcb_connect()` | Canvas context setup | `arcan_shmif_open()` |
| Create window | `xcb_create_window()` | Canvas element | Subsegment request |
| Poll events | `xcb_poll_for_event()` | DOM event callbacks | `arcan_shmif_poll()` |
| Flush | `xcb_flush()` | `putImageData` / swap | `arcan_shmif_signal()` |
| Atoms | `xcb_intern_atom()` | String hash table | String hash table |
| Clipboard | X selection protocol | `navigator.clipboard` | SHMIF `BCHUNK` events |

The render surface per backend:

| Backend | Render target |
|---|---|
| XCB | Cairo-XCB surface / Cairo-EGL surface / raw XCB pixmap |
| WASM | Canvas 2D / Cairo image surface blitted to canvas |
| Arcan/SHMIF | Cairo image surface on SHMIF `vidp` shared-memory buffer |

### Migration Strategy

1. Define `ISWPlatformOps` by auditing the actual XCB calls made by the libXt
   files. Most of the 800+ XCB references collapse into roughly 40-50 distinct
   operations.
2. Write the XCB implementation first as a thin wrapper around existing calls.
   Behavior is identical — just routed through the vtable.
3. Convert libXt files incrementally, one at a time. Each file compiles and
   works after conversion because the XCB backend does the same thing it
   always did.
4. Rename Xt-namespaced functions to ISW namespace as part of the conversion
   (e.g., `XtOpenDisplay` becomes an ISW-namespaced function returning a
   generic `ISWDisplay`).

## Generic Input Model

The X11 input model encodes hardware assumptions from the 1990s into the
toolkit core: three numbered mouse buttons, scroll as synthetic button 4/5
press/release pairs, no concept of touch input. Every non-X platform would
have to fake this, losing information in the process.

ISW will define a generic input model where widgets express intent rather than
hardware specifics. The translation happens once at the platform boundary, not
inside every widget.

### Pointer Events

Pointer actions use semantic roles instead of numbered buttons:

| Role | Meaning | XCB | WASM | Arcan |
|---|---|---|---|---|
| PRIMARY | Activate / select | Button 1 | button 0 | MOUSE_BTN1 |
| SECONDARY | Context action | Button 3 | button 2 | MOUSE_BTN3 |
| TERTIARY | Paste / middle action | Button 2 | button 1 | MOUSE_BTN2 |

### Scroll Events

Scroll is a first-class continuous axis, not synthetic button presses.
Backends translate their native scroll representation into a unified axis
event with both continuous (pixel-level delta) and discrete (click-wheel step)
values. This makes smooth trackpad scrolling, browser wheel events, and
Arcan analog axis input all work correctly without losing precision.

### Touch Events (Future)

Touch input (begin/move/end with touch ID) maps naturally onto the generic
model for the WASM backend. Not relevant for XCB or Arcan desktop use today,
but the input abstraction accommodates it without redesign.

### Translation Manager

The translation manager (`TMparse.c` / `TMstate.c`) currently parses event
specifications like `<Btn1Down>`. Under the generic model, the canonical form
becomes semantic: `<PrimaryDown>`, `<SecondaryUp>`, `<ScrollY>`, etc. Legacy
`<Btn1Down>` syntax is kept as an alias for backward compatibility. Platform
backends convert hardware events to the generic enum before the event reaches
the translation manager — the TM itself still matches against integer
constants, just different ones.

### Widget Impact

Widgets that currently reference `<Btn1Down>` in their default translation
tables would use `<PrimaryDown>` instead. The semantic is identical on X11 but
correct everywhere else. Widget callback code that inspects button numbers
would check against `ISW_INPUT_BUTTON_PRIMARY` etc. rather than `Button1`.
