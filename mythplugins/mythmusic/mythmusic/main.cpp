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
#include <mythtv/mythmedia.h>

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
    
    if (some_query.numRowsAffected() == 0)
    {
        //  first run from this host / recent version
        QString a_query_string = QString("INSERT INTO settings (value,data,"
                                         "hostname) VALUES "
                                         "(\"LastMusicPlaylistPush\", \"%1\", "
                                         " \"%2\");").arg(pending)
                                         .arg(gContext->GetHostName());
        
        QSqlQuery another_query(a_query_string, db);

    }
    else if (some_query.numRowsAffected() == 1)
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

void SearchDir(QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    BuildFileList(directory, music_files);

    QSqlQuery query("SELECT filename FROM musicmetadata "
                    "WHERE filename NOT LIKE ('%://%');",
                     QSqlDatabase::database());

    int counter = 0;

    MythProgressDialog *file_checking;
    file_checking = new MythProgressDialog(QObject::tr("Searching for music files"),
                                           query.numRowsAffected());

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString name = directory + query.value(0).toString();
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
            name.remove(0, directory.length());
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
                      "music_select", "music-", "music database");
    qApp->unlock();
    dbbox.exec();
    qApp->lock();
}

void startRipper(void)
{
    Ripper rip(QSqlDatabase::database(), gContext->GetMainWindow(), 
               "cd ripper");

    qApp->unlock();
    rip.exec();
    qApp->lock();
}

struct MusicData
{
    QString paths;
    QString startdir;
    PlaylistsContainer *all_playlists;
    AllMusic *all_music;
    QTranslator *trans;
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
        startRipper();
        //  Reconcile with the database
        SearchDir(mdata->startdir);
        //  Tell the metadata to reset itself
        mdata->all_music->resync();
        mdata->all_playlists->postLoad();
    }
    else if (sel == "settings_scan")
    {
        if ("" != mdata->startdir)
        {
            SearchDir(mdata->startdir);
            mdata->all_music->resync();
            mdata->all_playlists->postLoad();
        }
    }
    else if (sel == "music_set_general")
    {
        MusicGeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "music_set_player")
    {
        MusicPlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "music_set_ripper")
    {
        MusicRipperSettings settings;
        settings.exec(QSqlDatabase::database());
    }
}

void runMenu(MusicData *mdata, QString which_menu)
{
    QString themedir = gContext->GetThemeDir();
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "music menu");

    diag->setCallback(MusicCallback, mdata);
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

void runMusicPlayback(void);
void runMusicSelection(void);
void runRipCD(void);

void handleMedia(void) 
{
    mythplugin_run();
}

void setupKeys(void)
{
    REG_JUMP("Play music", "", "", runMusicPlayback);
    REG_JUMP("Select music playlists", "", "", runMusicSelection);
    REG_JUMP("Rip CD", "", "", runRipCD);

    REG_KEY("Music", "DELETE", "Delete track from playlist", "D");
    REG_KEY("Music", "NEXTTRACK", "Move to the next track", ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK", "Move to the previous track", ",,<,Q,Home");
    REG_KEY("Music", "FFWD", "Fast forward", "PgDown");
    REG_KEY("Music", "RWND", "Rewind", "PgUp");
    REG_KEY("Music", "PAUSE", "Pause/Start playback", "P");
    REG_KEY("Music", "STOP", "Stop playback", "O");
    REG_KEY("Music", "VOLUMEDOWN", "Volume down", "[,{,F10");
    REG_KEY("Music", "VOLUMEUP", "Volume up", "],},F11");
    REG_KEY("Music", "MUTE", "Mute", "|,\\,F9");
    REG_KEY("Music", "CYCLEVIS", "Cycle visualizer mode", "6");
    REG_KEY("Music", "BLANKSCR", "Blank screen", "5");
    REG_KEY("Music", "THMBUP", "Increase rating", "9");
    REG_KEY("Music", "THMBDOWN", "Decrease rating", "7");
    REG_KEY("Music", "REFRESH", "Refresh music tree", "8");

    REG_MEDIA_HANDLER("MythMusic Media Handler", "", "", handleMedia, MEDIATYPE_AUDIO | MEDIATYPE_MIXED);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeMusicDatabaseSchema();

    MusicGeneralSettings general;
    general.load(QSqlDatabase::database());
    general.save(QSqlDatabase::database());
    MusicPlayerSettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());
    MusicRipperSettings ripper;
    ripper.load(QSqlDatabase::database());
    ripper.save(QSqlDatabase::database());

    setupKeys();

    Decoder::SetLocationFormatUseTags();

    return 0;
}

static void preMusic(MusicData *mdata)
{
    mdata->trans = new QTranslator(0);
    mdata->trans->load(PREFIX + QString("/share/mythtv/i18n/mythmusic_") +
                       QString(gContext->GetSetting("Language").lower()) +
                       QString(".qm"), ".");
    qApp->installTranslator(mdata->trans);

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

    //  Load all available info about songs (once!)
    QString startdir = gContext->GetSetting("MusicLocation");
    startdir = QDir::cleanDirPath(startdir);
    if (!startdir.endsWith("/"));
        startdir += "/";

    Decoder::SetLocationFormatUseTags();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (startdir != "" && !musicdata_exists)
        SearchDir(startdir);

    QString paths = gContext->GetSetting("TreeLevels");
    AllMusic *all_music = new AllMusic(db, paths, startdir);

    //  Load all playlists into RAM (once!)
    PlaylistsContainer *all_playlists = new PlaylistsContainer(db, all_music, gContext->GetHostName());

    mdata->paths = paths;
    mdata->startdir = startdir;
    mdata->all_playlists = all_playlists;
    mdata->all_music = all_music;
}

static void postMusic(MusicData *mdata)
{
    // Automagically save all playlists and metadata (ratings) that have changed

    if (mdata->all_music->cleanOutThreads())
    {
        mdata->all_music->save();
    }

    if (mdata->all_playlists->cleanOutThreads())
    {
        mdata->all_playlists->save();
        int x = mdata->all_playlists->getPending();
        SavePending(QSqlDatabase::database(), x);
    }

    delete mdata->all_music;
    delete mdata->all_playlists;

    qApp->removeTranslator(mdata->trans);
    delete mdata->trans;
}

int mythplugin_run(void)
{
    MusicData mdata;

    preMusic(&mdata);
    runMenu(&mdata, "musicmenu.xml");
    postMusic(&mdata);

    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythmusic_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    MusicData mdata;
    mdata.paths = gContext->GetSetting("TreeLevels");
    mdata.startdir = gContext->GetSetting("MusicLocation");
    mdata.startdir = QDir::cleanDirPath(mdata.startdir);
    if (!mdata.startdir.endsWith("/"));
        mdata.startdir += "/";

    Decoder::SetLocationFormatUseTags();

    runMenu(&mdata, "music_settings.xml");

    qApp->removeTranslator(&translator);

    return 0;
}

void runMusicPlayback(void)
{
    MusicData mdata;

    preMusic(&mdata);
    startPlayback(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
}

void runMusicSelection(void)
{
    MusicData mdata;

    preMusic(&mdata);
    startDatabaseTree(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
}

void runRipCD(void)
{
    MusicData mdata;

    preMusic(&mdata);
    startRipper();
    SearchDir(mdata.startdir);
    mdata.all_music->resync();
    mdata.all_playlists->postLoad();
    postMusic(&mdata);
}
