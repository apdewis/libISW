/*
 * ISWPNG.c - PNG loading utilities
 *
 * Wraps lodepng for PNG decoding to RGBA buffers.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWPNG.h>
#include <ISW/ISWSVG.h>  /* ISWSVGResolvePath for shared path resolution */
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lodepng.h"

struct _ISWPNGImage {
    unsigned char *rgba;
    unsigned int width;
    unsigned int height;
};

ISWPNGImage*
ISWPNGLoadFile(const char *filename)
{
    ISWPNGImage *img;
    unsigned char *rgba;
    unsigned int w, h;
    unsigned int err;
    char resolved[PATH_MAX];
    const char *path;

    if (!filename)
	return NULL;

    /* Use the shared SVG path resolution (exe-relative, ISW_DATA_PATH, etc.) */
    if (ISWSVGResolvePath(filename, resolved, sizeof(resolved)))
	path = resolved;
    else
	path = filename;  /* let lodepng report the failure */

    err = lodepng_decode32_file(&rgba, &w, &h, path);
    if (err != 0)
	return NULL;

    img = (ISWPNGImage *)calloc(1, sizeof(ISWPNGImage));
    if (!img) {
	free(rgba);
	return NULL;
    }

    img->rgba = rgba;
    img->width = w;
    img->height = h;
    return img;
}

ISWPNGImage*
ISWPNGLoadData(const unsigned char *data, unsigned int len)
{
    ISWPNGImage *img;
    unsigned char *rgba;
    unsigned int w, h;
    unsigned int err;

    if (!data || len == 0)
	return NULL;

    err = lodepng_decode32(&rgba, &w, &h, data, (size_t)len);
    if (err != 0)
	return NULL;

    img = (ISWPNGImage *)calloc(1, sizeof(ISWPNGImage));
    if (!img) {
	free(rgba);
	return NULL;
    }

    img->rgba = rgba;
    img->width = w;
    img->height = h;
    return img;
}

void
ISWPNGDestroy(ISWPNGImage *image)
{
    if (!image)
	return;

    free(image->rgba);
    free(image);
}

unsigned int
ISWPNGGetWidth(ISWPNGImage *image)
{
    if (!image)
	return 0;
    return image->width;
}

unsigned int
ISWPNGGetHeight(ISWPNGImage *image)
{
    if (!image)
	return 0;
    return image->height;
}

const unsigned char*
ISWPNGGetRGBA(ISWPNGImage *image)
{
    if (!image)
	return NULL;
    return image->rgba;
}
