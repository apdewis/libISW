#ifndef _GO_ISW_TRAMPOLINES_H
#define _GO_ISW_TRAMPOLINES_H

#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>

extern void goCallbackTrampoline(Widget, IswPointer, IswPointer);
extern void goEventHandlerTrampoline(Widget, IswPointer, IswEvent*, Boolean*);
extern void goTimerTrampoline(IswPointer, IswIntervalId*);
extern void goInputTrampoline(IswPointer, int*, IswInputId*);
extern void goSignalTrampoline(IswPointer, IswSignalId*);
extern Boolean goWorkProcTrampoline(IswPointer);
extern void goActionTrampoline(Widget, IswEvent*, String*, Cardinal*);

static inline IswCallbackProc _isw_cb_trampoline(void) {
	return goCallbackTrampoline;
}
static inline IswEventHandler _isw_eh_trampoline(void) {
	return goEventHandlerTrampoline;
}
static inline IswTimerCallbackProc _isw_timer_trampoline(void) {
	return goTimerTrampoline;
}
static inline IswInputCallbackProc _isw_input_trampoline(void) {
	return goInputTrampoline;
}
static inline IswWorkProc _isw_workproc_trampoline(void) {
	return goWorkProcTrampoline;
}

static inline void _isw_register_action(IswAppContext app, const char *name) {
	IswActionsRec rec;
	rec.string = (String)name;
	rec.proc = goActionTrampoline;
	IswAppAddActions(app, &rec, 1);
}

#endif
