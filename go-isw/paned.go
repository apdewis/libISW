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
