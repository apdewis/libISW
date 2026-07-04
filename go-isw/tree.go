package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Tree.h>
*/
import "C"

// TreeForceLayout forces the tree widget to lay out its children.
func TreeForceLayout(w Widget) {
	C.IswTreeForceLayout(w.c)
}
