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
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWSVG.h>
#include <ISW/IconViewP.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define LABEL_MARGIN 2

#define Offset(field) XtOffsetOf(IconViewRec, field)

static XtResource resources[] = {
    {XtNiconLabels, XtCIconLabels, XtRPointer, sizeof(String *),
        Offset(iconView.labels), XtRImmediate, NULL},
    {XtNiconData, XtCIconData, XtRPointer, sizeof(String *),
        Offset(iconView.icon_data), XtRImmediate, NULL},
    {XtNnumIcons, XtCNumIcons, XtRInt, sizeof(int),
        Offset(iconView.nitems), XtRImmediate, (XtPointer) 0},
    {XtNiconSize, XtCIconSize, XtRDimension, sizeof(Dimension),
        Offset(iconView.icon_size), XtRImmediate, (XtPointer) 48},
    {XtNitemSpacing, XtCItemSpacing, XtRDimension, sizeof(Dimension),
        Offset(iconView.item_spacing), XtRImmediate, (XtPointer) 8},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        Offset(iconView.foreground), XtRString, XtDefaultForeground},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
        Offset(iconView.font), XtRString, XtDefaultFont},
#ifdef ISW_INTERNATIONALIZATION
    {XtNfontSet, XtCFontSet, XtRFontSet, sizeof(ISWFontSet *),
        Offset(iconView.fontset), XtRString, XtDefaultFontSet},
#endif
    {XtNselectCallback, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(iconView.select_callback), XtRCallback, NULL},
    {XtNmultiSelect, XtCMultiSelect, XtRBoolean, sizeof(Boolean),
        Offset(iconView.multi_select), XtRImmediate, (XtPointer) False},
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
static void SelectItem(Widget, XEvent *, String *, Cardinal *);

static char defaultTranslations[] =
    "<Btn1Down>: SelectItem()";

static XtActionsRec actions[] = {
    {"SelectItem", SelectItem},
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
    for (int i = 0; i < iw->iconView.nitems; i++) {
        if (iw->iconView.cache[i].raster)
            free(iw->iconView.cache[i].raster);
        if (iw->iconView.cache[i].svg_image)
            ISWSVGDestroy(iw->iconView.cache[i].svg_image);
    }
    free(iw->iconView.cache);
    iw->iconView.cache = NULL;
}

static void
AllocCache(IconViewWidget iw)
{
    FreeCache(iw);
    if (iw->iconView.sel_flags) {
        free(iw->iconView.sel_flags);
        iw->iconView.sel_flags = NULL;
    }
    if (iw->iconView.nitems <= 0)
        return;
    iw->iconView.cache = calloc((size_t)iw->iconView.nitems,
                                 sizeof(IconViewItemCache));
    iw->iconView.sel_flags = calloc((size_t)iw->iconView.nitems,
                                     sizeof(Boolean));
}

static void
ComputeLayout(IconViewWidget iw)
{
    Dimension icon_sz = ISWScaleDim((Widget)iw, iw->iconView.icon_size);
    Dimension spacing = ISWScaleDim((Widget)iw, iw->iconView.item_spacing);
    Dimension label_h = iw->iconView.font
        ? (Dimension)ISWScaledFontHeight((Widget)iw, iw->iconView.font)
        : ISWScaleDim((Widget)iw, 14);
    Dimension margin = ISWScaleDim((Widget)iw, LABEL_MARGIN);

    iw->iconView.cell_w = icon_sz + spacing;
    iw->iconView.cell_h = icon_sz + label_h + margin + spacing;

    if (iw->core.width > 0 && iw->iconView.cell_w > 0)
        iw->iconView.ncols = (int)iw->core.width / (int)iw->iconView.cell_w;
    else
        iw->iconView.ncols = 1;
    if (iw->iconView.ncols < 1)
        iw->iconView.ncols = 1;

    iw->iconView.nrows = (iw->iconView.nitems + iw->iconView.ncols - 1)
                          / iw->iconView.ncols;

    /* Set preferred height to fit all rows (Viewport uses this) */
    Dimension pref_h = (Dimension)(iw->iconView.nrows * (int)iw->iconView.cell_h);
    if (pref_h < 1) pref_h = 1;

    if (pref_h != iw->core.height) {
        Dimension actual_w, actual_h;
        XtMakeResizeRequest((Widget)iw, iw->core.width, pref_h,
                            &actual_w, &actual_h);
    }
}

static unsigned char *
GetItemRaster(IconViewWidget iw, int index)
{
    if (!iw->iconView.cache || index < 0 || index >= iw->iconView.nitems)
        return NULL;

    IconViewItemCache *ic = &iw->iconView.cache[index];
    Dimension icon_sz = ISWScaleDim((Widget)iw, iw->iconView.icon_size);

    /* Already rasterized at correct size? */
    if (ic->raster && ic->raster_w == icon_sz && ic->raster_h == icon_sz)
        return ic->raster;

    /* Parse SVG if needed */
    if (!ic->svg_image && iw->iconView.icon_data &&
        iw->iconView.icon_data[index]) {
        float dpi = 96.0f * ISWScaleFactor((Widget)iw);
        ic->svg_image = ISWSVGLoadData(iw->iconView.icon_data[index],
                                        "px", dpi, NULL);
    }

    if (!ic->svg_image)
        return NULL;

    /* Rasterize */
    if (ic->raster) {
        free(ic->raster);
        ic->raster = NULL;
    }
    ic->raster = ISWSVGRasterize(ic->svg_image, icon_sz, icon_sz);
    ic->raster_w = icon_sz;
    ic->raster_h = icon_sz;

    return ic->raster;
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    IconViewWidget iw = (IconViewWidget) new;
    (void)request; (void)args; (void)num_args;

    iw->iconView.sel_flags = NULL;
    iw->iconView.anchor = -1;
    iw->iconView.render_ctx = NULL;
    iw->iconView.cache = NULL;
    iw->iconView.ncols = 1;
    iw->iconView.nrows = 0;

    iw->iconView.icon_size = ISWScaleDim(new, iw->iconView.icon_size);
    iw->iconView.item_spacing = ISWScaleDim(new, iw->iconView.item_spacing);

    AllocCache(iw);

    if (iw->core.width == 0)
        iw->core.width = ISWScaleDim(new, 300);
    if (iw->core.height == 0)
        iw->core.height = ISWScaleDim(new, 200);

    ComputeLayout(iw);
}

static void
Realize(xcb_connection_t *dpy, Widget w, XtValueMask *valueMask, uint32_t *attributes)
{
    IconViewWidget iw = (IconViewWidget) w;

    (*iconViewWidgetClass->core_class.superclass->core_class.realize)
        (dpy, w, valueMask, attributes);

    iw->iconView.render_ctx = ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
}

static void
Destroy(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    FreeCache(iw);
    if (iw->iconView.sel_flags)
        free(iw->iconView.sel_flags);
    if (iw->iconView.render_ctx)
        ISWRenderDestroy(iw->iconView.render_ctx);
}

static void
Resize(Widget w)
{
    IconViewWidget iw = (IconViewWidget) w;
    ComputeLayout(iw);
    if (XtIsRealized(w))
        Redisplay(w, NULL, 0);
}

static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    IconViewWidget iw = (IconViewWidget) w;
    ISWRenderContext *ctx = iw->iconView.render_ctx;
    (void)event; (void)region;

    if (!ctx || !XtIsRealized(w))
        return;

    Dimension icon_sz = iw->iconView.icon_size;
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
        int cx = col * (int)iw->iconView.cell_w + (int)half_sp;
        int cy = row * (int)iw->iconView.cell_h + (int)half_sp;

        /* Selection highlight */
        if (iw->iconView.sel_flags && iw->iconView.sel_flags[i]) {
            ISWRenderSetColor(ctx, iw->iconView.foreground);
            ISWRenderFillRectangle(ctx, cx, cy,
                                   iw->iconView.cell_w - spacing,
                                   iw->iconView.cell_h - spacing);
        }

        /* Icon */
        unsigned char *raster = GetItemRaster(iw, i);
        if (raster) {
            int ix = cx + ((int)(iw->iconView.cell_w - spacing) - (int)icon_sz) / 2;
            ISWRenderDrawImageRGBA(ctx, raster, icon_sz, icon_sz,
                                   ix, cy, icon_sz, icon_sz);
        }

        /* Label */
        if (iw->iconView.labels && iw->iconView.labels[i]) {
            const char *label = iw->iconView.labels[i];
            int text_w = ISWRenderTextWidth(ctx, label, (int)strlen(label));
            int lx = cx + ((int)(iw->iconView.cell_w - spacing) - text_w) / 2;
            int ly = cy + (int)icon_sz + ISWScaledFontAscent(w, iw->iconView.font)
                   + (int)ISWScaleDim(w, LABEL_MARGIN);

            if (lx < cx) lx = cx;

            if (iw->iconView.sel_flags && iw->iconView.sel_flags[i]) {
                ISWRenderSetColor(ctx, w->core.background_pixel);
            } else {
                ISWRenderSetColor(ctx, iw->iconView.foreground);
            }
            ISWRenderDrawString(ctx, label, (int)strlen(label), lx, ly);
        }
    }

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
        AllocCache(diw);
        ComputeLayout(diw);
        redraw = TRUE;
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
        indices = (int *)XtMalloc((Cardinal)iw->iconView.nitems * sizeof(int));
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
    XtCallCallbacks(w, XtNselectCallback, (XtPointer)&cb);

    if (indices)
        XtFree((char *)indices);
}

static int
HitTest(IconViewWidget iw, Position x, Position y)
{
    int col = (int)x / (int)iw->iconView.cell_w;
    int row = (int)y / (int)iw->iconView.cell_h;

    if (col >= iw->iconView.ncols) col = iw->iconView.ncols - 1;
    if (col < 0) col = 0;

    int index = row * iw->iconView.ncols + col;
    if (index < 0 || index >= iw->iconView.nitems)
        return -1;
    return index;
}

static void
SelectItem(Widget w, XEvent *event, String *params, Cardinal *num_params)
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

    if (index < 0) {
        /* Click on empty space — clear selection */
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
    } else {
        /* Plain click: select only this item */
        ClearSelection(iw);
        iw->iconView.sel_flags[index] = True;
        iw->iconView.anchor = index;
    }

    Redisplay(w, NULL, 0);
    FireCallback(iw, index);
}

/* --- Public API --- */

void
IswIconViewSetItems(Widget w, String *labels, String *icon_data, int nitems)
{
    Arg args[3];
    XtSetArg(args[0], XtNiconLabels, labels);
    XtSetArg(args[1], XtNiconData, icon_data);
    XtSetArg(args[2], XtNnumIcons, nitems);
    XtSetValues(w, args, 3);
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

    int *buf = (int *)XtMalloc((Cardinal)iw->iconView.nitems * sizeof(int));
    for (int i = 0; i < iw->iconView.nitems; i++) {
        if (iw->iconView.sel_flags[i])
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
