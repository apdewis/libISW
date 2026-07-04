package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Viewport.h>
*/
import "C"

// ViewportSetLocation scrolls the viewport to fractional offsets in
// [0, 1] of the child's size; pass a negative value to leave an axis
// unchanged.
func ViewportSetLocation(w Widget, xoff, yoff float64) {
	C.IswViewportSetLocation(w.c, C.float(xoff), C.float(yoff))
}

// ViewportSetCoordinates scrolls the viewport so the child coordinate
// (x, y) is at the top-left.
func ViewportSetCoordinates(w Widget, x, y int) {
	C.IswViewportSetCoordinates(w.c, C.Position(x), C.Position(y))
}
