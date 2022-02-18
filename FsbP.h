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

#ifndef FSBP_H
#define FSBP_H

#include <X11/CoreP.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/ManagerP.h>
#include <Xm/FormP.h>

#include "Fsb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FSB_MAX_VIEWS 8
    

typedef struct FSBView FSBView;
struct FSBView {
    Widget widget;
    Widget focus;
    FSBViewUpdateProc  update;
    FSBViewSelectProc  select;
    FSBViewCleanupProc cleanup;
    FSBViewDestroyProc destroy;
    void *userData;
    Boolean useDirList;
};

    
typedef struct FSBClassPart {
    int unused;
} FSBClassPart;

typedef struct FSBClassRec {
    CoreClassPart             core_class;
    CompositeClassPart        composite_class;
    ConstraintClassPart       constraint_class;
    XmManagerClassPart        manager_class;
    XmBulletinBoardClassPart  bulletin_board_class;
    XmFormClassPart           form_class;
    FSBClassPart              fsb_class;
} FSBClassRec;

typedef struct FSBPart {
    XtCallbackList okCallback;
    XtCallbackList cancelCallback;
    
    Dimension widgetSpacing;
    Dimension windowSpacing;
    
    Boolean showHiddenButton;
    
    Widget path;
    PathBar *pathBar;
    Widget filter;
    Widget filterButton;
    Widget showHiddenButtonW;
    
    FSBFilterFunc filterFunc;
    
    char *filterStr;
    
    Widget dirUp;
    Widget home;
    Widget newFolder;
    
    Widget viewSelectorList;
    Widget viewSelectorDetail;
    
    Widget viewMenu;
    Widget viewOption;
    Widget detailToggleButton;
    
    Widget filterForm;
    Widget lsDirLabel;
    Widget lsFileLabel;
    
    Widget listContextMenu;
    Widget gridContextMenu;
    
    // icon view
    
    // dir/file list view
    Widget listform;
    Widget dirlist;
    
    FSBView view[FSB_MAX_VIEWS];
    int numviews;
    int selectedview;
    
    Widget filelist;
    Widget grid;
    
    Widget separator;
    
    Widget nameLabel;
    Widget name;
    
    Widget bottom_widget;
    
    Widget workarea;
    
    Widget okBtn;
    Widget cancelBtn;
    Widget helpBtn;
    
    FileElm *dirs;
    FileElm *files;
    int dircount;
    int filecount;
    int maxnamelen;
    
    char *homePath;
    
    char *currentPath;
    char *selectedPath;
    int selIsDir;
    Boolean showHidden;
    Boolean showViewMenu;
      
    int type;
    
    int end;
    int status;
    
    int disable_set_values;
    int gui_created;
        
    char *labelListView;
    char *labelDetailView;
    char* labelOpenFileTitle;
    char* labelSaveFileTitle;
    XmString labelDirUp;
    XmString labelHome;
    XmString labelNewFolder;
    XmString labelFilterButton;
    XmString labelShowHiddenFiles;
    XmString labelDirectories;
    XmString labelFiles;
    XmString labelRename;
    XmString labelDelete;
    XmString labelOpen;
    XmString labelSave;
    XmString labelOk;
    XmString labelCancel;
    XmString labelHelp;
    XmString labelFileName;
    XmString labelDirectoryName;
    XmString labelNewFileName;
    char *labelDeleteFile;
    
    char *detailHeadings;
    
    char *dateFormatSameYear;
    char *dateFormatOtherYear;
    char *suffixBytes;
    char *suffixKB;
    char *suffixMB;
    char *suffixGB;
    char *suffixTB;
    
    char *errorTitle;
    char *errorIllegalChar;
    char *errorRename;
    char *errorFolder;
    char *errorDelete;
    char *errorOpenDir;
} FSBPart;

typedef struct FSBRec {
   CorePart	        core;
   CompositePart        composite;
   ConstraintPart       constraint;
   XmManagerPart        manager;
   XmBulletinBoardPart  bulletin_board;
   XmFormPart           form;
   FSBPart              fsb;
} FSBRec;

typedef struct FSBRec *XnFileSelectionBox;

#ifdef __cplusplus
}
#endif

#endif /* FSBP_H */

