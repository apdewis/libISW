package isw

/*
#include <stdlib.h>
#include <string.h>
#include <ISW/Intrinsic.h>

static char *isw_clip_text = NULL;

static Boolean isw_clip_offer(Widget w, IswPointer *value, unsigned long *length) {
	(void)w;
	size_t n;
	char *copy;
	if (isw_clip_text == NULL)
		return False;
	n = strlen(isw_clip_text);
	copy = IswMalloc((Cardinal)(n + 1));
	memcpy(copy, isw_clip_text, n + 1);
	*value = (IswPointer)copy;
	*length = (unsigned long)n;
	return True;
}

static void isw_clip_lose(Widget w) {
	(void)w;
	free(isw_clip_text);
	isw_clip_text = NULL;
}

static void isw_clipboard_set(Widget w, const char *text) {
	free(isw_clip_text);
	isw_clip_text = strdup(text);
	IswSelectionOffer(w, IswLastTimestampProcessed(IswDisplayOf(w)),
			  isw_clip_offer, isw_clip_lose);
}
*/
import "C"

import "unsafe"

// ClipboardSet places text on the clipboard selection, owned by w. The
// text stays available until another client takes the selection.
func ClipboardSet(w Widget, text string) {
	cs := C.CString(text)
	defer C.free(unsafe.Pointer(cs))
	C.isw_clipboard_set(w.c, cs)
}
