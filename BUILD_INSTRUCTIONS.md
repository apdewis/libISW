# Xaw3d XCB Migration Build Instructions

## Critical Build Requirements

### 1. Use Custom XCB-Based libXt

**MANDATORY**: This project MUST use the custom XCB-based libXt located at `/home/adam/libXt`.

**DO NOT** use system-installed Xt headers from `/usr/include/X11/`.

### 2. Compiler Flags Required

The following compiler flags MUST be set to ensure the custom libXt is used:

```bash
CPPFLAGS="-I/home/adam/libxt/include"
LDFLAGS="-L/home/adam/libxt/src/.libs"
```

### 3. Configure Command

When running `./configure`, use:

```bash
./configure \
  CPPFLAGS="-I/home/adam/libxt/include" \
  LDFLAGS="-L/home/adam/libxt/src/.libs" \
  PKG_CONFIG_PATH="/home/adam/libXt:$PKG_CONFIG_PATH"
```

### 4. Makefile Verification

After running configure, verify that `src/Makefile` contains:

- `-I/home/adam/libxt/include` in CPPFLAGS or AM_CPPFLAGS
- `-L/home/adam/libxt/src/.libs` in LDFLAGS or AM_LDFLAGS

### 5. Header Include Order

In source files, the include order MUST be:

1. Local Xaw3d headers (e.g., `#include <X11/Xaw3d/Xaw3dP.h>`)
2. Custom libXt headers (automatically via -I flag)
3. XCB headers (e.g., `#include <xcb/xcb.h>`)
4. Standard C headers

**NEVER** include system Xlib headers like:
- `#include <X11/Xlib.h>` (use XCB instead)
- System `/usr/include/X11/Intrinsic.h` (use custom libXt)

### 6. Type Compatibility

The custom libXt provides XCB-compatible types:

- `Display` → XCB-backed Display structure
- `Window` → `xcb_window_t`
- `Pixmap` → `xcb_pixmap_t`
- `GC` → `xcb_gcontext_t`
- `XEvent` → XCB event structures
- `Region` → `xcb_xfixes_region_t`

Widget callback signatures use these XCB types, NOT Xlib types.

### 7. Xft/Font Rendering

**Current Status**: Xft dependency has been removed. Font rendering uses:

- XawFontSet wrapper structure (defined in `src/XawXftCompat.h`)
- XCB rendering primitives
- NO Xlib font functions

### 8. Common Pitfalls to Avoid

#### ❌ WRONG:
```c
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>  // System header
```

#### ✅ CORRECT:
```c
#include <X11/Intrinsic.h>  // From /home/adam/libXt/include
#include <xcb/xcb.h>
```

#### ❌ WRONG:
```c
static void Redisplay(Widget w, XEvent *event, Region region)
// Using Xlib types
```

#### ✅ CORRECT:
```c
static void Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
// Using XCB types from custom libXt
```

### 9. Dependency Chain

```
Xaw3d (this project)
  ↓
Custom libXt (/home/adam/libXt)
  ↓
XCB (libxcb, libxcb-xfixes, libxcb-render, etc.)
  ↓
X11 protocol
```

**NO Xlib in this chain!**

### 10. Build Verification

After building, verify no Xlib dependencies:

```bash
ldd src/.libs/libXaw3d.so | grep -i xlib
# Should return NOTHING

ldd src/.libs/libXaw3d.so | grep -i xcb
# Should show xcb libraries
```

### 11. Troubleshooting

If you see errors like:
- "conflicting types for 'XExtCodes'"
- "conflicting types for 'Visual'"
- "unknown type name 'xcb_xfixes_region_t'"

**Root Cause**: System Xlib headers are being included instead of custom libXt.

**Solution**:
1. Check compiler command for `-I/usr/include` (should NOT be there)
2. Verify `-I/home/adam/libXt/include` comes FIRST in include path
3. Check source files for explicit `#include <X11/Xlib.h>`
4. Ensure `configure.ac` sets proper CPPFLAGS

### 12. Makefile.am Configuration

The `src/Makefile.am` should include:

```makefile
AM_CPPFLAGS = -I$(top_srcdir)/include \
              -I/home/adam/libXt/include \
              $(XCB_CFLAGS)

libXaw3d_la_LIBADD = $(XCB_LIBS) \
                     -L/home/adam/libXt/src/.libs -lXt
```

### 13. Current Build Status

As of last update:
- ✅ XtJustify enum conflict resolved
- ✅ XawFontSet migration in progress
- ✅ List.c compiles with XCB types
- ⚠️  Paned.c needs XCB type fixes
- ⚠️  Need to verify all widget expose callbacks use XCB types

### 14. Next Steps

1. Ensure `configure.ac` properly sets CPPFLAGS for custom libXt
2. Regenerate build system: `./autogen.sh`
3. Configure with custom libXt: `./configure CPPFLAGS="-I/home/adam/libxt/include"`
4. Build and verify: `make && ldd src/.libs/libXaw3d.so`
5. Fix any remaining widget callbacks to use XCB types

---

**REMEMBER**: The goal is complete Xlib elimination. Every Xlib reference must be replaced with XCB equivalents from the custom libXt.
