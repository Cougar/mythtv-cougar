#include <unistd.h>      // for isatty() on Windows

#include "dialogbox.h"
#include "langsettings.h"
#include "mythcontext.h"
#include "schemawizard.h"
#include "util.h"

#include "libmythdb/mythtimer.h"
#include "libmythdb/mythverbose.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"


static SchemaUpgradeWizard * c_wizard = 0;


SchemaUpgradeWizard::SchemaUpgradeWizard(const QString &DBSchemaSetting,
                                         const QString &upgradeSchemaVal)
    : DBver(), didBackup(false), emptyDB(false), versionsBehind(-1),
      m_autoUpgrade(false),
      m_backupResult(),
      m_expertMode(false),
      m_schemaSetting(DBSchemaSetting),
      m_newSchemaVer(upgradeSchemaVal)
{
    c_wizard = this;

    // Users and developers can choose to live dangerously,
    // either to silently and automatically upgrade,
    // or an expert option to allow use of existing:
    switch (gContext->GetNumSetting("DBSchemaAutoUpgrade"))
    {
        case  1: m_autoUpgrade = true; break;
        case -1: m_expertMode  = true; break;
        default: break;
    }
}

SchemaUpgradeWizard::~SchemaUpgradeWizard()
{
    c_wizard = 0;
}

SchemaUpgradeWizard *
SchemaUpgradeWizard::Get(const QString &DBSchemaSetting,
                         const QString &upgradeSchemaVal)
{
    if (c_wizard == 0)
        c_wizard = new SchemaUpgradeWizard(DBSchemaSetting, upgradeSchemaVal);
    else
    {
        c_wizard->DBver            = QString();
        c_wizard->versionsBehind   = -1;
        c_wizard->m_schemaSetting  = DBSchemaSetting;
        c_wizard->m_newSchemaVer   = upgradeSchemaVal;
    }

    return c_wizard;
}

bool SchemaUpgradeWizard::BackupDB(void)
{
    if (emptyDB)
    {
        VERBOSE(VB_GENERAL,
                "The database seems to be empty - not attempting a backup");
        return false;
    }

    didBackup = DBUtil::BackupDB(m_backupResult);

    return didBackup;
}

int SchemaUpgradeWizard::Compare(void)
{
    DBver = gContext->GetSetting(m_schemaSetting);

    // No current schema? Investigate further:
    if (DBver.isEmpty() || DBver == "0")
    {
        VERBOSE(VB_GENERAL, "No current database version?");

        if (DBUtil::IsNewDatabase())
        {
            VERBOSE(VB_GENERAL, "Database appears to be empty/new!");
            emptyDB = true;
        }
    }
    else
        VERBOSE(VB_GENERAL, "Current Schema Version: " + DBver);

#if TESTING
    //DBver = "9" + DBver + "-testing";
    DBver += "-testing";
return 0;
#endif

    return versionsBehind = m_newSchemaVer.toInt() - DBver.toUInt();
}

int SchemaUpgradeWizard::CompareAndWait(const int seconds)
{
    if (Compare() > 0)  // i.e. if DB is older than expected
    {
        VERBOSE(VB_IMPORTANT, "Database schema is old."
                              " Waiting to see if DB is being upgraded.");

        MSqlQuery query(MSqlQuery::InitCon());
        bool      backupRunning  = false;
        bool      upgradeRunning = false;

        MythTimer elapsedTimer;
        elapsedTimer.start();
        while (versionsBehind && (elapsedTimer.elapsed() < seconds * 1000))
        {
            sleep(1);

            if (IsBackupInProgress())
            {
                VERBOSE(VB_IMPORTANT,
                        "Waiting for Database Backup to complete.");
                if (!backupRunning)
                {
                    elapsedTimer.restart();
                    backupRunning = true;
                }
                continue;
            }

            if (!lockSchema(query))
            {
                VERBOSE(VB_IMPORTANT,
                        "Waiting for Database Upgrade to complete.");
                if (!upgradeRunning)
                {
                    elapsedTimer.restart();
                    upgradeRunning = true;
                }
                continue;
            }

            Compare();
            unlockSchema(query);

            if (m_expertMode)  // Experts don't like to wait around :-)
                break;
        }

        if (versionsBehind)
            VERBOSE(VB_IMPORTANT, "Timed out waiting.");
        else
            VERBOSE(VB_IMPORTANT,
                    "Schema version was upgraded while we were waiting.");
    }
    // else DB is same version, or newer. Either way, we won't upgrade it

    return versionsBehind;
}


MythSchemaUpgrade SchemaUpgradeWizard::GuiPrompt(const QString &message,
                                                 bool upgradable, bool expert)
{
    DialogBox       * dlg;
    MythMainWindow  * win = gContext->GetMainWindow();

    if (!win)
        return MYTH_SCHEMA_ERROR;

    dlg = new DialogBox(win, message);
    dlg->AddButton(tr("Exit"));
    if (upgradable)
        dlg->AddButton(tr("Upgrade"));
    if (expert)
        dlg->AddButton(tr("Use current schema"));

    DialogCode selected = dlg->exec();
    dlg->deleteLater();

    switch (selected)
    {
        case kDialogCodeRejected:
        case kDialogCodeButton0:
            return MYTH_SCHEMA_EXIT;
        case kDialogCodeButton1:
            return upgradable ? MYTH_SCHEMA_UPGRADE: MYTH_SCHEMA_USE_EXISTING;
        case kDialogCodeButton2:
            return MYTH_SCHEMA_USE_EXISTING;
        default:
            return MYTH_SCHEMA_ERROR;
    }
}

/**
 * Tell the user that a schema needs to be upgraded, ask if that's OK,
 * remind them about backups, et c.  The GUI buttons default to Exit.
 * The shell command prompting requires an explicit "yes".
 *
 * \param name            What schema are we planning to upgrade? (TV? Music?)
 * \param upgradeAllowed  In not true, and DBSchemaAutoUpgrade isn't set for
 *                        expert mode, this is just a few information messages
 * \param upgradeIfNoUI   Default for non-interactive shells
 *
 * \todo Clarify whether the minDBMS stuff is just for upgrading,
 *       or if it is a runtime requirement too. If the latter,
 *       then this possibly should be called even if the schema match,
 *       to ensure the user is informed of the MySQL upgrade requirement.
 *
 * \todo This uses GetMythUI()->IsScreenSetup() to work out if this program's
 *       context is a GUI, but GetMythUI() might create a MythUIHelper.
 *       Having a static bool MythUIHelper::ValidMythUI() would be much tidier?
 */
enum MythSchemaUpgrade
SchemaUpgradeWizard::PromptForUpgrade(const char *name,
                                      const bool upgradeAllowed,
                                      const bool upgradeIfNoUI,
                                      const int  minDBMSmajor,
                                      const int  minDBMSminor,
                                      const int  minDBMSpoint)
{
    bool     connections;   // Are (other) FE/BEs connected?
    bool     gui;           // Was gContext Init'ed gui=true?
    bool     upgradable;    // Can/should we upgrade?
    bool     validDBMS;     // Do we measure up to minDBMS* ?
    QString  warnOldDBMS;
    QString  warnOtherCl;



    if (versionsBehind == -1)
        Compare();

#if minDBMS_is_only_for_schema_upgrades
    if (versionsBehind == 0)              // Why was this method even called?
        return MYTH_SCHEMA_USE_EXISTING;
#endif


    connections = CountClients() > 1;
    gui         = GetMythUI()->IsScreenSetup();
    validDBMS   = (minDBMSmajor == 0)   // If the caller provided no version,
                  ? true                // the upgrade code can't be fussy!
                  : CompareDBMSVersion(minDBMSmajor,
                                       minDBMSminor, minDBMSpoint) >= 0;
    upgradable  = validDBMS && (versionsBehind > 0)
                            && (upgradeAllowed || m_expertMode);


    // Build up strings used both in GUI and command shell contexts:
    if (connections)
        warnOtherCl = tr("There are also other clients using this"
                         " database. They should be shut down first.");
    if (!validDBMS)
        warnOldDBMS = tr("Error: This version of Myth%1"
                         " requires MySQL %2.%3.%4 or later."
                         "  You seem to be running MySQL version %5.")
                      .arg(name).arg(minDBMSmajor).arg(minDBMSminor)
                      .arg(minDBMSpoint).arg(GetDBMSVersion());



    //
    // 1. Deal with the trivial cases (No user prompting required)
    //
    if (validDBMS)
    {
        // Empty database? Always upgrade, to create tables
        if (emptyDB)
            return MYTH_SCHEMA_UPGRADE;

        if (m_autoUpgrade && !connections && upgradable)
            return MYTH_SCHEMA_UPGRADE;
    }

    if (!gui && (!isatty(fileno(stdin)) || !isatty(fileno(stdout))))
    {
        VERBOSE(VB_GENERAL, "Console is non-interactive, can't prompt user...");

        if (m_expertMode)
        {
            VERBOSE(VB_IMPORTANT, "Using existing schema.");
            return MYTH_SCHEMA_USE_EXISTING;
        }

        if (!validDBMS)
        {
            VERBOSE(VB_IMPORTANT, warnOldDBMS);
            return MYTH_SCHEMA_EXIT;
        }

        if (upgradeIfNoUI && validDBMS)
        {
            VERBOSE(VB_IMPORTANT, "Upgrading.");
            return MYTH_SCHEMA_UPGRADE;
        }

        return MYTH_SCHEMA_EXIT;
    }



    //
    // 2. Build up a compound message to show the user, wait for a response
    //
    enum MythSchemaUpgrade  returnValue = MYTH_SCHEMA_UPGRADE;
    QString                 message;

    if (upgradable)
    {
        if (m_autoUpgrade && connections)
        {
            message = tr("Error: MythTV cannot upgrade the schema of this"
                         " datatase because other clients are using it.\n\n"
                         "Please shut them down before upgrading.");
            returnValue = MYTH_SCHEMA_ERROR;
        }
        else
        {
            message = tr("Warning: MythTV wants to upgrade your database,")
                      + "\n" + tr("for the %1 schema, from %2 to %3.");
            if (m_expertMode)
                message += "\n\n" +
                           tr("You can try using the old schema,"
                              " but that may cause problems.");
        }
    }
    else if (!validDBMS)
    {
        message = warnOldDBMS;
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else if (versionsBehind > 0)
    {
        message = tr("This version of MythTV requires an updated database. ")
                  + tr("(schema is %1 versions behind)").arg(versionsBehind)
                  + "\n\n" + tr("Please run mythtv-setup or mythbackend "
                                "to update your database.");
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else   // This client is too old
    {
        if (m_expertMode)
            message = tr("Warning: MythTV database has newer"
                         " %1 schema (%2) than expected (%3).");
        else
        {
            message = tr("Error: MythTV database has newer"
                         " %1 schema (%2) than expected (%3).");
            returnValue = MYTH_SCHEMA_ERROR;
        }
    }

    if (!didBackup)
        message += "\n" + tr("MythTV was unable to backup your database.");

    if (message.contains("%1"))
        message = message.arg(name).arg(DBver).arg(m_newSchemaVer);


    if (gui)
    {
        if (returnValue == MYTH_SCHEMA_ERROR)
        {
            // Display error, return warning to caller
            MythPopupBox::showOkPopup(gContext->GetMainWindow(), "", message);
            return MYTH_SCHEMA_ERROR;
        }

        if (GuiPrompt(message, upgradable, m_expertMode) == MYTH_SCHEMA_EXIT)
            return MYTH_SCHEMA_EXIT;

        // The annoying extra confirmation:
        if (didBackup)
        {
            int dirPos = m_backupResult.lastIndexOf('/');
            QString dirName;
            QString fileName;
            if (dirPos > 0)
            {
                fileName = m_backupResult.mid(dirPos + 1);
                dirName  = m_backupResult.left(dirPos);
            }
            message = tr("If your system becomes unstable, a database"
                         " backup file called\n%1\nis located in %2")
                      .arg(fileName).arg(dirName);
        }
        else
            message = tr("This cannot be un-done, so having a"
                         " database backup would be a good idea.");
        if (connections)
            message += "\n\n" + warnOtherCl;

        return GuiPrompt(message, upgradable, m_expertMode);
    }

    // We are not in a GUI environment, so try to prompt the user in the shell
    QString resp;

    cout << endl << message.toLocal8Bit().constData() << endl << endl;

    if (returnValue == MYTH_SCHEMA_ERROR)
        return MYTH_SCHEMA_ERROR;

    if (!didBackup)
        cout << "WARNING: MythTV was unable to backup your database."
             << endl << endl;
    else if (m_backupResult != "")
        cout << "If your system becomes unstable, "
                "a database backup is located in "
             << m_backupResult.toLocal8Bit().constData() << endl << endl;

    if (m_expertMode)
    {
        resp = getResponse("Would you like to use the existing schema?", "yes");
        if (resp.isEmpty() || resp.left(1).toLower() == "y")
            return MYTH_SCHEMA_USE_EXISTING;
    }

    resp = getResponse("\nShall I upgrade this database?", "yes");
    if (!resp.isEmpty() && resp.left(1).toLower() != "y")
        return MYTH_SCHEMA_EXIT;

    if (connections)
        cout << endl << warnOtherCl.toLocal8Bit().constData() << endl;

    if (didBackup)
    {
        resp = getResponse("\nA database backup might be a good idea"
                           "\nAre you sure you want to upgrade?", "no");
        if (resp.isEmpty() || resp.left(1).toLower() == "n")
            return MYTH_SCHEMA_EXIT;
    }

    return MYTH_SCHEMA_UPGRADE;
}
