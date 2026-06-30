#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>
#include "_cgo_export.h"

void goCallbackTrampoline(Widget w, IswPointer closure, IswPointer call_data) {
	goCallbackBridge((uintptr_t)w, (uintptr_t)closure, (uintptr_t)call_data);
}

void goEventHandlerTrampoline(Widget w, IswPointer closure,
                              IswEvent *event, Boolean *cont) {
	Boolean c = *cont;
	goEventHandlerBridge((uintptr_t)w, (uintptr_t)closure, event, &c);
	*cont = c;
}

void goTimerTrampoline(IswPointer closure, IswIntervalId *id) {
	goTimerBridge((uintptr_t)closure, (uintptr_t)*id);
}

void goInputTrampoline(IswPointer closure, int *source, IswInputId *id) {
	goInputBridge((uintptr_t)closure, *source, (uintptr_t)*id);
}

void goSignalTrampoline(IswPointer closure, IswSignalId *id) {
	goSignalBridge((uintptr_t)closure, (uintptr_t)*id);
}

Boolean goWorkProcTrampoline(IswPointer closure) {
	return goWorkProcBridge((uintptr_t)closure);
}

void goActionTrampoline(Widget w, IswEvent *event,
                        String *params, Cardinal *num_params) {
	goActionBridge((uintptr_t)w, event, params, *num_params);
}
