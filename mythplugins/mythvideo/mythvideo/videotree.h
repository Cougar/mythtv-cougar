#ifndef VIDEOTREE_H_
#define VIDEOTREE_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>


#include "videodlg.h"


class QSqlDatabase;

class VideoTree : public VideoDialog
{
    Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;

    VideoTree(QSqlDatabase *ldb, 
              MythMainWindow *parent, const char *name = 0);
   ~VideoTree();

    void buildFileList(QString directory);
    bool ignoreExtension(QString extension);
    
  public slots:
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector* = NULL);
    virtual void slotParentalLevelChanged();
    virtual void slotWatchVideo();
    

  protected:
    void keyPressEvent(QKeyEvent *e);
    void doMenu(bool info);
    virtual void handleMetaFetch(Metadata* meta);
    virtual void fetchVideos();


  private:
    void fillTreeFromFilesystem();
    void syncMetaToTree(int nodeNumber);
    void wireUpTheme();
    
    bool         file_browser;
    QStringList  browser_mode_files;
       
    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *video_tree_list;
    GenericTree           *video_tree_root;
    GenericTree           *video_tree_data;
    UITextType            *video_title;
    UITextType            *video_file;
    UITextType            *video_player;
    UITextType            *pl_value;
    UIImageType           *video_poster;
};



#endif
