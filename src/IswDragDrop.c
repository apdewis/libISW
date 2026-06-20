/*
 * IswDragDrop.c - Platform-neutral drag-and-drop policy core
 *
 * Copyright (c) 2026 ISW Project
 *
 * The generic half of the DnD engine: type/action negotiation, per-widget drop
 * registration, widget-geometry hit-testing, modifier->action mapping and
 * URI-list parsing.  No platform (xcb) type appears here; type identities are
 * plain C strings compared with strcmp.  The platform DnD backend
 * (ISWPlatformDndXCB.c) drives the wire protocol and calls into these.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <ISW/IntrinsicP.h>
#include <ISW/CompositeP.h>
#include <ISW/IswDragDropP.h>

IswDndAction
_IswDndModifiersToAction(unsigned int modifiers)
{
    /* Shift = Move, Ctrl = Copy, Ctrl+Shift = Link */
    Boolean shift = (modifiers & IswModShift) != 0;
    Boolean ctrl  = (modifiers & IswModControl) != 0;

    if (ctrl && shift) return ISW_DND_ACTION_LINK;
    if (ctrl)          return ISW_DND_ACTION_COPY;
    if (shift)         return ISW_DND_ACTION_MOVE;
    return ISW_DND_ACTION_COPY; /* default */
}

/* ------------------------------------------------------------------ */
/* Per-widget drop config management                                  */
/* ------------------------------------------------------------------ */

DropConfig *
_IswDndFindConfig(IswDndCore *core, Widget w)
{
    DropConfig *dc;
    for (dc = core->drop_configs; dc; dc = dc->next) {
        if (dc->widget == w)
            return dc;
    }
    return NULL;
}

DropConfig *
_IswDndGetOrCreateConfig(IswDndCore *core, Widget w)
{
    DropConfig *dc = _IswDndFindConfig(core, w);
    if (!dc) {
        dc = (DropConfig *) IswCalloc(1, sizeof(DropConfig));
        dc->widget = w;
        dc->next = core->drop_configs;
        core->drop_configs = dc;
    }
    return dc;
}

/* ------------------------------------------------------------------ */
/* Type/action negotiation                                            */
/* ------------------------------------------------------------------ */

Boolean
_IswDndNegotiateType(IswDndCore *core, Widget target,
                     const char **type_out, IswDndAction *action_out)
{
    DropConfig *dc = _IswDndFindConfig(core, target);

    const char *best_type = NULL;

    if (dc && dc->accepted_types && dc->num_accepted_types > 0) {
        for (int i = 0; i < dc->num_accepted_types && !best_type; i++) {
            for (int j = 0; j < core->src_num_types; j++) {
                if (strcmp(dc->accepted_types[i], core->src_types[j]) == 0) {
                    best_type = dc->accepted_types[i];
                    break;
                }
            }
        }
    } else {
        if (core->src_num_types > 0)
            best_type = core->src_types[0];
    }

    if (!best_type)
        return False;

    /* Find best matching action */
    IswDndAction target_actions = (dc && dc->accepted_actions)
                                  ? dc->accepted_actions
                                  : (ISW_DND_ACTION_COPY | ISW_DND_ACTION_MOVE |
                                     ISW_DND_ACTION_LINK);
    IswDndAction common = core->src_actions & target_actions;

    if (common == ISW_DND_ACTION_NONE)
        return False;

    /* Prefer copy > move > link > ask > private */
    IswDndAction best_action = ISW_DND_ACTION_NONE;
    if (common & ISW_DND_ACTION_COPY)         best_action = ISW_DND_ACTION_COPY;
    else if (common & ISW_DND_ACTION_MOVE)    best_action = ISW_DND_ACTION_MOVE;
    else if (common & ISW_DND_ACTION_LINK)    best_action = ISW_DND_ACTION_LINK;
    else if (common & ISW_DND_ACTION_ASK)     best_action = ISW_DND_ACTION_ASK;
    else if (common & ISW_DND_ACTION_PRIVATE) best_action = ISW_DND_ACTION_PRIVATE;

    *type_out = best_type;
    *action_out = best_action;
    return True;
}

/* ------------------------------------------------------------------ */
/* Widget tree walk to find drop target                               */
/* ------------------------------------------------------------------ */

Widget
_IswDndFindDropChild(IswDndCore *core, Widget composite, int wx, int wy)
{
    if (!IswIsComposite(composite))
        return NULL;

    CompositeWidget cw = (CompositeWidget) composite;
    for (int i = cw->composite.num_children - 1; i >= 0; i--) {
        Widget child = cw->composite.children[i];
        if (!IswIsManaged(child) || !IswIsRealized(child))
            continue;

        int cx = child->core.x;
        int cy = child->core.y;
        int cw2 = child->core.width;
        int ch = child->core.height;

        if (wx >= cx && wx < cx + cw2 && wy >= cy && wy < cy + ch) {
            /* Check DropConfig first (works for any widget class),
             * then fall back to IswHasCallbacks (for widgets that
             * declare IswNdropCallback as a resource). */
            if (_IswDndFindConfig(core, child) ||
                IswHasCallbacks(child, IswNdropCallback) == IswCallbackHasSome)
                return child;

            if (IswIsComposite(child)) {
                Widget deeper = _IswDndFindDropChild(core, child, wx - cx, wy - cy);
                if (deeper)
                    return deeper;
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* text/uri-list parsing                                              */
/* ------------------------------------------------------------------ */

char **
_IswDndParseUriList(const char *data, int len, int *out_count)
{
    char **uris = NULL;
    int count = 0;
    int capacity = 0;
    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        int line_len = eol - p;
        if (line_len > 0 && *p != '#') {
            const char *uri = p;
            int uri_len = line_len;
            if (uri_len >= 7 && strncmp(uri, "file://", 7) == 0) {
                uri += 7;
                uri_len -= 7;
            }

            if (count >= capacity) {
                capacity = capacity ? capacity * 2 : 8;
                uris = (char **) IswRealloc((char *) uris,
                                           (capacity + 1) * sizeof(char *));
            }
            char *entry = IswMalloc(uri_len + 1);
            memcpy(entry, uri, uri_len);
            entry[uri_len] = '\0';
            uris[count++] = entry;
        }

        p = eol;
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }

    if (uris)
        uris[count] = NULL;

    *out_count = count;
    return uris;
}
