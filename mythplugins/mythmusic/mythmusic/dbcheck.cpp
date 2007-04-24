#include <qstring.h>
#include <qdir.h>

#include <iostream>
using namespace std;

#include "dbcheck.h"
#include "metadata.h"
#include "mythtv/mythcontext.h"
#include "mythtv/mythdbcon.h"

const QString currentDatabaseVersion = "1013";

static bool UpdateDBVersionNumber(const QString &newnumber)
{   

    if (!gContext->SaveSettingOnHost("MusicDBSchemaVer",newnumber,NULL))
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

    VERBOSE(VB_IMPORTANT, QString("Upgrading to MythMusic schema version ") + 
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

bool UpgradeMusicDatabaseSchema(void)
{
    QString dbver = gContext->GetSetting("MusicDBSchemaVer");
    
    if (dbver == currentDatabaseVersion)
        return true;

    if (dbver == "")
    {
        VERBOSE(VB_IMPORTANT, "Inserting MythMusic initial database information.");

        const QString updates[] = {
"CREATE TABLE IF NOT EXISTS musicmetadata ("
"    intid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    artist VARCHAR(128) NOT NULL,"
"    album VARCHAR(128) NOT NULL,"
"    title VARCHAR(128) NOT NULL,"
"    genre VARCHAR(128) NOT NULL,"
"    year INT UNSIGNED NOT NULL,"
"    tracknum INT UNSIGNED NOT NULL,"
"    length INT UNSIGNED NOT NULL,"
"    filename TEXT NOT NULL,"
"    rating INT UNSIGNED NOT NULL DEFAULT 5,"
"    lastplay TIMESTAMP NOT NULL,"
"    playcount INT UNSIGNED NOT NULL DEFAULT 0,"
"    INDEX (artist),"
"    INDEX (album),"
"    INDEX (title),"
"    INDEX (genre)"
");",
"CREATE TABLE IF NOT EXISTS musicplaylist ("
"    playlistid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128) NOT NULL,"
"    hostname VARCHAR(255),"
"    songlist TEXT NOT NULL"
");",
""
};
        if (!performActualUpdate(updates, "1000", dbver))
            return false;
    }

    if (dbver == "1000")
    {
        QString startdir = gContext->GetSetting("MusicLocation");
        startdir = QDir::cleanDirPath(startdir);
        if (!startdir.endsWith("/"))
            startdir += "/";

        MSqlQuery query(MSqlQuery::InitCon());
        // urls as filenames are NOT officially supported yet
        query.exec("SELECT filename, intid FROM musicmetadata WHERE "
                        "filename NOT LIKE ('%://%');");

        if (query.isActive() && query.size() > 0)
        {
            int i = 0;
            QString intid, name, newname;

            MSqlQuery modify(MSqlQuery::InitCon());
            while (query.next())
            {
                name = query.value(0).toString();
                newname = name;
                intid = query.value(1).toString();

                if (newname.startsWith(startdir))
                { 
                    newname.remove(0, startdir.length());
                    modify.exec(QString("UPDATE musicmetadata SET "
                                "filename = \"%1\" "
                                "WHERE filename = \"%2\" AND intid = %3;")
                                .arg(newname).arg(name).arg(intid));
                    if (modify.isActive())
                        i += modify.numRowsAffected();
                }
            }
            VERBOSE(VB_IMPORTANT, QString("Modified %1 entries for db schema 1001").arg(i));
        }

        const QString updates[] = {
""
};
        if (!performActualUpdate(updates, "1001", dbver))
            return false;
    }

    if (dbver == "1001")
    {
        const QString updates[] = {
"ALTER TABLE musicmetadata ADD mythdigest      VARCHAR(255);",
"ALTER TABLE musicmetadata ADD size            BIGINT UNSIGNED;",
"ALTER TABLE musicmetadata ADD date_added      DATETIME;",
"ALTER TABLE musicmetadata ADD date_modified   DATETIME;",
"ALTER TABLE musicmetadata ADD format          VARCHAR(4);",
"ALTER TABLE musicmetadata ADD description     VARCHAR(255);",
"ALTER TABLE musicmetadata ADD comment         VARCHAR(255);",
"ALTER TABLE musicmetadata ADD compilation     TINYINT DEFAULT 0;",
"ALTER TABLE musicmetadata ADD composer        VARCHAR(255);",
"ALTER TABLE musicmetadata ADD disc_count      SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD disc_number     SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD track_count     SMALLINT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD start_time      INT UNSIGNED DEFAULT 0;",
"ALTER TABLE musicmetadata ADD stop_time       INT UNSIGNED;",
"ALTER TABLE musicmetadata ADD eq_preset       VARCHAR(255);",
"ALTER TABLE musicmetadata ADD relative_volume TINYINT DEFAULT 0;",
"ALTER TABLE musicmetadata ADD sample_rate     INT UNSIGNED;",
"ALTER TABLE musicmetadata ADD bpm             SMALLINT UNSIGNED;",
"ALTER TABLE musicmetadata ADD INDEX (mythdigest);",
""
};
        if (!performActualUpdate(updates, "1002", dbver))
            return false;
    }
    
    if (dbver == "1002")
    {
        VERBOSE(VB_IMPORTANT, "Updating music metadata to be UTF-8 in the database");

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT intid, artist, album, title, genre, "
                      "filename FROM musicmetadata ORDER BY intid;");

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                int id = query.value(0).toInt();
                QString artist = query.value(1).toString();
                QString album = query.value(2).toString();
                QString title = query.value(3).toString();
                QString genre = query.value(4).toString();
                QString filename = query.value(5).toString();

                MSqlQuery subquery(MSqlQuery::InitCon());
                subquery.prepare("UPDATE musicmetadata SET "
                                 "artist = :ARTIST, album = :ALBUM, "
                                 "title = :TITLE, genre = :GENRE, "
                                 "filename = :FILENAME "
                                 "WHERE intid = :ID;");
                subquery.bindValue(":ARTIST", artist.utf8());
                subquery.bindValue(":ALBUM", album.utf8());
                subquery.bindValue(":TITLE", title.utf8());
                subquery.bindValue(":GENRE", genre.utf8());
                subquery.bindValue(":FILENAME", filename.utf8());
                subquery.bindValue(":ID", id);

                if (!subquery.exec() || !subquery.isActive())
                    MythContext::DBError("music utf8 update", subquery);
            }
        }

        query.prepare("SELECT playlistid, name FROM musicplaylist "
                      "ORDER BY playlistid;");

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            while (query.next())
            {
                int id = query.value(0).toInt();
                QString name = query.value(1).toString();

                MSqlQuery subquery(MSqlQuery::InitCon());
                subquery.prepare("UPDATE musicplaylist SET "
                                 "name = :NAME WHERE playlistid = :ID ;");
                subquery.bindValue(":NAME", name.utf8());
                subquery.bindValue(":ID", id);

                if (!subquery.exec() || !subquery.isActive())
                    MythContext::DBError("music playlist utf8 update", subquery);
            }
        }

        VERBOSE(VB_IMPORTANT, "Done updating music metadata to UTF-8");

        const QString updates[] = {
""
};
        if (!performActualUpdate(updates, "1003", dbver))
            return false;
    }
    
    if (dbver == "1003")
    {
        const QString updates[] = {
"DROP TABLE IF EXISTS smartplaylistcategory;",
"CREATE TABLE smartplaylistcategory ("
"    categoryid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128) NOT NULL,"
"    INDEX (name)"
");",

"INSERT INTO smartplaylistcategory SET categoryid = 1, "
"    name = \"Decades\";",
"INSERT INTO smartplaylistcategory SET categoryid = 2, "
"    name = \"Favourite Tracks\";",
"INSERT INTO smartplaylistcategory SET categoryid = 3, "
"    name = \"New Tracks\";",

"DROP TABLE IF EXISTS smartplaylist;",
"CREATE TABLE smartplaylist ("
"    smartplaylistid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    name VARCHAR(128) NOT NULL,"
"    categoryid INT UNSIGNED NOT NULL,"
"    matchtype SET('All', 'Any') NOT NULL DEFAULT 'All',"
"    orderby VARCHAR(128) NOT NULL DEFAULT '',"
"    limitto INT UNSIGNED NOT NULL DEFAULT 0,"
"    INDEX (name),"
"    INDEX (categoryid)"
");",
"DROP TABLE IF EXISTS smartplaylistitem;",
"CREATE TABLE smartplaylistitem ("
"    smartplaylistitemid INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,"
"    smartplaylistid INT UNSIGNED NOT NULL,"
"    field VARCHAR(50) NOT NULL,"
"    operator VARCHAR(20) NOT NULL,"
"    value1 VARCHAR(255) NOT NULL,"
"    value2 VARCHAR(255) NOT NULL,"
"    INDEX (smartplaylistid)"
");",
"INSERT INTO smartplaylist SET smartplaylistid = 1, name = \"1960's\", "
"    categoryid = 1, matchtype = \"All\", orderby = \"Artist (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 1, field = \"Year\","
"    operator = \"is between\", value1 = \"1960\", value2 = \"1969\";",

"INSERT INTO smartplaylist SET smartplaylistid = 2, name = \"1970's\", "
"    categoryid = 1, matchtype = \"All\", orderby = \"Artist (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 2, field = \"Year\","
"    operator = \"is between\", value1 = \"1970\", value2 = \"1979\";",

"INSERT INTO smartplaylist SET smartplaylistid = 3, name = \"1980's\", "
"    categoryid = 1, matchtype = \"All\", orderby = \"Artist (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 3, field = \"Year\","
"    operator = \"is between\", value1 = \"1980\", value2 = \"1989\";",

"INSERT INTO smartplaylist SET smartplaylistid = 4, name = \"1990's\", "
"    categoryid = 1, matchtype = \"All\", orderby = \"Artist (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 4, field = \"Year\","
"    operator = \"is between\", value1 = \"1990\", value2 = \"1999\";",

"INSERT INTO smartplaylist SET smartplaylistid = 5, name = \"2000's\", "
"    categoryid = 1, matchtype = \"All\", orderby = \"Artist (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 5, field = \"Year\","
"    operator = \"is between\", value1 = \"2000\", value2 = \"2009\";",

"INSERT INTO smartplaylist SET smartplaylistid = 6, name = \"Favorite Tracks\", "
"    categoryid = 2, matchtype = \"All\"," 
"    orderby = \"Artist (A), Album (A)\", limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 6, field = \"Rating\","
"    operator = \"is greater than\", value1 = \"7\", value2 = \"0\";",

"INSERT INTO smartplaylist SET smartplaylistid = 7, name = \"100 Most Played Tracks\", "
"    categoryid = 2, matchtype = \"All\", orderby = \"Play Count (D)\","
"    limitto = 100;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 7, field = \"Play Count\","
"    operator = \"is greater than\", value1 = \"0\", value2 = \"0\";",

"INSERT INTO smartplaylist SET smartplaylistid = 8, name = \"Never Played Tracks\", "
"    categoryid = 3, matchtype = \"All\", orderby = \"Artist (A), Album (A)\","
"    limitto = 0;",
"INSERT INTO smartplaylistitem SET smartplaylistid = 8, field = \"Play Count\","
"    operator = \"is equal to\", value1 = \"0\", value2 = \"0\";",

""
};

        if (!performActualUpdate(updates, "1004", dbver))
            return false;
    }

    if (dbver == "1004")
    {
        const QString updates[] = {
"ALTER TABLE musicmetadata ADD compilation_artist VARCHAR(128) NOT NULL AFTER artist;",
"ALTER TABLE musicmetadata ADD INDEX (compilation_artist);",
""
};

        if (!performActualUpdate(updates, "1005", dbver))
            return false;
    }


    if (dbver == "1005")
    {
        const QString updates[] = {
"CREATE TABLE music_albums ("
"    album_id int(11) unsigned NOT NULL auto_increment PRIMARY KEY,"
"    artist_id int(11) unsigned NOT NULL default '0',"
"    album_name varchar(255) NOT NULL default '',"
"    year smallint(6) NOT NULL default '0',"
"    compilation tinyint(1) unsigned NOT NULL default '0',"
"    INDEX idx_album_name(album_name)"
");",
"CREATE TABLE music_artists ("
"    artist_id int(11) unsigned NOT NULL auto_increment PRIMARY KEY,"
"    artist_name varchar(255) NOT NULL default '',"
"    INDEX idx_artist_name(artist_name)"
");",
"CREATE TABLE music_genres ("
"    genre_id int(11) unsigned NOT NULL auto_increment PRIMARY KEY,"
"    genre varchar(25) NOT NULL default '',"
"    INDEX idx_genre(genre)"
");",
"CREATE TABLE music_playlists ("
"    playlist_id int(11) unsigned NOT NULL auto_increment PRIMARY KEY,"
"    playlist_name varchar(255) NOT NULL default '',"
"    playlist_songs text NOT NULL default '',"
"    last_accessed timestamp NOT NULL,"
"    length int(11) unsigned NOT NULL default '0',"
"    songcount smallint(8) unsigned NOT NULL default '0',"
"    hostname VARCHAR(255) NOT NULL default ''"
");",
"CREATE TABLE music_songs ("
"    song_id int(11) unsigned NOT NULL auto_increment PRIMARY KEY,"
"    filename text NOT NULL default '',"
"    name varchar(255) NOT NULL default '',"
"    track smallint(6) unsigned NOT NULL default '0',"
"    artist_id int(11) unsigned NOT NULL default '0',"
"    album_id int(11) unsigned NOT NULL default '0',"
"    genre_id int(11) unsigned NOT NULL default '0',"
"    year smallint(6) NOT NULL default '0',"
"    length int(11) unsigned NOT NULL default '0',"
"    numplays int(11) unsigned NOT NULL default '0',"
"    rating tinyint(4) unsigned NOT NULL default '0',"
"    lastplay timestamp NOT NULL,"
"    date_entered datetime default NULL,"
"    date_modified datetime default NULL,"
"    format varchar(4) NOT NULL default '0',"
"    mythdigest VARCHAR(255),"
"    size BIGINT(20) unsigned,"
"    description VARCHAR(255),"
"    comment VARCHAR(255),"
"    disc_count SMALLINT(5) UNSIGNED DEFAULT '0',"
"    disc_number SMALLINT(5) UNSIGNED DEFAULT '0',"
"    track_count SMALLINT(5) UNSIGNED DEFAULT '0',"
"    start_time INT(10) UNSIGNED DEFAULT '0',"
"    stop_time INT(10) UNSIGNED,"
"    eq_preset VARCHAR(255),"
"    relative_volume TINYINT DEFAULT '0',"
"    sample_rate INT(10) UNSIGNED DEFAULT '0',"
"    bitrate INT(10) UNSIGNED DEFAULT '0',"
"    bpm SMALLINT(5) UNSIGNED,"
"    INDEX idx_name(name),"
"    INDEX idx_mythdigest(mythdigest)"
");",
"CREATE TABLE music_stats ("
"    num_artists smallint(5) unsigned NOT NULL default '0',"
"    num_albums smallint(5) unsigned NOT NULL default '0',"
"    num_songs mediumint(8) unsigned NOT NULL default '0',"
"    num_genres tinyint(3) unsigned NOT NULL default '0',"
"    total_time varchar(12) NOT NULL default '0',"
"    total_size varchar(10) NOT NULL default '0'"
");",
"RENAME TABLE smartplaylist TO music_smartplaylists;",
"RENAME TABLE smartplaylistitem TO music_smartplaylist_items;",
"RENAME TABLE smartplaylistcategory TO music_smartplaylist_categories;",
// Run necessary SQL to migrate the table structure
"CREATE TEMPORARY TABLE tmp_artists"
"  SELECT DISTINCT artist FROM musicmetadata;",
"INSERT INTO tmp_artists"
"  SELECT DISTINCT compilation_artist"
"  FROM musicmetadata"
"  WHERE compilation_artist<>artist;",
"INSERT INTO music_artists (artist_name) SELECT DISTINCT artist FROM tmp_artists;",
"INSERT INTO music_albums (artist_id, album_name, year, compilation) "
"  SELECT artist_id, album, ROUND(AVG(year)) AS year, IF(SUM(compilation),1,0) AS compilation"
"  FROM musicmetadata"
"  LEFT JOIN music_artists ON compilation_artist=artist_name"
"  GROUP BY artist_id, album;",
"INSERT INTO music_genres (genre) SELECT DISTINCT genre FROM musicmetadata;",
"INSERT INTO music_songs "
"   (song_id, artist_id, album_id, genre_id, year, lastplay,"
"    date_entered, date_modified, name, track, length, size, numplays,"
"    rating, filename)"
"  SELECT intid, ma.artist_id, mb.album_id, mg.genre_id, mmd.year, lastplay,"
"         date_added, date_modified, title, tracknum, length, IFNULL(size,0), playcount,"
"         rating, filename"
"  FROM musicmetadata AS mmd"
"  LEFT JOIN music_artists AS ma ON mmd.artist=ma.artist_name"
"  LEFT JOIN music_artists AS mc ON mmd.compilation_artist=mc.artist_name"
"  LEFT JOIN music_albums AS mb ON mmd.album=mb.album_name AND mc.artist_id=mb.artist_id"
"  LEFT JOIN music_genres AS mg ON mmd.genre=mg.genre;",
"INSERT INTO music_playlists"
"  (playlist_id,playlist_name,playlist_songs,hostname)"
"  SELECT playlistid, name, songlist, hostname"
"  FROM musicplaylist;",
// Set all real playlists to be global by killing the hostname
"UPDATE music_playlists"
"  SET hostname=''"
"  WHERE playlist_name<>'default_playlist_storage'"
"    AND playlist_name<>'backup_playlist_storage';",
""
};
        if (!performActualUpdate(updates, "1006", dbver))
            return false;
    }

    if (dbver == "1006")
    {
        const QString updates[] = {
"ALTER TABLE music_genres MODIFY genre VARCHAR(255) NOT NULL default '';",
""
};
        if (!performActualUpdate(updates, "1007", dbver))
            return false;
    }

    if (dbver == "1007")
    {
        const QString updates[] = {
"ALTER TABLE music_songs MODIFY lastplay DATETIME DEFAULT NULL;",
"CREATE TABLE music_directories (directory_id int(20) NOT NULL AUTO_INCREMENT "
"PRIMARY KEY, path TEXT NOT NULL DEFAULT '', "
"parent_id INT(20) NOT NULL DEFAULT '0') ;",
"INSERT IGNORE INTO music_directories (path) SELECT DISTINCT"
" SUBSTRING(filename FROM 1 FOR INSTR(filename, "
"SUBSTRING_INDEX(filename, '/', -1))-2) FROM music_songs;",
"CREATE TEMPORARY TABLE tmp_songs SELECT music_songs.*, directory_id "
"FROM music_songs, music_directories WHERE "
"music_directories.path=SUBSTRING(filename FROM 1 FOR "
"INSTR(filename, SUBSTRING_INDEX(filename, '/', -1))-2);",
"UPDATE tmp_songs SET filename=SUBSTRING_INDEX(filename, '/', -1);",
"DELETE FROM music_songs;",
"ALTER TABLE music_songs ADD COLUMN directory_id int(20) NOT NULL DEFAULT '0';",
"INSERT INTO music_songs SELECT * FROM tmp_songs;",
"ALTER TABLE music_songs ADD INDEX (directory_id);",
""
};

        if (!performActualUpdate(updates, "1008", dbver))
            return false;
    }

    if (dbver == "1008")
    {
        const QString updates[] = {
"CREATE TABLE music_albumart (albumart_id int(20) NOT NULL AUTO_INCREMENT "
"PRIMARY KEY, filename VARCHAR(255) NOT NULL DEFAULT '', directory_id INT(20) "
"NOT NULL DEFAULT '0');",
""
};

        if (!performActualUpdate(updates, "1009", dbver))
            return false;
    }

    if (dbver == "1009")
    {
        const QString updates[] = {
"ALTER TABLE music_albumart ADD COLUMN imagetype tinyint(3) NOT NULL DEFAULT '0';",
        ""
};

    if (!performActualUpdate(updates, "1010", dbver))
        return false;

    // scan though the music_albumart table and make a guess at what 
    // each image represents from the filename

    VERBOSE(VB_IMPORTANT, "Updating music_albumart image types");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT albumart_id, filename, directory_id, imagetype FROM music_albumart;");

    if (query.exec())
    {
        while (query.next())
        {
            int id = query.value(0).toInt();
            QString filename = query.value(1).toString();
            int directoryID = query.value(2).toInt();
            int type = IT_UNKNOWN;
            MSqlQuery subquery(MSqlQuery::InitCon());

            // guess the type from the filename
            type = AlbumArtImages::guessImageType(filename);

            // if type is still unknown check to see how many images are available in the dir
            // and assume that if this is the only image it must be the front cover
            if (type == IT_UNKNOWN)
            {
                subquery.prepare("SELECT count(directory_id) FROM music_albumart "
                                 "WHERE directory_id = :DIR;");
                subquery.bindValue(":DIR", directoryID);
                if (!subquery.exec() || !subquery.isActive())
                    MythContext::DBError("album art image count", subquery);
                subquery.first();
                if (query.value(0).toInt() == 1)
                    type = IT_FRONTCOVER;
            }

            // finally set the type in the music_albumart table
            subquery.prepare("UPDATE music_albumart "
                    "SET imagetype = :TYPE "
                    "WHERE albumart_id = :ID;");
            subquery.bindValue(":TYPE", type);
            subquery.bindValue(":ID", id);
            if (!subquery.exec() || !subquery.isActive())
                MythContext::DBError("album art image type update", subquery);
        }
    }
 }

    if (dbver == "1010")
    {
        const QString updates[] = {"", ""};

        // update the VisualMode setting to the new format
        QString setting = gContext->GetSetting("VisualMode");
        setting = setting.simplifyWhiteSpace();
        setting = setting.replace(' ', ";");
        gContext->SaveSetting("VisualMode", setting);

        if (!performActualUpdate(updates, "1011", dbver))
            return false;

    }

    if (dbver == "1011")
    {
        const QString updates[] = {
"ALTER TABLE music_albumart ADD COLUMN song_id int(11) NOT NULL DEFAULT '0', ADD COLUMN embedded TINYINT(1) NOT NULL DEFAULT '0';",
        ""
};

        if (!performActualUpdate(updates, "1012", dbver))
            return false;

    }

    if (dbver == "1012")
    {
        const QString updates[] = {
"ALTER TABLE music_songs ADD INDEX album_id (album_id);",
"ALTER TABLE music_songs ADD INDEX genre_id (genre_id);",
"ALTER TABLE music_songs ADD INDEX artist_id (artist_id);",
        ""
};

        if (!performActualUpdate(updates, "1013", dbver))
            return false;

    }
/* in 0.21 */
//"DROP TABLE musicmetadata;",
//"DROP TABLE musicplaylist;",


    return true;
}
