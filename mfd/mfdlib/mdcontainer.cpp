/*
	mdcontainer.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata container(s) and thread(s)

*/

#include "../config.h"

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qdir.h>

#include "mdcontainer.h"
#include "../mfd/mfd.h"
#include "mfd_events.h"

MetadataContainer::MetadataContainer(
                                        MFD *l_parent, 
                                        int l_unique_identifier,
                                        MetadataCollectionContentType  l_content_type,
                                        MetadataCollectionLocationType l_location_type
                                    )
{
    parent = l_parent;
    unique_identifier = l_unique_identifier;
    content_type = l_content_type;
    location_type = l_location_type;
    current_metadata = NULL;
    current_playlists = NULL;
    current_playlist_id = 2;
    generation = 1;
}

void MetadataContainer::log(const QString &log_message, int verbosity)
{
    if(parent)
    {
        LoggingEvent *le = new LoggingEvent(log_message, verbosity);
        QApplication::postEvent(parent, le);
    }
}

void MetadataContainer::warning(const QString &warning_message)
{
    QString warn_string = warning_message;
    warn_string.prepend("WARNING: ");
    log(warn_string, 1);
}

bool MetadataContainer::isAudio()
{
    if(content_type == MCCT_audio)
    {
        return true;
    }
    return false;
}


bool MetadataContainer::isVideo()
{
    if(content_type == MCCT_video)
    {
        return true;
    }
    return false;
}


bool MetadataContainer::isLocal()
{
    if(location_type == MCLT_host)
    {
        return true;
    }
    return false;
}

uint MetadataContainer::getMetadataCount()
{
    if(current_metadata)
    {
        return current_metadata->count();
    }
    return 0;
}

uint MetadataContainer::getPlaylistCount()
{
    if(current_playlists)
    {
        return current_playlists->count();
    }
    return 0;
}

Metadata* MetadataContainer::getMetadata(int item_id)
{
    if(current_metadata)
    {
        return current_metadata->find(item_id);
    }
    return NULL;
}

Playlist* MetadataContainer::getPlaylist(int pl_id)
{
    if(current_playlists)
    {
        return current_playlists->find(pl_id);
    }

    return NULL;
}

void MetadataContainer::dataSwap(
                                    QIntDict<Metadata>* new_metadata, 
                                    QValueList<int> metadata_in,
                                    QValueList<int> metadata_out,
                                    QIntDict<Playlist>* new_playlists,
                                    QValueList<int> playlist_in,
                                    QValueList<int> playlist_out,
                                    bool rewrite_playlists
                                )
{
    if(current_metadata)
    {
        current_metadata->setAutoDelete(true);
        delete current_metadata;
        current_metadata = NULL;
    }
    
    current_metadata = new_metadata;
    
    //
    //  Dereference the pointers that were passed
    //

    new_metadata = NULL;

    //
    //  NB: these are QDeepCopy variables, so this object now has its own
    //  private copy of the deltas (additions, deletions). That means the
    //  thread that assigned them can keep looking at (iterating over) it's
    //  copy without screwing up _this_ copy (I think).
    //

    metadata_additions = metadata_in;
    metadata_deletions = metadata_out;


    if(rewrite_playlists)
    {
        //
        //  Make the playlists map what they can to whatever the state of the
        //  metadata is
        //
    
        mapPlaylists(new_playlists, playlist_in, playlist_out, false);
    }
    else
    {
        if(current_playlists)
        {
            current_playlists->setAutoDelete(true);
            delete current_playlists;
            current_playlists = NULL;
        }

        current_playlists = new_playlists;
        new_playlists = NULL;
    
        playlist_additions = playlist_in;
        playlist_deletions = playlist_out;
    }

    ++generation;
}

void MetadataContainer::dataDelta(
                                    QIntDict<Metadata>* new_metadata, 
                                    QValueList<int> metadata_in,
                                    QValueList<int> metadata_out,
                                    QIntDict<Playlist>* new_playlists,
                                    QValueList<int> playlist_in,
                                    QValueList<int> playlist_out,
                                    bool rewrite_playlists
                                )
{
    //
    //  Add the new metadata
    //

    QIntDictIterator<Metadata> it( *new_metadata ); 
    for ( ; it.current(); ++it )
    {
        current_metadata->insert(it.currentKey(), it.current());
    }

    //
    //  Delete the -deltas
    //
    
    current_metadata->setAutoDelete(false);    
    QValueList<int>::iterator iter;
    for ( iter = metadata_out.begin(); iter != metadata_out.end(); ++iter )
    {
        if(!current_metadata->remove((*iter)))
        {
            warning(QString("while doing a neg delta, asked "
                    "to remove something that wasn't there: %1")
                    .arg((*iter)));
        }
    }

    new_metadata->setAutoDelete(false);
    delete new_metadata;
    new_metadata = NULL;


    //
    //  NB: these are QDeepCopy variables, so this object now has its own
    //  private copy of the deltas (additions, deletions). That means the
    //  thread that assigned them can keep looking at (iterating over) it's
    //  copy without screwing up _this_ copy (I think).
    //

    metadata_additions = metadata_in;
    metadata_deletions = metadata_out;

    if(rewrite_playlists)
    {
        //
        //  Make the playlists map what they can to whatever the state of the
        //  metadata is
        //
    
        mapPlaylists(new_playlists, playlist_in, playlist_out, true);
    }
    else
    {
        if(current_playlists)
        {
            current_playlists->setAutoDelete(true);
            delete current_playlists;
        }

        current_playlists = new_playlists;
        new_playlists = NULL;
    
        playlist_additions = playlist_in;
        playlist_deletions = playlist_out;
    }
    
    ++generation;
}

void MetadataContainer::mapPlaylists(
                                        QIntDict<Playlist>* new_playlists, 
                                        QValueList<int> playlist_in,
                                        QValueList<int> playlist_out,
                                        bool delta
                                    )

{
    
    if(delta)
    {

        //
        //  We need to add whatever new playlists there are to whatever we
        //  had already
        //

        if(!current_playlists)
        {
            current_playlists = new QIntDict<Playlist>;
        }

        QIntDictIterator<Playlist> add_it( *new_playlists ); 
        for ( ; add_it.current(); ++add_it )
        {
            add_it.current()->setId(bumpPlaylistId());
            current_playlists->insert(add_it.current()->getId(), add_it.current());
            playlist_in.push_back(add_it.current()->getId());
        }
            
        new_playlists->setAutoDelete(false);
        delete new_playlists;         
        new_playlists = NULL;


    }
    else
    {
        playlist_out.clear();

        //
        //  Take note of the old ones
        //

        if(current_playlists)
        {
            QIntDictIterator<Playlist> del_it( *current_playlists ); 
            for ( ; del_it.current(); ++del_it )
            {
                playlist_out.push_back(del_it.currentKey());
            }
            current_playlists->setAutoDelete(true);
            delete current_playlists;
            current_playlists = NULL;
        }


        //
        //  Build new set of playlists
        //


        current_playlists = new QIntDict<Playlist>;
        playlist_in.clear();
        QIntDictIterator<Playlist> add_it( *new_playlists ); 
        for ( ; add_it.current(); ++add_it )
        {
            add_it.current()->setId(bumpPlaylistId());
            playlist_in.push_back(add_it.current()->getId());
            current_playlists->insert(add_it.current()->getId(), add_it.current());
        }

        new_playlists->setAutoDelete(false);
        delete new_playlists;
        new_playlists = NULL;
    }
    
    //
    //  Always go through all (new and old) and ask them to map out
    //
    
    QIntDictIterator<Playlist> mapout_it( *current_playlists );
    for ( ; mapout_it.current(); ++mapout_it )
    {
        mapout_it.current()->mapDatabaseToId(
                                                current_metadata, 
                                                mapout_it.current()->getDbList(), 
                                                mapout_it.current()->getListPtr(),
                                                current_playlists, 
                                                0   // initial depth is 0
                                            );
    }
                                                                

    playlist_additions = playlist_in;
    playlist_deletions = playlist_out;
}

MetadataContainer::~MetadataContainer()
{
    if(current_metadata)
    {
        current_metadata->setAutoDelete(true);
        delete current_metadata;
        current_metadata = NULL;
    }
    if(current_playlists)
    {
        current_playlists->setAutoDelete(true);
        delete current_playlists;
        current_playlists = NULL;
    }
}

