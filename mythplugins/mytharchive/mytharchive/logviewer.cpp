#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>
#include <QFile>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitext.h>

// mytharchive
#include "archiveutil.h"
#include "logviewer.h"

const int DEFAULT_UPDATE_TIME = 5;

void showLogViewer(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    QString logDir = getTempDirectory() + "logs";

    // do any logs exist?
    if (QFile::exists(logDir + "/progress.log") || QFile::exists(logDir + "/mythburn.log"))
    {
        LogViewer *viewer = new LogViewer(mainStack);
        viewer->setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
        if (viewer->Create())
            mainStack->AddScreen(viewer);
    }
    else
        showWarningDialog(QObject::tr("Cannot find any logs to show!"));
}

LogViewer::LogViewer(MythScreenStack *parent)
          : MythScreenType(parent, "logviewer")
{
    m_updateTime = gContext->GetNumSetting("LogViewerUpdateTime", DEFAULT_UPDATE_TIME);
    m_updateTimer = NULL;
    m_autoUpdate = (gContext->GetNumSetting("LogViewerAutoUpdate", 1) == 1);
}

LogViewer::~LogViewer(void)
{
    gContext->SaveSetting("LogViewerUpdateTime", m_updateTime);
    gContext->SaveSetting("LogViewerAutoUpdate", (m_autoUpdate ? "1" : "0"));

    if (m_updateTimer)
        delete m_updateTimer;
}

bool LogViewer::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "logviewer", this);

    if (!foundtheme)
        return false;

    bool err = false; 
    UIUtilE::Assign(this, m_logList, "loglist", &err);
    UIUtilE::Assign(this, m_logText, "logitem_text", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_updateButton, "update_button", &err);
    UIUtilE::Assign(this, m_exitButton, "exit_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'logviewer'");
        return false;
    }

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelClicked()));

    m_updateButton->SetText(tr("Update"));
    connect(m_updateButton, SIGNAL(Clicked()), this, SLOT(updateClicked()));

    m_exitButton->SetText(tr("Exit"));
    connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_logList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(updateLogItem(MythUIButtonListItem*)));

    m_updateTimer = NULL;
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(updateTimerTimeout()) );

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_logList);

    return true;
}

void LogViewer::Init(void)
{
    updateClicked();
    if (m_logList->GetCount() > 0)
        m_logList->SetItemCurrent(m_logList->GetCount() - 1);
}

bool LogViewer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
         return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void LogViewer::updateTimerTimeout()
{
    updateClicked();
}

void LogViewer::toggleAutoUpdate(void)
{
    m_autoUpdate = ! m_autoUpdate;

    if (m_autoUpdate)
        m_updateTimer->start(m_updateTime * 1000);
    else
        m_updateTimer->stop();
}

void LogViewer::updateLogItem(MythUIButtonListItem *item)
{
    if (item)
        m_logText->SetText(item->text());
}

void LogViewer::cancelClicked(void)
{
    QString tempDir = gContext->GetSetting("MythArchiveTempDir", "");

    QString command("echo Cancel > " + tempDir + "/logs/mythburncancel.lck");
    system(qPrintable(command));

    ShowOkPopup(QObject::tr("Background creation has been asked to stop.\n" 
                            "This may take a few minutes."));
}

void LogViewer::updateClicked(void)
{
    m_updateTimer->stop();

    QStringList list;
    loadFile(m_currentLog, list, m_logList->GetCount());

    if (list.size() > 0)
    {
        bool bUpdateCurrent =
                (m_logList->GetCount() == m_logList->GetCurrentPos() + 1) ||
                (m_logList->GetCurrentPos() == 0);

        for (int x = 0; x < list.size(); x++)
            new MythUIButtonListItem(m_logList, list[x]);

        if (bUpdateCurrent)
            m_logList->SetItemCurrent(m_logList->GetCount() - 1);
    }

    bool bRunning = (getSetting("MythArchiveLastRunStatus") == "Running");

    if (!bRunning)
    {
        m_cancelButton->SetEnabled(false);
        m_updateButton->SetEnabled(false);
    }

    if (m_autoUpdate)
        m_updateTimer->start(m_updateTime * 1000);
}

QString LogViewer::getSetting(const QString &key)
{
    // read the setting direct from the DB rather than from the settings cache 
    // which isn't aware that the script may have changed something
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value = :VALUE "
                "AND hostname = :HOSTNAME ;");
        query.bindValue(":VALUE", key);
        query.bindValue(":HOSTNAME", gContext->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            return query.value(0).toString();
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
                QString("Database not open while trying to load setting: %1")
                        .arg(key));
    }

    return QString("");
}

bool LogViewer::loadFile(QString filename, QStringList &list, int startline)
{
    list.clear();

    QFile file(filename);

    if (!file.exists())
        return false;

    if (file.open( QIODevice::ReadOnly ))
    {
        QString s;
        QTextStream stream(&file);

         // ignore the first startline lines
        while ( !stream.atEnd() && startline > 0)
        {
            stream.readLine();
            startline--;
        }

         // read rest of file
        while ( !stream.atEnd() )
        {
            s = stream.readLine();
            list.append(s);
        }
        file.close();
    }
    else
        return false;

    return true;
}

void LogViewer::setFilenames(const QString &progressLog, const QString &fullLog)
{
    m_progressLog = progressLog;
    m_fullLog = fullLog;
    m_currentLog = progressLog;
}

void LogViewer::showProgressLog(void)
{
    m_currentLog = m_progressLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showFullLog(void)
{
    m_currentLog = m_fullLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox("Menu", popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    if (m_autoUpdate)
        menuPopup->AddButton(tr("Don't Auto Update"), SLOT(toggleAutoUpdate()));
    else
        menuPopup->AddButton(tr("Auto Update"), SLOT(toggleAutoUpdate()));

    menuPopup->AddButton(tr("Show Progress Log"), SLOT(showProgressLog()));
    menuPopup->AddButton(tr("Show Full Log"), SLOT(showFullLog()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}
