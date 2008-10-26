#ifndef WELCOMEDIALOG_H_
#define WELCOMEDIALOG_H_

// qt
#include <QDateTime>

// myth
#include "remoteutil.h"
#include "programinfo.h"
#include "mythscreentype.h"
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythdialogbox.h"

class WelcomeDialog : public MythScreenType
{

  Q_OBJECT

  public:

    WelcomeDialog(MythScreenStack *parent, const char *name);
    ~WelcomeDialog();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *e);

  protected slots:
    void startFrontendClick(void);
    void startFrontend(void);
    void updateAll(void);
    void updateStatus(void);
    void updateScreen(void);
    void closeDialog(void);
    void updateTime(void);
    void showMenu(void);
    void shutdownNow(void);
    void runEPGGrabber(void);
    void lockShutdown(void);
    void unlockShutdown(void);
    bool updateRecordingList(void);
    bool updateScheduledList(void);

  private:
    void updateStatusMessage(void);
    bool checkConnectionToServer(void);
    void checkAutoStart(void);
    void runMythFillDatabase(void);

    MythUIText   *GetMythUIText(const QString &name, bool optional = false);
    MythUIButton *GetMythUIButton(const QString &name, bool optional = false);

    //
    //  GUI stuff
    //
    MythUIText    *m_status_text;
    MythUIText    *m_recording_text;
    MythUIText    *m_scheduled_text;
    MythUIText    *m_warning_text;
    MythUIText    *m_time_text;
    MythUIText    *m_date_text;

    MythUIButton  *m_startfrontend_button;

    MythDialogBox *m_menuPopup;

    QTimer        *m_updateStatusTimer;
    QTimer        *m_updateScreenTimer;
    QTimer        *m_timeTimer;

    QString        m_installDir;
    QString        m_timeFormat;
    QString        m_dateFormat;
    bool           m_isRecording;
    bool           m_hasConflicts;
    bool           m_bWillShutdown;
    int            m_secondsToShutdown;
    QDateTime      m_nextRecordingStart;
    int            m_preRollSeconds;
    int            m_idleWaitForRecordingTime;
    uint           m_screenTunerNo;
    uint           m_screenScheduledNo;
    uint           m_statusListNo;
    QStringList    m_statusList;

    vector<TunerStatus> m_tunerList;
    ProgramDetailList   m_scheduledList;

    QMutex      m_RecListUpdateMuxtex;
    bool        m_pendingRecListUpdate;

    bool pendingRecListUpdate() const           { return m_pendingRecListUpdate; }
    void setPendingRecListUpdate(bool newState) { m_pendingRecListUpdate = newState; }

    QMutex      m_SchedUpdateMuxtex;
    bool        m_pendingSchedUpdate;

    bool pendingSchedUpdate() const           { return m_pendingSchedUpdate; }
    void setPendingSchedUpdate(bool newState) { m_pendingSchedUpdate = newState; }

};

#endif
