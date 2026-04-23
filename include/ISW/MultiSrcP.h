/*
 * Deprecated compatibility shim.
 *
 * MultiSrc was folded into AsciiSrc. Private struct layout is no longer
 * part of the supported surface — include <ISW/AsciiSrcP.h> if you need
 * it. This header exists only so existing `#include <ISW/MultiSrcP.h>`
 * statements in legacy code keep compiling.
 */
#ifndef _ISWMultiSrcP_h
#define _ISWMultiSrcP_h

#include <ISW/AsciiSrcP.h>
#include <ISW/MultiSrc.h>

#endif /* _ISWMultiSrcP_h */
