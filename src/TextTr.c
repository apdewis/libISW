/*

Copyright (c) 1991, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

/* #TODO these will want to be dynamic based on selected keymap at some
 * point for defaults that make sense for non latin/non US-101 keymaps
 * and perhaps helpers to make it configurable at all
 */
const char *_IswDefaultTextTranslations1 =
"\
Ctrl<Key>A:	select-all() \n\
Ctrl<Key>C:	copy-selection(CLIPBOARD) \n\
Ctrl<Key>V:	insert-selection(CLIPBOARD) \n\
Ctrl<Key>X:	kill-selection() \n\
<Key>Home:	beginning-of-line() \n\
:<Key>KP_Home:	beginning-of-line() \n\
Shift<Key>Home:	beginning-of-file() \n\
:Shift<Key>KP_Home:	beginning-of-file() \n\
<Key>End:	end-of-line() \n\
:<Key>KP_End:	end-of-line() \n\
Shift<Key>End:	end-of-file() \n\
:Shift<Key>KP_End:	end-of-file() \n\
<Key>Next:	next-page() \n\
:<Key>KP_Next:	next-page() \n\
<Key>Prior:	previous-page() \n\
:<Key>KP_Prior: previous-page() \n\
<Key>Right:	forward-character() \n\
:<Key>KP_Right: forward-character() \n\
<Key>Left:	backward-character() \n\
:<Key>KP_Left:	backward-character() \n\
<Key>Down:	next-line() \n\
:<Key>KP_Down:	next-line() \n\
<Key>Up:	previous-line() \n\
:<Key>KP_Up:	previous-line() \n\
<Key>Delete:	delete-previous-character() \n\
:<Key>KP_Delete: delete-previous-character() \n\
<Key>BackSpace:	delete-previous-character() \n\
<Key>Return:	newline() \n\
:<Key>KP_Enter:	newline() \n\
<Key>:		insert-char() \n\
<EnterWindow>:	enter-window() \n\
<LeaveWindow>:	leave-window() \n\
<FocusIn>:	focus-in() \n\
<FocusOut>:	focus-out() \n\
<Btn1Down>:	select-start() \n\
<Btn1Motion>:	extend-adjust() \n\
<Btn1Up>:	extend-end(PRIMARY, CLIPBOARD) \n\
<Btn2Down>:	insert-selection(PRIMARY, CLIPBOARD) \n\
<Btn3Down>:	extend-start() \n\
<Btn3Motion>:	extend-adjust() \n\
<Btn3Up>:	extend-end(PRIMARY, CLIPBOARD) \n\
";
