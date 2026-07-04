package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Paned.h>

static void _isw_paned_set_refigure_mode(Widget w, int mode) {
	IswPanedSetRefigureMode(w, (Boolean)mode);
}
*/
import "C"

// PanedSetRefigureMode enables or disables the paned widget's relayout
// (refigure) processing; re-enabling triggers a relayout.
func PanedSetRefigureMode(w Widget, mode bool) {
	m := C.int(0)
	if mode {
		m = 1
	}
	C._isw_paned_set_refigure_mode(w.c, m)
}

// PanedAllowResize sets whether the paned widget honours geometry
// requests from the given child.
func PanedAllowResize(child Widget, allow bool) {
	a := C.Boolean(0)
	if allow {
		a = 1
	}
	C.IswPanedAllowResize(child.c, a)
}

// PanedSetMinMax sets the minimum and maximum size of a pane.
func PanedSetMinMax(child Widget, min, max int) {
	C.IswPanedSetMinMax(child.c, C.int(min), C.int(max))
}

// PanedGetMinMax returns the minimum and maximum size of a pane.
func PanedGetMinMax(child Widget) (min, max int) {
	var cMin, cMax C.int
	C.IswPanedGetMinMax(child.c, &cMin, &cMax)
	return int(cMin), int(cMax)
}

// PanedGetNumSub returns the number of panes.
func PanedGetNumSub(w Widget) int {
	return int(C.IswPanedGetNumSub(w.c))
}
