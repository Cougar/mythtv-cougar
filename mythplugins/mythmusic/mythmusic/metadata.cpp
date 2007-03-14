#include <iostream> 
#include <qapplication.h>
#include <qregexp.h> 
#include <qdatetime.h>
#include <qdir.h>

using namespace std;

#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdbcon.h>

#include "metadata.h"
#include "treebuilders.h"

static QString thePrefix = "the ";

bool operator==(const Metadata& a, const Metadata& b)
{
    if (a.Filename() == b.Filename())
        return true;
    return false;
}

bool operator!=(const Metadata& a, const Metadata& b)
{
    if (a.Filename() != b.Filename())
        return true;
    return false;
}

Metadata& Metadata::operator=(Metadata *rhs)
{
    m_artist = rhs->m_artist;
    m_compilation_artist = rhs->m_compilation_artist;
    m_album = rhs->m_album;
    m_title = rhs->m_title;
    m_formattedartist = rhs->m_formattedartist;
    m_formattedtitle = rhs->m_formattedtitle;
    m_genre = rhs->m_genre;
    m_year = rhs->m_year;
    m_tracknum = rhs->m_tracknum;
    m_length = rhs->m_length;
    m_rating = rhs->m_rating;
    m_lastplay = rhs->m_lastplay;
    m_playcount = rhs->m_playcount;
    m_compilation = rhs->m_compilation;
    m_id = rhs->m_id;
    m_filename = rhs->m_filename;
    m_changed = rhs->m_changed;

    return *this;
}

QString Metadata::m_startdir = "";

void Metadata::SetStartdir(const QString &dir)
{
    Metadata::m_startdir = dir;
}

void Metadata::persist()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE music_songs set rating = :RATING , "
                  "numplays = :PLAYCOUNT , lastplay = :LASTPLAY "
                  "where song_id = :ID ;");
    query.bindValue(":RATING", m_rating);
    query.bindValue(":PLAYCOUNT", m_playcount);
    query.bindValue(":LASTPLAY", m_lastplay);
    query.bindValue(":ID", m_id);

    if (!query.exec() || query.numRowsAffected() < 1)
        MythContext::DBError("music persist", query);
}

int Metadata::compare(Metadata *other) 
{
    if (m_format == "cast") 
    {
        int artist_cmp = Artist().lower().localeAwareCompare(other->Artist().lower());
        
        if (artist_cmp == 0) 
            return Title().lower().localeAwareCompare(other->Title().lower());
        
        return artist_cmp;
    } 
    else 
    {
        return (Track() - other->Track());
    }
}

bool Metadata::isInDatabase()
{
    bool retval = false;

    QString sqlfilepath(m_filename);
    if (!sqlfilepath.contains("://"))
    {
        sqlfilepath.remove(0, m_startdir.length());
    }
    QString sqldir = sqlfilepath.section( '/', 0, -2);
    QString sqlfilename =sqlfilepath.section( '/', -1 ) ;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT music_artists.artist_name, "
    "music_comp_artists.artist_name AS compilation_artist, "
    "music_albums.album_name, music_songs.name, music_genres.genre, "
    "music_songs.year, music_songs.track, music_songs.length, "
    "music_songs.song_id, music_songs.rating, music_songs.numplays, "
    "music_songs.lastplay, music_albums.compilation, music_songs.format "
    "FROM music_songs "
    "LEFT JOIN music_directories "
    "ON music_songs.directory_id=music_directories.directory_id "
    "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
    "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
    "LEFT JOIN music_artists AS music_comp_artists "
    "ON music_albums.artist_id=music_comp_artists.artist_id "
    "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
    "WHERE music_songs.filename = :FILENAME "
    "AND music_directories.path = :DIRECTORY ;");
    query.bindValue(":FILENAME", sqlfilename.utf8());
    query.bindValue(":DIRECTORY", sqldir.utf8());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();

        m_artist = QString::fromUtf8(query.value(0).toString());
        m_compilation_artist = QString::fromUtf8(query.value(1).toString());
        m_album = QString::fromUtf8(query.value(2).toString());
        m_title = QString::fromUtf8(query.value(3).toString());
        m_genre = QString::fromUtf8(query.value(4).toString());
        m_year = query.value(5).toInt();
        m_tracknum = query.value(6).toInt();
        m_length = query.value(7).toInt();
        m_id = query.value(8).toUInt();
        m_rating = query.value(9).toInt();
        m_playcount = query.value(10).toInt();
        m_lastplay = query.value(11).toString();
        m_compilation = (query.value(12).toInt() > 0);
        m_format = query.value(13).toString();

        retval = true;
    }

    return retval;
}

void Metadata::dumpToDatabase()
{
    QString sqlfilepath(m_filename);
    if (!sqlfilepath.contains("://"))
    {
        sqlfilepath.remove(0, m_startdir.length());
    }
    QString sqldir = sqlfilepath.section( '/', 0, -2);
    QString sqlfilename = sqlfilepath.section( '/', -1 ) ;

    if (m_artist == "")
        m_artist = QObject::tr("Unknown Artist");
    if (m_compilation_artist == "")
        m_compilation_artist = m_artist; // This should be the same as Artist if blank.
    if (m_album == "")
        m_album = QObject::tr("Unknown Album");
    if (m_title == "")
        m_title = m_filename;
    if (m_genre == "")
        m_genre = QObject::tr("Unknown Genre");

    MSqlQuery query(MSqlQuery::InitCon());

    if (sqldir.isEmpty())
    {
        m_directoryid = 0;
    }
    else if (m_directoryid < 0)
    {
        // Load the directory id
        query.prepare("SELECT directory_id FROM music_directories "
                    "WHERE path = :DIRECTORY ;");
        query.bindValue(":DIRECTORY", sqldir.utf8());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("music select directory id", query);
            return;
        }
        if (query.next())
        {
            m_directoryid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_directories (path) VALUES (:DIRECTORY);");
            query.bindValue(":DIRECTORY", sqldir.utf8());

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythContext::DBError("music insert directory", query);
                return;
            }
            m_directoryid = query.lastInsertId().toInt();
        }
    }

    if (m_artistid < 0)
    {
        // Load the artist id
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValue(":ARTIST", m_artist.utf8());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("music select artist id", query);
            return;
        }
        if (query.next())
        {
            m_artistid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
            query.bindValue(":ARTIST", m_artist.utf8());

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythContext::DBError("music insert artist", query);
                return;
            }
            m_artistid = query.lastInsertId().toInt();
        }
    }

    // Compilation Artist
    if (m_artist == m_compilation_artist)
    {
        m_compartistid = m_artistid;
    }
    else
    {
        query.prepare("SELECT artist_id FROM music_artists "
                    "WHERE artist_name = :ARTIST ;");
        query.bindValue(":ARTIST", m_compilation_artist.utf8());
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("music select compilation artist id", query);
            return;
        }
        if (query.next())
        {
            m_compartistid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_artists (artist_name) VALUES (:ARTIST);");
            query.bindValue(":ARTIST", m_compilation_artist.utf8());

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythContext::DBError("music insert compilation artist", query);
                return;
            }
            m_compartistid = query.lastInsertId().toInt();
        }
    }

    // Album
    if (m_albumid < 0)
    {
        query.prepare("SELECT album_id FROM music_albums "
                    "WHERE artist_id = :COMP_ARTIST_ID "
                    " AND album_name = :ALBUM ;");
        query.bindValue(":COMP_ARTIST_ID", m_compartistid);
        query.bindValue(":ALBUM", m_album.utf8());
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("music select album id", query);
            return;
        }
        if (query.next())
        {
            m_albumid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_albums (artist_id, album_name, compilation, year) VALUES (:COMP_ARTIST_ID, :ALBUM, :COMPILATION, :YEAR);");
            query.bindValue(":COMP_ARTIST_ID", m_compartistid);
            query.bindValue(":ALBUM", m_album.utf8());
            query.bindValue(":COMPILATION", m_compilation);
            query.bindValue(":YEAR", m_year);

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythContext::DBError("music insert album", query);
                return;
            }
            m_albumid = query.lastInsertId().toInt();
        }
    }

    if (m_genreid < 0)
    {
        // Genres
        query.prepare("SELECT genre_id FROM music_genres "
                    "WHERE genre = :GENRE ;");
        query.bindValue(":GENRE", m_genre.utf8());
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("music select genre id", query);
            return;
        }
        if (query.next())
        {
            m_genreid = query.value(0).toInt();
        }
        else
        {
            query.prepare("INSERT INTO music_genres (genre) VALUES (:GENRE);");
            query.bindValue(":GENRE", m_genre.utf8());

            if (!query.exec() || !query.isActive() || query.numRowsAffected() <= 0)
            {
                MythContext::DBError("music insert genre", query);
                return;
            }
            m_genreid = query.lastInsertId().toInt();
        }
    }

    // We have all the id's now. We can insert it.
    QString strQuery;
    if (m_id < 1)
    {
        strQuery = "INSERT INTO music_songs ( directory_id, lastplay,"
                   " artist_id, album_id,  name,         genre_id,"
                   " year,      track,     length,       filename,"
                   " rating,    format,    date_entered, date_modified ) "
                   "VALUES ( "
                   " :DIRECTORY, :LASTPLAY,"
                   " :ARTIST,   :ALBUM,    :TITLE,       :GENRE,"
                   " :YEAR,     :TRACKNUM, :LENGTH,      :FILENAME,"
                   " :RATING,   :FORMAT,   :DATE_ADD,    :DATE_MOD );";
    }
    else
    {
        strQuery = "UPDATE music_songs SET"
                   " directory_id = :DIRECTORY"
                   ", artist_id = :ARTIST"
                   ", album_id = :ALBUM"
                   ", name = :TITLE"
                   ", genre_id = :GENRE"
                   ", year = :YEAR"
                   ", track = :TRACKNUM"
                   ", length = :LENGTH"
                   ", filename = :FILENAME"
                   ", rating = :RATING"
                   ", format = :FORMAT"
                   ", date_modified = :DATE_MOD "
                   "WHERE song_id= :ID ;";
    }

    query.prepare(strQuery);

    query.bindValue(":DIRECTORY", m_directoryid);
    query.bindValue(":ARTIST", m_artistid);
    query.bindValue(":ALBUM", m_albumid);
    query.bindValue(":TITLE", m_title.utf8());
    query.bindValue(":GENRE", m_genreid);
    query.bindValue(":YEAR", m_year);
    query.bindValue(":TRACKNUM", m_tracknum);
    query.bindValue(":LENGTH", m_length);
    query.bindValue(":FILENAME", sqlfilename.utf8());
    query.bindValue(":RATING", m_rating);
    query.bindValue(":FORMAT", m_format);
    query.bindValue(":DATE_MOD", QDateTime::currentDateTime());

    if (m_id < 1)
    {
        query.bindValue(":DATE_ADD",  QDateTime::currentDateTime());
        query.bindValue(":LASTPLAY",  QDateTime::currentDateTime());
    }
    else
        query.bindValue(":ID", m_id);

    query.exec();

    if (m_id < 1 && query.isActive() && 1 == query.numRowsAffected())
        m_id = query.lastInsertId().toInt();
}

// Default values for formats
// NB These will eventually be customizable....
QString Metadata::m_formatnormalfileartist      = "ARTIST";
QString Metadata::m_formatnormalfiletrack       = "TITLE";
QString Metadata::m_formatnormalcdartist        = "ARTIST";
QString Metadata::m_formatnormalcdtrack         = "TITLE";
QString Metadata::m_formatcompilationfileartist = "COMPARTIST";
QString Metadata::m_formatcompilationfiletrack  = "TITLE (ARTIST)";
QString Metadata::m_formatcompilationcdartist   = "COMPARTIST";
QString Metadata::m_formatcompilationcdtrack    = "TITLE (ARTIST)";

void Metadata::setArtistAndTrackFormats()
{
    QString tmp;
    
    tmp = gContext->GetSetting("MusicFormatNormalFileArtist");
    if (!tmp.isEmpty())
        m_formatnormalfileartist = tmp;
    
    tmp = gContext->GetSetting("MusicFormatNormalFileTrack");
    if (!tmp.isEmpty())
        m_formatnormalfiletrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatNormalCDArtist");
    if (!tmp.isEmpty())
        m_formatnormalcdartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatNormalCDTrack");
    if (!tmp.isEmpty())
        m_formatnormalcdtrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationFileArtist");
    if (!tmp.isEmpty())
        m_formatcompilationfileartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationFileTrack");
    if (!tmp.isEmpty())
        m_formatcompilationfiletrack = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationCDArtist");
    if (!tmp.isEmpty())
        m_formatcompilationcdartist = tmp;
        
    tmp = gContext->GetSetting("MusicFormatCompilationCDTrack");
    if (!tmp.isEmpty())
        m_formatcompilationcdtrack = tmp;
}


bool Metadata::determineIfCompilation(bool cd)
{ 
    m_compilation = (!m_compilation_artist.isEmpty() 
                   && m_artist != m_compilation_artist);
    setCompilationFormatting(cd);
    return m_compilation;
}


inline QString Metadata::formatReplaceSymbols(const QString &format)
{
  QString rv = format;
  rv.replace("COMPARTIST", m_compilation_artist);
  rv.replace("ARTIST", m_artist);
  rv.replace("TITLE", m_title);
  rv.replace("TRACK", QString("%1").arg(m_tracknum, 2));
  return rv;
}


inline void Metadata::setCompilationFormatting(bool cd)
{
    QString format_artist, format_title;
    
    if (!m_compilation
        || "" == m_compilation_artist
        || m_artist == m_compilation_artist)
    {
        if (!cd)
        {
          format_artist = m_formatnormalfileartist;
          format_title  = m_formatnormalfiletrack;
        }
        else
        {
          format_artist = m_formatnormalcdartist;
          format_title  = m_formatnormalcdtrack;
        }
    }
    else
    {
        if (!cd)
        {
          format_artist = m_formatcompilationfileartist;
          format_title  = m_formatcompilationfiletrack;
        }
        else
        {
          format_artist = m_formatcompilationcdartist;
          format_title  = m_formatcompilationcdtrack;
        }
    }

    // NB Could do some comparisons here to save memory with shallow copies...
    m_formattedartist = formatReplaceSymbols(format_artist);
    m_formattedtitle = formatReplaceSymbols(format_title);
}


QString Metadata::FormatArtist()
{
    if (m_formattedartist.isEmpty())
        setCompilationFormatting();

    return m_formattedartist;
}


QString Metadata::FormatTitle()
{
    if (m_formattedtitle.isEmpty())
        setCompilationFormatting();

    return m_formattedtitle;
}


void Metadata::setField(const QString &field, const QString &data)
{
    if (field == "artist")
        m_artist = data;
    // myth@colin.guthr.ie: Not sure what calls this method as I can't seem
    //                      to find anything that does!
    //                      I've added the compilation_artist stuff here for 
    //                      completeness.
    else if (field == "compilation_artist")
      m_compilation_artist = data;
    else if (field == "album")
        m_album = data;
    else if (field == "title")
        m_title = data;
    else if (field == "genre")
        m_genre = data;
    else if (field == "filename")
        m_filename = data;
    else if (field == "year")
        m_year = data.toInt();
    else if (field == "tracknum")
        m_tracknum = data.toInt();
    else if (field == "length")
        m_length = data.toInt();
    else if (field == "compilation")
        m_compilation = (data.toInt() > 0);

    else
    {
        VERBOSE(VB_IMPORTANT, QString("Something asked me to return data "
                              "about a field called %1").arg(field));
    }
}

void Metadata::getField(const QString &field, QString *data)
{
    if (field == "artist")
        *data = FormatArtist();
    else if (field == "album")
        *data = m_album;
    else if (field == "title")
        *data = FormatTitle();
    else if (field == "genre")
        *data = m_genre;
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Something asked me to return data "
                              "about a field called %1").arg(field));
        *data = "I Dunno";
    }
}

void Metadata::decRating()
{
    if (m_rating > 0)
    {
        m_rating--;
    }
    m_changed = true;
}

void Metadata::incRating()
{
    if (m_rating < 10)
    {
        m_rating++;
    }
    m_changed = true;
}

double Metadata::LastPlay()
{
    QString timestamp = m_lastplay;
    timestamp = timestamp.replace(':', "");
    timestamp = timestamp.replace('T', "");
    timestamp = timestamp.replace('-', "");

    return timestamp.toDouble();
}

void Metadata::setLastPlay()
{
    QDateTime cTime = QDateTime::currentDateTime();
    m_lastplay = cTime.toString("yyyyMMddhhmmss");
    m_changed = true;
}

void Metadata::incPlayCount()
{
    m_playcount++;
    m_changed = true;
}

QStringList Metadata::fillFieldList(QString field)
{
    QStringList searchList;
    searchList.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    if ("artist" == field)
    {
        query.prepare("SELECT artist_name FROM music_artists ORDER BY artist_name;");
    }
    else if ("compilation_artist" == field)
    {
        query.prepare("SELECT DISTINCT artist_name FROM music_artists, music_albums where "  
                "music_albums.artist_id=music_artists.artist_id ORDER BY artist_name");
    }
    else if ("album" == field)
    {
        query.prepare("SELECT album_name FROM music_albums ORDER BY album_name;");
    }
    else if ("title" == field)
    {
        query.prepare("SELECT name FROM music_songs ORDER BY name;");
    }
    else if ("genre" == field)
    {
        query.prepare("SELECT genre FROM music_genres ORDER BY genre;");
    }
    else
    {
        return searchList;
    }

    if (query.exec() && query.isActive())
    {
        while (query.next())
        {
            searchList << QString::fromUtf8(query.value(0).toString());
        }
    }
    return searchList;
}

QStringList Metadata::AlbumArtInDir(QString directory)
{
    QStringList paths;

    directory.remove(0, m_startdir.length());

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONCAT_WS('/', music_directories.path, "
                  "music_albumart.filename) FROM music_albumart "
                  "LEFT JOIN music_directories ON "
                  "music_directories.directory_id=music_albumart.directory_id "
                  "WHERE music_directories.path = :DIR;");
    query.bindValue(":DIR", directory.utf8());
    if (query.exec())
    {
        while (query.next())
        {
            paths += m_startdir + "/" +
                QString::fromUtf8(query.value(0).toString());
        }
    }
    return paths;
}

MetadataLoadingThread::MetadataLoadingThread(AllMusic *parent_ptr)
{
    parent = parent_ptr;
}

void MetadataLoadingThread::run()
{
    //if you want to simulate a big music collection load
    //sleep(3); 
    parent->resync();
}

AllMusic::AllMusic(QString path_assignment, QString a_startdir)
{
    m_startdir = a_startdir;
    m_done_loading = false;
    m_numPcs = m_numLoaded = 0;

    m_cd_title = QObject::tr("CD -- none");

    //  How should we sort?
    setSorting(path_assignment);

    m_root_node = new MusicNode(QObject::tr("All My Music"), m_paths);

    //
    //  Start a thread to do data
    //  loading and sorting
    //

    m_metadata_loader = NULL;
    startLoading();

    m_all_music.setAutoDelete(true);

    m_last_listed = -1;
}

AllMusic::~AllMusic()
{
    m_all_music.clear();

    delete m_root_node;

    m_metadata_loader->wait();
    delete m_metadata_loader;
}

bool AllMusic::cleanOutThreads()
{
    //  If this is still running, the user
    //  probably selected mythmusic and then
    //  escaped out right away
    
    if(m_metadata_loader->finished())
    {
        return true;
    }

    m_metadata_loader->wait();
    return false;
}

/** \fn AllMusic::startLoading(void)
 *  \brief Start loading metadata.
 *
 *  Makes the AllMusic object run it's resync in a thread.
 *  Once done, the doneLoading() method will return true.
 *
 *  \note Alternatively, this could be made to emit a signal
 *        so the caller won't have to poll for completion.
 *
 *  \returns true if the loader thread was started
 */
bool AllMusic::startLoading(void)
{
    // Set this to false early rather than letting it be
    // delayed till the thread calls resync.
    m_done_loading = false;

    if (m_metadata_loader)
    {
        cleanOutThreads();
        delete m_metadata_loader;
    }

    m_metadata_loader = new MetadataLoadingThread(this);
    m_metadata_loader->start();

    return true;
}

void AllMusic::resync()
{
    m_done_loading = false;

    QString aquery = "SELECT music_songs.song_id, music_artists.artist_name, music_comp_artists.artist_name AS compilation_artist, "
                     "music_albums.album_name, music_songs.name, music_genres.genre, music_songs.year, "
                     "music_songs.track, music_songs.length, CONCAT_WS('/', "
                     "music_directories.path, music_songs.filename) AS filename, "
                     "music_songs.rating, music_songs.numplays, music_songs.lastplay, music_albums.compilation, "
                     "music_songs.format "
                     "FROM music_songs "
                     "LEFT JOIN music_directories ON music_songs.directory_id=music_directories.directory_id "
                     "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
                     "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
                     "LEFT JOIN music_artists AS music_comp_artists ON music_albums.artist_id=music_comp_artists.artist_id "
                     "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id "
                     "ORDER BY music_songs.song_id;";

    QString filename, artist, album, title;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(aquery);

    m_root_node->clear();
    m_all_music.clear();

    m_numPcs = query.size() * 2;
    m_numLoaded = 0;

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            filename = QString::fromUtf8(query.value(9).toString());
            if (!filename.contains("://"))
                filename = m_startdir + filename;

            artist = QString::fromUtf8(query.value(1).toString());
            if (artist.isEmpty())
                artist = QObject::tr("Unknown Artist");

            album = QString::fromUtf8(query.value(3).toString());
            if (album.isEmpty())
                album = QObject::tr("Unknown Album");

            title = QString::fromUtf8(query.value(4).toString());
            if (title.isEmpty())
                title = QObject::tr("Unknown Title");

            Metadata *temp = new Metadata(
                filename,
                artist,
                QString::fromUtf8(query.value(2).toString()),
                album,
                title,
                QString::fromUtf8(query.value(5).toString()),
                query.value(6).toInt(),
                query.value(7).toInt(),
                query.value(8).toInt(),
                query.value(0).toInt(),
                query.value(10).toInt(), //rating
                query.value(11).toInt(), //playcount
                query.value(12).toString(), //lastplay
                (query.value(13).toInt() > 0), //compilation
                query.value(14).toString()); //format

            //  Don't delete temp, as PtrList now owns it
            m_all_music.append(temp);

            // compute max/min playcount,lastplay for all music
            if (query.at() == 0)
            { // first song
                m_playcountMin = m_playcountMax = temp->PlayCount();
                m_lastplayMin  = m_lastplayMax  = temp->LastPlay();
            }
            else
            {
                int playCount = temp->PlayCount();
                double lastPlay = temp->LastPlay();

                m_playcountMin = min(playCount, m_playcountMin);
                m_playcountMax = max(playCount, m_playcountMax);
                m_lastplayMin  = min(lastPlay,  m_lastplayMin);
                m_lastplayMax  = max(lastPlay,  m_lastplayMax);
            }
            m_numLoaded++;
        }
    }
    else
    {
         VERBOSE(VB_IMPORTANT, "MythMusic hasn't found any tracks! "
                               "That's ok with me if it's ok with you.");
    }

    //  To find this data quickly, build a map
    //  (a map to pointers!)

    QPtrListIterator<Metadata> an_iterator( m_all_music );
    Metadata *map_add;

    music_map.clear();
    while ( (map_add = an_iterator.current()) != 0 )
    {
        music_map[map_add->ID()] = map_add; 
        ++an_iterator;
    }

    //  Build a tree to reflect current state of 
    //  the metadata. Once built, sort it.

    buildTree(); 
    //printTree();
    sortTree();
    //printTree();
    m_done_loading = true;
}

void AllMusic::sortTree()
{
    m_root_node->sort();
}

void AllMusic::printTree()
{
    //  debugging

    cout << "Whole Music Tree" << endl;
    m_root_node->printYourself(0);
}

void AllMusic::buildTree()
{
    //
    //  Given "paths" and loaded metadata,
    //  build a tree (nodes, leafs, and all)
    //  that reflects the desired structure
    //  of the metadata. This is a structure
    //  that makes it easy (and QUICK) to 
    //  display metadata on (for example) a
    //  Select Music screen
    //

    QPtrListIterator<Metadata> an_iterator( m_all_music );
    Metadata *inserter;
    MetadataPtrList list;

    while ( (inserter = an_iterator.current()) != 0 )
    {
        if (inserter->isVisible())
            list.append(inserter);
        ++an_iterator;

        m_numLoaded++;
    }

    MusicTreeBuilder *builder = MusicTreeBuilder::createBuilder (m_paths);
    builder->makeTree (m_root_node, list);
    delete builder;
}

void AllMusic::writeTree(GenericTree *tree_to_write_to)
{
    m_root_node->writeTree(tree_to_write_to, 0);
}

bool AllMusic::putYourselfOnTheListView(TreeCheckItem *where)
{
    m_root_node->putYourselfOnTheListView(where, false);
    return true;
}

void AllMusic::putCDOnTheListView(CDCheckItem *where)
{
    ValueMetadata::iterator anit;
    for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
    {
        QString title_string = "";
        if((*anit).Title().length() > 0)
        {
            title_string = (*anit).FormatTitle();
        }
        else
        {
            title_string = QObject::tr("Unknown");
        }
        QString title_temp = QString("%1 - %2").arg((*anit).Track()).arg(title_string);
        QString level_temp = QObject::tr("title");
        CDCheckItem *new_item = new CDCheckItem(where, title_temp, level_temp, 
                                                -(*anit).Track());
        new_item->setCheck(false); //  Avoiding -Wall
    }
}

QString AllMusic::getLabel(int an_id, bool *error_flag)
{
    QString a_label = "";
    if(an_id > 0)
    {
   
        if (!music_map.contains(an_id))
        {
            a_label = QString(QObject::tr("Missing database entry: %1")).arg(an_id);
            *error_flag = true;
            return a_label;
        }
      
        a_label += music_map[an_id]->FormatArtist();
        a_label += " ~ ";
        a_label += music_map[an_id]->FormatTitle();
    

        if(a_label.length() < 1)
        {
            a_label = QObject::tr("Ooops");
            *error_flag = true;
        }
        else
        {
            *error_flag = false;
        }
        return a_label;
    }
    else
    {
        ValueMetadata::iterator anit;
        for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                a_label = QString("CD: %1 ~ %2 - %3").arg((*anit).FormatArtist()).arg((*anit).Track()).arg((*anit).FormatTitle());
                *error_flag = false;
                return a_label;
            }
        }
    }

    a_label = "";
    *error_flag = true;
    return a_label;
}

Metadata* AllMusic::getMetadata(int an_id)
{
    if(an_id > 0)
    {
        if (music_map.contains(an_id))
        {
            return music_map[an_id];    
        }
    }
    else if(an_id < 0)
    {
        ValueMetadata::iterator anit;
        for(anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
        {
            if( (*anit).Track() == an_id * -1)
            {
                return &(*anit);
            }
        }
    }
    return NULL;
}

bool AllMusic::updateMetadata(int an_id, Metadata *the_track)
{
    if(an_id > 0)
    {
        Metadata *mdata = getMetadata(an_id);
        if (mdata)
        {
            *mdata = the_track;
            return true;    
        }
    }
    return false;
}

void AllMusic::save()
{
    //  Check each Metadata entry and save those that 
    //  have changed (ratings, etc.)
    
    
    QPtrListIterator<Metadata> an_iterator( m_all_music );
    Metadata *searcher;
    while ( (searcher = an_iterator.current()) != 0 )
    {
        if(searcher->hasChanged())
        {
            searcher->persist();
        }
        ++an_iterator;
    }
}

void AllMusic::clearCDData()
{
    m_cd_data.clear();
    m_cd_title = QObject::tr("CD -- none");
}

void AllMusic::addCDTrack(Metadata *the_track)
{
    m_cd_data.append(*the_track);
}

bool AllMusic::checkCDTrack(Metadata *the_track)
{
    if (m_cd_data.count() < 1)
    {
        return false;
    }
    if (m_cd_data.last().FormatTitle() == the_track->FormatTitle())
    {
        return true;
    }
    return false;
}

bool AllMusic::getCDMetadata(int the_track, Metadata *some_metadata)
{
    ValueMetadata::iterator anit;
    for (anit = m_cd_data.begin(); anit != m_cd_data.end(); ++anit)
    {
        if ((*anit).Track() == the_track)
        {
            *some_metadata = (*anit);
            return true;
        }

    }  
    return false;
}

void AllMusic::setSorting(QString a_paths)
{
    m_paths = a_paths;
    MusicNode::SetStaticData(m_startdir, m_paths);

    if (m_paths == "directory")
        return;

    //  Error checking
    QStringList tree_levels = QStringList::split(" ", m_paths);
    QStringList::const_iterator it = tree_levels.begin();
    for (; it != tree_levels.end(); ++it)
    {
        if (*it != "genre"        &&
            *it != "artist"       &&
            *it != "splitartist"  && 
            *it != "splitartist1" && 
            *it != "album"        &&
            *it != "title")
        {
            VERBOSE(VB_IMPORTANT, QString("AllMusic::setSorting() "
                    "Unknown tree level '%1'").arg(*it));
        }
    }
}

void AllMusic::setAllVisible(bool visible)
{
    QPtrListIterator<Metadata> an_iterator( m_all_music );
    Metadata *md;
    while ( (md = an_iterator.current()) != 0 )
    {
        md->setVisible(visible);
        ++an_iterator;
    }
}

MusicNode::MusicNode(const QString &a_title, const QString &tree_level)
{
    my_title = a_title;
    my_level = tree_level;
    my_subnodes.setAutoDelete(true);
    setPlayCountMin(0);
    setPlayCountMax(0);
    setLastPlayMin(0);
    setLastPlayMax(0);
}

MusicNode::~MusicNode()
{
    my_subnodes.clear();
}

// static member vars

QString MusicNode::m_startdir = "";
QString MusicNode::m_paths = "";
int MusicNode::m_RatingWeight = 2;
int MusicNode::m_PlayCountWeight = 2;
int MusicNode::m_LastPlayWeight = 2;
int MusicNode::m_RandomWeight = 2;

void MusicNode::SetStaticData(const QString &startdir, const QString &paths)
{
    m_startdir = startdir;
    m_paths = paths;
    m_RatingWeight = gContext->GetNumSetting("IntelliRatingWeight", 2);
    m_PlayCountWeight = gContext->GetNumSetting("IntelliPlayCountWeight", 2);
    m_LastPlayWeight = gContext->GetNumSetting("IntelliLastPlayWeight", 2);
    m_RandomWeight = gContext->GetNumSetting("IntelliRandomWeight", 2);
}
    
void MusicNode::putYourselfOnTheListView(TreeCheckItem *parent, bool show_node)
{
    TreeCheckItem *current_parent;

    if (show_node)
    {
        QString title_temp = my_title;
        QString level_temp = my_level;
        current_parent = new TreeCheckItem(parent, title_temp, level_temp, 0);
    }
    else
    {
        current_parent = parent;
    }


    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    while ((a_track = anit.current() ) != 0)
    {
        QString title_temp = QString(QObject::tr("%1 - %2"))
                                  .arg(a_track->Track()).arg(a_track->Title());
        QString level_temp = QObject::tr("title");
        TreeCheckItem *new_item = new TreeCheckItem(current_parent, title_temp,
                                                    level_temp, a_track->ID());
        ++anit;
        new_item->setCheck(false); //  Avoiding -Wall     
    }  

    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *sub_traverse;
    while ((sub_traverse = iter.current() ) != 0)
    {
        sub_traverse->putYourselfOnTheListView(current_parent, true);
        ++iter;
    }
    
}

void MusicNode::writeTree(GenericTree *tree_to_write_to, int a_counter)
{
    
    GenericTree *sub_node = tree_to_write_to->addNode(my_title);
    sub_node->setAttribute(0, 0);
    sub_node->setAttribute(1, a_counter);
    sub_node->setAttribute(2, rand());
    sub_node->setAttribute(3, rand());
    
    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    int track_counter = 0;
    anit.toFirst();
    while( (a_track = anit.current() ) != 0)
    {
        QString title_temp = QString(QObject::tr("%1 - %2")).arg(a_track->Track()).arg(a_track->Title());
        GenericTree *subsub_node = sub_node->addNode(title_temp, a_track->ID(), true);
        subsub_node->setAttribute(0, 1);
        subsub_node->setAttribute(1, track_counter);    // regular order
        subsub_node->setAttribute(2, rand());           // random order

        //
        //  "Intelligent" ordering
        //
        int rating = a_track->Rating();
        int playcount = a_track->PlayCount();
        double lastplaydbl = a_track->LastPlay();
        double ratingValue = (double)(rating) / 10;
        double playcountValue, lastplayValue;

        if (m_playcountMax == m_playcountMin) 
            playcountValue = 0; 
        else 
            playcountValue = ((m_playcountMin - (double)playcount) / (m_playcountMax - m_playcountMin) + 1); 
        if (m_lastplayMax == m_lastplayMin) 
            lastplayValue = 0;
        else 
            lastplayValue = ((m_lastplayMin - lastplaydbl) / (m_lastplayMax - m_lastplayMin) + 1);

        double rating_value =  (m_RatingWeight * ratingValue + m_PlayCountWeight * playcountValue +
                                m_LastPlayWeight * lastplayValue + m_RandomWeight * (double)rand() /
                                (RAND_MAX + 1.0));
        int integer_rating = (int) (4000001 - rating_value * 10000);
        subsub_node->setAttribute(3, integer_rating);   //  "intelligent" order
        ++track_counter;
        ++anit;
    }  

    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *sub_traverse;
    int another_counter = 0;
    iter.toFirst();
    while( (sub_traverse = iter.current() ) != 0)
    {
        sub_traverse->setPlayCountMin(m_playcountMin);
        sub_traverse->setPlayCountMax(m_playcountMax);
        sub_traverse->setLastPlayMin(m_lastplayMin);
        sub_traverse->setLastPlayMax(m_lastplayMax);
        sub_traverse->writeTree(sub_node, another_counter);
        ++another_counter;
        ++iter;
    }
}


void MusicNode::sort()
{
    //  Sort any tracks
    my_tracks.sort();

    //  Sort any subnodes
    my_subnodes.sort();
    
    //  Tell any subnodes to sort themselves
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *crawler;
    while ( (crawler = iter.current()) != 0 )
    {
        crawler->sort();
        ++iter;
    }
}


void MusicNode::printYourself(int indent_level)
{

    for(int i = 0; i < (indent_level) * 4; ++i)
    {
        cout << " " ;
    }
    cout << my_title << endl;

    QPtrListIterator<Metadata>  anit(my_tracks);
    Metadata *a_track;
    while( (a_track = anit.current() ) != 0)
    {
        for(int j = 0; j < (indent_level + 1) * 4; j++)
        {
            cout << " " ;
        } 
        cout << a_track->Title() << endl ;
        ++anit;
    }       
    
    QPtrListIterator<MusicNode> iter(my_subnodes);
    MusicNode *print;
    while( (print = iter.current() ) != 0)
    {
        print->printYourself(indent_level + 1);
        ++iter;
    }
}

/**************************************************************************/

int MetadataPtrList::compareItems(QPtrCollection::Item item1, 
                                  QPtrCollection::Item item2)
{
    return ((Metadata*)item1)->compare((Metadata*)item2);
}

int MusicNodePtrList::compareItems (QPtrCollection::Item item1, 
                                    QPtrCollection::Item item2)
{
    MusicNode *itemA = (MusicNode*)item1;
    MusicNode *itemB = (MusicNode*)item2;

    QString title1 = itemA->getTitle().lower();
    QString title2 = itemB->getTitle().lower();
    
    // Cut "the " off the front of titles
    if (title1.left(4) == thePrefix) 
        title1 = title1.mid(4);
    if (title2.left(4) == thePrefix) 
        title2 = title2.mid(4);

    return title1.localeAwareCompare(title2);
}
