package isw

/*
#include <ISW/Intrinsic.h>
#include "wrappers.h"
*/
import "C"

func handleToPtr(h callbackHandle) C.IswPointer {
	return C._isw_handle_to_ptr(C.uintptr_t(h))
}
