package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Tabs.h>
*/
import "C"

// TabsSetTop brings the given child's tab to the front.
func TabsSetTop(tabs Widget, child Widget) {
	C.IswTabsSetTop(tabs.c, child.c)
}
