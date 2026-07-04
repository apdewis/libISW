#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>
#include <ISW/IswDragDrop.h>
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

/* The action-proc signature does not carry the action's name, so a hook
 * records it just before dispatch (hooks run synchronously before the
 * proc on the toolkit thread). */
static String _isw_current_action_name;

void goActionNameHook(Widget w, IswPointer closure, String name,
                      IswEvent *event, String *params, Cardinal *num_params) {
	(void)w; (void)closure; (void)event; (void)params; (void)num_params;
	_isw_current_action_name = name;
}

void goActionTrampoline(Widget w, IswEvent *event,
                        String *params, Cardinal *num_params) {
	goActionBridge((uintptr_t)w, _isw_current_action_name, event,
	               params, *num_params);
}

void goActionHookTrampoline(Widget w, IswPointer closure, String name,
                            IswEvent *event, String *params,
                            Cardinal *num_params) {
	goActionHookBridge((uintptr_t)w, (uintptr_t)closure, name, event,
	                   params, *num_params);
}

Boolean goDragConvertTrampoline(Widget w, const char *target_type,
                                IswPointer *data_return,
                                unsigned long *length_return,
                                int *format_return, IswPointer closure) {
	return goDragConvertBridge((uintptr_t)w, (char *)target_type,
	                           data_return, length_return, format_return,
	                           (uintptr_t)closure);
}

void goDragFinishedTrampoline(Widget w, IswDndAction performed_action,
                              Boolean accepted, IswPointer closure) {
	goDragFinishedBridge((uintptr_t)w, (int)performed_action, accepted,
	                     (uintptr_t)closure);
}
