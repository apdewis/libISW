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
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/ISWRender.h>
#include <ISW/FontChooserP.h>
#include <ISW/List.h>
#include <ISW/Label.h>
#include <ISW/Viewport.h>
#include <ISW/IswArgMacros.h>

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define superclass (&formClassRec)

#define Offset(field) IswOffsetOf(FontChooserRec, field)

static IswResource resources[] = {
    {IswNfontFamily, IswCFontFamily, IswRString, sizeof(String),
        Offset(fontChooser.family), IswRString, (IswPointer) "Sans"},
    {IswNfontSize, IswCFontSize, IswRInt, sizeof(int),
        Offset(fontChooser.size), IswRImmediate, (IswPointer) 12},
    {IswNfontWeight, IswCFontWeight, IswRInt, sizeof(int),
        Offset(fontChooser.weight), IswRImmediate, (IswPointer) FC_WEIGHT_NORMAL},
    {IswNfontSlant, IswCFontSlant, IswRInt, sizeof(int),
        Offset(fontChooser.slant), IswRImmediate, (IswPointer) FC_SLANT_ROMAN},
    {IswNpreviewText, IswCPreviewText, IswRString, sizeof(String),
        Offset(fontChooser.preview_text), IswRString,
        (IswPointer) "The quick brown fox jumps over the lazy dog"},
    {IswNfontChanged, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(fontChooser.font_changed), IswRCallback, NULL},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
};

#undef Offset

static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static void FamilySelected(Widget, IswPointer, IswPointer);
static void StyleSelected(Widget, IswPointer, IswPointer);
static void SizeSelected(Widget, IswPointer, IswPointer);
static void RefreshStyles(FontChooserWidget);
static void FreeStyles(FontChooserWidget);
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
    IswInheritRealize,
    NULL,
    0,
    resources,
    IswNumber(resources),
    NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
    IswInheritResize,
    IswInheritExpose,
    NULL,
    NULL,
    IswInheritSetValuesAlmost,
    NULL,
    NULL,
    IswVersion,
    NULL,
    NULL,
    IswInheritQueryGeometry,
    IswInheritDisplayAccelerator,
    NULL
  },
  { /* composite */
    IswInheritGeometryManager,
    IswInheritChangeManaged,
    IswInheritInsertChild,
    IswInheritDeleteChild,
    NULL
  },
  { /* constraint */
    NULL, 0,
    sizeof(FontChooserConstraintsRec),
    NULL, NULL, NULL, NULL
  },
  { /* form */
    IswInheritLayout
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
            free((void *)fcw->fontChooser.family_names[i]);
        free(fcw->fontChooser.family_names);
        fcw->fontChooser.family_names = NULL;
        fcw->fontChooser.num_families = 0;
    }
}

/* --- Style enumeration per family --- */

static void
FreeStyles(FontChooserWidget fcw)
{
    if (fcw->fontChooser.style_names) {
        for (int i = 0; i < fcw->fontChooser.num_styles; i++)
            free((void *)fcw->fontChooser.style_names[i]);
        free(fcw->fontChooser.style_names);
        fcw->fontChooser.style_names = NULL;
    }
    free(fcw->fontChooser.style_weights);
    fcw->fontChooser.style_weights = NULL;
    free(fcw->fontChooser.style_slants);
    fcw->fontChooser.style_slants = NULL;
    fcw->fontChooser.num_styles = 0;
}

static void
RefreshStyles(FontChooserWidget fcw)
{
    FreeStyles(fcw);

    FcPattern *pat = FcPatternCreate();
    FcPatternAddString(pat, FC_FAMILY, (FcChar8 *)fcw->fontChooser.family);
    FcObjectSet *os = FcObjectSetBuild(FC_STYLE, FC_WEIGHT, FC_SLANT, (char *)NULL);
    FcFontSet *fs = FcFontList(NULL, pat, os);

    if (!fs || fs->nfont == 0) {
        /* Fallback: single "Regular" entry */
        fcw->fontChooser.style_names   = calloc(1, sizeof(String));
        fcw->fontChooser.style_weights = calloc(1, sizeof(int));
        fcw->fontChooser.style_slants  = calloc(1, sizeof(int));
        fcw->fontChooser.style_names[0]   = strdup("Regular");
        fcw->fontChooser.style_weights[0] = FC_WEIGHT_NORMAL;
        fcw->fontChooser.style_slants[0]  = FC_SLANT_ROMAN;
        fcw->fontChooser.num_styles = 1;
        goto cleanup;
    }

    String *names   = calloc((size_t)fs->nfont, sizeof(String));
    int    *weights = calloc((size_t)fs->nfont, sizeof(int));
    int    *slants  = calloc((size_t)fs->nfont, sizeof(int));
    int count = 0;

    for (int i = 0; i < fs->nfont; i++) {
        FcChar8 *style = NULL;
        int w = FC_WEIGHT_NORMAL, s = FC_SLANT_ROMAN;

        if (FcPatternGetString(fs->fonts[i], FC_STYLE, 0, &style) != FcResultMatch
            || !style)
            continue;

        FcPatternGetInteger(fs->fonts[i], FC_WEIGHT, 0, &w);
        FcPatternGetInteger(fs->fonts[i], FC_SLANT, 0, &s);

        /* Deduplicate by style name */
        int dup = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(names[j], (char *)style) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) {
            names[count]   = strdup((char *)style);
            weights[count] = w;
            slants[count]  = s;
            count++;
        }
    }

    /* Sort by weight then slant so the list reads naturally */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (weights[i] > weights[j] ||
                (weights[i] == weights[j] && slants[i] > slants[j])) {
                String tn = names[i];   names[i]   = names[j];   names[j]   = tn;
                int tw     = weights[i]; weights[i] = weights[j]; weights[j] = tw;
                int ts     = slants[i];  slants[i]  = slants[j];  slants[j]  = ts;
            }
        }
    }

    fcw->fontChooser.style_names   = names;
    fcw->fontChooser.style_weights = weights;
    fcw->fontChooser.style_slants  = slants;
    fcw->fontChooser.num_styles    = count;

cleanup:
    if (fs) FcFontSetDestroy(fs);
    if (os) FcObjectSetDestroy(os);
    if (pat) FcPatternDestroy(pat);

    /* Update the style list widget */
    if (fcw->fontChooser.styleListW) {
        IswListChange(fcw->fontChooser.styleListW,
                      fcw->fontChooser.style_names,
                      fcw->fontChooser.num_styles, 0, True);

        /* Select the entry matching current weight/slant, or first */
        int sel = 0;
        for (int i = 0; i < fcw->fontChooser.num_styles; i++) {
            if (fcw->fontChooser.style_weights[i] == fcw->fontChooser.weight &&
                fcw->fontChooser.style_slants[i]  == fcw->fontChooser.slant) {
                sel = i;
                break;
            }
        }
        IswListHighlight(fcw->fontChooser.styleListW, sel);
        fcw->fontChooser.weight = fcw->fontChooser.style_weights[sel];
        fcw->fontChooser.slant  = fcw->fontChooser.style_slants[sel];
    }
}

/* --- Callbacks --- */

static void
FamilySelected(Widget w, IswPointer client_data, IswPointer call_data)
{
    FontChooserWidget fcw = (FontChooserWidget) client_data;
    IswListReturnStruct *item = (IswListReturnStruct *) call_data;
    (void)w;

    if (item && item->string)
        fcw->fontChooser.family = item->string;

    RefreshStyles(fcw);
    NotifyChange(fcw);
}

static void
StyleSelected(Widget w, IswPointer client_data, IswPointer call_data)
{
    FontChooserWidget fcw = (FontChooserWidget) client_data;
    IswListReturnStruct *item = (IswListReturnStruct *) call_data;
    (void)w;

    if (item && item->string) {
        for (int i = 0; i < fcw->fontChooser.num_styles; i++) {
            if (strcmp(fcw->fontChooser.style_names[i], item->string) == 0) {
                fcw->fontChooser.weight = fcw->fontChooser.style_weights[i];
                fcw->fontChooser.slant  = fcw->fontChooser.style_slants[i];
                break;
            }
        }
    }

    NotifyChange(fcw);
}

static void
SizeSelected(Widget w, IswPointer client_data, IswPointer call_data)
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

        IswFontStruct *fs = IswNew(IswFontStruct);
        memset(fs, 0, sizeof(*fs));
        fs->pt_size     = (double)fcw->fontChooser.size;
        fs->font_family = IswNewString(fcw->fontChooser.family);
        fs->font_weight = fcw->fontChooser.weight;
        fs->font_slant  = fcw->fontChooser.slant;

        IswArgBuilder ab = IswArgBuilderInit();
        IswArgLabel(&ab, buf);
        IswArgFont(&ab, fs);
        IswSetValues(fcw->fontChooser.previewW, ab.args, ab.count);
    }

    IswFontChooserCallbackData cb;
    cb.family = fcw->fontChooser.family;
    cb.size   = fcw->fontChooser.size;
    cb.weight = fcw->fontChooser.weight;
    cb.slant  = fcw->fontChooser.slant;
    IswCallCallbacks((Widget)fcw, IswNfontChanged, (IswPointer)&cb);
}

/* --- Widget methods --- */

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    FontChooserWidget fcw = (FontChooserWidget) new;
    IswArgBuilder ab = IswArgBuilderInit();
    Dimension list_w = (180);
    Dimension list_h = (150);
    Dimension size_w = (50);
    Dimension preview_w = (360);
    Dimension preview_h = (40);
    (void)request; (void)args; (void)num_args;

    /* Enumerate system fonts */
    EnumerateFonts(fcw);

    /* Family label */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Family");
    IswArgBorderWidth(&ab, 0);
    IswArgLeft(&ab, IswChainLeft);
    Widget familyLabel = IswCreateManagedWidget("familyLabel", labelWidgetClass, new, ab.args, ab.count);

    /* Family list in a viewport */
    Widget familyVp;
    IswArgBuilderReset(&ab);
    IswArgAllowVert(&ab, True);
    IswArgUseRight(&ab, True);
    IswArgWidth(&ab, list_w);
    IswArgHeight(&ab, list_h);
    IswArgFromVert(&ab, familyLabel);
    IswArgLeft(&ab, IswChainLeft);
    familyVp = IswCreateManagedWidget("familyViewport", viewportWidgetClass,
                                      new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    if (fcw->fontChooser.num_families > 0) {
        IswArgList(&ab, fcw->fontChooser.family_names);
        IswArgNumberStrings(&ab, fcw->fontChooser.num_families);
    }
    IswArgDefaultColumns(&ab, 1);
    IswArgForceColumns(&ab, True);
    fcw->fontChooser.familyListW = IswCreateManagedWidget(
        "familyList", listWidgetClass, familyVp, ab.args, ab.count);
    IswAddCallback(fcw->fontChooser.familyListW, IswNcallback,
                  FamilySelected, (IswPointer)fcw);

    /* Style label — positioned to the right of family viewport */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Style");
    IswArgBorderWidth(&ab, 0);
    IswArgFromHoriz(&ab, familyVp);
    Widget styleLabel = IswCreateManagedWidget("styleLabel", labelWidgetClass, new, ab.args, ab.count);

    /* Style list in a viewport */
    Dimension style_w = 120;
    Widget styleVp;
    IswArgBuilderReset(&ab);
    IswArgAllowVert(&ab, True);
    IswArgUseRight(&ab, True);
    IswArgWidth(&ab, style_w);
    IswArgHeight(&ab, list_h);
    IswArgFromHoriz(&ab, familyVp);
    IswArgFromVert(&ab, styleLabel);
    styleVp = IswCreateManagedWidget("styleViewport", viewportWidgetClass,
                                     new, ab.args, ab.count);

    /* Populate styles for the initial family */
    fcw->fontChooser.style_names   = NULL;
    fcw->fontChooser.style_weights = NULL;
    fcw->fontChooser.style_slants  = NULL;
    fcw->fontChooser.num_styles    = 0;
    fcw->fontChooser.styleListW    = NULL;
    RefreshStyles(fcw);

    IswArgBuilderReset(&ab);
    if (fcw->fontChooser.num_styles > 0) {
        IswArgList(&ab, fcw->fontChooser.style_names);
        IswArgNumberStrings(&ab, fcw->fontChooser.num_styles);
    }
    IswArgDefaultColumns(&ab, 1);
    IswArgForceColumns(&ab, True);
    fcw->fontChooser.styleListW = IswCreateManagedWidget(
        "styleList", listWidgetClass, styleVp, ab.args, ab.count);
    IswAddCallback(fcw->fontChooser.styleListW, IswNcallback,
                  StyleSelected, (IswPointer)fcw);

    /* Size label — positioned to the right of style viewport */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Size");
    IswArgBorderWidth(&ab, 0);
    IswArgFromHoriz(&ab, styleVp);
    IswCreateManagedWidget("sizeLabel", labelWidgetClass, new, ab.args, ab.count);

    /* Size list in a viewport */
    static String sizes[] = {
        "8", "9", "10", "11", "12", "14", "16", "18",
        "20", "24", "28", "32", "36", "48", "64", "72"
    };

    Widget sizeVp;
    IswArgBuilderReset(&ab);
    IswArgAllowVert(&ab, True);
    IswArgUseRight(&ab, True);
    IswArgWidth(&ab, size_w);
    IswArgHeight(&ab, list_h);
    IswArgFromHoriz(&ab, styleVp);
    IswArgFromVert(&ab, styleLabel);
    sizeVp = IswCreateManagedWidget("sizeViewport", viewportWidgetClass,
                                    new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgList(&ab, sizes);
    IswArgNumberStrings(&ab, IswNumber(sizes));
    IswArgDefaultColumns(&ab, 1);
    IswArgForceColumns(&ab, True);
    fcw->fontChooser.sizeListW = IswCreateManagedWidget(
        "sizeList", listWidgetClass, sizeVp, ab.args, ab.count);
    IswAddCallback(fcw->fontChooser.sizeListW, IswNcallback,
                  SizeSelected, (IswPointer)fcw);

    /* Preview label */
    char preview_buf[256];
    snprintf(preview_buf, sizeof(preview_buf), "%s %dpt: %s",
             fcw->fontChooser.family,
             fcw->fontChooser.size,
             fcw->fontChooser.preview_text);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, preview_buf);
    IswArgWidth(&ab, preview_w);
    IswArgHeight(&ab, preview_h);
    IswArgBorderWidth(&ab, 1);
    IswArgFromVert(&ab, familyVp);
    IswArgLeft(&ab, IswChainLeft);
    IswArgResize(&ab, False);
    fcw->fontChooser.previewW = IswCreateManagedWidget(
        "preview", labelWidgetClass, new, ab.args, ab.count);

    /* Reflect the initial family/style/size as highlighted rows. */
    for (int i = 0; i < fcw->fontChooser.num_families; i++) {
        if (strcmp(fcw->fontChooser.family_names[i],
                   fcw->fontChooser.family) == 0) {
            IswListHighlight(fcw->fontChooser.familyListW, i);
            break;
        }
    }
    for (int i = 0; i < fcw->fontChooser.num_styles; i++) {
        if (fcw->fontChooser.style_weights[i] == fcw->fontChooser.weight &&
            fcw->fontChooser.style_slants[i]  == fcw->fontChooser.slant) {
            IswListHighlight(fcw->fontChooser.styleListW, i);
            break;
        }
    }
    for (Cardinal i = 0; i < IswNumber(sizes); i++) {
        if (atoi(sizes[i]) == fcw->fontChooser.size) {
            IswListHighlight(fcw->fontChooser.sizeListW, i);
            break;
        }
    }
}

static void
Destroy(Widget w)
{
    FontChooserWidget fcw = (FontChooserWidget) w;
    FreeFontNames(fcw);
    FreeStyles(fcw);
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

int
IswFontChooserGetWeight(Widget w)
{
    return ((FontChooserWidget) w)->fontChooser.weight;
}

int
IswFontChooserGetSlant(Widget w)
{
    return ((FontChooserWidget) w)->fontChooser.slant;
}
