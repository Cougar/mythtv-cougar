#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <QDateTime>

#include "programlist.h"

#include "mythscreentype.h"

class TV;
class Timer;

class MythUIText;
class MythUIStateType;
class MythUIButtonList;
class MythUIButtonListItem;

/**
 * \class ViewScheduled
 *
 * \brief Screen for viewing and managing upcoming and conflicted recordings
 */
class ViewScheduled : public MythScreenType
{
    Q_OBJECT
  public:
    ViewScheduled(MythScreenStack *parent, TV *player = NULL,
                  bool showTV = false);
    ~ViewScheduled();

    static void * RunViewScheduled(void *player, bool);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  protected slots:
    void ChangeGroup(MythUIButtonListItem *item);
    void edit();
    void customEdit();
    void deleteRule();
    void upcoming();
    void details();
    void selected(MythUIButtonListItem *);
    void updateInfo(MythUIButtonListItem *);
    void SwitchList(void);

  private:
    void FillList(void);
    void LoadList(void);
    void setShowAll(bool all);
    void viewCards(void);
    void viewInputs(void);

    void EmbedTVWindow(void);

    bool m_conflictBool;
    QDate m_conflictDate;
    QString m_shortdateFormat;
    QString m_dateFormat;
    QString m_timeFormat;
    QString m_channelFormat;

    QRect m_tvRect;

    MythUIButtonList *m_schedulesList;
    MythUIButtonList *m_groupList;

    bool m_showAll;

    bool m_inEvent;
    bool m_inFill;
    bool m_needFill;

    int m_listPos;
    ProgramList m_recList;
    QMap<QDate, ProgramList> m_recgroupList;

    QDate m_currentGroup;
    QDate m_defaultGroup;

    QMap<int, int> m_cardref;
    int m_maxcard;
    int m_curcard;

    QMap<int, int> m_inputref;
    int m_maxinput;
    int m_curinput;

    TV *m_player;
};

#endif
