/*
	main.cpp

	MythArchive - mythtv plugin
	
	Starting point for the MythArchive module
*/
#include <iostream>
#include <cstdlib>
#include <signal.h>

using namespace std;

// Qt
#include <QApplication>
#include <QDir>
#include <QTimer>

// mythtv
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/mythplugin.h>
#include <mythtv/util.h>
#include <libmythui/myththemedmenu.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythdialogbox.h>

// mytharchive
#include "archivesettings.h"
#include "logviewer.h"
#include "fileselector.h"
#include "recordingselector.h"
#include "videoselector.h"
#include "dbcheck.h"
#include "archiveutil.h"
#include "selectdestination.h"
#include "exportnative.h"
#include "importnative.h"
#include "mythburn.h"

// return true if the process belonging to the lock file is still running
bool checkProcess(const QString &lockFile)
{
    // read the PID from the lock file
    QFile file(lockFile);

    bool bOK = file.open(QIODevice::ReadOnly);

    if (!bOK)
    {
        VERBOSE(VB_GENERAL, QString("Unable to open file %1").arg(lockFile));

        return true;
    }

    QString line(file.readLine(100));

    pid_t pid = line.toInt(&bOK);

    if (!bOK)
    {
        VERBOSE(VB_GENERAL, QString("Got bad PID '%1' from lock file").arg(pid));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Checking if PID %1 is still running").arg(pid));

    if (kill(pid, 0) == -1)
    {
        if (errno == ESRCH)
            return false;
    }

    return true;
}

// return true if a lock file is found and the owning process is still running
bool checkLockFile(const QString &lockFile)
{
    QFile file(lockFile);

    //is a job already running?
    if (file.exists())
    {
        // Is the process that created the lock still alive?
        if (!checkProcess(lockFile))
        {
            showWarningDialog(QObject::tr("Found a lock file but the owning process isn't running!\n"
                                          "Removing stale lock file."));
            if (!file.remove())
                VERBOSE(VB_IMPORTANT, QString("Failed to remove stale lock file - %1")
                        .arg(lockFile));
        }
        else
        {
            return true;
        }
    }

    return false;
}

void runCreateDVD(void)
{
    QString commandline;
    QString tempDir = getTempDirectory(true);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    // show the select destination dialog
    SelectDestination *dest = new SelectDestination(mainStack, false, "SelectDestination");

    if (dest->Create())
        mainStack->AddScreen(dest);
}

void runCreateArchive(void)
{
    QString commandline;
    QString tempDir = getTempDirectory(true);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    // show the select destination dialog
    SelectDestination *dest = new SelectDestination(mainStack, true, "SelectDestination");

    if (dest->Create())
        mainStack->AddScreen(dest);
}

void runEncodeVideo(void)
{

}

void runImportVideo(void)
{
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    QString filter = "*.xml";

    // show the find archive screen
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ArchiveFileSelector *selector = new ArchiveFileSelector(mainStack);

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void runShowLog(void)
{
    showLogViewer();
}

void runTestDVD(void)
{
    if (!gContext->GetSetting("MythArchiveLastRunType").startsWith("DVD"))
    {
        showWarningDialog(QObject::tr("Last run did not create a playable DVD."));
        return;
    }

    if (!gContext->GetSetting("MythArchiveLastRunStatus").startsWith("Success"))
    {
        showWarningDialog(QObject::tr("Last run failed to create a DVD."));
        return;
    }

    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString filename = tempDir + "work/dvd/";
    QString command = gContext->GetSetting("MythArchiveDVDPlayerCmd", "");

    if ((command.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
         (command.length() < 1))
    {
        filename = QString("dvd:/") + filename;
        command = "Internal";
        gContext->GetMainWindow()->HandleMedia(command, filename);
        return;
    }
    else
    {
        if (command.contains("%f"))
            command = command.replace(QRegExp("%f"), filename);
        myth_system(command);
    }
}

void runBurnDVD(void)
{
    BurnMenu *menu = new BurnMenu();
    menu->start();
}

void ArchiveCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.toLower();

    if (sel == "archive_create_dvd")
        runCreateDVD();
    else if (sel == "archive_create_archive")
        runCreateArchive();
    else if (sel == "archive_encode_video")
        runEncodeVideo();
    else if (sel == "archive_import_video")
        runImportVideo();
    else if (sel == "archive_last_log")
        runShowLog();
    else if (sel == "archive_test_dvd")
        runTestDVD();
    else if (sel == "archive_burn_dvd")
        runBurnDVD();
}

void runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "archive menu");

    diag->setCallback(ArchiveCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
    }
}

void initKeys(void)
{
    REG_KEY("Archive", "TOGGLECUT", "Toggle use cut list state for selected program", "C");
    REG_KEY("Archive", "DELETEITEM", "Delete current item in list", "D");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mytharchive", libversion,
                                    MYTH_BINARY_VERSION))
    {
        VERBOSE(VB_IMPORTANT, "Test Popup Version Failed ");
        return -1;
    }

    gContext->ActivateSettingsCache(false);
    if (!UpgradeArchiveDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(false);

    ArchiveSettings settings;
    settings.Load();
    settings.Save();

    initKeys();

    return 0;
}

int mythplugin_run(void)
{
    runMenu("archivemenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    ArchiveSettings settings;
    settings.exec();

    return 0;
}
