/*

Copyright 1987, 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/*****************************************************************

(C) COPYRIGHT International Business Machines Corp. 1992,1997
    All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE IBM CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING,
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the IBM Corporation shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from the IBM
Corporation.

******************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "Intrinsic.h"
#include "IntrinsicI.h"
#include "Core.h"
#include "CoreP.h"
#include "ShellP.h"
#include "StringDefs.h"
#include "ResConfigP.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_BUFFER 512

static void _search_child(Widget, char *, char *, char *, char *, char, char *);
static void _set_and_search(Widget, char *, char *, char *, char *, char,
                            char *);
static int _locate_children(Widget, Widget **);

/*
 * NAME: _set_resource_values
 *
 * FUNCTION:
 *      This function sets the value on the widget.  It must first determine
 *      if the last part is a valid resource for that widget.  (eg.
 *      labelString is a valid resource for label but not for bulletin board)
 *      It must also add the resource to the application's resource database
 *      and then query it out using specific resource strings that it builds
 *      from the widget information.  This ensures that a customizing tool
 *      on-the-fly paradigm is followed:  an application that is
 *      instantaneously updated should look the same as one that is restarted
 *      and uses the .Xdefaults file.
 *
 * PARAMETERS:
 *      w               the widget to match
 *      resource        the resource string to be matched
 *      value           the value to be set
 *      last_part       the last resource part (e.g. *background)
 *
 * RETURN VALUES: void
 *
 * ERRORS: none
 */
static void
_set_resource_values(Widget w, char *resource, char *value, char *last_part)
{
    xcb_xrm_database_t *db = NULL;
    char *resource_name = NULL;
    char *resource_class = NULL;
    char *resource_value;
    Widget cur = w;
    char *temp;
    IswResourceList resources_return = NULL;
    Cardinal num_resources_return = 0;
    Cardinal res_index;
    Boolean found_resource = False;
    xcb_connection_t *dpy;
    xcb_xrm_database_t *tmp_db;

    if (last_part == NULL)
        return;

    if (!IswIsWidget(w)) {
        if (w == 0 || w->core.parent == 0)
            return;
        dpy = IswDisplay(w->core.parent);
    }
    else {
        dpy = IswDisplay(w);
    }
    tmp_db = IswDatabase(dpy);

    /*
     * get a list of all the valid resources for this widget
     */
    IswGetResourceList(w->core.widget_class,
                      &resources_return, &num_resources_return);

    /*
     * try to match the last_part of the resource string with
     * a resource in this resource list
     */
    for (res_index = 0; res_index < num_resources_return; res_index++) {
        if ((strcmp(last_part,
                    resources_return[res_index].resource_name) == 0) ||
            (strcmp(last_part,
                    resources_return[res_index].resource_class) == 0)) {
            found_resource = True;
            break;
        }
    }

    /*
     * if resource is not a valid resource for this widget
     * or the resource name or class are NULL
     * then exit this function
     */
    if (!found_resource
        || !resources_return[res_index].resource_name
        || !resources_return[res_index].resource_class) {
        IswFree((char *) resources_return);
        return;
    }

    /*
     * build the full resource name and class specifications so
     * that you can query the resource database
     *      eg: .app.button1.foreground
     *          .App.XmPushButton.Foreground
     */
    while (cur != NULL) {
        /*
         * create resource name string
         */
        if (resource_name) {
            IswAsprintf(&temp, ".%s%s", cur->core.name, resource_name);
            IswFree(resource_name);
        }
        else if (!IswIsWidget(cur) || !cur->core.name) {
            cur = IswParent(cur);
            continue;
        }
        else {
            IswAsprintf(&temp, ".%s", cur->core.name);
        }
        resource_name = temp;

        /*
         * create resource class string
         */
        if ((IswIsTopLevelShell(cur)) && (IswParent(cur) == NULL)) {
            ApplicationShellWidget top = (ApplicationShellWidget) (cur);

            if (resource_class) {
                IswAsprintf(&temp, ".%s%s",
                           top->application.class, resource_class);
            }
            else {
                IswAsprintf(&temp, ".%s", top->application.class);
            }
        }
        else {
            if (resource_class) {
                IswAsprintf(&temp, ".%s%s",
                           cur->core.widget_class->core_class.class_name,
                           resource_class);
            }
            else {
                IswAsprintf(&temp, ".%s",
                           cur->core.widget_class->core_class.class_name);
            }
        }

        IswFree(resource_class);
        resource_class = temp;

        cur = IswParent(cur);
    }

    /*
     * add the resource name to the end of the resource name string
     */
    IswAsprintf(&temp, "%s.%s", resource_name,
               resources_return[res_index].resource_name);

    IswFree(resource_name);
    resource_name = temp;

    /*
     * add the resource class to the end of the resource class string
     */
    IswAsprintf(&temp, "%s.%s", resource_class,
               resources_return[res_index].resource_class);

    IswFree(resource_class);
    resource_class = temp;

#ifdef DEBUG
    fprintf(stderr, "resource_name = %s\n", resource_name);
    fprintf(stderr, "resource_class = %s\n", resource_class);
#endif

    /*
     * put the resource and its value in a resource database and
     * then query it back out again using the specific name and
     * class resource strings that were built above.  This is
     * necessary to maintain a precedence similar to the .Xdefaults
     * file
     */
    db = xcb_xrm_database_from_string("");
    if (db != NULL) {
        char *queried_value = NULL;

        xcb_xrm_database_put_resource(&db, resource, value);
        if (tmp_db != NULL) {
            xcb_xrm_database_combine(db, &tmp_db, False);
            xcb_xrm_database_free(db);
            db = NULL;
        }
        if (xcb_xrm_resource_get_string(tmp_db, resource_name,
                                         resource_class,
                                         &queried_value) >= 0
            && queried_value != NULL) {
            resource_value = IswNewString(queried_value);
            free(queried_value);
        } else {
            resource_value = IswNewString(value);
        }
    } else {
        resource_value = IswNewString(value);
    }

#ifdef DEBUG
    fprintf(stderr,
            "Apply:\n\twidget = %s\n\tlast_part = %s\n\tvalue = %s\n",
            (w->core.name == NULL) ? "NULL" : w->core.name,
            resources_return[res_index].resource_name, resource_value);
#endif
    /*
     * use IswVaSetValues with IswVaTypedArg to convert the value of
     * type String the the same type as the resource (last_part).
     * Then set the value.
     */
    IswVaSetValues(w,
                  IswVaTypedArg, resources_return[res_index].resource_name,
                  IswRString, resource_value, strlen(resource_value) + 1, NULL);

    IswFree((char *) resources_return);
    IswFree(resource_name);
    IswFree(resource_class);
    IswFree(resource_value);
}

/*
 * NAME: _apply_values_to_children
 *
 * FUNCTION:
 *      Once the resource string matches the value must be applied to
 *      all children if applicable. (eg. App*Form.background must apply
 *      background to all children of the Form widget)
 *
 * PARAMETERS:
 *      w               the widget to match
 *      remainder       the part of the resource string left over
 *      resource        the resource string to be matched
 *      value           the value to be set
 *      last_token      the last * or . before the final resource part
 *      last_part       the last resource part (e.g. *background)
 *
 * RETURN VALUES: void
 *
 * ERRORS: none
 */
static void
_apply_values_to_children(Widget w,
                          char *remainder,
                          char *resource,
                          char *value,
                          char last_token,
                          char *last_part)
{
    int i;
    int num_children;
    Widget *children;

    /*
     * Recursively search through the children
     */
    num_children = _locate_children(w, &children);

    for (i = 0; i < num_children; i++) {

#ifdef DEBUG
        if (IswIsWidget(children[i]) && IswIsWidget(w))
            fprintf(stderr, "searching child %s of parent %s\n",
                    children[i]->core.name, w->core.name);
        else
            fprintf(stderr, "searching child (NULL) of parent %s\n",
                    w->core.name);
        if (!IswIsWidget(children[i]))
            fprintf(stderr, "children[%d] is NOT a widget\n", i);
        if (!IswIsWidget(w))
            fprintf(stderr, "w is NOT a widget\n");
#endif

        _set_resource_values(children[i], resource, value, last_part);
        _apply_values_to_children(children[i], remainder,
                                  resource, value, last_token, last_part);
    }

    IswFree((char *) children);
}

/*
 * NAME: _search_child
 *
 * FUNCTION:
 *      descends through each child of the tree
 *
 * PARAMETERS:
 *      w               the widget whose children are to be searched
 *      indx            index into the resource string
 *      remainder       the remaining part of the resource string
 *      resource        the resource string to be matched
 *      value           the value to be applied
 *      last_token      the last * or . before the final resource part
 *      last_part       the last resource part (e.g. *background)
 *
 * RETURN VALUES: none
 *
 * ERRORS: none
 */
static void
_search_child(Widget w,
              char *indx,
              char *remainder,
              char *resource,
              char *value,
              char last_token,
              char *last_part)
{
    int i;
    int num_children;
    Widget *children;

    /*
     * Recursively search through the children
     */
    num_children = _locate_children(w, &children);
    for (i = 0; i < num_children; i++) {
        _set_and_search(children[i], indx, remainder, resource,
                        value, last_token, last_part);
    }

    IswFree((char *) children);
}

/*
 * NAME: _get_part
 *
 * FUNCTION:
 *      This routine will return the token and following part of the resource
 *      when given the current index it will update the index accordingly
 *
 * PARAMETERS:
 *      remainder       the part of the resource string left over
 *      indx            the index into the resource string
 *      part            the parsed off part of the resource string
 *
 * RETURN VALUES:
 *      char            the token (* or . or ?) preceding the resource part
 *      indx            the index into the resource string
 *      part            the parsed off part of the resource string
 *
 * ERRORS: none
 */
static char
_get_part(char *remainder _X_UNUSED, char **indx, char **part)
{
    char buffer[MAX_BUFFER];
    char *buf_ptr;
    char token = **indx;
    int i = 0;

    /*
     * copy the remainder part into the buffer
     */
    buf_ptr = buffer;
    (*indx)++;                  /* get rid of the token         */
    while (**indx && (**indx != '.') && (**indx != '*')) {
        *buf_ptr++ = *(*indx)++;
        if (++i >= MAX_BUFFER - 1)
            break;
    }
    *buf_ptr = '\0';

    *part = IswNewString(buffer);        /* return a new string to part  */

    if (strcmp(*indx, "") == 0)
        *indx = NULL;

    return (token);             /* return the token             */
}

/*
 * NAME: _match_resource_to_widget
 *
 * FUNCTION:
 *      This function matches the resource part to the widget name or class
 *
 * PARAMETERS:
 *      w               the widget to match
 *      part            the parsed off part of the resource string
 *
 * RETURN VALUES:
 *      Boolean         true if a match occurs
 *
 * ERRORS: none
 */
static Boolean
_match_resource_to_widget(Widget w, char *part)
{
    /*
     * Match any widget at this level if the ? is used
     */
    if (strcmp(part, "?") == 0)
        return (True);

    /*
     * if the object is really a widget then its name can be matched
     * otherwise only use its class.  Note that if you try to reference
     * a widget name when the object is not a widget, you may get a
     * core dump from an invalid pointer reference.
     */
    if (IswIsWidget(w)) {
        if ((strcmp(w->core.name, part) == 0) ||
            (strcmp(w->core.widget_class->core_class.class_name, part) == 0))
            return (True);
        else
            return (False);
    }
    else {
        if ((strcmp(w->core.widget_class->core_class.class_name, part) == 0))
            return (True);
        else
            return (False);
    }
}

/*
 * NAME: _set_and_search
 *
 * FUNCTION:
 *      The algorithm to search the widget tree and apply a resource string
 *
 * PARAMETERS:
 *      w               the widget to match
 *      indx            the index into the resource string
 *      remainder       the part of the resource string left over
 *      resource        the resource string to be matched
 *      value           the value to be set
 *      last_token      the last * or . before the final resource part
 *      last_part       the last resource part (e.g. *background)
 *
 * RETURN VALUES: none
 *
 * ERRORS: none
 *
 * ALGORITHM:
 * loop (look at all children)
 *      if (resource segment and current widget match)
 *              if '.'
 *                      if at end of resource string
 *                              set values (    .=over all children
 *                                              *=this widget only)
 *                      else
 *                              descend the widget tree
 *                              and parse off resource segment
 *                      exit the loop
 *              if '*'
 *                      if at end of resource string
 *                              set values (    .=over all children
 *                                              *=this widget only)
 *                      descend and parse
 *      else
 *              if '.'
 *                      continue looping
 *              if '*'
 *                      descend but don't parse
 *                      continue looping
 * end loop
 *
 * NOTE:  the _set_resource_values routine will not allow a value to be
 *      set on a resource against the rules of the resource database manager
 */
static void
_set_and_search(Widget w,
                char *indx,
                char *remainder,
                char *resource,
                char *value,
                char last_token,
                char *last_part)
{
    char *part;
    char *local_index = indx;
    char token;

    /*
     * parse off one part, return token and the new index
     */
    token = _get_part(remainder, &local_index, &part);

    if (_match_resource_to_widget(w, part)) {
        if (token == '.') {
            if (local_index == NULL) {
                if (last_token == '.') {
                    _set_resource_values(w, resource, value, last_part);
                }
                else if (last_token == '*') {
                    _set_resource_values(w, resource, value, last_part);
                    _apply_values_to_children(w,
                                              remainder, resource, value,
                                              last_token, last_part);
                }
            }
            else
                _search_child(w, local_index, remainder,
                              resource, value, last_token, last_part);
            IswFree(part);
            return;
        }
        if (token == '*') {
            if (local_index == NULL) {
                if (last_token == '.') {
                    _set_resource_values(w, resource, value, last_part);
                }
                else if (last_token == '*') {
                    _set_resource_values(w, resource, value, last_part);
                    _apply_values_to_children(w,
                                              remainder, resource, value,
                                              last_token, last_part);
                }
            }
            else
                _search_child(w, local_index, remainder,
                              resource, value, last_token, last_part);
        }
    }
    else {                      /* if the widget name and class don't match the part */
        /* if (token == '.') just continue looping */

        if (token == '*') {
            _search_child(w, indx, remainder, resource, value,
                          last_token, last_part);
        }
    }

    IswFree(part);
}

/*
 * NAME: _get_last_part
 *
 * FUNCTION:
 *      This routine will parse off the last segment of a resource string
 *      and its token and return them.  the remainder of resource is also
 *      returned.  strcoll is used to guarantee no problems with
 *      international strings.
 *
 * PARAMETERS:
 *      remainder       the part of the resource string left over
 *      part            the parsed off part of the resource string
 *
 * RETURN VALUES:
 *      char            the token (* or . or ?) preceding the resource part
 *      remainder       the part of the resource string left over
 *      part            the parsed off part of the resource string
 *
 * ERRORS: none
 */
static char
_get_last_part(char *remainder, char **part)
{
    char *loose, *tight;

    loose = strrchr(remainder, '*');
    tight = strrchr(remainder, '.');

    if ((loose == NULL) && (tight == NULL)) {
        *part = IswNewString(remainder);
        return ('.');
    }
    if ((loose == NULL) || (tight && (strcoll(loose, tight) < 0))) {
        *tight++ = '\0';        /* shorten the remainder string */
        *part = IswNewString(tight);
        return ('.');
    }
    if ((tight == NULL) || (strcoll(tight, loose) < 0)) {
        *loose++ = '\0';
        *part = IswNewString(loose);
        return ('*');
    }
    *part = NULL;

    return ('0');               /* error - return 0 */
}

/*
 * NAME: _search_widget_tree
 *
 * FUNCTION:
 *      This function tries to match a resource string to the widgets
 *      it applies to.  The functions it invokes to do this then set
 *      the value for that resource to each widget.
 *
 *      The resource string has to be parsed into the following format:
 *              resource = App*Form*button1.background
 *              remainder = *Form*button1
 *              last_part = background          last_token = .
 *      As the widget tree is recursively descended, these variables are
 *      passed.  The remainder is parsed at each level in the widget
 *      tree as the _set_and_search function attempts to match
 *      the resource part (eg. part = Form  token = *) to a widget.  When
 *      the entire resource string has been matched, the _set_resource_values
 *      functions is called to apply the value to the widget or widgets.
 *
 * PARAMETERS:
 *      w               a widget from whose toplevel shell ancestor
 *                      the search will start
 *      resource        the resource string to match
 *      value           the value to apply
 *
 * RETURN VALUES: none
 *
 * ERRORS: none
 */
static void
_search_widget_tree(Widget w, char *resource, char *value)
{
    Widget parent = w;
    char *last_part;
    char *remainder = NULL;
    char *loose, *tight;
    int loose_len, tight_len;

    if (resource == NULL)
        return;

    /*
     * Find the root of the tree given any widget
     */
    while (IswParent(parent) != NULL) {
        parent = IswParent(parent);
    }
#ifdef DEBUG
    if (IswIsWidget(w) && IswIsWidget(parent))
        fprintf(stderr, "widget = %s parent = %s\n",
                w->core.name, parent->core.name);
    else
        fprintf(stderr, "widget = NULL parent = NULL\n");
#endif

    /*
     * parse off the Class name that was prepended to this string in
     * a customizing tool
     */
    loose = strchr(resource, '*');
    tight = strchr(resource, '.');
    if ((loose == NULL) && (tight == NULL))
        return;

    loose_len = (loose) ? (int) strlen(loose) : 0;
    tight_len = (tight) ? (int) strlen(tight) : 0;

    if ((loose == NULL) || (tight_len > loose_len))
        remainder = IswNewString(tight);
    else if ((tight == NULL) || (loose_len > tight_len))
        remainder = IswNewString(loose);

    /*
     * Parse last segment off of resource string, (eg. background, font,
     * etc.)
     */
    if (remainder) {
        char last_token;

        last_token = _get_last_part(remainder, &last_part);
        /*
         * this case covers resources of only one level (eg. *background)
         */
        if (remainder[0] == 0) {
            _set_resource_values(w, resource, value, last_part);
            if (last_token == '*')
                _apply_values_to_children(parent, remainder, resource,
                                          value, last_token, last_part);
            /*
             * all other resource strings are recursively applied to the widget tree.
             * Prepend a '.' to the remainder string if there is no leading token.
             */
        }
        else {
            char *indx, *copy;

            if (remainder[0] != '*' && remainder[0] != '.') {
                IswAsprintf(&copy, ".%s", remainder);
                IswFree(remainder);
                remainder = copy;
            }
            indx = remainder;
            _set_and_search(parent, indx, remainder, resource, value,
                            last_token, last_part);
        }

        IswFree(remainder);
        IswFree(last_part);
    }
}

/*
 * NAME: _locate_children
 *
 * FUNCTION:
 *      returns a list of all of a widget's children
 *
 * PARAMETERS:
 *      w               the parent to search for its children
 *      children        the list of children that is created
 *      normal          flag for normal children
 *      popup           flag for popup children
 *
 * RETURN VALUES:
 *      int             the number of children
 *      children        the list of children found
 *
 * ERRORS: none
 */
static int
_locate_children(Widget parent, Widget **children)
{
    CompositeWidget comp = (CompositeWidget) parent;
    Cardinal i;
    int num_children = 0;
    int current = 0;

    /*
     * count the number of children
     */
    if (IswIsWidget(parent))
        num_children =
            (int) ((Cardinal) num_children + parent->core.num_popups);
    if (IswIsComposite(parent))
        num_children =
            (int) ((Cardinal) num_children + comp->composite.num_children);
    if (num_children == 0) {
        *children = NULL;
        return (0);
    }

    *children = IswMallocArray((Cardinal)num_children, (Cardinal)sizeof(Widget));

    if (IswIsComposite(parent)) {
        for (i = 0; i < comp->composite.num_children; i++) {
            (*children)[current] = comp->composite.children[i];
            current++;
        }
    }

    if (IswIsWidget(parent)) {
        for (i = 0; i < parent->core.num_popups; i++) {
            (*children)[current] = comp->core.popup_list[i];
            current++;
        }
    }

    return (num_children);
}

#ifdef DEBUG
/*
 * NAME: dump_widget_tree
 *
 * FUNCTION:
 *      recursively printout entire widget tree
 *
 * PARAMETERS:
 *      w               the widget to match
 *      indent          the amount to indent each line
 *
 * RETURN VALUES: void
 *
 * ERRORS: none
 */
static void
dump_widget_tree(Widget w, int indent)
{
    int i, j;
    int num_children;
    Widget *children;

    /*
     * Recursively search through the children
     */
    num_children = _locate_children(w, &children);
    indent += 2;
    for (i = 0; i < num_children; i++) {
        if (children[i] != NULL) {
            for (j = 0; j < indent; j++)
                fprintf(stderr, " ");
            if (IswIsWidget(children[i])) {
                fprintf(stderr, "(%s)\t", children[i]->core.name);
                fprintf(stderr, "(%s)\n",
                        children[i]->core.widget_class->core_class.class_name);
            }
            else {
                fprintf(stderr, "(NULL)\t");
                fprintf(stderr, "(%s)\n",
                        children[i]->core.widget_class->core_class.class_name);
            }
        }
        dump_widget_tree(children[i], indent);
    }

    IswFree((char *) children);
}
#endif

/*
 * NAME: _IswResourceConfiguationEH
 *
 * FUNCTION:
 *      This function is the event handler for the on-the-fly communication
 *      with a resource customization tool.  This event handler must be
 *      registered for the toplevel shell of each app.  This is best done
 *      in the _IswCreatePopupShell and _IswAppCreateShell functions in Xt's
 *      Create.c source file.
 *
 *      The property used to communicate with a customizing tool is
 *      placed on the toplevel shell window of the application.  The
 *      customizing tool places a property on this window which causes
 *      this event handler to be invoked via the PropertyNotify event.
 *      This event handler reads the property and then deletes it from
 *      the server.  The contents of the property are a resource string
 *      and value.  The event handler then calls functions to walk the
 *      applications widget tree, determining which widgets are affected
 *      by the resource string, and then applying the value with IswSetValues.
 *
 * PARAMETERS:
 *      w               the widget that invoked this event handler
 *      client_data     not used
 *      event           the event structure
 *
 * RETURN VALUES: none
 *
 * ERRORS: none
 */
void
_IswResourceConfigurationEH(Widget w,
                           IswPointer client_data _X_UNUSED,
                           IswEvent *iswev,
                           Boolean *continue_to_dispatch _X_UNUSED)
{
    ISW_NATIVE_EVENT(iswev);   /* PropertyNotify: X11 protocol, backend-internal */
    xcb_connection_t *dpy;
    IswPerDisplay pd;
    xcb_property_notify_event_t *pe = (xcb_property_notify_event_t *) event;

#ifdef DEBUG
    int indent = 0;

    fprintf(stderr, "in _IswResourceConfigurationEH atom = %u\n",
            (unsigned) pe->atom);
    fprintf(stderr, "    window = %x\n", (unsigned) IswWindow(w));
    if (IswIsWidget(w))
        fprintf(stderr, "    widget = %zx   name = %s\n", (size_t) w,
                w->core.name);
#endif

    dpy = IswDisplay(w);
    pd = _IswGetPerDisplay(dpy);

    /*
     * A customizing tool sends a "ping" to the application on
     * the RCM_INIT property.  The application answers the ping
     * by deleting the property.
     */
    if (pe->atom == pd->rcm_init) {
        xcb_delete_property(dpy, IswWindow(w), pd->rcm_init);
        xcb_flush(dpy);

#ifdef DEBUG
        if (IswIsWidget(w))
            fprintf(stderr, "%s\n", w->core.name);
        else
            fprintf(stderr, "NULL name\n");
        dump_widget_tree(w, indent);
        fprintf(stderr, "answer ping\n");
#endif
    }

    /*
     * Ignore any property notify events that are not RCM_INIT or RCM_DATA
     */
    if (pe->atom != pd->rcm_data)
        return;

    /*
     * Retrieve the data from the property and delete it
     */
#ifdef DEBUG
    fprintf(stderr, "receiving RCM_DATA property\n");
#endif
    {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(dpy, True, /* delete after read */
                             IswWindow(w), pd->rcm_data,
                             XCB_ATOM_STRING, 0, 8192);
        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(dpy, cookie, NULL);

        if (reply && reply->type == XCB_ATOM_STRING && reply->format == 8) {
            char *data = (char *) xcb_get_property_value(reply);
            int nitems = xcb_get_property_value_length(reply);

            /*
             *  data format is:
             *
             *      resource_length, resource, value
             *
             *  convert the resource_length to a long, skip over it, put a
             *  zero byte at the end of the resource, and pick off the
             *  resource and value fields.
             */
            if (data && nitems > 0) {
                char *data_end = data + nitems;
                char *data_ptr;
                char *data_value;
                unsigned long resource_len;

                resource_len = strtoul(data, &data_ptr, 10);

                if (data_ptr != data) {
                    data_ptr++;
                    data_value = data_ptr + resource_len;
                }
                else
                    data_ptr = data_value = NULL;

                if (data_value && data_value > data_ptr
                    && data_value < data_end) {
                    char *resource;
                    char *value;

                    *data_value++ = '\0';

                    resource = IswNewString(data_ptr);
                    value = IswNewString(data_value);
#ifdef DEBUG
                    fprintf(stderr, "resource_len=%lu\n", resource_len);
                    fprintf(stderr, "resource = %s\t value = %s\n",
                            resource, value);
#endif
                    _search_widget_tree(w, resource, value);

                    IswFree(resource);
                    IswFree(value);
                }
            }
        }

        free(reply);
    }
}
