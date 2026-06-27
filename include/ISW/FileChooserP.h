/*
 * FileChooserP.h - Private definitions for FileChooser widget
 */

#ifndef _ISW_IswFileChooserP_h
#define _ISW_IswFileChooserP_h

#include <ISW/FileChooser.h>
#include <ISW/FormP.h>
#include <limits.h>

typedef struct {int empty;} FileChooserClassPart;

typedef struct _FileChooserClassRec {
    CoreClassPart           core_class;
    CompositeClassPart      composite_class;
    ConstraintClassPart     constraint_class;
    FormClassPart           form_class;
    FileChooserClassPart    fileChooser_class;
} FileChooserClassRec;

extern FileChooserClassRec fileChooserClassRec;

typedef struct {
    /* resources */
    int              mode;
    String           initial_directory;
    String           filter;
    IswCallbackList  file_selected;
    IswCallbackList  file_cancelled;

    /* private */
    Widget           pathTextW;
    Widget           dirListW;
    Widget           fileListW;
    Widget           nameTextW;
    Widget           filterTextW;
    Widget           actionBtnW;
    Widget           cancelBtnW;

    char             cwd[PATH_MAX];
    char           **dir_names;
    int              ndir;
    char           **file_names;
    int              nfile;
    char            *selected_path;
    char            *filter_owned;
} FileChooserPart;

typedef struct _FileChooserRec {
    CorePart            core;
    CompositePart       composite;
    ConstraintPart      constraint;
    FormPart            form;
    FileChooserPart     fileChooser;
} FileChooserRec;

typedef struct {int empty;} FileChooserConstraintsPart;

typedef struct _FileChooserConstraintsRec {
    FormConstraintsPart         form;
    FileChooserConstraintsPart  fileChooser;
} FileChooserConstraintsRec;

#endif /* _ISW_IswFileChooserP_h */
