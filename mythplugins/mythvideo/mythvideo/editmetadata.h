#ifndef EDITMETADATA_H_
#define EDITMETADATA_H_

/*
	editmetadata.h

	(c) 2003 Thor Sigvaldason, Isaac Richards, and ?? ??
	Part of the mythTV project
	
    Class to let user edit the metadata associated with
    a given video

*/

#include <iostream>
using namespace std;
#include <qsqldatabase.h>
#include <mythtv/mythdialogs.h>

#include "metadata.h"

class EditMetadataDialog : public MythThemedDialog
{

  Q_OBJECT
  
    //
    //  Dialog to manipulate the data
    //
    
  public:
  
    EditMetadataDialog(QSqlDatabase *ldb,
                       Metadata *source_metadata,
                       MythMainWindow *parent, 
                       QString window_name,
                       QString theme_filename,
                       const char* name = 0);
    ~EditMetadataDialog();

    void keyPressEvent(QKeyEvent *e);
    void wireUpTheme();
    void fillWidgets();

  public slots:
  
    void takeFocusAwayFromEditor(bool up_or_down);
    void saveAndExit();
    void setTitle(QString new_title);
    void setCategory(int new_category);
    void setPlayer(QString new_player);
    void setLevel(int new_level);
    void setChild(int new_child);
    void toggleBrowse(bool yes_or_no);
    void findCoverArt();

  private:
  
    QSqlDatabase        *db;    
    Metadata            *working_metadata;

    //
    //  GUI stuff
    //  
    
    MythRemoteLineEdit  *title_editor;
    UIBlackHoleType     *title_hack;

    MythRemoteLineEdit  *player_editor;
    UIBlackHoleType     *player_hack;

    UISelectorType	*category_select;
    UISelectorType      *level_select;
    UISelectorType      *child_select;
    UICheckBoxType      *browse_check;
    UIPushButtonType    *coverart_button;
    UITextType          *coverart_text;
    UITextButtonType    *done_button;
 
};


#endif

class MythInputDialog: public MythDialog
{
  Q_OBJECT

    //
    //  Very simple class, not themeable
    //

  public:

    MythInputDialog( QString message,
                        bool *success,
                        QString *target,
                        MythMainWindow *parent, 
                        const char *name = 0, 
                        bool setsize = true);
    ~MythInputDialog();

 protected:
 
    void keyPressEvent(QKeyEvent *e);

  private:
  
    MythLineEdit        *text_editor;
    QString             *target_text;
    bool                *success_flag;
};

