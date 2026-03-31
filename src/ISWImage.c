/*
 * ISWImage.c - Unified image loading for ISW
 *
 * Dispatches to ISWSVG or ISWPNG based on format detection,
 * and holds the raster cache internally.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWImage.h>
#include <ISW/ISWSVG.h>
#include <ISW/ISWPNG.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum {
    ISW_IMAGE_SVG,
    ISW_IMAGE_PNG
} ISWImageType;

struct _ISWImage {
    ISWImageType    type;

    /* SVG: stored source for recoloring */
    char           *svg_source;   /* inline data, or NULL */
    char           *svg_file;     /* file path, or NULL */
    double          dpi;
    char            fg_hex[8];    /* current_color "#RRGGBB\0", or empty */
    ISWSVGImage    *svg;

    /* PNG: native decoded pixels */
    ISWPNGImage    *png;

    /* Raster cache */
    unsigned char  *raster;
    unsigned int    raster_w;
    unsigned int    raster_h;
};

static ISWImageType
_detect_format(const char *source)
{
    size_t len;

    if (source[0] == '<')
        return ISW_IMAGE_SVG;

    len = strlen(source);
    if (len >= 4 && strcasecmp(source + len - 4, ".svg") == 0)
        return ISW_IMAGE_SVG;

    return ISW_IMAGE_PNG;
}

ISWImage*
ISWImageLoad(const char *source, double dpi, const char *current_color)
{
    ISWImage *img;

    if (!source || !source[0])
        return NULL;

    img = (ISWImage *)calloc(1, sizeof(ISWImage));
    if (!img)
        return NULL;

    img->dpi = dpi > 0 ? dpi : 96.0;
    if (current_color)
        snprintf(img->fg_hex, sizeof(img->fg_hex), "%s", current_color);

    img->type = _detect_format(source);

    if (img->type == ISW_IMAGE_SVG) {
        const char *color = img->fg_hex[0] ? img->fg_hex : NULL;
        if (source[0] == '<') {
            img->svg_source = strdup(source);
            img->svg = ISWSVGLoadData(source, "px", (float)img->dpi, color);
        } else {
            img->svg_file = strdup(source);
            img->svg = ISWSVGLoadFile(source, "px", (float)img->dpi, color);
        }
        if (!img->svg) {
            free(img->svg_source);
            free(img->svg_file);
            free(img);
            return NULL;
        }
    } else {
        img->png = ISWPNGLoadFile(source);
        if (!img->png) {
            free(img);
            return NULL;
        }
    }

    return img;
}

void
ISWImageDestroy(ISWImage *image)
{
    if (!image)
        return;
    if (image->svg)
        ISWSVGDestroy(image->svg);
    if (image->png)
        ISWPNGDestroy(image->png);
    free(image->svg_source);
    free(image->svg_file);
    free(image->raster);
    free(image);
}

float
ISWImageGetWidth(ISWImage *image)
{
    if (!image)
        return 0.0f;
    if (image->type == ISW_IMAGE_SVG)
        return ISWSVGGetWidth(image->svg);
    return (float)ISWPNGGetWidth(image->png);
}

float
ISWImageGetHeight(ISWImage *image)
{
    if (!image)
        return 0.0f;
    if (image->type == ISW_IMAGE_SVG)
        return ISWSVGGetHeight(image->svg);
    return (float)ISWPNGGetHeight(image->png);
}

Boolean
ISWImageIsVector(ISWImage *image)
{
    return image && image->type == ISW_IMAGE_SVG;
}

const unsigned char*
ISWImageRasterize(ISWImage *image,
                  unsigned int hint_w, unsigned int hint_h,
                  unsigned int *out_w, unsigned int *out_h)
{
    if (!image)
        return NULL;

    if (image->type == ISW_IMAGE_SVG) {
        if (hint_w == 0 || hint_h == 0)
            return NULL;

        /* Return cached raster if size matches */
        if (image->raster &&
            image->raster_w == hint_w && image->raster_h == hint_h) {
            if (out_w) *out_w = hint_w;
            if (out_h) *out_h = hint_h;
            return image->raster;
        }

        free(image->raster);
        image->raster = ISWSVGRasterize(image->svg, hint_w, hint_h);
        if (!image->raster) {
            image->raster_w = 0;
            image->raster_h = 0;
            return NULL;
        }
        image->raster_w = hint_w;
        image->raster_h = hint_h;
        if (out_w) *out_w = hint_w;
        if (out_h) *out_h = hint_h;
        return image->raster;
    } else {
        /* PNG: native pixels, render layer handles scaling */
        const unsigned char *rgba = ISWPNGGetRGBA(image->png);
        if (out_w) *out_w = ISWPNGGetWidth(image->png);
        if (out_h) *out_h = ISWPNGGetHeight(image->png);
        return rgba;
    }
}

void
ISWImageRecolor(ISWImage *image, const char *hex_color)
{
    ISWSVGImage *new_svg;

    if (!image || image->type != ISW_IMAGE_SVG)
        return;

    /* No-op if color hasn't changed */
    if (hex_color && strncmp(image->fg_hex, hex_color, 7) == 0)
        return;
    if (!hex_color && !image->fg_hex[0])
        return;

    if (hex_color)
        snprintf(image->fg_hex, sizeof(image->fg_hex), "%s", hex_color);
    else
        image->fg_hex[0] = '\0';

    /* Re-parse SVG with new color */
    if (image->svg_source)
        new_svg = ISWSVGLoadData(image->svg_source, "px", (float)image->dpi,
                                 image->fg_hex[0] ? image->fg_hex : NULL);
    else if (image->svg_file)
        new_svg = ISWSVGLoadFile(image->svg_file, "px", (float)image->dpi,
                                 image->fg_hex[0] ? image->fg_hex : NULL);
    else
        return;

    if (!new_svg)
        return;

    ISWSVGDestroy(image->svg);
    image->svg = new_svg;

    /* Invalidate cache */
    free(image->raster);
    image->raster = NULL;
    image->raster_w = 0;
    image->raster_h = 0;
}
