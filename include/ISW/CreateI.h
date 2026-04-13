#ifndef _IswcreateI_h
#define _IswcreateI_h

_XFUNCPROTOBEGIN

extern Widget _IswCreateWidget(String name, WidgetClass widget_class,
			      Widget parent, ArgList args, Cardinal num_args,
			      IswTypedArgList typed_args,
			      Cardinal num_typed_args);
extern Widget _IswCreatePopupShell(String name, WidgetClass widget_class,
				  Widget parent, ArgList args,
				  Cardinal num_args, IswTypedArgList typed_args,
				  Cardinal num_typed_args);
extern Widget _IswAppCreateShell(String name, String class,
				WidgetClass widget_class, xcb_connection_t *display,
				ArgList args, Cardinal num_args,
				IswTypedArgList typed_args,
				Cardinal num_typed_args);
extern Widget _IswCreateHookObj(xcb_screen_t *screen, xcb_connection_t *dpy);

_XFUNCPROTOEND

#include <stdarg.h>

_XFUNCPROTOBEGIN

/* VarCreate.c */
extern Widget _IswVaOpenApplication(IswAppContext *app_context_return,
			_Xconst char* application_class,
			XrmOptionDescList options, Cardinal num_options,
			int *argc_in_out, _IswString *argv_in_out,
			String *fallback_resources, WidgetClass widget_class,
			va_list var_args);
extern Widget _IswVaAppInitialize(IswAppContext *app_context_return,
			_Xconst char* application_class,
			XrmOptionDescList options, Cardinal num_options,
			int *argc_in_out, _IswString *argv_in_out,
			String *fallback_resources, va_list var_args);

_XFUNCPROTOEND

#endif /* _IswcreateI_h */
