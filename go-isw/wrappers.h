#ifndef _GO_ISW_WRAPPERS_H
#define _GO_ISW_WRAPPERS_H

#include <ISW/Intrinsic.h>
#include <ISW/Shell.h>

static inline IswPointer _isw_handle_to_ptr(uintptr_t h) {
    return (IswPointer)h;
}

static inline Widget _isw_handle_to_widget(uintptr_t h) {
    return (Widget)h;
}

static inline void* _isw_uintptr_to_voidptr(uintptr_t v) {
    return (void*)v;
}

static inline Widget _isw_app_initialize(
    IswAppContext *ac, char *app_class,
    int *argc, char **argv,
    char **fallback, Arg *args, Cardinal num_args)
{
    return IswAppInitialize(ac, app_class, NULL, 0, argc, argv,
                            fallback, args, num_args);
}

static inline Widget _isw_open_application(
    IswAppContext *ac, char *app_class,
    int *argc, char **argv,
    char **fallback, WidgetClass wc, Arg *args, Cardinal num_args)
{
    return IswOpenApplication(ac, app_class, NULL, 0, argc, argv,
                              fallback, wc, args, num_args);
}

static inline Widget _isw_create_widget(
    char *name, WidgetClass wc, Widget parent,
    Arg *args, Cardinal n)
{
    return IswCreateWidget(name, wc, parent, args, n);
}

static inline Widget _isw_create_managed_widget(
    char *name, WidgetClass wc, Widget parent,
    Arg *args, Cardinal n)
{
    return IswCreateManagedWidget(name, wc, parent, args, n);
}

static inline Widget _isw_create_popup_shell(
    char *name, WidgetClass wc, Widget parent,
    Arg *args, Cardinal n)
{
    return IswCreatePopupShell(name, wc, parent, args, n);
}

static inline Widget _isw_app_create_shell(
    char *name, char *cls, WidgetClass wc,
    IswDisplay dpy, Arg *args, Cardinal n)
{
    return IswAppCreateShell(name, cls, wc, dpy, args, n);
}

static inline void _isw_set_values(Widget w, Arg *args, Cardinal n) {
    IswSetValues(w, args, n);
}

static inline void _isw_get_values(Widget w, Arg *args, Cardinal n) {
    IswGetValues(w, args, n);
}

static inline char** _isw_alloc_string_array(int n) {
    return (char**)calloc(n + 1, sizeof(char*));
}

static inline void _isw_string_array_set(char **arr, int idx, char *s) {
    arr[idx] = s;
}

static inline char* _isw_string_array_get(char **arr, int idx) {
    return arr[idx];
}

static inline uintptr_t _isw_charpp_to_uintptr(char **p) {
    return (uintptr_t)p;
}

static inline char** _isw_uintptr_to_charpp(uintptr_t v) {
    return (char**)v;
}

#endif
