/*
	mmusic.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Stay on top of MythMusic data

*/

#include "../../../config.h"

#include <qdir.h>
#include <qfileinfo.h>

#include "mmusic.h"
#include "mfd_events.h"
#include "settings.h"
#include "decoder.h"
#include "mythdigest.h"


MusicFile::MusicFile()
{
    //
    //  Backstop defaults;
    //

    file_name = "";
    QDateTime when;
    when.setTime_t(0);
    last_modification = when;
    myth_digest = "";
    checked_database = false;
    metadata_id = -1;
    database_id = -1;
}

MusicFile::MusicFile(const QString &a_file_name, QDateTime last_modified)
{
    file_name = a_file_name;
    last_modification = last_modified;
    checked_database = false;
    metadata_id = -1;
    database_id = -1;
}

bool MusicFile::calculateMythDigest()
{
    MythDigest a_digest(file_name);
    myth_digest = a_digest.calculate();
    if(myth_digest.length() < 1)
    {
        cerr << "myth digest error on filename of \"" << file_name << "\"" << endl;
        return false;
    }
    return true;
}

/*
---------------------------------------------------------------------
*/


MMusicWatcher::MMusicWatcher(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, -1, "mythmusic watcher", false)
{
    //
    //  Get a reference to the metadata server
    //
    
    metadata_server = parent->getMetadataServer();
    
    //
    //  This is a "magic" number signifying what we want to see
    //
    
    desired_database_version = "1002";

    //
    //  Initialize our contianer and set things up for a clean slate
    //
    
    initialize();
}

void MMusicWatcher::initialize()
{

    //
    //  On startup, we want it to sweep
    //

    force_sweep = true;

    //
    //  Clear warning flags
    //
    
    sent_directory_warning = false;
    sent_dir_is_not_dir_warning = false;
    sent_dir_is_not_readable_warning = false;
    sent_musicmetadata_table_warning = false;
    sent_playlist_table_warning = false;
    sent_database_version_warning = false;

    //
    //  Set to have no new data and no history. We create these lists
    //  (during a sweep), but the metadata server deletes them
    //

    new_metadata = new QIntDict<Metadata>;
    new_metadata->resize(9973);  // big prime
    metadata_additions.clear();
    metadata_deletions.clear();
    previous_metadata.clear();

    new_playlists = new QIntDict<Playlist>;
    previous_playlists.clear();
    playlist_additions.clear();
    playlist_deletions.clear();
    
    master_list.clear();
    latest_sweep.clear();
    files_to_ignore.clear();

    //
    //  Get a metadata container
    //
    
    metadata_container = 
        metadata_server->createContainer(MCCT_audio, MCLT_host);
    container_id = metadata_container->getIdentifier();
    current_metadata_id = 0;
    
    //
    //  Fill our container with an empty set of metadata and an empty set of
    //  playlists
    //
    
    metadata_server->doAtomicDataSwap(  
                                        metadata_container, 
                                        new_metadata, 
                                        metadata_additions,
                                        metadata_deletions,
                                        new_playlists,
                                        playlist_additions,
                                        playlist_deletions
                                     );
                                     
    //
    //  Now that we have handed the metadata to the metadata server,
    //  dereference it
    //
    
    new_metadata = NULL;
    new_playlists = NULL;
    
}

void MMusicWatcher::run()
{
    //
    //  set our priority
    //
    
    int nice_level = mfdContext->getNumSetting("plugin-mmmusic-nice", 18);
    nice(nice_level);
    
    //
    //  pointer to the database
    //
    
    db = parent->getDatabase();
    if(!db)
    {
        warning("cant't talk to database ... I'm out of here");
        return;
    }

    //
    //  Set a time stamp
    //

    metadata_sweep_time.start();
    
    while(keep_going)
    {
        //
        //  Check to see if our sweep interval has elapsed (default is 15
        //  minutes). Set to 0 to only sweep when a sweep is forced
        //

        int sweep_wait = mfdContext->getNumSetting("music_sweep_time", 1) * 60 * 1000;  
        if( ( metadata_sweep_time.elapsed() > sweep_wait  &&
              sweep_wait > 0 && keep_going ) || ( force_sweep && keep_going) )
        {
            //
            //  Toggle forced sweeping back off
            //

            force_sweep_mutex.lock();
                force_sweep = false;
            force_sweep_mutex.unlock();
            
            //
            //  Check to see if anything has changed
            //
            
            new_metadata = new QIntDict<Metadata>;
            new_metadata->resize(9973); // big prime
            metadata_additions.clear();
            metadata_deletions.clear();
            
            new_playlists = new QIntDict<Playlist>;
            playlist_additions.clear();
            playlist_deletions.clear();
            
            if(sweepMetadata())
            {
                //
                //  Do the atomic delta (sing it with me now ...)
                //
                
                metadata_server->doAtomicDataDelta(  
                                                    metadata_container, 
                                                    new_metadata, 
                                                    metadata_additions,
                                                    metadata_deletions,
                                                    new_playlists,
                                                    playlist_additions,
                                                    playlist_deletions
                                                  );
                
                //
                //  the metadata server owns the data now (and will delete
                //  it when required). We point away from it.
                //
                
                new_metadata = NULL;
                new_playlists = NULL;

                //
                //  Something changed. Fire off an event (this will tell the
                //  mfd to tell all the plusins (that care) that it's time
                //  to update)
                //
                
                MetadataChangeEvent *mce = new MetadataChangeEvent(container_id);
                QApplication::postEvent(parent, mce);    
                
            }
            else
            {
                //
                //  Nothing changed, so delete the storage that would have
                //  been used if something changed
                //
                
                delete new_metadata;
                new_metadata = NULL;
                delete new_playlists;
                new_playlists = NULL;
                
            }
            
            //
            //  In any case, reset the time stamp
            //

            metadata_sweep_time.restart();

        }
        else
        {
            //
            //  Sleep for a while, unless someone wakes us up. But only
            //  sleep to a time close to our next sweep time
            //

            int seconds_to_sleep = (sweep_wait - metadata_sweep_time.elapsed()) / 1000;
            if(seconds_to_sleep > 0)
            {
                setTimeout(seconds_to_sleep, 0);
                waitForSomethingToHappen();
            }
        }
    }
    
    //
    //  We are going away, tell the metadata server we no longer exist
    //

    metadata_server->deleteContainer(container_id);
}


bool MMusicWatcher::sweepMetadata()
{

    QTime sweep_timer;
    sweep_timer.start();

    //
    //  Figure out where the files are. We do this ever sweep, so ... in
    //  principal ... the user can redefine the music location with the mfd
    //  running, and this code will migrate metadata searching over to the
    //  new location.
    //
    
    QString startdir = mfdContext->GetSetting("MusicLocation");
    if(startdir.length() < 1)
    {
        warning("cannot look for music file paths "
                "starting with a null string");
        removeAllMetadata();
        return false;
    }
    startdir = QDir::cleanDirPath(startdir);
    if(!startdir.endsWith("/"));
    {
        startdir += "/";
    }


    //
    //  Check that the file location is valid and that the db is there and
    //  sensible
    //

    if(!checkDataSources(startdir, db))
    {
        //
        //  Somethings wrong, so no metadata can exist.
        //

        removeAllMetadata();
        return false;
    }

    //
    //  If we made it this far, we may need a container (in case we
    //  recovered from previous checkDataSources() failues)
    //
    
    if(!metadata_container)
    {
        if(new_metadata)
        {
            delete new_metadata;
            new_metadata = NULL;
        }
        if(new_playlists)
        {
            delete new_playlists;
            new_playlists = NULL;
        }

        initialize();

        new_metadata = new QIntDict<Metadata>;
        new_metadata->resize(9973);  // big prime
        new_playlists = new QIntDict<Playlist>;

    }


    //
    //  If our latest_sweep file list is empty (the fast one that just has
    //  file names in it with no digest), then go ahead and generate it.
    //
    
    if(latest_sweep.count() < 1)
    {
        QTime build_file_list_timer;
        build_file_list_timer.start();
        buildFileList(startdir, latest_sweep);

        if(latest_sweep.count() < 1)
        {
            warning(QString("not updating music files as none were "
                            "found in \"%1\"")
                            .arg(startdir));
            return false;
        }
        else
        {
            log(QString("built simple list of %1 audio files in %2 second(s)")
                .arg(latest_sweep.count())
                .arg(build_file_list_timer.elapsed() / 1000.0), 9);
        }


        //
        //  If there's anything in our database and/or master file list which
        //  is not now in this new fast/complete sweep, it should go
        //
        
        checkForDeletions(latest_sweep, startdir);
    }
   

    //
    //  Now, we need to check our master list against the latest sweep
    //
    
    compareToMasterList(latest_sweep, startdir);


    //
    //  Now go through the new metadata and make sure it's in the database
    //  with all the correct fields
    //

    checkDatabaseAgainstMaster(startdir);


    if(
        metadata_additions.count() > 0 ||
        metadata_deletions.count() > 0 ||
        playlist_additions.count() > 0 ||
        playlist_deletions.count() > 0
      )
    {
        log(QString("processed +%1/-%2 item(s) and +%3/-%4 playlist(s) in %5 second(s)")
            .arg(metadata_additions.count())
            .arg(metadata_deletions.count())
            .arg(playlist_additions.count())
            .arg(playlist_deletions.count())
            .arg(sweep_timer.elapsed() / 1000.0), 8);
        return true;
    }
    return false;

}


void MMusicWatcher::checkForDeletions(MusicFileMap &music_files, const QString &startdir)
{
    //
    //  This is called whenever we have a clean fresh fast sweep. We begin
    //  by removing anything that appears on the master list which is _not_
    //  in the new sweep
    //
    
    MusicFileMap::Iterator it;
    for ( it = master_list.begin(); it != master_list.end(); it++)
    {
        if(music_files.find(it.key()) == music_files.end())
        {
            metadata_deletions.push_back(it.data().getMetadataId());
            log(QString("removed \"%1\" from master list")
                .arg(it.key()), 7);
            master_list.remove(it);
        }
    }
    
    //
    //  We also have to go through the whole database and delete any rows
    //  that do not refer to something in the sweep
    //
    
    QSqlQuery query("SELECT intid, filename FROM musicmetadata ;");
    
    if(query.isActive())
    {
        if(query.numRowsAffected() > 0)
        {
            query.next();
            if(music_files.find(startdir + query.value(1).toString()) == music_files.end())
            {
                //
                //  This record should _not_ be in the database
                //
                QSqlQuery delete_query;
                delete_query.prepare("DELETE FROM musicmetadata WHERE intid = ?");
                delete_query.bindValue(0, query.value(0).toUInt());
                delete_query.exec();
            }
        }
    }
    else
    {
        warning("something wrong with your musicmetadata table");
    }
    
}

bool MMusicWatcher::checkDataSources(const QString &startdir, QSqlDatabase *a_db)
{

    //
    //  Make sure the place to look exists, is a directory, and is readable
    //            

    QFileInfo starting_directory(startdir);
    if (!starting_directory.exists())
    {
        if(!sent_directory_warning)
        {
            warning(QString("cannot look for files, directory "
                            "does not exist: \"%1\"")
                            .arg(startdir));
            sent_directory_warning = true;
        }
        return false;
    }

    if (!starting_directory.isDir())
    {
        if(!sent_dir_is_not_dir_warning)
        {
            warning(QString("cannot look for files, starting "
                            "point is not a directory: \"%1\"")
                            .arg(startdir));
            sent_dir_is_not_dir_warning = true;
        }
        return false;
    }

    if (!starting_directory.isReadable())
    {
        if(!sent_dir_is_not_readable_warning)
        {
            warning(QString("cannot look for files, starting "
                            "directory is not readable "
                            "(permissions?): \"%1\"")
                            .arg(startdir));
            sent_dir_is_not_readable_warning = true;
        }
        return false;
    }
    
    //
    //  Check the database version 
    //

    QString dbver = mfdContext->GetSetting("MusicDBSchemaVer");

    if (dbver != desired_database_version)
    {
        if(!sent_database_version_warning)
        {
            warning(QString("got desired (%1) versus actual (%2) "
                            "database mismatch, will not touch db")
                            .arg(desired_database_version)
                            .arg(dbver));
            sent_database_version_warning = true;
        }
        return false;
    }
                
    //
    //  Make sure the db exists, and we can see the two tables
    //
    
    QSqlQuery query("SELECT COUNT(filename) FROM musicmetadata;", a_db);
    
    if(!query.isActive())
    {
        if(!sent_musicmetadata_table_warning)
        {
            warning("cannot get data from a table called musicmetadata");
            sent_musicmetadata_table_warning = true;
        }
        return false;
        
    }
    
    QSqlQuery pl_query("SELECT COUNT(playlistid) FROM musicplaylist ", a_db);

    if(!pl_query.isActive())
    {
        if(!sent_playlist_table_warning)
        {
            warning("cannot get data from a table called musisplaylist");
            sent_playlist_table_warning = true;
        }
        return false;
        
    }
    
    sent_directory_warning = false;
    sent_dir_is_not_dir_warning = false;
    sent_dir_is_not_readable_warning = false;
    sent_musicmetadata_table_warning = false;
    sent_playlist_table_warning = false;
    sent_database_version_warning = false;
    return true;
}

void MMusicWatcher::removeAllMetadata()
{
    //
    //  Make the metadata server understand that we have _no_ metadata
    //

    if(metadata_container)
    {
        metadata_server->deleteContainer(metadata_container->getIdentifier());
        metadata_container = NULL;
        container_id = -1;
    }        
    
    //
    //  Make sure this object understands it has no metadata
    //
    
    master_list.clear();
    latest_sweep.clear();
    files_to_ignore.clear();
    
    
}

void MMusicWatcher::buildFileList(const QString &directory, MusicFileMap &music_files)
{

    //
    //  Recursively search to get all music files
    //

    QDir current_directory(directory);

    if (!current_directory.exists())
    {
        return;
    }

    const QFileInfoList *list = current_directory.entryInfoList();
    if (!list)
    {
        return;
    }

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
        {
            continue;
        }
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {
            //
            //  Recursively call myself
            //

            buildFileList(filename, music_files);
        }
        else
        {
            //
            //  If this is a music file (based on file extensions),
            //  then include it in the list
            //
            
            if(
                fi->extension(FALSE) == "ogg"  ||
                fi->extension(FALSE) == "flac" ||
#ifdef WMA_AUDIO_SUPPORT
                fi->extension(FALSE) == "wma"  ||
#endif
#ifdef AAC_AUDIO_SUPPORT
                fi->extension(FALSE) == "m4a"  ||
#endif
                fi->extension(FALSE) == "mp3" 
              )
            {
                if(files_to_ignore.find(filename) == files_to_ignore.end())
                {
                    MusicFile new_music_file(filename, fi->lastModified());
                    music_files[filename] = new_music_file;
                }
            }
        }
    }
}


void MMusicWatcher::compareToMasterList(MusicFileMap &music_files, const QString &startdir)
{
    //
    //  Is this file in our master list? (check a maximum of
    //  MusicFilesAtATime)
    //

    int music_files_at_a_time = mfdContext->getNumSetting("MusicFilesAtATime", 100);
    int counter = 0;
    MusicFileMap::Iterator it;
    for ( it = music_files.begin(); it != music_files.end(); )
    {
        if(master_list.find(it.key()) == master_list.end())
        {
            //
            //  New file found
            //

            counter++;
            
            MusicFile new_master_item(it.key(), it.data().lastModified());
            if(new_master_item.calculateMythDigest())
            {
                master_list[it.key()] = new_master_item;
            }
            else
            {
                //
                //  File is there, but I can't seem to do anything with it
                //

                files_to_ignore.push_back(it.key());
                log(QString("could not get digest (permissions problem?) for file: \"%1\"")
                    .arg(it.key()), 4);
            }
        }
        else if(master_list.find(it.key()).data().lastModified() != it.data().lastModified())
        {

            //
            //  It's there, but it changed modification dates
            //
            
            counter++;


            //
            //  Try and update it's metadata
            //

            QString file_name = it.key();

            AudioMetadata *new_item = loadFromDatabase(file_name, startdir);
            metadata_deletions.push_back(master_list.find(it.key()).data().getMetadataId());
            master_list.remove(master_list.find(it.key()));

            if(updateMetadata(new_item))
            {
                persistMetadata(new_item);
                MusicFile new_master_item(file_name, it.data().lastModified());
                new_master_item.setMetadataId(new_item->getId());
                new_master_item.setDbId(new_item->getDbId());
                new_master_item.setMythDigest(new_item->getMythDigest());

                master_list[file_name] = new_master_item;
           }
           else
           {
                //
                //  We failed to get information about this file.
                //  Remove it from the master list and put it on the
                //  ignore list
                //
               
                files_to_ignore.push_back(file_name);
                log(QString("file could not be opened/decoded: \"%1\"")
                    .arg(file_name), 4);
           }
        }

        //
        //  Remove this one
        //  
        
        music_files.remove(it);

        if(counter < music_files_at_a_time && keep_going)
        {
            ++it;
        }
        else
        {
            it = music_files.end();
            
            //
            //  We have done music_files_at_a_time worth of files, but there
            //  are still more to do. We set force_sweep on again so that
            //  the next batch will get looked at right away.
            //
            
            force_sweep_mutex.lock();
                force_sweep = true;
            force_sweep_mutex.unlock();
            
        }
    }
}


void MMusicWatcher::checkDatabaseAgainstMaster(const QString &startdir)
{

    //
    //  3 possibilities here:
    //
    //      File not in database --> add it to database if we can decode its
    //      metadata
    //
    //      File in database, but digest mismatch --> modify its database
    //      record if we can decode its (apparently new) metadata
    //
    //      File in database & digests match --> excellent
    //
    
    MusicFileMap::Iterator it;
    for ( it = master_list.begin(); it != master_list.end(); )
    {
        if(!it.data().checkedDatabase())
        {
            AudioMetadata *new_item = loadFromDatabase(it.key(), startdir);
            if(!new_item)
            {
                new_item = checkNewFile(it.key(), startdir);
            }
            
            if(new_item)
            {
                //
                //  it's in the database, check the digests
                //
                
                if(new_item->getMythDigest() == it.data().getMythDigest())
                {
                    //
                    //  Excellent
                    //

                    new_metadata->insert(new_item->getId(), new_item);
                    metadata_additions.push_back(new_item->getId());
                    it.data().checkedDatabase(true);
                    it.data().setMetadataId(new_item->getId());
                    it.data().setDbId(new_item->getDbId());
                }
                else
                {
                    //
                    //  Try and update it's metadata
                    //

                    if(updateMetadata(new_item))
                    {
                        persistMetadata(new_item);
                        new_metadata->insert(new_item->getId(), new_item);
                        metadata_additions.push_back(new_item->getId());
                        it.data().checkedDatabase(true);
                        it.data().setMetadataId(new_item->getId());
                        it.data().setDbId(new_item->getDbId());
                    }
                    else
                    {
                        //
                        //  We failed to get information about this file.
                        //  Remove it from the master list and put it on the
                        //  ignore list
                        //
                        
                        files_to_ignore.push_back(it.key());
                        log(QString("file could not be opened and/or decoded: \"%1\"")
                                    .arg(it.key()), 4);
                        
                        master_list.remove(it);
                    }
                }
            }
            else
            {
                //
                //  We failed to get information about this file.
                //  Remove it from the master list and put it on the
                //  ignore list
                //
                        
                files_to_ignore.push_back(it.key());
                log(QString("file could neither be loaded from database "
                            "nor opened as a new file: \"%1\"")
                            .arg(it.key()), 4);
                        
                master_list.remove(it);
            }
        }
        if(keep_going)
        {
            ++it;
        }
        else
        {
            it = master_list.end();
        }
    }
}

AudioMetadata* MMusicWatcher::loadFromDatabase(
                                                const QString &file_name, 
                                                const QString &startdir
                                              )
{
  
    //
    //  Load the metadata
    //

    QString non_const_startdir = startdir;
    QString sqlfilename = file_name;
    sqlfilename = sqlfilename.remove(0, non_const_startdir.length());

    QSqlQuery query(NULL, db);

    query.prepare("SELECT intid, artist, album, title, genre, "
                  "year, tracknum, length, rating, "
                  "lastplay, playcount, mythdigest, size, date_added, "
                  "date_modified, format, description, comment, "
                  "compilation, composer, disc_count, disc_number, "
                  "track_count, start_time, stop_time, eq_preset, "
                  "relative_volume, sample_rate, bpm "
                  "FROM musicmetadata WHERE filename = ? ;");

    query.bindValue(0, sqlfilename);
    
    query.exec();

    if (query.isActive())
    {
        if(query.numRowsAffected() > 0)
        {
            while (query.next())
            {
                //
                //  Convert datetime stuff
                //

                QDateTime seconds_since_disco;
                seconds_since_disco.setTime_t(0);
                
                QDateTime lastplay = seconds_since_disco;
                QDateTime date_added = seconds_since_disco;
                QDateTime date_modified = seconds_since_disco;               
                
                if(!query.value(9).isNull())
                {
                    lastplay  = getQtTimeFromMySqlTime(query.value(9).toString());
                }
                
                if(!query.value(13).isNull())
                {
                    date_added = getQtTimeFromMySqlTime(query.value(13).toString());
                }
                
                if(!query.value(14).isNull())
                {
                    date_modified = getQtTimeFromMySqlTime(query.value(14).toString());
                }
                
                //
                //  Build basic audio metadata object
                //

                AudioMetadata *new_audio = new AudioMetadata
                                    (
                                        container_id,
                                        bumpMetadataId(),
                                        QUrl("file://" + file_name),
                                        query.value(8).toInt(),
                                        lastplay,
                                        query.value(10).toInt(),
                                        query.value(1).toString(),
                                        query.value(2).toString(),
                                        query.value(3).toString(),
                                        query.value(4).toString(),
                                        query.value(5).toInt(),
                                        query.value(6).toInt(),
                                        query.value(7).toInt()
                                    );

                new_audio->setDbId(query.value(0).toInt());
                
                //
                //  Depending on what else came in on the query, set other values
                //
                
                QString mythdigest = query.value(11).toString();
                if(mythdigest.length() > 0)
                {
                    new_audio->setMythDigest(mythdigest);
                }
                
                if(date_added.isValid())
                {
                    new_audio->setDateAdded(date_added);
                }
                
                if(date_modified.isValid())
                {
                    new_audio->setDateModified(date_modified);
                }
                
                bool ok;
                uint size = query.value(12).toUInt(&ok);
                if(ok)
                {
                    new_audio->setSize(size);
                }

                QString format = query.value(15).toString();
                if(format.length() > 0)
                {
                    new_audio->setFormat(format);    
                }

                QString description = query.value(16).toString();
                if(description.length() > 0)
                {
                    new_audio->setDescription(description);
                }

                QString comment = query.value(17).toString();
                if(comment.length() > 0)
                {
                    new_audio->setComment(comment);
                }
                
                uint compilation = query.value(18).toUInt(&ok);
                if(ok)
                {
                    if(compilation)
                    {
                        new_audio->setCompilation(true);
                    }
                    else
                    {
                        new_audio->setCompilation(false);
                    }
                }

                QString composer = query.value(19).toString();
                if(composer.length() > 0)
                {
                    new_audio->setComment(composer);
                }
                
                new_audio->setDiscCount(query.value(20).toUInt());
                new_audio->setDiscNumber(query.value(21).toUInt());
                new_audio->setTrackCount(query.value(22).toUInt());

                uint start_time = query.value(23).toUInt(&ok);
                if(ok)
                {
                    new_audio->setStartTime(start_time);
                }

                uint stop_time = query.value(24).toUInt(&ok);
                if(ok)
                {
                    new_audio->setStopTime(stop_time);
                }

                QString eq_preset = query.value(25).toString();
                if(eq_preset.length() > 0)
                {
                    new_audio->setEqPreset(eq_preset);
                }                

                int relative_volume = query.value(26).toInt(&ok);
                if(ok)
                {
                    new_audio->setRelativeVolume(relative_volume);
                }
                
                uint sample_rate = query.value(27).toUInt(&ok);
                if(ok)
                {
                    new_audio->setSampleRate(sample_rate);
                }
                
                uint bpm = query.value(28).toUInt(&ok);
                if(ok)
                {
                    new_audio->setBpm(bpm);
                }
                
                
                //
                //  hand it back
                //
                
                return new_audio;
            }
        }
    }
    else
    {
        warning("could not seem to open your musicmetadata table ... giving up");
        return NULL;
    }
    


    return NULL;

}

AudioMetadata *MMusicWatcher::checkNewFile(
                                            const QString &filename,
                                            const QString &startdir
                                          )
{

    AudioMetadata *new_item = NULL;
    
    //
    //  We have found a new file. Either it should get added to the database
    //  (if it's supported and decodable) or we log a warning and then ignore it.
    //

    new_item = getMetadataFromFile(filename);
    
    if(new_item)
    {
        //
        //  Cool ... we could decode it ... give it place in the database.
        //
        
        QString non_const_startdir = startdir;
        QString sqlfilename = filename;
        sqlfilename = sqlfilename.remove(0, non_const_startdir.length());
        
        QSqlQuery query;
        query.prepare("INSERT INTO musicmetadata (filename, mythdigest) "
                      "values ( ? , ?)");

        query.bindValue(0, sqlfilename);
        query.bindValue(1, new_item->getMythDigest());
        
        query.exec();
        
        if(query.numRowsAffected() < 1)
        {
            warning("failed to insert new row for new metadata file (?)");
            delete new_item;
            return NULL;
        }
        
        QSqlQuery retrieve_query;
        retrieve_query.prepare("SELECT intid FROM musicmetadata "
                               "WHERE mythdigest = ? ;");
        retrieve_query.bindValue(0, new_item->getMythDigest());
        retrieve_query.exec();
        if(retrieve_query.numRowsAffected() < 1)
        {
            warning("failed to get back something we _just_ put in "
                    "the database");
            delete new_item;
            return NULL;
        }
        retrieve_query.next();
        
        //
        //  Set some values for new data
        //

        QDateTime earliest_possible;
        earliest_possible.setTime_t(0);
        
        new_item->setDbId(retrieve_query.value(0).toUInt());
        new_item->setDateAdded(QDateTime::currentDateTime());
        new_item->setLastPlayed(earliest_possible);
        
        log(QString("added audio file: \"%1\"").arg(filename), 4);
        
        //
        //  Fill out its database info.
        //
        
        persistMetadata(new_item);    
        
    }

    
    return new_item;
}

bool MMusicWatcher::updateMetadata(AudioMetadata *an_item)
{
    //
    //  This metadata has the right file name, but it's digest is not
    //  correct. Update it (if we can), save new settings back to the
    //  database, and return
    //
    
    
    AudioMetadata *corrected_item = getMetadataFromFile(an_item->getUrl().path());
    if(corrected_item)
    {
        //
        //  Swap data from the corrected item in the one that was passed to us  
        //
        
        an_item->setTitle(corrected_item->getTitle());
        an_item->setArtist(corrected_item->getArtist());
        an_item->setAlbum(corrected_item->getAlbum());
        an_item->setGenre(corrected_item->getGenre());
        an_item->setTrack(corrected_item->getTrack());
        an_item->setYear(corrected_item->getYear());
        an_item->setMythDigest(corrected_item->getMythDigest());
        an_item->setSize(corrected_item->getSize());
        an_item->setStartTime(corrected_item->getStartTime());
        an_item->setStopTime(corrected_item->getStopTime());
        an_item->setFormat(corrected_item->getFormat());
        an_item->setDateModified(corrected_item->getDateModified());

        delete corrected_item;
        log(QString("updated metadata for audio item \"%1\"")
            .arg(an_item->getId()), 8);

        return true;
        
    }
    
    warning(QString("failed to update metadata for \"%1\"")
            .arg(an_item->getUrl().path()));
    return false;
}

void MMusicWatcher::persistMetadata(AudioMetadata *an_item)
{
    //
    //  Save back to database while preserving id number. 
    //
    
    QString last_played = an_item->getLastPlayed().toString("yyyyMMddhhmmss");
    QString date_added = an_item->getDateAdded().toString("yyyyMMddhhmmss");
    QString date_modified = an_item->getDateModified().toString("yyyyMMddhhmmss");
    
    //
    //  Long and ugly, but easy to see what is going on
    //


    QSqlQuery query;
    
    query.prepare("UPDATE musicmetadata SET "
                  "title = ? , "
                  "artist = ? , "
                  "album = ? , "
                  "genre = ? , "
                  "year = ? , "
                  "tracknum = ? , "
                  "length = ? , "
                  "rating = ? , "
                  "lastplay = ? , "
                  "playcount = ? , "
                  "mythdigest = ? , "
                  "size = ? , "
                  "date_added = ? , "
                  "date_modified = ? , "
                  "format = ? , "
                  "description = ? , "
                  "comment = ? , "
                  "compilation = ? , "
                  "composer = ? , "
                  "disc_count = ? , "
                  "disc_number = ? , "
                  "track_count = ? , "
                  "start_time = ? , "
                  "stop_time = ? , "
                  "eq_preset = ? , "
                  "relative_volume = ? , "
                  "sample_rate = ? , "
                  "bpm = ?  "
                  "WHERE intid = ? ;");

    query.bindValue(0,  an_item->getTitle());
    query.bindValue(1,  an_item->getArtist());
    query.bindValue(2,  an_item->getAlbum());
    query.bindValue(3,  an_item->getGenre());
    query.bindValue(4,  an_item->getYear());
    query.bindValue(5,  an_item->getTrack());
    query.bindValue(6,  an_item->getLength());
    query.bindValue(7,  an_item->getRating());
    query.bindValue(8,  last_played);
    query.bindValue(9,  an_item->getPlayCount());
    query.bindValue(10, an_item->getMythDigest());
    query.bindValue(11, an_item->getSize());
    query.bindValue(12, date_added);
    query.bindValue(13, date_modified);
    query.bindValue(14, an_item->getFormat());
    query.bindValue(15, an_item->getDescription());
    query.bindValue(16, an_item->getComment());
    query.bindValue(17, an_item->getCompilation());
    query.bindValue(18, an_item->getComposer());
    query.bindValue(19, an_item->getDiscCount());
    query.bindValue(20, an_item->getDiscNumber());
    query.bindValue(21, an_item->getTrackCount());
    query.bindValue(22, an_item->getStartTime());
    query.bindValue(23, an_item->getStopTime());
    query.bindValue(24, an_item->getEqPreset());
    query.bindValue(25, an_item->getRelativeVolume());
    query.bindValue(26, an_item->getSampleRate());
    query.bindValue(27, an_item->getBpm());
    query.bindValue(28, an_item->getDbId());


    
    query.exec();

    if (query.numRowsAffected() < 1)
    {
        warning(QString("failed to update metadata for \"%1\" (intid=%2)")
                .arg(an_item->getTitle())
                .arg(an_item->getId()));
    }
}


AudioMetadata* MMusicWatcher::getMetadataFromFile(QString file_path)
{

    AudioMetadata *return_value = NULL;

    Decoder *decoder = Decoder::create(file_path, NULL, NULL, true);
    decoder->setParent(this);
    
    if(decoder)
    {
        return_value = decoder->getMetadata();
        if(return_value)
        {
            //
            //  We have the basic tags set, add whatever more we can
            //

            MythDigest new_digest(file_path);
            return_value->setMythDigest(new_digest.calculate());

            QFileInfo file_info(file_path);
            return_value->setSize(file_info.size());
            return_value->setStartTime(0);
            return_value->setStopTime(return_value->getLength());
            return_value->setFormat(file_info.extension(false));
            return_value->setDateModified(QDateTime::currentDateTime());
        }
        else
        {
            cout << "got decoder, but no metadata back" << endl;
        }
        delete decoder;   
    }
    else
    {
        cout << "got no decoder" << endl;
    }
    return return_value;
}


QDateTime MMusicWatcher::getQtTimeFromMySqlTime(QString timestamp)
{
    if(timestamp.contains('-') < 1)
    {
        timestamp.insert(4, '-');
        timestamp.insert(7, '-');
        timestamp.insert(10, 'T');
        timestamp.insert(13, ':');
        timestamp.insert(16, ':');
    }

    return QDateTime::fromString(timestamp, Qt::ISODate);
}

int MMusicWatcher::bumpMetadataId()
{
    int return_value;
    current_metadata_id_mutex.lock();
        current_metadata_id++;
        return_value = current_metadata_id;
    current_metadata_id_mutex.unlock();
    return return_value;
}

MMusicWatcher::~MMusicWatcher()
{

    if(new_metadata)
    {
        delete new_metadata;
        new_metadata = NULL;
    }
    if(new_playlists)
    {
        delete new_playlists;
        new_playlists = NULL;
    }

}
