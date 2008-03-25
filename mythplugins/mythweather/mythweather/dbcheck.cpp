#include <qstring.h>
#include <qdir.h>
#include <qstringlist.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "dbcheck.h"
#include "defs.h"

const QString currentDatabaseVersion = "1003";

static bool UpdateDBVersionNumber(const QString &newnumber)
{
 
    if (!gContext->SaveSettingOnHost("WeatherDBSchemaVer",newnumber,NULL))
    {   
        VERBOSE(VB_IMPORTANT, QString("DB Error (Setting new DB version number): %1\n")
                              .arg(newnumber));

        return false;
    }

    return true;
}

static bool performActualUpdate(const QStringList updates, QString version,
                                QString &dbver)
{
    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythWeather schema version ") +
            version);

    MSqlQuery query(MSqlQuery::InitCon());

    QStringList::const_iterator it = updates.begin();
    
    while (it != updates.end())
    {
        QString thequery = *it;
        query.exec(thequery);

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
        ++it;
    }

    if (!UpdateDBVersionNumber(version))
        return false;

    dbver = version;
    return true;
}

/*
 * TODO Probably the biggest change to simplify things would be to get rid of
 * the surrogate key screen_id in weatherscreens, draworder should be unique,
 * that way, with cascading, updating screens won't need to blow out everything
 * in the db everytime.
 */
bool InitializeDatabase()
{
    QString dbver = gContext->GetSetting("WeatherDBSchemaVer");

    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT,
                "Inserting MythWeather initial database information.");
        QStringList updates;
        updates << "CREATE TABLE IF NOT EXISTS weathersourcesettings ("
                        "sourceid INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                        "source_name VARCHAR(64) NOT NULL,"
                        "update_timeout INT UNSIGNED NOT NULL DEFAULT '600',"
                        "retrieve_timeout INT UNSIGNED NOT NULL DEFAULT '60',"
                        "hostname VARCHAR(255) NULL,"
                        "path VARCHAR(255) NULL,"
                        "author VARCHAR(128) NULL,"
                        "version VARCHAR(32) NULL,"
                        "email VARCHAR(255) NULL,"
                        "types MEDIUMTEXT NULL,"
                        "PRIMARY KEY(sourceid)) TYPE=InnoDB;"
               << "CREATE TABLE IF NOT EXISTS weatherscreens ("
                        "screen_id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                        "draworder INT UNSIGNED NOT NULL,"
                        "container VARCHAR(64) NOT NULL,"
                        "hostname VARCHAR(255) NULL,"
                        "units TINYINT UNSIGNED NOT NULL,"
                        "PRIMARY KEY(screen_id)) TYPE=InnoDB;"
               << "CREATE TABLE IF NOT EXISTS weatherdatalayout ("
                        "location VARCHAR(64) NOT NULL,"
                        "dataitem VARCHAR(64) NOT NULL,"
                        "weatherscreens_screen_id INT UNSIGNED NOT NULL,"
                        "weathersourcesettings_sourceid INT UNSIGNED NOT NULL,"
                        "PRIMARY KEY(location, dataitem, weatherscreens_screen_id,"
                            "weathersourcesettings_sourceid),"
                        "INDEX weatherdatalayout_FKIndex1(weatherscreens_screen_id),"
                        "INDEX weatherdatalayout_FKIndex2(weathersourcesettings_sourceid),"
                        "FOREIGN KEY(weatherscreens_screen_id) "
                        "REFERENCES weatherscreens(screen_id) "
                            "ON DELETE CASCADE "
                            "ON UPDATE CASCADE,"
                        "FOREIGN KEY(weathersourcesettings_sourceid) "
                        "REFERENCES weathersourcesettings(sourceid) "
                        "ON DELETE RESTRICT "
                        "ON UPDATE CASCADE) TYPE=InnoDB;";
        /*
         * TODO Possible want to delete old stuff (i.e. agressiveness, locale..)
         * that we don't use any more
         */

        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        QStringList updates;
        updates << "ALTER TABLE weathersourcesettings ADD COLUMN updated "
                   "TIMESTAMP;";

        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }



    if (dbver == "1001")
    {
        QStringList updates;
        updates << QString("ALTER DATABASE %1 DEFAULT CHARACTER SET latin1;")
            .arg(gContext->GetDatabaseParams().dbName) <<
            "ALTER TABLE weatherdatalayout"
            "  MODIFY location varbinary(64) NOT NULL,"
            "  MODIFY dataitem varbinary(64) NOT NULL;" <<
            "ALTER TABLE weatherscreens"
            "  MODIFY container varbinary(64) NOT NULL,"
            "  MODIFY hostname varbinary(64) default NULL;" <<
            "ALTER TABLE weathersourcesettings"
            "  MODIFY source_name varbinary(64) NOT NULL,"
            "  MODIFY hostname varbinary(64) default NULL,"
            "  MODIFY path varbinary(255) default NULL,"
            "  MODIFY author varbinary(128) default NULL,"
            "  MODIFY version varbinary(32) default NULL,"
            "  MODIFY email varbinary(255) default NULL,"
            "  MODIFY types mediumblob;";

        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }


    if (dbver == "1002")
    {
        QStringList updates;
        updates << QString("ALTER DATABASE %1 DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;")
                .arg(gContext->GetDatabaseParams().dbName) <<
            "ALTER TABLE weatherdatalayout"
            "  DEFAULT CHARACTER SET default,"
            "  MODIFY location varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY dataitem varchar(64) CHARACTER SET utf8 NOT NULL;" <<
            "ALTER TABLE weatherscreens"
            "  DEFAULT CHARACTER SET default,"
            "  MODIFY container varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY hostname varchar(64) CHARACTER SET utf8 default NULL;" <<
            "ALTER TABLE weathersourcesettings"
            "  DEFAULT CHARACTER SET default,"
            "  MODIFY source_name varchar(64) CHARACTER SET utf8 NOT NULL,"
            "  MODIFY hostname varchar(64) CHARACTER SET utf8 default NULL,"
            "  MODIFY path varchar(255) CHARACTER SET utf8 default NULL,"
            "  MODIFY author varchar(128) CHARACTER SET utf8 default NULL,"
            "  MODIFY version varchar(32) CHARACTER SET utf8 default NULL,"
            "  MODIFY email varchar(255) CHARACTER SET utf8 default NULL,"
            "  MODIFY types mediumtext CHARACTER SET utf8;";

        if (!performActualUpdate(updates, "1003", dbver))
            return false;
    }

    return true;
}
