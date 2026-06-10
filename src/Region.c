/*
 * Region.c - Neutral rectangle-set region implementation
 *
 * Copyright (c) 2026 ISW Project
 *
 * A small fixed-capacity set of rectangles with a cached bounding box.  Pure
 * geometry: no windowing-system dependency.  See ISW/Region.h.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <ISW/Region.h>

/* Maximum number of rectangles in a region (can be expanded if needed). */
#define ISW_REGION_MAXRECTS 64

/* Internal region structure.  Its pointer form is the neutral IswRegion handle
   (ISW/IswTypes.h) and the ISWRegionPtr alias. */
struct _IswRegion {
    int numRects;
    IswRectangle rects[ISW_REGION_MAXRECTS];
    IswRectangle extents;  /* Bounding box */
};

ISWRegionPtr
ISWCreateRegion(void)
{
    return (ISWRegionPtr) calloc(1, sizeof(struct _IswRegion));
}

void
ISWDestroyRegion(ISWRegionPtr region)
{
    if (region)
        free(region);
}

/* Recompute the cached bounding box from the current rectangle list. */
static void
UpdateRegionExtents(ISWRegionPtr region)
{
    int i;
    int16_t minx, miny, maxx, maxy;

    if (region->numRects == 0) {
        region->extents.x = 0;
        region->extents.y = 0;
        region->extents.width = 0;
        region->extents.height = 0;
        return;
    }

    minx = region->rects[0].x;
    miny = region->rects[0].y;
    maxx = region->rects[0].x + region->rects[0].width;
    maxy = region->rects[0].y + region->rects[0].height;

    for (i = 1; i < region->numRects; i++) {
        if (region->rects[i].x < minx)
            minx = region->rects[i].x;
        if (region->rects[i].y < miny)
            miny = region->rects[i].y;
        if (region->rects[i].x + region->rects[i].width > maxx)
            maxx = region->rects[i].x + region->rects[i].width;
        if (region->rects[i].y + region->rects[i].height > maxy)
            maxy = region->rects[i].y + region->rects[i].height;
    }

    region->extents.x = minx;
    region->extents.y = miny;
    region->extents.width = maxx - minx;
    region->extents.height = maxy - miny;
}

void
ISWUnionRectWithRegion(IswRectangle *rect, ISWRegionPtr source, ISWRegionPtr dest)
{
    int i;

    if (!rect || !source || !dest)
        return;

    /* Copy source to dest if different */
    if (source != dest) {
        dest->numRects = source->numRects;
        for (i = 0; i < source->numRects; i++)
            dest->rects[i] = source->rects[i];
    }

    /* Add the new rectangle if there's room. */
    if (dest->numRects < ISW_REGION_MAXRECTS) {
        dest->rects[dest->numRects] = *rect;
        dest->numRects++;
    }

    UpdateRegionExtents(dest);
}

void
ISWSubtractRegion(ISWRegionPtr regM, ISWRegionPtr regS, ISWRegionPtr regD)
{
    /*
     * Simplified subtraction for frame regions: if regM has 1 rect (outer) and
     * regS has 1 rect (inner), produce up to 4 rectangles for the frame.
     */
    if (!regM || !regS || !regD)
        return;

    if (regM->numRects == 1 && regS->numRects == 1) {
        IswRectangle *outer = &regM->rects[0];
        IswRectangle *inner = &regS->rects[0];

        regD->numRects = 0;

        /* Top rectangle */
        if (inner->y > outer->y) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = outer->y;
            regD->rects[regD->numRects].width = outer->width;
            regD->rects[regD->numRects].height = inner->y - outer->y;
            regD->numRects++;
        }

        /* Bottom rectangle */
        if ((inner->y + inner->height) < (outer->y + outer->height)) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = inner->y + inner->height;
            regD->rects[regD->numRects].width = outer->width;
            regD->rects[regD->numRects].height = (outer->y + outer->height) - (inner->y + inner->height);
            regD->numRects++;
        }

        /* Left rectangle */
        if (inner->x > outer->x) {
            regD->rects[regD->numRects].x = outer->x;
            regD->rects[regD->numRects].y = inner->y;
            regD->rects[regD->numRects].width = inner->x - outer->x;
            regD->rects[regD->numRects].height = inner->height;
            regD->numRects++;
        }

        /* Right rectangle */
        if ((inner->x + inner->width) < (outer->x + outer->width)) {
            regD->rects[regD->numRects].x = inner->x + inner->width;
            regD->rects[regD->numRects].y = inner->y;
            regD->rects[regD->numRects].width = (outer->x + outer->width) - (inner->x + inner->width);
            regD->rects[regD->numRects].height = inner->height;
            regD->numRects++;
        }

        UpdateRegionExtents(regD);
    } else {
        /* For complex cases, just copy regM */
        int i;
        regD->numRects = regM->numRects;
        for (i = 0; i < regM->numRects; i++)
            regD->rects[i] = regM->rects[i];
        regD->extents = regM->extents;
    }
}
