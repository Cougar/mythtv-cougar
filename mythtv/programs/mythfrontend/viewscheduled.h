#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"

class QSqlDatabase;

class ViewScheduled : public MythDialog
{
    Q_OBJECT
  public:
    ViewScheduled(QSqlDatabase *ldb, MythMainWindow *parent, 
                  const char *name = 0);
    ~ViewScheduled();

  protected slots:
    void edit();
    void upcoming();
    void details();
    void selected();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

  private:
    void FillList(void);
    void setShowAll(bool all);

    void updateBackground(void);
    void updateList(QPainter *);
    void updateConflict(QPainter *);
    void updateShowLevel(QPainter *);
    void updateInfo(QPainter *);
    void updateRecStatus(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    QSqlDatabase *db;

    QPixmap myBackground;

    bool conflictBool;
    QString dateformat;
    QString timeformat;
    QString channelFormat;

    QRect listRect;
    QRect infoRect;
    QRect conflictRect;
    QRect showLevelRect;
    QRect recStatusRect;
    QRect fullRect;

    int listsize;

    bool showAll;

    bool inEvent;
    bool inFill;
    bool needFill;

    int listPos;
    ProgramList recList;
};

#endif
