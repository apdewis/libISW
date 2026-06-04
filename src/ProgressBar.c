/*
 * ProgressBar.c - ProgressBar widget implementation
 *
 * A display-only widget showing a filled bar proportional to a value (0-100).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/ProgressBarP.h>
#include <xcb/xcb.h>
#include <cairo/cairo.h>

#define offset(field) IswOffsetOf(ProgressBarRec, field)

static IswResource resources[] = {
    {IswNvalue, IswCValue, IswRInt, sizeof(int),
	offset(progress_bar.value), IswRImmediate, (IswPointer) 0},
    {IswNforeground, IswCForeground, IswRPixel, sizeof(Pixel),
	offset(progress_bar.foreground), IswRString, (IswPointer) IswDefaultForeground},
    {IswNorientation, IswCOrientation, IswROrientation, sizeof(IswOrientation),
	offset(progress_bar.orientation), IswRImmediate, (IswPointer) IswOrientHorizontal},
    {IswNshowValue, IswCShowValue, IswRBoolean, sizeof(Boolean),
	offset(progress_bar.show_value), IswRImmediate, (IswPointer) True},
    {IswNfont, IswCFont, IswRFontStruct, sizeof(IswFontStruct *),
	offset(progress_bar.font), IswRString, IswDefaultFont},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
	IswOffsetOf(RectObjRec, rectangle.border_width), IswRImmediate, (IswPointer) 1},
};
#undef offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void Resize(Widget);
static void Redisplay(Widget, xcb_generic_event_t *, xcb_xfixes_region_t);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

#define SuperClass ((SimpleWidgetClass)&simpleClassRec)

ProgressBarClassRec progressBarClassRec = {
  {
    (WidgetClass) SuperClass,		/* superclass		  */
    "ProgressBar",			/* class_name		  */
    sizeof(ProgressBarRec),		/* size			  */
    NULL,				/* class_initialize	  */
    NULL,				/* class_part_initialize  */
    FALSE,				/* class_inited		  */
    Initialize,				/* initialize		  */
    NULL,				/* initialize_hook	  */
    IswInheritRealize,			/* realize		  */
    NULL,				/* actions		  */
    0,					/* num_actions		  */
    resources,				/* resources		  */
    IswNumber(resources),		/* resource_count	  */
    NULLQUARK,				/* xrm_class		  */
    TRUE,				/* compress_motion	  */
    TRUE,				/* compress_exposure	  */
    TRUE,				/* compress_enterleave    */
    FALSE,				/* visible_interest	  */
    Destroy,				/* destroy		  */
    Resize,				/* resize		  */
    Redisplay,				/* expose		  */
    SetValues,				/* set_values		  */
    NULL,				/* set_values_hook	  */
    IswInheritSetValuesAlmost,		/* set_values_almost	  */
    NULL,				/* get_values_hook	  */
    NULL,				/* accept_focus		  */
    IswVersion,				/* version		  */
    NULL,				/* callback_private	  */
    NULL,				/* tm_table (no translations) */
    IswInheritQueryGeometry,		/* query_geometry	  */
    IswInheritDisplayAccelerator,	/* display_accelerator	  */
    NULL				/* extension		  */
  },
  {
    IswInheritChangeSensitive		/* change_sensitive	  */
  },
  {
    0					/* makes_compiler_happy   */
  }
};

WidgetClass progressBarWidgetClass = (WidgetClass) &progressBarClassRec;

static void
ClampValue(ProgressBarWidget pbw)
{
    if (pbw->progress_bar.value < 0) pbw->progress_bar.value = 0;
    if (pbw->progress_bar.value > 100) pbw->progress_bar.value = 100;
}

/* ARGSUSED */
static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ProgressBarWidget pbw = (ProgressBarWidget) new;

    new->core.windowless = True;

    /* ProgressBar strokes its own trough/border; suppress the windowless
       backend's generic border ring to avoid a redundant border. */
    ((SimpleWidget) new)->simple.self_border = True;

    ClampValue(pbw);

    /* Set default size if not specified (logical pixels) */
    if (pbw->core.width == 0) {
	if (pbw->progress_bar.orientation == IswOrientHorizontal)
	    pbw->core.width = 200;
	else
	    pbw->core.width = 24;
    }
    if (pbw->core.height == 0) {
	if (pbw->progress_bar.orientation == IswOrientHorizontal)
	    pbw->core.height = 24;
	else
	    pbw->core.height = 200;
    }

    pbw->progress_bar.render_ctx = NULL;
}

static void
Destroy(Widget w)
{
    ProgressBarWidget pbw = (ProgressBarWidget) w;

    if (pbw->progress_bar.render_ctx) {
	ISWRenderDestroy(pbw->progress_bar.render_ctx);
	pbw->progress_bar.render_ctx = NULL;
    }
}

static void
Resize(Widget w)
{
    (void)w;
}

/*
 * Draw a rounded rectangle path on the given Cairo context.
 */
static void
RoundedRectPath(cairo_t *cr, double x, double y, double w, double h, double r)
{
    if (r > w / 2.0) r = w / 2.0;
    if (r > h / 2.0) r = h / 2.0;

    cairo_new_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI/2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI/2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI/2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
}

/* ARGSUSED */
static void
Redisplay(Widget w, xcb_generic_event_t *event, xcb_xfixes_region_t region)
{
    ProgressBarWidget pbw = (ProgressBarWidget) w;
    ISWRenderContext *ctx = pbw->progress_bar.render_ctx;
    /* Drawing in logical pixels; Cairo scale transform handles physical */
    double r = 2.0;
    double pad = 2.0;

    /* Lazy-create render context */
    if (!ctx && w->core.width > 0 && w->core.height > 0 && IswIsRealized(w)) {
	ctx = pbw->progress_bar.render_ctx =
	    ISWRenderCreate(w, ISW_RENDER_BACKEND_AUTO);
    }
    if (!ctx) return;

    ISWRenderBegin(ctx);

    /* Clear background */
    ISWRenderSetColor(ctx, w->core.background_pixel);
    ISWRenderFillRectangle(ctx, 0, 0, w->core.width, w->core.height);

    cairo_t *cr = (cairo_t *)ISWRenderGetCairoContext(ctx);
    if (!cr) {
	ISWRenderEnd(ctx);
	return;
    }

    cairo_save(cr);

    /* Draw trough (border) */
    double bx = pad, by = pad;
    double bw = w->core.width - 2 * pad;
    double bh = w->core.height - 2 * pad;

    if (bw <= 0 || bh <= 0) {
	cairo_restore(cr);
	ISWRenderEnd(ctx);
	return;
    }

    ISWRenderSetColor(ctx, pbw->progress_bar.foreground);
    cairo_set_line_width(cr, 1.0);
    RoundedRectPath(cr, bx, by, bw, bh, r);
    cairo_stroke(cr);

    /* Compute fill bar dimensions (needed for both drawing and text clipping) */
    double inner_pad = 2.0;
    double ix = bx + inner_pad;
    double iy = by + inner_pad;
    double iw = bw - 2 * inner_pad;
    double ih = bh - 2 * inner_pad;
    double fill_w = 0, fill_h = 0, fill_x = ix, fill_y = iy;

    if (iw > 0 && ih > 0 && pbw->progress_bar.value > 0) {
	if (pbw->progress_bar.orientation == IswOrientHorizontal) {
	    fill_w = (iw * pbw->progress_bar.value) / 100.0;
	    fill_h = ih;
	    fill_x = ix;
	    fill_y = iy;
	} else {
	    fill_w = iw;
	    fill_h = (ih * pbw->progress_bar.value) / 100.0;
	    fill_x = ix;
	    fill_y = iy + ih - fill_h;
	}

	double fill_r = r * 0.5;
	ISWRenderSetColor(ctx, pbw->progress_bar.foreground);
	RoundedRectPath(cr, fill_x, fill_y, fill_w, fill_h, fill_r);
	cairo_fill(cr);
    }

    /* Draw percentage text with split-color clipping */
    if (pbw->progress_bar.show_value) {
	char buf[8];
	int len = snprintf(buf, sizeof(buf), "%d%%", pbw->progress_bar.value);

	ISWRenderSetFont(ctx, pbw->progress_bar.font);

	/* Measure at the current font size, then scale down to fit */
	int text_w = ISWScaledTextWidth(w, pbw->progress_bar.font, buf, len);
	int text_h = ISWScaledFontHeight(w, pbw->progress_bar.font);
	int text_ascent = ISWScaledFontAscent(w, pbw->progress_bar.font);

	int avail_w, avail_h;
	if (pbw->progress_bar.orientation == IswOrientHorizontal) {
	    avail_w = (int)w->core.width - 4;
	    avail_h = (int)w->core.height - 4;
	} else {
	    avail_w = (int)w->core.height - 4;
	    avail_h = (int)w->core.width - 4;
	}

	float text_scale = 1.0f;
	if (text_w > 0 && text_h > 0 && avail_w > 0 && avail_h > 0) {
	    if (text_w > avail_w)
		text_scale = (float)avail_w / text_w;
	    if (text_h > avail_h) {
		float sy = (float)avail_h / text_h;
		if (sy < text_scale)
		    text_scale = sy;
	    }
	}

	/* Recompute metrics at the effective scale */
	float eff_text_w = text_w * text_scale;
	float eff_text_h = text_h * text_scale;
	float eff_ascent = text_ascent * text_scale;

	if (pbw->progress_bar.orientation == IswOrientHorizontal) {
	    double tx = (w->core.width - eff_text_w) / 2.0;
	    double ty = (w->core.height - eff_text_h) / 2.0 + eff_ascent;

	    /* Pass 1: foreground text clipped to unfilled region */
	    cairo_save(cr);
	    cairo_rectangle(cr, 0, 0, fill_x + fill_w, w->core.height);
	    cairo_rectangle(cr, 0, 0, w->core.width, w->core.height);
	    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	    cairo_clip(cr);
	    if (text_scale < 1.0f) {
		cairo_translate(cr, tx, ty);
		cairo_scale(cr, text_scale, text_scale);
		tx = 0;
		ty = 0;
	    }
	    ISWRenderSetColor(ctx, pbw->progress_bar.foreground);
	    cairo_move_to(cr, tx, ty);
	    cairo_show_text(cr, buf);
	    cairo_restore(cr);

	    /* Recalculate for pass 2 (restore resets transform) */
	    tx = (w->core.width - eff_text_w) / 2.0;
	    ty = (w->core.height - eff_text_h) / 2.0 + eff_ascent;

	    /* Pass 2: background text clipped to filled region */
	    cairo_save(cr);
	    cairo_rectangle(cr, fill_x, fill_y, fill_w, fill_h);
	    cairo_clip(cr);
	    if (text_scale < 1.0f) {
		cairo_translate(cr, tx, ty);
		cairo_scale(cr, text_scale, text_scale);
		tx = 0;
		ty = 0;
	    }
	    ISWRenderSetColor(ctx, w->core.background_pixel);
	    cairo_move_to(cr, tx, ty);
	    cairo_show_text(cr, buf);
	    cairo_restore(cr);
	} else {
	    double tx = -eff_text_w / 2.0;
	    double ty = eff_ascent - eff_text_h / 2.0;

	    /* Pass 1: foreground text clipped to unfilled region */
	    cairo_save(cr);
	    cairo_rectangle(cr, fill_x, fill_y, fill_w, fill_h);
	    cairo_rectangle(cr, 0, 0, w->core.width, w->core.height);
	    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	    cairo_clip(cr);
	    cairo_translate(cr, w->core.width / 2.0, w->core.height / 2.0);
	    cairo_rotate(cr, -M_PI / 2.0);
	    if (text_scale < 1.0f)
		cairo_scale(cr, text_scale, text_scale);
	    ISWRenderSetColor(ctx, pbw->progress_bar.foreground);
	    cairo_move_to(cr, tx, ty);
	    cairo_show_text(cr, buf);
	    cairo_restore(cr);

	    /* Pass 2: background text clipped to filled region */
	    cairo_save(cr);
	    cairo_rectangle(cr, fill_x, fill_y, fill_w, fill_h);
	    cairo_clip(cr);
	    cairo_translate(cr, w->core.width / 2.0, w->core.height / 2.0);
	    cairo_rotate(cr, -M_PI / 2.0);
	    if (text_scale < 1.0f)
		cairo_scale(cr, text_scale, text_scale);
	    ISWRenderSetColor(ctx, w->core.background_pixel);
	    cairo_move_to(cr, tx, ty);
	    cairo_show_text(cr, buf);
	    cairo_restore(cr);
	}
    }

    cairo_new_path(cr);
    cairo_restore(cr);
    ISWRenderEnd(ctx);
}

/* ARGSUSED */
static Boolean
SetValues(Widget current, Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    ProgressBarWidget cur = (ProgressBarWidget) current;
    ProgressBarWidget neww = (ProgressBarWidget) new;
    Boolean redisplay = FALSE;

    ClampValue(neww);

    if (cur->progress_bar.value != neww->progress_bar.value ||
	cur->progress_bar.foreground != neww->progress_bar.foreground ||
	cur->progress_bar.orientation != neww->progress_bar.orientation ||
	cur->progress_bar.show_value != neww->progress_bar.show_value ||
	cur->progress_bar.font != neww->progress_bar.font) {
	redisplay = TRUE;
    }

    if (cur->core.width != neww->core.width ||
	cur->core.height != neww->core.height) {
	if (neww->progress_bar.render_ctx) {
	    ISWRenderDestroy(neww->progress_bar.render_ctx);
	    neww->progress_bar.render_ctx = NULL;
	}
	redisplay = TRUE;
    }

    return redisplay;
}
