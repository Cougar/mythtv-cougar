#include <qsqldatabase.h>
#include <qstring.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"

#include "mythtv/mythcontext.h"

const QString currentDatabaseVersion = "1000";

static void UpdateDBVersionNumber(const QString &newnumber)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    db_conn->exec("DELETE FROM settings WHERE value='GalleryDBSchemaVer';");
    db_conn->exec(QString("INSERT INTO settings (value, data, hostname) "
                          "VALUES ('GalleryDBSchemaVer', %1, NULL);")
                         .arg(newnumber));
}

static void performActualUpdate(const QString updates[], QString version,
                                QString &dbver)
{
    QSqlDatabase *db_conn = QSqlDatabase::database();

    VERBOSE(VB_ALL, QString("Upgrading to MythGallery schema version ") + 
            version);

    int counter = 0;
    QString thequery = updates[counter];

    while (thequery != "")
    {
        db_conn->exec(thequery);
        counter++;
        thequery = updates[counter];
    }

    UpdateDBVersionNumber(version);
    dbver = version;
}

void UpgradeGalleryDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("GalleryDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return;

    if (dbver == "")
    {
        VERBOSE(VB_ALL, "Inserting MythGallery initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS gallerymetadata ("
"  image VARCHAR(255) NOT NULL PRIMARY KEY,"
"  angle INTEGER NOT NULL"
");",
"INSERT INTO settings VALUES ('GalleryDBSchemaVer', 1000, NULL);",
""
};
        performActualUpdate(updates, "1000", dbver);
    }
}
