/*
 * IconView.c - IconView (grid view) widget implementation
 *
 * Displays a scrollable grid of items, each with an SVG icon and text label.
 * Designed to be placed inside a Viewport for scrolling.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWUtf8.h>
#include <ISW/ISWImage.h>
#include <ISW/IconViewP.h>
#include <ISW/FocusMgrI.h>
#include <ISW/ISWXdnd.h>
#include <ISW/Viewport.h>
#include <ISW/IswArgMacros.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define LABEL_MARGIN 2

#define Offset(field) IswOffsetOf(IconViewRec, field)

static IswResource resources[] = {
    {IswNiconLabels, IswCIconLabels, IswRPointer, sizeof(String *),
        Offset(iconView.labels), IswRImmediate, NULL},
    {IswNiconData, IswCIconData, IswRPointer, sizeof(String *),
        Offset(iconView.icon_data), IswRImmediate, NULL},
    {IswNnumIcons, IswCNumIcons, IswRInt, sizeof(int),
        Offset(iconView.nitems), IswRImmediate, (IswPointer) 0},
    {IswNiconSize, IswCIconSize, IswRDimension, sizeof(Dimension),
        Offset(iconView.icon_size), IswRImmediate, (IswPointer) 48},
    {IswNitemSpacing, IswCItemSpacing, IswRDimension, sizeof(Dimension),
        Offset(iconView.item_spacing), IswRImmediate, (IswPointer) 8},
    {IswNlabelLines, IswCLabelLines, IswRInt, sizeof(int),
        Offset(iconView.label_lines), IswRImmediate, (IswPointer) 1},
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
        Offset(iconView.foreground), IswRString, IswDefaultForeground},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
        Offset(iconView.font), IswRString, IswDefaultFont},
    {IswNselectCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(iconView.select_callback), IswRCallback, NULL},
    {IswNmultiSelect, IswCMultiSelect, IswRBoolean, sizeof(Boolean),
        Offset(iconView.multi_select), IswRImmediate, (IswPointer) False},
    {IswNcursorItem, IswCCursorItem, IswRInt, sizeof(int),
        Offset(iconView.cursor), IswRImmediate, (IswPointer) -1},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
};

#undef Offset

/* Forward declarations */
static int CountLabelLines(ISWRenderContext *, const char *, int);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);
static void SelectItem(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void BandDrag(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void BandFinish(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MoveCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ExtendSelection(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ActivateCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ToggleCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void SelectAll(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void HandleFocus(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ResolveForegroundRGB(IconViewWidget);
static void BandUpdateSelection(IconViewWidget);
static void ScrollToCursor(IconViewWidget);

static char defaultTranslations[] =
    "<Btn1Down>: SelectItem()\n"
    "<Btn1Motion>: BandDrag()\n"
    "<Btn1Up>: BandFinish()\n"
    "Ctrl<Key>a: SelectAll()\n"
    "Shift<Key>Left: ExtendSelection(left)\n"
    "Shift<Key>Right: ExtendSelection(right)\n"
    "Shift<Key>Up: ExtendSelection(up)\n"
    "Shift<Key>Down: ExtendSelection(down)\n"
    "~Shift ~Ctrl<Key>Left: MoveCursor(left)\n"
    "~Shift ~Ctrl<Key>Right: MoveCursor(right)\n"
    "~Shift ~Ctrl<Key>Up: MoveCursor(up)\n"
    "~Shift ~Ctrl<Key>Down: MoveCursor(down)\n"
    "<Key>Home: MoveCursor(home)\n"
    "<Key>End: MoveCursor(end)\n"
    "<Key>Return: ActivateCursor()\n"
    "<Key>space: ToggleCursor()\n"
    "<FocusIn>: HandleFocus(in)\n"
    "<FocusOut>: HandleFocus(out)";

static IswActionsRec actions[] = {
    {"SelectItem",      SelectItem},
    {"BandDrag",        BandDrag},
    {"BandFinish",      BandFinish},
    {"MoveCursor",      MoveCursor},
    {"ExtendSelection", ExtendSelection},
    {"ActivateCursor",  ActivateCursor},
    {"ToggleCursor",    ToggleCursor},
    {"SelectAll",       SelectAll},
    {"HandleFocus",     HandleFocus},
};

IconViewClassRec iconViewClassRec = {
  { /* core */
    (WidgetClass) &simpleClassRec,
    "IconView",
    sizeof(IconViewRec),
    IswInitializeWidgetSet,
    NULL,
    FALSE,
    Initialize,
    NULL,
    Realize,
    actions,
    IswNumber(actions),
    resources,
    IswNumber(resources),
    NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
    Resize,
    Redisplay,
    SetValues,
    NULL,
    IswInheritSetValuesAlmost,
    NULL,
    NULL,
    IswVersion,
    NULL,
    defaultTranslations,
    IswInheritQueryGeometry,
    IswInheritDisplayAccelerator,
    NULL
  },
  { /* simple */
    IswInheritChangeSensitive
  },
  { /* iconView */
    0
  }
};

WidgetClass iconViewWidgetClass = (WidgetClass)&iconViewClassRec;

/* --- Helpers --- */

static void
FreeCache(IconViewWidget iw)
{
    if (!iw->iconView.cache)
        return;
    for (int i = 0; i < iw->iconView.cache_size; i++) {
        if (iw->iconView.cache[i].image)
            ISWImageDestroy(iw->iconView.cache[i].image);
    }
    free(iw->iconView.cache);
    iw->iconView.cache = NULL;
    iw->iconView.cache_size = 0;
}

static void
FlushSVGCache(IconViewWidget iw)
{
    if (!iw->iconView.cache)
        return;
    for (int i = 0; i < iw->iconView.cache_size; i++) {
        if (iw->iconView.cache[i].image) {
            ISWImageDestroy(iw->iconView.cache[i].image);
            iw->iconView.cache[i].image = NULL;
        }
        iw->iconView.cache[i].raster = NULL;
        iw->iconView.cache[i].raster_w = 0;
        iw->iconView.cache[i].raster_h = 0;
    }
}

static void
AllocCache(IconViewWidget iw)
{
    FreeCache(iw);
    if (iw->iconView.sel_flags) {
        free(iw->iconView.sel_flags);
        iw->iconView.sel_flags = NULL;
    }
    if (iw->iconView.band_saved) {
        free(iw->iconView.band_saved);
        iw->iconView.band_saved = NULL;
    }
    iw->iconView.drop_highlight = -1;
    if (iw->iconView.nitems <= 0)
        return;
    iw->iconView.cache = calloc((size_t)iw->iconView.nitems,
                                 sizeof(IconViewItemCache));
    iw->iconView.cache_size = iw->iconView.nitems;
    iw->iconView.sel_flags = calloc((size_t)iw->iconView.nitems,
                                     sizeof(Boolean));
    iw->iconView.band_saved = calloc((size_t)iw->iconView.nitems,
                                      sizeof(Boolean));
}

static void
ComputeLayout(IconViewWidget iw)
{
    Widget w = (Widget)iw;
    Dimension icon_sz = (iw->iconView.icon_size);
    Dimension spacing = (iw->iconView.item_spacing);
    Dimension font_h = iw->iconView.font
        ? (Dimension)ISWScaledFontHeight(w, iw->iconView.font)
        : (14);
    Dimension margin = (LABEL_MARGIN);
    int max_lines = iw->iconView.label_lines;
    if (max_lines < 1) max_lines = 1;

    iw->iconView.cell_w = icon_sz + 4 * spacing;

    if (iw->core.width > 0 && iw->iconView.cell_w > 0)
        iw->iconView.ncols = (int)iw->core.width / (int)iw->iconView.cell_w;
    else
        iw->iconView.ncols = 1;
    if (iw->iconView.ncols < 1)
        iw->iconView.ncols = 1;

    iw->iconView.nrows = (iw->iconView.nitems + iw->iconView.ncols - 1)
                          / iw->iconView.ncols;

    /* (Re)allocate per-row arrays */
    free(iw->iconView.row_h);
    free(iw->iconView.row_y);
    iw->iconView.row_h = calloc((size_t)iw->iconView.nrows, sizeof(Dimension));
    iw->iconView.row_y = calloc((size_t)iw->iconView.nrows, sizeof(int));

    /* Measure labels to determine per-row heights */
    int label_w = (int)(iw->iconView.cell_w - spacing);
    ISWRenderContext *ctx = iw->iconView.render_ctx;
    if (ctx && iw->iconView.font)
        ISWRenderSetFont(ctx, iw->iconView.font);

    for (int r = 0; r < iw->iconView.nrows; r++) {
        int row_lines = 1;
        if (ctx && iw->iconView.labels) {
            int first = r * iw->iconView.ncols;
            int last = first + iw->iconView.ncols;
            if (last > iw->iconView.nitems)
                last = iw->iconView.nitems;
            for (int i = first; i < last; i++) {
                if (!iw->iconView.labels[i]) continue;
                int n = CountLabelLines(ctx, iw->iconView.labels[i], label_w);
                if (n > max_lines) n = max_lines;
                if (n > row_lines) row_lines = n;
            }
        }
        iw->iconView.row_h[r] = icon_sz + (Dimension)(font_h * (Dimension)row_lines)
                                 + margin + spacing;
    }

    /* Compute cumulative Y offsets */
    int y = 0;
    for (int r = 0; r < iw->iconView.nrows; r++) {
        iw->iconView.row_y[r] = y;
        y += (int)iw->iconView.row_h[r];
    }

    /* Set preferred height to fit all rows (Viewport uses this) */
    Dimension pref_h = (y > 0) ? (Dimension)y : 1;

    if (pref_h != iw->core.height) {
        Dimension actual_w, actual_h;
        IswGeometryResult r = IswMakeResizeRequest(w, iw->core.width, pref_h,
                                                  &actual_w, &actual_h);
        if (r == IswGeometryAlmost)
            IswMakeResizeRequest(w, actual_w, actual_h, NULL, NULL);
    }
}

static const unsigned char *
GetItemRaster(IconViewWidget iw, int index)
{
    if (!iw->iconView.cache || index < 0 || index >= iw->iconView.nitems)
        return NULL;

    IconViewItemCache *ic = &iw->iconView.cache[index];
    Dimension icon_sz = (iw->iconView.icon_size);
    float sf = (float)ISWScaleFactor((Widget)iw);
    unsigned int phys_sz = (unsigned int)(icon_sz * sf + 0.5f);

    /* Recolor existing image if foreground changed */
    if (ic->image) {
        char fg_hex[8];
        snprintf(fg_hex, sizeof(fg_hex), "#%02x%02x%02x",
                 (int)(iw->iconView.fg_r * 255.0),
                 (int)(iw->iconView.fg_g * 255.0),
                 (int)(iw->iconView.fg_b * 255.0));
        ic->raster = NULL;
        ISWImageRecolor(ic->image, fg_hex);
    }

    /* Already rasterized at correct size? */
    if (ic->raster && ic->raster_w == phys_sz && ic->raster_h == phys_sz)
        return ic->raster;

    /* Load image if needed */
    if (!ic->image && iw->iconView.icon_data &&
        iw->iconView.icon_data[index]) {
        double dpi = 96.0 * ISWScaleFactor((Widget)iw);
        char fg_hex[8];
        snprintf(fg_hex, sizeof(fg_hex), "#%02x%02x%02x",
                 (int)(iw->iconView.fg_r * 255.0),
                 (int)(iw->iconView.fg_g * 255.0),
                 (int)(iw->iconView.fg_b * 255.0));
        ic->image = ISWImageLoad(iw->iconView.icon_data[index],
                                  dpi, fg_hex);
    }

    if (!ic->image)
        return NULL;

    /* Rasterize */
    unsigned int rw, rh;
    ic->raster = ISWImageRasterize(ic->image, phys_sz, phys_sz, &rw, &rh);
    ic->raster_w = rw;
    ic->raster_h = rh;

    return ic->raster;
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    IconViewWidget iw = (IconViewWidget) new;
    (void)request; (void)args; (void)num_args;

    new->core.windowless = True;

    iw->iconView.sel_flags = NULL;
    iw->iconView.band_saved = NULL;
    iw->iconView.anchor = -1;
    iw->iconView.cursor = -1;
    iw->iconView.has_focus = False;
    ((SimpleWidget) new)->simple.traversal_on = True;
    iw->iconView.render_ctx = NULL;
    iw->iconView.cache = NULL;
    iw->iconView.cache_size = 0;
    iw->iconView.ncols = 1;
    iw->iconView.nrows = 0;
    iw->iconView.band_active = False;
    iw->iconView.redraw_pending = False;
    iw->iconView.work_proc_id = 0;
    iw->iconView.deselect_pending = False;
    iw->iconView.deselect_index = -1;
    iw->iconView.drop_highlight = -1;

    iw->iconView.icon_size = (iw->iconView.icon_size);
    iw->iconView.item_spacing = (iw->iconView.item_spacing);

    AllocCache(iw);

    if (iw->core.width == 0)
        iw->core.width = (300);
    if (iw->core.height == 0)
        iw->core.height = (200);

    ComputeLayout(iw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
    IconViewWidget iw = (IconViewWidget) w;

    (*iconViewWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    iw->iconView.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);

    ResolveForegroundRGB(iw);
}

static void
Destroy(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    if (iw->iconView.work_proc_id)
        IswRemoveWorkProc(iw->iconView.work_proc_id);
    FreeCache(iw);
    if (iw->iconView.sel_flags)
        free(iw->iconView.sel_flags);
    if (iw->iconView.band_saved)
        free(iw->iconView.band_saved);
    free(iw->iconView.row_h);
    free(iw->iconView.row_y);
    if (iw->iconView.render_ctx)
        ISWRenderDestroy(iw->iconView.render_ctx);
}

static void
Resize(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    ComputeLayout(iw);
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

/*
 * Count how many wrapped lines a label needs at the given width.
 * Uses the same word-break logic as DrawWrappedLabel.
 */
static int
CountLabelLines(ISWRenderContext *ctx, const char *label, int max_w)
{
    int len = (int)strlen(label);
    if (len == 0 || max_w <= 0) return 1;

    if (ISWRenderTextWidth(ctx, label, len) <= max_w)
        return 1;

    const char *pos = label;
    int remaining = len;
    int lines = 0;

    while (remaining > 0) {
        if (lines > 0) {
            while (remaining > 0 && *pos == ' ') { pos++; remaining--; }
            if (remaining == 0) break;
        }
        lines++;
        if (ISWRenderTextWidth(ctx, pos, remaining) <= max_w)
            break;
        int brk = 0, last_space = -1;
        int i = 0;
        while (i < remaining) {
            int step = _IswUtf8CharLen(pos + i, remaining - i);
            if (step <= 0) step = 1;
            int next = i + step;
            if (pos[i] == ' ' || pos[i] == '-' || pos[i] == '_'
                || pos[i] == '.')
                last_space = next;
            if (ISWRenderTextWidth(ctx, pos, next) > max_w) {
                brk = (last_space > 0) ? last_space : i;
                break;
            }
            i = next;
        }
        if (brk == 0) break;
        pos += brk;
        remaining -= brk;
    }
    return lines < 1 ? 1 : lines;
}

/*
 * Draw label text wrapped into up to max_lines lines, centered.
 * The last visible line is truncated with "..." if text remains.
 */
static void
DrawWrappedLabel(ISWRenderContext *ctx, const char *label, int max_w,
                 int max_lines, int cx, int baseline_y, int line_h)
{
    int len = (int)strlen(label);
    if (len == 0 || line_h <= 0 || max_w <= 0) return;

    if (max_lines < 1) max_lines = 1;

    /* Fast path: entire label fits on one line */
    int full_w = ISWRenderTextWidth(ctx, label, len);
    if (full_w <= max_w) {
        int lx = cx + (max_w - full_w) / 2;
        ISWRenderDrawString(ctx, label, len, lx, baseline_y);
        return;
    }

    const char *pos = label;
    int remaining = len;

    for (int line = 0; line < max_lines && remaining > 0; line++) {
        /* Skip leading spaces on continuation lines */
        if (line > 0) {
            while (remaining > 0 && *pos == ' ') {
                pos++;
                remaining--;
            }
            if (remaining == 0) break;
        }

        int is_last = (line == max_lines - 1);

        /* Check if the rest fits on this line */
        int rest_w = ISWRenderTextWidth(ctx, pos, remaining);
        if (rest_w <= max_w) {
            int lx = cx + (max_w - rest_w) / 2;
            ISWRenderDrawString(ctx, pos, remaining,
                                lx, baseline_y + line * line_h);
            break;
        }

        if (is_last) {
            /* Truncate with ellipsis */
            static const char ellipsis[] = "...";
            int ew = ISWRenderTextWidth(ctx, ellipsis, 3);
            int avail = max_w - ew;
            int trunc = 0;
            if (avail > 0) {
                /* Walk codepoints forward keeping the largest prefix that fits. */
                int i = 0;
                while (i < remaining) {
                    int step = _IswUtf8CharLen(pos + i, remaining - i);
                    if (step <= 0) step = 1;
                    int next = i + step;
                    if (ISWRenderTextWidth(ctx, pos, next) > avail) break;
                    i = next;
                }
                trunc = i;
            }
            char buf[256];
            if (trunc + 3 < (int)sizeof(buf)) {
                memcpy(buf, pos, (size_t)trunc);
                memcpy(buf + trunc, ellipsis, 4);
            } else {
                memcpy(buf, ellipsis, 4);
                trunc = 0;
            }
            int tw = ISWRenderTextWidth(ctx, buf, trunc + 3);
            int lx = cx + (max_w - tw) / 2;
            ISWRenderDrawString(ctx, buf, trunc + 3,
                                lx, baseline_y + line * line_h);
        } else {
            /* Find word-break point that fits within max_w */
            int brk = 0;
            int last_space = -1;
            int i = 0;
            while (i < remaining) {
                int step = _IswUtf8CharLen(pos + i, remaining - i);
                if (step <= 0) step = 1;
                int next = i + step;
                if (pos[i] == ' ' || pos[i] == '-' || pos[i] == '_'
                    || pos[i] == '.')
                    last_space = next;
                if (ISWRenderTextWidth(ctx, pos, next) > max_w) {
                    brk = (last_space > 0) ? last_space : i;
                    break;
                }
                i = next;
            }
            if (brk == 0) brk = remaining;

            int w1 = ISWRenderTextWidth(ctx, pos, brk);
            int lx = cx + (max_w - w1) / 2;
            ISWRenderDrawString(ctx, pos, brk,
                                lx, baseline_y + line * line_h);
            pos += brk;
            remaining -= brk;
        }
    }
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    IconViewWidget iw = (IconViewWidget) w;
    ISWRenderContext *ctx = iw->iconView.render_ctx;
    (void)event; (void)region;

    if (!ctx || !IswIsRealized(w))
        return;

    Dimension icon_sz = (iw->iconView.icon_size);
    Dimension spacing = iw->iconView.item_spacing;
    Dimension half_sp = spacing / 2;

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    if (iw->iconView.font)
        ISWRenderSetFont(ctx, iw->iconView.font);

    for (int i = 0; i < iw->iconView.nitems; i++) {
        int col = i % iw->iconView.ncols;
        int row = i / iw->iconView.ncols;
        Dimension rh = iw->iconView.row_h[row];
        int cx = col * (int)iw->iconView.cell_w + (int)half_sp;
        int cy = iw->iconView.row_y[row] + (int)half_sp;

        /* Selection highlight */
        if (iw->iconView.sel_flags && iw->iconView.sel_flags[i]) {
            ISWRenderSetColor(ctx, iw->iconView.foreground);
            ISWRenderFillRectangle(ctx, cx, cy,
                                   iw->iconView.cell_w - spacing,
                                   rh - spacing);
        }

        /* Icon */
        const unsigned char *raster = GetItemRaster(iw, i);
        if (raster) {
            IconViewItemCache *ic = &iw->iconView.cache[i];
            int ix = cx + ((int)(iw->iconView.cell_w - spacing) - (int)icon_sz) / 2;
            if (ISWImageIsMonochrome(ic->image)) {
                Boolean selected = iw->iconView.sel_flags &&
                                   iw->iconView.sel_flags[i];
                Pixel fg = selected ? w->core.background_pixel
                                    : iw->iconView.foreground;
                ISWRenderDrawImageMasked(ctx, fg, raster, ic->raster_w,
                                         ic->raster_h, ix, cy,
                                         icon_sz, icon_sz);
            } else {
                ISWRenderDrawImageRGBA(ctx, raster, ic->raster_w,
                                       ic->raster_h, ix, cy,
                                       icon_sz, icon_sz);
            }
        }

        /* Label (word-wrapped, ellipsis on last visible line) */
        if (iw->iconView.labels && iw->iconView.labels[i]) {
            int label_w = (int)(iw->iconView.cell_w - spacing);
            int ascent = ISWScaledFontAscent(w, iw->iconView.font);
            int margin = (int)(LABEL_MARGIN);
            int ly = cy + (int)icon_sz + ascent + margin;
            int line_h = iw->iconView.font
                ? ISWScaledFontHeight(w, iw->iconView.font)
                : (int)(14);
            int label_top = cy + (int)icon_sz + margin;
            int cell_bottom = cy + (int)(rh - spacing);
            int max_lines = (cell_bottom - label_top) / line_h;
            if (max_lines < 1) max_lines = 1;

            if (iw->iconView.sel_flags && iw->iconView.sel_flags[i]) {
                ISWRenderSetColor(ctx, w->core.background_pixel);
            } else {
                ISWRenderSetColor(ctx, iw->iconView.foreground);
            }
            DrawWrappedLabel(ctx, iw->iconView.labels[i],
                             label_w, max_lines, cx, ly, line_h);
        }
    }

    /* Drop-target highlight */
    if (iw->iconView.drop_highlight >= 0 &&
        iw->iconView.drop_highlight < iw->iconView.nitems) {
        int di = iw->iconView.drop_highlight;
        int col = di % iw->iconView.ncols;
        int row = di / iw->iconView.ncols;
        int dx = col * (int)iw->iconView.cell_w + (int)half_sp;
        int dy = iw->iconView.row_y[row] + (int)half_sp;
        int dw = (int)(iw->iconView.cell_w - spacing);
        int dh = (int)(iw->iconView.row_h[row] - spacing);

        /* Semi-transparent fill to show it's a target */
        ISWRenderSetColorRGBA(ctx,
            iw->iconView.fg_r, iw->iconView.fg_g, iw->iconView.fg_b, 0.15);
        ISWRenderFillRectangle(ctx, dx, dy, dw, dh);

        /* 2px solid border */
        ISWRenderSetColorRGBA(ctx,
            iw->iconView.fg_r, iw->iconView.fg_g, iw->iconView.fg_b, 0.8);
        ISWRenderSetLineWidth(ctx, 2.0);
        ISWRenderStrokeRectangle(ctx, dx + 1, dy + 1, dw - 2, dh - 2);
    }

    /* Cursor focus indicator */
    if (iw->iconView.has_focus && iw->iconView.cursor >= 0 &&
        iw->iconView.cursor < iw->iconView.nitems) {
        int ci = iw->iconView.cursor;
        int col = ci % iw->iconView.ncols;
        int row = ci / iw->iconView.ncols;
        int fx = col * (int)iw->iconView.cell_w + (int)half_sp;
        int fy = iw->iconView.row_y[row] + (int)half_sp;
        int fw = (int)(iw->iconView.cell_w - spacing);
        int fh = (int)(iw->iconView.row_h[row] - spacing);

        ISWRenderSetColor(ctx, iw->iconView.foreground);
        ISWRenderSetLineWidth(ctx, 1.0);

        /* Dashed outline: draw short segments along each edge */
        int dash = 3, gap = 3, step = dash + gap;
        /* Top and bottom edges */
        for (int dx = 0; dx < fw; dx += step) {
            int seg = (dx + dash > fw) ? fw - dx : dash;
            ISWRenderDrawLine(ctx, fx + dx, fy, fx + dx + seg, fy);
            ISWRenderDrawLine(ctx, fx + dx, fy + fh, fx + dx + seg, fy + fh);
        }
        /* Left and right edges */
        for (int dy = 0; dy < fh; dy += step) {
            int seg = (dy + dash > fh) ? fh - dy : dash;
            ISWRenderDrawLine(ctx, fx, fy + dy, fx, fy + dy + seg);
            ISWRenderDrawLine(ctx, fx + fw, fy + dy, fx + fw, fy + dy + seg);
        }
    }

    /* Rubber band overlay */
    if (iw->iconView.band_active) {
        int bx = iw->iconView.band_start_x < iw->iconView.band_cur_x
               ? iw->iconView.band_start_x : iw->iconView.band_cur_x;
        int by = iw->iconView.band_start_y < iw->iconView.band_cur_y
               ? iw->iconView.band_start_y : iw->iconView.band_cur_y;
        int bw = abs(iw->iconView.band_cur_x - iw->iconView.band_start_x);
        int bh = abs(iw->iconView.band_cur_y - iw->iconView.band_start_y);

        if (bw > 0 && bh > 0) {
            /* Semi-transparent fill */
            ISWRenderSetColorRGBA(ctx,
                iw->iconView.fg_r, iw->iconView.fg_g, iw->iconView.fg_b,
                0.15);
            ISWRenderFillRectangle(ctx, bx, by, bw, bh);

            /* 1px border at full opacity */
            ISWRenderSetColorRGBA(ctx,
                iw->iconView.fg_r, iw->iconView.fg_g, iw->iconView.fg_b,
                1.0);
            ISWRenderSetLineWidth(ctx, 1.0);
            ISWRenderStrokeRectangle(ctx, bx, by, bw, bh);
        }
    }

    _IswFocusMgrDrawRing(w, ctx, iw->iconView.foreground, 2.0);

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired,
          ArgList args, Cardinal *num_args)
{
    IconViewWidget ciw = (IconViewWidget) current;
    IconViewWidget diw = (IconViewWidget) desired;
    Boolean redraw = FALSE;
    (void)request; (void)args; (void)num_args;

    if (ciw->iconView.nitems != diw->iconView.nitems ||
        ciw->iconView.labels != diw->iconView.labels ||
        ciw->iconView.icon_data != diw->iconView.icon_data) {
        AllocCache(diw);  /* resets drop_highlight */
        ComputeLayout(diw);
        redraw = TRUE;
    }

    if (ciw->iconView.foreground != diw->iconView.foreground) {
        ResolveForegroundRGB(diw);
        FlushSVGCache(diw);
    }

    if (ciw->iconView.icon_size != diw->iconView.icon_size ||
        ciw->iconView.item_spacing != diw->iconView.item_spacing ||
        ciw->iconView.foreground != diw->iconView.foreground ||
        ciw->core.background_pixel != diw->core.background_pixel) {
        ComputeLayout(diw);
        redraw = TRUE;
    }

    return redraw;
}

/* --- Actions --- */

static void
ClearSelection(IconViewWidget iw)
{
    if (iw->iconView.sel_flags)
        memset(iw->iconView.sel_flags, 0,
               (size_t)iw->iconView.nitems * sizeof(Boolean));
}

static void
FireCallback(IconViewWidget iw, int clicked)
{
    Widget w = (Widget)iw;
    int *indices = NULL;
    int count = 0;

    /* Build array of selected indices */
    if (iw->iconView.sel_flags) {
        indices = (int *)IswMalloc((Cardinal)iw->iconView.nitems * sizeof(int));
        for (int i = 0; i < iw->iconView.nitems; i++) {
            if (iw->iconView.sel_flags[i])
                indices[count++] = i;
        }
    }

    IswIconViewCallbackData cb;
    cb.index = clicked;
    cb.label = (clicked >= 0 && iw->iconView.labels &&
                iw->iconView.labels[clicked])
               ? iw->iconView.labels[clicked] : NULL;
    cb.selected = indices;
    cb.num_selected = count;
    IswCallCallbacks(w, IswNselectCallback, (IswPointer)&cb);

    if (indices)
        IswFree((char *)indices);
}

static int
HitTest(IconViewWidget iw, Position x, Position y)
{
    Dimension spacing = iw->iconView.item_spacing;
    Dimension half_sp = spacing / 2;
    int col = (int)x / (int)iw->iconView.cell_w;

    /* Binary search for row by Y coordinate */
    int row = -1;
    int lo = 0, hi = iw->iconView.nrows - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if ((int)y < iw->iconView.row_y[mid])
            hi = mid - 1;
        else if ((int)y >= iw->iconView.row_y[mid] + (int)iw->iconView.row_h[mid])
            lo = mid + 1;
        else { row = mid; break; }
    }
    if (row < 0)
        return -1;

    if (col < 0 || col >= iw->iconView.ncols)
        return -1;

    int index = row * iw->iconView.ncols + col;
    if (index < 0 || index >= iw->iconView.nitems)
        return -1;

    /* Check if click is within the item's content rect, not just the cell */
    int cx = col * (int)iw->iconView.cell_w + (int)half_sp;
    int cy = iw->iconView.row_y[row] + (int)half_sp;
    int cw = (int)(iw->iconView.cell_w - spacing);
    int ch = (int)(iw->iconView.row_h[row] - spacing);

    if ((int)x < cx || (int)x >= cx + cw ||
        (int)y < cy || (int)y >= cy + ch)
        return -1;

    return index;
}

static void
SelectItem(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    Position x, y;

    uint8_t type = event->response_type & ~0x80;
    if (type == XCB_BUTTON_PRESS) {
        xcb_button_press_event_t *ev = (xcb_button_press_event_t *)event;
        x = ev->event_x;
        y = ev->event_y;
    } else {
        return;
    }

    int index = HitTest(iw, x, y);

    /* Detect modifiers from event state */
    uint16_t state = ((xcb_button_press_event_t *)event)->state;
    Boolean toggle = (state & XCB_MOD_MASK_CONTROL) != 0;
    Boolean extend = (state & XCB_MOD_MASK_SHIFT) != 0;

    if (!iw->iconView.sel_flags)
        return;

    /* Cancel any stuck rubber band from a previous interaction */
    if (iw->iconView.band_active)
        iw->iconView.band_active = False;

    if (index < 0) {
        /* Click on empty space — start rubber band if multi-select */
        if (iw->iconView.multi_select && IswIsRealized(w)) {
            if (!toggle)
                ClearSelection(iw);
            /* Save current selection for additive (Ctrl) mode */
            if (iw->iconView.band_saved && iw->iconView.sel_flags)
                memcpy(iw->iconView.band_saved, iw->iconView.sel_flags,
                       (size_t)iw->iconView.nitems * sizeof(Boolean));

            iw->iconView.band_active = True;
            iw->iconView.band_start_x = x;
            iw->iconView.band_start_y = y;
            iw->iconView.band_cur_x = x;
            iw->iconView.band_cur_y = y;

            ResolveForegroundRGB(iw);
            Redisplay(w, NULL, 0);
            return;
        }
        ClearSelection(iw);
        iw->iconView.anchor = -1;
    } else if (iw->iconView.multi_select && toggle) {
        /* Ctrl+click: toggle individual item */
        iw->iconView.sel_flags[index] = !iw->iconView.sel_flags[index];
        iw->iconView.anchor = index;
    } else if (iw->iconView.multi_select && extend &&
               iw->iconView.anchor >= 0) {
        /* Shift+click: range select from anchor to clicked */
        ClearSelection(iw);
        int lo = iw->iconView.anchor < index
                 ? iw->iconView.anchor : index;
        int hi = iw->iconView.anchor > index
                 ? iw->iconView.anchor : index;
        for (int i = lo; i <= hi; i++)
            iw->iconView.sel_flags[i] = True;
    } else if (iw->iconView.multi_select && iw->iconView.sel_flags[index]) {
        /* Click on already-selected item: defer deselect to ButtonRelease
         * so that dragging a group of selected icons preserves the selection */
        iw->iconView.deselect_pending = True;
        iw->iconView.deselect_index = index;
    } else {
        /* Plain click: select only this item */
        ClearSelection(iw);
        iw->iconView.sel_flags[index] = True;
        iw->iconView.anchor = index;
    }

    if (index >= 0)
        iw->iconView.cursor = index;

    Redisplay(w, NULL, 0);
    FireCallback(iw, index);
}

static void
ResolveForegroundRGB(IconViewWidget iw)
{
    xcb_connection_t *conn = ((Widget)iw)->core.display;
    xcb_colormap_t cmap = ((Widget)iw)->core.colormap;
    uint32_t pixel = (uint32_t)iw->iconView.foreground;
    xcb_query_colors_cookie_t cookie = xcb_query_colors(conn, cmap, 1, &pixel);
    xcb_query_colors_reply_t *reply = xcb_query_colors_reply(conn, cookie, NULL);
    if (reply) {
        xcb_rgb_t *rgb = xcb_query_colors_colors(reply);
        if (xcb_query_colors_colors_length(reply) > 0) {
            iw->iconView.fg_r = (double)(rgb[0].red >> 8) / 255.0;
            iw->iconView.fg_g = (double)(rgb[0].green >> 8) / 255.0;
            iw->iconView.fg_b = (double)(rgb[0].blue >> 8) / 255.0;
        }
        free(reply);
    }
}

static void
BandUpdateSelection(IconViewWidget iw)
{
    int bx1 = iw->iconView.band_start_x < iw->iconView.band_cur_x
            ? iw->iconView.band_start_x : iw->iconView.band_cur_x;
    int by1 = iw->iconView.band_start_y < iw->iconView.band_cur_y
            ? iw->iconView.band_start_y : iw->iconView.band_cur_y;
    int bx2 = iw->iconView.band_start_x > iw->iconView.band_cur_x
            ? iw->iconView.band_start_x : iw->iconView.band_cur_x;
    int by2 = iw->iconView.band_start_y > iw->iconView.band_cur_y
            ? iw->iconView.band_start_y : iw->iconView.band_cur_y;

    Dimension spacing = iw->iconView.item_spacing;
    Dimension half_sp = spacing / 2;

    for (int i = 0; i < iw->iconView.nitems; i++) {
        int col = i % iw->iconView.ncols;
        int row = i / iw->iconView.ncols;
        int ix = col * (int)iw->iconView.cell_w + (int)half_sp;
        int iy = iw->iconView.row_y[row] + (int)half_sp;
        int ix2 = ix + (int)(iw->iconView.cell_w - spacing);
        int iy2 = iy + (int)(iw->iconView.row_h[row] - spacing);

        Boolean intersects = !(ix2 < bx1 || ix > bx2 ||
                               iy2 < by1 || iy > by2);

        /* Additive (Ctrl): saved state OR band intersection.
         * Normal: band intersection only. */
        if (iw->iconView.band_saved)
            iw->iconView.sel_flags[i] = iw->iconView.band_saved[i] || intersects;
        else
            iw->iconView.sel_flags[i] = intersects;
    }
}

static Boolean
BandRedrawWorkProc(IswPointer closure)
{
    Widget w = (Widget) closure;
    IconViewWidget iw = (IconViewWidget) w;

    iw->iconView.work_proc_id = 0;
    if (iw->iconView.redraw_pending) {
        iw->iconView.redraw_pending = False;
        BandUpdateSelection(iw);
        Redisplay(w, NULL, 0);
    }
    return True;  /* remove work proc */
}

static void
BandDrag(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)params; (void)num_params;

    if (ISWXdndIsDragging(w))
        return;

    /* Motion after clicking a selected item — cancel deferred deselect
     * so the multi-selection is preserved for drag-and-drop */
    iw->iconView.deselect_pending = False;

    if (!iw->iconView.band_active)
        return;

    uint8_t type = event->response_type & ~0x80;
    if (type != XCB_MOTION_NOTIFY)
        return;

    xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
    Position new_x = ev->event_x;
    Position new_y = ev->event_y;

    /* Skip if position unchanged */
    if (new_x == iw->iconView.band_cur_x && new_y == iw->iconView.band_cur_y)
        return;

    iw->iconView.band_cur_x = new_x;
    iw->iconView.band_cur_y = new_y;

    /* Coalesce: defer redraw to a work proc so multiple motion events
     * arriving in the same event-loop pass produce only one repaint */
    iw->iconView.redraw_pending = True;
    if (!iw->iconView.work_proc_id) {
        iw->iconView.work_proc_id = IswAppAddWorkProc(
            IswWidgetToApplicationContext(w), BandRedrawWorkProc, (IswPointer)w);
    }
}

static void
BandFinish(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (ISWXdndIsDragging(w))
        return;

    /* Resolve deferred deselect: user clicked a selected item and released
     * without dragging, so narrow the selection to just that item */
    if (iw->iconView.deselect_pending) {
        iw->iconView.deselect_pending = False;
        int idx = iw->iconView.deselect_index;
        ClearSelection(iw);
        if (idx >= 0 && idx < iw->iconView.nitems)
            iw->iconView.sel_flags[idx] = True;
        iw->iconView.anchor = idx;
        Redisplay(w, NULL, 0);
        FireCallback(iw, idx);
        return;
    }

    if (!iw->iconView.band_active)
        return;

    iw->iconView.band_active = False;

    /* Cancel any pending coalesced redraw */
    if (iw->iconView.work_proc_id) {
        IswRemoveWorkProc(iw->iconView.work_proc_id);
        iw->iconView.work_proc_id = 0;
    }
    iw->iconView.redraw_pending = False;

    Redisplay(w, NULL, 0);
    FireCallback(iw, -1);
}

/* --- Keyboard navigation helpers --- */

static int
ComputeNewCursor(IconViewWidget iw, const char *direction)
{
    int cur = iw->iconView.cursor;
    int n = iw->iconView.nitems;
    int ncols = iw->iconView.ncols;

    if (n <= 0) return -1;

    /* If no cursor yet, start at 0 */
    if (cur < 0) return 0;

    if (strcmp(direction, "left") == 0) {
        return (cur > 0) ? cur - 1 : cur;
    } else if (strcmp(direction, "right") == 0) {
        return (cur < n - 1) ? cur + 1 : cur;
    } else if (strcmp(direction, "up") == 0) {
        return (cur >= ncols) ? cur - ncols : cur;
    } else if (strcmp(direction, "down") == 0) {
        return (cur + ncols < n) ? cur + ncols : cur;
    } else if (strcmp(direction, "home") == 0) {
        return 0;
    } else if (strcmp(direction, "end") == 0) {
        return n - 1;
    }
    return cur;
}

static void
ScrollToCursor(IconViewWidget iw)
{
    Widget w = (Widget)iw;
    Widget parent = IswParent(w);
    int cur = iw->iconView.cursor;

    if (cur < 0 || !parent || !IswIsRealized(w))
        return;

    /* Check if parent is a Viewport by seeing if it has a clip child.
     * We use the parent's visible height to determine if scrolling is needed. */
    Dimension visible_h = parent->core.height;
    Position child_y = w->core.y;  /* Our position within the viewport clip */

    int row = cur / iw->iconView.ncols;
    int item_top = iw->iconView.row_y[row];
    int item_bot = item_top + (int)iw->iconView.row_h[row];

    /* Convert to viewport-relative coordinates */
    int vis_top = -(int)child_y;
    int vis_bot = vis_top + (int)visible_h;

    if (item_top < vis_top) {
        /* Scroll up */
        IswViewportSetCoordinates(parent, 0, (Position)item_top);
    } else if (item_bot > vis_bot) {
        /* Scroll down */
        Position new_y = (Position)(item_bot - (int)visible_h);
        if (new_y < 0) new_y = 0;
        IswViewportSetCoordinates(parent, 0, new_y);
    }
}

static void
MoveCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1 || iw->iconView.nitems <= 0)
        return;

    int new_cur = ComputeNewCursor(iw, params[0]);
    if (new_cur == iw->iconView.cursor && iw->iconView.cursor >= 0)
        return;

    iw->iconView.cursor = new_cur;

    /* Plain arrow: select only cursor item */
    if (iw->iconView.sel_flags) {
        ClearSelection(iw);
        iw->iconView.sel_flags[new_cur] = True;
    }
    iw->iconView.anchor = new_cur;

    ScrollToCursor(iw);
    Redisplay(w, NULL, 0);
}

static void
ExtendSelection(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1 || iw->iconView.nitems <= 0)
        return;
    if (!iw->iconView.multi_select || !iw->iconView.sel_flags)
        return;

    int new_cur = ComputeNewCursor(iw, params[0]);
    iw->iconView.cursor = new_cur;

    /* Set anchor if not yet established */
    if (iw->iconView.anchor < 0)
        iw->iconView.anchor = new_cur;

    /* Select range from anchor to cursor */
    ClearSelection(iw);
    int lo = iw->iconView.anchor < new_cur ? iw->iconView.anchor : new_cur;
    int hi = iw->iconView.anchor > new_cur ? iw->iconView.anchor : new_cur;
    for (int i = lo; i <= hi; i++)
        iw->iconView.sel_flags[i] = True;

    ScrollToCursor(iw);
    Redisplay(w, NULL, 0);
}

static void
ActivateCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (iw->iconView.cursor < 0 || iw->iconView.cursor >= iw->iconView.nitems)
        return;

    /* Ensure cursor item is selected */
    if (iw->iconView.sel_flags) {
        ClearSelection(iw);
        iw->iconView.sel_flags[iw->iconView.cursor] = True;
    }

    Redisplay(w, NULL, 0);
    FireCallback(iw, iw->iconView.cursor);
}

static void
ToggleCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (iw->iconView.cursor < 0 || iw->iconView.cursor >= iw->iconView.nitems)
        return;
    if (!iw->iconView.sel_flags)
        return;

    if (iw->iconView.multi_select) {
        iw->iconView.sel_flags[iw->iconView.cursor] =
            !iw->iconView.sel_flags[iw->iconView.cursor];
    } else {
        ClearSelection(iw);
        iw->iconView.sel_flags[iw->iconView.cursor] = True;
    }

    Redisplay(w, NULL, 0);
}

static void
SelectAll(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (!iw->iconView.multi_select || !iw->iconView.sel_flags)
        return;

    for (int i = 0; i < iw->iconView.nitems; i++)
        iw->iconView.sel_flags[i] = True;

    Redisplay(w, NULL, 0);
}

static void
HandleFocus(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    IconViewWidget iw = (IconViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1)
        return;

    if (strcmp(params[0], "in") == 0) {
        iw->iconView.has_focus = True;
        if (iw->iconView.cursor < 0 && iw->iconView.nitems > 0)
            iw->iconView.cursor = 0;
    } else {
        iw->iconView.has_focus = False;
    }

    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

/* --- Public API --- */

void
IswIconViewSetItems(Widget w, String *labels, String *icon_data, int nitems)
{
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgIconLabels(&ab, labels);
    IswArgIconData(&ab, icon_data);
    IswArgNumIcons(&ab, nitems);
    IswSetValues(w, ab.args, ab.count);
}

int
IswIconViewGetSelected(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    if (!iw->iconView.sel_flags)
        return -1;
    /* Return first selected index for backward compat */
    for (int i = 0; i < iw->iconView.nitems; i++) {
        if (iw->iconView.sel_flags[i])
            return i;
    }
    return -1;
}

int
IswIconViewGetSelectedItems(Widget w, int **indices_out)
{
    IconViewWidget iw = (IconViewWidget) w;
    int count = 0;

    if (!iw->iconView.sel_flags || !indices_out) {
        if (indices_out) *indices_out = NULL;
        return 0;
    }

    int *buf = (int *)IswMalloc((Cardinal)iw->iconView.nitems * sizeof(int));
    for (int i = 0; i < iw->iconView.nitems; i++) {
        if (iw->iconView.sel_flags[i])
            buf[count++] = i;
    }

    if (count == 0) {
        IswFree((char *)buf);
        *indices_out = NULL;
        return 0;
    }

    *indices_out = buf;
    return count;
}

Boolean
IswIconViewBandActive(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    return iw->iconView.band_active;
}

void
IswIconViewSetDropHighlight(Widget w, int item_index)
{
    IconViewWidget iw = (IconViewWidget) w;
    if (item_index == iw->iconView.drop_highlight)
        return;
    iw->iconView.drop_highlight = item_index;
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

int
IswIconViewHitTest(Widget w, int x, int y)
{
    return HitTest((IconViewWidget)w, (Position)x, (Position)y);
}

const unsigned char *
IswIconViewGetItemRaster(Widget w, int index,
                         unsigned int *width_out, unsigned int *height_out)
{
    IconViewWidget iw = (IconViewWidget) w;
    const unsigned char *raster = GetItemRaster(iw, index);
    if (!raster)
        return NULL;
    if (width_out)  *width_out  = iw->iconView.cache[index].raster_w;
    if (height_out) *height_out = iw->iconView.cache[index].raster_h;
    return raster;
}
