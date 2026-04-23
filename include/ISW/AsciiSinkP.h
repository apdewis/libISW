/*
 * Deprecated compatibility shim.
 *
 * AsciiSink's private types were absorbed into TextSinkP.h. Include that
 * instead.
 */

#ifndef _ISWAsciiSinkP_h
#define _ISWAsciiSinkP_h

#include <ISW/TextSinkP.h>
#include <ISW/AsciiSink.h>

typedef TextSinkClassRec AsciiSinkClassRec;
typedef TextSinkPart     AsciiSinkPart;
typedef TextSinkRec      AsciiSinkRec;

extern TextSinkClassRec asciiSinkClassRec;

#endif /* _ISWAsciiSinkP_h */
