#include <qdir.h>
#include <iostream>
#include <map>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <unistd.h>

#include <cdaudio.h>

#include "decoder.h"
#include "metadata.h"
#include "maddecoder.h"
#include "vorbisdecoder.h"
#include "databasebox.h"
#include "playbackbox.h"
#include "cdrip.h"
#include "playlist.h"
#include "globalsettings.h"
#include "dbcheck.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>

void CheckFreeDBServerFile(void)
{
    char filename[1024];
    if (getenv("HOME") == NULL)
    {
        cerr << "main.o: You don't have a HOME environment variable. CD lookup will almost certainly not work." << endl;
        return;
    }
    sprintf(filename, "%s/.cdserverrc", getenv("HOME"));

    QFile file(filename);

    if (!file.exists())
    {
        struct cddb_conf cddbconf;
        struct cddb_serverlist list;
        struct cddb_host proxy_host;

        cddbconf.conf_access = CDDB_ACCESS_REMOTE;
        list.list_len = 1;
        strncpy(list.list_host[0].host_server.server_name,
                "freedb.freedb.org", 256);
        strncpy(list.list_host[0].host_addressing, "~cddb/cddb.cgi", 256);
        list.list_host[0].host_server.server_port = 80;
        list.list_host[0].host_protocol = CDDB_MODE_HTTP;

        cddb_write_serverlist(cddbconf, list, proxy_host.host_server);
    }
}

Decoder *getDecoder(const QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
    return decoder;
}

void CheckFile(const QString &filename)
{
    Decoder *decoder = getDecoder(filename);

    if (decoder)
    {
        QSqlDatabase *db = QSqlDatabase::database();

        Metadata *data = decoder->getMetadata(db);
        if (data)
            delete data;

        delete decoder;
    }
}

enum MusicFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};

typedef QMap <QString, MusicFileLocation> MusicLoadedMap;

void BuildFileList(QString &directory, MusicLoadedMap &music_files)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
            BuildFileList(filename, music_files);
        else
            music_files[filename] = kFileSystem;
    }
}

void SavePending(QSqlDatabase *db, int pending)
{
    //  Temporary Hack until mythmusic
    //  has a proper settings/setup

    QString some_query_string = QString("SELECT * FROM settings WHERE "
                                       "value=\"LastMusicPlaylistPush\" "
                                       "and hostname = \"%1\" ;")
                                       .arg(gContext->GetHostName());

    QSqlQuery some_query(some_query_string, db);
    
    if(some_query.numRowsAffected() == 0)
    {
        //  first run from this host / recent version
        QString a_query_string = QString("INSERT INTO settings (value,data,"
                                         "hostname) VALUES "
                                         "(\"LastMusicPlaylistPush\", \"%1\", "
                                         " \"%2\");").arg(pending)
                                         .arg(gContext->GetHostName());
        
        QSqlQuery another_query(a_query_string, db);

    }
    else if(some_query.numRowsAffected() == 1)
    {
        //  ah, just right
        
        QString a_query_string = QString("UPDATE settings SET data = \"%1\" "
                                         "WHERE value=\"LastMusicPlaylistPush\""
                                         " AND hostname = \"%2\" ;")
                                         .arg(pending)
                                         .arg(gContext->GetHostName());

        QSqlQuery another_query(a_query_string, db);
    }                       
    else
    {
        //  correct thor's diabolical plot to 
        //  consume all table space
        
        QString a_query_string = QString("DELETE FROM settings WHERE "
                                         "value=\"LastMusicPlaylistPush\" "
                                         "and hostname = \"%1\" ;")
                                         .arg(gContext->GetHostName());
        
        QSqlQuery another_query(a_query_string, db);

        a_query_string = QString("INSERT INTO settings (value, data, hostname) "
                                 " VALUES (\"LastMusicPlaylistPush\", \"%1\",   "
                                 " \"%2\");").arg(pending)
                                 .arg(gContext->GetHostName());

        QSqlQuery one_more_query(a_query_string, db);
        
    }
}

void SearchDir(QSqlDatabase *db, QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    BuildFileList(directory, music_files);

    QSqlQuery query("SELECT filename FROM musicmetadata;", db);

    int counter = 0;

    MythProgressDialog *file_checking;
    file_checking = new MythProgressDialog(QObject::tr("Searching for music files"),
                                           query.numRowsAffected());

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString name = query.value(0).toString();
            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                    music_files.remove(iter);
                else
                    music_files[name] = kDatabase;
            }
            file_checking->setProgress(++counter);
        }
    }

    file_checking->Close();
    delete file_checking;

    file_checking = new MythProgressDialog(QObject::tr("Updating music database"), 
                                           music_files.size());

    QRegExp quote_regex("\"");
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            CheckFile(iter.key());
        }
        else if (*iter == kDatabase)
        {
            QString name(iter.key());
            name.replace(quote_regex, "\"\"");

            QString querystr = QString("DELETE FROM musicmetadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
        }

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

void startPlayback(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    PlaybackBox *pbb = new PlaybackBox(gContext->GetMainWindow(),
                                       "music_play", "music-", 
                                       all_playlists, all_music,
                                       "music_playback");
    qApp->unlock();
    pbb->exec();
    qApp->lock();

    pbb->stop();

    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      QObject::tr("music database"));
    qApp->unlock();
    dbbox.exec();
    qApp->lock();
}

void startRipper(QSqlDatabase *db)
{
    Ripper rip(db, gContext->GetMainWindow(), QObject::tr("cd ripper"));

    qApp->unlock();
    rip.exec();
    qApp->lock();
}

struct MusicData
{
    QString paths;
    QSqlDatabase *db;
    QString startdir;
    PlaylistsContainer *all_playlists;
    AllMusic *all_music;
};

void MusicCallback(void *data, QString &selection)
{
    MusicData *mdata = (MusicData *)data;

    QString sel = selection.lower();

    if (sel == "music_create_playlist")
        startDatabaseTree(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_play")
        startPlayback(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_rip")
    {
        startRipper(mdata->db);
        //  Reconcile with the database
        SearchDir(mdata->db, mdata->startdir);
        //  Tell the metadata to reset itself
        mdata->all_music->resync();
        mdata->all_playlists->postLoad();
    }
    else if (sel == "settings_scan")
    {
        if ("" != mdata->startdir)
        {
            SearchDir(mdata->db, mdata->startdir);
            mdata->all_music->resync();
            mdata->all_playlists->postLoad();
        }
    }
    else if (sel == "music_set_general")
    {
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "music_set_player")
    {
        PlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "music_set_ripper")
    {
        RipperSettings settings;
        settings.exec(QSqlDatabase::database());
    }
}

void runMenu(QString paths, QString startdir,
             PlaylistsContainer *all_playlists, AllMusic *all_music,
             QString which_menu)
{
    QString themedir = gContext->GetThemeDir();
    QSqlDatabase *db = QSqlDatabase::database();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "music menu");

    MusicData data;

    data.paths = paths;
    data.db = db;
    data.startdir = startdir;
    data.all_playlists = all_playlists;
    data.all_music = all_music;

    diag->setCallback(MusicCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        gContext->GetLCDDevice()->switchToTime();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeMusicDatabaseSchema();

    GeneralSettings general;
    general.load(QSqlDatabase::database());
    general.save(QSqlDatabase::database());
    PlayerSettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythmusic_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    srand(time(NULL));

    CheckFreeDBServerFile();

    QSqlDatabase *db = QSqlDatabase::database();

    QSqlQuery count_query("SELECT COUNT(*) FROM musicmetadata;", db);

    bool musicdata_exists = false;
    if (count_query.isActive())
    {
        if(count_query.next() && 
           0 != count_query.value(0).toInt())
        {
            musicdata_exists = true;
        }
    }
    else
    {
        DialogBox *no_db_dialog = new DialogBox(gContext->GetMainWindow(),
             QObject::tr("\n\nYou have no MythMusic tables in your database."));
        no_db_dialog->AddButton(QObject::tr("OK, I'll read the documentation"));
        no_db_dialog->exec();

        delete no_db_dialog;
        return -1;
    }

    //  Load all available info about songs (once!)
    QString startdir = gContext->GetSetting("MusicLocation");

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (startdir != "" && !musicdata_exists)
        SearchDir(db, startdir);

    QString paths = gContext->GetSetting("TreeLevels");
    AllMusic *all_music = new AllMusic(db, paths, startdir);

    //  Load all playlists into RAM (once!)
    PlaylistsContainer *all_playlists = new PlaylistsContainer(db, all_music);
    all_playlists->setHost(gContext->GetHostName());
    //  And away we go ...
    runMenu(paths, startdir, all_playlists, all_music, "musicmenu.xml");

    //  Automagically save all playlists and metadata (ratings) that have changed


    if (all_music->cleanOutThreads())
    {
        all_music->save();
    }
    if(all_playlists->cleanOutThreads())
    {
        all_playlists->save();
        int x = all_playlists->getPending();
        SavePending(db, x);
    }

    delete all_music;
    delete all_playlists;

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythmusic_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QString paths = gContext->GetSetting("TreeLevels");
    QString startdir = gContext->GetSetting("MusicLocation");

    runMenu(paths, startdir, NULL, NULL, "music_settings.xml");

    qApp->removeTranslator(&translator);

    return 0;
}

