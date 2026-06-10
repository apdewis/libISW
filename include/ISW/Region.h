/*
 * Region.h - Neutral rectangle-set region API
 *
 * Copyright (c) 2026 ISW Project
 *
 * A simple rectangle-set region (a small fixed-capacity list of IswRectangle
 * plus a bounding box).  Platform-neutral by construction: pure geometry math,
 * no windowing-system dependency.  The opaque handle is IswRegion
 * (ISW/IswTypes.h); ISWRegionPtr is a same-typed alias used by the API below.
 */

#ifndef _ISW_Region_h
#define _ISW_Region_h

#include <ISW/Intrinsic.h>     /* IswRegion handle, IswRectangle */
#include <ISW/ISWPlatform.h>   /* IswRectangle */

/* Pointer-to-region handle.  Same type as IswRegion (IswTypes.h). */
typedef struct _IswRegion *ISWRegionPtr;

/* Create an empty region (caller frees with ISWDestroyRegion). */
ISWRegionPtr ISWCreateRegion(void);

/* Free a region. */
void ISWDestroyRegion(ISWRegionPtr region);

/* Add `rect` to `source`, storing the result in `dest` (dest may equal
   source).  Simplified: appends the rectangle without merging overlaps. */
void ISWUnionRectWithRegion(IswRectangle *rect, ISWRegionPtr source,
                            ISWRegionPtr dest);

/* Subtract regS from regM into regD.  Simplified for the frame case
   (1 outer rect minus 1 inner rect -> up to 4 edge rectangles). */
void ISWSubtractRegion(ISWRegionPtr regM, ISWRegionPtr regS, ISWRegionPtr regD);

#endif /* _ISW_Region_h */
