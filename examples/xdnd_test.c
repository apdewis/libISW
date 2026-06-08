/*
 * xdnd_test.c - Focused XDND (drag-and-drop) test app for ISW.
 *
 * Reproduces a real drag-and-drop workflow so the windowless-conversion
 * regression (drag source failing to find drop targets) can be verified:
 *
 *   - An IconView (a windowless widget) acts as the DRAG SOURCE.  A button
 *     press on an item starts a drag carrying that item's label as
 *     text/plain and a synthetic file:// URI as text/uri-list.  The drag
 *     icon is auto-generated from the item raster, exactly as a file
 *     manager would do it.
 *
 *   - A Label acts as an in-app DROP TARGET, accepting text/plain and
 *     text/uri-list.  Its drop/enter/leave callbacks print confirmation.
 *
 * The IconView item can be dragged onto the in-app target OR onto any
 * external XDND v5 application (file manager, text editor, terminal).
 * Every protocol milestone is printed to stdout so a non-interactive
 * run can be checked against the log:
 *
 *   DRAG START   - source armed the drag for an item
 *   DROP TARGET  - in-app target received a drop (the fixed path)
 *   DRAG FINISH  - source learned the result (accepted/rejected)
 *
 * Build:  cmake --build build --target xdnd_test
 * Run:    cmake --build build --target run_xdnd_test
 */

#include <ISW/Intrinsic.h>
#include <ISW/StringDefs.h>
#include <ISW/Shell.h>
#include <ISW/Form.h>
#include <ISW/Label.h>
#include <ISW/Viewport.h>
#include <ISW/IconView.h>
#include <ISW/IswDragDrop.h>
#include <ISW/IswArgMacros.h>

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Dummy icon set — representative of a small file-manager folder.    */
/* ------------------------------------------------------------------ */

static String item_labels[] = {
    "report.txt", "photo.png", "music.ogg", "archive.zip",
    "notes.md",   "script.sh", "data.csv",  "readme"
};

static String item_icons[] = {
    /* text document */
    "<svg viewBox='0 0 16 16'><rect x='3' y='1' width='10' height='14' fill='none' stroke='black' stroke-width='1.5'/><line x1='5' y1='5' x2='11' y2='5' stroke='black'/><line x1='5' y1='8' x2='11' y2='8' stroke='black'/><line x1='5' y1='11' x2='9' y2='11' stroke='black'/></svg>",
    /* image */
    "<svg viewBox='0 0 16 16'><rect x='2' y='3' width='12' height='10' fill='none' stroke='black' stroke-width='1.2'/><circle cx='6' cy='7' r='1.5' fill='black'/><path d='M3 12 L7 8 L10 11 L12 9 L13 12' fill='none' stroke='black'/></svg>",
    /* audio */
    "<svg viewBox='0 0 16 16'><circle cx='5' cy='12' r='2' fill='none' stroke='black' stroke-width='1.2'/><line x1='7' y1='12' x2='7' y2='3' stroke='black' stroke-width='1.2'/><path d='M7 3 L12 5 L12 7 L7 5' fill='none' stroke='black'/></svg>",
    /* archive */
    "<svg viewBox='0 0 16 16'><rect x='3' y='2' width='10' height='12' fill='none' stroke='black' stroke-width='1.2'/><line x1='8' y1='2' x2='8' y2='14' stroke='black' stroke-dasharray='1 1'/></svg>",
    /* markdown */
    "<svg viewBox='0 0 16 16'><rect x='2' y='4' width='12' height='8' fill='none' stroke='black' stroke-width='1.2'/><path d='M4 10 L4 6 L6 8 L8 6 L8 10' fill='none' stroke='black'/><path d='M11 6 L11 10 M9.5 8.5 L11 10 L12.5 8.5' fill='none' stroke='black'/></svg>",
    /* shell script */
    "<svg viewBox='0 0 16 16'><rect x='2' y='2' width='12' height='12' fill='none' stroke='black' stroke-width='1.2'/><path d='M4 6 L6 8 L4 10' fill='none' stroke='black'/><line x1='7' y1='10' x2='11' y2='10' stroke='black'/></svg>",
    /* spreadsheet/csv */
    "<svg viewBox='0 0 16 16'><rect x='2' y='3' width='12' height='10' fill='none' stroke='black' stroke-width='1.2'/><line x1='2' y1='6' x2='14' y2='6' stroke='black'/><line x1='6' y1='3' x2='6' y2='13' stroke='black'/><line x1='10' y1='3' x2='10' y2='13' stroke='black'/></svg>",
    /* generic */
    "<svg viewBox='0 0 16 16'><path d='M3 2 L9 2 L13 6 L13 14 L3 14 Z' fill='none' stroke='black' stroke-width='1.2'/><path d='M9 2 L9 6 L13 6' fill='none' stroke='black'/></svg>",
};

#define NUM_ITEMS ((int)(sizeof(item_labels) / sizeof(item_labels[0])))

/* Index of the item the user pressed on, captured at drag start and used
 * by the convert proc to provide that item's data. */
static int dragged_index = -1;

/* ------------------------------------------------------------------ */
/* Drop target (in-app) — prints confirmation of received drops.      */
/* ------------------------------------------------------------------ */

static void
drop_callback(Widget w, IswPointer client_data, IswPointer call_data)
{
    IswDropCallbackData *d = (IswDropCallbackData *) call_data;
    (void) client_data;

    printf("DROP TARGET  : received drop at widget (%d,%d), action=%d, format=%d\n",
           d->x, d->y, (int) d->action, d->data_format);

    if (d->num_uris > 0) {
        printf("DROP TARGET  : %d URI(s):\n", d->num_uris);
        for (int i = 0; i < d->num_uris; i++)
            printf("DROP TARGET  :   [%d] %s\n", i, d->uris[i]);
    } else if (d->data && d->data_length > 0) {
        printf("DROP TARGET  : %lu bytes of data: \"%.*s\"\n",
               d->data_length, (int) d->data_length, (char *) d->data);
    } else {
        printf("DROP TARGET  : (no data)\n");
    }

    /* Reflect the dropped name in the label so the result is visible. */
    char text[256];
    if (d->num_uris > 0)
        snprintf(text, sizeof(text), "Dropped: %s", d->uris[0]);
    else if (d->data && d->data_length > 0)
        snprintf(text, sizeof(text), "Dropped: %.*s",
                 (int) d->data_length, (char *) d->data);
    else
        snprintf(text, sizeof(text), "Dropped (no data)");

    IswArgBuilder ab = IswArgBuilderInit();
    IswArgLabel(&ab, text);
    IswSetValues(w, ab.args, ab.count);

    fflush(stdout);
}

static void
drag_motion_callback(Widget w, IswPointer client_data, IswPointer call_data)
{
    IswDragOverCallbackData *d = (IswDragOverCallbackData *) call_data;
    (void) client_data;
    (void) d;
    /* Highlight while a drag hovers the target. */
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgBorderWidth(&ab, 3);
    IswSetValues(w, ab.args, ab.count);
}

static void
drag_leave_callback(Widget w, IswPointer client_data, IswPointer call_data)
{
    (void) client_data;
    (void) call_data;
    printf("DROP TARGET  : drag left target\n");
    fflush(stdout);
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgBorderWidth(&ab, 1);
    IswSetValues(w, ab.args, ab.count);
}

/* ------------------------------------------------------------------ */
/* Drag source (IconView) — provides the dragged item's data.         */
/* ------------------------------------------------------------------ */

static Boolean
drag_convert(Widget widget, Atom target_type,
             IswPointer *data_return, unsigned long *length_return,
             int *format_return, IswPointer client_data)
{
    (void) client_data;

    const char *name =
        (dragged_index >= 0 && dragged_index < NUM_ITEMS)
            ? item_labels[dragged_index] : "unknown";

    Atom text_plain = IswDndInternType(widget, "text/plain");
    Atom text_uri   = IswDndInternType(widget, "text/uri-list");

    if (target_type == text_plain) {
        int len = (int) strlen(name);
        char *copy = IswMalloc(len + 1);
        memcpy(copy, name, len + 1);
        *data_return = copy;
        *length_return = len;
        *format_return = 8;
        printf("DRAG CONVERT : provided text/plain \"%s\"\n", name);
        fflush(stdout);
        return True;
    }

    if (target_type == text_uri) {
        char uri[512];
        int len = snprintf(uri, sizeof(uri),
                           "file:///tmp/isw-xdnd-test/%s\r\n", name);
        char *copy = IswMalloc(len + 1);
        memcpy(copy, uri, len + 1);
        *data_return = copy;
        *length_return = len;
        *format_return = 8;
        printf("DRAG CONVERT : provided text/uri-list \"%s\"\n", uri);
        fflush(stdout);
        return True;
    }

    return False;
}

static void
drag_finished(Widget widget, IswDndAction performed_action,
              Boolean accepted, IswPointer client_data)
{
    (void) widget;
    (void) client_data;
    printf("DRAG FINISH  : %s (action=%d)\n",
           accepted ? "ACCEPTED" : "rejected", (int) performed_action);
    fflush(stdout);
}

static void
drag_start_action(Widget w, IswEvent *event,
                  String *params, Cardinal *num_params)
{
    (void) params;
    (void) num_params;

    if (event == NULL)
        return;

    /* Bound to <Btn1Motion>: fires while a button is held and the pointer
       moves.  If the press started a rubber band (press on empty space),
       leave it to BandDrag — don't arm a drag. */
    if (IswIconViewBandActive(w))
        return;

    /* Hit-test needs the native event's physical pointer coords (the same
       reason the DnD backend reads the native trigger).  Recover it via the
       IswEventNative() bridge; the motion event shares the core pointer-event
       layout with a button press. */
    xcb_button_press_event_t *be =
        (xcb_button_press_event_t *) IswEventNative(event);
    if (be == NULL)
        return;
    dragged_index = IswIconViewHitTest(w, be->event_x, be->event_y);
    if (dragged_index < 0)
        return;   /* pointer not over an item */

    printf("DRAG START   : item %d \"%s\"\n",
           dragged_index, item_labels[dragged_index]);
    fflush(stdout);

    Atom types[2];
    types[0] = IswDndInternType(w, "text/plain");
    types[1] = IswDndInternType(w, "text/uri-list");

    IswDragSourceDesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.types = types;
    desc.num_types = 2;
    desc.actions = ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE;
    desc.convert = drag_convert;
    desc.finished = drag_finished;
    desc.client_data = NULL;
    /* icon_pixmap left 0: IconView auto-generates the drag icon from the
       pressed item's raster — the representative file-manager behaviour. */

    IswDndStartDrag(w, event, &desc);
}

/* ------------------------------------------------------------------ */
/* Selection callback — reports clicks for context.                   */
/* ------------------------------------------------------------------ */

static void
iconview_select_callback(Widget w, IswPointer client_data, IswPointer call_data)
{
    IswIconViewCallbackData *d = (IswIconViewCallbackData *) call_data;
    (void) w;
    (void) client_data;
    printf("ICONVIEW     : clicked %d \"%s\"\n",
           d->index, d->label ? d->label : "");
    fflush(stdout);
}

/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
    IswAppContext app;
    Widget toplevel, form, instructions, viewport, iconview, drop_target;
    IswArgBuilder ab = IswArgBuilderInit();

    toplevel = IswAppInitialize(&app, "IswXdndTest", NULL, 0,
                                &argc, argv, NULL, NULL, 0);

    IswArgBuilderReset(&ab);
    IswArgWidth(&ab, 520);
    IswArgHeight(&ab, 360);
    IswArgTitle(&ab, "ISW XDND Test — drag an icon to the target (or any app)");
    IswArgAllowShellResize(&ab, True);
    IswSetValues(toplevel, ab.args, ab.count);

    form = IswCreateManagedWidget("form", formWidgetClass, toplevel, NULL, 0);

    /* Instructions */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab,
        "Drag any icon below onto 'Drop target' (or into another XDND app).");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    instructions = IswCreateManagedWidget("instructions", labelWidgetClass,
                                          form, ab.args, ab.count);

    /* Drag source: IconView inside a Viewport (windowless widget path). */
    IswArgBuilderReset(&ab);
    IswArgAllowVert(&ab, True);
    IswArgWidth(&ab, 300);
    IswArgHeight(&ab, 240);
    IswArgBorderWidth(&ab, 1);
    IswArgFromVert(&ab, instructions);
    IswArgTop(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    viewport = IswCreateManagedWidget("iconViewport", viewportWidgetClass,
                                      form, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgIconLabels(&ab, item_labels);
    IswArgIconData(&ab, item_icons);
    IswArgNumIcons(&ab, NUM_ITEMS);
    IswArgIconSize(&ab, 32);
    IswArgWidth(&ab, 300);
    IswArgMultiSelect(&ab, True);
    iconview = IswCreateManagedWidget("iconView", iconViewWidgetClass,
                                      viewport, ab.args, ab.count);
    IswAddCallback(iconview, IswNselectCallback, iconview_select_callback, NULL);

    /* Register the drag-start action and bind it to a button press. */
    static IswActionsRec actions[] = {
        { "drag-start", drag_start_action }
    };
    IswAppAddActions(app, actions, IswNumber(actions));
    /* Add drag-start on Btn1 MOTION, leaving the IconView's own translations
       (SelectItem on press, BandDrag on motion, BandFinish on release)
       intact.  The widget is designed to coexist with XDnd: press selects an
       item (without starting a band) or starts a rubber band on empty space;
       on motion BandDrag yields while IswDndIsDragging() and only runs when a
       band is active, while drag-start arms a drag only when the press landed
       on an item (no band).  The two are mutually exclusive via band_active,
       so a click selects, a drag from an item drags, and a drag from empty
       space band-selects — exactly the original behaviour plus drag. */
    IswOverrideTranslations(iconview,
        IswParseTranslationTable("<Btn1Motion>: drag-start() BandDrag()"));

    /* In-app drop target. */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Drop target");
    IswArgWidth(&ab, 160);
    IswArgHeight(&ab, 240);
    IswArgBorderWidth(&ab, 1);
    IswArgResize(&ab, False);
    IswArgFromHoriz(&ab, viewport);
    IswArgFromVert(&ab, instructions);
    IswArgHorizDistance(&ab, 20);
    IswArgTop(&ab, IswChainTop);
    drop_target = IswCreateManagedWidget("dropTarget", labelWidgetClass,
                                         form, ab.args, ab.count);
    /* Label's class does not declare the drop/drag callbacks as resources,
       so register them with the direct setters (per IswDragDrop.h). */
    IswDndWidgetAcceptDrops(drop_target);
    IswDndSetDropCallback(drop_target, drop_callback, NULL);
    IswDndSetDragMotionCallback(drop_target, drag_motion_callback, NULL);
    IswDndSetDragLeaveCallback(drop_target, drag_leave_callback, NULL);

    /* Only accept the types we know how to handle. */
    {
        Atom accepted[2];
        accepted[0] = IswDndInternType(drop_target, "text/plain");
        accepted[1] = IswDndInternType(drop_target, "text/uri-list");
        IswDndSetAcceptedTypes(drop_target, accepted, 2);
        IswDndSetAcceptedActions(drop_target,
                                  ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE);
    }

    printf("ISW XDND test ready. Drag an icon to the target.\n");
    printf("-------------------------------------------------\n");
    fflush(stdout);

    IswRealizeWidget(toplevel);
    IswAppMainLoop(app);
    return 0;
}
