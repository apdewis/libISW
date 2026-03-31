/*
 * ISWImage.h - Unified image loading for ISW
 *
 * Provides a single interface for loading images from any supported
 * format (SVG, PNG). All images are decoded to RGBA pixel buffers
 * on demand. The raster cache is owned by the ISWImage.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_ISWImage_h
#define _ISW_ISWImage_h

#include <X11/Intrinsic.h>

typedef struct _ISWImage ISWImage;

/*
 * ISWImageLoad - Load an image from a file path or inline SVG data.
 *
 * Format detection:
 *   - If source starts with '<': treated as inline SVG XML
 *   - If source ends with ".svg" (case-insensitive): loaded as SVG file
 *   - Otherwise: loaded as PNG file
 *
 * Parameters:
 *   source        - File path or inline SVG string
 *   dpi           - DPI for SVG sizing (use 96.0 * scale_factor)
 *   current_color - "#RRGGBB" to substitute SVG currentColor, or NULL
 *
 * Returns: ISWImage handle, or NULL on failure.
 *          Must be freed with ISWImageDestroy().
 */
ISWImage* ISWImageLoad(const char *source, double dpi, const char *current_color);

/*
 * ISWImageDestroy - Free an image and its raster cache.
 *   Safe to call with NULL.
 */
void ISWImageDestroy(ISWImage *image);

/*
 * ISWImageGetWidth / ISWImageGetHeight - Native image dimensions.
 * For SVG: intrinsic dimensions. For PNG: pixel dimensions.
 */
float ISWImageGetWidth(ISWImage *image);
float ISWImageGetHeight(ISWImage *image);

/*
 * ISWImageIsVector - Returns True if the image is vector (SVG), False for raster.
 * Used to decide whether to apply HiDPI scale when computing display dimensions.
 */
Boolean ISWImageIsVector(ISWImage *image);

/*
 * ISWImageRasterize - Get RGBA pixel data for the image.
 *
 * For SVG: rasterizes at hint_w x hint_h (result is cached by size).
 * For PNG: returns native pixels; hint_w/hint_h are ignored.
 *
 * The buffer is cached internally and valid until the next call with
 * different dimensions, ISWImageRecolor(), or ISWImageDestroy().
 *
 * Parameters:
 *   image         - Image to rasterize
 *   hint_w/hint_h - Desired output size (used for SVG; ignored for PNG)
 *   out_w, out_h  - Actual buffer dimensions (may differ from hint for PNG)
 *
 * Returns: RGBA pixel buffer (4 bytes/pixel), or NULL on failure.
 */
const unsigned char* ISWImageRasterize(ISWImage *image,
                                       unsigned int hint_w, unsigned int hint_h,
                                       unsigned int *out_w, unsigned int *out_h);

/*
 * ISWImageRecolor - Re-parse SVG with a new currentColor value.
 *
 * Invalidates the raster cache. No-op for PNG images.
 *
 * Parameters:
 *   image     - Image to recolor
 *   hex_color - New color as "#RRGGBB", or NULL to remove substitution
 */
void ISWImageRecolor(ISWImage *image, const char *hex_color);

#endif /* _ISW_ISWImage_h */
