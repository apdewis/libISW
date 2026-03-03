/*
 * XawAtoms.c - Atom caching functions for Xaw3d
 * 
 * Replaces Xmu/Atoms.h functionality with XCB-based implementations.
 * Provides atom interning, lookup, and caching to minimize server round-trips.
 *
 * Copyright (c) 2026 Xaw3d Project
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "XawUtils.h"
#include "XawXcbTypes.h"
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdlib.h>
#include <string.h>

/*
 * Static atom table for common X atoms
 * Avoids server round-trips for well-known atoms
 */
typedef struct {
    const char *name;
    Atom atom;
} AtomTableEntry;

static AtomTableEntry predefined_atoms[] = {
    /* Predefined atoms from X11/Xatom.h */
    { "PRIMARY", XA_PRIMARY },
    { "SECONDARY", XA_SECONDARY },
    { "ARC", XA_ARC },
    { "ATOM", XA_ATOM },
    { "BITMAP", XA_BITMAP },
    { "CARDINAL", XA_CARDINAL },
    { "COLORMAP", XA_COLORMAP },
    { "CURSOR", XA_CURSOR },
    { "CUT_BUFFER0", XA_CUT_BUFFER0 },
    { "CUT_BUFFER1", XA_CUT_BUFFER1 },
    { "CUT_BUFFER2", XA_CUT_BUFFER2 },
    { "CUT_BUFFER3", XA_CUT_BUFFER3 },
    { "CUT_BUFFER4", XA_CUT_BUFFER4 },
    { "CUT_BUFFER5", XA_CUT_BUFFER5 },
    { "CUT_BUFFER6", XA_CUT_BUFFER6 },
    { "CUT_BUFFER7", XA_CUT_BUFFER7 },
    { "DRAWABLE", XA_DRAWABLE },
    { "FONT", XA_FONT },
    { "INTEGER", XA_INTEGER },
    { "PIXMAP", XA_PIXMAP },
    { "POINT", XA_POINT },
    { "RECTANGLE", XA_RECTANGLE },
    { "RESOURCE_MANAGER", XA_RESOURCE_MANAGER },
    { "RGB_COLOR_MAP", XA_RGB_COLOR_MAP },
    { "RGB_BEST_MAP", XA_RGB_BEST_MAP },
    { "RGB_BLUE_MAP", XA_RGB_BLUE_MAP },
    { "RGB_DEFAULT_MAP", XA_RGB_DEFAULT_MAP },
    { "RGB_GRAY_MAP", XA_RGB_GRAY_MAP },
    { "RGB_GREEN_MAP", XA_RGB_GREEN_MAP },
    { "RGB_RED_MAP", XA_RGB_RED_MAP },
    { "STRING", XA_STRING },
    { "VISUALID", XA_VISUALID },
    { "WINDOW", XA_WINDOW },
    { "WM_COMMAND", XA_WM_COMMAND },
    { "WM_HINTS", XA_WM_HINTS },
    { "WM_CLIENT_MACHINE", XA_WM_CLIENT_MACHINE },
    { "WM_ICON_NAME", XA_WM_ICON_NAME },
    { "WM_ICON_SIZE", XA_WM_ICON_SIZE },
    { "WM_NAME", XA_WM_NAME },
    { "WM_NORMAL_HINTS", XA_WM_NORMAL_HINTS },
    { "WM_SIZE_HINTS", XA_WM_SIZE_HINTS },
    { "WM_ZOOM_HINTS", XA_WM_ZOOM_HINTS },
    { "MIN_SPACE", XA_MIN_SPACE },
    { "NORM_SPACE", XA_NORM_SPACE },
    { "MAX_SPACE", XA_MAX_SPACE },
    { "END_SPACE", XA_END_SPACE },
    { "SUPERSCRIPT_X", XA_SUPERSCRIPT_X },
    { "SUPERSCRIPT_Y", XA_SUPERSCRIPT_Y },
    { "SUBSCRIPT_X", XA_SUBSCRIPT_X },
    { "SUBSCRIPT_Y", XA_SUBSCRIPT_Y },
    { "UNDERLINE_POSITION", XA_UNDERLINE_POSITION },
    { "UNDERLINE_THICKNESS", XA_UNDERLINE_THICKNESS },
    { "STRIKEOUT_ASCENT", XA_STRIKEOUT_ASCENT },
    { "STRIKEOUT_DESCENT", XA_STRIKEOUT_DESCENT },
    { "ITALIC_ANGLE", XA_ITALIC_ANGLE },
    { "X_HEIGHT", XA_X_HEIGHT },
    { "QUAD_WIDTH", XA_QUAD_WIDTH },
    { "WEIGHT", XA_WEIGHT },
    { "POINT_SIZE", XA_POINT_SIZE },
    { "RESOLUTION", XA_RESOLUTION },
    { "COPYRIGHT", XA_COPYRIGHT },
    { "NOTICE", XA_NOTICE },
    { "FONT_NAME", XA_FONT_NAME },
    { "FAMILY_NAME", XA_FAMILY_NAME },
    { "FULL_NAME", XA_FULL_NAME },
    { "CAP_HEIGHT", XA_CAP_HEIGHT },
    { "WM_CLASS", XA_WM_CLASS },
    { "WM_TRANSIENT_FOR", XA_WM_TRANSIENT_FOR },
    { NULL, 0 }
};

/*
 * XawInternAtom - Intern a single atom
 * 
 * This is the XCB-based replacement for XmuInternAtom().
 * Converts an atom name string to an Atom identifier.
 * 
 * @dpy: Display connection (actually xcb_connection_t*)
 * @name: Atom name string to intern
 * @only_if_exists: If True, only return atom if it already exists
 * 
 * Returns: The Atom value, or None if the atom doesn't exist and only_if_exists is True
 */
Atom
XawInternAtom(xcb_connection_t *dpy, const char *name, Bool only_if_exists)
{
    xcb_connection_t *conn = (xcb_connection_t *)dpy;
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    Atom atom = None;
    
    if (!conn || !name)
        return None;
    
    /* Send intern atom request */
    cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0, 
                              strlen(name), name);
    
    /* Get reply (synchronous) */
    reply = xcb_intern_atom_reply(conn, cookie, NULL);
    
    if (reply) {
        atom = reply->atom;
        free(reply);
    }
    
    return atom;
}

/*
 * XawInternStrings - Intern multiple atoms at once
 * 
 * This is the XCB-based replacement for XmuInternStrings().
 * Efficiently interns multiple atoms in a batch.
 * 
 * @dpy: Display connection (actually xcb_connection_t*)
 * @names: Array of atom name strings
 * @count: Number of names in the array
 * @atoms_return: Array to receive the interned atoms (must be pre-allocated)
 * 
 * Note: This implementation calls XawInternAtom for each atom.
 * A more efficient implementation could pipeline the requests.
 */
void
XawInternStrings(xcb_connection_t *dpy, String *names, Cardinal count, Atom *atoms_return)
{
    Cardinal i;
    
    if (!dpy || !names || !atoms_return || count == 0)
        return;
    
    /* Intern each atom */
    for (i = 0; i < count; i++) {
        atoms_return[i] = XawInternAtom(dpy, names[i], False);
    }
}

/*
 * XawNameOfAtom - Get the string name of an atom
 * 
 * This is the XCB-based replacement for XmuNameOfAtom().
 * Performs reverse lookup from Atom to name string.
 * 
 * @dpy: Display connection (actually xcb_connection_t*)
 * @atom: The atom to look up
 * 
 * Returns: Allocated string containing the atom name (caller must free with XFree())
 *          or NULL on error
 */
char*
XawNameOfAtom(xcb_connection_t *dpy, Atom atom)
{
    xcb_connection_t *conn = (xcb_connection_t *)dpy;
    xcb_get_atom_name_cookie_t cookie;
    xcb_get_atom_name_reply_t *reply;
    char *name = NULL;
    int name_len;
    
    if (!conn || atom == None)
        return NULL;
    
    /* Send get atom name request */
    cookie = xcb_get_atom_name(conn, atom);
    
    /* Get reply (synchronous) */
    reply = xcb_get_atom_name_reply(conn, cookie, NULL);
    
    if (reply) {
        name_len = xcb_get_atom_name_name_length(reply);
        const char *atom_name = xcb_get_atom_name_name(reply);
        
        /* Allocate and copy the name */
        name = (char *)malloc(name_len + 1);
        if (name) {
            memcpy(name, atom_name, name_len);
            name[name_len] = '\0';
        }
        
        free(reply);
    }
    
    return name;
}

/*
 * XawMakeAtom - Get a predefined atom without server round-trip
 * 
 * This is the XCB-based replacement for XmuMakeAtom().
 * Uses a static lookup table for well-known X atoms.
 * 
 * @name: Atom name (must be a well-known X atom)
 * 
 * Returns: The atom value from the static table, or None if not found
 * 
 * Note: This function only works for predefined X atoms (those in X11/Xatom.h).
 * For custom or extension atoms, use XawInternAtom() instead.
 */
Atom
XawMakeAtom(const char *name)
{
    AtomTableEntry *entry;
    
    if (!name)
        return None;
    
    /* Search the predefined atom table */
    for (entry = predefined_atoms; entry->name != NULL; entry++) {
        if (strcmp(name, entry->name) == 0)
            return entry->atom;
    }
    
    /* Not found in static table */
    return None;
}
