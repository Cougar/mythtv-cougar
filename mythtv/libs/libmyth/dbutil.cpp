#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>
#include <qdatetime.h>

#include "dbutil.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "storagegroup.h"
#include "util.h"
#include "mythdirs.h"

#define LOC QString("DBUtil: ")
#define LOC_ERR QString("DBUtil Error: ")

const int DBUtil::kUnknownVersionNumber = INT_MIN;

/** \fn DBUtil::DBUtil(void)
 *  \brief Constructs the DBUtil object.
 */
DBUtil::DBUtil(void)
    : m_versionString(QString::null), m_versionMajor(-1), m_versionMinor(-1),
      m_versionPoint(-1)
{
}

/** \fn DBUtil::GetDBMSVersion(void)
 *  \brief Returns the QString version name of the DBMS or QString::null in
 *         the event of an error.
 */
QString DBUtil::GetDBMSVersion(void)
{
    if (m_versionString.isEmpty())
        QueryDBMSVersion();
    return m_versionString;
}

/** \fn DBUtil::CompareDBMSVersion(int, int, int)
 *  \brief Compares the version of the active DBMS with the provided version.
 *
 *   Returns negative, 0, or positive if the active DBMS version is less than,
 *   equal to, or greater than the provided version or returns
 *   DBUtil::kUnknownVersionNumber if the version cannot be determined.
 *
 *  \param major The major version number (i.e. 5 in "5.0.22")
 *  \param minor The minor version number (i.e. 0 in "5.0.22")
 *  \param point The point version number (i.e. 22 in "5.0.22")
 *  \return negative, 0, or positive or DBUtil::kUnknownVersionNumber for error
 */
int DBUtil::CompareDBMSVersion(int major, int minor, int point)
{
    if (m_versionMajor < 0)
        if (!ParseDBMSVersion())
           return kUnknownVersionNumber;

    int result = 0;
    int version[3] = {m_versionMajor, m_versionMinor, m_versionPoint};
    int compareto[3] = {major, minor, point};
    for (int i = 0; i < 3 && !result; i++)
    {
        if ((version[i] > -1) || (compareto[i] != 0))
            result = version[i] - compareto[i];
    }

    return result;
}

/** \fn DBUtil::IsNewDatabase(void)
 *  \brief Returns true for a new (empty) database.
 */
bool DBUtil::IsNewDatabase(void)
{
    const QStringList tables = GetTables();
    const int size = tables.size();
    // Usually there will be a single table called schemalock, but check for
    // no tables, also, just in case.
    return (((size == 1) && (tables.at(0) == "schemalock")) ||
            (size == 0));
}

/** \fn DBUtil::IsBackupInProgress(void)
 *  \brief Test to see if a DB backup is in progress
 *
 */
bool DBUtil::IsBackupInProgress(void)
{
    QString backupStartTimeStr = gContext->GetSetting("BackupDBLastRunStart");
    QString backupEndTimeStr = gContext->GetSetting("BackupDBLastRunEnd");

    if (backupStartTimeStr.isEmpty())
    {
        VERBOSE(VB_DATABASE, "DBUtil::BackupInProgress(): No start time found, "
                "database backup is not in progress.");
        return false;
    }

    backupStartTimeStr.replace(" ", "T");

    QDateTime backupStartTime =
        QDateTime::fromString(backupStartTimeStr, Qt::ISODate);

    // No end time set
    if (backupEndTimeStr.isEmpty())
    {
        // If DB Backup started less then 10 minutes ago, assume still running
        if (backupStartTime.secsTo(QDateTime::currentDateTime()) < 600)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): Found "
                    "database backup start time of %1 which was %2 seconds "
                    "ago, therefore it appears the backup is still running.")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return true;
        }
        else
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but no end time was found. "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running .")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return false;
        }
    }
    else
    {
        backupEndTimeStr.replace(" ", "T");

        QDateTime backupEndTime =
            QDateTime::fromString(backupEndTimeStr, Qt::ISODate);

        if (backupEndTime >= backupStartTime)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): Found "
                    "database backup end time of %1 later than start time "
                    "of %2, therefore backup is not running.")
                    .arg(backupEndTimeStr).arg(backupStartTimeStr));
            return false;
        }
        else if (backupStartTime.secsTo(QDateTime::currentDateTime()) > 600)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but has not ended yet.  "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return false;
        }
        else
        {
            // start > end and started less than 10 minutes ago
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, and is still running.")
                    .arg(backupStartTimeStr));
            return true;
        }
    }

    // Shouldn't get here
    return false;
}

/** \fn DBUtil::BackupDB(QString&)
 *  \brief Requests a backup of the database.
 *
 *   If the DatabaseBackupScript exists in the ShareDir, it will be executed.
 *   All required database information will be made available as name=value
 *   pairs in a temporary file whose filename will be passed to the backup
 *   script.  The script may parse this file to obtain the required information
 *   to run a backup program, such as mysqldump or mysqlhotcopy.
 *
 *   If the DatabaseBackupScript does not exist, a backup will be performed
 *   using mysqldump directly.  The database password will be passed in a
 *   temporary file so it does not have to be specified on the command line.
 *
 *   Care should be taken in calling this function.  It has the potential to
 *   corrupt in-progress recordings or interfere with playback.
 */
bool DBUtil::BackupDB(QString &filename)
{
    filename = "";

#ifdef USING_MINGW
    VERBOSE(VB_IMPORTANT, "Database backups disabled on Windows.");
    return true;
#endif

    if (gContext->GetNumSetting("DisableAutomaticBackup", 0))
    {
        VERBOSE(VB_IMPORTANT, "Database backups disabled.  Skipping backup.");
        return true;
    }

    if (IsNewDatabase())
    {
        VERBOSE(VB_IMPORTANT, "New database detected.  Skipping backup.");
        return true;
    }

    QString backupScript = GetShareDir() + "mythconverg_backup.pl";
    backupScript = gContext->GetSetting("DatabaseBackupScript", backupScript);

    if (!QFile::exists(backupScript))
    {
        VERBOSE(VB_IMPORTANT, QString("Database backup script does "
                                      "not exist: %1").arg(backupScript));
        backupScript = QString::null;
    }

    bool result = false;
    MSqlQuery query(MSqlQuery::InitCon());

    gContext->SaveSettingOnHost("BackupDBLastRunStart",
                                QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd hh:mm:ss"), NULL);

    if (backupScript.isEmpty())
        result = DoBackup(filename);
    else
        result = DoBackup(backupScript, filename);

    gContext->SaveSettingOnHost("BackupDBLastRunEnd",
                                QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd hh:mm:ss"), NULL);

    if (query.isConnected())
    {
        QString dbTag("BackupDB");
        query.prepare("DELETE FROM housekeeping WHERE tag = :TAG ;");
        query.bindValue(":TAG", dbTag);
        query.exec();

        query.prepare("INSERT INTO housekeeping(tag,lastrun) "
                       "values(:TAG ,now()) ;");
        query.bindValue(":TAG", dbTag);
        query.exec();
    }

    return result;
}

QMap<QString,bool> DBUtil::GetTableMap(void)
{
    QMap<QString,bool> found_table;
    const QStringList list = GetTables();
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        found_table[*it] = true;
    return found_table;
}

/** \fn DBUtil::GetTables(void)
 *  \brief Retrieves a list of tables from the database.
 *
 *   The function tries to ensure that the list contains only tables and no
 *   views.  However, for MySQL 5.0.1, the list will also contain any views
 *   defined in the database.
 *
 *  \return QStringList containing table names
 */
QStringList DBUtil::GetTables(void)
{
    QStringList result;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return result;

    query.prepare("SHOW FULL TABLES");
    if (!query.exec())
    {
        MythDB::DBError("DBUtil Finding Tables", query);
        return result;
    }

    while (query.next())
    {
        if (query.value(1).toString() != "VIEW")
            result.append(query.value(0).toString());
    }

    return result;
}

/** \fn DBUtil::CreateBackupFilename(QString prefix, QString extension)
 *  \brief Creates a filename to use for the filename.
 *
 *   The filename is a concatenation of the given prefix, a hyphen, the current
 *   date/time, and the extension.
 *
 *  \param prefix The prefix (i.e. a database name) which should appear before
 *                the date/time
 *  \param extension The extension to use for the file, including a dot, if
 *                   desired
 *  \return QString name
 */
QString DBUtil::CreateBackupFilename(QString prefix, QString extension)
{
    QDateTime now = QDateTime::currentDateTime();
    QString time = now.toString("yyyyMMddhhmmss");
    return QString("%1-%2%3").arg(prefix).arg(time).arg(extension);
}

/** \fn DBUtil::GetBackupDirectory(void)
 *  \brief Determines the appropriate path for the database backup.
 *
 *   The function requests the special "DB Backups" storage group.  In the
 *   event the group is not defined, the StorageGroup will fall back to using
 *   the "Default" group.  For users upgrading from version 0.20 or before
 *   (which do not support Storage Groups), the StorageGroup will fall back to
 *   using the old RecordFilePrefix.
 */
QString DBUtil::GetBackupDirectory()
{
    QString directory;
    StorageGroup sgroup("DB Backups", gContext->GetHostName());
    QStringList dirList = sgroup.GetDirList();
    if (dirList.size())
    {
        directory = sgroup.FindNextDirMostFree();

        if (!QDir(directory).exists())
        {
            VERBOSE(VB_FILE, "GetBackupDirectory() - ignoring "
                             + directory + ", using /tmp");
            directory = QString::null;
        }
    }

    if (directory.isNull())
        // Rather than use kDefaultStorageDir, the default for
        // FindNextDirMostFree() when no dirs are defined for the StorageGroup,
        // use /tmp as it's possible that kDefaultStorageDir doesn't exist
        // and (at least on *nix) less possible that /tmp doesn't exist
        directory = "/tmp";

    return directory;
}

/** \brief Creates temporary file containing sensitive DB info.
 *
 *   So we don't have to specify the password on the command line, use
 *   --defaults-extra-file to specify a temporary file with a [client] and
 *  [mysqldump] section that provides the password.  This will fail if the
 *   user's ~/.my.cnf (which is read after the --defaults-extra-file)
 *  specifies a different password that's incorrect for dbUserName
 */
bool DBUtil::CreateTemporaryDBConf(
    const QString &privateinfo, QString &filename)
{
    bool ok = true;
    filename = createTempFile("/tmp/mythtv_db_backup_conf_XXXXXX");
    const QByteArray     tmpfile     = filename.toLocal8Bit();
    const DatabaseParams dbParams    = gContext->GetDatabaseParams();
    const QString        dbSchemaVer = gContext->GetSetting("DBSchemaVer");

    FILE *fp = fopen(tmpfile.constData(), "w");
    if (!fp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Unable to create temporary "
                        "configuration file for creating DB backup: %1")
                .arg(tmpfile.constData()));
        filename = "";
        ok = false;
    }
    else
    {
        chmod(tmpfile.constData(), S_IRUSR);

        QByteArray outarr = privateinfo.toLocal8Bit();
        fprintf(fp, outarr.constData());

        if (fclose(fp))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Error closing '%1'")
                    .arg(tmpfile.constData()) + ENO);
        }
    }

    return ok;
}

/** \fn DBUtil::DoBackup(const QString&, QString&)
 *  \brief Creates a backup of the database by executing the backupScript.
 *
 *   This function executes the specified backup script to create a database
 *   backup.  This is the preferred approach for creating the backup.
 */
bool DBUtil::DoBackup(const QString &backupScript, QString &filename)
{
    DatabaseParams dbParams = gContext->GetDatabaseParams();
    QString     dbSchemaVer = gContext->GetSetting("DBSchemaVer");
    QString backupDirectory = GetBackupDirectory();
    QString  backupFilename = CreateBackupFilename(dbParams.dbName + "-" +
                                                   dbSchemaVer, ".sql");
    QString      scriptArgs = gContext->GetSetting("BackupDBScriptArgs");
    if (!scriptArgs.isEmpty())
        scriptArgs.prepend(" ");


    QString privateinfo =
        QString("DBHostName=%1\nDBPort=%2\n"
                "DBUserName=%3\nDBPassword=%4\n"
                "DBName=%5\nDBSchemaVer=%6\n"
                "DBBackupDirectory=%7\nDBBackupFilename=%8\n")
        .arg(dbParams.dbHostName).arg(dbParams.dbPort)
        .arg(dbParams.dbUserName).arg(dbParams.dbPassword)
        .arg(dbParams.dbName).arg(dbSchemaVer)
        .arg(backupDirectory).arg(backupFilename);
    QString tempDatabaseConfFile = QString::null;
    bool hastemp = CreateTemporaryDBConf(privateinfo, tempDatabaseConfFile);
    if (!hastemp)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Attempting backup, anyway.");

    VERBOSE(VB_IMPORTANT, QString("Backing up database with script: '%1'")
            .arg(backupScript));

    QString command = backupScript + scriptArgs + " " + tempDatabaseConfFile;
    QByteArray tmpcmd = command.toLocal8Bit();
    uint status = system(tmpcmd.constData());

    if (hastemp)
    {
        QByteArray tmpfile = tempDatabaseConfFile.toLocal8Bit();
        unlink(tmpfile.constData());
    }

    if (status)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Error backing up database: %1 (%2)")
                .arg(tmpcmd.constData()).arg(status));
        filename = "__FAILED__";
        return false;
    }

    VERBOSE(VB_IMPORTANT, "Database Backup complete.");

    QDir dir(backupDirectory, backupFilename + "*");
    uint numfiles = dir.count();
    if (numfiles < 1)
    {
        // If no file begins with the suggested filename, don't show the backup
        // filename in the GUI message -- the script probably used some other
        // filename
        filename = "";
        VERBOSE(VB_FILE, LOC_ERR + QString(
                    "No files beginning with the suggested database backup "
                    "filename '%1' were found in '%2'.")
                .arg(backupFilename).arg(backupDirectory));
    }
    else
    {
        filename = dir.path() + "/" + dir[0];;
        if (numfiles > 1)
        {
            VERBOSE(VB_FILE, LOC_ERR + QString(
                        "Multiple files beginning with the suggested database "
                        "backup filename '%1' were found in '%2'. "
                        "Assuming the first is the backup.")
                    .arg(backupFilename).arg(backupDirectory));
        }
    }

    if (!filename.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Backed up database to file: '%1'")
                .arg(filename));
    }

    return true;
}

/** \fn DBUtil::DoBackup(QString&)
 *  \brief Creates a backup of the database.
 *
 *   This fallback function is used only if the database backup script cannot
 *   be found.
 */
bool DBUtil::DoBackup(QString &filename)
{
    DatabaseParams dbParams = gContext->GetDatabaseParams();
    QString     dbSchemaVer = gContext->GetSetting("DBSchemaVer");
    QString backupDirectory = GetBackupDirectory();

    QString command;
    QString compressCommand("");
    QString extension = ".sql";
    if (QFile::exists("/bin/gzip"))
        compressCommand = "/bin/gzip";
    else if (QFile::exists("/usr/bin/gzip"))
        compressCommand = "/usr/bin/gzip";
    else
        VERBOSE(VB_IMPORTANT, "Neither /bin/gzip nor /usr/bin/gzip exist. "
                              "The database backup will be uncompressed.");

    QString backupFilename = CreateBackupFilename(
        dbParams.dbName + "-" + dbSchemaVer, extension);
    QString backupPathname = backupDirectory + "/" + backupFilename;

    QString privateinfo = QString(
        "[client]\npassword=%1\n[mysqldump]\npassword=%2\n")
        .arg(dbParams.dbPassword).arg(dbParams.dbPassword);
    QString tempExtraConfFile = QString::null;
    if (!CreateTemporaryDBConf(privateinfo, tempExtraConfFile))
        return false;

    QString portArg = "";
    if (dbParams.dbPort > 0)
        portArg = QString(" --port='%1'").arg(dbParams.dbPort);
    command = QString("mysqldump --defaults-extra-file='%1' --host='%2'%3"
                      " --user='%4' --add-drop-table --add-locks"
                      " --allow-keywords --complete-insert"
                      " --extended-insert --lock-tables --no-create-db --quick"
                      " '%5' > '%6' 2>/dev/null")
                      .arg(tempExtraConfFile).arg(dbParams.dbHostName)
                      .arg(portArg).arg(dbParams.dbUserName)
                      .arg(dbParams.dbName).arg(backupPathname);

    VERBOSE(VB_FILE, QString("Backing up database with command: '%1'")
            .arg(command));
    VERBOSE(VB_IMPORTANT, QString("Backing up database to file: '%1'")
            .arg(backupPathname));

    QByteArray tmpcmd = command.toAscii();
    uint status = system(tmpcmd.constData());

    QByteArray tmpfile = tempExtraConfFile.toLocal8Bit();
    unlink(tmpfile.constData());

    if (status)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Error backing up database: '%1' (%2)")
                .arg(command).arg(status));
        filename = "__FAILED__";
        return false;
    }

    if (compressCommand != "")
    {
        VERBOSE(VB_IMPORTANT, "Compressing database backup file.");
        compressCommand += " " + backupPathname;
        QByteArray tmpcompresscmd = compressCommand.toAscii();
        status = system(tmpcompresscmd.constData());

        if (status)
        {
            VERBOSE(VB_IMPORTANT,
                   "Compression failed, backup file will remain uncompressed.");
        }
        else
        {
            backupPathname += ".gz";

            VERBOSE(VB_IMPORTANT, QString("Database Backup filename: '%1'")
                    .arg(backupPathname));
        }
    }

    VERBOSE(VB_IMPORTANT, "Database Backup complete.");

    filename = backupPathname;
    return true;
}

/** \fn DBUtil::QueryDBMSVersion(void)
 *  \brief Reads and returns the QString version name from the DBMS or
 *         returns QString::null in the event of an error.
 */
bool DBUtil::QueryDBMSVersion(void)
{
    // Allow users to override the string provided by the database server in
    // case the value was changed to an unrecognizable string by whomever
    // compiled the MySQL server
    QString dbmsVersion = gContext->GetSetting("DBMSVersionOverride");

    if (dbmsVersion.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT VERSION();");
        if (!query.exec() || !query.next())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to determine MySQL "
                    "version.");
            MythDB::DBError("DBUtil Querying DBMS version", query);
            dbmsVersion = QString::null;
        }
        else
            dbmsVersion = query.value(0).toString();
    }
    m_versionString = dbmsVersion;

    return !m_versionString.isEmpty();
}

/** \fn DBUtil::ParseDBMSVersion(void)
 *  \brief Parses m_versionString to find the major, minor, and point version.
 */
bool DBUtil::ParseDBMSVersion()
{
    if (m_versionString.isEmpty())
        if (!QueryDBMSVersion())
            return false;

    bool ok;
    QString section;
    int pos = 0, i = 0;
    int version[3] = {-1, -1, -1};
    QRegExp digits("(\\d+)");

    while ((i < 3) && ((pos = digits.search(m_versionString, pos)) > -1))
    {
        section = digits.cap(1);
        pos += digits.matchedLength();
        version[i] = section.toInt(&ok, 10);
        if (!ok)
            version[i] = -1;
        i++;
    }

    m_versionMajor = version[0];
    m_versionMinor = version[1];
    m_versionPoint = version[2];

    return m_versionMajor > -1;
}

/**
 * Estimate the number of MythTV programs using the database
 */
int DBUtil::CountClients(void)
{
    DatabaseParams DB    = gContext->GetDatabaseParams();
    int            count = 0;

    // QSqlQuery doesn't know how to parse the results of "SHOW PROCESSLIST",
    // so instead of a nice query.next loop, we have to do it in a hacky way

    QString     cmd = "mysql";
    QStringList params;
    QProcess    proc;

    params << "-h" << DB.dbHostName;
    params << "-u" << DB.dbUserName;
    params << "-p" + DB.dbPassword;
    params << "-e" << "SHOW PROCESSLIST";

    proc.start(cmd, params);
    if (!proc.waitForStarted(1000) ||
        !proc.waitForFinished(3000))
    {
        proc.kill();
        return 0;
    }

    while (proc.canReadLine())
    {
        QByteArray dbname = DB.dbName.toAscii();
        if (proc.readLine().contains(dbname.constData()))
            ++count;
    }


    // On average, each myth program has 4 database connections,
    // but we round up just in case a new program is loading:
    count = (count + 3)/4; 

    VERBOSE(VB_GENERAL+VB_EXTRA,
            QString("DBUtil::CountClients() found %1").arg(count));

    return count;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
