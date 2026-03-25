# Font Type Refactor Plan

## Problem

The XCB port retained Xlib's `XFontStruct` as `XtFontStruct` — a compat shim carrying fields (`fid`, `min_byte1`, `max_byte1`, etc.) that the Cairo rendering backend never uses. The Cairo path resolves fonts via fontconfig and only needs family name, size, weight, and slant. The `font_family` field was bolted on as a workaround, but the right fix is replacing the type entirely.

## Target Type

```c
typedef struct {
    char   *family;    /* fontconfig family name ("Sans", "DejaVu Serif", etc.) */
    double  size;      /* point size */
    int     weight;    /* FC_WEIGHT_NORMAL, FC_WEIGHT_BOLD, etc. */
    int     slant;     /* FC_SLANT_ROMAN, FC_SLANT_ITALIC, etc. */
} ISWFont;
```

## Scope

Every reference to `XFontStruct *` / `XtFontStruct *` across the codebase needs updating:

### Widget resources (`XtNfont`)
- Label.c / LabelP.h — `font` field, GC creation, text measurement
- List.c / ListP.h — row height, text drawing
- SmeBSB.c / SmeBSBP.h — menu item text
- AsciiSink.c / AsciiSinkP.h — text widget rendering
- MultiSink.c / MultiSinkP.h — international text
- Scale.c / ScaleP.h — value label
- SpinBox.c — child text widget font
- FontChooser.c — preview font
- IconView.c / IconViewP.h — icon label font
- ProgressBar.c / ProgressBarP.h — percentage text
- Tip.c / TipP.h — tooltip text
- Toggle.c — indicator sizing from font metrics

### Render API
- ISWRender.h / ISWRender.c — `ISWRenderSetFont()`, `ISWScaledFontHeight()`, `ISWScaledFontAscent()`, `ISWScaledFontCapHeight()`
- ISWRenderCairoXCB.c — `cairo_xcb_set_font()`, `_ISWSetCairoFontFromXFont()`
- ISWRenderXCB.c — XCB font path (if retained as fallback)
- ISWRenderPrivate.h — vtable `set_font` signature

### Embedded Xt / infrastructure
- Converters.c — `_XtLoadQueryFont()`, `_XtQueryFont()`, font resource converter
- XtTypes.h — struct definition and typedefs
- Resources.c — `XtRFontStruct` handling
- GCManager.c — GC font field
- IswXcbDraw.c / ISWXcbDraw.c — text drawing helpers

### Resource converter
The `XtRFontStruct` converter currently parses XLFD names and opens XCB fonts. It needs to parse fontconfig-style names ("Sans 12", "DejaVu Serif Bold Italic 14") and populate the new `ISWFont` struct instead.

## Migration strategy

1. Define `ISWFont` in a new header or in `ISWRender.h`
2. Add `XtRISWFont` resource type and converter
3. Update `ISWRender` API to take `ISWFont *` instead of `XFontStruct *`
4. Update each widget one at a time (Label first as the base class, then subclasses)
5. Remove `XtFontStruct` / `XFontStruct` typedefs
6. Remove `_XtLoadQueryFont` / `_XtQueryFont` if no longer needed
7. Remove `xcb_open_font` / `xcb_query_font` calls (no XCB server-side fonts)

## Risk

- The XCB pure-fallback backend (`ISWRenderXCB.c`) uses `xcb_image_text_8` which requires a server-side font GC. If this backend is retained, it still needs XCB font loading. Consider whether the pure-XCB backend should be dropped (Cairo is mandatory anyway).
- `XtRFontStruct` is a well-known Xt resource type string. Application code using `XtVaGetValues(..., XtNfont, &fs, NULL)` will break. Need a migration path or keep the resource name but change the type behind it.
