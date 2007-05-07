#include <iostream>

// qt
#include <qstring.h>
#include <qdir.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// mytharchive
#include "dbcheck.h"


const QString currentDatabaseVersion = "1001";

static bool UpdateDBVersionNumber(const QString &newnumber)
{

    if (!gContext->SaveSettingOnHost("ArchiveDBSchemaVer",newnumber,NULL)) 
    {
        VERBOSE(VB_IMPORTANT, QString("DB Error (Setting new DB version number): %1\n")
                              .arg(newnumber));

        return false;
    }

    return true;
}

static bool performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythArchive schema version ") + 
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        query.prepare(thequery);
        query.exec();

        if (query.lastError().type() != QSqlError::None)
        {
            QString msg =
                QString("DB Error (Performing database upgrade): \n"
                        "Query was: %1 \nError was: %2 \nnew version: %3")
                .arg(thequery)
                .arg(MythContext::DBErrorMessage(query.lastError()))
                .arg(version);
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }

        counter++;
        thequery = updates[counter];
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

bool UpgradeArchiveDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("ArchiveDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythArchive initial database information.");

        const QString updates[] = {
"DROP TABLE IF EXISTS archiveitems;",

"CREATE TABLE IF NOT EXISTS archiveitems ("
"    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    type set ('Recording','Video','File'),"
"    title VARCHAR(128),"
"    subtitle VARCHAR(128),"
"    description TEXT,"
"    startdate VARCHAR(30),"
"    starttime VARCHAR(30),"
"    size INT UNSIGNED NOT NULL,"
"    filename TEXT NOT NULL,"
"    hascutlist BOOL NOT NULL DEFAULT 0,"
"    cutlist TEXT DEFAULT '',"
"    INDEX (title)"
");",
""
};
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        const QString updates[] =
        {
            "ALTER TABLE archiveitems MODIFY size BIGINT UNSIGNED NOT NULL;",
            ""
        };

        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    return true;
}

