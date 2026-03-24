/*
 * nanosvg_impl.c - Compilation unit for nanosvg header-only libraries
 *
 * This file defines the implementation macros and includes the nanosvg
 * headers to produce the actual compiled code. The headers themselves
 * (nanosvg.h, nanosvgrast.h) remain unmodified from upstream.
 *
 * https://github.com/memononen/nanosvg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
