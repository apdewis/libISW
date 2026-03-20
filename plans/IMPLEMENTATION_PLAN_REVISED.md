# Isw3d XCB Migration - Revised Implementation Plan

**Date:** 2026-03-03  
**Based on User Decisions:**
- ✅ Use `xcb_xfixes_region_t` (scalar) for all region handling
- ✅ XCB core fonts only (no Xft initially)
- ✅ No separate infrastructure files (inline implementations)
- ✅ Remove Label2.c and all phantom file references

---

## Phase 1: Cleanup and Preparation

### Step 1.1: Remove Non-Existent File References

**File:** [`src/Makefile.am`](../src/Makefile.am)

**Remove these lines from `libIsw3d_la_SOURCES`:**
```makefile
IswXcbCompat.c \
IswXcbCompat.h \
IswXcbTypes.h \
IswRegion.c \
IswRegion.h \
IswXftCompat.c \
IswXftCompat.h \
IswUtils.c \
IswUtils.h \
IswConverters.c
```

**Keep only:**
```makefile
IswXcbDraw.c \
IswXcbDraw.h \
```

### Step 1.2: Remove Include Directives from Source Files

**Search and remove from ALL .c files:**
```c
#include "IswUtils.h"
#include "IswXcbCompat.h"
#include "IswXcbTypes.h"
#include "IswXftCompat.h"
#include "IswRegion.h"
#include "IswConverters.h"
```

**Affected files (from analysis):**
- src/AllWidgets.c
- src/AsciiSrc.c
- src/Box.c
- src/Command.c
- src/Dialog.c
- src/Form.c
- src/Layout.c
- src/List.c
- src/MultiSink.c
- src/MultiSrc.c
- src/Paned.c
- src/Panner.c
- src/Porthole.c
- src/Scrollbar.c
- src/Simple.c
- src/SimpleMenu.c
- src/SmeBSB.c
- src/Text.c
- src/TextAction.c
- src/TextPop.c
- src/TextSrc.c
- src/ThreeD.c
- src/Tip.c
- src/Toggle.c
- src/Vendor.c
- src/Viewport.c
- src/IswAtoms.c
- src/IswDrawing.c

**Action:** Use sed or manual search/replace to remove all these includes.

### Step 1.3: Check for Label2.c References

**Investigate generated Makefile:**
```bash
cd /home/adam/Isw3d/src
grep -n "Label2" Makefile
```

**If Label2 references found:**
- Check if it's a typo/remnant of Label.c
- Verify Makefile.am only lists Label.c once
- Run `./autogen.sh` to regenerate build system
- Reconfigure: `./configure CPPFLAGS="-I/home/adam/libxt/include" LDFLAGS="-L/home/adam/libxt/src/.libs"`

**Expected result:** Only Label.c should be referenced.

---

## Phase 2: Region Type Unification

### Step 2.1: Update Widget Class Structures

**File:** [`include/X11/Isw3d/ThreeDP.h`](../include/X11/Isw3d/ThreeDP.h)

**Verify shadowdraw signature is:**
```c
typedef struct {
    void (*shadowdraw)(Widget, xcb_generic_event_t *, xcb_xfixes_region_t, XtRelief, Boolean);
} ThreeDClassPart;
```

**Action:** This should already be correct per analysis.

### Step 2.2: Update All Widget Redisplay/Expose Callbacks

**Pattern to find:**
```c
static void Redisplay(Widget w, XEvent *event, Region region)
static void Redisplay(Widget w, xcb_generic_event_t *event, Region region)
```

**Replace with:**
```c
static void Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
```

**Files to update:**
- src/Label.c - Redisplay()
- src/Command.c - Redisplay()
- src/Simple.c - Redisplay()
- src/SmeBSB.c - Redisplay()
- src/List.c - Redisplay()
- src/Text.c - ProcessExposeRegion()
- src/ThreeD.c - _ISWDrawShadows()
- Any other widgets with Redisplay/Expose methods

**Also update forward declarations:**
```c
// OLD:
static void Redisplay(Widget, XEvent *, Region);
static void Redisplay(Widget, xcb_generic_event_t *, Region);

// NEW:
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
```

### Step 2.3: Update Region Usage in Function Bodies

**For region checks:**
```c
// OLD:
if (region == NULL) { ... }
if (XRectInRegion(region, x, y, width, height)) { ... }

// NEW:
if (region == 0) { ... }
// NOTE: xcb_xfixes_fetch_region() needed for actual region intersection
// For now, pass 0 (no clipping) to shadowdraw and drawing functions
```

**For shadowdraw calls:**
```c
// OLD:
(*lwclass->threeD_class.shadowdraw)(gw, event, region, relief, out);

// NEW:  
(*lwclass->threeD_class.shadowdraw)(gw, event, region, relief, out);
// Should work if region is xcb_xfixes_region_t
```

**Note on XRectInRegion:**
This Xlib function doesn't exist in XCB. For now:
```c
// TEMPORARY: Assume region intersects (always redraw)
// if (XRectInRegion(region, x, y, width, height)) {
if (1) {  // Always true for now
    // ... drawing code ...
}
```

**Future XFixes-based solution (for later):**
```c
xcb_xfixes_fetch_region_cookie_t cookie;
xcb_xfixes_fetch_region_reply_t *reply;
cookie = xcb_xfixes_fetch_region(conn, region);
reply = xcb_xfixes_fetch_region_reply(conn, cookie, NULL);
// Check rectangles in reply
free(reply);
```

---

## Phase 3: Font Operations Migration

### Step 3.1: XFontStruct max_bounds Access

**Problem:** Custom libXt's XFontStruct lacks max_bounds member.

**Solution:** Use XCB font queries:

**Find pattern:**
```c
XFontStruct *fs = widget->label.font;
int height = fs->max_bounds.ascent + fs->max_bounds.descent;
Position baseline = y + fs->max_bounds.ascent;
```

**Replace with:**
```c
XFontStruct *fs = widget->label.font;
// Query font properties from server
xcb_connection_t *conn = XtDisplay(widget);
xcb_query_font_cookie_t cookie;
xcb_query_font_reply_t *font_info;

cookie = xcb_query_font(conn, fs->fid);
font_info = xcb_query_font_reply(conn, cookie, NULL);

if (font_info) {
    int ascent = font_info->font_ascent;
    int descent = font_info->font_descent;
    int height = ascent + descent;
    Position baseline = y + ascent;
    
    free(font_info);
} else {
    // Fallback if query fails
    int height = 12;  // Default
    Position baseline = y + 10;
}
```

**Note:** This queries server every time. Cache results in widget structure for performance.

### Step 3.2: XTextWidth Replacement

**Find pattern:**
```c
int width = XTextWidth(fs, text, len);
```

**Replace with:**
```c
int width;
xcb_connection_t *conn = XtDisplay(widget);
xcb_query_text_extents_cookie_t cookie;
xcb_query_text_extents_reply_t *extents;

// Convert text to xcb_char2b_t array
xcb_char2b_t *chars = malloc(len * sizeof(xcb_char2b_t));
for (int i = 0; i < len; i++) {
    chars[i].byte1 = 0;
    chars[i].byte2 = (unsigned char)text[i];
}

cookie = xcb_query_text_extents(conn, fs->fid, len, chars);
extents = xcb_query_text_extents_reply(conn, cookie, NULL);

if (extents) {
    width = extents->overall_width;
    free(extents);
} else {
    width = len * 8;  // Fallback estimate
}

free(chars);
```

**Optimization:** Create helper function to avoid code duplication:
```c
/* Add to src/IswXcbDraw.c */
int ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len)
{
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t *extents;
    int width;
    
    xcb_char2b_t *chars = malloc(len * sizeof(xcb_char2b_t));
    for (int i = 0; i < len; i++) {
        chars[i].byte1 = 0;
        chars[i].byte2 = (unsigned char)text[i];
    }
    
    cookie = xcb_query_text_extents(conn, font, len, chars);
    extents = xcb_query_text_extents_reply(conn, cookie, NULL);
    
    if (extents) {
        width = extents->overall_width;
        free(extents);
    } else {
        width = len * 8;  // Fallback
    }
    
    free(chars);
    return width;
}
```

**Add declaration to [`src/IswXcbDraw.h`](../src/IswXcbDraw.h):**
```c
int ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len);
```

**Then use in widgets:**
```c
#include "IswXcbDraw.h"

int width = ISWXcbTextWidth(XtDisplay(widget), widget->label.font->fid,
                             text, len);
```

### Step 3.3: XDrawString Replacement

**Find pattern:**
```c
XDrawString(dpy, win, gc, x, y, text, len);
```

**Replace with:**
```c
xcb_connection_t *conn = XtDisplay(widget);
xcb_window_t win = XtWindow(widget);
xcb_gcontext_t gc = widget->label.normal_GC;

xcb_image_text_8(conn, len, win, gc, x, y, text);
xcb_flush(conn);
```

**Note:** `xcb_image_text_8()` draws text with background (like XDrawImageString).
For text without background, use `xcb_poly_text_8()` (more complex).

### Step 3.4: XTextWidth16 and XDrawString16 (16-bit text)

**Problem:** XChar2b type doesn't exist, and 16-bit text functions are complex.

**Short-term solution:** Disable 16-bit text support:
```c
// In widgets that use encoding:
#if 0  // DISABLE 16-bit text for now
if (widget->label.encoding) {
    width = XTextWidth16(fs, (TXT16*)label, len/2);
    XDrawString16(dpy, win, gc, x, y, (TXT16*)label, len/2);
}
#else
{
    // Force 8-bit text
    width = ISWXcbTextWidth(conn, font, label, len);
    xcb_image_text_8(conn, len, win, gc, x, y, label);
    xcb_flush(conn);
}
#endif
```

**Long-term solution (for later Xft migration):**
- Xft handles all character encodings via FreeType
- XftDrawStringUtf8() will replace both 8-bit and 16-bit text

### Step 3.5: XFontSet Functions (Internationalization)

**Problem:** XmbTextEscapement, XmbDrawString don't exist in XCB.

**Short-term solution:** Stub out internationalization:
```c
#ifdef ISW_INTERNATIONALIZATION
if (widget->simple.international == True) {
    // TEMPORARY: Disable XFontSet rendering
    // Fall back to single-byte font
    #if 0
    XFontSet fset = widget->label.fontset;
    XFontSetExtents *ext = XExtentsOfFontSet(fset);
    int width = XmbTextEscapement(fset, label, len);
    XmbDrawString(dpy, win, fset, gc, x, y, label, len);
    #else
    // Use single-byte font instead
    int width = ISWXcbTextWidth(conn, widget->label.font->fid, label, len);
    xcb_image_text_8(conn, len, win, gc, x, y, label);
    xcb_flush(conn);
    #endif
}
#endif
```

**Long-term solution (document for Xft migration):**
- Create ISWFontSet wrapper structure
- Wrap Xft fonts in ISWFontSet
- Implement IswTextWidth(), IswDrawString() that use Xft internally
- See Phase 7 for migration guide

---

## Phase 4: GC Operations Migration

### Step 4.1: Replace XGCValues Structure

**Find pattern:**
```c
XGCValues values;
values.foreground = pixel;
values.background = bg_pixel;
values.font = font->fid;
values.graphics_exposures = False;

GC gc = XtGetGC(widget, 
                GCForeground | GCBackground | GCFont | GCGraphicsExposures,
                &values);
```

**Replace with:**
```c
xcb_create_gc_value_list_t values;
memset(&values, 0, sizeof(values));
values.foreground = pixel;
values.background = bg_pixel;
values.font = font->fid;
values.graphics_exposures = 0;  // False

uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | 
                XCB_GC_FONT | XCB_GC_GRAPHICS_EXPOSURES;

xcb_gcontext_t gc = XtGetGC(widget, mask, &values);
```

**Files to update:**
- src/Label.c - GetnormalGC(), GetgrayGC()
- Any other widget files with GC creation

**Note on XtGetGC vs XtAllocateGC:**
Both should accept `xcb_create_gc_value_list_t*` in custom libXt.
If XtAllocateGC signature is different, check custom libXt headers.

### Step 4.2: IswCreateStippledPixmap Call

**Find pattern:**
```c
values.tile = IswCreateStippledPixmap(XtScreen((Widget)lw),
                                      lw->label.foreground,
                                      lw->core.background_pixel,
                                      lw->core.depth);
```

**Issue:** IswCreateStippledPixmap might not exist or might return wrong type.

**Check if it's defined in:** Xmu library or custom implementation needed.

**If undefined, create bitmap manually:**
```c
static const char stipple_bits[] = {
    0x02, 0x01  // Simple 2x2 stipple pattern
};

xcb_connection_t *conn = XtDisplay((Widget)lw);
xcb_screen_t *screen = XtScreen((Widget)lw);
xcb_pixmap_t stipple;

stipple = xcb_generate_id(conn);
xcb_create_pixmap(conn, 1, stipple, screen->root, 2, 2);

// Upload bitmap data
xcb_put_image(conn, XCB_IMAGE_FORMAT_XY_BITMAP, stipple,
              lw->label.normal_GC, 2, 2, 0, 0, 0, 1,
              2, (uint8_t*)stipple_bits);
xcb_flush(conn);

values.tile = stipple;
```

---

## Phase 5: Geometry Operations Migration

### Step 5.1: Replace XGetGeometry

**Find pattern:**
```c
Window root;
int x, y;
unsigned int width, height, border_width, depth;

if (XGetGeometry(XtDisplay(lw), lw->label.pixmap, &root, &x, &y,
                 &width, &height, &border_width, &depth)) {
    lw->label.label_height = height;
    lw->label.label_width = width;
    lw->label.depth = depth;
}
```

**Replace with:**
```c
xcb_connection_t *conn = XtDisplay(lw);
xcb_get_geometry_cookie_t cookie;
xcb_get_geometry_reply_t *reply;
xcb_generic_error_t *error = NULL;

cookie = xcb_get_geometry(conn, lw->label.pixmap);
reply = xcb_get_geometry_reply(conn, cookie, &error);

if (reply != NULL) {
    lw->label.label_height = reply->height;
    lw->label.label_width = reply->width;
    lw->label.depth = reply->depth;
    free(reply);
} else {
    if (error) {
        // Handle error
        free(error);
    }
}
```

**Files to update:**
- src/Label.c - SetTextWidthAndHeight(), set_bitmap_info()
- Any other files using XGetGeometry

---

## Phase 6: Update src/IswXcbDraw.c/h

### Step 6.1: Add Helper Functions

**Add to [`src/IswXcbDraw.h`](../src/IswXcbDraw.h):**
```c
/*
 * XCB Core Font Helpers (non-Xft)
 */

/* Text width calculation using xcb_query_text_extents */
int ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len);

/* Font metrics query using xcb_query_font */
typedef struct {
    int ascent;
    int descent;
    int max_char_width;
} ISWFontMetrics;

void ISWXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                            ISWFontMetrics *metrics);

/* Text drawing using xcb_image_text_8 */
void ISWXcbDrawText(xcb_connection_t *conn, xcb_drawable_t d,
                    xcb_gcontext_t gc, int16_t x, int16_t y,
                    const char *text, uint8_t len);
```

**Implement in [`src/IswXcbDraw.c`](../src/IswXcbDraw.c):**
```c
int ISWXcbTextWidth(xcb_connection_t *conn, xcb_font_t font,
                    const char *text, int len)
{
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t *extents;
    int width;
    xcb_char2b_t *chars;
    int i;
    
    if (len == 0) return 0;
    
    chars = malloc(len * sizeof(xcb_char2b_t));
    if (!chars) return 0;
    
    for (i = 0; i < len; i++) {
        chars[i].byte1 = 0;
        chars[i].byte2 = (unsigned char)text[i];
    }
    
    cookie = xcb_query_text_extents(conn, font, len, chars);
    extents = xcb_query_text_extents_reply(conn, cookie, NULL);
    
    if (extents) {
        width = extents->overall_width;
        free(extents);
    } else {
        width = len * 8;  /* Fallback estimate */
    }
    
    free(chars);
    return width;
}

void ISWXcbQueryFontMetrics(xcb_connection_t *conn, xcb_font_t font,
                            ISWFontMetrics *metrics)
{
    xcb_query_font_cookie_t cookie;
    xcb_query_font_reply_t *font_info;
    
    cookie = xcb_query_font(conn, font);
    font_info = xcb_query_font_reply(conn, cookie, NULL);
    
    if (font_info) {
        metrics->ascent = font_info->font_ascent;
        metrics->descent = font_info->font_descent;
        metrics->max_char_width = font_info->max_bounds.character_width;
        free(font_info);
    } else {
        /* Fallback values */
        metrics->ascent = 10;
        metrics->descent = 2;
        metrics->max_char_width = 8;
    }
}

void ISWXcbDrawText(xcb_connection_t *conn, xcb_drawable_t d,
                    xcb_gcontext_t gc, int16_t x, int16_t y,
                    const char *text, uint8_t len)
{
    if (len > 0) {
        xcb_image_text_8(conn, len, d, gc, x, y, text);
        xcb_flush(conn);
    }
}
```

---

## Phase 7: Future Xft Migration Guide

**Document for later migration to Xft-based text rendering.**

Create file: [`plans/XFT_MIGRATION_GUIDE.md`](XFT_MIGRATION_GUIDE.md)

### Overview

Xft provides high-quality anti-aliased text rendering with full Unicode support.
Migration from XCB core fonts to Xft involves:

1. Wrapping Xft fonts in ISWFontSet structure
2. Replacing ISWXcbTextWidth with XftTextExtentsUtf8
3. Replacing ISWXcbDrawText with XftDrawStringUtf8
4. Creating XftDraw contexts for widgets

### ISWFontSet Structure

```c
/* To be added to a new IswXftCompat.h */
typedef struct _IswFontSet {
    XftFont *xft_font;        /* Xft font handle */
    int ascent;               /* Cached metrics */
    int descent;
    int height;
    int max_char_width;
} ISWFontSet;
```

### Font Loading

**Current (XCB core fonts):**
```c
XFontStruct *font = XLoadQueryFont(dpy, fontname);
```

**Future (Xft):**
```c
ISWFontSet *fontset = IswLoadFontSet(dpy, screen, fontname);

/* Implementation: */
ISWFontSet *IswLoadFontSet(Display *dpy, Screen *screen, const char *name)
{
    ISWFontSet *fs = malloc(sizeof(ISWFontSet));
    fs->xft_font = XftFontOpenName(dpy, screen->screen_num, name);
    if (fs->xft_font) {
        fs->ascent = fs->xft_font->ascent;
        fs->descent = fs->xft_font->descent;
        fs->height = fs->ascent + fs->descent;
        fs->max_char_width = fs->xft_font->max_advance_width;
    }
    return fs;
}
```

### Text Width

**Current (XCB):**
```c
int width = ISWXcbTextWidth(conn, font, text, len);
```

**Future (Xft):**
```c
int width = IswTextWidth(fontset, text, len);

/* Implementation: */
int IswTextWidth(ISWFontSet *fs, const char *text, int len)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, fs->xft_font, (FcChar8*)text, len, &extents);
    return extents.xOff;
}
```

### Text Drawing

**Current (XCB):**
```c
ISWXcbDrawText(conn, win, gc, x, y, text, len);
```

**Future (Xft):**
```c
IswDrawString(widget, fontset, color, x, y, text, len);

/* Implementation: */
void IswDrawString(Widget w, ISWFontSet *fs, Pixel pixel,
                   int x, int y, const char *text, int len)
{
    xcb_connection_t *conn = XtDisplay(w);
    xcb_window_t win = XtWindow(w);
    XftDraw *draw;
    XftColor xft_color;
    
    /* Create XftDraw if not cached */
    draw = XftDrawCreate(/* ... */);
    
    /* Convert Pixel to XftColor */
    /* ... color conversion ... */
    
    XftDrawStringUtf8(draw, &xft_color, fs->xft_font,
                      x, y, (FcChar8*)text, len);
}
```

### Widget Integration

**Changes needed in widget structures:**
```c
/* In LabelP.h and similar: */
typedef struct {
    /* ... */
#ifdef ISW_USE_XFT
    ISWFontSet *fontset;
#else
    XFontStruct *font;
#endif
    /* ... */
} LabelPart;
```

**Configuration option:**
```makefile
# In configure.ac:
AC_ARG_ENABLE([xft],
    AS_HELP_STRING([--enable-xft], [Use Xft for text rendering (default: no)]),
    [ISW_USE_XFT=$enableval], [ISW_USE_XFT=no])
if test "x$ISW_USE_XFT" = xyes; then
    PKG_CHECK_MODULES(XFT, xft)
    AC_DEFINE(ISW_USE_XFT, 1, [Use Xft for rendering])
fi
```

---

## Testing Strategy

### Test After Each Phase

**Phase 1 (Cleanup):**
```bash
./autogen.sh
./configure CPPFLAGS="-I/home/adam/libxt/include" LDFLAGS="-L/home/adam/libxt/src/.libs"
make 2>&1 | tee build.log
# Should have fewer "file not found" errors
```

**Phase 2 (Region types):**
```bash
make src/Label.lo 2>&1 | grep -i region
# Should not show region type conflicts
```

**Phase 3 (Font operations):**
```bash
make src/Label.lo 2>&1 | grep -E "(XTextWidth|XDrawString|max_bounds)"
# Should not show these errors
```

**Phase 4 (GC operations):**
```bash
make src/Label.lo 2>&1 | grep -i "XGCValues"
# Should not show XGCValues errors
```

**Phase 5 (Geometry):**
```bash
make src/Label.lo 2>&1 | grep -i "XGetGeometry"
# Should not show XGetGeometry errors
```

**Full build:**
```bash
make 2>&1 | tee build.log
# Check for remaining errors
ldd src/.libs/libIsw3d.so | grep -i xlib
# Should be empty (no Xlib dependency)
ldd src/.libs/libIsw3d.so | grep xcb
# Should show xcb libraries
```

---

## Implementation Order

### Priority 1 (Build must complete):
1. Phase 1 - Remove non-existent files
2. Phase 2 - Fix region types
3. Phase 6 - Add IswXcbDraw helpers
4. Phase 3 - Fix font operations in Label.c

### Priority 2 (Get one widget working):
5. Phase 4 - Fix GC operations in Label.c
6. Phase 5 - Fix geometry queries in Label.c
7. Test Label widget compiles

### Priority 3 (Complete migration):
8. Apply Phase 3-5 fixes to remaining widgets
9. Test full build
10. Runtime testing with sample program

### Priority 4 (Documentation):
11. Phase 7 - Write Xft migration guide
12. Update BUILD_INSTRUCTIONS.md
13. Update PORTING_GUIDELINES.md with actual patterns used

---

## Success Criteria

- [x] No missing file errors in build
- [x] No region type conflicts
- [x] No undefined font functions (XTextWidth, XDrawString)
- [x] No XGCValues structure errors
- [x] Label.c compiles successfully
- [ ] All widget .c files compile
- [ ] libIsw3d.so links successfully
- [ ] No libX11 dependency (only libxcb)
- [ ] Simple test program runs

---

## Notes

- Keep changes minimal and focused per phase
- Test incrementally after each change
- Document any deviations from plan
- Cache font metrics in widget structures for performance
- Consider performance optimization after basic functionality works
- Xft migration is Phase 2 of project (after core font XCB works)

---

**End of Implementation Plan**
