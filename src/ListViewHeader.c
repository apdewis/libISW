/*
 * ListViewHeader.c - Column header strip for the ListView widget
 *
 * Draws column titles, separators, and sort arrows, and owns the
 * header-local interaction: click-to-sort and drag-to-resize columns.
 * Created internally by ListView (showHeader); the column array is
 * shared by pointer from the parent and never written here — resize
 * drags are reported through columnResizeCallback and the parent
 * applies the width.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/IntrinsicI.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ListViewHeaderP.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define CELL_PAD_X   6
#define CELL_PAD_Y   2
#define RESIZE_GRIP  4   /* pixels from column separator edge for resize cursor */
#define SORT_ARROW_W 7   /* width of sort direction arrow */
#define SORT_ARROW_H 4   /* height of sort direction arrow */
#define SORT_ARROW_GAP 4 /* gap between title text and arrow */

#define Offset(field) IswOffsetOf(ListViewHeaderRec, field)

static IswResource resources[] = {
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
        Offset(listViewHeader.foreground), IswRString, IswDefaultForeground},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
        Offset(listViewHeader.font), IswRString, IswDefaultFont},
    {IswNheaderHeight, IswCHeaderHeight, IswRDimension, sizeof(Dimension),
        Offset(listViewHeader.header_height), IswRImmediate, (IswPointer) 0},
    {IswNsortCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(listViewHeader.sort_callback), IswRCallback, NULL},
    {IswNcolumnResizeCallback, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(listViewHeader.resize_callback), IswRCallback, NULL},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
};

#undef Offset

/* Forward declarations */
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Realize(IswDisplay, Widget, IswValueMask *, uint32_t *);
static void Resize(Widget);
static void Redisplay(Widget, IswEvent *, IswRegion);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void HeaderPress(Widget, IswEvent *, String *, Cardinal *);
static void HeaderDrag(Widget, IswEvent *, String *, Cardinal *);
static void HeaderRelease(Widget, IswEvent *, String *, Cardinal *);
static void HeaderMotion(Widget, IswEvent *, String *, Cardinal *);

static char defaultTranslations[] =
    "<PrimaryDown>: HeaderPress()\n"
    "<PrimaryMotion>: HeaderDrag()\n"
    "<PrimaryUp>: HeaderRelease()\n"
    "<Motion>: HeaderMotion()";

static IswActionsRec actions[] = {
    {"HeaderPress",   HeaderPress},
    {"HeaderDrag",    HeaderDrag},
    {"HeaderRelease", HeaderRelease},
    {"HeaderMotion",  HeaderMotion},
};

ListViewHeaderClassRec listViewHeaderClassRec = {
  { /* core */
    (WidgetClass) &simpleClassRec,
    "ListViewHeader",
    sizeof(ListViewHeaderRec),
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
    ISW_NULLQUARK,
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
  { /* listViewHeader */
    0
  }
};

WidgetClass listViewHeaderWidgetClass = (WidgetClass)&listViewHeaderClassRec;

/* ================================================================
 * Internal helpers
 * ================================================================ */

static int
ColAtX(ListViewHeaderWidget hw, Position x)
{
    int cx = 0;
    for (int i = 0; i < hw->listViewHeader.col_count; i++) {
        cx += (int)hw->listViewHeader.col_info[i].width;
        if ((int)x < cx)
            return i;
    }
    return -1;
}

/* Check if x is near a column separator for resize */
static int
ResizeHitTest(ListViewHeaderWidget hw, Position x)
{
    int cx = 0;
    int grip = (int)(RESIZE_GRIP);
    for (int i = 0; i < hw->listViewHeader.col_count; i++) {
        cx += (int)hw->listViewHeader.col_info[i].width;
        if (abs((int)x - cx) <= grip)
            return i;
    }
    return -1;
}

static void
UpdateResizeCursor(ListViewHeaderWidget hw, Boolean over_grip)
{
    Widget w = (Widget)hw;
    if (!IswIsRealized(w))
        return;

    if (over_grip && !hw->listViewHeader.resize_cursor_set) {
        ((SimpleWidget)w)->simple.cursor = hw->listViewHeader.resize_cursor;
        _IswSimpleApplyCursor(w);
        hw->listViewHeader.resize_cursor_set = True;
    } else if (!over_grip && hw->listViewHeader.resize_cursor_set) {
        ((SimpleWidget)w)->simple.cursor = hw->listViewHeader.default_cursor;
        _IswSimpleApplyCursor(w);
        hw->listViewHeader.resize_cursor_set = False;
    }
}

/* ================================================================
 * Widget methods
 * ================================================================ */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) new;
    (void)request; (void)args; (void)num_args;

    hw->listViewHeader.col_info = NULL;
    hw->listViewHeader.col_count = 0;
    hw->listViewHeader.sort_column = -1;
    hw->listViewHeader.sort_direction = IswListViewSortNone;
    hw->listViewHeader.render_ctx = NULL;
    hw->listViewHeader.col_resize_active = False;
    hw->listViewHeader.col_resize_index = -1;
    hw->listViewHeader.resize_cursor = IswCursorNone;
    hw->listViewHeader.default_cursor = IswCursorNone;
    hw->listViewHeader.resize_cursor_set = False;

    if (new->core.width == 0)
        new->core.width = 1;    /* parent resizes us to its width */
    if (new->core.height == 0)
        new->core.height = _IswListViewHeaderPreferredHeight(new);
}

static void
Realize(IswDisplay dpy, Widget w, IswValueMask *valueMask, uint32_t *attributes)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;

    (*listViewHeaderWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    hw->listViewHeader.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);

    hw->listViewHeader.resize_cursor = _IswLoadThemedCursor(dpy, w->core.screen,
        "sb_h_double_arrow");
    hw->listViewHeader.default_cursor = ((SimpleWidget)w)->simple.cursor;
}

static void
Destroy(Widget w)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    if (hw->listViewHeader.render_ctx)
        ISWRenderDestroy(hw->listViewHeader.render_ctx);
    _IswFreeCursor(w, hw->listViewHeader.resize_cursor);
}

static void
Resize(Widget w)
{
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

/* ================================================================
 * Rendering
 * ================================================================ */

static void
Redisplay(Widget w, IswEvent *event, IswRegion region)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    ISWRenderContext *ctx = hw->listViewHeader.render_ctx;
    Dimension hdr_h = w->core.height;
    Dimension pad_x = (CELL_PAD_X);
    (void)event; (void)region;

    if (!ctx || !IswIsRealized(w))
        return;

    ISWRenderBegin(ctx);

    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, (int)w->core.width, (int)hdr_h);

    /* Header background: slightly darker than widget background */
    ISWRenderSetColor(ctx, ISW_PIXEL_WITH_ALPHA_F(hw->listViewHeader.foreground, 0.08));
    ISWRenderFillRectangle(ctx, 0, 0, (int)w->core.width, (int)hdr_h);

    /* Header bottom line */
    ISWRenderSetColor(ctx, hw->listViewHeader.foreground);
    ISWRenderSetLineWidth(ctx, 1.0);
    ISWRenderDrawLine(ctx, 0, (int)hdr_h - 1, (int)w->core.width, (int)hdr_h - 1);

    if (hw->listViewHeader.font)
        ISWRenderSetFont(ctx, hw->listViewHeader.font);

    int ascent = hw->listViewHeader.font
        ? ISWScaledFontAscent(w, hw->listViewHeader.font)
        : (int)(11);

    int x = 0;
    for (int i = 0; i < hw->listViewHeader.col_count; i++) {
        Dimension cw = hw->listViewHeader.col_info[i].width;

        /* Column separator line */
        if (i > 0) {
            ISWRenderSetColor(ctx, ISW_PIXEL_WITH_ALPHA_F(hw->listViewHeader.foreground, 0.2));
            ISWRenderDrawLine(ctx, x, 0, x, (int)hdr_h);
        }

        /* Title text and sort arrow */
        int text_x = x + (int)pad_x;
        int avail_w = (int)cw - 2 * (int)pad_x;
        int ty = ((int)hdr_h - 1 + ascent) / 2;

        /* Reserve space for sort arrow if this is the sorted column */
        int arrow_w_scaled = (int)(SORT_ARROW_W);
        int arrow_h_scaled = (int)(SORT_ARROW_H);
        int arrow_gap = (int)(SORT_ARROW_GAP);
        Boolean is_sorted = (hw->listViewHeader.sort_column == i &&
                             hw->listViewHeader.sort_direction != IswListViewSortNone);

        if (is_sorted)
            avail_w -= arrow_w_scaled + arrow_gap;

        if (hw->listViewHeader.col_info[i].title) {
            const char *title = hw->listViewHeader.col_info[i].title;
            int len = (int)strlen(title);

            ISWRenderSetColor(ctx, hw->listViewHeader.foreground);
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

                ISWRenderSetColor(ctx, hw->listViewHeader.foreground);

                IswPoint pts[3];
                if (hw->listViewHeader.sort_direction == IswListViewSortAscending) {
                    /* Up arrow: triangle pointing up */
                    pts[0] = (IswPoint){ax + arrow_w_scaled / 2, ay};
                    pts[1] = (IswPoint){ax, ay + arrow_h_scaled};
                    pts[2] = (IswPoint){ax + arrow_w_scaled, ay + arrow_h_scaled};
                } else {
                    /* Down arrow: triangle pointing down */
                    pts[0] = (IswPoint){ax, ay};
                    pts[1] = (IswPoint){ax + arrow_w_scaled, ay};
                    pts[2] = (IswPoint){ax + arrow_w_scaled / 2, ay + arrow_h_scaled};
                }
                ISWRenderFillPolygon(ctx, pts, 3);
            }
        } else if (is_sorted) {
            /* No title but sorted — draw arrow centered */
            int ax = x + ((int)cw - arrow_w_scaled) / 2;
            int ay = ((int)hdr_h - arrow_h_scaled) / 2;

            ISWRenderSetColor(ctx, hw->listViewHeader.foreground);

            IswPoint pts[3];
            if (hw->listViewHeader.sort_direction == IswListViewSortAscending) {
                pts[0] = (IswPoint){ax + arrow_w_scaled / 2, ay};
                pts[1] = (IswPoint){ax, ay + arrow_h_scaled};
                pts[2] = (IswPoint){ax + arrow_w_scaled, ay + arrow_h_scaled};
            } else {
                pts[0] = (IswPoint){ax, ay};
                pts[1] = (IswPoint){ax + arrow_w_scaled, ay};
                pts[2] = (IswPoint){ax + arrow_w_scaled / 2, ay + arrow_h_scaled};
            }
            ISWRenderFillPolygon(ctx, pts, 3);
        }

        x += (int)cw;
    }

    ISWRenderEnd(ctx);
}

static Boolean
SetValues(Widget current, Widget request, Widget desired,
          ArgList args, Cardinal *num_args)
{
    ListViewHeaderWidget chw = (ListViewHeaderWidget) current;
    ListViewHeaderWidget dhw = (ListViewHeaderWidget) desired;
    Boolean redraw = FALSE;
    (void)request; (void)args; (void)num_args;

    if (chw->listViewHeader.foreground != dhw->listViewHeader.foreground ||
        chw->core.background_pixel != dhw->core.background_pixel ||
        chw->listViewHeader.font != dhw->listViewHeader.font ||
        chw->listViewHeader.header_height != dhw->listViewHeader.header_height)
        redraw = TRUE;

    return redraw;
}

/* ================================================================
 * Actions
 * ================================================================ */

static void
HeaderPress(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    (void)params; (void)num_params;

    if (iswev->kind != IswButtonDown)
        return;

    Position x = IswEventX(iswev);

    /* Column resize? */
    int resize_col = ResizeHitTest(hw, x);
    if (resize_col >= 0) {
        hw->listViewHeader.col_resize_active = True;
        hw->listViewHeader.col_resize_index = resize_col;
        hw->listViewHeader.col_resize_start_x = x;
        hw->listViewHeader.col_resize_start_w =
            hw->listViewHeader.col_info[resize_col].width;
        UpdateResizeCursor(hw, True);
        return;
    }

    /* Click on a column title (not on separator) — toggle sort */
    int col = ColAtX(hw, x);
    if (col < 0)
        return;

    if (hw->listViewHeader.sort_column == col) {
        if (hw->listViewHeader.sort_direction == IswListViewSortAscending)
            hw->listViewHeader.sort_direction = IswListViewSortDescending;
        else
            hw->listViewHeader.sort_direction = IswListViewSortAscending;
    } else {
        hw->listViewHeader.sort_column = col;
        hw->listViewHeader.sort_direction = IswListViewSortAscending;
    }

    IswListViewReorderCallbackData cb;
    cb.column = col;
    cb.direction = hw->listViewHeader.sort_direction;
    IswCallCallbacks(w, IswNsortCallback, (IswPointer)&cb);
    Redisplay(w, NULL, 0);
}

static void
HeaderDrag(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    (void)params; (void)num_params;

    if (iswev->kind != IswMotion)
        return;
    if (!hw->listViewHeader.col_resize_active)
        return;

    int ci = hw->listViewHeader.col_resize_index;
    if (ci < 0 || ci >= hw->listViewHeader.col_count)
        return;

    int delta = (int)IswEventX(iswev) - (int)hw->listViewHeader.col_resize_start_x;
    int new_w = (int)hw->listViewHeader.col_resize_start_w + delta;
    Dimension min_w = hw->listViewHeader.col_info[ci].min_width;
    if (new_w < (int)min_w)
        new_w = (int)min_w;
    if ((Dimension)new_w == hw->listViewHeader.col_info[ci].width)
        return;

    /* The parent owns the column model — report the new width and let it
       apply the change and repaint rows and header together. */
    IswListViewHeaderResizeCallbackData cb;
    cb.column = ci;
    cb.width = (Dimension)new_w;
    IswCallCallbacks(w, IswNcolumnResizeCallback, (IswPointer)&cb);
}

static void
HeaderRelease(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    (void)iswev; (void)params; (void)num_params;

    if (!hw->listViewHeader.col_resize_active)
        return;

    hw->listViewHeader.col_resize_active = False;
    hw->listViewHeader.col_resize_index = -1;
    UpdateResizeCursor(hw, False);
}

static void
HeaderMotion(Widget w, IswEvent *iswev, String *params, Cardinal *num_params)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    (void)params; (void)num_params;

    if (iswev->kind != IswMotion)
        return;

    /* Skip during active drag — cursor is already set */
    if (hw->listViewHeader.col_resize_active)
        return;

    int col = ResizeHitTest(hw, IswEventX(iswev));
    UpdateResizeCursor(hw, col >= 0);
}

/* ================================================================
 * Parent-side interface (called by ListView.c)
 * ================================================================ */

void
_IswListViewHeaderSetColumns(Widget w, ListViewColumnInfo *cols, int ncols)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    hw->listViewHeader.col_info = cols;
    hw->listViewHeader.col_count = ncols;
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

void
_IswListViewHeaderSetSort(Widget w, int column,
                          IswListViewSortDirection direction)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;
    hw->listViewHeader.sort_column = column;
    hw->listViewHeader.sort_direction = direction;
    if (IswIsRealized(w))
        Redisplay(w, NULL, 0);
}

Dimension
_IswListViewHeaderPreferredHeight(Widget w)
{
    ListViewHeaderWidget hw = (ListViewHeaderWidget) w;

    if (hw->listViewHeader.header_height > 0)
        return hw->listViewHeader.header_height;

    int fh = hw->listViewHeader.font
        ? ISWScaledFontHeight(w, hw->listViewHeader.font)
        : (int)(14);
    return (Dimension)(fh + 2 * (CELL_PAD_Y) + 2);
}
