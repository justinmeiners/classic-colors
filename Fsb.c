/*
 * Copyright 2021 Olaf Wintermann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include "Fsb.h"
#include "FsbP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>

#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <fnmatch.h>

#include <Xm/XmAll.h>
#include <Xm/DropDown.h>

#ifdef FSB_ENABLE_DETAIL
#include <XmL/Grid.h>
#endif

#define WIDGET_SPACING 5
#define WINDOW_SPACING 8

#define BUTTON_EXTRA_SPACE 4

#define DATE_FORMAT_SAME_YEAR  "%b %d %H:%M"
#define DATE_FORMAT_OTHER_YEAR "%b %d  %Y"

#define KB_SUFFIX "KiB"
#define MB_SUFFIX "MiB"
#define GB_SUFFIX "GiB"
#define TB_SUFFIX "TiB"

#define FSB_ERROR_TITLE          "Error"
#define FSB_ERROR_CHAR           "Character '/' is not allowed in file names"
#define FSB_ERROR_RENAME         "Cannot rename file: %s"
#define FSB_ERROR_DELETE         "Cannot delete file: %s"
#define FSB_ERROR_CREATE_FOLDER  "Cannot create folder: %s"
#define FSB_ERROR_OPEN_DIR       "Cannot open directory: %s"

#define FSB_DETAIL_HEADINGS "Name|Size|Last Modified"

static void fsb_class_init(void);
static void fsb_class_part_init (WidgetClass wc);
static void fsb_init(Widget request, Widget neww, ArgList args, Cardinal *num_args);
static void fsb_resize(Widget widget);
static void fsb_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes);
static void fsb_destroy(Widget widget);
static Boolean fsb_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args);
static Boolean fsb_acceptfocus(Widget widget, Time *time);

static void fsb_insert_child(Widget child);

static void fsb_mapcb(Widget widget, XtPointer u, XtPointer cb);

static int FSBGlobFilter(const char *a, const char *b);

static void FSBUpdateTitle(Widget w);

static void FocusInAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void ErrDialog(XnFileSelectionBox w, const char *title, const char *errmsg);

static void FSBRename(XnFileSelectionBox fsb, const char *path);
static void FSBDelete(XnFileSelectionBox fsb, const char *path);

static void FSBSelectItem(XnFileSelectionBox fsb, const char *item);

static char* set_selected_path(XnFileSelectionBox data, XmString item);

static void FileContextMenuCB(Widget item, XtPointer index, XtPointer cd);

static Widget CreateContextMenu(XnFileSelectionBox fsb, Widget parent, XtCallbackProc callback);

static void FileListUpdate(Widget fsb, Widget view, FileElm *dirlist, int dircount, FileElm *files, int filecount, const char *filter, int maxnamelen, void *userData);
static void FileListSelect(Widget fsb, Widget view, const char *item);
static void FileListCleanup(Widget fsb, Widget view, void *userData);
static void FileListDestroy(Widget fsb, Widget view, void *userData);

static void FileListActivateCB(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb);
static void FileListSelectCB(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb);

static void FileListWidgetAdd(XnFileSelectionBox fsb, Widget w, int showHidden, const char *filter, FileElm *ls, int count);

#ifdef FSB_ENABLE_DETAIL
static void FileListDetailUpdate(Widget fsb, Widget view, FileElm *dirlist, int dircount, FileElm *files, int filecount, const char *filter, int maxnamelen, void *userData);
static void FileListDetailSelect(Widget fsb, Widget view, const char *item);
static void FileListDetailCleanup(Widget fsb, Widget view, void *userData);
static void FileListDetailDestroy(Widget fsb, Widget view, void *userData);
static void FileListDetailAdjustColWidth(Widget grid);
static void FileListDetailAdd(XnFileSelectionBox fsb, Widget grid, int showHidden, const char *filter, FileElm *ls, int count, int maxWidth);
#endif
static void FSBNewFolder(Widget w, XnFileSelectionBox data, XtPointer u);

static void FSBHome(Widget w, XnFileSelectionBox data, XtPointer u);

static void FileSelectionCallback(XnFileSelectionBox fsb, XtCallbackList cb, int reason, const char *value);

static void CreateUI(XnFileSelectionBox w);
static FSBViewWidgets CreateView(XnFileSelectionBox w, FSBViewCreateProc createProc, void *userData, Boolean useDirList);
static void AddViewMenuItem(XnFileSelectionBox w, const char *name, int viewIndex);
static void SelectView(XnFileSelectionBox f, int view);

static char* FSBDialogTitle(Widget w);

static FSBViewWidgets CreateListView(Widget fsb, ArgList args, int n, void *userData);
static FSBViewWidgets CreateDetailView(Widget fsb, ArgList args, int n, void *userData);

static const char* GetHomeDir(void);

static char* ConcatPath(const char *parent, const char *name);
static char* FileName(char *path);
static char* ParentPath(const char *path);
//static int   CheckFileName(const char *name);

static int filedialog_update_dir(XnFileSelectionBox data, const char *path);
static void filedialog_cleanup_filedata(XnFileSelectionBox data);

static void pathbar_resize(Widget w, PathBar *p, XtPointer d);

static XtResource resources[] = {
    {XmNokCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(XnFileSelectionBox, fsb.okCallback), XmRCallback, NULL},
    {XmNcancelCallback, XmCCallback, XmRCallback, sizeof(XtCallbackList), XtOffset(XnFileSelectionBox, fsb.cancelCallback), XmRCallback, NULL},
    {XnNwidgetSpacing, XmCSpacing, XmRDimension, sizeof(Dimension), XtOffset(XnFileSelectionBox, fsb.widgetSpacing), XmRImmediate, (XtPointer)WIDGET_SPACING},
    {XnNwindowSpacing, XmCSpacing, XmRDimension, sizeof(Dimension), XtOffset(XnFileSelectionBox, fsb.windowSpacing), XmRImmediate, (XtPointer)WINDOW_SPACING},
    {XnNfsbType, XnCfsbType, XmRInt, sizeof(int), XtOffset(XnFileSelectionBox, fsb.type), XmRImmediate, (XtPointer)FILEDIALOG_OPEN},
    {XnNshowHidden, XnCshowHidden, XmRBoolean, sizeof(Boolean), XtOffset(XnFileSelectionBox, fsb.showHidden), XmRImmediate, (XtPointer)False},
    {XnNshowHiddenButton, XnCshowHiddenButton, XmRBoolean, sizeof(Boolean), XtOffset(XnFileSelectionBox, fsb.showHiddenButton), XmRImmediate, (XtPointer)True},
    {XnNshowViewMenu, XnCshowViewMenu, XmRBoolean, sizeof(Boolean), XtOffset(XnFileSelectionBox, fsb.showViewMenu), XmRImmediate, (XtPointer)False},
    {XnNselectedView, XnCselectedView, XmRInt, sizeof(int), XtOffset(XnFileSelectionBox, fsb.selectedview), XmRImmediate, (XtPointer)0},
    
    {XnNdirectory, XnCdirectory, XmRString, sizeof(XmString), XtOffset(XnFileSelectionBox, fsb.currentPath), XmRString, NULL},
    {XnNselectedPath, XnCselectedPath, XmRString, sizeof(XmString), XtOffset(XnFileSelectionBox, fsb.selectedPath), XmRString, NULL},
    {XnNhomePath, XnChomePath, XmRString, sizeof(XmString), XtOffset(XnFileSelectionBox, fsb.homePath), XmRString, NULL},
    
    {XnNfilter,XnCfilter,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.filterStr), XmRString, "*"},
    {XnNfilterFunc,XnCfilterFunc,XmRFunction,sizeof(FSBFilterFunc),XtOffset(XnFileSelectionBox, fsb.filterFunc), XmRFunction, NULL},
    
    {XnNlabelListView,XnClabelListView,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelListView), XmRString, "List"},
    {XnNlabelDetailView,XnClabelDetailView,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDetailView), XmRString, "Detail"},
    {XnNlabelOpenFileTitle,XnClabelOpenFileTitle,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelOpenFileTitle), XmRString, "Open File"},
    {XnNlabelSaveFileTitle,XnClabelSaveFileTitle,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelSaveFileTitle), XmRString, "Save File"},
    {XnNlabelDirUp,XnClabelDirUp,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDirUp), XmRString, "Dir Up"},
    {XnNlabelHome,XnClabelHome,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelHome), XmRString, "Home"},
    {XnNlabelNewFolder,XnClabelNewFolder,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelNewFolder), XmRString, "New Folder"},
    {XnNlabelFilterButton,XnClabelFilterButton,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelFilterButton), XmRString, "Filter"},
    {XnNlabelShowHiddenFiles,XnClabelShowHiddenFiles,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelShowHiddenFiles), XmRString, "Show hiden files"},
    {XnNlabelDirectories,XnClabelDirectories,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDirectories), XmRString, "Directories"},
    {XnNlabelFiles,XnClabelFiles,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelFiles), XmRString, "Files"},
    {XnNlabelRename,XnClabelRename,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelRename), XmRString, "Rename"},
    {XnNlabelDelete,XnClabelDelete,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDelete), XmRString, "Delete"},
    {XnNlabelOpen,XnClabelOpen,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelOpen), XmRString, "Open"},
    {XnNlabelSave,XnClabelSave,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelSave), XmRString, "Save"},
    {XnNlabelOk,XnClabelOk,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelOk), XmRString, "OK"},
    {XnNlabelCancel,XnClabelCancel,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelCancel), XmRString, "Cancel"},
    {XnNlabelHelp,XnClabelHelp,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelHelp), XmRString, "Help"},
    {XnNlabelFileName,XnClabelFileName,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelFileName), XmRString, "New File Name"},
    {XnNlabelDirectoryName,XnClabelDirectoryName,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDirectoryName), XmRString, "Directory name:"},
    {XnNlabelNewFileName,XnClabelNewFileName,XmRXmString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelNewFileName), XmRString, "New file name:"},
    {XnNlabelDeleteFile,XnClabelDeleteFile,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.labelDeleteFile), XmRString, "Delete file '%s'?"},
    {XnNdetailHeadings,XnCdetailHeadings,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.detailHeadings), XmRString,FSB_DETAIL_HEADINGS},
    {XnNdateFormatSameYear,XnCdateFormatSameYear,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.dateFormatSameYear), XmRString,DATE_FORMAT_SAME_YEAR},
    {XnNdateFormatOtherYear,XnNdateFormatOtherYear,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.dateFormatOtherYear), XmRString,DATE_FORMAT_OTHER_YEAR},
    {XnNsuffixBytes,XnCsuffixBytes,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.suffixBytes), XmRString,"bytes"},
    {XnNsuffixKB,XnCsuffixKB,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.suffixKB), XmRString,KB_SUFFIX},
    {XnNsuffixMB,XnCsuffixKB,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.suffixMB), XmRString,MB_SUFFIX},
    {XnNsuffixGB,XnCsuffixKB,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.suffixGB), XmRString,GB_SUFFIX},
    {XnNsuffixTB,XnCsuffixKB,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.suffixTB), XmRString,TB_SUFFIX},
    
    {XnNerrorTitle,XnCerrorTitle,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorTitle), XmRString,FSB_ERROR_TITLE},
    {XnNerrorIllegalChar,XnCerrorIllegalChar,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorIllegalChar), XmRString,FSB_ERROR_CHAR},
    {XnNerrorRename,XnCerrorRename,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorRename), XmRString,FSB_ERROR_RENAME},
    {XnNerrorCreateFolder,XnCerrorCreateFolder,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorFolder), XmRString,FSB_ERROR_CREATE_FOLDER},
    {XnNerrorDelete,XnCerrorDelete,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorDelete), XmRString,FSB_ERROR_DELETE},
    {XnNerrorOpenDir,XnCerrorOpenDir,XmRString,sizeof(XmString),XtOffset(XnFileSelectionBox, fsb.errorOpenDir), XmRString,FSB_ERROR_OPEN_DIR}
};

static XtActionsRec actionslist[] = {
  {"focusIn", FocusInAP},
  {"NULL", NULL}
};


static char defaultTranslations[] = "<FocusIn>:		        focusIn()";

static XtResource constraints[] = {};

FSBClassRec fsbWidgetClassRec = {
    // Core Class
    {
        (WidgetClass)&xmFormClassRec,
        "XnFSB",                         // class_name
        sizeof(FSBRec),                  // widget_size
        fsb_class_init,                  // class_initialize
        fsb_class_part_init,             // class_part_initialize
        FALSE,                           // class_inited
        fsb_init,                        // initialize
        NULL,                            // initialize_hook
        fsb_realize,                     // realize
        actionslist,                     // actions
        XtNumber(actionslist),           // num_actions
        resources,                       // resources
        XtNumber(resources),             // num_resources
        NULLQUARK,                       // xrm_class
        True,                            // compress_motion
        True,                            // compress_exposure
        True,                            // compress_enterleave
        False,                           // visible_interest
        fsb_destroy,                     // destroy
        fsb_resize,                      // resize
        XtInheritExpose,                 // expose
        fsb_set_values,                  // set_values
        NULL,                            // set_values_hook
        XtInheritSetValuesAlmost,        // set_values_almost
        NULL,                            // get_values_hook
        fsb_acceptfocus,                 // accept_focus
        XtVersion,                       // version
        NULL,                            // callback_offsets
        defaultTranslations,             // tm_table
        XtInheritQueryGeometry,          // query_geometry
        XtInheritDisplayAccelerator,     // display_accelerator
        NULL,                            // extension
    },
    // Composite Class
    {
        XtInheritGeometryManager, // geometry_manager 
        XtInheritChangeManaged,   // change_managed
        fsb_insert_child,         // insert_child 
        XtInheritDeleteChild,     // delete_child  
        NULL,                     // extension   
    },
    // Constraint Class
    {
        constraints,                 // resources
        XtNumber(constraints),       // num_resources   
        sizeof(XmFormConstraintRec), // constraint_size  
        NULL,                        // initialize 
        NULL,                        // destroy
        NULL,                        // set_value
        NULL,                        // extension 
    },
    // XmManager Class
    {
        XtInheritTranslations, // translations
        NULL, // syn_resources
        0,    // num_syn_resources
        NULL, // syn_constraint_resources
        0,    // num_syn_constraint_resources
        XmInheritParentProcess, // parent_process
        NULL  // extension
    },
    // XmBulletinBoard
    {
        FALSE,
        NULL,
        XmInheritFocusMovedProc,
        NULL
    },
    // XmForm Class
    {
        NULL
    },
    // FSB Class
    {
        0
    }
};

WidgetClass xnFsbWidgetClass = (WidgetClass)&fsbWidgetClassRec;


static void fsb_class_init(void) {

}

static void fsb_class_part_init (WidgetClass wc) {
    FSBClassRec *fsbClass = (FSBClassRec*)wc;
    XmFormClassRec *formClass = (XmFormClassRec*)xmFormWidgetClass;
    
    fsbClass->constraint_class.initialize = formClass->constraint_class.initialize;
    fsbClass->constraint_class.set_values = formClass->constraint_class.set_values;
}


#define STRDUP_RES(a) if(a) a = strdup(a)
#define XMS_STRDUP_RES(a) if(a) a = XmStringCopy(a)

static void fsb_init(Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    XnFileSelectionBox fsb = (XnFileSelectionBox)neww;
    (xmFormClassRec.core_class.initialize)(request, neww, args, num_args);
    
    fsb->fsb.disable_set_values = 0;
    
    STRDUP_RES(fsb->fsb.homePath);
    STRDUP_RES(fsb->fsb.selectedPath);
    STRDUP_RES(fsb->fsb.currentPath);
    STRDUP_RES(fsb->fsb.filterStr);
    STRDUP_RES(fsb->fsb.labelListView);
    STRDUP_RES(fsb->fsb.labelDetailView);
    STRDUP_RES(fsb->fsb.labelOpenFileTitle);
    STRDUP_RES(fsb->fsb.labelSaveFileTitle);
    XMS_STRDUP_RES(fsb->fsb.labelDirUp);
    XMS_STRDUP_RES(fsb->fsb.labelHome);
    XMS_STRDUP_RES(fsb->fsb.labelNewFolder);
    XMS_STRDUP_RES(fsb->fsb.labelFilterButton);
    XMS_STRDUP_RES(fsb->fsb.labelShowHiddenFiles);
    XMS_STRDUP_RES(fsb->fsb.labelDirectories);
    XMS_STRDUP_RES(fsb->fsb.labelFiles);
    XMS_STRDUP_RES(fsb->fsb.labelRename);
    XMS_STRDUP_RES(fsb->fsb.labelDelete);
    XMS_STRDUP_RES(fsb->fsb.labelOpen);
    XMS_STRDUP_RES(fsb->fsb.labelSave);
    XMS_STRDUP_RES(fsb->fsb.labelCancel);
    XMS_STRDUP_RES(fsb->fsb.labelHelp);
    XMS_STRDUP_RES(fsb->fsb.labelFileName);
    XMS_STRDUP_RES(fsb->fsb.labelDirectoryName);
    XMS_STRDUP_RES(fsb->fsb.labelNewFileName);
    STRDUP_RES(fsb->fsb.labelDeleteFile);
    STRDUP_RES(fsb->fsb.detailHeadings);
    STRDUP_RES(fsb->fsb.dateFormatSameYear);
    STRDUP_RES(fsb->fsb.dateFormatOtherYear);
    STRDUP_RES(fsb->fsb.suffixBytes);
    STRDUP_RES(fsb->fsb.suffixKB);
    STRDUP_RES(fsb->fsb.suffixMB);
    STRDUP_RES(fsb->fsb.suffixGB);
    STRDUP_RES(fsb->fsb.suffixTB);
    STRDUP_RES(fsb->fsb.errorTitle);
    STRDUP_RES(fsb->fsb.errorIllegalChar);
    STRDUP_RES(fsb->fsb.errorRename);
    STRDUP_RES(fsb->fsb.errorFolder);
    STRDUP_RES(fsb->fsb.errorDelete);
    STRDUP_RES(fsb->fsb.errorOpenDir);
    
    CreateUI((XnFileSelectionBox)fsb);
    
    XtAddCallback(neww, XmNmapCallback, fsb_mapcb, NULL);
}

#define STR_FREE(a) if(a) free(a)
#define XMSTR_FREE(a) if(a) XmStringFree(a)

static void fsb_destroy(Widget widget) {
    XnFileSelectionBox w = (XnFileSelectionBox)widget;
    
    // destroy all views
    for(int i=0;i<w->fsb.numviews;i++) {
        FSBView v = w->fsb.view[i];
        v.destroy(widget, v.widget, v.userData);
    }
    
    STR_FREE(w->fsb.homePath);
    
    // free filelists
    filedialog_cleanup_filedata(w);
    STR_FREE(w->fsb.currentPath);
    STR_FREE(w->fsb.selectedPath);
    STR_FREE(w->fsb.filterStr);
    
    PathBarDestroy(w->fsb.pathBar);
    
    // free strings
    STR_FREE(w->fsb.labelListView);
    STR_FREE(w->fsb.labelDetailView);
    STR_FREE(w->fsb.labelOpenFileTitle);
    STR_FREE(w->fsb.labelSaveFileTitle);
    
    XMSTR_FREE(w->fsb.labelDirUp);
    XMSTR_FREE(w->fsb.labelHome);
    XMSTR_FREE(w->fsb.labelNewFolder);
    XMSTR_FREE(w->fsb.labelFilterButton);
    XMSTR_FREE(w->fsb.labelShowHiddenFiles);
    XMSTR_FREE(w->fsb.labelDirectories);
    XMSTR_FREE(w->fsb.labelFiles);
    XMSTR_FREE(w->fsb.labelRename);
    XMSTR_FREE(w->fsb.labelDelete);
    XMSTR_FREE(w->fsb.labelOpen);
    XMSTR_FREE(w->fsb.labelSave);
    XMSTR_FREE(w->fsb.labelCancel);
    XMSTR_FREE(w->fsb.labelHelp);
    XMSTR_FREE(w->fsb.labelFileName);
    XMSTR_FREE(w->fsb.labelDirectoryName);
    XMSTR_FREE(w->fsb.labelNewFileName);
    STR_FREE(w->fsb.labelDeleteFile);
    STR_FREE(w->fsb.detailHeadings);
    
    STR_FREE(w->fsb.dateFormatSameYear);
    STR_FREE(w->fsb.dateFormatOtherYear);
    STR_FREE(w->fsb.suffixBytes);
    STR_FREE(w->fsb.suffixKB);
    STR_FREE(w->fsb.suffixMB);
    STR_FREE(w->fsb.suffixGB);
    STR_FREE(w->fsb.suffixTB);
    
    STR_FREE(w->fsb.errorTitle);
    STR_FREE(w->fsb.errorIllegalChar);
    STR_FREE(w->fsb.errorRename);
    STR_FREE(w->fsb.errorFolder);
    STR_FREE(w->fsb.errorDelete);
    STR_FREE(w->fsb.errorOpenDir);
}

static void fsb_resize(Widget widget) {
    XnFileSelectionBox w = (XnFileSelectionBox)widget;
    (xmFormClassRec.core_class.resize)(widget);
    
#ifdef FSB_ENABLE_DETAIL
    if(w->fsb.view[w->fsb.selectedview].update == FileListDetailUpdate) {
        FileListDetailAdjustColWidth(w->fsb.grid);
    }
#endif
}

static void fsb_realize(Widget widget, XtValueMask *mask, XSetWindowAttributes *attributes) {
    XnFileSelectionBox w = (XnFileSelectionBox)widget;   
    (xmFormClassRec.core_class.realize)(widget, mask, attributes);
    
    FSBView view = w->fsb.view[w->fsb.selectedview];
    XmProcessTraversal(view.focus, XmTRAVERSE_CURRENT);
    
#ifdef FSB_ENABLE_DETAIL
    if(w->fsb.view[w->fsb.selectedview].update == FileListDetailUpdate) {
        FileListDetailAdjustColWidth(w->fsb.grid);
    }
#endif
}

static void FSBUpdateTitle(Widget w) {
    if(XtParent(w)->core.widget_class == xmDialogShellWidgetClass) {
        char *title = FSBDialogTitle(w);
        XtVaSetValues(XtParent(w), XmNtitle, title, NULL);
    }
}

static Boolean fsb_set_values(Widget old, Widget request, Widget neww, ArgList args, Cardinal *num_args) {
    Boolean r = False;

    XnFileSelectionBox o = (XnFileSelectionBox)old;
    XnFileSelectionBox n = (XnFileSelectionBox)neww;
    
    int setOkBtnLabel = 0;
    int ismanaged = XtIsManaged(neww);
    Dimension width, height;
    if(!ismanaged) {
        width = n->core.width;
        height = n->core.height;
        if(n->fsb.pathBar) {
            n->fsb.pathBar->disableResize = True;
        }
    }
    
    if(o->fsb.selectedview != n->fsb.selectedview) {
        int selectedview = n->fsb.selectedview;
        n->fsb.selectedview = o->fsb.selectedview;
        SelectView(n, selectedview);
    }
    
    char *updateDir = NULL;
    int selectItem = 0;
    if(o->fsb.selectedPath != n->fsb.selectedPath) {
        STR_FREE(o->fsb.selectedPath);
        STRDUP_RES(n->fsb.selectedPath);
        XmTextFieldSetString(n->fsb.name, FileName(n->fsb.selectedPath));
        // also update current directory
        updateDir = ParentPath(n->fsb.selectedPath);
        selectItem = 1;
    }
    if(o->fsb.currentPath != n->fsb.currentPath) {
        STR_FREE(o->fsb.currentPath);
        updateDir = strdup(n->fsb.currentPath);
        n->fsb.currentPath = NULL;
    }
    
    if(o->fsb.filterStr != n->fsb.filterStr) {
        STR_FREE(o->fsb.filterStr);
        STRDUP_RES(n->fsb.filterStr);
        XmTextFieldSetString(XmDropDownGetText(n->fsb.filter), n->fsb.filterStr);
        if(!updateDir) {
            filedialog_update_dir(n, NULL);
        }
    }
    
    if(updateDir) {
        filedialog_update_dir(n, updateDir);
        PathBarSetPath(n->fsb.pathBar, updateDir);
        free(updateDir);
    }
    
    if(o->fsb.type != n->fsb.type) {
        if(n->fsb.type == FILEDIALOG_OPEN) {
            XtVaSetValues(n->fsb.workarea, XmNbottomWidget, n->fsb.separator, NULL);
            XtUnmanageChild(n->fsb.name);
            XtUnmanageChild(n->fsb.nameLabel);
        } else {
            XtManageChild(n->fsb.name);
            XtManageChild(n->fsb.nameLabel);
            XtVaSetValues(n->fsb.workarea, XmNbottomWidget, n->fsb.nameLabel, NULL);
        }
        FSBUpdateTitle(neww);
        setOkBtnLabel = 1;
    }
      
    // label strings
    int updateTitle = 0;
    if(o->fsb.labelListView != n->fsb.labelListView) {
        STR_FREE(o->fsb.labelListView);
        STRDUP_RES(n->fsb.labelListView);
        XmString label = XmStringCreateLocalized(n->fsb.labelListView);
        XtVaSetValues(n->fsb.viewSelectorList, XmNlabelString, label, NULL);
        XmStringFree(label);
    }
    if(o->fsb.labelDetailView != n->fsb.labelDetailView) {
        STR_FREE(o->fsb.labelDetailView);
        STRDUP_RES(n->fsb.labelDetailView);
        XmString label = XmStringCreateLocalized(n->fsb.labelDetailView);
        XtVaSetValues(n->fsb.viewSelectorDetail, XmNlabelString, label, NULL);
        if(n->fsb.detailToggleButton) {
            XtVaSetValues(n->fsb.detailToggleButton, XmNlabelString, label, NULL);
        }
        XmStringFree(label);
    }
    if(o->fsb.labelOpenFileTitle != n->fsb.labelOpenFileTitle) {
        STR_FREE(o->fsb.labelOpenFileTitle);
        STRDUP_RES(n->fsb.labelOpenFileTitle);
        updateTitle = 1;
    }
    if(o->fsb.labelSaveFileTitle != n->fsb.labelSaveFileTitle) {
        STR_FREE(o->fsb.labelSaveFileTitle);
        STRDUP_RES(n->fsb.labelSaveFileTitle);
        updateTitle = 1;
    }
    
    if(o->fsb.labelDirUp != n->fsb.labelDirUp) {
        XMSTR_FREE(o->fsb.labelDirUp);
        XMS_STRDUP_RES(n->fsb.labelDirUp);
        XtVaSetValues(n->fsb.dirUp, XmNlabelString, n->fsb.labelDirUp, NULL);
    }
    if(o->fsb.labelHome != n->fsb.labelHome) {
        XMSTR_FREE(o->fsb.labelHome);
        XMS_STRDUP_RES(n->fsb.labelHome);
        XtVaSetValues(n->fsb.dirUp, XmNlabelString, n->fsb.labelHome, NULL);
    }
    if(o->fsb.labelNewFolder != n->fsb.labelNewFolder) {
        XMSTR_FREE(o->fsb.labelNewFolder);
        XMS_STRDUP_RES(n->fsb.labelNewFolder);
        XtVaSetValues(n->fsb.newFolder, XmNlabelString, n->fsb.labelNewFolder, NULL);
    }
    if(o->fsb.labelFilterButton != n->fsb.labelFilterButton) {
        XMSTR_FREE(o->fsb.labelFilterButton);
        XMS_STRDUP_RES(n->fsb.labelFilterButton);
        XtVaSetValues(n->fsb.filterButton, XmNlabelString, n->fsb.labelFilterButton, NULL);
    }
    if(o->fsb.labelShowHiddenFiles != n->fsb.labelShowHiddenFiles) {
        XMSTR_FREE(o->fsb.labelShowHiddenFiles);
        XMS_STRDUP_RES(n->fsb.labelShowHiddenFiles);
        XtVaSetValues(n->fsb.showHiddenButtonW, XmNlabelString, n->fsb.labelShowHiddenFiles, NULL);
    }
    if(o->fsb.labelDirectories != n->fsb.labelDirectories) {
        XMSTR_FREE(o->fsb.labelDirectories);
        XMS_STRDUP_RES(n->fsb.labelDirectories);
        XtVaSetValues(n->fsb.lsDirLabel, XmNlabelString, n->fsb.labelDirectories, NULL);
    }
    if(o->fsb.labelFiles != n->fsb.labelFiles) {
        XMSTR_FREE(o->fsb.labelFiles);
        XMS_STRDUP_RES(n->fsb.labelFiles);
        XtVaSetValues(n->fsb.lsFileLabel, XmNlabelString, n->fsb.labelFiles, NULL);
    }
    int recreateContextMenu = 0;
    if(o->fsb.labelRename != n->fsb.labelRename) {
        XMSTR_FREE(o->fsb.labelRename);
        XMS_STRDUP_RES(n->fsb.labelRename);
        recreateContextMenu = 1;
    }
    if(o->fsb.labelDelete != n->fsb.labelDelete) {
        XMSTR_FREE(o->fsb.labelDelete);
        XMS_STRDUP_RES(n->fsb.labelDelete);
        recreateContextMenu = 1;
    }

    if(o->fsb.labelOpen != n->fsb.labelOpen) {
        XMSTR_FREE(o->fsb.labelOpen);
        XMS_STRDUP_RES(n->fsb.labelOpen);
        setOkBtnLabel = 1;
    }
    if(o->fsb.labelSave != n->fsb.labelSave) {
        XMSTR_FREE(o->fsb.labelSave);
        XMS_STRDUP_RES(n->fsb.labelSave);
        setOkBtnLabel = 1;
    }
    if(o->fsb.labelCancel != n->fsb.labelCancel) {
        XMSTR_FREE(o->fsb.labelCancel);
        XMS_STRDUP_RES(n->fsb.labelCancel);
        XtVaSetValues(n->fsb.cancelBtn, XmNlabelString, n->fsb.labelCancel, NULL);
    }
    if(o->fsb.labelHelp != n->fsb.labelHelp) {
        XMSTR_FREE(o->fsb.labelHelp);
        XMS_STRDUP_RES(n->fsb.labelHelp);
        XtVaSetValues(n->fsb.helpBtn, XmNlabelString, n->fsb.labelHelp, NULL);
    }
    if(o->fsb.labelFileName != n->fsb.labelFileName) {
        XMSTR_FREE(o->fsb.labelFileName);
        XMS_STRDUP_RES(n->fsb.labelFileName);
        XtVaSetValues(n->fsb.nameLabel, XmNlabelString, n->fsb.labelFileName, NULL);
    }
    if(o->fsb.labelDirectoryName != n->fsb.labelDirectoryName) {
        XMSTR_FREE(o->fsb.labelDirectoryName);
        XMS_STRDUP_RES(n->fsb.labelDirectoryName);
    }
    if(o->fsb.labelNewFileName != n->fsb.labelNewFileName) {
        XMSTR_FREE(o->fsb.labelNewFileName);
        XMS_STRDUP_RES(n->fsb.labelNewFileName);
    }
    
    if(o->fsb.labelDeleteFile != n->fsb.labelDeleteFile) {
        STR_FREE(o->fsb.labelDeleteFile);
        STRDUP_RES(n->fsb.labelDeleteFile);
    }
#ifdef FSB_ENABLE_DETAIL
    if(o->fsb.detailHeadings != n->fsb.detailHeadings) {
        STR_FREE(o->fsb.detailHeadings);
        STRDUP_RES(n->fsb.detailHeadings);
        XtVaSetValues(n->fsb.grid, XmNsimpleHeadings, n->fsb.detailHeadings, NULL);
    }
#endif
    if(o->fsb.dateFormatSameYear != n->fsb.dateFormatSameYear) {
        STR_FREE(o->fsb.dateFormatSameYear);
        STRDUP_RES(n->fsb.dateFormatSameYear);
    }
    if(o->fsb.dateFormatOtherYear != n->fsb.dateFormatOtherYear) {
        STR_FREE(o->fsb.dateFormatOtherYear);
        STRDUP_RES(n->fsb.dateFormatOtherYear);
    }
    if(o->fsb.suffixBytes != n->fsb.suffixBytes) {
        STR_FREE(o->fsb.suffixBytes);
        STRDUP_RES(n->fsb.suffixBytes);
    }
    if(o->fsb.suffixMB != n->fsb.suffixMB) {
        STR_FREE(o->fsb.suffixMB);
        STRDUP_RES(n->fsb.suffixMB);
    }
    if(o->fsb.suffixGB != n->fsb.suffixGB) {
        STR_FREE(o->fsb.suffixGB);
        STRDUP_RES(n->fsb.suffixGB);
    }
    if(o->fsb.suffixTB != n->fsb.suffixTB) {
        STR_FREE(o->fsb.suffixTB);
        STRDUP_RES(n->fsb.suffixTB);
    }
    if(o->fsb.errorTitle != n->fsb.errorTitle) {
        STR_FREE(o->fsb.errorTitle);
        STRDUP_RES(n->fsb.errorTitle);
    }
    if(o->fsb.errorIllegalChar != n->fsb.errorIllegalChar) {
        STR_FREE(o->fsb.errorIllegalChar);
        STRDUP_RES(n->fsb.errorIllegalChar);
    }
    if(o->fsb.errorRename != n->fsb.errorRename) {
        STR_FREE(o->fsb.errorRename);
        STRDUP_RES(n->fsb.errorRename);
    }
    if(o->fsb.errorFolder != n->fsb.errorFolder) {
        STR_FREE(o->fsb.errorFolder);
        STRDUP_RES(n->fsb.errorFolder);
    }
    if(o->fsb.errorDelete != n->fsb.errorDelete) {
        STR_FREE(o->fsb.errorDelete);
        STRDUP_RES(n->fsb.errorDelete);
    }
    if(o->fsb.errorOpenDir != n->fsb.errorOpenDir) {
        STR_FREE(o->fsb.errorOpenDir);
        STRDUP_RES(n->fsb.errorOpenDir);
    }
    
    if(updateTitle) {
        FSBUpdateTitle(neww);
    }
    if(recreateContextMenu) {
        XtDestroyWidget(n->fsb.listContextMenu);
        XtDestroyWidget(n->fsb.gridContextMenu);
        n->fsb.listContextMenu = CreateContextMenu(n, n->fsb.filelist, FileContextMenuCB);
        n->fsb.gridContextMenu = CreateContextMenu(n, n->fsb.grid, FileContextMenuCB);
    }
    if(setOkBtnLabel) {
        XtVaSetValues(n->fsb.okBtn, XmNlabelString, n->fsb.type == FILEDIALOG_OPEN ? n->fsb.labelOpen : n->fsb.labelSave, NULL);
    }
    
    if(!ismanaged && !n->fsb.disable_set_values) {
        n->fsb.disable_set_values = 1;
        XtVaSetValues(neww, XmNwidth, width, XmNheight, height, NULL);
        n->fsb.disable_set_values = 0;
        
        if(n->fsb.pathBar)
            n->fsb.pathBar->disableResize = False;
    }
    
    if(selectItem) {
        if(ismanaged) {
            FSBSelectItem(n, FileName(n->fsb.selectedPath));
        }
    }
     
    Boolean fr = (xmFormClassRec.core_class.set_values)(old, request, neww, args, num_args);
    return fr ? fr : r;
}

static void fsb_insert_child(Widget child) {
    XnFileSelectionBox p = (XnFileSelectionBox)XtParent(child);
    (xmFormClassRec.composite_class.insert_child)(child);
    
    if(!p->fsb.gui_created) {
        return;
    }
    
    // custom child widget insert
    XtVaSetValues(child,
            XmNbottomAttachment, XmATTACH_WIDGET,
            XmNbottomWidget, p->fsb.bottom_widget,
            XmNbottomOffset, p->fsb.widgetSpacing,
            XmNleftAttachment, XmATTACH_FORM,
            XmNleftOffset, p->fsb.windowSpacing,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNrightOffset, p->fsb.windowSpacing,
            NULL);
    
    
    XtVaSetValues(p->fsb.listform,
            XmNbottomWidget, child,
            XmNbottomOffset, 0,
            NULL);
    
    p->fsb.workarea = child;
}

Boolean fsb_acceptfocus(Widget widget, Time *time) {
    return 0;
}

static void fsb_mapcb(Widget widget, XtPointer u, XtPointer cb) {
    XnFileSelectionBox w = (XnFileSelectionBox)widget;
    pathbar_resize(w->fsb.pathBar->widget, w->fsb.pathBar, NULL);
    
    if(w->fsb.type == FILEDIALOG_OPEN) {
        FSBView view = w->fsb.view[w->fsb.selectedview];
        XmProcessTraversal(view.focus, XmTRAVERSE_CURRENT);
    } else {
        XmProcessTraversal(w->fsb.name, XmTRAVERSE_CURRENT);
    }
    
    
    if(w->fsb.selectedPath) {
        FSBSelectItem(w, FileName(w->fsb.selectedPath));
    }
}

static void FocusInAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
    
}

static int apply_filter(XnFileSelectionBox w, const char *pattern, const char *string) {
    if(!pattern) return 0;
    
    FSBFilterFunc func = w->fsb.filterFunc ? w->fsb.filterFunc : FSBGlobFilter;
    return func(pattern, string);
}

static int FSBGlobFilter(const char *a, const char *b) {
    return fnmatch(a, b, 0);
}


static void errCB(Widget w, XtPointer d, XtPointer cbs) {
    XtDestroyWidget(w);
}

static void ErrDialog(XnFileSelectionBox w, const char *title, const char *errmsg) {
    Arg args[16];
    int n = 0;
    
    XmString titleStr = XmStringCreateLocalized((char*)title);
    XmString msg = XmStringCreateLocalized((char*)errmsg);
    
    XtSetArg(args[n], XmNdialogTitle, titleStr); n++;
    XtSetArg(args[n], XmNselectionLabelString, msg); n++;
    XtSetArg(args[n], XmNokLabelString, w->fsb.labelOk); n++;
    XtSetArg(args[n], XmNcancelLabelString, w->fsb.labelCancel); n++;
    
    Widget dialog = XmCreatePromptDialog ((Widget)w, "NewFolderPrompt", args, n);
    
    Widget help = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(help);
    Widget cancel = XmSelectionBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON);
    XtUnmanageChild(cancel);
    Widget text = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);
    XtUnmanageChild(text);
    
    XtAddCallback(dialog, XmNokCallback, errCB, NULL);
    
    XtManageChild(dialog);
    
    XmStringFree(titleStr);
    XmStringFree(msg);
}

static void rename_file_cb(Widget w, const char *path, XmSelectionBoxCallbackStruct *cb) {
    XnFileSelectionBox fsb = NULL;
    XtVaGetValues(w, XmNuserData, &fsb, NULL);
    
    char *fileName = NULL;
    XmStringGetLtoR(cb->value, XmSTRING_DEFAULT_CHARSET, &fileName);
    
    // make sure the new file name doesn't contain a path separator
    if(strchr(fileName, '/')) {
        ErrDialog(fsb, fsb->fsb.errorTitle, fsb->fsb.errorIllegalChar);
        XtFree(fileName);
        return;
    }
    
    char *parentPath = ParentPath(path);
    char *newPath = ConcatPath(parentPath, fileName);
    
    if(rename(path, newPath)) {
        char errmsg[256];
        snprintf(errmsg, 256, fsb->fsb.errorRename, strerror(errno));
        ErrDialog(fsb, fsb->fsb.errorTitle, errmsg);
    } else {
        filedialog_update_dir(fsb, parentPath);
    }
    
    free(parentPath);
    free(newPath);
    XtFree(fileName);
    XtDestroyWidget(XtParent(w));
}

static void selectionbox_cancel(Widget w, XtPointer data, XtPointer d) {
    XtDestroyWidget(XtParent(w));
}

static void FSBRename(XnFileSelectionBox fsb, const char *path) {
    Arg args[16];
    int n = 0;
    Widget w = (Widget)fsb;
    
    char *name = FileName((char*)path);
    
    XmString filename = XmStringCreateLocalized(name);
    XtSetArg(args[n], XmNselectionLabelString,fsb->fsb.labelNewFileName); n++;
    XtSetArg(args[n], XmNtextString, filename); n++;
    XtSetArg(args[n], XmNuserData, fsb); n++;
    XtSetArg(args[n], XmNdialogTitle, fsb->fsb.labelRename); n++;
    XtSetArg(args[n], XmNokLabelString, fsb->fsb.labelOk); n++;
    XtSetArg(args[n], XmNcancelLabelString, fsb->fsb.labelCancel); n++;
    Widget dialog = XmCreatePromptDialog (w, "RenameFilePrompt", args, n);
    
    Widget help = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(help);
    
    XtAddCallback(dialog, XmNokCallback, (XtCallbackProc)rename_file_cb, (char*)path);
    XtAddCallback(dialog, XmNcancelCallback, (XtCallbackProc)selectionbox_cancel, NULL);
    
    XmStringFree(filename);
    XtManageChild(dialog);
}

static void delete_file_cb(Widget w, const char *path, XmSelectionBoxCallbackStruct *cb) {
    XnFileSelectionBox fsb = NULL;
    XtVaGetValues(w, XmNuserData, &fsb, NULL);
    
    if(unlink(path)) {
        char errmsg[256];
        snprintf(errmsg, 256, fsb->fsb.errorDelete, strerror(errno));
        ErrDialog(fsb, fsb->fsb.errorTitle, errmsg);
    } else {
        char *parentPath = ParentPath(path);
        filedialog_update_dir(fsb, parentPath);
        free(parentPath);
    }
    
    XtDestroyWidget(XtParent(w));
}

static void FSBDelete(XnFileSelectionBox fsb, const char *path) {
    Arg args[16];
    int n = 0;
    Widget w = (Widget)fsb;
    
    char *name = FileName((char*)path);
    size_t len = strlen(name);
    size_t msglen = len + strlen(fsb->fsb.labelDeleteFile) + 4;
    char *msg = malloc(msglen);
    snprintf(msg, msglen, fsb->fsb.labelDeleteFile, name);
    
    XmString prompt = XmStringCreateLocalized(msg);
    XtSetArg(args[n], XmNselectionLabelString, prompt); n++;
    XtSetArg(args[n], XmNuserData, fsb); n++;
    XtSetArg(args[n], XmNdialogTitle, fsb->fsb.labelDelete); n++;
    XtSetArg(args[n], XmNokLabelString, fsb->fsb.labelOk); n++;
    XtSetArg(args[n], XmNcancelLabelString, fsb->fsb.labelCancel); n++;
    Widget dialog = XmCreatePromptDialog (w, "DeleteFilePrompt", args, n);
    
    Widget help = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(help);
    Widget text = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);
    XtUnmanageChild(text);
    
    XtAddCallback(dialog, XmNokCallback, (XtCallbackProc)delete_file_cb, (char*)path);
    XtAddCallback(dialog, XmNcancelCallback, (XtCallbackProc)selectionbox_cancel, NULL);
    
    free(msg);
    XmStringFree(prompt);
    XtManageChild(dialog);
}

static void FSBSelectItem(XnFileSelectionBox fsb, const char *item) {
    FSBView view = fsb->fsb.view[fsb->fsb.selectedview];
    if(view.select) {
        view.select((Widget)fsb, view.widget, item);
    }
}

static char* set_selected_path(XnFileSelectionBox data, XmString item)
{
    char *name = NULL;
    XmStringGetLtoR(item, XmFONTLIST_DEFAULT_TAG, &name);
    if(!name) {
        return NULL;
    }
    char *path = ConcatPath(data->fsb.currentPath, name);
    XtFree(name);
    
    if(data->fsb.selectedPath) {
        free(data->fsb.selectedPath);
    }
    data->fsb.selectedPath = path;
    
    return path;
}

// item0: rename
// item1: delete
static void FileContextMenuCB(Widget item, XtPointer index, XtPointer cd) {
    intptr_t i = (intptr_t)index;
    Widget parent = XtParent(item);
    XnFileSelectionBox fsb = NULL;
    XtVaGetValues(parent, XmNuserData, &fsb, NULL);
    
    const char *path = fsb->fsb.selectedPath;
    if(path) {
        if(i == 0) {
            FSBRename(fsb, path);
        } else if(i == 1) {
            FSBDelete(fsb, path);
        }
    }
}

static Widget CreateContextMenu(XnFileSelectionBox fsb, Widget parent, XtCallbackProc callback) {
    return XmVaCreateSimplePopupMenu(
            parent, "popup", callback, XmNpopupEnabled, XmPOPUP_AUTOMATIC,
            XmNuserData, fsb,
            XmVaPUSHBUTTON, fsb->fsb.labelRename, 'R', NULL, NULL,
            XmVaPUSHBUTTON, fsb->fsb.labelDelete, 'D', NULL, NULL,
            NULL);
}


static void FileListUpdate(Widget fsb, Widget view, FileElm *dirs, int dircount, FileElm *files, int filecount, const char *filter, int maxnamelen, void *userData) {
    XnFileSelectionBox data = userData;
    FileListWidgetAdd(data, data->fsb.filelist, data->fsb.showHidden, filter, files, filecount);
}

static void FileListSelect(Widget fsb, Widget view, const char *item) {
    XnFileSelectionBox w = (XnFileSelectionBox)fsb;
    
    int numItems = 0;
    XmStringTable items = NULL;
    XtVaGetValues(w->fsb.filelist, XmNitemCount, &numItems, XmNitems, &items, NULL);
    
    for(int i=0;i<numItems;i++) {
        char *str = NULL;
        XmStringGetLtoR(items[i], XmFONTLIST_DEFAULT_TAG, &str);
        if(!strcmp(str, item)) {
            XmListSelectPos(w->fsb.filelist, i+1, False);
            break;
        }
        XtFree(str);
    }
}

static void FileListCleanup(Widget fsb, Widget view, void *userData) {
    XnFileSelectionBox data = userData;
    XmListDeleteAllItems(data->fsb.filelist);
}

static void FileListDestroy(Widget fsb, Widget view, void *userData) {
    // unused
}

static void FileListActivateCB(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        data->fsb.end = True;
        data->fsb.status = FILEDIALOG_OK;
        data->fsb.selIsDir = False;
        FileSelectionCallback(data, data->fsb.okCallback, XmCR_OK, data->fsb.selectedPath);
    }
}

static void FileListSelectCB(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb)
{
    if(data->fsb.type == FILEDIALOG_SAVE) {
        char *name = NULL;
        XmStringGetLtoR(cb->item, XmFONTLIST_DEFAULT_TAG, &name);
        XmTextFieldSetString(data->fsb.name, name);
        XtFree(name);
    } else {
        char *path = set_selected_path(data, cb->item);
        if(path) {
            data->fsb.selIsDir = False;
        }
    }
}


static void FileListWidgetAdd(XnFileSelectionBox fsb, Widget w, int showHidden, const char *filter, FileElm *ls, int count)
{   
    if(count > 0) {
        XmStringTable items = calloc(count, sizeof(XmString));
        int i = 0;
        
        for(int j=0;j<count;j++) {
            FileElm *e = &ls[j];
            
            char *name = FileName(e->path);
            if((!showHidden && name[0] == '.') || apply_filter(fsb, filter, name)) {
                continue;
            }
            
            items[i] = XmStringCreateLocalized(name);
            i++;
        }
        XmListAddItems(w, items, i, 0);
        for(i=0;i<count;i++) {
            XmStringFree(items[i]);
        }
        free(items);
    }
}


#ifdef FSB_ENABLE_DETAIL
static void FileListDetailUpdate(Widget fsb, Widget view, FileElm *dirs, int dircount, FileElm *files, int filecount, const char *filter, int maxnamelen, void *userData) {
    XnFileSelectionBox data = userData;
    FileListDetailAdd(data, data->fsb.grid, data->fsb.showHidden, filter, files, filecount, maxnamelen);
}
#endif


/*
 * create file size string with kb/mb/gb/tb suffix
 */
static char* size_str(XnFileSelectionBox fsb, FileElm *f) {
    char *str = malloc(16);
    uint64_t size = f->size;
    
    if(f->isDirectory) {
        str[0] = '\0';
    } else if(size < 0x400) {
        snprintf(str, 16, "%d %s", (int)size, fsb->fsb.suffixBytes);
    } else if(size < 0x100000) {
        float s = (float)size/0x400;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x2800 && diff != 0) {
            // size < 10 KiB
            snprintf(str, 16, "%.1f %s", s, fsb->fsb.suffixKB);
        } else {
            snprintf(str, 16, "%.0f %s", s, fsb->fsb.suffixKB);
        }
    } else if(size < 0x40000000) {
        float s = (float)size/0x100000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0xa00000 && diff != 0) {
            // size < 10 MiB
            snprintf(str, 16, "%.1f %s", s, fsb->fsb.suffixMB);
        } else {
            snprintf(str, 16, "%.0f %s", s, fsb->fsb.suffixMB);
        }
    } else if(size < 0x1000000000ULL) {
        float s = (float)size/0x40000000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x280000000 && diff != 0) {
            // size < 10 GiB
            snprintf(str, 16, "%.1f %s", s, fsb->fsb.suffixGB);
        } else {
            snprintf(str, 16, "%.0f %s", s, fsb->fsb.suffixGB);
        }
    } else {
        size /= 1024;
        float s = (float)size/0x40000000;
        int diff = (s*100 - (int)s*100);
        if(diff > 90) {
            diff = 0;
            s += 0.10f;
        }
        if(size < 0x280000000 && diff != 0) {
            // size < 10 TiB
            snprintf(str, 16, "%.1f %s", s, fsb->fsb.suffixTB);
        } else {
            snprintf(str, 16, "%.0f %s", s, fsb->fsb.suffixTB);
        }
    }
    return str;
}

static char* date_str(XnFileSelectionBox fsb, time_t tm) {
    struct tm t;
    struct tm n;
    time_t now = time(NULL);
    
    localtime_r(&tm, &t);
    localtime_r(&now, &n);
    
    char *str = malloc(24);
    if(t.tm_year == n.tm_year) {
        strftime(str, 24, fsb->fsb.dateFormatSameYear, &t);
    } else {
        strftime(str, 24, fsb->fsb.dateFormatOtherYear, &t);
    }
    return str;
}

#ifdef FSB_ENABLE_DETAIL
static void FileListDetailAdjustColWidth(Widget grid) {
    XmLGridColumn column0 = XmLGridGetColumn(grid, XmCONTENT, 0);
    XmLGridColumn column1 = XmLGridGetColumn(grid, XmCONTENT, 1);
    XmLGridColumn column2 = XmLGridGetColumn(grid, XmCONTENT, 2);
    
    Dimension col0Width = XmLGridColumnWidthInPixels(column0);
    Dimension col1Width = XmLGridColumnWidthInPixels(column1);
    Dimension col2Width = XmLGridColumnWidthInPixels(column2);
    
    Dimension totalWidth = col0Width + col1Width + col2Width;
    
    Dimension gridWidth = 0;
    Dimension gridShadow = 0;
    XtVaGetValues(grid, XmNwidth, &gridWidth, XmNshadowThickness, &gridShadow, NULL);
    
    Dimension widthDiff = gridWidth - totalWidth - gridShadow - gridShadow;
    
    if(gridWidth > totalWidth) {
            XtVaSetValues(grid,
            XmNcolumnRangeStart, 0,
            XmNcolumnRangeEnd, 0,
            XmNcolumnWidth, col0Width + widthDiff - XmLGridVSBWidth(grid) - 2,
            XmNcolumnSizePolicy, XmCONSTANT,
            NULL);
    }
}

static void FileListDetailAdd(XnFileSelectionBox fsb, Widget grid, int showHidden, const char *filter, FileElm *ls, int count, int maxWidth)
{
    XmLGridAddRows(grid, XmCONTENT, 1, count);
    
    int row = 0;
    for(int i=0;i<count;i++) {
        FileElm *e = &ls[i];
        
        char *name = FileName(e->path);
        if((!showHidden && name[0] == '.') || (!e->isDirectory && apply_filter(fsb, filter, name))) {
            continue;
        }
        
        // name
        XmString str = XmStringCreateLocalized(name);
        XtVaSetValues(grid,
                XmNcolumn, 0, 
                XmNrow, row,
                XmNcellString, str, NULL);
        XmStringFree(str);
        // size
        char *szbuf = size_str(fsb, e);
        str = XmStringCreateLocalized(szbuf);
        XtVaSetValues(grid,
                XmNcolumn, 1, 
                XmNrow, row,
                XmNcellString, str, NULL);
        free(szbuf);
        XmStringFree(str);
        // date
        char *datebuf = date_str(fsb, e->lastModified);
        str = XmStringCreateLocalized(datebuf);
        XtVaSetValues(grid,
                XmNcolumn, 2, 
                XmNrow, row,
                XmNcellString, str, NULL);
        free(datebuf);
        XmStringFree(str);
        
        XtVaSetValues(grid, XmNrow, row, XmNrowUserData, e, NULL);
        row++;
    }
    
    // remove unused rows
    if(count > row) {
        XmLGridDeleteRows(grid, XmCONTENT, row, count-row);
    }
    
    if(maxWidth < 16) {
        maxWidth = 16;
    }
    
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 0,
        XmNcolumnRangeEnd, 0,
        XmNcolumnWidth, maxWidth,
        XmNcellAlignment, XmALIGNMENT_LEFT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 1,
        XmNcolumnRangeEnd, 1,
        XmNcolumnWidth, 9,
        XmNcellAlignment, XmALIGNMENT_LEFT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    XtVaSetValues(grid,
        XmNcolumnRangeStart, 2,
        XmNcolumnRangeEnd, 2,
        XmNcolumnWidth, 16,
        XmNcellAlignment, XmALIGNMENT_RIGHT,
        XmNcolumnSizePolicy, XmVARIABLE,
        NULL);
    
    FileListDetailAdjustColWidth(grid);
}

static void FileListDetailSelect(Widget fsb, Widget view, const char *item) {
    XnFileSelectionBox w = (XnFileSelectionBox)fsb;
    
    int numRows = 0;
    XtVaGetValues(w->fsb.grid, XmNrows, &numRows, NULL);
    
    XmLGridColumn col = XmLGridGetColumn(w->fsb.grid, XmCONTENT, 0);
    for(int i=0;i<numRows;i++) {
        XmLGridRow row = XmLGridGetRow(w->fsb.grid, XmCONTENT, i);
        FileElm *elm = NULL;
        XtVaGetValues(w->fsb.grid, XmNrowPtr, row, XmNcolumnPtr, col, XmNrowUserData, &elm, NULL);
        if(elm) {
            if(!strcmp(item, FileName(elm->path))) {
                XmLGridSelectRow(w->fsb.grid, i, False);
                XmLGridFocusAndShowRow(w->fsb.grid, i+1);
                break;
            }
        }
    }
}

static void FileListDetailCleanup(Widget fsb, Widget view, void *userData) {
    XnFileSelectionBox data = userData;
    // cleanup grid
    Cardinal rows = 0;
    XtVaGetValues(data->fsb.grid, XmNrows, &rows, NULL);
    XmLGridDeleteRows(data->fsb.grid, XmCONTENT, 0, rows);
}

static void FileListDetailDestroy(Widget fsb, Widget view, void *userData) {
    // unused
}
#endif

static void create_folder(Widget w, XnFileSelectionBox data, XmSelectionBoxCallbackStruct *cbs) {
    char *fileName = NULL;
    XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &fileName);
    
    char *newFolder = ConcatPath(data->fsb.currentPath ? data->fsb.currentPath : "", fileName);
    if(mkdir(newFolder, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
        char errmsg[256];
        snprintf(errmsg, 256, data->fsb.errorFolder, strerror(errno));
        ErrDialog(data, data->fsb.errorTitle, errmsg);
    } else {
        char *p = strdup(data->fsb.currentPath);
        filedialog_update_dir(data, p);
        free(p);
    }
    free(newFolder);
    
    XtDestroyWidget(XtParent(w));
}

static void new_folder_cancel(Widget w, XnFileSelectionBox data, XtPointer d) {
    XtDestroyWidget(XtParent(w));
}

static void FSBNewFolder(Widget w, XnFileSelectionBox data, XtPointer u)
{
    Arg args[16];
    int n = 0;
    
    XtSetArg(args[n], XmNdialogTitle, data->fsb.labelNewFolder); n++;
    XtSetArg (args[n], XmNselectionLabelString, data->fsb.labelDirectoryName); n++;
    XtSetArg(args[n], XmNokLabelString, data->fsb.labelOk); n++;
    XtSetArg(args[n], XmNcancelLabelString, data->fsb.labelCancel); n++;
    Widget dialog = XmCreatePromptDialog (w, "NewFolderPrompt", args, n);
    
    Widget help = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
    XtUnmanageChild(help);
    
    XtAddCallback(dialog, XmNokCallback, (XtCallbackProc)create_folder, data);
    XtAddCallback(dialog, XmNcancelCallback, (XtCallbackProc)new_folder_cancel, data);
    
    XtManageChild(dialog);
    
}

static void FSBHome(Widget w, XnFileSelectionBox data, XtPointer u) {
    const char *homePath = data->fsb.homePath ? data->fsb.homePath : GetHomeDir();
    filedialog_update_dir(data, homePath);
    PathBarSetPath(data->fsb.pathBar, homePath);
}


/*
 * file_cmp_field
 * 0: compare path
 * 1: compare size
 * 2: compare mtime
 */
static int file_cmp_field = 0;

/*
 * 1 or -1
 */
static int file_cmp_order = 1;

static int filecmp(const void *f1, const void *f2)
{
    const FileElm *file1 = f1;
    const FileElm *file2 = f2;
    if(file1->isDirectory != file2->isDirectory) {
        return file1->isDirectory < file2->isDirectory;
    }
    
    int cmp_field = file_cmp_field;
    int cmp_order = file_cmp_order;
    if(file1->isDirectory) {
        cmp_field = 0;
        cmp_order = 1;
    }
    
    int ret = 0;
    switch(cmp_field) {
        case 0: {
            ret = strcmp(FileName(file1->path), FileName(file2->path));
            break;
        }
        case 1: {
            if(file1->size < file2->size) {
                ret = -1;
            } else if(file1->size == file2->size) {
                ret = 0;
            } else {
                ret = 1;
            }
            break;
        }
        case 2: {
            if(file1->lastModified < file2->lastModified) {
                ret = -1;
            } else if(file1->lastModified == file2->lastModified) {
                ret = 0;
            } else {
                ret = 1;
            }
            break;
        }
    }
    
    return ret * cmp_order;
}


static void free_files(FileElm *ls, int count)
{
    for(int i=0;i<count;i++) {
        if(ls[i].path) {
            free(ls[i].path);
        }
    }
    free(ls);
}

static void filedialog_cleanup_filedata(XnFileSelectionBox data)
{
    free_files(data->fsb.dirs, data->fsb.dircount);
    free_files(data->fsb.files, data->fsb.filecount);
    data->fsb.dirs = NULL;
    data->fsb.files = NULL;
    data->fsb.dircount = 0;
    data->fsb.filecount = 0;
    data->fsb.maxnamelen = 0;
}

#define FILE_ARRAY_SIZE 1024

static void file_array_add(FileElm **files, int *alloc, int *count, FileElm elm) {
    int c = *count;
    int a = *alloc;
    if(c >= a) {
        a *= 2;
        FileElm *newarray = realloc(*files, sizeof(FileElm) * a);
        
        *files = newarray;
        *alloc = a;
    }
    
    (*files)[c] = elm;
    c++;
    *count = c;
}

static int filedialog_update_dir(XnFileSelectionBox data, const char *path)
{
    DIR *dir = NULL;
    if(path) {
        // try to check first, if we can open the path
        dir = opendir(path);
        if(!dir) {
            char errmsg[256];
            snprintf(errmsg, 256, data->fsb.errorOpenDir, strerror(errno));
            
            ErrDialog(data, data->fsb.errorTitle, errmsg);
            return 1;
        }
    }
    
    FSBView view = data->fsb.view[data->fsb.selectedview];
    view.cleanup((Widget)data, view.widget, view.userData);
      
    if(view.useDirList) {
        XmListDeleteAllItems(data->fsb.dirlist);
    }
    
    /* read dir and insert items */
    if(path) {
        int dircount = 0; 
        int filecount = 0;
        size_t maxNameLen = 0;
        
        FileElm *dirs = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        FileElm *files = calloc(sizeof(FileElm), FILE_ARRAY_SIZE);
        int dirs_alloc = FILE_ARRAY_SIZE;
        int files_alloc = FILE_ARRAY_SIZE;
        
        filedialog_cleanup_filedata(data);
    
        /* dir reading complete - set the path textfield */  
        XmTextFieldSetString(data->fsb.path, (char*)path);
        char *oldPath = data->fsb.currentPath;
        data->fsb.currentPath = strdup(path);
        if(oldPath) {
            free(oldPath);
        }
        path = data->fsb.currentPath;

        struct dirent *ent;
        while((ent = readdir(dir)) != NULL) {
            if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }

            char *entpath = ConcatPath(path, ent->d_name);

            struct stat s;
            if(stat(entpath, &s)) {
                free(entpath);
                continue;
            }

            FileElm new_entry;
            new_entry.path = entpath;
            new_entry.isDirectory = S_ISDIR(s.st_mode);
            new_entry.size = (uint64_t)s.st_size;
            new_entry.lastModified = s.st_mtime;

            size_t nameLen = strlen(ent->d_name);
            if(nameLen > maxNameLen) {
                maxNameLen = nameLen;
            }

            if(new_entry.isDirectory) {
                file_array_add(&dirs, &dirs_alloc, &dircount, new_entry);
            } else {
                file_array_add(&files, &files_alloc, &filecount, new_entry);
            }
        }
        closedir(dir);
        
        data->fsb.dirs = dirs;
        data->fsb.files = files;
        data->fsb.dircount = dircount;
        data->fsb.filecount = filecount;
        data->fsb.maxnamelen = maxNameLen;
        
        // sort file arrays
        qsort(dirs, dircount, sizeof(FileElm), filecmp);
        qsort(files, filecount, sizeof(FileElm), filecmp);
    }
    
    Widget filterTF = XmDropDownGetText(data->fsb.filter);
    char *filter = XmTextFieldGetString(filterTF);
    char *filterStr = filter;
    if(!filter || strlen(filter) == 0) {
        filterStr = "*";
    }
    
    if(view.useDirList) {
        FileListWidgetAdd(data, data->fsb.dirlist, data->fsb.showHidden, NULL, data->fsb.dirs, data->fsb.dircount);
        view.update(
                (Widget)data,
                view.widget,
                NULL,
                0,
                data->fsb.files,
                data->fsb.filecount,
                filterStr,
                data->fsb.maxnamelen,
                view.userData);
    } else {
        view.update(
                (Widget)data,
                view.widget,
                data->fsb.dirs,
                data->fsb.dircount,
                data->fsb.files,
                data->fsb.filecount,
                filterStr,
                data->fsb.maxnamelen,
                view.userData);
    }
    
    if(filter) {
        XtFree(filter);
    }
    
    return 0;
}


static void dirlist_activate(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        if(!filedialog_update_dir(data, path)) {
            PathBarSetPath(data->fsb.pathBar, path);
            data->fsb.selIsDir = TRUE;
        } 
    }    
}

static void dirlist_select(Widget w, XnFileSelectionBox data, XmListCallbackStruct *cb)
{
    char *path = set_selected_path(data, cb->item);
    if(path) {
        data->fsb.selIsDir = TRUE;
    }
}

static void filedialog_enable_detailview(Widget w, XnFileSelectionBox data, XmToggleButtonCallbackStruct *tb) {
    SelectView(data, tb->set); // 0: list, 1: detail
}


static void filedialog_setshowhidden(
        Widget w,
        XnFileSelectionBox data,
        XmToggleButtonCallbackStruct *tb)
{
    data->fsb.showHidden = tb->set;
    filedialog_update_dir(data, NULL);
}

static void filedialog_filter(Widget w, XnFileSelectionBox data, XtPointer c)
{
    filedialog_update_dir(data, NULL);
}

static void filedialog_update_filter(Widget w, XnFileSelectionBox data, XtPointer c)
{
    filedialog_update_dir(data, NULL);
    
}

static void filedialog_goup(Widget w, XnFileSelectionBox data, XtPointer d)
{
    char *newPath = ParentPath(data->fsb.currentPath);
    filedialog_update_dir(data, newPath);
    PathBarSetPath(data->fsb.pathBar, newPath);
    free(newPath);
}

static void filedialog_ok(Widget w, XnFileSelectionBox data, XtPointer d)
{
    if(data->fsb.type == FILEDIALOG_SAVE) {
        char *newName = XmTextFieldGetString(data->fsb.name);
        if(newName) {
            if(strchr(newName, '/')) {
                ErrDialog(data, data->fsb.errorTitle, data->fsb.errorIllegalChar);
                XtFree(newName);
                return;
            }
            
            if(strlen(newName) > 0) {
                char *selPath = ConcatPath(data->fsb.currentPath, newName);
                if(data->fsb.selectedPath) free(data->fsb.selectedPath);
                data->fsb.selectedPath = selPath;
            }
            XtFree(newName);
            
            data->fsb.selIsDir = False;
        }
    }
    
    if(data->fsb.selectedPath) {
        if(!data->fsb.selIsDir) {
            data->fsb.status = FILEDIALOG_OK;
            data->fsb.end = True;
            FileSelectionCallback(data, data->fsb.okCallback, XmCR_OK, data->fsb.selectedPath);
        }
    }
}

static void filedialog_cancel(Widget w, XnFileSelectionBox data, XtPointer d)
{
    data->fsb.end = 1;
    data->fsb.status = FILEDIALOG_CANCEL;
    FileSelectionCallback(data, data->fsb.cancelCallback, XmCR_CANCEL, data->fsb.currentPath);
}

static void filedialog_help(Widget w, XnFileSelectionBox data, XtPointer d)
{
    FileSelectionCallback(data, data->manager.help_callback, XmCR_HELP, data->fsb.currentPath);
}

static void FileSelectionCallback(XnFileSelectionBox fsb, XtCallbackList cb, int reason, const char *value) {
    XmFileSelectionBoxCallbackStruct cbs;
    memset(&cbs, 0, sizeof(XmFileSelectionBoxCallbackStruct));
    
    char *dir = fsb->fsb.currentPath;
    size_t dirlen = dir ? strlen(dir) : 0;
    if(dir && dirlen > 0) {
        char *dir2 = NULL;
        if(dir[dirlen-1] != '/') {
            // add a trailing / to the dir string 
            dir2 = malloc(dirlen+2);
            memcpy(dir2, dir, dirlen);
            dir2[dirlen] = '/';
            dir2[dirlen+1] = '\0';
            dirlen++;
            dir = dir2;
        }
        cbs.dir = XmStringCreateLocalized(dir);
        cbs.dir_length = dirlen;
        if(dir2) {
            free(dir2);
        }
    } else {
        cbs.dir = XmStringCreateLocalized("");
        cbs.dir_length = 0;
    }
    cbs.reason = reason;
    
    cbs.value = XmStringCreateLocalized((char*)value);
    cbs.length = strlen(value);
    
    XtCallCallbackList((Widget)fsb, cb, (XtPointer)&cbs);
    
    XmStringFree(cbs.dir);
    XmStringFree(cbs.value);
}

static void CreateUI(XnFileSelectionBox w) { 
    Arg args[32];
    int n = 0;
    XmString str;
       
    int widget_spacing = w->fsb.widgetSpacing;
    int window_spacing = w->fsb.windowSpacing;
    
    Widget form = (Widget)w;
    int type = w->fsb.type;
    
    XtVaSetValues((Widget)w, XmNautoUnmanage, False, NULL);
     
    /* upper part of the gui */
    
    n = 0;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelDirUp); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, window_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNresizable, True); n++;
    XtSetArg(args[n], XmNarrowDirection, XmARROW_UP); n++;
    w->fsb.dirUp = XmCreatePushButton(form, "DirUp", args, n);
    XtManageChild(w->fsb.dirUp);
    XtAddCallback(w->fsb.dirUp, XmNactivateCallback,
                 (XtCallbackProc)filedialog_goup, w);
    
    // View Option Menu
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, window_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, window_spacing); n++;
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    Widget viewframe = XmCreateForm(form, "vframe", args, n);
    XtManageChild(viewframe);

    w->fsb.viewMenu = XmCreatePulldownMenu(viewframe, "menu", NULL, 0);
    
    Widget view;
    if(w->fsb.showViewMenu) {
        n = 0;
        XtSetArg(args[n], XmNsubMenuId, w->fsb.viewMenu); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNmarginHeight, 0); n++;
        XtSetArg(args[n], XmNmarginWidth, 0); n++;
        view = XmCreateOptionMenu(viewframe, "option_menu", args, n);
        XtManageChild(view);
        w->fsb.viewOption = view;
        w->fsb.detailToggleButton = NULL;
    } else {
        n = 0;
        str = XmStringCreateLocalized(w->fsb.labelDetailView);
        XtSetArg(args[n], XmNlabelString, str); n++;
        XtSetArg(args[n], XmNfillOnSelect, True); n++;
        XtSetArg(args[n], XmNindicatorOn, False); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        if(w->fsb.selectedview == 1) {
            XtSetArg(args[n], XmNset, 1); n++;
        }
        w->fsb.detailToggleButton = XmCreateToggleButton(viewframe, "ToggleDetailView", args, n);
        XtManageChild(w->fsb.detailToggleButton);
        view = w->fsb.detailToggleButton;
        XmStringFree(str);
        
        XtAddCallback(
            w->fsb.detailToggleButton,
            XmNvalueChangedCallback,
            (XtCallbackProc)filedialog_enable_detailview,
            w);
        
        w->fsb.viewOption = NULL;
    }    

    n = 0;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightWidget, view); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelNewFolder); n++;
    w->fsb.newFolder = XmCreatePushButton(viewframe, "NewFolder", args, n);
    XtManageChild(w->fsb.newFolder);
    XtAddCallback(
            w->fsb.newFolder,
            XmNactivateCallback,
            (XtCallbackProc)FSBNewFolder,
            w);
    

    n = 0;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelHome); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, w->fsb.newFolder); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    w->fsb.home = XmCreatePushButton(viewframe, "Home", args, n);
    XtManageChild(w->fsb.home);
    XtAddCallback(
            w->fsb.home,
            XmNactivateCallback,
            (XtCallbackProc)FSBHome,
            w);
    
    // match visual appearance of detailToggleButton with the other buttons
    if(w->fsb.detailToggleButton) {
        Dimension highlight, shadow;
        XtVaGetValues(w->fsb.newFolder, XmNshadowThickness, &shadow, XmNhighlightThickness, &highlight, NULL);
        XtVaSetValues(w->fsb.detailToggleButton, XmNshadowThickness, shadow, XmNhighlightThickness, highlight, NULL);
    }
    
    // pathbar
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopOffset, window_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, w->fsb.dirUp); n++;
    XtSetArg(args[n], XmNleftOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, viewframe); n++;
    XtSetArg(args[n], XmNrightOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN); n++;
    Widget pathBarFrame = XmCreateFrame(form, "pathbar_frame", args, n);
    XtManageChild(pathBarFrame);
    w->fsb.pathBar = CreatePathBar(pathBarFrame, args, 0);
    w->fsb.pathBar->updateDir = (updatedir_callback)filedialog_update_dir;
    w->fsb.pathBar->updateDirData = w;
    XtManageChild(w->fsb.pathBar->widget);
    w->fsb.path = XmCreateTextField(form, "textfield", args, 0);
    
    XtVaSetValues(w->fsb.dirUp, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, pathBarFrame, NULL);
    if(!w->fsb.showViewMenu) {
        XtVaSetValues(viewframe, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, pathBarFrame, NULL);
    }
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, pathBarFrame); n++;
    XtSetArg(args[n], XmNtopOffset, 2*widget_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, window_spacing); n++;
    w->fsb.filterForm = XmCreateForm(form, "filterform", args, n);
    XtManageChild(w->fsb.filterForm);
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelDirectories); n++;
    w->fsb.lsDirLabel = XmCreateLabel(w->fsb.filterForm, "labelDirs", args, n);
    XtManageChild(w->fsb.lsDirLabel);
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 35); n++;
    XtSetArg(args[n], XmNleftOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelFiles); n++;
    w->fsb.lsFileLabel = XmCreateLabel(w->fsb.filterForm, "labelFiles", args, n);
    XtManageChild(w->fsb.lsFileLabel);
      
    if(w->fsb.showHiddenButton) {
        n = 0;
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNlabelString, w->fsb.labelShowHiddenFiles); n++;
        XtSetArg(args[n], XmNset, w->fsb.showHidden); n++;
        w->fsb.showHiddenButtonW = XmCreateToggleButton(w->fsb.filterForm, "showHidden", args, n);
        XtManageChild(w->fsb.showHiddenButtonW);
        XtAddCallback(w->fsb.showHiddenButtonW, XmNvalueChangedCallback,
                     (XtCallbackProc)filedialog_setshowhidden, w);
    }
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelFilterButton); n++;
    if(w->fsb.showHiddenButton) {
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNrightWidget, w->fsb.showHiddenButtonW); n++;
    } else {
        XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    }
    w->fsb.filterButton = XmCreatePushButton(w->fsb.filterForm, "filedialog_filter", args, n);
    XtManageChild(w->fsb.filterButton);
    XtAddCallback(w->fsb.filterButton, XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, w);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, w->fsb.lsFileLabel); n++;
    XtSetArg(args[n], XmNleftOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, w->fsb.filterButton); n++;
    XtSetArg(args[n], XmNrightOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNshowLabel, False); n++;
    XtSetArg(args[n], XmNuseTextField, True); n++;
    XtSetArg(args[n], XmNverify, False); n++;
    w->fsb.filter = XmCreateDropDown(w->fsb.filterForm, "filedialog_filter_textfield", args, n);
    XtManageChild(w->fsb.filter);
    XmTextFieldSetString(XmDropDownGetText(w->fsb.filter), w->fsb.filterStr);
    XtAddCallback(XmDropDownGetText(w->fsb.filter), XmNactivateCallback,
                 (XtCallbackProc)filedialog_filter, w);
    XtAddCallback(w->fsb.filter, XmNupdateTextCallback,
                 (XtCallbackProc)filedialog_update_filter, w);
    Widget filterList = XmDropDownGetList(w->fsb.filter);
    str = XmStringCreateSimple("*");
    XmListAddItem(filterList, str, 0);
    XmStringFree(str);
        
    /* lower part */
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomOffset, window_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, window_spacing); n++;
    XtSetArg(args[n], XmNtopOffset, widget_spacing * 2); n++;
    Widget buttons = XmCreateForm(form, "buttons", args, n);
    XtManageChild(buttons);
    
    n = 0;
    str = type == FILEDIALOG_OPEN ? w->fsb.labelOpen : w->fsb.labelSave;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, str); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 14); n++;
    w->fsb.okBtn = XmCreatePushButton(buttons, "filedialog_open", args, n);
    XtManageChild(w->fsb.okBtn);
    XmStringFree(str);
    XtAddCallback(w->fsb.okBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_ok, w);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelHelp); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 86); n++;
    w->fsb.helpBtn = XmCreatePushButton(buttons, "filedialog_help", args, n);
    XtManageChild(w->fsb.helpBtn);
    XtAddCallback(w->fsb.helpBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_help, w);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNleftPosition, 43); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 57); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelCancel); n++;
    w->fsb.cancelBtn = XmCreatePushButton(buttons, "filedialog_cancel", args, n);
    XtManageChild(w->fsb.cancelBtn);
    XtAddCallback(w->fsb.cancelBtn, XmNactivateCallback,
                 (XtCallbackProc)filedialog_cancel, w);
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, buttons); n++;
    XtSetArg(args[n], XmNbottomOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 1); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 1); n++;
    w->fsb.separator = XmCreateSeparator(form, "ofd_separator", args, n);
    XtManageChild(w->fsb.separator);
    
    Widget bottomWidget = w->fsb.separator;
    
    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, w->fsb.separator); n++;
    XtSetArg(args[n], XmNbottomOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, window_spacing); n++;
    w->fsb.name = XmCreateTextField(form, "textfield", args, n);
    XtAddCallback(w->fsb.name, XmNactivateCallback,
             (XtCallbackProc)filedialog_ok, w);

    n = 0;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, w->fsb.name); n++;
    XtSetArg(args[n], XmNbottomOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelFileName); n++;
    w->fsb.nameLabel = XmCreateLabel(form, "label", args, n);
        
    if(type == FILEDIALOG_SAVE) {
        bottomWidget = w->fsb.nameLabel;
        XtManageChild(w->fsb.name);
        XtManageChild(w->fsb.nameLabel);
    }
    w->fsb.bottom_widget = bottomWidget;
    
    
    // middle 
    // form for dir/file lists
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, w->fsb.filterForm); n++;
    XtSetArg(args[n], XmNtopOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, bottomWidget); n++;
    XtSetArg(args[n], XmNleftOffset, window_spacing); n++;
    XtSetArg(args[n], XmNrightOffset, window_spacing); n++;
    XtSetArg(args[n], XmNbottomOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNwidth, 580); n++;
    XtSetArg(args[n], XmNheight, 400); n++;
    w->fsb.listform = XmCreateForm(form, "fds_listform", args, n); 
    
    // dir/file lists
    
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopWidget, w->fsb.lsDirLabel); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
    XtSetArg(args[n], XmNrightPosition, 35); n++;
    w->fsb.dirlist = XmCreateScrolledList(w->fsb.listform, "dirlist", args, n);
    Dimension width, height;
    XtMakeResizeRequest(w->fsb.dirlist, 150, 200, &width, &height);
    XtManageChild(w->fsb.dirlist);
    
    XtAddCallback(
            w->fsb.dirlist,
            XmNdefaultActionCallback,
            (XtCallbackProc)dirlist_activate,
            w); 
    XtAddCallback(
            w->fsb.dirlist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)dirlist_select,
            w);
    
    // FileList
    XnFileSelectionBoxAddView(
            (Widget)w,
            w->fsb.labelListView,
            CreateListView,
            FileListUpdate,
            FileListSelect,
            FileListCleanup,
            FileListDestroy,
            True,
            w);
    
    // Detail FileList
#ifdef FSB_ENABLE_DETAIL
    XnFileSelectionBoxAddView(
            (Widget)w,
            w->fsb.labelDetailView,
            CreateDetailView,
            FileListDetailUpdate,
            FileListDetailSelect,
            FileListDetailCleanup,
            FileListDetailDestroy,
            True,
            w);
#endif
       
    /*
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, w->fsb.dirlist); n++;
    XtSetArg(args[n], XmNleftOffset, widget_spacing); n++;
    //XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    //XtSetArg(args[n], XmNbottomWidget, w->fsb.filelist); n++;
    XtSetArg(args[n], XmNbottomOffset, widget_spacing); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNlabelString, w->fsb.labelFiles); n++;
    w->fsb.lsFileLabel = XmCreateLabel(w->fsb.listform, "label", args, n);
    XtManageChild(w->fsb.lsFileLabel);
    */
    
    XtManageChild(w->fsb.listform);
    
    int selview = w->fsb.selectedview;
    if(selview < 2) {
        XtManageChild(w->fsb.view[selview].widget);
    } else {
        w->fsb.selectedview = 0;
    }
    
    
    if(w->fsb.selectedPath) {
        char *parentPath = ParentPath(w->fsb.selectedPath);
        filedialog_update_dir(w, parentPath);
        PathBarSetPath(w->fsb.pathBar, parentPath);
        free(parentPath);
        
        if(w->fsb.type == FILEDIALOG_SAVE) {
            XmTextFieldSetString(w->fsb.name, FileName(w->fsb.selectedPath));
        }
    } else {
        char cwd[PATH_MAX];
        const char *currentPath = w->fsb.currentPath;
        if(!currentPath) {
            if(getcwd(cwd, PATH_MAX)) {
                currentPath = cwd;
            } else {
                currentPath = GetHomeDir();
            }
        }
        
        filedialog_update_dir(w, currentPath);
        PathBarSetPath(w->fsb.pathBar, w->fsb.currentPath);
    }


    w->fsb.selectedview = selview;
         
    XtVaSetValues((Widget)w, XmNcancelButton, w->fsb.cancelBtn, NULL);
    
    w->fsb.gui_created = 1;
}

static char* FSBDialogTitle(Widget widget) {
    XnFileSelectionBox w = (XnFileSelectionBox)widget;
    if(w->fsb.type == FILEDIALOG_OPEN) {
        return w->fsb.labelOpenFileTitle;
    } else {
        return w->fsb.labelSaveFileTitle;
    }
}

static FSBViewWidgets CreateView(XnFileSelectionBox w, FSBViewCreateProc create, void *userData, Boolean useDirList) {
    Arg args[64];
    int n = 0;
    
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    if(useDirList) {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
        XtSetArg(args[n], XmNleftWidget, w->fsb.dirlist); n++;
        XtSetArg(args[n], XmNleftOffset, w->fsb.widgetSpacing); n++;
    } else {
        XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
        XtSetArg(args[n], XmNtopOffset, w->fsb.widgetSpacing); n++;
        XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    }
    
    return create(w->fsb.listform, args, n, userData);
}


typedef struct FSBViewSelection {
    XnFileSelectionBox fsb;
    int index;
} FSBViewSelection;

static void SelectView(XnFileSelectionBox f, int view) {
    FSBView current = f->fsb.view[f->fsb.selectedview];
    FSBView newview = f->fsb.view[view];
    
    XtUnmanageChild(current.widget);
    if(newview.useDirList != current.useDirList) {
        if(current.useDirList) {
            XtUnmanageChild(f->fsb.listform);
        } else {
            XtManageChild(f->fsb.listform);
        }
    }
    
    current.cleanup((Widget)f, current.widget, current.userData);
    XtManageChild(newview.widget);
    
    f->fsb.selectedview = view;
    
    filedialog_update_dir(f, NULL);
    XmProcessTraversal(newview.focus, XmTRAVERSE_CURRENT);
}

static void SelectViewCallback(Widget w, FSBViewSelection *data, XtPointer u) {
    SelectView(data->fsb, data->index);
}

static void SelectViewItemDestroy(Widget w, FSBViewSelection *data, XtPointer u) {
    free(data);
}

static void AddViewMenuItem(XnFileSelectionBox w, const char *name, int viewIndex) {
    Arg args[4];
    int n = 0;
    
    XmString label = XmStringCreateLocalized((char*)name);
    
    XtSetArg(args[n], XmNlabelString, label); n++;
    XtSetArg(args[1], XmNpositionIndex, w->fsb.selectedview == w->fsb.numviews ? 0 : w->fsb.numviews+1); n++;
    Widget item = XmCreatePushButton(w->fsb.viewMenu, "menuitem", args, n);
    
    if(viewIndex == 0) {
        w->fsb.viewSelectorList = item;
    } else if(viewIndex == 1) {
        w->fsb.viewSelectorDetail = item;
    }
   
    XtManageChild(item);
    XmStringFree(label);
    
    FSBViewSelection *data = malloc(sizeof(FSBViewSelection));
    data->fsb = w;
    data->index = viewIndex;
    
    XtAddCallback(
            item,
            XmNactivateCallback,
            (XtCallbackProc)SelectViewCallback,
            data);
    XtAddCallback(
            item,
            XmNdestroyCallback,
            (XtCallbackProc)SelectViewItemDestroy,
            data);
}

static FSBViewWidgets CreateListView(Widget parent, ArgList args, int n, void *userData) {
    XnFileSelectionBox fsb = (XnFileSelectionBox)userData;
    
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    Widget frame = XmCreateFrame(parent, "filelistframe", args, n);
    
    fsb->fsb.filelist = XmCreateScrolledList(frame, "filelist", NULL, 0);
    XtManageChild(fsb->fsb.filelist);
    
    XtAddCallback(
            fsb->fsb.filelist,
            XmNdefaultActionCallback,
            (XtCallbackProc)FileListActivateCB,
            userData); 
    XtAddCallback(
            fsb->fsb.filelist,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)FileListSelectCB,
            userData);
    
    fsb->fsb.listContextMenu = CreateContextMenu(fsb, fsb->fsb.filelist, FileContextMenuCB);
    
    FSBViewWidgets widgets;
    widgets.view = frame;
    widgets.focus = fsb->fsb.filelist;
    return widgets;
}


#ifdef FSB_ENABLE_DETAIL
static void set_path_from_row(XnFileSelectionBox data, int row) {
    FileElm *elm = NULL;
    XmLGridRow rowPtr = XmLGridGetRow(data->fsb.grid, XmCONTENT, row);
    XtVaGetValues(data->fsb.grid, XmNrowPtr, rowPtr, XmNrowUserData, &elm, NULL);
    if(!elm) {
        fprintf(stderr, "error: no row data\n");
        return;
    }
    
    char *path = strdup(elm->path);
    
    data->fsb.selIsDir = False;
    if(data->fsb.type == FILEDIALOG_SAVE) {
        XmTextFieldSetString(data->fsb.name, FileName(path));
    }
    
    if(data->fsb.selectedPath) {
        free(data->fsb.selectedPath);
    }
    data->fsb.selectedPath = path;
}

static void grid_select(Widget w, XnFileSelectionBox data, XmLGridCallbackStruct *cb) {
    set_path_from_row(data, cb->row);
}

static void grid_activate(Widget w, XnFileSelectionBox data, XmLGridCallbackStruct *cb) {
    set_path_from_row(data, cb->row);
    data->fsb.end = True;
    data->fsb.status = FILEDIALOG_OK;
    
    FileSelectionCallback(data, data->fsb.okCallback, XmCR_OK, data->fsb.selectedPath);
}
 
static void grid_key_pressed(Widget w, XnFileSelectionBox data, XmLGridCallbackStruct *cb) {
    char chars[16];
    KeySym keysym;
    int nchars;
    
    nchars = XLookupString(&cb->event->xkey, chars, 15, &keysym, NULL);
    
    if(nchars == 0) return;

    // if data->showHidden is 0, data->files contains more items than the grid
    // this means SelectedRow might not be the correct index for data->files
    // we have to count files manually and increase 'row', if the file
    // is actually displayed in the grid
    int row = 0;
    int selectedRow = XmLGridGetSelectedRow(w);
    
    int match = -1;
    
    for(int i=0;i<data->fsb.filecount;i++) {
        const char *name = FileName(data->fsb.files[i].path);
        if(!data->fsb.showHidden && name[0] == '.') continue;
        
        size_t namelen = strlen(name);
        
        size_t cmplen = namelen < nchars ? namelen : nchars;
        if(!memcmp(name, chars, cmplen)) {
            if(row <= selectedRow) {
                if(match == -1) {
                    match = row;
                }
            } else {
                match = row;
                break;
            }
        }
        
        row++;
    }
    
    if(match > -1) {
        XmLGridSelectRow(w, match, True);
        XmLGridFocusAndShowRow(w, match+1);
    } else {
        XBell(XtDisplay(w), 0);
    }
}

static void grid_header_clicked(Widget w, XnFileSelectionBox data, XmLGridCallbackStruct *cb) { 
    int new_cmp_field = 0;
    switch(cb->column) {
        case 0: {
            new_cmp_field = 0;            
            break;
        }
        case 1: {
            new_cmp_field = 1;
            break;
        }
        case 2: {
            new_cmp_field = 2;
            break;
        }
    }
    
    if(new_cmp_field == file_cmp_field) {
        file_cmp_order = -file_cmp_order; // revert sort order
    } else {
        file_cmp_field = new_cmp_field; // change file cmp order to new field
        file_cmp_order = 1;
    }
    
    int sort_type = file_cmp_order == 1 ? XmSORT_ASCENDING : XmSORT_DESCENDING;
    XmLGridSetSort(data->fsb.grid, file_cmp_field, sort_type);
    
    qsort(data->fsb.files, data->fsb.filecount, sizeof(FileElm), filecmp);
    
    // refresh widget
    filedialog_update_dir(data, NULL);
} 

static FSBViewWidgets CreateDetailView(Widget parent, ArgList args, int n, void *userData) {
    XnFileSelectionBox w = userData;
    
    XtSetArg(args[n], XmNshadowThickness, 0); n++;
    Widget gridcontainer = XmCreateFrame(parent, "gridcontainer", args, n);
    XtManageChild(gridcontainer);
    
    n = 0;
    XtSetArg(args[n], XmNcolumns, 3); n++;
    XtSetArg(args[n], XmNheadingColumns, 0); n++;
    XtSetArg(args[n], XmNheadingRows, 1); n++;
    XtSetArg(args[n], XmNallowColumnResize, 1); n++;
    XtSetArg(args[n], XmNsimpleHeadings, w->fsb.detailHeadings); n++;
    XtSetArg(args[n], XmNhorizontalSizePolicy, XmCONSTANT); n++;
    
    w->fsb.grid = XmLCreateGrid(gridcontainer, "grid", args, n);
    XmLGridSetIgnoreModifyVerify(w->fsb.grid, True);
    XtManageChild(w->fsb.grid);
    
    XtVaSetValues(
            w->fsb.grid,
            XmNcellDefaults, True,
            XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
            XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
            NULL);
    
    XtAddCallback(w->fsb.grid, XmNselectCallback, (XtCallbackProc)grid_select, w);
    XtAddCallback(w->fsb.grid, XmNactivateCallback, (XtCallbackProc)grid_activate, w);
    XtAddCallback(w->fsb.grid, XmNheaderClickCallback, (XtCallbackProc)grid_header_clicked, w);
    XtAddCallback(w->fsb.grid, XmNgridKeyPressedCallback, (XtCallbackProc)grid_key_pressed, w);
    
    // context menu
    w->fsb.gridContextMenu = CreateContextMenu(w, w->fsb.grid, FileContextMenuCB);
    
    FSBViewWidgets widgets;
    widgets.view = gridcontainer;
    widgets.focus = w->fsb.grid;
    return widgets;
}
#endif


/* ------------------------------ Path Utils  ------------------------------ */

const char* GetHomeDir(void) {
    char *home = getenv("HOME");
    if(!home) {
        home = getenv("USERPROFILE");
        if(!home) {
            home = "/";
        }
    }
    return home;
}

static char* ConcatPath(const char *parent, const char *name)
{ 
    size_t parentlen = strlen(parent);
    size_t namelen = strlen(name);
    
    size_t pathlen = parentlen + namelen + 2;
    char *path = malloc(pathlen);
    
    memcpy(path, parent, parentlen);
    if(parentlen > 0 && parent[parentlen-1] != '/') {
        path[parentlen] = '/';
        parentlen++;
    }
    if(name[0] == '/') {
        name++;
        namelen--;
    }
    memcpy(path+parentlen, name, namelen);
    path[parentlen+namelen] = '\0';
    return path;
}

static char* FileName(char *path) {
    int si = 0;
    int osi = 0;
    int i = 0;
    int p = 0;
    char c;
    while((c = path[i]) != 0) {
        if(c == '/') {
            osi = si;
            si = i;
            p = 1;
        }
        i++;
    }
    
    char *name = path + si + p;
    if(name[0] == 0) {
        name = path + osi + p;
        if(name[0] == 0) {
            return path;
        }
    }
    
    return name;
}

static char* ParentPath(const char *path) {
    char *name = FileName((char*)path);
    size_t namelen = strlen(name);
    size_t pathlen = strlen(path);
    size_t parentlen = pathlen - namelen;
    if(parentlen == 0) {
        parentlen++;
    }
    char *parent = malloc(parentlen + 1);
    memcpy(parent, path, parentlen);
    parent[parentlen] = '\0';
    return parent;
}

// unused at the moment, maybe reactivate if more illegal characters
// are defined
/*
static int CheckFileName(const char *fileName) {
    size_t len = strlen(fileName);
    for(int i=0;i<len;i++) {
        if(fileName[i] == '/') {
            return 0;
        }
    }
    return 1;
}
*/

/* ------------------------------ PathBar  ------------------------------ */

static void pathbar_resize(Widget w, PathBar *p, XtPointer d)
{
    if(p->disableResize) return;
    
    Dimension width, height;
    XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
    
    Dimension xoff;
    XtVaGetValues(p->down, XmNwidth, &xoff, NULL);
    
    Dimension *segW = calloc(p->numSegments, sizeof(Dimension));
    
    Dimension maxHeight = 0;
    
    /* get width/height from all widgets */
    Dimension pathWidth = 0;
    for(int i=0;i<p->numSegments;i++) {
        Dimension segWidth;
        Dimension segHeight;
        XtVaGetValues(p->pathSegments[i], XmNwidth, &segWidth, XmNheight, &segHeight, NULL);
        segW[i] = segWidth;
        pathWidth += segWidth;
        if(segHeight > maxHeight) {
            maxHeight = segHeight;
        }
    }
    Dimension tfHeight;
    XtVaGetValues(p->textfield, XmNheight, &tfHeight, NULL);
    if(tfHeight > maxHeight) {
        maxHeight = tfHeight;
    }
    
    Boolean arrows = False;
    if(pathWidth + xoff + 10 > width) {
        arrows = True;
        //pathWidth += p->lw + p->rw;
    }
    
    /* calc max visible widgets */
    int start = 0;
    if(arrows) {
        Dimension vis = p->lw+p->rw;
        for(int i=p->numSegments;i>0;i--) {
            Dimension segWidth = segW[i-1];
            if(vis + segWidth + xoff + 10 > width) {
                start = i;
                arrows = True;
                break;
            }
            vis += segWidth;
        }
    } else {
        p->shift = 0;
    }
    
    int leftShift = 0;
    if(p->shift < 0) {
        if(start + p->shift < 0) {
            leftShift = start;
            start = 0;
            p->shift = -leftShift;
        } else {
            leftShift = -p->shift; /* negative shift */
            start += p->shift;
        }
    }
    
    int x = 0;
    if(arrows) {
        XtManageChild(p->left);
        XtManageChild(p->right);
        x += p->lw;
    } else {
        XtUnmanageChild(p->left);
        XtUnmanageChild(p->right);
    }
    
    for(int i=0;i<p->numSegments;i++) {
        if(i >= start && i < p->numSegments - leftShift && !p->input) {
            XtVaSetValues(p->pathSegments[i], XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
            x += segW[i];
            XtManageChild(p->pathSegments[i]);
        } else {
            XtUnmanageChild(p->pathSegments[i]);
        }
    }
    
    if(arrows) {
        XtVaSetValues(p->left, XmNx, 0, XmNy, 0, XmNheight, maxHeight, NULL);
        XtVaSetValues(p->right, XmNx, x, XmNy, 0, XmNheight, maxHeight, NULL);
    }
    XtVaSetValues(p->down, XmNx, width-xoff, XmNheight, maxHeight, NULL);
    free(segW);
    
    Dimension rw, rh;
    XtMakeResizeRequest(w, width, maxHeight, &rw, &rh);
    XtVaSetValues(p->textfield, XmNwidth, rw-xoff, XmNheight, rh, NULL);
}

static void pathbar_input(Widget w, PathBar *p, XtPointer c)
{
    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct*)c;
    XEvent *xevent = cbs->event;
    
    if (cbs->reason == XmCR_INPUT) {
        if (xevent->xany.type == ButtonPress) {
            XtUnmanageChild(p->left);
            XtUnmanageChild(p->right);
            
            XtManageChild(p->textfield);
            p->input = 1;
            
            XmProcessTraversal(p->textfield, XmTRAVERSE_CURRENT);
            
            pathbar_resize(p->widget, p, NULL);
        }
    }
}

static void pathbar_losingfocus(Widget w, PathBar *p, XtPointer c)
{
    p->input = False;
    XtUnmanageChild(p->textfield);
    pathbar_resize(p->widget, p, NULL);
}

static void pathbar_pathinput(Widget w, PathBar *pb, XtPointer d)
{
    char *newpath = XmTextFieldGetString(pb->textfield);
    if(newpath) {
        if(newpath[0] == '~') {
            char *p = newpath+1;
            char *cp = ConcatPath(GetHomeDir(), p);
            XtFree(newpath);
            newpath = cp;
        } else if(newpath[0] != '/') {
            char cwd[PATH_MAX];
            if(!getcwd(cwd, sizeof(cwd))) {
                cwd[0] = '/';
                cwd[1] = '\0';
            }
            char *cp = ConcatPath(cwd, newpath);
            XtFree(newpath);
            newpath = cp;
        }
        
        /* update path */
        if(pb->updateDir) {
            if(!pb->updateDir(pb->updateDirData, newpath)) {
                PathBarSetPath(pb, newpath);
            }
        } else {
            PathBarSetPath(pb, newpath);
        }
        XtFree(newpath);
        
        /* hide textfield and show path as buttons */
        XtUnmanageChild(pb->textfield);
        pathbar_resize(pb->widget, pb, NULL);
    }
}

static void pathbar_shift_left(Widget w, PathBar *p, XtPointer d) {
    p->shift--;
    pathbar_resize(p->widget, p, NULL);
}

static void pathbar_shift_right(Widget w, PathBar *p, XtPointer d) {
    if(p->shift < 0) {
        p->shift++;
    }
    pathbar_resize(p->widget, p, NULL);
}

static void pathbar_list_select(Widget w, PathBar *p, XmListCallbackStruct *cb) {
    char *value = NULL;
    XmStringGetLtoR(cb->item, XmSTRING_DEFAULT_CHARSET, &value);
    p->updateDir(p->updateDirData, value);
    PathBarSetPath(p, value);
    free(value);
}

static void pathbar_popup(Widget w, PathBar *p, XtPointer d) {
    Widget parent = XtParent(w);
    Display *dp = XtDisplay(parent);
    Window root = XDefaultRootWindow(dp);
    
    int x, y;
    Window child;
    XTranslateCoordinates(dp, XtWindow(parent), root, 0, 0, &x, &y, &child);

    XtManageChild(p->list);
    XtPopupSpringLoaded(p->popup);
    XtVaSetValues(p->popup, XmNx, x, XmNy, y + parent->core.height, XmNwidth, parent->core.width, XmNheight, 200, NULL);
    
    XmProcessTraversal(p->list, XmTRAVERSE_CURRENT);
}

static void popupEH(Widget widget, XtPointer data, XEvent *event, Boolean *dispatch) {
    PathBar *bar = data;
    
    Window w1 = bar->hs ? XtWindow(bar->hs) : 0;
    Window w2 = bar->vs ? XtWindow(bar->vs) : 0;
    
    if(event->type == ButtonPress) {
        if(event->xbutton.window != 0 && (event->xbutton.window == w1 || event->xbutton.window == w2)) {
            bar->popupScrollEvent = 1;
        } else {
            bar->popupScrollEvent = 0;
        }
    } else if(event->type == ButtonRelease) {
        if(bar->popupScrollEvent) {
            *dispatch = False;
        }
        bar->popupScrollEvent = 0;
    } else if(event->type == KeyReleaseMask) {
        int keycode = event->xkey.keycode;
        if(keycode == 36 || keycode == 9) {
            XtUnmapWidget(bar->popup);
        }
    }
}

static void pathTextEH(Widget widget, XtPointer data, XEvent *event, Boolean *dispatch) {
    PathBar *pb = data;
    if(event->type == KeyReleaseMask) {
        if(event->xkey.keycode == 9) {
            XtUnmanageChild(pb->textfield);
            pathbar_resize(pb->widget, pb, NULL);
            *dispatch = False;
        }
    }
}


PathBar* CreatePathBar(Widget parent, ArgList args, int n)
{
    PathBar *bar = malloc(sizeof(PathBar));
    bar->path = NULL;
    bar->updateDir = NULL;
    bar->updateDirData = NULL;
    bar->disableResize = False;
    
    bar->shift = 0;
    
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    bar->widget = XmCreateDrawingArea(parent, "pathbar", args, n);
    XtAddCallback(
            bar->widget,
            XmNresizeCallback,
            (XtCallbackProc)pathbar_resize,
            bar);
    XtAddCallback(
            bar->widget,
            XmNinputCallback,
            (XtCallbackProc)pathbar_input,
            bar);
    
    n = 0;
    XtSetArg(args[n], XmNownerEvents, True), n++;
    XtSetArg(args[n], XmNgrabStyle, GrabModeSync), n++;
    bar->popup = XmCreateGrabShell(bar->widget, "pbpopup", args, n);
    bar->list = XmCreateScrolledList(bar->popup, "pblist", NULL, 0);
    XtAddEventHandler(bar->popup, KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask, FALSE, popupEH, bar);
    bar->popupScrollEvent = 0;
    
    XtAddCallback(
            bar->list,
            XmNdefaultActionCallback,
            (XtCallbackProc)pathbar_list_select,
            bar); 
    XtAddCallback(
            bar->list,
            XmNbrowseSelectionCallback,
            (XtCallbackProc)pathbar_list_select,
            bar);
    
    bar->vs = NULL;
    bar->hs = NULL;
    XtVaGetValues(XtParent(bar->list), XmNhorizontalScrollBar, &bar->hs, XmNverticalScrollBar, &bar->vs, NULL);
    
    Arg a[4];
    XtSetArg(a[0], XmNshadowThickness, 0);
    XtSetArg(a[1], XmNx, 0);
    XtSetArg(a[2], XmNy, 0);
    bar->textfield = XmCreateTextField(bar->widget, "pbtext", a, 3);
    bar->input = 0;
    XtAddCallback(
            bar->textfield,
            XmNlosingFocusCallback,
            (XtCallbackProc)pathbar_losingfocus,
            bar);
    XtAddCallback(bar->textfield, XmNactivateCallback,
                 (XtCallbackProc)pathbar_pathinput, bar);
    XtAddEventHandler(bar->textfield, KeyPressMask | KeyReleaseMask, FALSE, pathTextEH, bar);
    
    XtSetArg(a[0], XmNarrowDirection, XmARROW_DOWN);
    bar->down = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtManageChild(bar->down);
    XtSetArg(a[0], XmNarrowDirection, XmARROW_LEFT);
    bar->left = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtSetArg(a[0], XmNarrowDirection, XmARROW_RIGHT);
    bar->right = XmCreateArrowButton(bar->widget, "pbbutton", a, 1);
    XtAddCallback(
                bar->down,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_popup,
                bar);
    XtAddCallback(
                bar->left,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_left,
                bar);
    XtAddCallback(
                bar->right,
                XmNactivateCallback,
                (XtCallbackProc)pathbar_shift_right,
                bar);
    
    Pixel bg;
    XtVaGetValues(bar->textfield, XmNbackground, &bg, NULL);
    XtVaSetValues(bar->widget, XmNbackground, bg, NULL);
    
    //XtManageChild(bar->left);
    //XtManageChild(bar->right);
    
    XtVaGetValues(bar->left, XmNwidth, &bar->lw, NULL);
    XtVaGetValues(bar->right, XmNwidth, &bar->rw, NULL);
    
    bar->segmentAlloc = 16;
    bar->numSegments = 0;
    bar->pathSegments = calloc(16, sizeof(Widget));
    
    bar->selection = 0;
    
    return bar;
}

static void PathBarChangeDir(Widget w, PathBar *bar, XtPointer unused) {
    XmToggleButtonSetState(bar->pathSegments[bar->selection], False, False);
    
    for(int i=0;i<bar->numSegments;i++) {  
        if(bar->pathSegments[i] == w) {
            bar->selection = i;
            XmToggleButtonSetState(w, True, False);
            break;
        }
    }
    
    int plen = strlen(bar->path);
    int countSeg = 0;
    for(int i=0;i<=plen;i++) {
        char c = bar->path[i];
        if(c == '/' || c == '\0') {
            if(countSeg == bar->selection) {
                char *dir = malloc(i+2);
                memcpy(dir, bar->path, i+1);
                dir[i+1] = '\0';
                if(bar->updateDir) {
                    bar->updateDir(bar->updateDirData, dir);
                }
                free(dir);
            }
            countSeg++;
        }
    }
}

void PathBarSetPath(PathBar *bar, const char *path) {
    if(bar->path) {
        free(bar->path);
    }
    bar->path = strdup(path);
     
    for(int i=0;i<bar->numSegments;i++) {
        XtDestroyWidget(bar->pathSegments[i]);
    }
    XtUnmanageChild(bar->textfield);
    //XtManageChild(bar->left);
    //XtManageChild(bar->right);
    bar->input = False;
    
    Arg args[4];
    XmString str;
    
    bar->numSegments = 0;
    
    int i=0;
    if(path[0] == '/') {
        str = XmStringCreateLocalized("/");
        XtSetArg(args[0], XmNlabelString, str);
        XtSetArg(args[1], XmNfillOnSelect, True);
        XtSetArg(args[2], XmNindicatorOn, False);
        bar->pathSegments[0] = XmCreateToggleButton(
                bar->widget, "pbbutton", args, 3);
        XtAddCallback(
                bar->pathSegments[0],
                XmNvalueChangedCallback,
                (XtCallbackProc)PathBarChangeDir,
                bar);
        XmStringFree(str);
        bar->numSegments++;
        i++;
    }
    
    int len = strlen(path);
    int begin = i;
    for(;i<=len;i++) {
        char c = path[i];
        if((c == '/' || c == '\0') && i > begin) {
            char *segStr = malloc(i - begin + 1);
            memcpy(segStr, path+begin, i-begin);
            segStr[i-begin] = '\0';
            begin = i+1;
            
            str = XmStringCreateLocalized(segStr);
            free(segStr);
            XtSetArg(args[0], XmNlabelString, str);
            XtSetArg(args[1], XmNfillOnSelect, True);
            XtSetArg(args[2], XmNindicatorOn, False);
            Widget button = XmCreateToggleButton(bar->widget, "pbbutton", args, 3);
            XtAddCallback(
                    button,
                    XmNvalueChangedCallback,
                    (XtCallbackProc)PathBarChangeDir,
                    bar);
            XmStringFree(str);
            
            if(bar->numSegments >= bar->segmentAlloc) {
                bar->segmentAlloc += 8;
                bar->pathSegments = realloc(bar->pathSegments, bar->segmentAlloc * sizeof(Widget));
            }
            
            bar->pathSegments[bar->numSegments++] = button;
        }
    }
    
    bar->selection = bar->numSegments-1;
    XmToggleButtonSetState(bar->pathSegments[bar->selection], True, False);
    
    XmTextFieldSetString(bar->textfield, (char*)path);
    XmTextFieldSetInsertionPosition(bar->textfield, XmTextFieldGetLastPosition(bar->textfield));
    
    pathbar_resize(bar->widget, bar, NULL);
}

void PathBarSetDirList(PathBar *bar, const char **dirlist, size_t nelm) {
    XmStringTable items = calloc(nelm, sizeof(XmString));
    for(int i=0;i<nelm;i++) {
        items[i] = XmStringCreateLocalized((char*)dirlist[i]);
    }
    XmListDeleteAllItems(bar->list);
    XmListAddItems(bar->list, items, nelm, 0);
    XmListSelectPos(bar->list, 1, False);
    for(int i=0;i<nelm;i++) {
        XmStringFree(items[i]);
    }
    free(items);
}

void PathBarDestroy(PathBar *bar) {
    if(bar->path) {
        free(bar->path);
    }
    free(bar->pathSegments);
    free(bar);
}

/* ------------------------------ public API ------------------------------ */

Widget XnCreateFileSelectionDialog(
        Widget parent,
        String name,
        ArgList arglist,
        Cardinal argcount)
{
    Widget dialog = XmCreateDialogShell(parent, "FileDialog", NULL, 0);
    Widget fsb = XnCreateFileSelectionBox(dialog, name, arglist, argcount);
    char *title = FSBDialogTitle(fsb);
    XtVaSetValues(dialog, XmNtitle, title, NULL);
    return fsb;
}

Widget XnCreateFileSelectionBox(
        Widget parent,
        String name,
        ArgList arglist,
        Cardinal argcount)
{
    Widget fsb = XtCreateWidget(name, xnFsbWidgetClass, parent, arglist, argcount);
    return fsb;
}

void XnFileSelectionBoxAddView(
        Widget fsb,
        const char *name,
        FSBViewCreateProc create,
        FSBViewUpdateProc update,
        FSBViewSelectProc select,
        FSBViewCleanupProc cleanup,
        FSBViewDestroyProc destroy,
        Boolean useDirList,
        void *userData)
{
    XnFileSelectionBox f = (XnFileSelectionBox)fsb;
    if(f->fsb.numviews >= FSB_MAX_VIEWS) {
        fprintf(stderr, "XnFileSelectionBox: too many views\n");
        return;
    }
    
    FSBView view;
    view.update = update;
    view.select = select;
    view.cleanup = cleanup;
    view.destroy = destroy;
    view.useDirList = useDirList;
    view.userData = userData;
    
    FSBViewWidgets widgets = CreateView(f, create, userData, useDirList);
    view.widget = widgets.view;
    view.focus = widgets.focus;
    
    AddViewMenuItem(f, name, f->fsb.numviews);
    
    f->fsb.view[f->fsb.numviews++] = view;
}

void XnFileSelectionBoxSetDirList(Widget fsb, const char **dirlist, size_t nelm) {
    XnFileSelectionBox f = (XnFileSelectionBox)fsb;
    PathBarSetDirList(f->fsb.pathBar, dirlist, nelm);
}

Widget XnFileSelectionBoxWorkArea(Widget fsb) {
    XnFileSelectionBox f = (XnFileSelectionBox)fsb;
    return f->fsb.workarea;
}

Widget XnFileSelectionBoxGetChild(Widget fsb, enum XnFSBChild child) {
    XnFileSelectionBox w = (XnFileSelectionBox)fsb;
    switch(child) {
        case XnFSB_DIR_UP_BUTTON: return w->fsb.dirUp;
        case XnFSB_HOME_BUTTON: return w->fsb.home;
        case XnFSB_NEW_FOLDER_BUTTON: return w->fsb.newFolder;
        case XnFSB_DETAIL_TOGGLE_BUTTON: return w->fsb.detailToggleButton;
        case XnFSB_VIEW_OPTION_BUTTON: return w->fsb.viewOption;
        case XnFSB_FILTER_DROPDOWN: return w->fsb.filter;
        case XnFSB_FILTER_BUTTON: return w->fsb.filterButton;
        case XnFSB_SHOW_HIDDEN_TOGGLE_BUTTON: return w->fsb.showHiddenButtonW;
        case XnFSB_DIRECTORIES_LABEL: return w->fsb.lsDirLabel;
        case XnFSB_FILES_LABEL: return w->fsb.lsFileLabel;
        case XnFSB_DIRLIST: return w->fsb.dirlist;
        case XnFSB_FILELIST: return w->fsb.filelist;
        case XnFSB_GRID: return w->fsb.grid;
        case XnFSB_OK_BUTTON: return w->fsb.okBtn;
        case XnFSB_CANCEL_BUTTON: return w->fsb.cancelBtn;
        case XnFSB_HELP_BUTTON: return w->fsb.helpBtn;
    }
    return NULL;
}

void XnFileSelectionBoxDeleteFilters(Widget fsb) {
    XnFileSelectionBox w = (XnFileSelectionBox)fsb;
    Widget filterList = XmDropDownGetList(w->fsb.filter);
    XmListDeleteAllItems(filterList);
}

void XnFileSelectionBoxAddFilter(Widget fsb, const char *filter) {
    XnFileSelectionBox w = (XnFileSelectionBox)fsb;
    Widget filterList = XmDropDownGetList(w->fsb.filter);
    
    XmString str = XmStringCreateSimple((char*)filter);
    XmListAddItem(filterList, str, 0);
    XmStringFree(str);
}
