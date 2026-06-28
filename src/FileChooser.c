#define _GNU_SOURCE
/*
 * FileChooser.c - FileChooser widget implementation
 *
 * A Form subclass with two-pane directory/file lists, path and filter
 * text fields, and OK/Cancel buttons.  Ported from ISDE's procedural
 * isde-filechooser into a proper widget.
 */

#include <ISW/ISWP.h>
#include <ISW/IntrinsicP.h>
#include <ISW/StringDefs.h>
#include <ISW/ISWInit.h>
#include <ISW/FileChooserP.h>
#include <ISW/Label.h>
#include <ISW/Command.h>
#include <ISW/List.h>
#include <ISW/Viewport.h>
#include <ISW/Text.h>
#include <ISW/ComboBox.h>
#include <ISW/IswArgMacros.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

#define superclass (&formClassRec)

#define Offset(field) IswOffsetOf(FileChooserRec, field)

static Boolean CvtStringToFileChooserMode(IswDisplay, IswValuePtr, Cardinal*,
                                          IswValuePtr, IswValuePtr, IswPointer*);

static IswResource resources[] = {
    {IswNfileMode, IswCFileMode, IswRFileChooserMode, sizeof(int),
        Offset(fileChooser.mode), IswRImmediate, (IswPointer) IswFileOpen},
    {IswNinitialDirectory, IswCInitialDirectory, IswRString, sizeof(String),
        Offset(fileChooser.initial_directory), IswRString, NULL},
    {IswNfileFilters, IswCFileFilters, IswRPointer, sizeof(IswFileFilter *),
        Offset(fileChooser.filters), IswRPointer, NULL},
    {IswNnumFileFilters, IswCNumFileFilters, IswRInt, sizeof(int),
        Offset(fileChooser.num_filters), IswRImmediate, (IswPointer) 0},
    {IswNfileSelected, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(fileChooser.file_selected), IswRCallback, NULL},
    {IswNfileCancelled, IswCCallback, IswRCallback, sizeof(IswPointer),
        Offset(fileChooser.file_cancelled), IswRCallback, NULL},
    {IswNdefaultDistance, IswCThickness, IswRInt, sizeof(int),
        Offset(form.default_spacing), IswRImmediate, (IswPointer) 8},
    {IswNborderWidth, IswCBorderWidth, IswRDimension, sizeof(Dimension),
        Offset(core.border_width), IswRImmediate, (IswPointer) 0},
    {IswNwidth, IswCWidth, IswRDimension, sizeof(Dimension),
        Offset(core.width), IswRImmediate, (IswPointer) 500},
    {IswNheight, IswCHeight, IswRDimension, sizeof(Dimension),
        Offset(core.height), IswRImmediate, (IswPointer) 400},
};

#undef Offset

static void ClassInitialize(void);
static void Initialize(Widget, Widget, ArgList, Cardinal *);
static void Destroy(Widget);
static Boolean SetValues(Widget, Widget, Widget, ArgList, Cardinal *);

static void PathGoAction(Widget, IswEvent *, String *, Cardinal *);

static IswActionsRec actions[] = {
    {"fc-path-go",   PathGoAction},
};

FileChooserClassRec fileChooserClassRec = {
  { /* core */
    (WidgetClass) superclass,
    "FileChooser",
    sizeof(FileChooserRec),
    ClassInitialize,
    NULL,
    FALSE,
    Initialize,
    NULL,
    IswInheritRealize,
    actions,
    IswNumber(actions),
    resources,
    IswNumber(resources),
    ISW_NULLQUARK,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    Destroy,
    IswInheritResize,
    IswInheritExpose,
    SetValues,
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
    sizeof(FileChooserConstraintsRec),
    NULL, NULL, NULL, NULL
  },
  { /* form */
    IswInheritLayout
  },
  { /* fileChooser */
    0
  }
};

WidgetClass fileChooserWidgetClass = (WidgetClass)&fileChooserClassRec;

/* ================================================================
 * Type converter
 * ================================================================ */

static Boolean
CvtStringToFileChooserMode(IswDisplay dpy, IswValuePtr args,
                           Cardinal *num_args, IswValuePtr from,
                           IswValuePtr to, IswPointer *data)
{
    static int result;
    String s = (String) from->addr;
    (void)args; (void)num_args; (void)data;

    if (strcasecmp(s, "open") == 0 || strcasecmp(s, "IswFileOpen") == 0)
        result = IswFileOpen;
    else if (strcasecmp(s, "save") == 0 || strcasecmp(s, "IswFileSave") == 0)
        result = IswFileSave;
    else {
        IswDisplayStringConversionWarning(dpy, s, IswRFileChooserMode);
        return False;
    }

    if (to->addr != NULL) {
        if (to->size < sizeof(int)) {
            to->size = sizeof(int);
            return False;
        }
        *(int *)to->addr = result;
    } else {
        to->addr = (IswPointer)&result;
    }
    to->size = sizeof(int);
    return True;
}

/* ================================================================
 * Directory scanning
 * ================================================================ */

static int
str_compare(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static void
free_string_array(char ***arr, int *count)
{
    if (*arr) {
        for (int i = 0; i < *count; i++)
            free((*arr)[i]);
        free(*arr);
        *arr = NULL;
    }
    *count = 0;
}

static void
scan_directory(FileChooserWidget fcw)
{
    FileChooserPart *fc = &fcw->fileChooser;

    free_string_array(&fc->dir_names, &fc->ndir);
    free_string_array(&fc->file_names, &fc->nfile);

    DIR *d = opendir(fc->cwd);
    if (!d) return;

    int dir_cap = 32, file_cap = 64;
    fc->dir_names = malloc(dir_cap * sizeof(char *));
    fc->file_names = malloc(file_cap * sizeof(char *));

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;

        if (name[0] == '.' && name[1] != '\0' &&
            !(name[1] == '.' && name[2] == '\0'))
            continue;

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", fc->cwd, name);

        struct stat st;
        if (stat(fullpath, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            if (fc->ndir >= dir_cap) {
                dir_cap *= 2;
                fc->dir_names = realloc(fc->dir_names,
                                        dir_cap * sizeof(char *));
            }
            fc->dir_names[fc->ndir++] = strdup(name);
        } else if (S_ISREG(st.st_mode)) {
            if (fc->filters && fc->active_filter >= 0 &&
                fc->active_filter < fc->num_filters) {
                const char *pat = fc->filters[fc->active_filter].pattern;
                if (pat && pat[0] && strcmp(pat, "*") != 0 &&
                    fnmatch(pat, name, 0) != 0)
                    continue;
            }

            if (fc->nfile >= file_cap) {
                file_cap *= 2;
                fc->file_names = realloc(fc->file_names,
                                         file_cap * sizeof(char *));
            }
            fc->file_names[fc->nfile++] = strdup(name);
        }
    }
    closedir(d);

    if (fc->ndir > 0)
        qsort(fc->dir_names, fc->ndir, sizeof(char *), str_compare);
    if (fc->nfile > 0)
        qsort(fc->file_names, fc->nfile, sizeof(char *), str_compare);
}

static void
update_lists(FileChooserWidget fcw)
{
    FileChooserPart *fc = &fcw->fileChooser;

    scan_directory(fcw);

    if (fc->dirListW)
        IswListChange(fc->dirListW,
                      fc->ndir > 0 ? (String *)fc->dir_names : NULL,
                      fc->ndir, 0, True);

    if (fc->fileListW)
        IswListChange(fc->fileListW,
                      fc->nfile > 0 ? (String *)fc->file_names : NULL,
                      fc->nfile, 0, True);

    if (fc->pathTextW) {
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgString(&ab, fc->cwd);
        IswSetValues(fc->pathTextW, ab.args, ab.count);
    }
}

/* ================================================================
 * Navigation
 * ================================================================ */

static void
navigate_to(FileChooserWidget fcw, const char *path)
{
    char resolved[PATH_MAX];
    if (!realpath(path, resolved))
        return;

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode))
        return;

    snprintf(fcw->fileChooser.cwd, sizeof(fcw->fileChooser.cwd),
             "%s", resolved);
    update_lists(fcw);
}

/* ================================================================
 * Selection / accept / cancel
 * ================================================================ */

static void
fc_accept(FileChooserWidget fcw, const char *filename)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", fcw->fileChooser.cwd, filename);

    free(fcw->fileChooser.selected_path);
    fcw->fileChooser.selected_path = strdup(path);

    IswFileChooserCallbackData cb;
    cb.path = fcw->fileChooser.selected_path;
    IswCallCallbacks((Widget)fcw, IswNfileSelected, (IswPointer)&cb);
}

static void
dir_select_cb(Widget w, IswPointer cd, IswPointer call)
{
    (void)w;
    FileChooserWidget fcw = (FileChooserWidget)cd;
    IswListReturnStruct *ret = (IswListReturnStruct *)call;
    if (!ret || !ret->string) return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s",
             fcw->fileChooser.cwd, ret->string);
    navigate_to(fcw, path);
}

static void
file_select_cb(Widget w, IswPointer cd, IswPointer call)
{
    (void)w;
    FileChooserWidget fcw = (FileChooserWidget)cd;
    IswListReturnStruct *ret = (IswListReturnStruct *)call;
    if (!ret || !ret->string) return;

    if (fcw->fileChooser.mode == IswFileSave && fcw->fileChooser.nameTextW) {
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgString(&ab, ret->string);
        IswSetValues(fcw->fileChooser.nameTextW, ab.args, ab.count);
    } else {
        fc_accept(fcw, ret->string);
    }
}

static void
action_cb(Widget w, IswPointer cd, IswPointer call)
{
    (void)w; (void)call;
    FileChooserWidget fcw = (FileChooserWidget)cd;

    if (fcw->fileChooser.mode == IswFileSave && fcw->fileChooser.nameTextW) {
        String val = NULL;
        IswArgBuilder ab = IswArgBuilderInit();
        IswArgString(&ab, &val);
        IswGetValues(fcw->fileChooser.nameTextW, ab.args, ab.count);
        if (val && val[0]) {
            fc_accept(fcw, val);
            return;
        }
    } else {
        IswListReturnStruct *cur = IswListShowCurrent(fcw->fileChooser.fileListW);
        if (cur && cur->list_index != XAW_LIST_NONE && cur->string) {
            fc_accept(fcw, cur->string);
            return;
        }
    }
}

static void
cancel_cb(Widget w, IswPointer cd, IswPointer call)
{
    (void)call;
    FileChooserWidget fcw = (FileChooserWidget)cd;
    fprintf(stderr, "FC cancel_cb: triggered by widget '%s'\n",
            IswName(w));
    IswCallCallbacks((Widget)fcw, IswNfileCancelled, NULL);
}

/* ================================================================
 * Action procedures (text field Enter key)
 * ================================================================ */

static FileChooserWidget
find_filechooser_ancestor(Widget w)
{
    while (w) {
        if (IswIsSubclass(w, fileChooserWidgetClass))
            return (FileChooserWidget) w;
        w = IswParent(w);
    }
    return NULL;
}

static void
PathGoAction(Widget w, IswEvent *ev, String *params, Cardinal *nparams)
{
    (void)ev; (void)params; (void)nparams;
    FileChooserWidget fcw = find_filechooser_ancestor(w);
    if (!fcw) return;

    String val = NULL;
    IswArgBuilder ab = IswArgBuilderInit();
    IswArgString(&ab, &val);
    IswGetValues(w, ab.args, ab.count);
    if (val && val[0])
        navigate_to(fcw, val);
}

static void
filter_select_cb(Widget w, IswPointer cd, IswPointer call)
{
    (void)w;
    FileChooserWidget fcw = (FileChooserWidget)cd;
    IswListReturnStruct *ret = (IswListReturnStruct *)call;
    if (!ret) return;

    fprintf(stderr, "FC filter_select_cb: index=%d\n", ret->list_index);
    fcw->fileChooser.active_filter = ret->list_index;
    update_lists(fcw);
}

/* ================================================================
 * Widget methods
 * ================================================================ */

static void
ClassInitialize(void)
{
    IswInitializeWidgetSet();
    IswSetTypeConverter(IswRString, IswRFileChooserMode,
                        CvtStringToFileChooserMode,
                        NULL, 0, IswCacheNone, NULL);
}

static void
Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    FileChooserWidget fcw = (FileChooserWidget) new;
    FileChooserPart *fc = &fcw->fileChooser;
    IswArgBuilder ab = IswArgBuilderInit();
    (void)request; (void)args; (void)num_args;

    fc->dir_names = NULL;
    fc->ndir = 0;
    fc->file_names = NULL;
    fc->nfile = 0;
    fc->selected_path = NULL;

    fc->active_filter = 0;
    fc->filter_labels = NULL;
    if (fc->filters && fc->num_filters > 0) {
        fc->filter_labels = malloc(fc->num_filters * sizeof(String));
        for (int i = 0; i < fc->num_filters; i++)
            fc->filter_labels[i] = fc->filters[i].label;
    }

    /* Resolve initial directory */
    if (fc->initial_directory && fc->initial_directory[0]) {
        char resolved[PATH_MAX];
        if (realpath(fc->initial_directory, resolved))
            snprintf(fc->cwd, sizeof(fc->cwd), "%s", resolved);
        else
            snprintf(fc->cwd, sizeof(fc->cwd), "%s", getenv("HOME"));
    } else {
        snprintf(fc->cwd, sizeof(fc->cwd), "%s", getenv("HOME"));
    }

    /* --- Row 1: Location label + path text --- */
    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Location:");
    IswArgBorderWidth(&ab, 0);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    Widget loc_label = IswCreateManagedWidget("locLabel", labelWidgetClass,
                                              new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgString(&ab, fc->cwd);
    IswArgFromHoriz(&ab, loc_label);
    IswArgEditType(&ab, IswtextEdit);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainTop);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainRight);
    IswArgResize(&ab, 1);
    IswArgWidth(&ab, 300);
    fc->pathTextW = IswCreateManagedWidget("pathText", textWidgetClass,
                                           new, ab.args, ab.count);
    IswOverrideTranslations(fc->pathTextW, IswParseTranslationTable(
        "<Key>Return: fc-path-go()\n"));

    /* --- Row 2: Directory + File panes --- */

    Dimension pane_h = 250;
    Dimension dir_w  = 160;

    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, loc_label);
    IswArgAllowVert(&ab, True);
    IswArgUseRight(&ab, True);
    IswArgForceBars(&ab, True);
    IswArgWidth(&ab, dir_w);
    IswArgHeight(&ab, pane_h);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainBottom);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    Widget dir_vp = IswCreateManagedWidget("dirViewport", viewportWidgetClass,
                                           new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgVerticalList(&ab, True);
    IswArgForceColumns(&ab, True);
    IswArgDefaultColumns(&ab, 1);
    IswArgBorderWidth(&ab, 0);
    fc->dirListW = IswCreateManagedWidget("dirList", listWidgetClass,
                                          dir_vp, ab.args, ab.count);
    IswOverrideTranslations(fc->dirListW, IswParseTranslationTable(
        "<Btn1Down>: Set()\n"
        "<Btn1Up>: \n"
        "<Btn1Up>(2): Notify()\n"));
    IswAddCallback(fc->dirListW, IswNcallback, dir_select_cb, (IswPointer)fcw);

    IswArgBuilderReset(&ab);
    IswArgFromVert(&ab, loc_label);
    IswArgFromHoriz(&ab, dir_vp);
    IswArgAllowVert(&ab, True);
    IswArgUseRight(&ab, True);
    IswArgForceBars(&ab, True);
    IswArgHeight(&ab, pane_h);
    IswArgTop(&ab, IswChainTop);
    IswArgBottom(&ab, IswChainBottom);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainRight);
    Widget file_vp = IswCreateManagedWidget("fileViewport", viewportWidgetClass,
                                            new, ab.args, ab.count);

    IswArgBuilderReset(&ab);
    IswArgVerticalList(&ab, True);
    IswArgForceColumns(&ab, True);
    IswArgDefaultColumns(&ab, 1);
    IswArgBorderWidth(&ab, 0);
    fc->fileListW = IswCreateManagedWidget("fileList", listWidgetClass,
                                           file_vp, ab.args, ab.count);
    IswOverrideTranslations(fc->fileListW, IswParseTranslationTable(
        "<Btn1Down>: Set()\n"
        "<Btn1Up>: \n"
        "<Btn1Up>(2): Notify()\n"));
    IswAddCallback(fc->fileListW, IswNcallback, file_select_cb, (IswPointer)fcw);

    Widget bottom_anchor = dir_vp;

    /* --- Row 3 (SAVE mode only): Filename --- */
    fc->nameTextW = NULL;
    if (fc->mode == IswFileSave) {
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, "Name:");
        IswArgBorderWidth(&ab, 0);
        IswArgFromVert(&ab, dir_vp);
        IswArgTop(&ab, IswChainBottom);
        IswArgBottom(&ab, IswChainBottom);
        IswArgLeft(&ab, IswChainLeft);
        IswArgRight(&ab, IswChainLeft);
        Widget name_label = IswCreateManagedWidget("nameLabel",
                                                    labelWidgetClass,
                                                    new, ab.args, ab.count);

        IswArgBuilderReset(&ab);
        IswArgString(&ab, "");
        IswArgFromVert(&ab, dir_vp);
        IswArgFromHoriz(&ab, name_label);
        IswArgEditType(&ab, IswtextEdit);
        IswArgTop(&ab, IswChainBottom);
        IswArgBottom(&ab, IswChainBottom);
        IswArgLeft(&ab, IswChainLeft);
        IswArgRight(&ab, IswChainRight);
        fc->nameTextW = IswCreateManagedWidget("nameText",
                                                textWidgetClass,
                                                new, ab.args, ab.count);
        bottom_anchor = name_label;
    }

    /* --- Row 4: Filter dropdown --- */
    Widget filter_label = NULL;
    fc->filterComboW = NULL;
    if (fc->filters && fc->num_filters > 0) {
        IswArgBuilderReset(&ab);
        IswArgLabel(&ab, "Filter:");
        IswArgBorderWidth(&ab, 0);
        IswArgFromVert(&ab, bottom_anchor);
        IswArgTop(&ab, IswChainBottom);
        IswArgBottom(&ab, IswChainBottom);
        IswArgLeft(&ab, IswChainLeft);
        IswArgRight(&ab, IswChainLeft);
        filter_label = IswCreateManagedWidget("filterLabel",
                                              labelWidgetClass,
                                              new, ab.args, ab.count);

        IswArgBuilderReset(&ab);
        IswArgDropdownMode(&ab, True);
        IswArgList(&ab, fc->filter_labels);
        IswArgNumberStrings(&ab, fc->num_filters);
        IswArgFromVert(&ab, bottom_anchor);
        IswArgFromHoriz(&ab, filter_label);
        IswArgTop(&ab, IswChainBottom);
        IswArgBottom(&ab, IswChainBottom);
        IswArgLeft(&ab, IswChainLeft);
        IswArgRight(&ab, IswChainRight);
        IswArgWidth(&ab, 300);
        fc->filterComboW = IswCreateManagedWidget("filterCombo",
                                                   comboBoxWidgetClass,
                                                   new, ab.args, ab.count);
        IswAddCallback(fc->filterComboW, IswNcallback, filter_select_cb,
                       (IswPointer)fcw);
    }

    /* --- Row 5: Buttons --- */
    Widget btn_anchor = filter_label ? filter_label : bottom_anchor;
    const char *action_label = (fc->mode == IswFileSave) ? "Save" : "Open";

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, action_label);
    IswArgFromVert(&ab, btn_anchor);
    IswArgTop(&ab, IswChainBottom);
    IswArgBottom(&ab, IswChainBottom);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    fc->actionBtnW = IswCreateManagedWidget("actionButton",
                                             commandWidgetClass,
                                             new, ab.args, ab.count);
    IswAddCallback(fc->actionBtnW, IswNcallback, action_cb, (IswPointer)fcw);

    IswArgBuilderReset(&ab);
    IswArgLabel(&ab, "Cancel");
    IswArgFromVert(&ab, btn_anchor);
    IswArgFromHoriz(&ab, fc->actionBtnW);
    IswArgTop(&ab, IswChainBottom);
    IswArgBottom(&ab, IswChainBottom);
    IswArgLeft(&ab, IswChainLeft);
    IswArgRight(&ab, IswChainLeft);
    fc->cancelBtnW = IswCreateManagedWidget("cancelButton",
                                             commandWidgetClass,
                                             new, ab.args, ab.count);
    IswAddCallback(fc->cancelBtnW, IswNcallback, cancel_cb, (IswPointer)fcw);

    /* Initial scan */
    scan_directory(fcw);
    if (fc->ndir > 0)
        IswListChange(fc->dirListW, (String *)fc->dir_names, fc->ndir, 0, True);
    if (fc->nfile > 0)
        IswListChange(fc->fileListW, (String *)fc->file_names, fc->nfile, 0, True);
}

static void
Destroy(Widget w)
{
    FileChooserWidget fcw = (FileChooserWidget) w;
    free_string_array(&fcw->fileChooser.dir_names, &fcw->fileChooser.ndir);
    free_string_array(&fcw->fileChooser.file_names, &fcw->fileChooser.nfile);
    free(fcw->fileChooser.selected_path);
    free(fcw->fileChooser.filter_labels);
}

static Boolean
SetValues(Widget current, Widget request, Widget new,
          ArgList in_args, Cardinal *in_num_args)
{
    FileChooserWidget old_fcw = (FileChooserWidget) current;
    FileChooserWidget fcw = (FileChooserWidget) new;
    (void)request;
    (void)in_args; (void)in_num_args;

    if (fcw->fileChooser.filters != old_fcw->fileChooser.filters ||
        fcw->fileChooser.num_filters != old_fcw->fileChooser.num_filters) {
        free(fcw->fileChooser.filter_labels);
        fcw->fileChooser.filter_labels = NULL;
        if (fcw->fileChooser.filters && fcw->fileChooser.num_filters > 0) {
            fcw->fileChooser.filter_labels =
                malloc(fcw->fileChooser.num_filters * sizeof(String));
            for (int i = 0; i < fcw->fileChooser.num_filters; i++)
                fcw->fileChooser.filter_labels[i] =
                    fcw->fileChooser.filters[i].label;
            if (fcw->fileChooser.filterComboW)
                IswListChange(fcw->fileChooser.filterComboW,
                              fcw->fileChooser.filter_labels,
                              fcw->fileChooser.num_filters, 0, True);
        }
        fcw->fileChooser.active_filter = 0;
        update_lists(fcw);
    }

    if (fcw->fileChooser.initial_directory !=
        old_fcw->fileChooser.initial_directory) {
        if (fcw->fileChooser.initial_directory &&
            fcw->fileChooser.initial_directory[0])
            navigate_to(fcw, fcw->fileChooser.initial_directory);
    }

    return False;
}

/* ================================================================
 * Public API
 * ================================================================ */

String
IswFileChooserGetPath(Widget w)
{
    return ((FileChooserWidget) w)->fileChooser.selected_path;
}
