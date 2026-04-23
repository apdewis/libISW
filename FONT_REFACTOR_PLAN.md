# Font Type Refactor Plan

## Status (2026-04-23)

The refactor is **substantially complete**. The codebase evolved `IswFontStruct` in place rather than introducing a parallel `ISWFont` type. The Cairo rendering path is fontconfig-native; the legacy XCB font-server path survives only as vestigial fallback in the resource converter.

What's left is cleanup and one small conversion (`ISWFontSet`), not a big refactor.

## Current state

### `IswFontStruct` — the actual type in use

Defined at [include/ISW/IswTypes.h:244-257](include/ISW/IswTypes.h#L244-L257):

```c
typedef struct _IswFontStruct {
    xcb_font_t  fid;                          /* legacy; 0 for fontconfig-loaded fonts */
    unsigned    direction, min_char_or_byte2, max_char_or_byte2,
                min_byte1, max_byte1;         /* legacy XCB metadata, largely unused */
    int         ascent, descent;              /* from FreeType */
    char       *font_family;                  /* fontconfig family */
    int         font_weight;                  /* FC_WEIGHT_* */
    int         font_slant;                   /* FC_SLANT_* */
} IswFontStruct;
```

Note: **size is not stored** — it's carried separately (typically via the widget's `XtNpointSize` or equivalent, or embedded in the original fontconfig pattern string and lost after parse). This is a gap; see "Remaining work" below.

### What's done

- ✓ All widgets store `IswFontStruct *` (Label, List, SmeBSB, AsciiSink, MultiSink, Slider, SpinBox, FontChooser, IconView, ListView, ProgressBar, Tip, Toggle, Tabs).
- ✓ `ISWRenderSetFont()` and the backend vtable `set_font` take `IswFontStruct *` — [include/ISW/ISWRender.h:382](include/ISW/ISWRender.h#L382), [src/ISWRenderPrivate.h:77](src/ISWRenderPrivate.h#L77).
- ✓ Cairo-XCB backend drives text via fontconfig family/weight/slant fields — [src/ISWRenderCairoXCB.c](src/ISWRenderCairoXCB.c), [src/ISWRender.c:911-913](src/ISWRender.c#L911-L913).
- ✓ Pure-XCB render backend (`ISWRenderXCB.c`) has been removed. Only Cairo-XCB (and optionally Cairo-EGL) remain.
- ✓ Fontconfig loader `_IswLoadFontconfigFont()` at [src/Converters.c:724](src/Converters.c#L724) parses patterns like `"Sans-10"` / `"DejaVu Serif Bold Italic 14"` and populates `IswFontStruct` via FreeType.
- ✓ Old `_XtLoadQueryFont` / `_XtQueryFont` removed.
- ✓ `XFontStruct` / `XtFontStruct` gone from active code (one stray doc-comment typo in [include/ISW/Toggle.h:62](include/ISW/Toggle.h#L62)).

### What's pragmatic / carried over

- `IswFontStruct` kept the legacy XCB fields (`fid`, `min_byte1`, byte-range metadata) rather than being replaced with a clean struct. For fontconfig-loaded fonts `fid == 0` and the byte-range fields are meaningless.
- Resource type string is still `IswRFontStruct` (see [include/ISW/StringDefs.h:231](include/ISW/StringDefs.h#L231)); no `XtRISWFont` type was introduced. Given everything already uses `IswFontStruct`, this is fine — renaming would just churn application code.
- `IswCvtStringToFont()` (raw XCB font-ID converter) still has an XLFD fallback pattern at [src/Converters.c:1180](src/Converters.c#L1180) and calls `xcb_open_font()` via `_IswLoadFont()` at [src/Converters.c:796](src/Converters.c#L796). Unused by rendering; reachable only if an app explicitly requests `IswRFont` (raw XID).

## Remaining work

In rough priority order.

### 1. Decide what to do about `ISWFontSet`

Two conflicting definitions:
- [include/ISW/IswTypes.h:259](include/ISW/IswTypes.h#L259) — `typedef void *IswFontSet;` (opaque)
- [include/ISW/ISWXftCompat.h:23-30](include/ISW/ISWXftCompat.h#L23-L30) — a real struct wrapping `{xcb_connection_t*, xcb_font_t, ascent, descent, height}`

It's used alongside `IswFontStruct *` in menu paths (SmeBSB, `IswTextWidth`, `IswDrawString` in [src/ISWXcbDraw.c](src/ISWXcbDraw.c)). Its converter `IswCvtStringToFontSet()` at [src/Converters.c:1230](src/Converters.c#L1230) explicitly returns failure ("not supported in XCB port").

Action: either **delete `ISWFontSet`** (replace usages with `IswFontStruct *`) or collapse it into a trivial alias. The fact that its converter is a stub is the tell — nothing is actually constructing these.

### 2. Add size to `IswFontStruct`

Currently the fontconfig pattern's point size is used at load time but not stored. Cairo text output re-derives size from widget-level resources, which is fragile (FontChooser and ProgressBar both work around this). Add:

```c
double size;   /* point size; 0 => use context default */
```

to `IswFontStruct`, populate it in `_IswLoadFontconfigFont()`, and read it from `ISWRenderCairoXCB.c` instead of the current out-of-band path.

### 3. Prune legacy XCB font-server path

If nothing inside the library still needs raw `xcb_font_t`:
- Drop `IswCvtStringToFont()` and the XLFD fallback at [src/Converters.c:1180](src/Converters.c#L1180).
- Drop `_IswLoadFont()` / `_IswFreeFont()` (the `xcb_open_font` / `xcb_close_font` wrappers).
- Remove the `fid` + byte-range fields from `IswFontStruct`.

A grep for `xcb_open_font` / `xcb_image_text_8` across `src/` should confirm no render-time caller remains before doing this.

### 4. Trivial cleanup

- Fix the `XFontStructx*` typo in the comment at [include/ISW/Toggle.h:62](include/ISW/Toggle.h#L62).
- Audit the `direction` / `min_char_or_byte2` / `max_char_or_byte2` / `min_byte1` / `max_byte1` readers — if nothing consumes them, remove with (3).

## Non-goals

- Introducing a parallel `ISWFont` type. `IswFontStruct` is the de-facto one; a second type just doubles the surface.
- Renaming the `IswRFontStruct` resource type. Application code already uses it; rename is churn without benefit.
- UTF-8 / shaping work. That lives in [TODO.md](TODO.md) under the i18n section and is independent of the font-type question (it's a text-pipeline question: `ISWRenderDrawString` / `IswTextWidth` handling multibyte input).
