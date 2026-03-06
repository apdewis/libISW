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
    xcb_atom_t atom;
} AtomTableEntry;

static AtomTableEntry predefined_atoms[] = {
    /* Predefined atoms from X11/Xatom.h */
    { "PRIMARY", XCB_ATOM_PRIMARY },
    { "SECONDARY", XCB_ATOM_SECONDARY },
    { "ARC", XCB_ATOM_ARC },
    { "ATOM", XCB_ATOM_ATOM },
    { "BITMAP", XCB_ATOM_BITMAP },
    { "CARDINAL", XCB_ATOM_CARDINAL },
    { "COLORMAP", XCB_ATOM_COLORMAP },
    { "CURSOR", XCB_ATOM_CURSOR },
    { "CUT_BUFFER0", XCB_ATOM_CUT_BUFFER0 },
    { "CUT_BUFFER1", XCB_ATOM_CUT_BUFFER1 },
    { "CUT_BUFFER2", XCB_ATOM_CUT_BUFFER2 },
    { "CUT_BUFFER3", XCB_ATOM_CUT_BUFFER3 },
    { "CUT_BUFFER4", XCB_ATOM_CUT_BUFFER4 },
    { "CUT_BUFFER5", XCB_ATOM_CUT_BUFFER5 },
    { "CUT_BUFFER6", XCB_ATOM_CUT_BUFFER6 },
    { "CUT_BUFFER7", XCB_ATOM_CUT_BUFFER7 },
    { "DRAWABLE", XCB_ATOM_DRAWABLE },
    { "FONT", XCB_ATOM_FONT },
    { "INTEGER", XCB_ATOM_INTEGER },
    { "PIXMAP", XCB_ATOM_PIXMAP },
    { "POINT", XCB_ATOM_POINT },
    { "RECTANGLE", XCB_ATOM_RECTANGLE },
    { "RESOURCE_MANAGER", XCB_ATOM_RESOURCE_MANAGER },
    { "RGB_COLOR_MAP", XCB_ATOM_RGB_COLOR_MAP },
    { "RGB_BEST_MAP", XCB_ATOM_RGB_BEST_MAP },
    { "RGB_BLUE_MAP", XCB_ATOM_RGB_BLUE_MAP },
    { "RGB_DEFAULT_MAP", XCB_ATOM_RGB_DEFAULT_MAP },
    { "RGB_GRAY_MAP", XCB_ATOM_RGB_GRAY_MAP },
    { "RGB_GREEN_MAP", XCB_ATOM_RGB_GREEN_MAP },
    { "RGB_RED_MAP", XCB_ATOM_RGB_RED_MAP },
    { "STRING", XCB_ATOM_STRING },
    { "VISUALID", XCB_ATOM_VISUALID },
    { "WINDOW", XCB_ATOM_WINDOW },
    { "WM_COMMAND", XCB_ATOM_WM_COMMAND },
    { "WM_HINTS", XCB_ATOM_WM_HINTS },
    { "WM_CLIENT_MACHINE", XCB_ATOM_WM_CLIENT_MACHINE },
    { "WM_ICON_NAME", XCB_ATOM_WM_ICON_NAME },
    { "WM_ICON_SIZE", XCB_ATOM_WM_ICON_SIZE },
    { "WM_NAME", XCB_ATOM_WM_NAME },
    { "WM_NORMAL_HINTS", XCB_ATOM_WM_NORMAL_HINTS },
    { "WM_SIZE_HINTS", XCB_ATOM_WM_SIZE_HINTS },
    { "WM_ZOOM_HINTS", XCB_ATOM_WM_ZOOM_HINTS },
    { "MIN_SPACE", XCB_ATOM_MIN_SPACE },
    { "NORM_SPACE", XCB_ATOM_NORM_SPACE },
    { "MAX_SPACE", XCB_ATOM_MAX_SPACE },
    { "END_SPACE", XCB_ATOM_END_SPACE },
    { "SUPERSCRIPT_X", XCB_ATOM_SUPERSCRIPT_X },
    { "SUPERSCRIPT_Y", XCB_ATOM_SUPERSCRIPT_Y },
    { "SUBSCRIPT_X", XCB_ATOM_SUBSCRIPT_X },
    { "SUBSCRIPT_Y", XCB_ATOM_SUBSCRIPT_Y },
    { "UNDERLINE_POSITION", XCB_ATOM_UNDERLINE_POSITION },
    { "UNDERLINE_THICKNESS", XCB_ATOM_UNDERLINE_THICKNESS },
    { "STRIKEOUT_ASCENT", XCB_ATOM_STRIKEOUT_ASCENT },
    { "STRIKEOUT_DESCENT", XCB_ATOM_STRIKEOUT_DESCENT },
    { "ITALIC_ANGLE", XCB_ATOM_ITALIC_ANGLE },
    { "X_HEIGHT", XCB_ATOM_X_HEIGHT },
    { "QUAD_WIDTH", XCB_ATOM_QUAD_WIDTH },
    { "WEIGHT", XCB_ATOM_WEIGHT },
    { "POINT_SIZE", XCB_ATOM_POINT_SIZE },
    { "RESOLUTION", XCB_ATOM_RESOLUTION },
    { "COPYRIGHT", XCB_ATOM_COPYRIGHT },
    { "NOTICE", XCB_ATOM_NOTICE },
    { "FONT_NAME", XCB_ATOM_FONT_NAME },
    { "FAMILY_NAME", XCB_ATOM_FAMILY_NAME },
    { "FULL_NAME", XCB_ATOM_FULL_NAME },
    { "CAP_HEIGHT", XCB_ATOM_CAP_HEIGHT },
    { "WM_CLASS", XCB_ATOM_WM_CLASS },
    { "WM_TRANSIENT_FOR", XCB_ATOM_WM_TRANSIENT_FOR },
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
xcb_atom_t
XawInternAtom(xcb_connection_t *dpy, const char *name, Bool only_if_exists)
{
    xcb_connection_t *conn = (xcb_connection_t *)dpy;
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom = None;
    
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
XawInternStrings(xcb_connection_t *dpy, String *names, Cardinal count, xcb_atom_t *atoms_return)
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
XawNameOfAtom(xcb_connection_t *dpy, xcb_atom_t atom)
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
xcb_atom_t
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
