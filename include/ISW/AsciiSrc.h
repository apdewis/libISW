/*
 * Deprecated compatibility shim.
 *
 * AsciiSrc was merged into the concrete TextSrc class. `asciiSrcObjectClass`
 * survives as an alias of `textSrcObjectClass`. The IswAsciiSource* functions
 * are kept as thin wrappers around IswTextSource* equivalents. Prefer
 * <ISW/TextSrc.h> in new code.
 */

#ifndef _ISWAsciiSrc_h
#define _ISWAsciiSrc_h

#include <ISW/TextSrc.h>

extern WidgetClass asciiSrcObjectClass;

typedef struct _TextSrcClassRec *AsciiSrcObjectClass;
typedef struct _TextSrcRec      *AsciiSrcObject;

#define AsciiSourceObjectClass AsciiSrcObjectClass
#define AsciiSourceObject      AsciiSrcObject

_XFUNCPROTOBEGIN

extern void    IswAsciiSourceFreeString(Widget);
extern Boolean IswAsciiSave(Widget);
extern Boolean IswAsciiSaveAsFile(Widget, _Xconst char*);
extern Boolean IswAsciiSourceChanged(Widget);

_XFUNCPROTOEND

#endif /* _ISWAsciiSrc_h */
