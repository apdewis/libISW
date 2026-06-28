/*
 * FileChooser.h - Public header for FileChooser widget
 *
 * A file selection widget with directory list, file list, filter,
 * and optional filename entry for save mode.
 */

#ifndef _ISW_IswFileChooser_h
#define _ISW_IswFileChooser_h

#include <ISW/Form.h>

#define IswNfileMode        "fileMode"
#define IswCFileMode        "FileMode"
#define IswNinitialDirectory "initialDirectory"
#define IswCInitialDirectory "InitialDirectory"
#define IswNfileFilters     "fileFilters"
#define IswCFileFilters     "FileFilters"
#define IswNnumFileFilters  "numFileFilters"
#define IswCNumFileFilters  "NumFileFilters"

typedef struct {
    String label;
    String pattern;
} IswFileFilter;
#define IswNfileSelected    "fileSelected"
#define IswNfileCancelled   "fileCancelled"

typedef enum {
    IswFileOpen,
    IswFileSave,
} IswFileChooserMode;

#define IswRFileChooserMode "FileChooserMode"

typedef struct {
    String path;
} IswFileChooserCallbackData;

extern WidgetClass fileChooserWidgetClass;

typedef struct _FileChooserClassRec *FileChooserWidgetClass;
typedef struct _FileChooserRec      *FileChooserWidget;

_XFUNCPROTOBEGIN

extern String IswFileChooserGetPath(Widget w);

_XFUNCPROTOEND

#endif /* _ISW_IswFileChooser_h */
