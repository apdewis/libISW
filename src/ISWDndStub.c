/*
 * ISWDndStub.c - TEMPORARY no-op drag-and-drop implementation.
 *
 * Copyright (c) 2026 ISW Project
 *
 * XDnd is temporarily disabled while the event path is generalised: the XCB DnD
 * engine (ISWPlatformDndXCB.c) reaches for native xcb_generic_event_t events,
 * which is incompatible with the neutral poll-and-translate boundary.  Until the
 * DnD engine is reworked to consume neutral IswEvents, this file provides no-op
 * stubs for the public IswDnd* API so the toolkit and widgets still build and
 * run (drag-and-drop simply does nothing).
 *
 * To re-enable: drop this file from the build, restore src/platform/X11/
 * ISWPlatformDndXCB.c, and re-wire .dnd in the XCB platform ops vtable.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ISW/Intrinsic.h>
#include <ISW/IswDragDrop.h>

void
IswDndEnable(Widget shell)
{
    (void) shell;
}

void
IswDndWidgetAcceptDrops(Widget w)
{
    (void) w;
}

void
IswDndStartDrag(Widget source_widget, IswEvent *trigger_event,
                IswDragSourceDesc *desc)
{
    (void) source_widget;
    (void) trigger_event;
    (void) desc;
}

void
IswDndSetAcceptedTypes(Widget w, Atom *types, int num_types)
{
    (void) w;
    (void) types;
    (void) num_types;
}

void
IswDndSetAcceptedActions(Widget w, IswDndAction actions)
{
    (void) w;
    (void) actions;
}

void
IswDndSetDropCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    (void) w;
    (void) proc;
    (void) closure;
}

void
IswDndSetDragMotionCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    (void) w;
    (void) proc;
    (void) closure;
}

void
IswDndSetDragLeaveCallback(Widget w, IswCallbackProc proc, IswPointer closure)
{
    (void) w;
    (void) proc;
    (void) closure;
}

Atom
IswDndInternType(Widget w, const char *mime_type)
{
    (void) w;
    (void) mime_type;
    return None;
}

Boolean
IswDndIsDragging(Widget w)
{
    (void) w;
    return False;
}
