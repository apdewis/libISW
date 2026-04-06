/*
 * ListView.c - Multi-column list view widget implementation
 *
 * Displays a scrollable table with resizable column headers,
 * multiselect, and rubber band selection. Designed to be placed
 * inside a Viewport for scrolling.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ListViewP.h>
#include <ISW/Viewport.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define CELL_PAD_X   6
#define CELL_PAD_Y   2
#define RESIZE_GRIP  4   /* pixels from column separator edge for resize cursor */
#define DEFAULT_COL_W 120
#define MIN_COL_W     30
#define SORT_ARROW_W  7  /* width of sort direction arrow */
#define SORT_ARROW_H  4  /* height of sort direction arrow */
#define SORT_ARROW_GAP 4 /* gap between title text and arrow */

#define Offset(field) XtOffsetOf(ListViewRec, field)

static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        Offset(listView.foreground), XtRString, XtDefaultForeground},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
        Offset(listView.font), XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
    {XtNfontSet, XtCFontSet, XtRFontSet, sizeof(ISWFontSet *),
        Offset(listView.fontset), XtRString, XtDefaultFontSet},
#endif
    {XtNselectCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(listView.select_callback), XtRCallback, NULL},
    {XtNreorderCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(listView.reorder_callback), XtRCallback, NULL},
    {XtNmultiSelect, XtCMultiSelect, XtRBoolean, sizeof(Boolean),
        Offset(listView.multi_select), XtRImmediate, (XtPointer) False},
    {XtNshowHeader, XtCShowHeader, XtRBoolean, sizeof(Boolean),
        Offset(listView.show_header), XtRImmediate, (XtPointer) True},
    {XtNrowHeight, XtCRowHeight, XtRDimension, sizeof(Dimension),
        Offset(listView.row_height), XtRImmediate, (XtPointer) 0},
    {XtNheaderHeight, XtCHeaderHeight, XtRDimension, sizeof(Dimension),
        Offset(listView.header_height), XtRImmediate, (XtPointer) 0},
    {XtNlistViewColumns, XtCListViewColumns, XtRPointer, sizeof(XtPointer),
        Offset(listView.columns_res), XtRImmediate, NULL},
    {XtNnumColumns, XtCNumColumns, XtRInt, sizeof(int),
        Offset(listView.ncols_res), XtRImmediate, (XtPointer) 0},
    {XtNnumRows, XtCNumRows, XtRInt, sizeof(int),
        Offset(listView.nrows), XtRImmediate, (XtPointer) 0},
    {XtNlistViewData, XtCListViewData, XtRPointer, sizeof(XtPointer),
        Offset(listView.data), XtRImmediate, NULL},
    {XtNcursorRow, XtCCursorRow, XtRInt, sizeof(int),
        Offset(listView.cursor), XtRImmediate, (XtPointer) -1},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
};

#undef Offset

/* Forward declarations */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(xcb_connection_t *, Widget, XtValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void SelectRow(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void BandDrag(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void BandFinish(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void MoveCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ExtendSelection(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ActivateCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void ToggleCursor(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void SelectAll(Widget, xcb_generic_event_t *, String *, Cardinal *);
static void HandleFocus(Widget, xcb_generic_event_t *, String *, Cardinal *);

static char defaultTranslations[] =
    "<Btn1Down>: SelectRow()\n"
    "<Btn1Motion>: BandDrag()\n"
    "<Btn1Up>: BandFinish()\n"
    "Ctrl<Key>a: SelectAll()\n"
    "Shift<Key>Up: ExtendSelection(up)\n"
    "Shift<Key>Down: ExtendSelection(down)\n"
    "~Shift ~Ctrl<Key>Up: MoveCursor(up)\n"
    "~Shift ~Ctrl<Key>Down: MoveCursor(down)\n"
    "<Key>Home: MoveCursor(home)\n"
    "<Key>End: MoveCursor(end)\n"
    "<Key>Return: ActivateCursor()\n"
    "<Key>space: ToggleCursor()\n"
    "<FocusIn>: HandleFocus(in)\n"
    "<FocusOut>: HandleFocus(out)";

static XtActionsRec actions[] = {
    {"SelectRow",        SelectRow},
    {"BandDrag",         BandDrag},
    {"BandFinish",       BandFinish},
    {"MoveCursor",       MoveCursor},
    {"ExtendSelection",  ExtendSelection},
    {"ActivateCursor",   ActivateCursor},
    {"ToggleCursor",     ToggleCursor},
    {"SelectAll",        SelectAll},
    {"HandleFocus",      HandleFocus},
};

ListViewClassRec listViewClassRec = {
  { /* core */
    (WidgetClass) &simpleClassRec,
    "ListView",
    sizeof(ListViewRec),
    IswInitializeWidgetSet,
    NULL,
    FALSE,
    Initialize,
    NULL,
    Realize,
    actions,
    XtNumber(actions),
    resources,
    XtNumber(resources),
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
    XtInheritSetValuesAlmost,
    NULL,
    NULL,
    XtVersion,
    NULL,
    defaultTranslations,
    XtInheritQueryGeometry,
    XtInheritDisplayAccelerator,
    NULL
  },
  { /* simple */
    XtInheritChangeSensitive
  },
  { /* listView */
    0
  }
};

WidgetClass listViewWidgetClass = (WidgetClass)&listViewClassRec;

/* ================================================================
 * Internal helpers
 * ================================================================ */

static void
FreeColumns(ListViewWidget lv)
{
    if (lv->listView.col_info) {
        free(lv->listView.col_info);
        lv->listView.col_info = NULL;
    }
    lv->listView.col_count = 0;
}

static void
FreeSelFlags(ListViewWidget lv)
{
    if (lv->listView.sel_flags) {
        free(lv->listView.sel_flags);
        lv->listView.sel_flags = NULL;
    }
    if (lv->listView.band_saved) {
        free(lv->listView.band_saved);
        lv->listView.band_saved = NULL;
    }
}

static void
AllocSelFlags(ListViewWidget lv)
{
    FreeSelFlags(lv);
    if (lv->listView.nrows <= 0)
        return;
    lv->listView.sel_flags = calloc((size_t)lv->listView.nrows, sizeof(Boolean));
    lv->listView.band_saved = calloc((size_t)lv->listView.nrows, sizeof(Boolean));
}

static void
BuildColumns(ListViewWidget lv)
{
    FreeColumns(lv);

    int n = lv->listView.ncols_res;
    if (n <= 0)
        return;

    lv->listView.col_info = calloc((size_t)n, sizeof(ListViewColumnInfo));
    lv->listView.col_count = n;
    lv->listView.ncols = n;

    for (int i = 0; i < n; i++) {
        IswListViewColumn *src = &lv->listView.columns_res[i];
        ListViewColumnInfo *dst = &lv->listView.col_info[i];
        dst->title = src->title;
        dst->width = src->width > 0
            ? ISWScaleDim((Widget)lv, src->width)
            : ISWScaleDim((Widget)lv, DEFAULT_COL_W);
        dst->min_width = src->min_width > 0
            ? ISWScaleDim((Widget)lv, src->min_width)
            : ISWScaleDim((Widget)lv, MIN_COL_W);
    }
}

static void
ComputeMetrics(ListViewWidget lv)
{
    Widget w = (Widget)lv;

    /* Row height */
    if (lv->listView.row_height > 0) {
        lv->listView.computed_row_h = ISWScaleDim(w, lv->listView.row_height);
    } else {
        int fh = lv->listView.font
            ? ISWScaledFontHeight(w, lv->listView.font)
            : (int)ISWScaleDim(w, 14);
        lv->listView.computed_row_h = (Dimension)(fh + 2 * ISWScaleDim(w, CELL_PAD_Y));
    }

    /* Header height */
    if (!lv->listView.show_header) {
        lv->listView.computed_hdr_h = 0;
    } else if (lv->listView.header_height > 0) {
        lv->listView.computed_hdr_h = ISWScaleDim(w, lv->listView.header_height);
    } else {
        int fh = lv->listView.font
            ? ISWScaledFontHeight(w, lv->listView.font)
            : (int)ISWScaleDim(w, 14);
        lv->listView.computed_hdr_h = (Dimension)(fh + 2 * ISWScaleDim(w, CELL_PAD_Y) + 2);
    }

    /* Total column width */
    Dimension tw = 0;
    for (int i = 0; i < lv->listView.col_count; i++)
        tw += lv->listView.col_info[i].width;
    lv->listView.total_col_w = tw;

    /* Request preferred size: width = sum of columns, height = header + rows */
    Dimension pref_w = tw > 0 ? tw : ISWScaleDim(w, 300);
    Dimension pref_h = lv->listView.computed_hdr_h
                     + (Dimension)(lv->listView.nrows * (int)lv->listView.computed_row_h);
    if (pref_h < 1) pref_h = 1;

    if (pref_h != w->core.height) {
        Dimension actual_w, actual_h;
        XtMakeResizeRequest(w, w->core.width > 0 ? w->core.width : pref_w,
                            pref_h, &actual_w, &actual_h);
    }
}

static int
RowAtY(ListViewWidget lv, Position y)
{
    int hdr = (int)lv->listView.computed_hdr_h;
    if ((int)y < hdr)
        return -1;
    int row = ((int)y - hdr) / (int)lv->listView.computed_row_h;
    if (row < 0 || row >= lv->listView.nrows)
        return -1;
    return row;
}

static int
ColAtX(ListViewWidget lv, Position x)
{
    int cx = 0;
    for (int i = 0; i < lv->listView.col_count; i++) {
        cx += (int)lv->listView.col_info[i].width;
        if ((int)x < cx)
            return i;
    }
    return -1;
}

/* Check if x is near a column separator for resize */
static int
ResizeHitTest(ListViewWidget lv, Position x, Position y)
{
    if ((int)y >= (int)lv->listView.computed_hdr_h)
        return -1;  /* only in header */
    int cx = 0;
    int grip = (int)ISWScaleDim((Widget)lv, RESIZE_GRIP);
    for (int i = 0; i < lv->listView.col_count; i++) {
        cx += (int)lv->listView.col_info[i].width;
        if (abs((int)x - cx) <= grip)
            return i;
    }
    return -1;
}

static void
ClearSelection(ListViewWidget lv)
{
    if (lv->listView.sel_flags)
        memset(lv->listView.sel_flags, 0,
               (size_t)lv->listView.nrows * sizeof(Boolean));
}

static void
FireCallback(ListViewWidget lv, int clicked_row, int clicked_col)
{
    Widget w = (Widget)lv;
    int *indices = NULL;
    int count = 0;

    if (lv->listView.sel_flags) {
        indices = (int *)XtMalloc((Cardinal)lv->listView.nrows * sizeof(int));
        for (int i = 0; i < lv->listView.nrows; i++) {
            if (lv->listView.sel_flags[i])
                indices[count++] = i;
        }
    }

    IswListViewCallbackData cb;
    cb.row = clicked_row;
    cb.column = clicked_col;
    cb.selected = indices;
    cb.num_selected = count;
    XtCallCallbacks(w, XtNselectCallback, (XtPointer)&cb);

    if (indices)
        XtFree((char *)indices);
}

static void
ResolveForegroundRGB(ListViewWidget lv)
{
    xcb_connection_t *conn = ((Widget)lv)->core.display;
    xcb_colormap_t cmap = ((Widget)lv)->core.colormap;
    uint32_t pixel = (uint32_t)lv->listView.foreground;
    xcb_query_colors_cookie_t cookie = xcb_query_colors(conn, cmap, 1, &pixel);
    xcb_query_colors_reply_t *reply = xcb_query_colors_reply(conn, cookie, NULL);
    if (reply) {
        xcb_rgb_t *rgb = xcb_query_colors_colors(reply);
        if (xcb_query_colors_colors_length(reply) > 0) {
            lv->listView.fg_r = (double)(rgb[0].red >> 8) / 255.0;
            lv->listView.fg_g = (double)(rgb[0].green >> 8) / 255.0;
            lv->listView.fg_b = (double)(rgb[0].blue >> 8) / 255.0;
        }
        free(reply);
    }
}

static void
ScrollToCursor(ListViewWidget lv)
{
    Widget w = (Widget)lv;
    Widget parent = XtParent(w);
    int cur = lv->listView.cursor;

    if (cur < 0 || !parent || !XtIsRealized(w))
        return;

    Dimension visible_h = parent->core.height;
    Position child_y = w->core.y;

    int hdr = (int)lv->listView.computed_hdr_h;
    int item_top = hdr + cur * (int)lv->listView.computed_row_h;
    int item_bot = item_top + (int)lv->listView.computed_row_h;

    int vis_top = -(int)child_y;
    int vis_bot = vis_top + (int)visible_h;

    if (item_top < vis_top) {
        IswViewportSetCoordinates(parent, 0, (Position)item_top);
    } else if (item_bot > vis_bot) {
        Position new_y = (Position)(item_bot - (int)visible_h);
        if (new_y < 0) new_y = 0;
        IswViewportSetCoordinates(parent, 0, new_y);
    }
}

/* ================================================================
 * Widget methods
 * ================================================================ */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListViewWidget lv = (ListViewWidget) new;
    (void)request; (void)args; (void)num_args;

    lv->listView.col_info = NULL;
    lv->listView.col_count = 0;
    lv->listView.sel_flags = NULL;
    lv->listView.band_saved = NULL;
    lv->listView.anchor = -1;
    lv->listView.cursor = -1;
    lv->listView.has_focus = False;
    lv->listView.render_ctx = NULL;
    lv->listView.band_active = False;
    lv->listView.redraw_pending = False;
    lv->listView.work_proc_id = 0;
    lv->listView.deselect_pending = False;
    lv->listView.deselect_index = -1;
    lv->listView.col_resize_active = False;
    lv->listView.col_resize_index = -1;
    lv->listView.total_col_w = 0;
    lv->listView.sort_column = -1;
    lv->listView.sort_direction = IswListViewSortNone;

    BuildColumns(lv);
    AllocSelFlags(lv);

    if (new->core.width == 0)
        new->core.width = ISWScaleDim(new, 400);
    if (new->core.height == 0)
        new->core.height = ISWScaleDim(new, 200);

    ComputeMetrics(lv);
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    ListViewWidget lv = (ListViewWidget) w;

    (*listViewWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    lv->listView.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

static void
Destroy(Widget w)
{
    ListViewWidget lv = (ListViewWidget) w;
    if (lv->listView.work_proc_id)
        XtRemoveWorkProc(lv->listView.work_proc_id);
    FreeColumns(lv);
    FreeSelFlags(lv);
    if (lv->listView.render_ctx)
        ISWRenderDestroy(lv->listView.render_ctx);
}

static void
Resize(Widget w)
{
    ListViewWidget lv = (ListViewWidget) w;
    ComputeMetrics(lv);
    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);
}

/* ================================================================
 * Rendering
 * ================================================================ */

static void
DrawHeader(ListViewWidget lv, ISWRenderContext *ctx)
{
    Widget w = (Widget)lv;
    Dimension hdr_h = lv->listView.computed_hdr_h;
    Dimension pad_x = ISWScaleDim(w, CELL_PAD_X);

    if (!hdr_h)
        return;

    /* Header background: slightly darker than widget background */
    ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                          lv->listView.fg_b, 0.08);
    ISWRenderFillRectangle(ctx, 0, 0, (int)w->core.width, (int)hdr_h);

    /* Header bottom line */
    ISWRenderSetColor(ctx, lv->listView.foreground);
    ISWRenderSetLineWidth(ctx, 1.0);
    ISWRenderDrawLine(ctx, 0, (int)hdr_h - 1, (int)w->core.width, (int)hdr_h - 1);

    if (lv->listView.font)
        ISWRenderSetFont(ctx, lv->listView.font);

    int ascent = lv->listView.font
        ? ISWScaledFontAscent(w, lv->listView.font)
        : (int)ISWScaleDim(w, 11);

    int x = 0;
    for (int i = 0; i < lv->listView.col_count; i++) {
        Dimension cw = lv->listView.col_info[i].width;

        /* Column separator line */
        if (i > 0) {
            ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                                  lv->listView.fg_b, 0.2);
            ISWRenderDrawLine(ctx, x, 0, x, (int)hdr_h);
        }

        /* Title text and sort arrow */
        {
            int text_x = x + (int)pad_x;
            int avail_w = (int)cw - 2 * (int)pad_x;
            int ty = ((int)hdr_h - 1 + ascent) / 2;

            /* Reserve space for sort arrow if this is the sorted column */
            int arrow_w_scaled = (int)ISWScaleDim(w, SORT_ARROW_W);
            int arrow_h_scaled = (int)ISWScaleDim(w, SORT_ARROW_H);
            int arrow_gap = (int)ISWScaleDim(w, SORT_ARROW_GAP);
            Boolean is_sorted = (lv->listView.sort_column == i &&
                                 lv->listView.sort_direction != IswListViewSortNone);

            if (is_sorted)
                avail_w -= arrow_w_scaled + arrow_gap;

            if (lv->listView.col_info[i].title) {
                const char *title = lv->listView.col_info[i].title;
                int len = (int)strlen(title);

                ISWRenderSetColor(ctx, lv->listView.foreground);
                ISWRenderSetClipRectangle(ctx, text_x, 0, avail_w, (int)hdr_h);
                ISWRenderDrawString(ctx, title, len, text_x, ty);
                ISWRenderClearClip(ctx);

                /* Draw sort direction arrow right of the text */
                if (is_sorted) {
                    int tw = ISWRenderTextWidth(ctx, title, len);
                    if (tw > avail_w)
                        tw = avail_w;
                    int ax = text_x + tw + arrow_gap;
                    int ay = ((int)hdr_h - arrow_h_scaled) / 2;

                    ISWRenderSetColor(ctx, lv->listView.foreground);

                    xcb_point_t pts[3];
                    if (lv->listView.sort_direction == IswListViewSortAscending) {
                        /* Up arrow: triangle pointing up */
                        pts[0] = (xcb_point_t){ax + arrow_w_scaled / 2, ay};
                        pts[1] = (xcb_point_t){ax, ay + arrow_h_scaled};
                        pts[2] = (xcb_point_t){ax + arrow_w_scaled, ay + arrow_h_scaled};
                    } else {
                        /* Down arrow: triangle pointing down */
                        pts[0] = (xcb_point_t){ax, ay};
                        pts[1] = (xcb_point_t){ax + arrow_w_scaled, ay};
                        pts[2] = (xcb_point_t){ax + arrow_w_scaled / 2, ay + arrow_h_scaled};
                    }
                    ISWRenderFillPolygon(ctx, pts, 3);
                }
            } else if (is_sorted) {
                /* No title but sorted — draw arrow centered */
                int ax = x + ((int)cw - arrow_w_scaled) / 2;
                int ay = ((int)hdr_h - arrow_h_scaled) / 2;

                ISWRenderSetColor(ctx, lv->listView.foreground);

                xcb_point_t pts[3];
                if (lv->listView.sort_direction == IswListViewSortAscending) {
                    pts[0] = (xcb_point_t){ax + arrow_w_scaled / 2, ay};
                    pts[1] = (xcb_point_t){ax, ay + arrow_h_scaled};
                    pts[2] = (xcb_point_t){ax + arrow_w_scaled, ay + arrow_h_scaled};
                } else {
                    pts[0] = (xcb_point_t){ax, ay};
                    pts[1] = (xcb_point_t){ax + arrow_w_scaled, ay};
                    pts[2] = (xcb_point_t){ax + arrow_w_scaled / 2, ay + arrow_h_scaled};
                }
                ISWRenderFillPolygon(ctx, pts, 3);
            }
        }

        x += (int)cw;
    }
}

static void
DrawRows(ListViewWidget lv, ISWRenderContext *ctx)
{
    Widget w = (Widget)lv;
    Dimension hdr_h = lv->listView.computed_hdr_h;
    Dimension row_h = lv->listView.computed_row_h;
    Dimension pad_x = ISWScaleDim(w, CELL_PAD_X);

    if (lv->listView.font)
        ISWRenderSetFont(ctx, lv->listView.font);

    int ascent = lv->listView.font
        ? ISWScaledFontAscent(w, lv->listView.font)
        : (int)ISWScaleDim(w, 11);

    for (int row = 0; row < lv->listView.nrows; row++) {
        int ry = (int)hdr_h + row * (int)row_h;

        /* Selection highlight */
        if (lv->listView.sel_flags && lv->listView.sel_flags[row]) {
            ISWRenderSetColor(ctx, lv->listView.foreground);
            ISWRenderFillRectangle(ctx, 0, ry, (int)w->core.width, (int)row_h);
        }

        /* Alternating row tint for unselected rows */
        if ((!lv->listView.sel_flags || !lv->listView.sel_flags[row]) &&
            (row & 1)) {
            ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                                  lv->listView.fg_b, 0.03);
            ISWRenderFillRectangle(ctx, 0, ry, (int)w->core.width, (int)row_h);
        }

        /* Cell text */
        int cx = 0;
        for (int col = 0; col < lv->listView.col_count; col++) {
            Dimension cw = lv->listView.col_info[col].width;

            /* Column separator in row area */
            if (col > 0) {
                ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                                      lv->listView.fg_b, 0.1);
                ISWRenderDrawLine(ctx, cx, ry, cx, ry + (int)row_h);
            }

            const char *text = NULL;
            if (lv->listView.data && col < lv->listView.ncols &&
                row < lv->listView.nrows)
                text = lv->listView.data[row * lv->listView.ncols + col];

            if (text) {
                int len = (int)strlen(text);
                if (lv->listView.sel_flags && lv->listView.sel_flags[row])
                    ISWRenderSetColor(ctx, w->core.background_pixel);
                else
                    ISWRenderSetColor(ctx, lv->listView.foreground);

                ISWRenderSetClipRectangle(ctx, cx + (int)pad_x, ry,
                                          (int)cw - 2 * (int)pad_x, (int)row_h);
                int ty = ry + ((int)row_h + ascent) / 2;
                ISWRenderDrawString(ctx, text, len, cx + (int)pad_x, ty);
                ISWRenderClearClip(ctx);
            }

            cx += (int)cw;
        }
    }
}

static void
DrawCursor(ListViewWidget lv, ISWRenderContext *ctx)
{
    if (!lv->listView.has_focus || lv->listView.cursor < 0 ||
        lv->listView.cursor >= lv->listView.nrows)
        return;

    Widget w = (Widget)lv;
    int hdr = (int)lv->listView.computed_hdr_h;
    int ry = hdr + lv->listView.cursor * (int)lv->listView.computed_row_h;
    int rw = (int)w->core.width;
    int rh = (int)lv->listView.computed_row_h;

    ISWRenderSetColor(ctx, lv->listView.foreground);
    ISWRenderSetLineWidth(ctx, 1.0);

    int dash = 3, gap = 3, step = dash + gap;
    for (int dx = 0; dx < rw; dx += step) {
        int seg = (dx + dash > rw) ? rw - dx : dash;
        ISWRenderDrawLine(ctx, dx, ry, dx + seg, ry);
        ISWRenderDrawLine(ctx, dx, ry + rh, dx + seg, ry + rh);
    }
    for (int dy = 0; dy < rh; dy += step) {
        int seg = (dy + dash > rh) ? rh - dy : dash;
        ISWRenderDrawLine(ctx, 0, ry + dy, 0, ry + dy + seg);
        ISWRenderDrawLine(ctx, rw, ry + dy, rw, ry + dy + seg);
    }
}

static void
DrawBand(ListViewWidget lv, ISWRenderContext *ctx)
{
    if (!lv->listView.band_active)
        return;

    int bx = lv->listView.band_start_x < lv->listView.band_cur_x
           ? lv->listView.band_start_x : lv->listView.band_cur_x;
    int by = lv->listView.band_start_y < lv->listView.band_cur_y
           ? lv->listView.band_start_y : lv->listView.band_cur_y;
    int bw = abs(lv->listView.band_cur_x - lv->listView.band_start_x);
    int bh = abs(lv->listView.band_cur_y - lv->listView.band_start_y);

    if (bw > 0 && bh > 0) {
        ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                              lv->listView.fg_b, 0.15);
        ISWRenderFillRectangle(ctx, bx, by, bw, bh);

        ISWRenderSetColorRGBA(ctx, lv->listView.fg_r, lv->listView.fg_g,
                              lv->listView.fg_b, 1.0);
        ISWRenderSetLineWidth(ctx, 1.0);
        ISWRenderStrokeRectangle(ctx, bx, by, bw, bh);
    }
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    ListViewWidget lv = (ListViewWidget) w;
    ISWRenderContext *ctx = lv->listView.render_ctx;
    (void)event; (void)region;

    if (!ctx || !XtIsRealized(w))
        return;

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    DrawRows(lv, ctx);
    DrawHeader(lv, ctx);
    DrawCursor(lv, ctx);
    DrawBand(lv, ctx);

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired,
          ArgList args, Cardinal *num_args)
{
    ListViewWidget clv = (ListViewWidget) current;
    ListViewWidget dlv = (ListViewWidget) desired;
    Boolean redraw = FALSE;
    (void)request; (void)args; (void)num_args;

    if (clv->listView.columns_res != dlv->listView.columns_res ||
        clv->listView.ncols_res != dlv->listView.ncols_res) {
        BuildColumns(dlv);
        ComputeMetrics(dlv);
        redraw = TRUE;
    }

    if (clv->listView.nrows != dlv->listView.nrows ||
        clv->listView.data != dlv->listView.data) {
        AllocSelFlags(dlv);
        ComputeMetrics(dlv);
        redraw = TRUE;
    }

    if (clv->listView.foreground != dlv->listView.foreground ||
        clv->core.background_pixel != dlv->core.background_pixel ||
        clv->listView.font != dlv->listView.font ||
        clv->listView.show_header != dlv->listView.show_header) {
        ComputeMetrics(dlv);
        redraw = TRUE;
    }

    return redraw;
}

/* ================================================================
 * Rubber band selection (row-based)
 * ================================================================ */

static void
BandUpdateSelection(ListViewWidget lv)
{
    int by1 = lv->listView.band_start_y < lv->listView.band_cur_y
            ? lv->listView.band_start_y : lv->listView.band_cur_y;
    int by2 = lv->listView.band_start_y > lv->listView.band_cur_y
            ? lv->listView.band_start_y : lv->listView.band_cur_y;

    int hdr = (int)lv->listView.computed_hdr_h;
    int rh = (int)lv->listView.computed_row_h;

    for (int i = 0; i < lv->listView.nrows; i++) {
        int ry1 = hdr + i * rh;
        int ry2 = ry1 + rh;

        Boolean intersects = !(ry2 < by1 || ry1 > by2);

        if (lv->listView.band_saved)
            lv->listView.sel_flags[i] = lv->listView.band_saved[i] || intersects;
        else
            lv->listView.sel_flags[i] = intersects;
    }
}

static Boolean
BandRedrawWorkProc(XtPointer closure)
{
    Widget w = (Widget) closure;
    ListViewWidget lv = (ListViewWidget) w;

    lv->listView.work_proc_id = 0;
    if (lv->listView.redraw_pending) {
        lv->listView.redraw_pending = False;
        BandUpdateSelection(lv);
        Redisplay(w, NULL, 0);
    }
    return True;
}

/* ================================================================
 * Actions
 * ================================================================ */

static void
SelectRow(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)params; (void)num_params;
    Position x, y;

    uint8_t type = event->response_type & ~0x80;
    if (type == XCB_BUTTON_PRESS) {
        xcb_button_press_event_t *ev = (xcb_button_press_event_t *)event;
        x = ev->event_x;
        y = ev->event_y;
    } else {
        return;
    }

    /* Column resize? */
    int resize_col = ResizeHitTest(lv, x, y);
    if (resize_col >= 0) {
        lv->listView.col_resize_active = True;
        lv->listView.col_resize_index = resize_col;
        lv->listView.col_resize_start_x = x;
        lv->listView.col_resize_start_w = lv->listView.col_info[resize_col].width;
        return;
    }

    /* Click in header (not on separator) — toggle sort */
    if ((int)y < (int)lv->listView.computed_hdr_h) {
        int col = ColAtX(lv, x);
        if (col >= 0) {
            /* Toggle sort direction for this column */
            if (lv->listView.sort_column == col) {
                if (lv->listView.sort_direction == IswListViewSortAscending)
                    lv->listView.sort_direction = IswListViewSortDescending;
                else
                    lv->listView.sort_direction = IswListViewSortAscending;
            } else {
                lv->listView.sort_column = col;
                lv->listView.sort_direction = IswListViewSortAscending;
            }

            IswListViewReorderCallbackData cb;
            cb.column = col;
            cb.direction = lv->listView.sort_direction;
            XtCallCallbacks(w, XtNreorderCallback, (XtPointer)&cb);
            Redisplay(w, NULL, 0);
        }
        return;
    }

    int row = RowAtY(lv, y);
    int col = ColAtX(lv, x);

    uint16_t state = ((xcb_button_press_event_t *)event)->state;
    Boolean toggle = (state & XCB_MOD_MASK_CONTROL) != 0;
    Boolean extend = (state & XCB_MOD_MASK_SHIFT) != 0;

    if (!lv->listView.sel_flags)
        return;

    if (lv->listView.band_active)
        lv->listView.band_active = False;

    if (row < 0) {
        /* Click below last row — start rubber band if multi-select */
        if (lv->listView.multi_select && XtIsRealized(w)) {
            if (!toggle)
                ClearSelection(lv);
            if (lv->listView.band_saved && lv->listView.sel_flags)
                memcpy(lv->listView.band_saved, lv->listView.sel_flags,
                       (size_t)lv->listView.nrows * sizeof(Boolean));

            lv->listView.band_active = True;
            lv->listView.band_start_x = x;
            lv->listView.band_start_y = y;
            lv->listView.band_cur_x = x;
            lv->listView.band_cur_y = y;
            ResolveForegroundRGB(lv);
            Redisplay(w, NULL, 0);
            return;
        }
        ClearSelection(lv);
        lv->listView.anchor = -1;
    } else if (lv->listView.multi_select && toggle) {
        lv->listView.sel_flags[row] = !lv->listView.sel_flags[row];
        lv->listView.anchor = row;
    } else if (lv->listView.multi_select && extend &&
               lv->listView.anchor >= 0) {
        ClearSelection(lv);
        int lo = lv->listView.anchor < row ? lv->listView.anchor : row;
        int hi = lv->listView.anchor > row ? lv->listView.anchor : row;
        for (int i = lo; i <= hi; i++)
            lv->listView.sel_flags[i] = True;
    } else if (lv->listView.multi_select && lv->listView.sel_flags[row]) {
        lv->listView.deselect_pending = True;
        lv->listView.deselect_index = row;
    } else {
        /* Plain click or start rubber band from a row */
        if (lv->listView.multi_select && XtIsRealized(w)) {
            /* Allow rubber band starting from a row area too */
            ClearSelection(lv);
            lv->listView.sel_flags[row] = True;
            lv->listView.anchor = row;

            /* Also set up band start in case user drags */
            if (lv->listView.band_saved && lv->listView.sel_flags)
                memcpy(lv->listView.band_saved, lv->listView.sel_flags,
                       (size_t)lv->listView.nrows * sizeof(Boolean));
            lv->listView.band_start_x = x;
            lv->listView.band_start_y = y;
            lv->listView.band_cur_x = x;
            lv->listView.band_cur_y = y;
            ResolveForegroundRGB(lv);
        } else {
            ClearSelection(lv);
            lv->listView.sel_flags[row] = True;
            lv->listView.anchor = row;
        }
    }

    if (row >= 0)
        lv->listView.cursor = row;

    Redisplay(w, NULL, 0);
    FireCallback(lv, row, col);
}

static void
BandDrag(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)params; (void)num_params;

    uint8_t type = event->response_type & ~0x80;
    if (type != XCB_MOTION_NOTIFY)
        return;

    xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
    Position new_x = ev->event_x;
    Position new_y = ev->event_y;

    /* Column resize drag */
    if (lv->listView.col_resize_active) {
        int ci = lv->listView.col_resize_index;
        if (ci >= 0 && ci < lv->listView.col_count) {
            int delta = (int)new_x - (int)lv->listView.col_resize_start_x;
            int new_w = (int)lv->listView.col_resize_start_w + delta;
            Dimension min_w = lv->listView.col_info[ci].min_width;
            if (new_w < (int)min_w)
                new_w = (int)min_w;
            lv->listView.col_info[ci].width = (Dimension)new_w;
            ComputeMetrics(lv);
            Redisplay(w, NULL, 0);
        }
        return;
    }

    lv->listView.deselect_pending = False;

    /* Activate band on first drag if started from a row */
    if (!lv->listView.band_active && lv->listView.multi_select &&
        lv->listView.band_start_x != 0) {
        int dx = abs((int)new_x - (int)lv->listView.band_start_x);
        int dy = abs((int)new_y - (int)lv->listView.band_start_y);
        if (dx > 3 || dy > 3)
            lv->listView.band_active = True;
    }

    if (!lv->listView.band_active)
        return;

    if (new_x == lv->listView.band_cur_x && new_y == lv->listView.band_cur_y)
        return;

    lv->listView.band_cur_x = new_x;
    lv->listView.band_cur_y = new_y;

    lv->listView.redraw_pending = True;
    if (!lv->listView.work_proc_id) {
        lv->listView.work_proc_id = XtAppAddWorkProc(
            XtWidgetToApplicationContext(w), BandRedrawWorkProc, (XtPointer)w);
    }
}

static void
BandFinish(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event; (void)params; (void)num_params;

    /* Column resize end */
    if (lv->listView.col_resize_active) {
        lv->listView.col_resize_active = False;
        lv->listView.col_resize_index = -1;
        return;
    }

    /* Deferred deselect */
    if (lv->listView.deselect_pending) {
        lv->listView.deselect_pending = False;
        int idx = lv->listView.deselect_index;
        ClearSelection(lv);
        if (idx >= 0 && idx < lv->listView.nrows)
            lv->listView.sel_flags[idx] = True;
        lv->listView.anchor = idx;
        lv->listView.band_active = False;
        Redisplay(w, NULL, 0);
        FireCallback(lv, idx, -1);
        return;
    }

    if (!lv->listView.band_active) {
        /* Reset band start position */
        lv->listView.band_start_x = 0;
        lv->listView.band_start_y = 0;
        return;
    }

    lv->listView.band_active = False;

    if (lv->listView.work_proc_id) {
        XtRemoveWorkProc(lv->listView.work_proc_id);
        lv->listView.work_proc_id = 0;
    }
    lv->listView.redraw_pending = False;
    lv->listView.band_start_x = 0;
    lv->listView.band_start_y = 0;

    Redisplay(w, NULL, 0);
    FireCallback(lv, -1, -1);
}

/* ================================================================
 * Keyboard navigation
 * ================================================================ */

static int
ComputeNewCursor(ListViewWidget lv, const char *direction)
{
    int cur = lv->listView.cursor;
    int n = lv->listView.nrows;

    if (n <= 0) return -1;
    if (cur < 0) return 0;

    if (strcmp(direction, "up") == 0)
        return (cur > 0) ? cur - 1 : cur;
    else if (strcmp(direction, "down") == 0)
        return (cur < n - 1) ? cur + 1 : cur;
    else if (strcmp(direction, "home") == 0)
        return 0;
    else if (strcmp(direction, "end") == 0)
        return n - 1;
    return cur;
}

static void
MoveCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1 || lv->listView.nrows <= 0)
        return;

    int new_cur = ComputeNewCursor(lv, params[0]);
    if (new_cur == lv->listView.cursor && lv->listView.cursor >= 0)
        return;

    lv->listView.cursor = new_cur;

    if (lv->listView.sel_flags) {
        ClearSelection(lv);
        lv->listView.sel_flags[new_cur] = True;
    }
    lv->listView.anchor = new_cur;

    ScrollToCursor(lv);
    Redisplay(w, NULL, 0);
}

static void
ExtendSelection(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1 || lv->listView.nrows <= 0)
        return;
    if (!lv->listView.multi_select || !lv->listView.sel_flags)
        return;

    int new_cur = ComputeNewCursor(lv, params[0]);
    lv->listView.cursor = new_cur;

    if (lv->listView.anchor < 0)
        lv->listView.anchor = new_cur;

    ClearSelection(lv);
    int lo = lv->listView.anchor < new_cur ? lv->listView.anchor : new_cur;
    int hi = lv->listView.anchor > new_cur ? lv->listView.anchor : new_cur;
    for (int i = lo; i <= hi; i++)
        lv->listView.sel_flags[i] = True;

    ScrollToCursor(lv);
    Redisplay(w, NULL, 0);
}

static void
ActivateCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (lv->listView.cursor < 0 || lv->listView.cursor >= lv->listView.nrows)
        return;

    if (lv->listView.sel_flags) {
        ClearSelection(lv);
        lv->listView.sel_flags[lv->listView.cursor] = True;
    }

    Redisplay(w, NULL, 0);
    FireCallback(lv, lv->listView.cursor, -1);
}

static void
ToggleCursor(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (lv->listView.cursor < 0 || lv->listView.cursor >= lv->listView.nrows)
        return;
    if (!lv->listView.sel_flags)
        return;

    if (lv->listView.multi_select) {
        lv->listView.sel_flags[lv->listView.cursor] =
            !lv->listView.sel_flags[lv->listView.cursor];
    } else {
        ClearSelection(lv);
        lv->listView.sel_flags[lv->listView.cursor] = True;
    }

    Redisplay(w, NULL, 0);
}

static void
SelectAll(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event; (void)params; (void)num_params;

    if (!lv->listView.multi_select || !lv->listView.sel_flags)
        return;

    for (int i = 0; i < lv->listView.nrows; i++)
        lv->listView.sel_flags[i] = True;

    Redisplay(w, NULL, 0);
}

static void
HandleFocus(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    ListViewWidget lv = (ListViewWidget) w;
    (void)event;

    if (!num_params || *num_params < 1)
        return;

    if (strcmp(params[0], "in") == 0) {
        lv->listView.has_focus = True;
        if (lv->listView.cursor < 0 && lv->listView.nrows > 0)
            lv->listView.cursor = 0;
    } else {
        lv->listView.has_focus = False;
    }

    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);
}

/* ================================================================
 * Public API
 * ================================================================ */

void
IswListViewSetData(Widget w, String *data, int nrows, int ncols)
{
    ListViewWidget lv = (ListViewWidget) w;
    /* data is a flat array: data[row * ncols + col] */
    Arg args[3];
    XtSetArg(args[0], XtNlistViewData, data);
    XtSetArg(args[1], XtNnumRows, nrows);
    XtSetArg(args[2], XtNnumColumns, ncols);
    /* Update ncols to match data layout (columns_res drives column headers,
     * ncols in data can differ if user hasn't set columns yet) */
    lv->listView.ncols = ncols;
    XtSetValues(w, args, 3);
}

void
IswListViewSetColumns(Widget w, IswListViewColumn *cols, int ncols)
{
    Arg args[2];
    XtSetArg(args[0], XtNlistViewColumns, cols);
    XtSetArg(args[1], XtNnumColumns, ncols);
    XtSetValues(w, args, 2);
}

int
IswListViewAddColumn(Widget w, const char *title, Dimension width, Dimension min_width)
{
    ListViewWidget lv = (ListViewWidget) w;
    int n = lv->listView.col_count + 1;

    lv->listView.col_info = realloc(lv->listView.col_info,
                                    (size_t)n * sizeof(ListViewColumnInfo));
    ListViewColumnInfo *ci = &lv->listView.col_info[n - 1];
    ci->title = (String)title;
    ci->width = width > 0
        ? ISWScaleDim(w, width)
        : ISWScaleDim(w, DEFAULT_COL_W);
    ci->min_width = min_width > 0
        ? ISWScaleDim(w, min_width)
        : ISWScaleDim(w, MIN_COL_W);

    lv->listView.col_count = n;
    lv->listView.ncols = n;
    ComputeMetrics(lv);

    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);

    return n - 1;  /* index of new column */
}

int
IswListViewGetSelected(Widget w)
{
    ListViewWidget lv = (ListViewWidget) w;
    if (!lv->listView.sel_flags)
        return -1;
    for (int i = 0; i < lv->listView.nrows; i++) {
        if (lv->listView.sel_flags[i])
            return i;
    }
    return -1;
}

int
IswListViewGetSelectedRows(Widget w, int **indices_out)
{
    ListViewWidget lv = (ListViewWidget) w;
    int count = 0;

    if (!lv->listView.sel_flags || !indices_out) {
        if (indices_out) *indices_out = NULL;
        return 0;
    }

    int *buf = (int *)XtMalloc((Cardinal)lv->listView.nrows * sizeof(int));
    for (int i = 0; i < lv->listView.nrows; i++) {
        if (lv->listView.sel_flags[i])
            buf[count++] = i;
    }

    if (count == 0) {
        XtFree((char *)buf);
        *indices_out = NULL;
        return 0;
    }

    *indices_out = buf;
    return count;
}

Boolean
IswListViewBandActive(Widget w)
{
    ListViewWidget lv = (ListViewWidget) w;
    return lv->listView.band_active;
}

void
IswListViewSetSort(Widget w, int column, IswListViewSortDirection direction)
{
    ListViewWidget lv = (ListViewWidget) w;
    lv->listView.sort_column = column;
    lv->listView.sort_direction = direction;
    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);
}
