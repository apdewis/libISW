package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/Tabs.h>
*/
import "C"

// TabsCallbackData is the Go representation of Tabs tabCallback data.
type TabsCallbackData struct {
	Child    Widget
	TabIndex int
}

// ParseTabsCallbackData converts C call_data from a Tabs tabCallback to
// Go.
func ParseTabsCallbackData(callData CallData) *TabsCallbackData {
	cd := (*C.TabsCallbackStruct)(callData.ptr)
	return &TabsCallbackData{
		Child:    Widget{cd.child},
		TabIndex: int(cd.tab_index),
	}
}

// TabsSetTop brings the given child's tab to the front.
func TabsSetTop(tabs Widget, child Widget) {
	C.IswTabsSetTop(tabs.c, child.c)
}
