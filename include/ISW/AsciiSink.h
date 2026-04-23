/*
 * Deprecated compatibility shim.
 *
 * AsciiSink was merged into the concrete TextSink class. `asciiSinkObjectClass`
 * survives as an alias of `textSinkObjectClass` so legacy applications keep
 * linking. Prefer <ISW/TextSink.h> in new code.
 */

#ifndef _ISWAsciiSink_h
#define _ISWAsciiSink_h

#include <ISW/TextSink.h>

extern WidgetClass asciiSinkObjectClass;

typedef struct _TextSinkClassRec *AsciiSinkObjectClass;
typedef struct _TextSinkRec      *AsciiSinkObject;

#endif /* _ISWAsciiSink_h */
