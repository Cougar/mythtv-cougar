/*
        MythProgramFind
        Version 0.8
        January 19th, 2003
        Updated: 4/8/2003, Added support for new ui.xml 

        By John Danner

        Note: Portions of this code taken from MythMusic
*/


#ifndef PROGFIND_H_
#define PROGFIND_H_

#include <qdatetime.h>

#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"

class QListView;
class ProgramInfo;
class QSqlDatabase;
class QWidget;
class TV;

void RunProgramFind(bool thread = false);

class ProgFinder : public MythDialog
{
    struct showRecord 
    {
        QString title;
        QString subtitle;
        QString description;
        QString channelID;
        QString startDisplay;
        QString endDisplay;
        QString channelNum;
        QString channelCallsign;
        QString starttime;
        QString endtime;
	QDateTime startdatetime;
	int recording;
	QString recText;
    };

    struct recordingRecord 
    {
        QString chanid;
        QDateTime startdatetime;
        QString title;
        QString subtitle;
        QString description;
        int type;
    };

    Q_OBJECT
  public:
    ProgFinder(MythMainWindow *parent, const char *name = 0);
    virtual ~ProgFinder();

    void Initialize(void);

  private slots:
    void update_timeout();
    void escape();
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void getInfo(bool toggle = false);
    void showGuide();
    void pageUp();
    void pageDown();
    void select();
    void quickRecord();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

    virtual void fillSearchData(void);
    virtual void getAllProgramData(void);
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual QString whereClauseGetSearchData(int canNum);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void updateBackground();
    void updateList(QPainter *);
    void updateInfo(QPainter *);
   
    int showsPerListing;
    int curSearch;
    int curProgram;
    int curShow;
    int recordingCount;
    int searchCount;
    int listCount;
    int showCount;
    int inSearch;
    bool showInfo;
    bool pastInitial;
    bool running;
    int *gotInitData;

    QTimer *update_Timer;

    showRecord *showData;
    recordingRecord *curRecordings;

    TV *m_player;

    QString baseDir;
    QString curDateTime;
    QString curChannel;
    QString *searchData;
    QString *initData;
    QString *progData;

    QSqlDatabase *m_db;

    void showSearchList();
    void showProgramList();
    void showShowingList();
    void clearProgramList();
    void clearShowData();
    void selectSearchData();
    void selectShowData(QString);
    int checkRecordingStatus(int);
    void getRecordingInfo();
    void getSearchData(int);
    void getInitialProgramData();

    QRect listRect;
    QRect infoRect;

    QString dateFormat;
    QString timeFormat;

    bool allowkeypress;

    int displaychannum;
};

class JaProgFinder : public ProgFinder
{
  public:
    JaProgFinder(MythMainWindow *parent, const char *name = 0);

  protected:
    virtual void fillSearchData();
    virtual void getAllProgramData();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual QString whereClauseGetSearchData(int canNum);

  private:
    static const char* searchChars[];
    int numberOfSearchChars;
};

#endif
