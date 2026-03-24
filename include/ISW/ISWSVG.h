/*
 * ISWSVG.h - SVG parsing and rasterization utilities for ISW
 *
 * Provides SVG loading (from file or inline data) and rasterization
 * to RGBA pixel buffers using nanosvg. Used by the Label widget for
 * SVG display (and thus by Command for SVG button icons).
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_ISWSVG_h
#define _ISW_ISWSVG_h

#include <X11/Intrinsic.h>

/*
 * Opaque handle to a parsed SVG image
 */
typedef struct _ISWSVGImage ISWSVGImage;

/*
 * ISWSVGLoadFile - Parse an SVG file with automatic path resolution
 *
 * If filename is an absolute path, it is used directly. If relative,
 * the following locations are searched in order:
 *   1. Relative to the executable's directory
 *   2. $ISW_DATA_PATH directories (colon-separated)
 *   3. $XDG_DATA_HOME/isw/ (defaults to ~/.local/share/isw/)
 *   4. System data directories: /usr/local/share/isw/, /usr/share/isw/
 *   5. Current working directory
 *
 * Parameters:
 *   filename - Path or basename of SVG file
 *   units    - Units for dimensions ("px", "pt", "mm", "in", etc.)
 *   dpi      - DPI for unit conversion (96.0 is typical)
 *
 * Returns: Parsed SVG image, or NULL on failure
 *          Must be freed with ISWSVGDestroy()
 */
ISWSVGImage* ISWSVGLoadFile(const char *filename, const char *units, float dpi);

/*
 * ISWSVGResolvePath - Resolve a filename using the SVG search path
 *
 * Searches the same locations as ISWSVGLoadFile. Useful when you need
 * the resolved path for other purposes.
 *
 * Parameters:
 *   filename - Path or basename to resolve
 *   resolved - Output buffer for the resolved path
 *   size     - Size of the output buffer
 *
 * Returns: True if the file was found, False otherwise
 *          On success, resolved contains the full path.
 */
Boolean ISWSVGResolvePath(const char *filename, char *resolved, unsigned int size);

/*
 * ISWSVGLoadData - Parse SVG from an in-memory string
 *
 * Parameters:
 *   data  - SVG XML string (will be copied internally)
 *   units - Units for dimensions ("px", "pt", "mm", "in", etc.)
 *   dpi   - DPI for unit conversion
 *
 * Returns: Parsed SVG image, or NULL on failure
 *          Must be freed with ISWSVGDestroy()
 */
ISWSVGImage* ISWSVGLoadData(const char *data, const char *units, float dpi);

/*
 * ISWSVGDestroy - Free a parsed SVG image and any cached rasterization
 *
 * Parameters:
 *   image - SVG image to free (safe to call with NULL)
 */
void ISWSVGDestroy(ISWSVGImage *image);

/*
 * ISWSVGGetWidth - Get native width of the SVG
 */
float ISWSVGGetWidth(ISWSVGImage *image);

/*
 * ISWSVGGetHeight - Get native height of the SVG
 */
float ISWSVGGetHeight(ISWSVGImage *image);

/*
 * ISWSVGRasterize - Rasterize SVG to fit within width x height
 *
 * Scales the SVG to fit within the given dimensions, preserving
 * aspect ratio. Transparent pixels fill any letterbox area.
 *
 * Parameters:
 *   image  - Parsed SVG image
 *   width  - Desired output width in pixels
 *   height - Desired output height in pixels
 *
 * Returns: RGBA pixel buffer (4 bytes per pixel, row-major),
 *          or NULL on failure. Caller must free() the buffer.
 */
unsigned char* ISWSVGRasterize(ISWSVGImage *image,
                               unsigned int width, unsigned int height);

/*
 * ISWSVGRasterizeScale - Rasterize SVG at a specific scale factor
 *
 * Parameters:
 *   image   - Parsed SVG image
 *   scale   - Scale factor (1.0 = native size)
 *   out_w   - Output: actual rasterized width
 *   out_h   - Output: actual rasterized height
 *
 * Returns: RGBA pixel buffer, or NULL on failure.
 *          Caller must free() the buffer.
 */
unsigned char* ISWSVGRasterizeScale(ISWSVGImage *image, float scale,
                                    unsigned int *out_w, unsigned int *out_h);

#endif /* _ISW_ISWSVG_h */
