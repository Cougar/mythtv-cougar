#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "scheduledrecording.h"

class QSqlDatabase;

class ProgramRecPriorityInfo : public ProgramInfo
{
  public:
    ProgramRecPriorityInfo();
    ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other);
    ProgramRecPriorityInfo& operator=(const ProgramInfo&);


    int channelRecPriority;
    int recTypeRecPriority;
    RecordingType recType;
};

class ProgramRecPriority : public MythDialog
{
    Q_OBJECT
  public:
    enum SortType
    {
        byTitle,
        byRecPriority,
    };

    ProgramRecPriority(QSqlDatabase *ldb, MythMainWindow *parent, 
                 const char *name = 0);
    ~ProgramRecPriority();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void changeRecPriority(int howMuch);
    void saveRecPriority(void);
    void edit();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void SortList();
    QMap<QString, ProgramRecPriorityInfo> programData;
    QMap<QString, int> origRecPriorityData;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ProgramRecPriorityInfo *curitem;

    QSqlDatabase *db;

    QPixmap myBackground;
    QPixmap *bgTransBackup;

    bool pageDowner;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    QRect listRect;
    QRect infoRect;
    QRect fullRect;

    int listsize;

    SortType sortType;
};

#endif
