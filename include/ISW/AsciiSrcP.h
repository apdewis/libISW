/*
 * Deprecated compatibility shim.
 *
 * AsciiSrc's private types were absorbed into TextSrcP.h. Include that
 * instead.
 */

#ifndef _ISW_IswAsciiSrcP_h
#define _ISW_IswAsciiSrcP_h

#include <ISW/TextSrcP.h>
#include <ISW/AsciiSrc.h>

typedef TextSrcClassRec AsciiSrcClassRec;
typedef TextSrcPart     AsciiSrcPart;
typedef TextSrcRec      AsciiSrcRec;

extern TextSrcClassRec asciiSrcClassRec;

/* Legacy macros kept for any residual in-tree uses. */
#ifdef L_tmpnam
#define TMPSIZ L_tmpnam
#else
#define TMPSIZ 32
#endif

#define MAGIC_VALUE ((ISWTextPosition) -1)
#define streq(a, b) ( strcmp((a), (b)) == 0 )

#endif /* _ISW_IswAsciiSrcP_h */
