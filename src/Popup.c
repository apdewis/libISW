/***********************************************************

Copyright 1987, 1988, 1994, 1998  The Open Group

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

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "IntrinsicI.h"
#include "EventI.h"
#include "ShellP.h"
#include <ISW/ISWRender.h>
#include <ISW/ISWPlatform.h>

void
_IswPopup(Widget widget, IswGrabKind grab_kind)
{
    register ShellWidget shell_widget = (ShellWidget) widget;

    if (!IswIsShell(widget)) {
        IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                      "invalidClass", "xtPopup", IswCIswToolkitError,
                      "IswPopup requires a subclass of shellWidgetClass",
                      NULL, NULL);
    }

    if (!shell_widget->shell.popped_up) {
        IswGrabKind call_data = grab_kind;

        IswCallCallbacks(widget, IswNpopupCallback, (IswPointer) &call_data);
        shell_widget->shell.popped_up = TRUE;
        shell_widget->shell.grab_kind = grab_kind;
        if (shell_widget->shell.create_popup_child_proc != NULL) {
            (*(shell_widget->shell.create_popup_child_proc)) (widget);
        }
        if (grab_kind == IswGrabExclusive) {
            IswAddGrab(widget, TRUE);
        }
        else if (grab_kind == IswGrabNonexclusive) {
            IswAddGrab(widget, FALSE);
        }
        IswRealizeWidget(widget);
        {
            IswWindowGeometry g;
            memset(&g, 0, sizeof(g));
            _IswPlatformMapWindow(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
            _IswPlatformConfigureWindow(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                                        &g, ISW_CONFIG_STACK,
                                        ISW_STACK_ABOVE, NULL);
            _IswPlatformFlush(IswDisplayOf(widget));
        }

        /* Synchronize with the server so the map is fully processed,
         * then force Expose on every managed child.  Without this,
         * child widgets painted before the shell was mapped (e.g. from
         * IswListChange) have their content cleared by the server's
         * background fill at map time, and the Expose that should
         * trigger a repaint doesn't always arrive. */
        {
            /* Round-trip to ensure the map has been processed */
            _IswPlatformSync(IswDisplayOf(widget));

            if (IswIsComposite(widget)) {
                CompositeWidget cw = (CompositeWidget)widget;
                Cardinal i;
                for (i = 0; i < cw->composite.num_children; i++) {
                    Widget child = cw->composite.children[i];
                    if (IswIsWidget(child) && IswIsRealized(child) &&
                        IswIsManaged(child)) {
                            _IswRepaintWindowless(child);
                    }
                }
                _IswPlatformFlush(IswDisplayOf(widget));
            }
        }

    }
    else {
        IswWindowGeometry g;
        memset(&g, 0, sizeof(g));
        _IswPlatformConfigureWindow(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)),
                                    &g, ISW_CONFIG_STACK, ISW_STACK_ABOVE, NULL);
        _IswPlatformFlush(IswDisplayOf(widget));
    }

}                               /* _IswPopup */

void
IswPopup(Widget widget, IswGrabKind grab_kind)
{
    Widget hookobj;

    switch (grab_kind) {

    case IswGrabNone:
    case IswGrabExclusive:
    case IswGrabNonexclusive:
        break;

    default:
        IswAppWarningMsg(IswWidgetToApplicationContext(widget),
                        "invalidGrabKind", "xtPopup", IswCIswToolkitError,
                        "grab kind argument has invalid value; IswGrabNone assumed",
                        NULL, NULL);
        grab_kind = IswGrabNone;
    }

    _IswPopup(widget, grab_kind);

    hookobj = IswHooksOfDisplay(IswDisplayOf(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHpopup;
        call_data.widget = widget;
        call_data.event_data = (IswPointer) (IswIntPtr) grab_kind;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
}                               /* IswPopup */

void
IswPopdown(Widget widget)
{
    /* Unmap a shell widget if it is mapped, and remove from grab list */
    Widget hookobj;
    ShellWidget shell_widget = (ShellWidget) widget;
    IswGrabKind grab_kind;

    if (!IswIsShell(widget)) {
        IswAppErrorMsg(IswWidgetToApplicationContext(widget),
                      "invalidClass", "xtPopdown", IswCIswToolkitError,
                      "IswPopdown requires a subclass of shellWidgetClass",
                      NULL, NULL);
    }

    //#TODO remove ifndef gaurd if/when shown to no longer be necessary
//#ifndef X_NO_XT_POPDOWN_CONFORMANCE
    if (!shell_widget->shell.popped_up)
        return;
//#endif

    grab_kind = shell_widget->shell.grab_kind;
    _IswPlatformUnmapWindow(IswDisplayOf(widget), _IswPlatformWidgetWindow(IswDisplayOf((Widget)(widget)), (Widget)(widget)));
    _IswPlatformFlush(IswDisplayOf(widget));
    /* The shell's window is now unmapped.  Cancel any composite queued for this
       windowed root earlier in the dispatch so it is not re-presented to the
       hidden window, which would leave the popup visible after popdown. */
    ISWRenderForgetRoot(widget);
    if (grab_kind != IswGrabNone)
        IswRemoveGrab(widget);
    shell_widget->shell.popped_up = FALSE;
    IswCallCallbacks(widget, IswNpopdownCallback, (IswPointer) &grab_kind);

    hookobj = IswHooksOfDisplay(IswDisplayOf(widget));
    if (IswHasCallbacks(hookobj, IswNchangeHook) == IswCallbackHasSome) {
        IswChangeHookDataRec call_data;

        call_data.type = IswHpopdown;
        call_data.widget = widget;
        IswCallCallbackList(hookobj,
                           ((HookObject) hookobj)->hooks.changehook_callbacks,
                           (IswPointer) &call_data);
    }
}                               /* IswPopdown */

void
IswCallbackPopdown(Widget widget _X_UNUSED,
                  IswPointer closure,
                  IswPointer call_data _X_UNUSED)
{
    register IswPopdownID id = (IswPopdownID) closure;

    IswPopdown(id->shell_widget);
    if (id->enable_widget != NULL) {
        IswSetSensitive(id->enable_widget, TRUE);
    }
}                               /* IswCallbackPopdown */
