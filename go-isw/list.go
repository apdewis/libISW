package isw

/*
#include <ISW/Intrinsic.h>
#include <ISW/List.h>
*/
import "C"

// ListCallbackData is the Go representation of List select callback data.
type ListCallbackData struct {
	String string
	Index  int
}

// ParseListCallbackData converts call data from a List callback to Go.
func ParseListCallbackData(callData CallData) *ListCallbackData {
	cd := (*C.IswListReturnStruct)(callData.ptr)
	return &ListCallbackData{
		String: C.GoString(cd.string),
		Index:  int(cd.list_index),
	}
}
