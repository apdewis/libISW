/*

Copyright (c) 1989, 1994  X Consortium

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

/************************************************************
 *
 * This file is broken up into three sections one dealing with
 * each of the three popups created here:
 *
 * FileInsert, Search, and Replace.
 *
 * There is also a section at the end for utility functions
 * used by all more than one of these dialogs.
 *
 * The following functions are the only non-static ones defined
 * in this module.  They are located at the begining of the
 * section that contains this dialog box that uses them.
 *
 * void _IswTextInsertFileAction(w, event, params, num_params);
 * void _IswTextDoSearchAction(w, event, params, num_params);
 * void _IswTextDoReplaceAction(w, event, params, num_params);
 * void _IswTextInsertFile(w, event, params, num_params);
 *
 *************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <ISW/TextP.h>
#include <ISW/Text.h>
#include <ISW/Cardinals.h>
#include <ISW/Command.h>
#include <ISW/Form.h>
#include <ISW/Toggle.h>
#include <ISW/IswArgMacros.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/Xos.h>		/* for O_RDONLY */
#include <errno.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_icccm.h>
#include "ISWXcbDraw.h"

#ifdef X_NOT_STDC_ENV
extern int errno;
#endif

#define INSERT_FILE ("Enter Filename:")

#define SEARCH_LABEL_1  ("Use <Tab> to change fields.")
#define SEARCH_LABEL_2  ("Use ^q<Tab> for <Tab>.")
#define DISMISS_NAME  ("cancel")
#define DISMISS_NAME_LEN 6
#define FORM_NAME     ("form")
#define LABEL_NAME    ("label")
#define TEXT_NAME     ("text")

#define R_OFFSET      1

extern char *_IswTextGetText(TextWidget, ISWTextPosition, ISWTextPosition);

static void CenterWidgetOnPoint(Widget, xcb_generic_event_t *);
static void PopdownSearch(Widget, IswPointer, IswPointer);
static void DoInsert(Widget, IswPointer, IswPointer);
static void _SetField(Widget, Widget);
static void InitializeSearchWidget(struct SearchAndReplace *,
                                   IswTextScanDirection, Boolean);
static void SetResource(Widget, char *, IswArgVal);
static void SetSearchLabels(struct SearchAndReplace *, String, String, Boolean);
static void DoReplaceOne(Widget, IswPointer, IswPointer);
static void DoReplaceAll(Widget, IswPointer, IswPointer);
static Widget CreateDialog(Widget, String, String, void (*)(Widget, String, Widget));
static Widget GetShell(Widget);
static void SetWMProtocolTranslations(Widget);
static Boolean DoSearch(struct SearchAndReplace *);
static Boolean SetResourceByName(Widget, char *, char *, IswArgVal);
static Boolean Replace(struct SearchAndReplace *, Boolean, Boolean);
static String GetString(Widget);
static String GetStringRaw(Widget);
static void AddInsertFileChildren(Widget, String, Widget);
static Boolean InsertFileNamed(Widget, char *);
static void AddSearchChildren(Widget, String, Widget);

void _IswTextDoReplaceAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextDoSearchAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextInsertFile(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextInsertFileAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextPopdownSearchAction(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextSearch(Widget, xcb_generic_event_t *, String *, Cardinal *);
void _IswTextSetField(Widget, xcb_generic_event_t *, String *, Cardinal *);


static char radio_trans_string[] =
    "<Btn1Down>,<Btn1Up>:   set() notify()";

static char search_text_trans[] =
  "~Shift<Key>Return:      DoSearchAction(Popdown) \n\
   Shift<Key>Return:       DoSearchAction() SetField(Replace) \n\
   Ctrl<Key>q,<Key>Tab:    insert-char()    \n\
   Ctrl<Key>c:             PopdownSearchAction() \n\
   <Btn1Down>:             select-start() SetField(Search) \n\
   <Key>Tab:               DoSearchAction() SetField(Replace)";

static char rep_text_trans[] =
  "~Shift<Key>Return:      DoReplaceAction(Popdown) \n\
   Shift<Key>Return:       SetField(Search) \n\
   Ctrl<Key>q,<Key>Tab:    insert-char()     \n\
   Ctrl<Key>c:             PopdownSearchAction() \n\
   <Btn1Down>:             select-start() DoSearchAction() SetField(Replace)\n\
   <Key>Tab:               SetField(Search)";

/************************************************************
 *
 * This section of the file contains all the functions that
 * the file insert dialog box uses.
 *
 ************************************************************/

/*	Function Name: _IswTextInsertFileAction
 *	Description: Action routine that can be bound to dialog box's
 *                   Text Widget that will insert a file into the main
 *                   Text Widget.
 *	Arguments:   (Standard Action Routine args)
 *	Returns:     none.
 */

/* ARGSUSED */
void
_IswTextInsertFileAction(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  DoInsert(w, (IswPointer) IswParent(IswParent(IswParent(w))), (IswPointer)NULL);
}

/*	Function Name: _IswTextInsertFile
 *	Description: Action routine that can be bound to the text widget
 *                   it will popup the insert file dialog box.
 *	Arguments:   w - the text widget.
 *                   event - X Event (used to get x and y location).
 *                   params, num_params - the parameter list.
 *	Returns:     none.
 *
 * NOTE:
 *
 * The parameter list may contain one entry.
 *
 *  Entry:  This entry is optional and contains the value of the default
 *          file to insert.
 */

void
_IswTextInsertFile(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget)w;
  char * ptr;
  IswTextEditType edit_mode;
  IswArgBuilder ab = IswArgBuilderInit();
  IswArgEditType(&ab, (IswArgVal)&edit_mode);
  IswGetValues(ctx->text.source, ab.args, ab.count);

  if (edit_mode != IswtextEdit) {
    /* XCB equivalent of XBell */
    xcb_bell(IswDisplay(w), 0);
    return;
  }

  if (*num_params == 0)
    ptr = "";
  else
    ptr = params[0];

  if (!ctx->text.file_insert) {
    ctx->text.file_insert = CreateDialog(w, ptr, "insertFile",
					 AddInsertFileChildren);
    IswRealizeWidget(ctx->text.file_insert);
    SetWMProtocolTranslations(ctx->text.file_insert);
  }

  CenterWidgetOnPoint(ctx->text.file_insert, event);
  IswPopup(ctx->text.file_insert, IswGrabNone);
}

/*	Function Name: PopdownFileInsert
 *	Description: Pops down the file insert button.
 *	Arguments: w - the widget that caused this action.
 *                 closure - a pointer to the main text widget that
 *                           popped up this dialog.
 *                 call_data - *** NOT USED ***.
 *	Returns: none.
 */

/* ARGSUSED */
static void
PopdownFileInsert(Widget w, IswPointer closure, IswPointer call_data)
{
  TextWidget ctx = (TextWidget) closure;

  IswPopdown( ctx->text.file_insert );
  (void) SetResourceByName( ctx->text.file_insert, LABEL_NAME,
			   IswNlabel, (IswArgVal) INSERT_FILE);
}

/*	Function Name: DoInsert
 *	Description: Actually insert the file named in the text widget
 *                   of the file dialog.
 *	Arguments:   w - the widget that activated this callback.
 *                   closure - a pointer to the text widget to insert the
 *                             file into.
 *	Returns: none.
 */

/* ARGSUSED */
static void
DoInsert(Widget w, IswPointer closure, IswPointer call_data)
{
  TextWidget ctx = (TextWidget) closure;
  char buf[BUFSIZ], msg[BUFSIZ];
  Widget temp_widget;

  (void) sprintf(buf, "%s.%s", FORM_NAME, TEXT_NAME);
  if ( (temp_widget = IswNameToWidget(ctx->text.file_insert, buf)) == NULL ) {
    (void) strcpy(msg,
	   "*** Error: Could not get text widget from file insert popup");
  }
  else
    if (InsertFileNamed( (Widget) ctx, GetString( temp_widget ))) {
      PopdownFileInsert(w, closure, call_data);
      return;
    }
    else
      (void) sprintf( msg, "*** Error: %s ***", strerror(errno));

  (void)SetResourceByName(ctx->text.file_insert,
			  LABEL_NAME, IswNlabel, (IswArgVal) msg);
  /* XCB equivalent of XBell */
  xcb_bell(IswDisplay(w), 0);
}

/*	Function Name: InsertFileNamed
 *	Description: Inserts a file into the text widget.
 *	Arguments: tw - The text widget to insert this file into.
 *                 str - name of the file to insert.
 *	Returns: TRUE if the insert was sucessful, FALSE otherwise.
 */


static Boolean
InsertFileNamed(Widget tw, char *str)
{
  FILE *file;
  ISWTextBlock text;
  ISWTextPosition pos;

  if ( (str == NULL) || (strlen(str) == 0) ||
       ((file = fopen(str, "r")) == NULL))
    return(FALSE);

  pos = IswTextGetInsertionPoint(tw);

  fseek(file, 0L, 2);


  text.firstPos = 0;
  text.length = (ftell(file))/sizeof(unsigned char);
  text.ptr = IswMalloc((text.length + 1) * sizeof(unsigned char));
  text.format = IswFmt8Bit;

  fseek(file, 0L, 0);
  if (fread(text.ptr, sizeof(unsigned char), text.length, file) != text.length)
      IswErrorMsg("readError", "insertFileNamed", "IswError",
                 "fread returned error.", NULL, NULL);

  if (IswTextReplace(tw, pos, pos, &text) != IswEditDone) {
     IswFree(text.ptr);
     fclose(file);
     return(FALSE);
  }
  pos += text.length;
  IswFree(text.ptr);
  fclose(file);
  IswTextSetInsertionPoint(tw, pos);
  return(TRUE);
}


/*	Function Name: AddInsertFileChildren
 *	Description: Adds all children to the InsertFile dialog widget.
 *	Arguments: form - the form widget for the insert dialog widget.
 *                 ptr - a pointer to the initial string for the Text Widget.
 *                 tw - the main text widget.
 *	Returns: none
 */

static void
AddInsertFileChildren(Widget form, String ptr, Widget tw)
{
  Widget label, text, cancel, insert;
  IswTranslations trans;
  IswArgBuilder ab = IswArgBuilderInit();

  IswArgLabel(&ab, INSERT_FILE);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgResizable(&ab, TRUE);
  IswArgBorderWidth(&ab, 0);
  label = IswCreateManagedWidget (LABEL_NAME, labelWidgetClass, form,
				 ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgFromVert(&ab, label);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainRight);
  IswArgEditType(&ab, IswtextEdit);
  IswArgResizable(&ab, TRUE);
  IswArgResize(&ab, IswtextResizeWidth);
  IswArgString(&ab, ptr);
  text = IswCreateManagedWidget(TEXT_NAME, textWidgetClass, form,
				ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Insert File");
  IswArgFromVert(&ab, text);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  insert = IswCreateManagedWidget("insert", commandWidgetClass, form,
				 ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Cancel");
  IswArgFromVert(&ab, text);
  IswArgFromHoriz(&ab, insert);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  cancel = IswCreateManagedWidget(DISMISS_NAME, commandWidgetClass, form,
				 ab.args, ab.count);

  IswAddCallback(cancel, IswNcallback, PopdownFileInsert, (IswPointer) tw);
  IswAddCallback(insert, IswNcallback, DoInsert, (IswPointer) tw);

  IswSetKeyboardFocus(form, text);

/*
 * Bind <CR> to insert file.
 */

  trans = IswParseTranslationTable("<Key>Return: InsertFileAction()");
  IswOverrideTranslations(text, trans);

}

/************************************************************
 *
 * This section of the file contains all the functions that
 * the search dialog box uses.
 *
 ************************************************************/

/*	Function Name: _IswTextDoSearchAction
 *	Description: Action routine that can be bound to dialog box's
 *                   Text Widget that will search for a string in the main
 *                   Text Widget.
 *	Arguments:   (Standard Action Routine args)
 *	Returns:     none.
 *
 * Note:
 *
 * If the search was sucessful and the argument popdown is passed to
 * this action routine then the widget will automatically popdown the
 * search widget.
 */

/* ARGSUSED */
void
_IswTextDoSearchAction(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget tw = (TextWidget) IswParent(IswParent(IswParent(w)));
  Boolean popdown = FALSE;

  if ( (*num_params == 1) &&
       ((params[0][0] == 'p') || (params[0][0] == 'P')) )
      popdown = TRUE;

  if (DoSearch(tw->text.search) && popdown)
    PopdownSearch(w, (IswPointer) tw->text.search, (IswPointer)NULL);
}

/*	Function Name: _IswTextPopdownSearchAction
 *	Description: Action routine that can be bound to dialog box's
 *                   Text Widget that will popdown the search widget.
 *	Arguments:   (Standard Action Routine args)
 *	Returns:     none.
 */

/* ARGSUSED */
void
_IswTextPopdownSearchAction(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget tw = (TextWidget) IswParent(IswParent(IswParent(w)));

  PopdownSearch(w, (IswPointer) tw->text.search, (IswPointer)NULL);
}

/*	Function Name: PopdownSeach
 *	Description: Pops down the search widget and resets it.
 *	Arguments: w - *** NOT USED ***.
 *                 closure - a pointer to the search structure.
 *                 call_data - *** NOT USED ***.
 *	Returns: none
 */

/* ARGSUSED */
static void
PopdownSearch(Widget w, IswPointer closure, IswPointer call_data)
{
  struct SearchAndReplace * search = (struct SearchAndReplace *) closure;

  IswPopdown( search->search_popup );
  SetSearchLabels(search, SEARCH_LABEL_1, SEARCH_LABEL_2, FALSE);
}

/*	Function Name: SearchButton
 *	Description: Performs a search when the button is clicked.
 *	Arguments: w - *** NOT USED **.
 *                 closure - a pointer to the search info.
 *                 call_data - *** NOT USED ***.
 *	Returns:
 */

/* ARGSUSED */
static void
SearchButton(Widget w, IswPointer closure, IswPointer call_data)
{
  (void) DoSearch( (struct SearchAndReplace *) closure );
}

/*	Function Name: _IswTextSearch
 *	Description: Action routine that can be bound to the text widget
 *                   it will popup the search dialog box.
 *	Arguments:   w - the text widget.
 *                   event - X Event (used to get x and y location).
 *                   params, num_params - the parameter list.
 *	Returns:     none.
 *
 * NOTE:
 *
 * The parameter list contains one or two entries that may be the following.
 *
 * First Entry:   The first entry is the direction to search by default.
 *                This arguement must be specified and may have a value of
 *                "left" or "right".
 *
 * Second Entry:  This entry is optional and contains the value of the default
 *                string to search for.
 */

#define SEARCH_HEADER ("Text Widget - Search():")

void
_IswTextSearch(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget)w;
  IswTextScanDirection dir;
  char * ptr, buf[BUFSIZ];
  IswTextEditType edit_mode;

#ifdef notdef
  if (ctx->text.source->Search == NULL) {
      xcb_bell(IswDisplay(w), 0);
      return;
  }
#endif

  if ( (*num_params < 1) || (*num_params > 2) ) {
    (void) sprintf(buf, "%s %s\n%s", SEARCH_HEADER,
	    "This action must have only",
	    "one or two parameters");
    IswAppWarning(IswWidgetToApplicationContext(w), buf);
    return;
  }

  if (*num_params == 2 )
      ptr = params[1];
  else
          ptr = "";

  switch(params[0][0]) {
  case 'b':			/* Left. */
  case 'B':
    dir = IswsdLeft;
    break;
  case 'f':			/* Right. */
  case 'F':
    dir = IswsdRight;
    break;
  default:
    (void) sprintf(buf, "%s %s\n%s", SEARCH_HEADER,
	    "The first parameter must be",
	    "Either 'backward' or 'forward'");
    IswAppWarning(IswWidgetToApplicationContext(w), buf);
    return;
  }

  if (ctx->text.search== NULL) {
    ctx->text.search = IswNew(struct SearchAndReplace);
    ctx->text.search->search_popup = CreateDialog(w, ptr, "search",
						  AddSearchChildren);
    IswRealizeWidget(ctx->text.search->search_popup);
    SetWMProtocolTranslations(ctx->text.search->search_popup);
  }
  else if (*num_params > 1) {
    IswVaSetValues(ctx->text.search->search_text, IswNstring, ptr, NULL);
  }

  {
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgEditType(&ab, (IswArgVal)&edit_mode);
    IswGetValues(ctx->text.source, ab.args, ab.count);
  }

  InitializeSearchWidget(ctx->text.search, dir, (edit_mode == IswtextEdit));

  CenterWidgetOnPoint(ctx->text.search->search_popup, event);
  IswPopup(ctx->text.search->search_popup, IswGrabNone);
}

/*	Function Name: InitializeSearchWidget
 *	Description: This function initializes the search widget and
 *                   is called each time the search widget is poped up.
 *	Arguments: search - the search widget structure.
 *                 dir - direction to search.
 *                 replace_active - state of the sensitivity for the
 *                                  replace button.
 *	Returns: none.
 */

static void
InitializeSearchWidget(struct SearchAndReplace *search, IswTextScanDirection dir,
                       Boolean replace_active)
{
  SetResource(search->rep_one, IswNsensitive, (IswArgVal) replace_active);
  SetResource(search->rep_all, IswNsensitive, (IswArgVal) replace_active);
  SetResource(search->rep_label, IswNsensitive, (IswArgVal) replace_active);
  SetResource(search->rep_text, IswNsensitive, (IswArgVal) replace_active);

  switch (dir) {
  case IswsdLeft:
    SetResource(search->left_toggle, IswNstate, (IswArgVal) TRUE);
    break;
  case IswsdRight:
    SetResource(search->right_toggle, IswNstate, (IswArgVal) TRUE);
    break;
  default:
    break;
  }
}

/*	Function Name: AddSearchChildren
 *	Description: Adds all children to the Search Dialog Widget.
 *	Arguments: form - the form widget for the search widget.
 *                 ptr - a pointer to the initial string for the Text Widget.
 *                 tw - the main text widget.
 *	Returns: none.
 */

static void
AddSearchChildren(Widget form, String ptr, Widget tw)
{
  Widget cancel, search_button, s_label, s_text, r_text;
  IswTranslations trans;
  struct SearchAndReplace * search = ((TextWidget) tw)->text.search;
  IswArgBuilder ab = IswArgBuilderInit();

  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgResizable(&ab, TRUE);
  IswArgBorderWidth(&ab, 0);
  search->label1 = IswCreateManagedWidget("label1", labelWidgetClass, form,
					 ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgFromVert(&ab, search->label1);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgResizable(&ab, TRUE);
  IswArgBorderWidth(&ab, 0);
  search->label2 = IswCreateManagedWidget("label2", labelWidgetClass, form,
					 ab.args, ab.count);

/*
 * We need to add R_OFFSET to the radio_data, because the value zero (0)
 * has special meaning.
 */

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Backward");
  IswArgFromVert(&ab, search->label2);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgRadioData(&ab, (IswPointer) IswsdLeft + R_OFFSET);
  search->left_toggle = IswCreateManagedWidget("backwards", toggleWidgetClass,
					      form, ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Forward");
  IswArgFromVert(&ab, search->label2);
  IswArgFromHoriz(&ab, search->left_toggle);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgRadioGroup(&ab, search->left_toggle);
  IswArgRadioData(&ab, (IswPointer) IswsdRight + R_OFFSET);
  search->right_toggle = IswCreateManagedWidget("forwards", toggleWidgetClass,
					       form, ab.args, ab.count);

  {
    IswTranslations radio_translations;

    radio_translations = IswParseTranslationTable(radio_trans_string);
    IswOverrideTranslations(search->left_toggle, radio_translations);
    IswOverrideTranslations(search->right_toggle, radio_translations);
  }

  IswArgBuilderReset(&ab);
  IswArgFromVert(&ab, search->left_toggle);
  IswArgLabel(&ab, "Search for:  ");
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgBorderWidth(&ab, 0);
  s_label = IswCreateManagedWidget("searchLabel", labelWidgetClass, form,
				  ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgFromVert(&ab, search->left_toggle);
  IswArgFromHoriz(&ab, s_label);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainRight);
  IswArgEditType(&ab, IswtextEdit);
  IswArgResizable(&ab, TRUE);
  IswArgResize(&ab, IswtextResizeWidth);
  IswArgString(&ab, ptr);
  s_text = IswCreateManagedWidget("searchText", textWidgetClass, form,
				 ab.args, ab.count);
  search->search_text = s_text;

  IswArgBuilderReset(&ab);
  IswArgFromVert(&ab, s_text);
  IswArgLabel(&ab, "Replace with:");
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  IswArgBorderWidth(&ab, 0);
  search->rep_label = IswCreateManagedWidget("replaceLabel", labelWidgetClass,
					    form, ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgFromHoriz(&ab, s_label);
  IswArgFromVert(&ab, s_text);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainRight);
  IswArgEditType(&ab, IswtextEdit);
  IswArgResizable(&ab, TRUE);
  IswArgResize(&ab, IswtextResizeWidth);
  IswArgString(&ab, "");
  r_text = IswCreateManagedWidget("replaceText", textWidgetClass,
				 form, ab.args, ab.count);
  search->rep_text = r_text;

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Search");
  IswArgFromVert(&ab, r_text);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  search_button = IswCreateManagedWidget("search", commandWidgetClass, form,
					ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Replace");
  IswArgFromVert(&ab, r_text);
  IswArgFromHoriz(&ab, search_button);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  search->rep_one = IswCreateManagedWidget("replaceOne", commandWidgetClass,
					  form, ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Replace All");
  IswArgFromVert(&ab, r_text);
  IswArgFromHoriz(&ab, search->rep_one);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  search->rep_all = IswCreateManagedWidget("replaceAll", commandWidgetClass,
					  form, ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgLabel(&ab, "Cancel");
  IswArgFromVert(&ab, r_text);
  IswArgFromHoriz(&ab, search->rep_all);
  IswArgLeft(&ab, IswChainLeft);
  IswArgRight(&ab, IswChainLeft);
  cancel = IswCreateManagedWidget(DISMISS_NAME, commandWidgetClass, form,
				 ab.args, ab.count);

  IswAddCallback(search_button, IswNcallback, SearchButton, (IswPointer) search);
  IswAddCallback(search->rep_one, IswNcallback, DoReplaceOne, (IswPointer) search);
  IswAddCallback(search->rep_all, IswNcallback, DoReplaceAll, (IswPointer) search);
  IswAddCallback(cancel, IswNcallback, PopdownSearch, (IswPointer) search);

/*
 * Initialize the text entry fields.
 */

  {
    Pixel color;
    IswArgBuilderReset(&ab);
    IswArgBackground(&ab, (IswArgVal)&color);
    IswGetValues(search->rep_text, ab.args, ab.count);
    IswArgBuilderReset(&ab);
    IswArgBorderColor(&ab, color);
    IswSetValues(search->rep_text, ab.args, ab.count);
    IswSetKeyboardFocus(form, search->search_text);
  }

  SetSearchLabels(search, SEARCH_LABEL_1, SEARCH_LABEL_2, FALSE);

/*
 * Bind Extra translations.
 */

  trans = IswParseTranslationTable(search_text_trans);
  IswOverrideTranslations(search->search_text, trans);

  trans = IswParseTranslationTable(rep_text_trans);
  IswOverrideTranslations(search->rep_text, trans);
}

/*	Function Name: DoSearch
 *	Description: Performs a search.
 *	Arguments: search - the serach structure.
 *	Returns: TRUE if sucessful.
 */

/* ARGSUSED */
static Boolean
DoSearch(struct SearchAndReplace * search)
{
  char msg[BUFSIZ];
  Widget tw = IswParent(search->search_popup);
  ISWTextPosition pos;
  IswTextScanDirection dir;
  ISWTextBlock text;

  TextWidget ctx = (TextWidget)tw;

  text.ptr = GetStringRaw(search->search_text);
  text.format = _IswTextFormat(ctx);
      text.length = strlen(text.ptr);
  text.firstPos = 0;

  dir = (IswTextScanDirection)(intptr_t) ((IswPointer)IswToggleGetCurrent(search->left_toggle) -
				R_OFFSET);

  pos = IswTextSearch( tw, dir, &text);


   /* The Raw string in find.ptr may be WC I can't use here, so I re - call
   GetString to get a tame version. */

  if (pos == IswTextSearchError)
    (void) sprintf( msg, "Could not find string ``%s''.", GetString( search->search_text ) );
  else {
    if (dir == IswsdRight)
      IswTextSetInsertionPoint( tw, pos + text.length);
    else
      IswTextSetInsertionPoint( tw, pos);

    IswTextSetSelection( tw, pos, pos + text.length);
    search->selection_changed = FALSE; /* selection is good. */
    return(TRUE);
  }

  IswTextUnsetSelection(tw);
  SetSearchLabels(search, msg, "", TRUE);
  return(FALSE);
}

/************************************************************
 *
 * This section of the file contains all the functions that
 * the replace dialog box uses.
 *
 ************************************************************/

/*	Function Name: _IswTextDoReplaceAction
 *	Description: Action routine that can be bound to dialog box's
 *                   Text Widget that will replace a string in the main
 *                   Text Widget.
 *	Arguments:   (Standard Action Routine args)
 *	Returns:     none.
 */

/* ARGSUSED */
void
_IswTextDoReplaceAction(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  TextWidget ctx = (TextWidget) IswParent(IswParent(IswParent(w)));
  Boolean popdown = FALSE;

  if ( (*num_params == 1) &&
       ((params[0][0] == 'p') || (params[0][0] == 'P')) )
    popdown = TRUE;

  if (Replace( ctx->text.search, TRUE, popdown) && popdown)
    PopdownSearch(w, (IswPointer) ctx->text.search, (IswPointer)NULL);
}

/*	Function Name: DoReplaceOne
 *	Description:  Replaces the first instance of the string
 *                     in the search dialog's text widget
 *                    with the one in the replace dialog's text widget.
 *	Arguments: w - *** Not Used ***.
 *                 closure - a pointer to the search structure.
 *                 call_data - *** Not Used ***.
 *	Returns: none.
 */

/* ARGSUSED */
static void
DoReplaceOne(Widget w, IswPointer closure, IswPointer call_data)
{
  Replace( (struct SearchAndReplace *) closure, TRUE, FALSE);
}

/*	Function Name: DoReplaceOne
 *	Description:  Replaces every instance of the string
 *                    in the search dialog's text widget
 *                    with the one in the replace dialog's text widget.
 *	Arguments: w - *** Not Used ***.
 *                 closure - a pointer to the search structure.
 *                 call_data - *** Not Used ***.
 *	Returns: none.
 */

/* ARGSUSED */
static void
DoReplaceAll(Widget w, IswPointer closure, IswPointer call_data)
{
  Replace( (struct SearchAndReplace *) closure, FALSE, FALSE);
}

/*	Function Name: Replace
 *	Description: This is the function that does the real work of
 *                   replacing strings in the main text widget.
 *	Arguments: tw - the Text Widget to replce the string in.
 *                 once_only - If TRUE then only replace the first one found.
 *                             other replace all of them.
 *                 show_current - If true then leave the selection on the
 *                                string that was just replaced, otherwise
 *                                move it onto the next one.
 *	Returns: none.
 */

static Boolean
Replace(struct SearchAndReplace *search, Boolean once_only, Boolean show_current)
{
  ISWTextPosition pos, new_pos, end_pos;
  IswTextScanDirection dir;
  ISWTextBlock find, replace;
  Widget tw = IswParent(search->search_popup);
  int count = 0;

  TextWidget ctx = (TextWidget)tw;

  find.ptr = GetStringRaw( search->search_text);
  find.format = _IswTextFormat(ctx);
      find.length = strlen(find.ptr);
  find.firstPos = 0;

  replace.ptr = GetStringRaw(search->rep_text);
  replace.firstPos = 0;
  replace.format = _IswTextFormat(ctx);
      replace.length = strlen(replace.ptr);

  dir = (IswTextScanDirection)(intptr_t) ((IswPointer)IswToggleGetCurrent(search->left_toggle) -
				R_OFFSET);
  /* CONSTCOND */
  while (TRUE) {
    if (count != 0) {
      new_pos = IswTextSearch( tw, dir, &find);

      if (new_pos == IswTextSearchError) {
	if (count == 0) {
	  char msg[BUFSIZ];

             /* The Raw string in find.ptr may be WC I can't use here,
		so I call GetString to get a tame version.*/

	  (void) sprintf( msg, "%s %s %s", "*** Error: Could not find string ``",
		  GetString( search->search_text ), "''. ***");
	  SetSearchLabels(search, msg, "", TRUE);
	  return(FALSE);
	}
	else
	  break;
      }
      pos = new_pos;
      end_pos = pos + find.length;
    }
    else {
      IswTextGetSelectionPos(tw, &pos, &end_pos);

      if (search->selection_changed) {
	SetSearchLabels(search, "Selection has been modified, aborting.",
			"", TRUE);
	return(FALSE);
      }
      if (pos == end_pos)
	  return(FALSE);
    }

    if (IswTextReplace(tw, pos, end_pos, &replace) != IswEditDone) {
      char msg[BUFSIZ];

      (void) sprintf( msg, "'%s' with '%s'. ***", find.ptr, replace.ptr);
      SetSearchLabels(search, "*** Error while replacing", msg, TRUE);
      return(FALSE);
    }

    if (dir == IswsdRight)
      IswTextSetInsertionPoint( tw, pos + replace.length);
    else
      IswTextSetInsertionPoint( tw, pos);

    if (once_only) {
      if (show_current)
	break;
      else {
	DoSearch(search);
	return(TRUE);
      }
    }
    count++;
  }

  if (replace.length == 0)
    IswTextUnsetSelection(tw);
  else
    IswTextSetSelection( tw, pos, pos + replace.length);

  return(TRUE);
}

/*	Function Name: SetSearchLabels
 *	Description: Sets both the search labels, and also rings the bell
 *	Arguments: search - the search structure.
 *                 msg1, msg2 - message to put in each search label.
 *                 bell - if TRUE then ring bell.
 *	Returns: none.
 */

static void
SetSearchLabels(struct SearchAndReplace *search, String msg1, String msg2, Boolean bell)
{
  (void) SetResource( search->label1, IswNlabel, (IswArgVal) msg1);
  (void) SetResource( search->label2, IswNlabel, (IswArgVal) msg2);
  if (bell)
    xcb_bell(IswDisplay(search->search_popup), 0);
}

/************************************************************
 *
 * This section of the file contains utility routines used by
 * other functions in this file.
 *
 ************************************************************/


/*	Function Name: _IswTextSetField
 *	Description: Action routine that can be bound to dialog box's
 *                   Text Widget that will send input to the field specified.
 *	Arguments:   (Standard Action Routine args)
 *	Returns:     none.
 */

/* ARGSUSED */
void
_IswTextSetField(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
  struct SearchAndReplace * search;
  Widget new, old;

  search = ((TextWidget) IswParent(IswParent(IswParent(w))))->text.search;

  if (*num_params != 1) {
    SetSearchLabels(search, "*** Error: SetField Action must have",
		    "exactly one argument. ***", TRUE);
    return;
  }
  switch (params[0][0]) {
  case 's':
  case 'S':
    new = search->search_text;
    old = search->rep_text;
    break;
  case 'r':
  case 'R':
    old = search->search_text;
    new = search->rep_text;
    break;
  default:
    SetSearchLabels(search, "*** Error: SetField Action's first Argument must",
		    "be either 'Search' or 'Replace'. ***", TRUE);
    return;
  }
  _SetField(new, old);
}

/*	Function Name: SetField
 *	Description: Sets the current text field.
 *	Arguments: new, old - new and old text fields.
 *	Returns: none
 */

static void
_SetField(Widget new, Widget old)
{
  Pixel new_border, old_border, old_bg;
  IswArgBuilder ab = IswArgBuilderInit();

  if (!IswIsSensitive(new)) {
    xcb_bell(IswDisplay(old), 0);	/* Don't set field to an inactive Widget. */
    return;
  }

  IswSetKeyboardFocus(IswParent(new), new);

  IswArgBorderColor(&ab, (IswArgVal)&old_border);
  IswArgBackground(&ab, (IswArgVal)&old_bg);
  IswGetValues(new, ab.args, ab.count);

  IswArgBuilderReset(&ab);
  IswArgBorderColor(&ab, (IswArgVal)&new_border);
  IswGetValues(old, ab.args, ab.count);

  if (old_border != old_bg)	/* Colors are already correct, return. */
      return;

  SetResource(old, IswNborderColor, (IswArgVal) old_border);
  SetResource(new, IswNborderColor, (IswArgVal) new_border);
}

/*	Function Name: SetResourceByName
 *	Description: Sets a resource in any of the dialog children given
 *                   name of the child and the shell widget of the dialog.
 *	Arguments: shell - shell widget of the popup.
 *                 name - name of the child.
 *                 res_name - name of the resource.
 *                 value - the value of the resource.
 *	Returns: TRUE if sucessful.
 */

static Boolean
SetResourceByName(Widget shell, char *name, char *res_name, IswArgVal value)
{
  Widget temp_widget;
  char buf[BUFSIZ];

  (void) sprintf(buf, "%s.%s", FORM_NAME, name);

  if ( (temp_widget = IswNameToWidget(shell, buf)) != NULL) {
    SetResource(temp_widget, res_name, value);
    return(TRUE);
  }
  return(FALSE);
}

/*	Function Name: SetResource
 *	Description: Sets a resource in a widget
 *	Arguments: w - the widget.
 *                 res_name - name of the resource.
 *                 value - the value of the resource.
 *	Returns: none.
 */

static void
SetResource(Widget w, char *res_name, IswArgVal value)
{
  IswArgBuilder ab = IswArgBuilderInit();

  IswArgBuilderAdd(&ab, res_name, value);
  IswSetValues( w, ab.args, ab.count );
}

/*	Function Name: GetString{Raw}
 *	Description:   Gets the value for the string in the popup.
 *	Arguments:     text - the text widget whose string we will get.
 *
 *	GetString returns the string as a MB.
 *	GetStringRaw returns the exact buffer contents suitable for a search.
 *
 */

static String
GetString(Widget text)
{
  String string;
  IswArgBuilder ab = IswArgBuilderInit();
  IswArgString(&ab, (IswArgVal)&string);
  IswGetValues(text, ab.args, ab.count);
  return(string);
}

static String
GetStringRaw(Widget tw)
{
  TextWidget ctx = (TextWidget)tw;
  ISWTextPosition last;

  last = IswTextSourceScan(ctx->text.source, 0, IswstAll, IswsdRight,
			     ctx->text.mult, TRUE);
  return (_IswTextGetText(ctx, 0, last));
}

/*	Function Name: CenterWidgetOnPoint.
 *	Description: Centers a shell widget on a point relative to
 *                   the root window.
 *	Arguments: w - the shell widget.
 *                 event - event containing the location of the point
 *	Returns: none.
 *
 * NOTE: The widget is not allowed to go off the screen.
 */

static void
CenterWidgetOnPoint(Widget w, xcb_generic_event_t *event)
{
  Dimension width, height, b_width;
  Position x = 0, y = 0, max_x, max_y;

  if (event != NULL) {
    uint8_t type = event->response_type & ~0x80;
    switch (type) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
      {
        xcb_button_press_event_t *bev = (xcb_button_press_event_t *)event;
        x = bev->root_x;
        y = bev->root_y;
      }
      break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
      {
        xcb_key_press_event_t *kev = (xcb_key_press_event_t *)event;
        x = kev->root_x;
        y = kev->root_y;
      }
      break;
    default:
      return;
    }
  }

  {
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgWidth(&ab, (IswArgVal)&width);
    IswArgHeight(&ab, (IswArgVal)&height);
    IswArgBorderWidth(&ab, (IswArgVal)&b_width);
    IswGetValues(w, ab.args, ab.count);
  }

  width += 2 * b_width;
  height += 2 * b_width;

  x -= ( (Position) width/2 );
  if (x < 0) x = 0;
  if ( x > (max_x = (Position) (IswScreen(w)->width_in_pixels - width)) ) x = max_x;

  y -= ( (Position) height/2 );
  if (y < 0) y = 0;
  if ( y > (max_y = (Position) (IswScreen(w)->height_in_pixels - height)) ) y = max_y;

  {
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgX(&ab, x);
    IswArgY(&ab, y);
    IswSetValues(w, ab.args, ab.count);
  }
}

/*	Function Name: CreateDialog
 *	Description: Actually creates a dialog.
 *	Arguments: parent - the parent of the dialog - the main text widget.
 *                 ptr - initial_string for the dialog.
 *                 name - name of the dialog.
 *                 func - function to create the children of the dialog.
 *	Returns: the popup shell of the dialog.
 *
 * NOTE:
 *
 * The function argument is passed the following arguements.
 *
 * form - the from widget that is the dialog.
 * ptr - the initial string for the dialog's text widget.
 * parent - the parent of the dialog - the main text widget.
 */

static Widget
CreateDialog(Widget parent, String ptr, String name,
             void (*func)(Widget, String, Widget))
{
  Widget popup, form;
  IswArgBuilder ab = IswArgBuilderInit();

  IswArgIconName(&ab, name);
  IswArgGeometry(&ab, NULL);
  IswArgAllowShellResize(&ab, TRUE);
  IswArgTransientFor(&ab, GetShell(parent));
  popup = IswCreatePopupShell(name, transientShellWidgetClass,
			     parent, ab.args, ab.count);

  form = IswCreateManagedWidget(FORM_NAME, formWidgetClass, popup,
			       (ArgList)NULL, ZERO);
  IswManageChild (form);

  (*func) (form, ptr, parent);
  return(popup);
}

 /*	Function Name: GetShell
  * 	Description: Walks up the widget hierarchy to find the
  * 		nearest shell widget.
  * 	Arguments: w - the widget whose parent shell should be returned.
  * 	Returns: The shell widget among the ancestors of w that is the
  * 		fewest levels up in the widget hierarchy.
  */

static Widget
GetShell(Widget w)
{
    while ((w != NULL) && !IswIsShell(w))
	w = IswParent(w);

    return (w);
}

static Boolean
InParams(String str, String *p, Cardinal n)
{
    int i;
    for (i=0; i < n; p++, i++)
	if (! ISWCompareISOLatin1(*p, str)) return True;
    return False;
}

static char *WM_DELETE_WINDOW = "WM_DELETE_WINDOW";

static void
WMProtocols(Widget w, xcb_generic_event_t *event, String *params, Cardinal *num_params)
{
    xcb_atom_t wm_delete_window;
    xcb_atom_t wm_protocols;

    wm_delete_window = IswXcbInternAtom(IswDisplay(w), WM_DELETE_WINDOW, True);
    wm_protocols = IswXcbInternAtom(IswDisplay(w), "WM_PROTOCOLS", True);

    /* Respond to a recognized WM protocol request iff
     * event type is ClientMessage and no parameters are passed, or
     * event type is ClientMessage and event data is matched to parameters, or
     * event type isn't ClientMessage and parameters make a request.
     */
#define DO_DELETE_WINDOW InParams(WM_DELETE_WINDOW, params, *num_params)

    /* XCB event handling */
    uint8_t type = event->response_type & ~0x80;
    Boolean is_client_message = (type == XCB_CLIENT_MESSAGE);
    xcb_atom_t message_type = 0;
    xcb_atom_t data0 = 0;
    
    if (is_client_message) {
        xcb_client_message_event_t *cm = (xcb_client_message_event_t *)event;
        message_type = cm->type;
        data0 = cm->data.data32[0];
    }

    if ((is_client_message &&
	 message_type == wm_protocols &&
	 data0 == wm_delete_window &&
	 (*num_params == 0 || DO_DELETE_WINDOW))
	||
	(!is_client_message && DO_DELETE_WINDOW)) {

#undef DO_DELETE_WINDOW

	Widget cancel;
	char descendant[DISMISS_NAME_LEN + 2];
	(void) sprintf(descendant, "*%s", DISMISS_NAME);
	cancel = IswNameToWidget(w, descendant);
	if (cancel) IswCallCallbacks(cancel, IswNcallback, (IswPointer)NULL);
    }
}

static void
SetWMProtocolTranslations(Widget w)
{
    int i;
    IswAppContext app_context;
    xcb_atom_t wm_delete_window;
    static IswTranslations compiled_table;	/* initially 0 */
    static IswAppContext *app_context_list;	/* initially 0 */
    static Cardinal list_size;			/* initially 0 */

    app_context = IswWidgetToApplicationContext(w);

    /* parse translation table once */
    if (! compiled_table) compiled_table = IswParseTranslationTable
	("<Message>WM_PROTOCOLS: IswWMProtocols()\n");

    /* add actions once per application context */
    for (i=0; i < list_size && app_context_list[i] != app_context; i++) ;
    if (i == list_size) {
	IswActionsRec actions[1];
	actions[0].string = "IswWMProtocols";
	actions[0].proc = WMProtocols;
	list_size++;
	app_context_list = (IswAppContext *) IswRealloc
	    ((char *)app_context_list, list_size * sizeof(IswAppContext));
	IswAppAddActions(app_context, actions, 1);
	app_context_list[i] = app_context;
    }

    /* establish communication between the window manager and each shell */
    IswAugmentTranslations(w, compiled_table);
    wm_delete_window = IswXcbInternAtom(IswDisplay(w), WM_DELETE_WINDOW, False);
    /* XCB equivalent of XSetWMProtocols */
    xcb_icccm_set_wm_protocols(IswDisplay(w), IswWindow(w), 
                               IswXcbInternAtom(IswDisplay(w), "WM_PROTOCOLS", False),
                               1, &wm_delete_window);
}
