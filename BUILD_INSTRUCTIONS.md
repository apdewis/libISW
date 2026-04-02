# ISW (Infi Systems Widgets) Build Instructions

## Overview

ISW is a modern fork of Xaw3d (X 3D Athena Widget Set) that uses XCB (X C-language Binding) instead of Xlib for X11 communication. The project includes an embedded XCB-based X Toolkit Intrinsics (libXt) implementation, eliminating the need for external Xlib dependencies.

## Architecture

```
ISW - libISW.so (single unified library)
  |
  ├─ ISW Widgets (Box, Button, List, Text, etc.)
  ├─ X Toolkit Intrinsics (XCB-based libXt)
  └─ XCB Libraries (libxcb, libxcb-xfixes, libxcb-render, etc.)
       └─ X11 protocol
```

**NO Xlib in this chain!** The entire stack uses XCB for X11 communication.

## Build Dependencies

### Required Packages

The build system will check for these dependencies:

- **X Toolkit Intrinsics dependencies** (embedded in libISW):
  - libsm (Session Management)
  - libice (Inter-Client Exchange)
  - xproto (X11 protocol headers)
  - libxcb (X C-language Binding core)
  - libxcb-xrm (XCB X Resource Manager)
  - libxcb-keysyms (XCB keyboard symbols)

- **ISW widget dependencies**:
  - libxcb-xfixes (XCB fixes extension)
  - libxcb-shape (XCB shape extension)
  - libxcb-present (XCB present extension)
  - cairo / cairo-xcb (rendering backend)
  - fontconfig
  - freetype2

- **Optional dependencies**:
  - libxkbcommon (keyboard handling, recommended)
  - egl + cairo-gl (hardware-accelerated Cairo-EGL backend)

### Installation Commands

**Debian/Ubuntu**:
```bash
sudo apt-get install \
  libsm-dev libice-dev \
  libxcb1-dev libxcb-xrm-dev libxcb-keysyms1-dev \
  libxcb-xfixes0-dev libxcb-shape0-dev libxcb-present-dev \
  libcairo2-dev libfontconfig-dev libfreetype-dev \
  libxkbcommon-dev \
  cmake pkg-config gcc \
  bison flex
```

**Fedora/RHEL**:
```bash
sudo dnf install \
  libSM-devel libICE-devel \
  libxcb-devel xcb-util-xrm-devel xcb-util-keysyms-devel \
  cairo-devel fontconfig-devel freetype-devel \
  libxkbcommon-devel \
  cmake gcc pkgconfig \
  bison flex
```

**Arch Linux**:
```bash
sudo pacman -S \
  libsm libice \
  libxcb xcb-util-xrm xcb-util-keysyms \
  cairo fontconfig freetype2 \
  libxkbcommon \
  cmake base-devel pkg-config \
  bison flex
```

## Building from Source

### 1. Configure

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
```

Configuration options:
- `-DISW_INTERNATIONALIZATION=ON` (default): Enable I18N/multibyte support
- `-DISW_HAS_XIM=ON`: Enable X Input Method for CJK input
- `-DISW_GRAY_BLKWHT_STIPPLES=ON`: Enable gray stipple patterns

### 2. Build

```bash
cmake --build build -j$(nproc)
```

This will:
1. Build `util/makestrs` utility
2. Generate `StringDefs.c` and `StringDefs.h` from `util/string.list`
3. Compile all libXt sources (X Toolkit Intrinsics)
4. Compile all ISW widget sources
5. Link everything into `libISW.so`

### 3. Install

```bash
sudo cmake --install build
```

This installs:
- `libISW.so.1.0.0` → `<prefix>/lib/`
- Headers → `<prefix>/include/ISW/` and `<prefix>/include/X11/`
- pkg-config file → `<prefix>/lib/pkgconfig/isw.pc`
- CMake config → `<prefix>/lib/cmake/ISW/`

### 4. Update Library Cache

```bash
sudo ldconfig
```

## Build Verification

After building, verify the library has no Xlib dependencies:

```bash
# Should show NO libX11
ldd build/libISW.so | grep libX11

# Should show XCB libraries
ldd build/libISW.so | grep xcb
```

Expected XCB dependencies:
```
libxcb-xrm.so.0
libxcb-keysyms.so.1
libxcb-xfixes.so.0
libxcb-render.so.0
libxcb-shape.so.0
libxcb.so.1
libxcb-util.so.1
```

## Type Compatibility

The embedded XCB-based libXt provides these type mappings:

| Traditional Xlib | XCB Equivalent | Notes |
|-----------------|----------------|-------|
| `Display*` | `xcb_connection_t*` | XCB connection |
| `Window` | `xcb_window_t` | Window ID |
| `Pixmap` | `xcb_pixmap_t` | Pixmap ID |
| `GC` | `xcb_gcontext_t` | Graphics context |
| `XEvent` | `xcb_generic_event_t*` | Event structures |
| `Region` | `xcb_xfixes_region_t` | Region ID |

Widget callback signatures use these XCB types, NOT Xlib types.

## Header Organization

```
include/
├── ISW/              # ISW widget public headers
│   ├── Box.h
│   ├── Command.h
│   ├── List.h
│   ├── Text.h
│   └── ...
└── X11/              # X Toolkit Intrinsics headers
    ├── Intrinsic.h   # Core Xt API
    ├── StringDefs.h  # String constants (generated)
    ├── Shell.h       # Shell widgets (generated)
    ├── XtQuark.h     # String interning
    ├── XtValue.h     # Value conversions
    └── ...
```

## Using ISW in Your Application

### pkg-config

```bash
gcc myapp.c -o myapp $(pkg-config --cflags --libs isw)
```

### Makefile Example

```makefile
CFLAGS = $(shell pkg-config --cflags isw)
LIBS = $(shell pkg-config --libs isw)

myapp: myapp.c
	$(CC) $(CFLAGS) -o myapp myapp.c $(LIBS)
```

### CMake Example

```cmake
find_package(ISW REQUIRED)
add_executable(myapp myapp.c)
target_link_libraries(myapp ISW::ISW)
```

Or via pkg-config:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(ISW REQUIRED isw)

add_executable(myapp myapp.c)
target_include_directories(myapp PRIVATE ${ISW_INCLUDE_DIRS})
target_link_libraries(myapp ${ISW_LIBRARIES})
```

### Source Code

```c
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <ISW/Box.h>
#include <ISW/Command.h>

int main(int argc, char **argv)
{
    Widget toplevel, box, button;
    xcb_connection_t *dpy;

    toplevel = XtInitialize(argv[0], "MyApp", NULL, 0, &argc, argv);

    box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);
    button = XtCreateManagedWidget("quit", commandWidgetClass, box, NULL, 0);

    XtRealizeWidget(toplevel);
    XtMainLoop();

    return 0;
}
```

## Current Build Status

- CMake build system (minimum version 3.20)
- XCB-based implementation (no Xlib)
- All ISW widgets compile and link
- String generation system working
- Arrow scrollbars always enabled (ISW_ARROW_SCROLLBARS)
- Internationalization enabled by default (ISW_INTERNATIONALIZATION)
- Complete Xaw3d → ISW renaming

## Troubleshooting

### CMake configure fails with "Package X not found"

**Solution**: Install missing dependencies. The CMake output will tell you which package is missing.

### Build fails with "X11/StringDefs.h: No such file"

**Solution**: The string generation failed. Check that:
1. `util/makestrs` built successfully
2. `util/string.list` exists
3. Re-run `cmake --build build --clean-first`

### Runtime error: "cannot open shared object file"

**Solution**: Update the library cache:
```bash
sudo ldconfig
```

Or add to `LD_LIBRARY_PATH`:
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Application crashes with XCB errors

**Solution**: Verify you're using XCB types, not Xlib types. Check that:
1. Your code includes `<X11/Intrinsic.h>` from ISW
2. You're using `xcb_connection_t*` not `Display*`
3. Event handlers use `xcb_generic_event_t*` not `XEvent*`

## Development

### Directory Structure

```
.
├── CMakeLists.txt        # Build configuration
├── config.h.cmake.in     # config.h template
├── isw.pc.cmake.in       # pkg-config template
├── cmake/
│   └── ISWConfig.cmake.in  # CMake package config template
├── src/
│   ├── ActionHook.c     # LibXt sources (X Toolkit Intrinsics)
│   ├── Alloc.c
│   ├── ...
│   ├── AllWidgets.c     # ISW widget sources
│   ├── Box.c
│   ├── Command.c
│   └── ...
├── include/
│   ├── ISW/             # ISW widget headers
│   │   ├── Box.h
│   │   └── ...
│   └── X11/             # X Toolkit Intrinsics headers
│       ├── Intrinsic.h
│       └── ...
├── util/
│   ├── makestrs.c       # String generation utility
│   └── string.list      # String definitions
└── examples/
    ├── isw_demo.c       # Demo application
    └── ...
```

### Adding New Source Files

1. Modify source files in `src/`
2. Update headers in `include/ISW/` or `include/X11/`
3. Add new source files to the appropriate list in `CMakeLists.txt`
4. Rebuild: `cmake --build build -j$(nproc)`

### Debugging Build Issues

Enable verbose output:
```bash
cmake --build build -j$(nproc) -- VERBOSE=1
```

Check preprocessor output:
```bash
gcc -E src/MyFile.c $(pkg-config --cflags isw) | less
```

---

**REMEMBER**: ISW is a complete XCB-based widget toolkit with embedded X Toolkit Intrinsics. No external libXt or Xlib dependencies are required.
