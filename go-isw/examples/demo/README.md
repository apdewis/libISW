# ISW Go Demo

A demo application exercising the go-isw bindings. It creates a tabbed
window with four pages:

- **Basic Widgets** — buttons, toggles, slider, progress bar, spin box,
  text input
- **Selection** — List, ComboBox, ListBox
- **Drawing** — DrawingArea with ISWRender API calls from Go (shapes,
  paths, gradients, text)
- **Form Layout** — Form widget with labelled text fields

It also demonstrates menus (via MenuBar/SimpleMenu/SmeBSB), a status bar,
and callback wiring between Go closures and C widget callbacks.

## Prerequisites

1. **ISW library** installed (`libISW.so` + headers under
   `<prefix>/include/isw/`).
2. **pkg-config** can find `isw` and `xcb-icccm`:

   ```
   pkg-config --cflags --libs isw xcb-icccm
   ```

3. **Go >= 1.21** with cgo enabled (`CGO_ENABLED=1`, the default).

All of these are satisfied on the ISW development machine after a normal
`cmake --install build`.

## Building

From this directory:

```bash
go build -o isw_go_demo .
```

Or from the repo root:

```bash
cd go-isw/examples/demo
go build -o isw_go_demo .
```

## Running

```bash
./isw_go_demo
```

Requires a running X server (XCB). The window opens at 900×650.

## What it exercises

| Binding layer          | Demonstrated by                          |
|------------------------|------------------------------------------|
| App init / main loop   | `isw.AppInitialize`, `app.MainLoop`      |
| Widget lifecycle       | `CreateManagedWidget`, `Realize`, `SetValues` |
| Callbacks              | Go closures on buttons, toggles, slider, menus |
| ArgList builder        | `NewArgList().Add().AddString().AddWidget()` |
| Resource constants     | `Nlabel`, `Ncallback`, `Nwidth`, etc.    |
| Widget classes         | 15+ classes (Box, Form, Command, Toggle, Slider, …) |
| ISWRender API          | DrawingArea expose callback with shapes, paths, text |
| Pixel helpers          | `PixelARGB` for render colours            |
| Menus (via cgo shims)  | SimpleMenu + SmeBSB + MenuButton          |
| Status bar             | StatusBar + Label with live updates       |
