/*
 * ISWSVG.c - SVG parsing and rasterization utilities
 *
 * Wraps nanosvg for SVG parsing and rasterization to RGBA buffers.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWSVG.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>

#include "nanosvg.h"
#include "nanosvgrast.h"

struct _ISWSVGImage {
    NSVGimage *nsvg;
};

/*
 * Replace all occurrences of "currentColor" with a "#RRGGBB" hex string
 * in a mutable buffer.  The replacement is shorter (7 vs 12 chars) so the
 * buffer shrinks in place — no reallocation needed.
 */
static void
_svg_substitute_current_color(char *buf, const char *hex_color)
{
    const char *needle = "currentColor";
    const size_t needle_len = 12;
    const size_t hex_len = 7;  /* "#RRGGBB" */
    char *p;

    if (!buf || !hex_color)
        return;

    p = buf;
    while ((p = strstr(p, needle)) != NULL) {
        memcpy(p, hex_color, hex_len);
        memmove(p + hex_len, p + needle_len, strlen(p + needle_len) + 1);
        p += hex_len;
    }
}

/*
 * Read a file into a malloc'd NUL-terminated buffer.
 * Returns NULL on failure.
 */
static char *
_svg_read_file(const char *path)
{
    FILE *fp;
    long len;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (len <= 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    if ((long)fread(buf, 1, (size_t)len, fp) != len) {
        fclose(fp);
        free(buf);
        return NULL;
    }

    fclose(fp);
    buf[len] = '\0';
    return buf;
}

/*
 * Try a candidate path — returns True if the file exists and is readable.
 */
static Boolean
_try_path(const char *dir, const char *filename, char *out, unsigned int size)
{
    int n;

    if (dir[0] == '\0')
        return False;

    n = snprintf(out, size, "%s/%s", dir, filename);
    if (n < 0 || (unsigned int)n >= size)
        return False;

    return access(out, R_OK) == 0;
}

Boolean
ISWSVGResolvePath(const char *filename, char *resolved, unsigned int size)
{
    char exe_dir[PATH_MAX];
    const char *env;
    ssize_t len;

    if (!filename || !resolved || size == 0)
        return False;

    /* Absolute path — use directly */
    if (filename[0] == '/') {
        if (access(filename, R_OK) == 0) {
            snprintf(resolved, size, "%s", filename);
            return True;
        }
        return False;
    }

    /* 1. Relative to the executable's directory */
    len = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (len > 0) {
        char *slash;
        exe_dir[len] = '\0';
        slash = strrchr(exe_dir, '/');
        if (slash) {
            *(slash + 1) = '\0';
            if (_try_path(exe_dir, filename, resolved, size))
                return True;
        }
    }

    /* 2. $ISW_DATA_PATH (colon-separated) */
    env = getenv("ISW_DATA_PATH");
    if (env && env[0]) {
        char *pathcopy = strdup(env);
        char *saveptr = NULL;
        char *dir;

        for (dir = strtok_r(pathcopy, ":", &saveptr);
             dir != NULL;
             dir = strtok_r(NULL, ":", &saveptr)) {
            if (_try_path(dir, filename, resolved, size)) {
                free(pathcopy);
                return True;
            }
        }
        free(pathcopy);
    }

    /* 3. $XDG_DATA_HOME/isw/ (defaults to ~/.local/share/isw/) */
    env = getenv("XDG_DATA_HOME");
    if (env && env[0]) {
        char xdg_dir[PATH_MAX];
        snprintf(xdg_dir, sizeof(xdg_dir), "%s/isw", env);
        if (_try_path(xdg_dir, filename, resolved, size))
            return True;
    } else {
        env = getenv("HOME");
        if (env && env[0]) {
            char xdg_dir[PATH_MAX];
            snprintf(xdg_dir, sizeof(xdg_dir), "%s/.local/share/isw", env);
            if (_try_path(xdg_dir, filename, resolved, size))
                return True;
        }
    }

    /* 4. System data directories */
    if (_try_path("/usr/local/share/isw", filename, resolved, size))
        return True;
    if (_try_path("/usr/share/isw", filename, resolved, size))
        return True;

    /* 5. Current working directory */
    if (access(filename, R_OK) == 0) {
        snprintf(resolved, size, "%s", filename);
        return True;
    }

    return False;
}

ISWSVGImage*
ISWSVGLoadFile(const char *filename, const char *units, float dpi,
               const char *current_color)
{
    ISWSVGImage *img;
    NSVGimage *nsvg;
    char resolved[PATH_MAX];
    const char *path;

    if (!filename)
        return NULL;

    /* Resolve through search path */
    if (ISWSVGResolvePath(filename, resolved, sizeof(resolved)))
        path = resolved;
    else
        path = filename;  /* let nsvgParseFromFile report the failure */

    if (current_color) {
        /* Read file manually so we can substitute currentColor */
        char *buf = _svg_read_file(path);
        if (!buf)
            return NULL;
        _svg_substitute_current_color(buf, current_color);
        nsvg = nsvgParse(buf, units ? units : "px", dpi > 0 ? dpi : 96.0f);
        free(buf);
    } else {
        nsvg = nsvgParseFromFile(path, units ? units : "px", dpi > 0 ? dpi : 96.0f);
    }

    if (!nsvg)
        return NULL;

    img = (ISWSVGImage *)calloc(1, sizeof(ISWSVGImage));
    if (!img) {
        nsvgDelete(nsvg);
        return NULL;
    }

    img->nsvg = nsvg;
    return img;
}

ISWSVGImage*
ISWSVGLoadData(const char *data, const char *units, float dpi,
               const char *current_color)
{
    ISWSVGImage *img;
    NSVGimage *nsvg;
    char *copy;

    if (!data)
        return NULL;

    /* nsvgParse modifies the input string, so we must copy it */
    copy = strdup(data);
    if (!copy)
        return NULL;

    if (current_color)
        _svg_substitute_current_color(copy, current_color);

    nsvg = nsvgParse(copy, units ? units : "px", dpi > 0 ? dpi : 96.0f);
    free(copy);

    if (!nsvg)
        return NULL;

    img = (ISWSVGImage *)calloc(1, sizeof(ISWSVGImage));
    if (!img) {
        nsvgDelete(nsvg);
        return NULL;
    }

    img->nsvg = nsvg;
    return img;
}

void
ISWSVGDestroy(ISWSVGImage *image)
{
    if (!image)
        return;

    if (image->nsvg)
        nsvgDelete(image->nsvg);

    free(image);
}

float
ISWSVGGetWidth(ISWSVGImage *image)
{
    if (!image || !image->nsvg)
        return 0.0f;
    return image->nsvg->width;
}

float
ISWSVGGetHeight(ISWSVGImage *image)
{
    if (!image || !image->nsvg)
        return 0.0f;
    return image->nsvg->height;
}

unsigned char*
ISWSVGRasterize(ISWSVGImage *image, unsigned int width, unsigned int height)
{
    NSVGrasterizer *rast;
    unsigned char *buf;
    float svg_w, svg_h, scale_x, scale_y, scale;
    float offset_x, offset_y;

    if (!image || !image->nsvg || width == 0 || height == 0)
        return NULL;

    svg_w = image->nsvg->width;
    svg_h = image->nsvg->height;
    if (svg_w <= 0 || svg_h <= 0)
        return NULL;

    /* Fit within bounds, preserving aspect ratio */
    scale_x = (float)width / svg_w;
    scale_y = (float)height / svg_h;
    scale = (scale_x < scale_y) ? scale_x : scale_y;

    /* Center within the output buffer */
    offset_x = ((float)width - svg_w * scale) * 0.5f;
    offset_y = ((float)height - svg_h * scale) * 0.5f;

    buf = (unsigned char *)calloc(width * height, 4);
    if (!buf)
        return NULL;

    rast = nsvgCreateRasterizer();
    if (!rast) {
        free(buf);
        return NULL;
    }

    nsvgRasterize(rast, image->nsvg, offset_x, offset_y, scale,
                  buf, (int)width, (int)height, (int)(width * 4));

    nsvgDeleteRasterizer(rast);
    return buf;
}

unsigned char*
ISWSVGRasterizeScale(ISWSVGImage *image, float scale,
                     unsigned int *out_w, unsigned int *out_h)
{
    NSVGrasterizer *rast;
    unsigned char *buf;
    unsigned int w, h;

    if (!image || !image->nsvg || scale <= 0)
        return NULL;

    w = (unsigned int)ceilf(image->nsvg->width * scale);
    h = (unsigned int)ceilf(image->nsvg->height * scale);
    if (w == 0 || h == 0)
        return NULL;

    buf = (unsigned char *)calloc(w * h, 4);
    if (!buf)
        return NULL;

    rast = nsvgCreateRasterizer();
    if (!rast) {
        free(buf);
        return NULL;
    }

    nsvgRasterize(rast, image->nsvg, 0, 0, scale,
                  buf, (int)w, (int)h, (int)(w * 4));

    nsvgDeleteRasterizer(rast);

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    return buf;
}
