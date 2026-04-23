/*
 * Deprecated compatibility shim.
 *
 * MultiSink was folded into AsciiSink. Private struct layout is no longer
 * part of the supported surface — include <ISW/AsciiSinkP.h> if you need
 * it. This header exists only so existing `#include <ISW/MultiSinkP.h>`
 * statements in legacy code keep compiling.
 */
#ifndef _ISWMultiSinkP_h
#define _ISWMultiSinkP_h

#include <ISW/AsciiSinkP.h>
#include <ISW/MultiSink.h>

#endif /* _ISWMultiSinkP_h */
