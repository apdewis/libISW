// Package isw provides Go bindings for the ISW (Infi Systems Widgets) library,
// an XCB-based widget toolkit.
//
// All ISW calls must happen on a single OS thread. Call Init or AppInitialize
// from the goroutine that will run the event loop — it locks the thread
// automatically. Widget updates from other goroutines must be posted via
// AppContext.Invoke.
package isw

/*
#cgo pkg-config: isw xcb-icccm
#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWRender.h>
#include <ISW/ISWPNG.h>
#include <ISW/ISWSVG.h>
#include <ISW/IswDragDrop.h>
#include <stdlib.h>
#include <string.h>
#include "trampolines.h"
#include "wrappers.h"
*/
import "C"

import (
	"os"
	"runtime"
	"unsafe"
)

func init() {
	runtime.LockOSThread()
}

// AppContext wraps an IswAppContext.
type AppContext struct {
	c C.IswAppContext
}

// Display wraps an IswDisplay.
type Display struct {
	c C.IswDisplay
}

// Widget wraps a C Widget pointer.
type Widget struct {
	c C.Widget
}

// WidgetClass wraps a C WidgetClass pointer.
type WidgetClass struct {
	c C.WidgetClass
}

// NilWidget is a zero-value Widget (NULL).
var NilWidget = Widget{}

// IsNil returns true if the widget is NULL.
func (w Widget) IsNil() bool { return w.c == nil }

func buildCArgv() (argc C.int, argv **C.char) {
	args := os.Args
	n := len(args)
	arr := C._isw_alloc_string_array(C.int(n))
	for i, s := range args {
		C._isw_string_array_set(arr, C.int(i), C.CString(s))
	}
	return C.int(n), arr
}

func makeFallback(fallbackResources []string) **C.char {
	if len(fallbackResources) == 0 {
		return nil
	}
	arr := C._isw_alloc_string_array(C.int(len(fallbackResources)))
	for i, s := range fallbackResources {
		C._isw_string_array_set(arr, C.int(i), C.CString(s))
	}
	return arr
}

// AppInitialize initialises the toolkit and creates a top-level shell.
func AppInitialize(appClass string, fallbackResources []string) (AppContext, Widget) {
	cClass := C.CString(appClass)
	defer C.free(unsafe.Pointer(cClass))

	argc, argv := buildCArgv()
	cFallback := makeFallback(fallbackResources)

	var ac C.IswAppContext
	w := C._isw_app_initialize(&ac, cClass, &argc, argv, cFallback, nil, 0)
	return AppContext{ac}, Widget{w}
}

// OpenApplication initialises and creates a shell of the given class.
func OpenApplication(appClass string, shellClass WidgetClass, fallbackResources []string) (AppContext, Widget) {
	cClass := C.CString(appClass)
	defer C.free(unsafe.Pointer(cClass))

	argc, argv := buildCArgv()
	cFallback := makeFallback(fallbackResources)

	var ac C.IswAppContext
	w := C._isw_open_application(&ac, cClass, &argc, argv, cFallback, shellClass.c, nil, 0)
	return AppContext{ac}, Widget{w}
}

// MainLoop runs the event loop. Does not return.
func (ac AppContext) MainLoop() {
	C.IswAppMainLoop(ac.c)
}

// ProcessEvent processes one event matching mask.
func (ac AppContext) ProcessEvent(mask uint) {
	C.IswAppProcessEvent(ac.c, C.IswInputMask(mask))
}

// Pending returns the pending event mask.
func (ac AppContext) Pending() uint {
	return uint(C.IswAppPending(ac.c))
}

// Destroy destroys the application context.
func (ac AppContext) Destroy() {
	C.IswDestroyApplicationContext(ac.c)
}

// Invoke schedules a function to run on the event-loop thread via
// IswAppAddWorkProc. Safe to call from any goroutine.
func (ac AppContext) Invoke(fn func()) {
	h := registerWorkProc(func() bool {
		fn()
		return true
	})
	C.IswAppAddWorkProc(ac.c, C._isw_workproc_trampoline(),
		handleToPtr(h))
}
