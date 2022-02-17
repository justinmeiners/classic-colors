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

#ifndef FSB_H
#define FSB_H

#include <X11/Intrinsic.h>
#include <Xm/PrimitiveP.h>

#ifdef __cplusplus
extern "C" {
#endif

extern WidgetClass xnFsbWidgetClass;

#define FILEDIALOG_OPEN 1
#define FILEDIALOG_SAVE 2
    
#define FILEDIALOG_OK 1
#define FILEDIALOG_CANCEL 2

#define XnNwidgetSpacing         "fsbWidgetSpacing"
#define XnNwindowSpacing         "fsbWindowSpacing"

#define XnNfsbType               "fsbType"
#define XnCfsbType               "fsbType"

#define XnNshowHidden            "showHidden"
#define XnCshowHidden            "showHidden"
#define XnNshowHiddenButton      "showHiddenButton"
#define XnCshowHiddenButton      "showHiddenButton"

#define XnNshowViewMenu          "showViewMenu"
#define XnCshowViewMenu          "showViewMenu"

#define XnNselectedView          "fsbSelectedView"
#define XnCselectedView          "fsbSelectedView"

#define XnNdirectory             "directory"
#define XnCdirectory             "directory"
#define XnNselectedPath          "selectedPath"
#define XnCselectedPath          "selectedPath"
#define XnNhomePath              "homePath"
#define XnChomePath              "homePath"

#define XnNfilter                "filter"
#define XnCfilter                "filter"

#define XnNfilterFunc            "filterFunc"
#define XnCfilterFunc            "filterFunc"

#define XnNlabelListView         "labelListView"
#define XnClabelListView         "labelListView"
#define XnNlabelDetailView       "labelDetailView"
#define XnClabelDetailView       "labelDetailView"
#define XnNlabelOpenFileTitle    "labelOpenFileTitle"
#define XnClabelOpenFileTitle    "labelOpenFileTitle"
#define XnNlabelSaveFileTitle    "labelSaveFileTitle"
#define XnClabelSaveFileTitle    "labelSaveFileTitel"
#define XnNlabelDirUp            "labelDirUp"
#define XnClabelDirUp            "labelDirUp"
#define XnNlabelHome             "labelHome"
#define XnClabelHome             "labelHome"
#define XnNlabelNewFolder        "labelNewFolder"
#define XnClabelNewFolder        "labelNewFolder"
#define XnNlabelFilter           "labelFilter"
#define XnClabelFilter           "labelFilter"
#define XnNlabelFilterButton     "labelFilterButton"
#define XnClabelFilterButton     "labelFilterButton"
#define XnNlabelShowHiddenFiles  "labelShowHiddenFiles"
#define XnClabelShowHiddenFiles  "labelShowHiddenFiles"
#define XnNlabelDirectories      "labelDirectories"
#define XnClabelDirectories      "labelDirectories"
#define XnNlabelFiles            "labelFiles"
#define XnClabelFiles            "labelFiles"
#define XnNlabelRename           "labelRename"
#define XnClabelRename           "labelRename"
#define XnNlabelDelete           "labelDelete"
#define XnClabelDelete           "labelDelete"
#define XnNlabelOpen             "labelOpen"
#define XnClabelOpen             "labelOpen"
#define XnNlabelSave             "labelSave"
#define XnClabelSave             "labelSave"
#define XnNlabelOk               "labelOk"
#define XnClabelOk               "labelOk"
#define XnNlabelCancel           "labelCancel"
#define XnClabelCancel           "labelCancel"
#define XnNlabelHelp             "labelHelp"
#define XnClabelHelp             "labelHelp"
#define XnNlabelFileName         "labelFileName"
#define XnClabelFileName         "labelFileName"
#define XnNlabelDirectoryName    "labelDirectoryName"
#define XnClabelDirectoryName    "labelDirectoryName"
#define XnNlabelNewFileName      "labelNewFileName"
#define XnClabelNewFileName      "labelNewFileName"
#define XnNlabelDeleteFile       "labelDeleteFile"
#define XnClabelDeleteFile       "labelDeleteFile"
#define XnNdetailHeadings        "detailHeadings"
#define XnCdetailHeadings        "detailHeadings"
#define XnNdateFormatSameYear    "dateFormatSameYear"
#define XnCdateFormatSameYear    "dateFormatSameYear"
#define XnNdateFormatOtherYear   "dateFormatOtherYear"
#define XnCdateFormatOtherYear   "dateFormatOtherYear"
#define XnNsuffixBytes           "suffixBytes"
#define XnCsuffixBytes           "suffixBytes"
#define XnNsuffixKB              "suffixKB"
#define XnCsuffixKB              "suffixKB"
#define XnNsuffixMB              "suffixMB"
#define XnCsuffixMB              "suffixMB"
#define XnNsuffixGB              "suffixGB"
#define XnCsuffixGB              "suffixGB"
#define XnNsuffixTB              "suffixTB"
#define XnCsuffixTB              "suffixTB"
#define XnNerrorTitle            "errorTitle"
#define XnCerrorTitle            "errorTitle"
#define XnNerrorIllegalChar      "errorIllegalChar"
#define XnCerrorIllegalChar      "errorIllegalChar"
#define XnNerrorRename           "errorRename"
#define XnCerrorRename           "errorRename"
#define XnNerrorCreateFolder     "errorCreateFolder"
#define XnCerrorCreateFolder     "errorCreateFolder"
#define XnNerrorDelete           "errorDelete"
#define XnCerrorDelete           "errorDelete"
#define XnNerrorOpenDir          "errorOpenDir"
#define XnCerrorOpenDir          "errorOpenDir"

/*
 * int FSBFilterFunc(const char *pattern, const char *string)
 * 
 * Checks checks whether the string matches the pattern
 * 
 * Return
 *   zero if the string matches the pattern
 *   non-zero if there is no match
 */
typedef int(*FSBFilterFunc)(const char*, const char*);

typedef int(*updatedir_callback)(void*,char*);

typedef struct PathBar {  
    Widget widget;
    Widget textfield;
    
    Widget down;
    Widget left;
    Widget right;
    Dimension lw;
    Dimension rw;
    
    Widget popup;
    Widget list;
    
    int shift;
    
    Widget *pathSegments;
    size_t numSegments;
    size_t segmentAlloc;
    
    char *path;
    int selection;
    Boolean input;
    
    Widget hs;
    Widget vs;
    int popupScrollEvent;
    
    updatedir_callback updateDir;
    void *updateDirData;
    
    Boolean disableResize;
} PathBar;

typedef struct FileElm FileElm;
struct FileElm {
    char *path;
    int isDirectory;
    unsigned long long size;
    time_t lastModified;
};

typedef struct {
    Widget view;
    Widget focus;
} FSBViewWidgets;

enum XnFSBChild {
    XnFSB_DIR_UP_BUTTON = 0,
    XnFSB_HOME_BUTTON,
    XnFSB_NEW_FOLDER_BUTTON,
    XnFSB_DETAIL_TOGGLE_BUTTON,
    XnFSB_VIEW_OPTION_BUTTON,
    XnFSB_FILTER_DROPDOWN,
    XnFSB_FILTER_BUTTON,
    XnFSB_SHOW_HIDDEN_TOGGLE_BUTTON,
    XnFSB_DIRECTORIES_LABEL,
    XnFSB_FILES_LABEL,
    XnFSB_DIRLIST,
    XnFSB_FILELIST,
    XnFSB_GRID,
    XnFSB_OK_BUTTON,
    XnFSB_CANCEL_BUTTON,
    XnFSB_HELP_BUTTON
};

typedef FSBViewWidgets(*FSBViewCreateProc)(Widget parent, ArgList args, int n, void *userData);
typedef void(*FSBViewUpdateProc)(Widget fsb, Widget view, FileElm *dirs, int dircount, FileElm *files, int filecount, const char *filter, int maxnamelen, void *userData);
typedef void(*FSBViewSelectProc)(Widget fsb, Widget view, const char *item);
typedef void(*FSBViewCleanupProc)(Widget fsb, Widget view, void *userData);
typedef void(*FSBViewDestroyProc)(Widget fsb, Widget view, void *userData);

PathBar* CreatePathBar(Widget parent, ArgList args, int n);
void PathBarSetPath(PathBar *bar, const char *path);
void PathBarSetDirList(PathBar *bar, const char **dirlist, size_t nelm);
void PathBarDestroy(PathBar *bar);

Widget XnCreateFileSelectionDialog(
        Widget parent,
        String name,
        ArgList arglist,
        Cardinal argcount);

Widget XnCreateFileSelectionBox(
        Widget parent,
        String name,
        ArgList arglist,
        Cardinal argcount);

void XnFileSelectionBoxAddView(
        Widget fsb,
        const char *name,
        FSBViewCreateProc create,
        FSBViewUpdateProc update,
        FSBViewSelectProc select,
        FSBViewCleanupProc cleanup,
        FSBViewDestroyProc destroy,
        Boolean useDirList,
        void *userData);

void XnFileSelectionBoxSetDirList(Widget fsb, const char **dirlist, size_t nelm);

Widget XnFileSelectionBoxWorkArea(Widget fsb);

Widget XnFileSelectionBoxGetChild(Widget fsb, enum XnFSBChild child);

void XnFileSelectionBoxDeleteFilters(Widget fsb);

void XnFileSelectionBoxAddFilter(Widget fsb, const char *filter);

#ifdef __cplusplus
}
#endif

#endif /* FSB_H */
