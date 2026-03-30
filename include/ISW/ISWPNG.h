/*
 * ISWPNG.h - PNG loading utilities for ISW
 *
 * Provides PNG loading (from file or in-memory buffer) and decoding
 * to RGBA pixel buffers using lodepng. Used by the Label widget for
 * PNG icon display via the bitmap resource, and by the string-to-pixmap
 * resource converter.
 *
 * Copyright (c) 2026 ISW Project
 */

#ifndef _ISW_ISWPNG_h
#define _ISW_ISWPNG_h

#include <X11/Intrinsic.h>

/*
 * Opaque handle to a decoded PNG image
 */
typedef struct _ISWPNGImage ISWPNGImage;

/*
 * ISWPNGLoadFile - Decode a PNG file with automatic path resolution
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
 *   filename - Path or basename of PNG file
 *
 * Returns: Decoded PNG image, or NULL on failure
 *          Must be freed with ISWPNGDestroy()
 */
ISWPNGImage* ISWPNGLoadFile(const char *filename);

/*
 * ISWPNGLoadData - Decode PNG from an in-memory buffer
 *
 * Parameters:
 *   data - Raw PNG file data
 *   len  - Length of data in bytes
 *
 * Returns: Decoded PNG image, or NULL on failure
 *          Must be freed with ISWPNGDestroy()
 */
ISWPNGImage* ISWPNGLoadData(const unsigned char *data, unsigned int len);

/*
 * ISWPNGDestroy - Free a decoded PNG image
 *
 * Parameters:
 *   image - PNG image to free (safe to call with NULL)
 */
void ISWPNGDestroy(ISWPNGImage *image);

/*
 * ISWPNGGetWidth - Get width of the decoded image
 */
unsigned int ISWPNGGetWidth(ISWPNGImage *image);

/*
 * ISWPNGGetHeight - Get height of the decoded image
 */
unsigned int ISWPNGGetHeight(ISWPNGImage *image);

/*
 * ISWPNGGetRGBA - Get pointer to decoded RGBA pixel data
 *
 * Returns: Pointer to RGBA pixel buffer (4 bytes per pixel, row-major).
 *          The buffer is owned by the ISWPNGImage and freed on destroy.
 *          Returns NULL if image is NULL.
 */
const unsigned char* ISWPNGGetRGBA(ISWPNGImage *image);

#endif /* _ISW_ISWPNG_h */
