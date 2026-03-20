# Xaw3d to ISW Renaming Plan

## Executive Summary

This document outlines the comprehensive plan to rename the Xaw3d library to ISW (Isometric Software Widgets). This is a substantial fork that requires renaming throughout the codebase while maintaining build stability.

## Naming Analysis

### Current Naming Patterns

####  Library Names
- **Library:** `libXaw3d.so` / `libXaw3d.la`
- **Package Config:** `xaw3d.pc`
- **Project Name:** `libXaw3d`

#### Include Paths
- **Headers Location:** `include/X11/Xaw3d/`
- **Install Path:** `${includedir}/X11/Xaw3d/`
- **Include Pattern:** `#include <X11/Xaw3d/Widget.h>`

#### Function Prefixes
- **Public API:** `Xaw*` (e.g., `ISWQueryPointer`, `ISWCreateRegion`)
- **Internal:** `_Xaw*` (e.g., `_ISW_iswspace`)
- **XCB-specific:** `XawXcb*` (e.g., `ISWXcbDrawString`)
- **Utility:** `XawInit*`, `XawSet*`, `XawGet*`

#### Widget Class Names
- **Prefix:** None (e.g., `Box`, `Command`, `Scrollbar`)
- **Resource Names:** `Xt*` (X Toolkit standard)

#### C Preprocessor Defines
- **Feature Flags:** `XAW_*` (e.g., `ISW_ARROW_SCROLLBARS`, `ISW_INTERNATIONALIZATION`)
- **Header Guards:** `_*_h` (e.g., `_Scrollbar_h`, `_ISWP_h`)

## Proposed ISW Naming Conventions

### 1. Library Names
```
OLD: libXaw3d.so.8.0.0
NEW: libISW.so.1.0.0

OLD: xaw3d.pc
NEW: isw.pc
```

### 2. Include Paths
```
OLD: #include <X11/Xaw3d/Scrollbar.h>
NEW: #include <ISW/Scrollbar.h>

OLD: ${includedir}/X11/Xaw3d/
NEW: ${includedir}/ISW/
```

### 3. Function Prefixes
```
OLD: ISWQueryPointer()
NEW: ISWQueryPointer()

OLD: ISWXcbDrawString()
NEW: ISWXcbDrawString()

OLD: _ISW_iswspace()
NEW: _ISW_iswspace()
```

### 4. Widget Class Names
**KEEP UNCHANGED** - Widget names like `Box`, `Command`, `Scrollbar` remain as-is for X11 compatibility

### 5. Resource Names
**KEEP UNCHANGED** - `XtN*` and `XtC*` are X Toolkit standards and should not be changed

### 6. C Preprocessor Defines
```
OLD: ISW_ARROW_SCROLLBARS
NEW: ISW_ARROW_SCROLLBARS

OLD: ISW_INTERNATIONALIZATION  
NEW: ISW_INTERNATIONALIZATION

OLD: _Scrollbar_h
NEW: _ISW_Scrollbar_h
```

### 7. Internal Symbols
```
OLD: Xaw3dP.h / Xaw3dP.c
NEW: ISWP.h / ISWP.c

OLD: XawInit.h / XawInit.c
NEW: ISWInit.h / ISWInit.c
```

## Renaming Strategy

### Phase 1: Analysis & Preparation
**Status:** Current Phase

**Tasks:**
1. ✅ Document current naming patterns
2. ✅ Define new naming conventions
3. [ ] Create automated renaming scripts
4. [ ] Create backup of current codebase
5. [ ] Update TODO list with all renaming tasks

### Phase 2: Build System Updates
**Estimated Files:** 10 files

**Files to Update:**
- `configure.ac` - Package name, version, variables
- `Makefile.am` - Package config filename
- `xaw3d.pc.in` → `isw.pc.in` - Rename and update contents
- `src/Makefile.am` - Library name, sources
- `include/Makefile.am` - Include directory path

**Changes:**
```makefile
# configure.ac
OLD: AC_INIT([libXaw3d], [1.6.2], ...)
NEW: AC_INIT([libISW], [1.0.0], ...)

OLD: ISW_CPPFLAGS
NEW: ISW_CPPFLAGS

# src/Makefile.am
OLD: lib_LTLIBRARIES = libXaw3d.la
NEW: lib_LTLIBRARIES = libISW.la

OLD: libXaw3d_la_SOURCES =
NEW: libISW_la_SOURCES =
```

### Phase 3: Header File Renaming
**Estimated Files:** 90+ header files

**Directory Structure Change:**
```
OLD: include/X11/Xaw3d/*.h
NEW: include/ISW/*.h
```

**Header Guard Updates:**
All headers need their include guards updated:
```c
OLD: #ifndef _Scrollbar_h
     #define _Scrollbar_h
NEW: #ifndef _ISW_Scrollbar_h
     #define _ISW_Scrollbar_h
```

**Internal Headers:**
```
include/X11/Xaw3d/Xaw3dP.h    → include/ISW/ISWP.h
include/X11/Xaw3d/XawInit.h   → include/ISW/ISWInit.h
include/X11/Xaw3d/XawImP.h    → include/ISW/ISWImP.h
include/X11/Xaw3d/XawContext.h → include/ISW/ISWContext.h
```

### Phase 4: Source File Function Renaming
**Estimated Files:** 70+ source files

**Function Prefix Changes:**
```c
// Public API functions
ISWQueryPointer()      → ISWQueryPointer()
ISWCreateRegion()      → ISWCreateRegion()
ISWFreePixmap()        → ISWFreePixmap()
ISWAllocColor()        → ISWAllocColor()

// XCB-specific functions  
ISWXcbDrawString()     → ISWXcbDrawString()
ISWXcbTextWidth()      → ISWXcbTextWidth()
ISWXcbQueryFontMetrics() → ISWXcbQueryFontMetrics()

// Internal functions
_ISW_iswspace()        → _ISW_iswspace()
_ISWTextMBToWC()       → _ISWTextMBToWC()
```

**Files Requiring Updates:**
- `src/XawXcbDraw.c` → `src/ISWXcbDraw.c` (rename file + all functions)
- `src/XawXcbDraw.h` → `src/ISWXcbDraw.h` (rename file + all declarations)
- `src/XawInit.c` → `src/ISWInit.c`
- `src/XawContext.c` → `src/ISWContext.c`
- `src/XawAtoms.c` → `src/ISWAtoms.c`
- `src/XawDrawing.c` → `src/ISWDrawing.c`
- `src/XawIm.c` → `src/ISWIm.c`
- `src/XawI18n.c` → `src/ISWI18n.c`
- `src/Xaw3dP.c` → `src/ISWP.c`

**All Widget Source Files:**
Every widget source file that calls `Xaw*` functions must be updated to call `ISW*` functions.

### Phase 5: Preprocessor Define Updates
**Estimated Occurrences:** 50+ locations

**Feature Flags:**
```c
OLD: #ifdef ISW_ARROW_SCROLLBARS
NEW: #ifdef ISW_ARROW_SCROLLBARS

OLD: #ifdef ISW_INTERNATIONALIZATION  
NEW: #ifdef ISW_INTERNATIONALIZATION

OLD: #ifdef ISW_HAS_XIM
NEW: #ifdef ISW_HAS_XIM

OLD: #ifdef ISW_MULTIPLANE_PIXMAPS
NEW: #ifdef ISW_MULTIPLANE_PIXMAPS

OLD: #ifdef ISW_GRAY_BLKWHT_STIPPLES
NEW: #ifdef ISW_GRAY_BLKWHT_STIPPLES
```

**Build System Updates:**
```bash
# configure.ac
OLD: ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_ARROW_SCROLLBARS"
NEW: ISW_CPPFLAGS="${ISW_CPPFLAGS} -DISW_ARROW_SCROLLBARS"
```

### Phase 6: Include Statement Updates
**Estimated Files:** 70+ files

**Pattern Replacement:**
```c
OLD: #include <X11/Xaw3d/Scrollbar.h>
NEW: #include <ISW/Scrollbar.h>

OLD: #include "XawXcbDraw.h"
NEW: #include "ISWXcbDraw.h"

OLD: #include "../include/X11/Xaw3d/ThreeD.h"
NEW: #include "../include/ISW/ThreeD.h"
```

### Phase 7: Documentation Updates
**Estimated Files:** 15+ markdown files

**Files to Update:**
- `README` - Project description
- `BUILD_INSTRUCTIONS.md` - Build references
- `plans/*.md` - All planning documents
- `examples/README.md` - Demo documentation
- Source code comments referencing "Xaw3d"

**Example Changes:**
```markdown
OLD: # Xaw3d XCB Migration Build Instructions
NEW: # ISW (Isometric Software Widgets) Build Instructions

OLD: ldd src/.libs/libXaw3d.so
NEW: ldd src/.libs/libISW.so
```

### Phase 8: Demo Application Updates
**Files:** `examples/` directory

**Changes Required:**
```c
// Include statements
OLD: #include <X11/Xaw3d/Scrollbar.h>
NEW: #include <ISW/Scrollbar.h>

// Demo file names
examples/xaw3d_demo.c    → examples/isw_demo.c
examples/xaw3d_demo_3d.c → examples/isw_demo_3d.c

// Executable names  
xaw3d_demo     → isw_demo
xaw3d_demo_3d  → isw_demo_3d
```

## Backward Compatibility Strategy

### Option A: No Compatibility (Recommended for Clean Fork)
- Complete break from Xaw3d
- Simplest approach
- Clear distinction as a separate project

### Option B: Compatibility Headers (If Needed Later)
Create compatibility shim headers:
```c
// X11/Xaw3d/Scrollbar.h (compatibility)
#warning "X11/Xaw3d headers are deprecated, use <ISW/Scrollbar.h>"
#include <ISW/Scrollbar.h>

// Function aliases
#define ISWQueryPointer ISWQueryPointer
#define ISWCreateRegion ISWCreateRegion
```

**Recommendation:** Start with Option A, add compatibility only if external users need it.

## Implementation Checklist

### Pre-Renaming
- [ ] Create full backup of codebase
- [ ] Create git branch for renaming
- [ ] Write automated sed/awk scripts for bulk renaming
- [ ] Test scripts on small subset

### Phase 1: Build System (Day 1)
- [ ] Rename `xaw3d.pc.in` to `isw.pc`
- [ ] Update `configure.ac` - project name, version, variables
- [ ] Update `Makefile.am` - pkg-config filename  
- [ ] Update `src/Makefile.am` - library name
- [ ] Update `include/Makefile.am` - include path
- [ ] Test: `./autogen.sh && ./configure` works

### Phase 2: Directory Restructure (Day 1-2)
- [ ] Move `include/X11/Xaw3d/` to `include/ISW/`
- [ ] Update all #include statements in source files
- [ ] Update include guards in all headers
- [ ] Test: Headers can be found

### Phase 3: File Renaming (Day 2)
- [ ] Rename Xaw*.h files to ISW*.h
- [ ] Rename Xaw*.c files to ISW*.c
- [ ] Update Makefile.am with new filenames
- [ ] Test: `make` compiles (with errors expected)

### Phase 4: Symbol Renaming (Day 2-3)
- [ ] Rename all `Xaw*` functions to `ISW*`
- [ ] Rename all `_Xaw*` internals to `_ISW*`
- [ ] Update all function calls throughout codebase
- [ ] Test: `make` compiles without errors

### Phase 5: Preprocessor Defines (Day 3)
- [ ] Rename `XAW_*` to `ISW_*` in all files
- [ ] Update configure.ac feature flags
- [ ] Test: Feature flags work correctly

### Phase 6: Demo Apps (Day 3)
- [ ] Rename demo source files
- [ ] Update demo #includes
- [ ] Update demo Makefile
- [ ] Test: Demos compile and run

### Phase 7: Documentation (Day 4)
- [ ] Update all README files
- [ ] Update plans/*.md documents
- [ ] Update code comments
- [ ] Test: Documentation is consistent

### Phase 8: Final Testing (Day 4-5)
- [ ] Full clean build: `make distclean && ./autogen.sh && ./configure && make`
- [ ] Run all demos
- [ ] Check library dependencies: `ldd src/.libs/libISW.so`
- [ ] Verify no Xaw3d references remain (except comments)
- [ ] Test installation: `make install`

## Automated Renaming Scripts

### Script 1: Bulk File Renaming
```bash
#!/bin/bash
# rename_files.sh

# Rename source files
for file in src/Xaw*.c src/Xaw*.h; do
    if [ -f "$file" ]; then
        newname=$(echo "$file" | sed 's/Xaw/ISW/g')
        git mv "$file" "$newname"
    fi
done

# Rename headers
find include/X11/Xaw3d -name "*.h" | while read file; do
    newname=$(echo "$file" | sed 's/Xaw3d/ISW/g' | sed 's/X11\/Xaw3d/ISW/g')
    mkdir -p "$(dirname "$newname")"
    git mv "$file" "$newname"
done
```

### Script 2: Symbol Replacement
```bash
#!/bin/bash
# rename_symbols.sh

# Function names
find . -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i \
    -e 's/\bXawQueryPointer\b/ISWQueryPointer/g' \
    -e 's/\bXawCreateRegion\b/ISWCreateRegion/g' \
    -e 's/\bXawXcbDraw/ISWXcbDraw/g' \
    -e 's/\b_Xaw_/_ISW_/g' \
    {} \;

# Preprocessor defines
find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.ac" -o -name "*.am" \) -exec sed -i \
    -e 's/\bISW_ARROW_SCROLLBARS\b/ISW_ARROW_SCROLLBARS/g' \
    -e 's/\bISW_INTERNATIONALIZATION\b/ISW_INTERNATIONALIZATION/g' \
    -e 's/\bISW_CPPFLAGS\b/ISW_CPPFLAGS/g' \
    {} \;

# Include guards
find include/ISW -name "*.h" -exec sed -i \
    -e 's/^#ifndef _\(.*\)_h$/#ifndef _ISW_\1_h/' \
    -e 's/^#define _\(.*\)_h$/#define _ISW_\1_h/' \
    {} \;
```

### Script 3: Include Statement Updates
```bash
#!/bin/bash
# update_includes.sh

find . -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i \
    -e 's|<X11/Xaw3d/\(.*\)>|<ISW/\1>|g' \
    -e 's|"X11/Xaw3d/\(.*\)"|"ISW/\1"|g' \
    -e 's|"XawXcbDraw\.h"|"ISWXcbDraw.h"|g' \
    {} \;
```

## Risk Assessment

### High Risk Areas
1. **Widget Class Registration** - Widget class names used by X Toolkit
2. **Resource Converter Registration** - Resource type strings
3. **External Dependencies** - Custom libXt may have hard-coded Xaw3d references

### Mitigation Strategies
1. **Incremental Testing** - Test after each phase
2. **Git Branches** - Easy rollback if issues arise
3. **Compiler Warnings** - Fix all warnings before proceeding
4. **Dependency Audit** - Check custom libXt for Xaw3d dependencies

## Timeline Estimate

- **Preparation & Scripts:** 0.5 day
- **Build System Updates:** 0.5 day
- **Header Renaming:** 1 day
- **Source Renaming:** 1.5 days
- **Testing & Fixes:** 1.5 days
- **Documentation:** 0.5 day

**Total:** ~5-6 working days

## Success Criteria

- [ ] Library builds successfully as `libISW.so`
- [ ] All header files are in `include/ISW/`
- [ ] No references to "Xaw3d" in source code (except comments/history)
- [ ] Demo applications compile and run
- [ ] `pkg-config isw --cflags` works
- [ ] No Xlib dependencies (only XCB)
- [ ] Documentation updated consistently

## Notes

- Widget class names (Box, Command, Scrollbar) remain unchanged as they're X Toolkit standards
- Resource names (XtN*, XtC*) remain unchanged as they're X Toolkit standards
- Version number resets to 1.0.0 as this is a new fork
- Install path changes from `/usr/include/X11/Xaw3d` to `/usr/include/ISW`

---

**Document Version:** 1.0  
**Created:** 2026-03-20  
**Last Updated:** 2026-03-20
