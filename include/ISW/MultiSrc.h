/*
 * Deprecated compatibility shim.
 *
 * The MultiSrc class was never functionally distinct from AsciiSrc in the
 * XCB port. The symbol `multiSrcObjectClass` is kept as an alias pointing
 * at `asciiSrcObjectClass`, defined in AsciiSrc.c, so legacy applications
 * keep linking. Prefer <ISW/AsciiSrc.h> in new code.
 */
#ifndef _IswMultiSrc_h
#define _IswMultiSrc_h

#include <ISW/AsciiSrc.h>

extern WidgetClass multiSrcObjectClass;

#endif /* _IswMultiSrc_h */
