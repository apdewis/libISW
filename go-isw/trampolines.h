#ifndef _GO_ISW_TRAMPOLINES_H
#define _GO_ISW_TRAMPOLINES_H

#include <ISW/Intrinsic.h>
#include <ISW/IswEvent.h>
#include <ISW/IswDragDrop.h>

extern void goCallbackTrampoline(Widget, IswPointer, IswPointer);
extern void goEventHandlerTrampoline(Widget, IswPointer, IswEvent*, Boolean*);
extern void goTimerTrampoline(IswPointer, IswIntervalId*);
extern void goInputTrampoline(IswPointer, int*, IswInputId*);
extern void goSignalTrampoline(IswPointer, IswSignalId*);
extern Boolean goWorkProcTrampoline(IswPointer);
extern void goActionTrampoline(Widget, IswEvent*, String*, Cardinal*);
extern void goActionNameHook(Widget, IswPointer, String, IswEvent*, String*, Cardinal*);
extern void goActionHookTrampoline(Widget, IswPointer, String, IswEvent*, String*, Cardinal*);
extern Boolean goDragConvertTrampoline(Widget, const char*, IswPointer*, unsigned long*, int*, IswPointer);
extern void goDragFinishedTrampoline(Widget, IswDndAction, Boolean, IswPointer);

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

static inline void _isw_install_action_name_hook(IswAppContext app) {
	IswAppAddActionHook(app, goActionNameHook, NULL);
}

static inline IswActionHookProc _isw_action_hook_trampoline(void) {
	return goActionHookTrampoline;
}

static inline IswDragConvertProc _isw_drag_convert_trampoline(void) {
	return goDragConvertTrampoline;
}

static inline IswDragFinishedProc _isw_drag_finished_trampoline(void) {
	return goDragFinishedTrampoline;
}

#endif
