# UTF-8 Support — Execution Plan

## Goal

Make ISW render and measure UTF-8 text correctly end-to-end. ASCII/Latin-1 already works incidentally (Cairo tolerates UTF-8 bytes), but measurement, cursor positioning, character navigation, and menu layout all assume one byte = one character. This plan fixes that.

Input methods (XIM / IBus / Fcitx) are **out of scope** here — tracked separately under "XIM replacement" in TODO.md. This plan covers *display and editing of UTF-8 text that is already in memory*.

## Guiding decisions

1. **UTF-8 is the only internal representation.** No wchar, no mbstate round-trips. Strings in widgets, the render API, and the clipboard are all UTF-8.
2. **The Cairo backend is the text renderer.** It already handles UTF-8. No shaping / HarfBuzz yet — that's a later step once the pipeline is UTF-8-clean.
3. **No new public font type.** The font refactor already landed `IswFontStruct` ([FONT_REFACTOR_PLAN.md](FONT_REFACTOR_PLAN.md)). UTF-8 work uses that; it does not introduce a parallel "font abstraction" as the older TODO note suggested.
4. **Delete `ISWFontSet` rather than refill it.** It's a vestigial Xlib-era wrapper whose converter is already a stub. Widgets that take it (MultiSink, SmeBSB, ListView, Tip) move to `IswFontStruct *` like everything else.
5. **Retire `MultiSink` / `MultiSrc` / wchar conversion.** Once the ASCII path is UTF-8-clean, the "Multi" variant has no reason to exist. One sink, one source, always UTF-8.

## Phase 1 — UTF-8 in the render API (unlocks display widgets)

Smallest change, biggest visible payoff. Everything downstream (measurement, cursor math) depends on this being correct.

### 1a. Define codepoint iteration helpers

New small internal header `src/ISWUtf8.h` (or folded into an existing util):

```c
/* Decode one codepoint. Returns bytes consumed, or 0 on invalid/empty.
 * Invalid bytes decode as U+FFFD, consuming 1 byte. */
int  _IswUtf8Decode(const char *s, int len, uint32_t *cp_out);

/* Byte length of the codepoint starting at s[0], 1 for invalid. */
int  _IswUtf8CharLen(const char *s, int len);

/* Count codepoints in a UTF-8 byte range. */
int  _IswUtf8CodepointCount(const char *s, int byte_len);

/* Step forward/backward by one codepoint; clamps to [0, len]. */
int  _IswUtf8Next(const char *s, int len, int pos);
int  _IswUtf8Prev(const char *s, int pos);
```

No external dependency — ~60 lines of C. `MultiSink.c` already has a decoder (`IswWcToUtf8` and friends) that can be the starting point, but the new helpers are byte-oriented (no wchar involved).

### 1b. Fix the Cairo text path to measure correctly

[src/ISWRenderCairoXCB.c](src/ISWRenderCairoXCB.c) — `cairo_show_text` already draws UTF-8 bytes correctly; the bug is that `cairo_text_extents` is called on `(text, length)` where `length` is already correct bytes. The measurement problem isn't actually in Cairo — it's in callers that then do `length * avg_char_width` or `length / char_count` arithmetic assuming bytes == chars.

Audit: grep for callers doing byte-index arithmetic. [src/AsciiSink.c](src/AsciiSink.c), [src/Label.c:645](src/Label.c#L645) (segment_len), [src/IconView.c:425](src/IconView.c#L425) (incremental `i+1` width probing to truncate — this is broken for UTF-8; truncating between continuation bytes produces mojibake).

### 1c. Fix byte-index arithmetic

Every place that advances through a string one byte at a time and asks "does this prefix fit?" needs to advance by codepoint instead:

- [src/IconView.c:418-522](src/IconView.c#L418-L522) — ellipsis truncation loops
- [src/ListView.c](src/ListView.c) — similar truncation in `ISWRenderDrawString` callers
- [src/Label.c:628-654](src/Label.c#L628-L654) — `segment_len` word-wrap (wraps on ASCII spaces — that's fine for UTF-8 since bytes ≥0x80 can't collide with space, but the length passed must end on a codepoint boundary)
- [src/AsciiSink.c:227](src/AsciiSink.c#L227) — per-character width lookup (`ch_buf[2]`) assumes 1-2 byte cells. Becomes "decode one codepoint, measure it".

Mechanical change, but has to be done everywhere.

### 1d. Remove the `ISWXcbDraw.c` byte-text fallbacks

[src/ISWXcbDraw.c:398-698](src/ISWXcbDraw.c#L398-L698) still define `ISWXcbTextWidth` / `ISWXcbDrawText` / `IswTextWidth` / `IswDrawString` around `xcb_query_text_extents` and `xcb_image_text_8`. These are single-byte and tied to server-side fonts.

Two sub-steps:
1. Grep for callers. [src/SmeBSB.c:371,384,404,663](src/SmeBSB.c) still use `IswTextWidth` / `IswDrawString` directly in addition to `ISWRenderDrawString`. Route them through `ISWRenderTextWidth` / `ISWRenderDrawString`.
2. Once unreferenced, delete the byte-text functions. Also delete their duplicates in the `Isw`-cased shadow files (`src/IswXcbDraw.c`, `src/IswXcbDraw.h`).

This is also what eventually lets `IswFontStruct.fid` be deleted (see font plan).

**Exit criterion for Phase 1:** demo app with a deliberately-UTF-8 label ("café", "日本語", "→") renders correctly in Label, List, Command, Tip, IconView, SmeBSB, Slider tick labels, ListView cells. Truncation ellipsis doesn't split codepoints.

## Phase 2 — Delete `ISWFontSet`

Precondition: Phase 1 done. `IswFontStruct *` carries all the metrics any widget needs, and routing is already through `ISWRender*` for display widgets.

Steps:

1. In [include/ISW/ISWXftCompat.h](include/ISW/ISWXftCompat.h), delete the `_IswFontSet` struct. Keep the header as a thin compat shim or delete entirely if no external user is left.
2. Delete the dual `IswFontSet` typedefs in [include/ISW/IswTypes.h:259-260](include/ISW/IswTypes.h#L259-L260).
3. In [src/Converters.c:1230-1260](src/Converters.c#L1230-L1260), delete `IswCvtStringToFontSet` (already a no-op).
4. Replace `fontset` resources in widgets with `font` resources (or alias `IswNfontSet` as a deprecated synonym for `IswNfont` for a release):
   - [src/MultiSink.c:167](src/MultiSink.c#L167)
   - [src/ListView.c:49](src/ListView.c#L49)
   - [src/Tip.c:139](src/Tip.c#L139)
   - [src/SmeBSB.c](src/SmeBSB.c) — `entry->sme_bsb.fontset` → `entry->sme_bsb.font`
   - [src/ISWIm.c:110](src/ISWIm.c#L110) / [src/IswIm.c:111](src/IswIm.c#L111)
5. Delete `ISWFontSetTextWidth` shim in [src/ISWXcbDraw.c:327-332](src/ISWXcbDraw.c#L327-L332).

**Exit criterion:** `grep -ri fontset src/ include/` returns only dead-string-pool entries, deprecation aliases, or is empty.

## Phase 3 — Unify the text sink/source into one UTF-8 implementation

Precondition: Phases 1 & 2.

Today there are two parallel implementations of the same thing: `AsciiSink`/`AsciiSrc` (byte path, `#ifdef` scaffolding) and `MultiSink`/`MultiSrc` (wchar path that never worked properly post-XCB-port). Both names are misleading once the toolkit speaks UTF-8 — "Ascii" is a lie, "Multi" implies a distinction that no longer exists.

The right shape is **one** sink class and **one** source class, both UTF-8, named for what they are rather than the legacy encoding dichotomy.

### Proposed names

- `TextSink` / `TextSinkObjectClass` (replaces both `AsciiSink` and `MultiSink`)
- `TextSource` / `TextSourceObjectClass` (replaces both `AsciiSrc` and `MultiSrc`)

Or keep the existing `TextSrc` base class name if that's already the common supertype — check the hierarchy and pick the name that requires the least reshuffling. The important thing is that the name doesn't encode a character-set assumption.

### Steps

1. Identify what each of the four implementations actually does differently. Most of the "Multi" code is wchar plumbing that does nothing useful (see earlier audit) — but anything real (RTL hinting, tab-width handling, locale-aware line break probes at [src/MultiSink.c:408,474,777,798,865](src/MultiSink.c#L408)) needs to be preserved.
2. Build the unified `TextSink` / `TextSource` as the UTF-8 implementation, taking whichever existing file is the cleaner starting point (likely `AsciiSink.c`/`AsciiSrc.c` since they actually render) and folding in the preserved bits from the Multi* variants.
3. Delete `MultiSink.c` / `MultiSrc.c` / `AsciiSink.c` / `AsciiSrc.c` and their headers.
4. The Text widget's default sink/source class becomes the unified one. No more conditional selection.
5. `_ISWTextWCToMB` and the wchar conversion surface in [src/TextSrc.c](src/TextSrc.c) go away — strings are UTF-8 at the API boundary.

### Compatibility

The old class symbols (`asciiSinkObjectClass`, `multiSinkObjectClass`, `asciiSrcObjectClass`, `multiSrcObjectClass`) stay as aliases pointing at the new class records. Application code that does `XtCreateWidget(..., asciiSinkObjectClass, ...)` keeps working. The aliases can be marked deprecated in headers and removed in a later release.

## Phase 4 — Text widget edit operations

Precondition: Phase 1 (rendering); can proceed in parallel with 2 & 3.

Text editing is where byte/codepoint mismatches cause visible bugs (cursor between continuation bytes, backspace deleting one byte of a multibyte char, selection mid-codepoint).

Files: [src/TextAction.c](src/TextAction.c), [src/Text.c](src/Text.c), [src/AsciiSrc.c](src/AsciiSrc.c).

Fix points:

- **Cursor motion** — `forward-character`, `backward-character`: advance by `_IswUtf8Next` / `_IswUtf8Prev`, not by 1.
- **Deletion** — `delete-previous-character`, `delete-next-character`: delete one codepoint, not one byte.
- **Word navigation** — `forward-word` / `backward-word`: treat any byte ≥0x80 as word-constituent for now (good enough for most scripts; proper Unicode word boundaries come later).
- **Selection endpoints** — snap to codepoint boundaries when set programmatically or via mouse drag mid-glyph.
- **Line wrap / column computation** — any `XawTextPosition` arithmetic that divides by an average char width is broken. Use `ISWRenderTextWidth` on the actual byte range.

The `#ifdef ISW_INTERNATIONALIZATION` branches in TextAction.c go away — the new path is unconditionally UTF-8.

## Phase 5 — Remove i18n scaffolding

Precondition: 1-4 done.

~130 occurrences of `ISW_INTERNATIONALIZATION` across 36 files. With Multi* gone and the ASCII path UTF-8-clean, most are dead:

- Delete the CMake option in [CMakeLists.txt](CMakeLists.txt).
- Delete `#ifdef ISW_INTERNATIONALIZATION` / `#endif` pairs; keep the code on the "ON" side where it did something real, drop it where it just selected between equivalent paths.
- Same treatment for the scaffolding in [src/ISWI18n.c](src/ISWI18n.c) — delete what's now obsolete; anything still useful (locale detection for date/number formatting, if any) moves into a non-`ifdef`'d utility file.

Leave `ISW_HAS_XIM` alone — that gates the XIM code and is the subject of the separate input-method effort.

## Phase 6 — Shaping (optional, deferred)

Phases 1-5 get ISW to a state where any UTF-8 string whose *visual form is the codepoints laid out left-to-right in order* renders correctly. That covers a lot — European scripts with precomposed accents, Greek, Cyrillic, CJK as long as the input is already composed, basic emoji the font happens to have a glyph for. Phase 6 covers everything where *what you draw* differs from *the codepoints you were given*. That's the job of a shaping engine, and in practice that means HarfBuzz.

### What "shaping" actually means

A shaping engine takes a run of codepoints + a font and returns a sequence of `(glyph_id, x_advance, y_advance, x_offset, y_offset)` tuples. The number of output glyphs does not equal the number of input codepoints, and their order does not match input order. The engine consults the font's GSUB/GPOS OpenType tables and a Unicode script database to figure out what to substitute and how to position.

Concretely, here's what that unlocks:

### Arabic and Hebrew: joining + RTL

Arabic letters have up to four contextual forms (isolated, initial, medial, final). The codepoints don't change — ع is always U+0639 — but the glyph the font uses depends on neighbours. Without shaping, every letter renders in its isolated form, producing recognisable-but-broken text that any Arabic reader will flag as obviously wrong. HarfBuzz's `arab` / `arab2` script handling picks the right glyph via the font's GSUB tables.

Separately, RTL **layout** (pen moves right-to-left) is a BiDi problem, not a shaping problem — the Unicode Bidirectional Algorithm (UAX #9) runs before shaping to segment text into runs of consistent direction. ISW needs FriBiDi or equivalent for that; HarfBuzz alone isn't enough. This is called out as a separate non-goal.

### Indic, Brahmic, Southeast Asian: reordering

Devanagari, Bengali, Tamil, Thai, Khmer etc. have reordering rules where a codepoint written *after* a base consonant may need to render *before* it (the Devanagari short-i vowel U+093F is the canonical example — it's typed after its consonant but drawn to the left of it). Without shaping, the text is literally unreadable to a native speaker — not "ugly", *wrong*. HarfBuzz's Indic shapers handle this plus the consonant-cluster / conjunct logic.

### Combining marks

"café" works today because the é is almost always typed as the precomposed U+00E9. But Unicode also permits U+0065 U+0301 (e + combining acute), and normalisation forms NFD / NFKD produce exactly that. Without shaping/positioning, the combining acute renders as its own advance-carrying glyph *after* the e, giving "é " instead of "é". Same problem with every diacritic in every language that uses decomposed forms — Vietnamese is particularly affected because it stacks marks.

HarfBuzz uses the font's GPOS mark-positioning tables to anchor the mark over (or under, or beside) the base glyph with zero advance.

### Emoji ZWJ sequences

Modern emoji like 👨‍👩‍👧 (family) or 🏳️‍🌈 (rainbow flag) are sequences of multiple codepoints joined with U+200D ZERO WIDTH JOINER and/or U+FE0F VARIATION SELECTOR-16. The font has a single ligature glyph for the whole sequence in its GSUB table. Without shaping, they render as the component emoji in sequence: 👨👩👧 or 🏳🌈. With shaping, the engine recognises the sequence and substitutes the ligature.

Skin-tone modifiers (👋🏽) are the same pattern — U+1F44B U+1F3FD pairs via GSUB into a single toned glyph.

### Ligatures and kerning

Pure-typography wins, present even for English:

- **Ligatures**: fi → ﬁ, fl → ﬂ, and in code fonts `==`, `!=`, `=>` → single crafted glyphs (Fira Code, Iosevka etc.). Without shaping these render as their component characters — still readable, but the whole point of a programming ligature font is lost.
- **Kerning (GPOS)**: Cairo's toy text API applies basic kerning from the font's legacy kern table but ignores modern GPOS kerning, which is where most fonts since ~2010 put their pair adjustments. Text looks loose or uneven in ways designers notice.

### Why not just Cairo?

Cairo has two text APIs. The **toy** API (`cairo_show_text`, `cairo_text_extents`) is what ISW uses today via `ISWRenderCairoXCB.c` — it takes a UTF-8 string, picks glyphs 1:1 from the font's cmap, applies legacy kerning, and draws. No GSUB, no GPOS, no script awareness. The **scaled-font / glyph** API (`cairo_show_glyphs`, `cairo_user_scaled_font`) takes a pre-shaped glyph run. That's the API HarfBuzz output feeds into.

The work in Phase 6 is: run a shaper over each text run, then hand the resulting glyph array to `cairo_show_glyphs`. The GL backend (TODO.md) does the same thing but feeds the glyph array into its FreeType atlas lookup instead.

### Cost

- HarfBuzz itself — ~2MB .so, widely packaged, mature. One new required dep.
- FriBiDi for BiDi — small, similarly widely packaged. Only needed if RTL is in scope.
- A text-run segmenter — splits input into runs of consistent script + direction + font. ~200 lines of code, or use HarfBuzz's built-in unicode funcs plus a small script-boundary pass.
- Glyph run cache — shaping isn't free (micro to low milliseconds per run). For static widget labels, cache the glyph array keyed by `(string, font, size)` and invalidate on change. The Text widget's edit buffer needs incremental reshaping, typically line-at-a-time.
- API changes: `ISWRenderDrawString(utf8, len)` stays as the convenience entry point for simple cases, but a lower-level `ISWRenderDrawGlyphs(glyph_array, count)` is wanted so the Text widget can cache shaped runs across redraws.

### When to do it

Defer unless a concrete requirement appears. Realistic triggers:

1. An application using ISW needs to display user-supplied text in an RTL or Indic script (not just ASCII-plus-accents).
2. A designer-led app wants programming ligatures or proper GPOS kerning for English text quality.
3. Emoji rendering bugs get reported by users typing modern sequences.

Until one of those lands, the toy-text path is adequate and the dependency/complexity cost isn't justified.

## Suggested sequencing

```
Phase 1 (render API UTF-8)
       │
       ├─► Phase 2 (delete ISWFontSet)        ─┐
       │                                        ├─► Phase 5 (scaffolding cleanup)
       ├─► Phase 3 (collapse Multi*)          ─┤
       │                                        │
       └─► Phase 4 (Text editing)             ─┘

Phase 6 (shaping) — separate, later, optional.
```

Phase 1 is the gate: without it, nothing else makes sense. 2, 3, 4 are independent and can be done in any order or in parallel.

## Non-goals

- HarfBuzz / complex shaping (Phase 6, deferred).
- Input methods / XIM replacement — tracked separately in TODO.md. Note that without shaping, IME-composed CJK still *displays* fine once Phase 1 lands; it just can't be *entered* from the keyboard without XIM.
- BiDi. Needed for Arabic/Hebrew but orthogonal — even with HarfBuzz the logical→visual reordering is a separate pass.
- Unicode-aware case folding, normalisation, or collation. Not needed for a widget toolkit.
- Grapheme-cluster-aware cursor movement (beyond codepoints). Nice to have; not blocking. Add once HarfBuzz is present.

## Risk and rollback

- **Risk: byte-arithmetic sites missed.** A Phase-1 audit pass that greps for every `+1`/`-1`/`* width` near a text pointer catches most. The rest surface as visual bugs in the demo; fix as found.
- **Risk: applications relying on `IswFontSet` typedef.** Keep the typedef as `typedef IswFontStruct *IswFontSet;` for one release if needed. Remove after.
- **Risk: `MultiSinkObjectClass` symbol used by application code.** Keep the symbol as an alias for `AsciiSinkObjectClass` (→ renamed or not) permanently; it's a small cost.
- **Rollback unit per phase** — each phase is a set of commits. Phase 1's audit sites can ship incrementally (one widget at a time), so a regression in IconView truncation doesn't block Label from landing.

## Files touched (summary)

**New:** `src/ISWUtf8.h` / `src/ISWUtf8.c` (~80 lines).

**Heavily edited:**
- `src/ISWRenderCairoXCB.c`, `src/ISWRender.c` — audit text paths
- `src/AsciiSink.c`, `src/IconView.c`, `src/Label.c`, `src/ListView.c`, `src/SmeBSB.c`, `src/List.c`, `src/Tip.c`, `src/Slider.c`, `src/Tabs.c` — byte→codepoint arithmetic
- `src/TextAction.c`, `src/Text.c`, `src/AsciiSrc.c` — cursor/delete/select
- `src/ISWXcbDraw.c`, `src/IswXcbDraw.c` — delete byte-text fallbacks

**Deleted (by end):**
- `src/AsciiSink.c`, `src/AsciiSrc.c`, `src/MultiSink.c`, `src/MultiSrc.c` and their headers — replaced by unified `TextSink` / `TextSource`
- `include/ISW/ISWXftCompat.h` (or reduced to an empty compat shim)
- `IswFontSet` typedef, `ISWFontSetTextWidth`, `IswCvtStringToFontSet`
- ~100 `#ifdef ISW_INTERNATIONALIZATION` blocks

**Kept as deprecated aliases:**
- `asciiSinkObjectClass`, `multiSinkObjectClass`, `asciiSrcObjectClass`, `multiSrcObjectClass` (point at unified classes)
