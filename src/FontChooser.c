/*
 * FontChooser.c - FontChooser widget implementation
 *
 * A Form subclass with a font family list (populated from fontconfig),
 * a size selector list, and a text preview label.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ISW/ISWP.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/FontChooserP.h>
#include <ISW/List.h>
#include <ISW/Label.h>
#include <ISW/Viewport.h>

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define superclass (&formClassRec)

#define Offset(field) XtOffsetOf(FontChooserRec, field)

static XtResource resources[] = {
    {XtNfontFamily, XtCFontFamily, XtRString, sizeof(String),
        Offset(fontChooser.family), XtRString, (XtPointer) "Sans"},
    {XtNfontSize, XtCFontSize, XtRInt, sizeof(int),
        Offset(fontChooser.size), XtRImmediate, (XtPointer) 12},
    {XtNpreviewText, XtCPreviewText, XtRString, sizeof(String),
        Offset(fontChooser.preview_text), XtRString,
        (XtPointer) "The quick brown fox jumps over the lazy dog"},
    {XtNfontChanged, XtCCallback, XtRCallback, sizeof(XtPointer),
        Offset(fontChooser.font_changed), XtRCallback, NULL},
    {XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
        Offset(core.border_width), XtRImmediate, (XtPointer) 0},
};

#undef Offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void FamilySelected(Widget, XtPointer, XtPointer);
static void SizeSelected(Widget, XtPointer, XtPointer);
static void NotifyChange(FontChooserWidget);

FontChooserClassRec fontChooserClassRec = {
  { /* core */
    (WidgetClass) superclass,
    "FontChooser",
    sizeof(FontChooserRec),
    IswInitializeWidgetSet,
    NULL,
    FALSE,
    Initialize,
    NULL,
    XtInheritRealize,
    NULL,
    0,
    resources,
    XtNumber(resources),
    NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
    XtInheritResize,
    XtInheritExpose,
    NULL,
    NULL,
    XtInheritSetValuesAlmost,
    NULL,
    NULL,
    XtVersion,
    NULL,
    NULL,
    XtInheritQueryGeometry,
    XtInheritDisplayAccelerator,
    NULL
  },
  { /* composite */
    XtInheritGeometryManager,
    XtInheritChangeManaged,
    XtInheritInsertChild,
    XtInheritDeleteChild,
    NULL
  },
  { /* constraint */
    NULL, 0,
    sizeof(FontChooserConstraintsRec),
    NULL, NULL, NULL, NULL
  },
  { /* form */
    XtInheritLayout
  },
  { /* fontChooser */
    0
  }
};

WidgetClass fontChooserWidgetClass = (WidgetClass)&fontChooserClassRec;

/* --- Font enumeration via fontconfig --- */

static int
cmp_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void
EnumerateFonts(FontChooserWidget fcw)
{
    FcPattern *pat = FcPatternCreate();
    FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, (char *)NULL);
    FcFontSet *fs = FcFontList(NULL, pat, os);

    fcw->fontChooser.family_names = NULL;
    fcw->fontChooser.num_families = 0;

    if (fs) {
        /* Collect unique family names */
        String *names = calloc((size_t)fs->nfont, sizeof(String));
        int count = 0;

        for (int i = 0; i < fs->nfont; i++) {
            FcChar8 *family = NULL;
            if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family)
                == FcResultMatch && family) {
                /* Check for duplicates */
                int dup = 0;
                for (int j = 0; j < count; j++) {
                    if (strcmp(names[j], (char *)family) == 0) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup)
                    names[count++] = strdup((char *)family);
            }
        }

        /* Sort alphabetically */
        qsort(names, (size_t)count, sizeof(String), cmp_strings);

        fcw->fontChooser.family_names = names;
        fcw->fontChooser.num_families = count;

        FcFontSetDestroy(fs);
    }

    if (os) FcObjectSetDestroy(os);
    if (pat) FcPatternDestroy(pat);
}

static void
FreeFontNames(FontChooserWidget fcw)
{
    if (fcw->fontChooser.family_names) {
        for (int i = 0; i < fcw->fontChooser.num_families; i++)
            free(fcw->fontChooser.family_names[i]);
        free(fcw->fontChooser.family_names);
        fcw->fontChooser.family_names = NULL;
        fcw->fontChooser.num_families = 0;
    }
}

/* --- Callbacks --- */

static void
FamilySelected(Widget w, XtPointer client_data, XtPointer call_data)
{
    FontChooserWidget fcw = (FontChooserWidget) client_data;
    IswListReturnStruct *item = (IswListReturnStruct *) call_data;
    (void)w;

    if (item && item->string)
        fcw->fontChooser.family = item->string;

    NotifyChange(fcw);
}

static void
SizeSelected(Widget w, XtPointer client_data, XtPointer call_data)
{
    FontChooserWidget fcw = (FontChooserWidget) client_data;
    IswListReturnStruct *item = (IswListReturnStruct *) call_data;
    (void)w;

    if (item && item->string)
        fcw->fontChooser.size = atoi(item->string);

    NotifyChange(fcw);
}

static void
NotifyChange(FontChooserWidget fcw)
{
    /* Update preview label text and font */
    if (fcw->fontChooser.previewW) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %dpt: %s",
                 fcw->fontChooser.family,
                 fcw->fontChooser.size,
                 fcw->fontChooser.preview_text);

        /* Create an XFontStruct with the selected family for Cairo rendering */
        XFontStruct *fs = XtNew(XFontStruct);
        memset(fs, 0, sizeof(*fs));
        fs->ascent = fcw->fontChooser.size;
        fs->descent = fcw->fontChooser.size / 3;
        fs->font_family = XtNewString(fcw->fontChooser.family);
        fs->font_weight = FC_WEIGHT_NORMAL;
        fs->font_slant  = FC_SLANT_ROMAN;

        Arg a[2];
        XtSetArg(a[0], XtNlabel, buf);
        XtSetArg(a[1], XtNfont, fs);
        XtSetValues(fcw->fontChooser.previewW, a, 2);
    }

    IswFontChooserCallbackData cb;
    cb.family = fcw->fontChooser.family;
    cb.size = fcw->fontChooser.size;
    XtCallCallbacks((Widget)fcw, XtNfontChanged, (XtPointer)&cb);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    FontChooserWidget fcw = (FontChooserWidget) new;
    Arg a[10];
    Cardinal n;
    Dimension list_w = ISWScaleDim(new, 180);
    Dimension list_h = ISWScaleDim(new, 150);
    Dimension size_w = ISWScaleDim(new, 50);
    Dimension preview_w = ISWScaleDim(new, 240);
    Dimension preview_h = ISWScaleDim(new, 40);
    (void)request; (void)args; (void)num_args;

    /* Enumerate system fonts */
    EnumerateFonts(fcw);

    /* Family label */
    n = 0;
    XtSetArg(a[n], XtNlabel, "Family"); n++;
    XtSetArg(a[n], XtNborderWidth, 0); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    XtCreateManagedWidget("familyLabel", labelWidgetClass, new, a, n);

    /* Size label */
    Widget familyLabel = XtNameToWidget(new, "familyLabel");

    n = 0;
    XtSetArg(a[n], XtNlabel, "Size"); n++;
    XtSetArg(a[n], XtNborderWidth, 0); n++;
    XtSetArg(a[n], XtNfromHoriz, familyLabel); n++;
    XtSetArg(a[n], XtNhorizDistance, (int)list_w); n++;
    XtCreateManagedWidget("sizeLabel", labelWidgetClass, new, a, n);

    /* Family list in a viewport */
    Widget familyVp;
    n = 0;
    XtSetArg(a[n], XtNallowVert, True); n++;
    XtSetArg(a[n], XtNwidth, list_w); n++;
    XtSetArg(a[n], XtNheight, list_h); n++;
    XtSetArg(a[n], XtNfromVert, familyLabel); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    familyVp = XtCreateManagedWidget("familyViewport", viewportWidgetClass,
                                      new, a, n);

    n = 0;
    if (fcw->fontChooser.num_families > 0) {
        XtSetArg(a[n], XtNlist, fcw->fontChooser.family_names); n++;
        XtSetArg(a[n], XtNnumberStrings, fcw->fontChooser.num_families); n++;
    }
    XtSetArg(a[n], XtNdefaultColumns, 1); n++;
    XtSetArg(a[n], XtNforceColumns, True); n++;
    fcw->fontChooser.familyListW = XtCreateManagedWidget(
        "familyList", listWidgetClass, familyVp, a, n);
    XtAddCallback(fcw->fontChooser.familyListW, XtNcallback,
                  FamilySelected, (XtPointer)fcw);

    /* Size list in a viewport */
    static String sizes[] = {
        "8", "9", "10", "11", "12", "14", "16", "18",
        "20", "24", "28", "32", "36", "48", "64", "72"
    };

    Widget sizeVp;
    n = 0;
    XtSetArg(a[n], XtNallowVert, True); n++;
    XtSetArg(a[n], XtNwidth, size_w); n++;
    XtSetArg(a[n], XtNheight, list_h); n++;
    XtSetArg(a[n], XtNfromHoriz, familyVp); n++;
    XtSetArg(a[n], XtNfromVert, familyLabel); n++;
    sizeVp = XtCreateManagedWidget("sizeViewport", viewportWidgetClass,
                                    new, a, n);

    n = 0;
    XtSetArg(a[n], XtNlist, sizes); n++;
    XtSetArg(a[n], XtNnumberStrings, XtNumber(sizes)); n++;
    XtSetArg(a[n], XtNdefaultColumns, 1); n++;
    XtSetArg(a[n], XtNforceColumns, True); n++;
    fcw->fontChooser.sizeListW = XtCreateManagedWidget(
        "sizeList", listWidgetClass, sizeVp, a, n);
    XtAddCallback(fcw->fontChooser.sizeListW, XtNcallback,
                  SizeSelected, (XtPointer)fcw);

    /* Preview label */
    char preview_buf[256];
    snprintf(preview_buf, sizeof(preview_buf), "%s %dpt: %s",
             fcw->fontChooser.family,
             fcw->fontChooser.size,
             fcw->fontChooser.preview_text);

    n = 0;
    XtSetArg(a[n], XtNlabel, preview_buf); n++;
    XtSetArg(a[n], XtNwidth, preview_w); n++;
    XtSetArg(a[n], XtNheight, preview_h); n++;
    XtSetArg(a[n], XtNborderWidth, 1); n++;
    XtSetArg(a[n], XtNfromVert, familyVp); n++;
    XtSetArg(a[n], XtNleft, XtChainLeft); n++;
    XtSetArg(a[n], XtNresize, False); n++;
    fcw->fontChooser.previewW = XtCreateManagedWidget(
        "preview", labelWidgetClass, new, a, n);
}

static void
Destroy(Widget w)
{
    FontChooserWidget fcw = (FontChooserWidget) w;
    FreeFontNames(fcw);
}

/* --- Public API --- */

String
IswFontChooserGetFamily(Widget w)
{
    return ((FontChooserWidget) w)->fontChooser.family;
}

int
IswFontChooserGetSize(Widget w)
{
    return ((FontChooserWidget) w)->fontChooser.size;
}
