/*
 * Deprecated compatibility shim.
 *
 * The MultiSink class was never functionally distinct from AsciiSink in the
 * XCB port. The symbol `multiSinkObjectClass` is kept as an alias pointing
 * at `asciiSinkObjectClass`, defined in AsciiSink.c, so legacy applications
 * keep linking. Prefer <ISW/AsciiSink.h> in new code.
 */
#ifndef _IswMultiSink_h
#define _IswMultiSink_h

#include <ISW/AsciiSink.h>

#ifndef IswNfontSet
#define IswNfontSet		"fontSet"
#endif
#ifndef IswCFontSet
#define IswCFontSet		"FontSet"
#endif

extern WidgetClass multiSinkObjectClass;

#endif /* _IswMultiSink_h */
