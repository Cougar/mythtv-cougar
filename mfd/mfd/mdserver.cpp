/*
	mdserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Implementation of a threaded object that opens a server port and
	explains the state of the metadata to any client that asks. It _also_
	holds all the metadatacontainers, so it is _the_ place to ask about what
	content is available

*/

#include <iostream>
using namespace std;

#include "settings.h"

#include "mdserver.h"
#include "mfd_events.h"
#include "httpoutresponse.h"
#include "mdcaprequest.h"

MetadataServer::MetadataServer(MFD* owner, int port)
               :MFDHttpPlugin(owner, -1, port, "metadata server", 2)
{
    metadata_containers = new QPtrList<MetadataContainer>;
    metadata_containers->setAutoDelete(true);
    local_audio_metadata_containers = new QPtrList<MetadataContainer>;
    local_audio_metadata_containers->setAutoDelete(false);
    metadata_local_audio_generation = 4;  //  don't ask
    container_identifier = 0;
    
    local_audio_metadata_count = 0;
    local_audio_playlist_count = 0;
    metadata_container_count = 0;
    last_destroyed_container = -1;

    //
    //  We keep a dictionary (lookup table) of local metadata organized by
    //  mythdigest. That way, if we see a request for non local metadata and
    //  we have the same content locally (based on the myth digest
    //  persistent id), we can return the local content instead.
    //

    local_mythdigest_dictionary = new QDict<Metadata>(9973, false);
}

void MetadataServer::run()
{
    //
    //  Advertise ourselves to the world (via zeroconfig) as an mdcap (Myth
    //  Digital Content Access Protocol) service
    //

    QString local_hostname = mfdContext->getHostName();

    ServiceEvent *se = new ServiceEvent(QString("services add mdcap %1 Myth Metadata Services on %2")
                                       .arg(port_number)
                                       .arg(local_hostname));
    QApplication::postEvent(parent, se);


    //
    //  Init our sockets (note, these are low-level Socket Devices, which
    //  are thread safe)
    //
    
    if( !initServerSocket())
    {
        fatal("metadata server could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }
}


void MetadataServer::handleIncoming(HttpInRequest *http_request, int client_id)
{
    //
    //  The MfdHttpPlugin class ensures that this function gets called in
    //  it's own thread (from a dynamic pool). So we have to be very careful
    //  about anything that gets called from here!
    //
    
    
    //
    //  Create an MdcapRequest object that will be built up to understand the
    //  request that has just come in from a client
    //

    MdcapRequest *mdcap_request = new MdcapRequest();

    //
    //  Start to build up the request by parsing the path of the request.
    //
    
    mdcap_request->parsePath(http_request);
    
    //
    //  Make sure we understand the path
    //

    if(mdcap_request->getRequestType() == MDCAP_REQUEST_NOREQUEST)
    {
        warning("got a request that had no path/url");
        delete mdcap_request;
        mdcap_request = NULL;
        return;
    }


    //
    //  Log what's happening
    //

    log(QString("an mdcap client (%1) asked for \"%2\"")
                .arg(client_id)
                .arg(http_request->getUrl()), 8);
    
    //
    //  Send back server info if that's what they asked for
    //
    
    if(mdcap_request->getRequestType() == MDCAP_REQUEST_SERVINFO )
    {
        sendServerInfo(http_request);
        delete mdcap_request;
        mdcap_request = NULL;
        return;
    }   

    else if(mdcap_request->getRequestType() == MDCAP_REQUEST_LOGIN)
    {
        uint32_t session_id = mdcap_sessions.getNewId();
        sendLogin( http_request, session_id);
        delete mdcap_request;
        mdcap_request = NULL;
        return;
    }

    //
    //  If we've made it this far, then we have a logged in client, so we
    //  should be able to parse the request to get the session ID
    //

    bool ok = false;
    uint32_t session_id = http_request->getVariable("session-id").toULong(&ok);
    if(!ok || !mdcap_sessions.isValid(session_id))
    {
        warning("mdcap request did not contain a valid session id");
        delete mdcap_request;
        mdcap_request = NULL;
        return;
        
    }

    //
    //  Ok, client is valid login (good session id), so they probably want some metadata, etc.
    //
    
    if(mdcap_request->getRequestType() == MDCAP_REQUEST_UPDATE)
    {
        possiblySendUpdate(http_request, client_id);
    }
    else if(mdcap_request->getRequestType() == MDCAP_REQUEST_CONTAINERS)
    {
        sendContainers(http_request, mdcap_request);
    }
    else
    {
        warning("unhandled request type");
    }
    
    delete mdcap_request;
    mdcap_request = NULL;
}

void MetadataServer::lockMetadata()
{
    metadata_mutex.lock();
}

void MetadataServer::unlockMetadata()
{
    metadata_mutex.unlock();
}

uint MetadataServer::getMetadataLocalAudioGeneration()
{
    uint return_value;
    metadata_local_audio_generation_mutex.lock();
        return_value = metadata_local_audio_generation;
    metadata_local_audio_generation_mutex.unlock();
    return return_value;
}

uint MetadataServer::getMetadataContainerCount()
{
    uint return_value = 0;
    metadata_container_count_mutex.lock();
        return_value = metadata_container_count;
    metadata_container_count_mutex.unlock();
    return return_value;
}

uint MetadataServer::getAllLocalAudioMetadataCount()
{
    uint return_value = 0;
    local_audio_metadata_count_mutex.lock();
        if(local_audio_metadata_count > -1)
        {
            return_value = (uint) local_audio_metadata_count;
        }
    local_audio_metadata_count_mutex.unlock();
    return return_value;
}

uint MetadataServer::getAllLocalAudioPlaylistCount()
{
    uint return_value = 0;
    local_audio_playlist_count_mutex.lock();
        if(local_audio_playlist_count > -1)
        {
            return_value = (uint) local_audio_playlist_count;
        }
    local_audio_playlist_count_mutex.unlock();
    return return_value;
}

Metadata* MetadataServer::getMetadataByUniversalId(uint universal_id)
{
    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getMetadataByUniversalId() called without "
                "metadata_mutex being locked");
    }

    if(universal_id <= METADATA_UNIVERSAL_ID_DIVIDER)
    {
        warning("something asked for metadata with "
                "universal id that is too small");
        return NULL;
    }
    
    int collection_id = universal_id / METADATA_UNIVERSAL_ID_DIVIDER;
    int item_id = universal_id % METADATA_UNIVERSAL_ID_DIVIDER;
    
    //
    //  Find the collection
    //
    
    Metadata *return_value = NULL;
    
    MetadataContainer * a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == collection_id)
        {
            return_value = a_container->getMetadata(item_id);
            
            //
            //  if this container is not local, check and see if we can
            //  return something local instead
            //
            
            if(!a_container->isLocal())
            {
                QString myth_digest = return_value->getMythDigest();
                if(myth_digest.length() > 0)
                {
                    Metadata *alternative = local_mythdigest_dictionary->find(myth_digest);
                    if(alternative)
                    {
                        return_value = alternative;
                    }
                }
            }
            break; 
        }
    }

    
    return return_value;
}

Metadata* MetadataServer::getMetadataByContainerAndId(int container_id, int metadata_id)
{
    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getMetadataByContainerAndId() called without "
                "metadata_mutex being locked");
    }

    //
    //  Find the collection
    //
    
    Metadata *return_value = NULL;
    
    MetadataContainer * a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == container_id)
        {
            return_value = a_container->getMetadata(metadata_id);

            if(return_value)
            {
                //
                //  if this container is not local, check and see if we can
                //  return something local instead
                //
            
                if(!a_container->isLocal())
                {
                    QString myth_digest = return_value->getMythDigest();
                    if(myth_digest.length() > 0)
                    {
                        Metadata *alternative = local_mythdigest_dictionary->find(myth_digest);
                        if(alternative)
                        {
                            return_value = alternative;
                        }
                    }
                }
                break; 
            } 
        }
    }
    
    return return_value;
}

Playlist* MetadataServer::getPlaylistByUniversalId(uint universal_id)
{

    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getPlaylistByUniversalId() called without "
                "metadata_mutex being locked");
    }


    if(universal_id <= METADATA_UNIVERSAL_ID_DIVIDER)
    {
        warning("something asked for playlist with "
                "universal id that is too small");
        return NULL;
    }
    

    int collection_id = universal_id / METADATA_UNIVERSAL_ID_DIVIDER;
    int playlist_id = universal_id % METADATA_UNIVERSAL_ID_DIVIDER;
    
    //
    //  Find the collection
    //
    
    Playlist *return_value = NULL;
    
    MetadataContainer * a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == collection_id)
        {
            return_value = a_container->getPlaylist(playlist_id);
            break; 
        }
    }
    return return_value;
}

Playlist* MetadataServer::getPlaylistByContainerAndId(int container_id, int playlist_id)
{
    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getPlaylistByContainerAndId() called without "
                "metadata_mutex being locked");
    }

    //
    //  Find the collection
    //
    
    Playlist *return_value = NULL;
    
    MetadataContainer * a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == container_id)
        {
            return_value = a_container->getPlaylist(playlist_id);
            break; 
        }
    }
    
    return return_value;
}



MetadataContainer* MetadataServer::createContainer(
                                                    MetadataCollectionContentType content_type,
                                                    MetadataCollectionLocationType location_type
                                                  )
{
    MetadataContainer *return_value;
    lockMetadata();
        return_value = new MetadataContainer(parent, bumpContainerId(), content_type, location_type);
        metadata_containers->append(return_value);
        
        //
        //  The metadata_containers pointer list (above) owns this object,
        //  and will delete them if told to or when it exits. The following
        //  container(s) are just shortcuts that make it easier to find
        //  stuff.
        //
        
        //
        //  NB: at some point, we will add mutex's around these ptr list
        //  containers, so, for example, video metadata can get accessed
        //  independently of audio metadata ...
        //

        if(content_type == MCCT_audio && location_type == MCLT_host)
        {
            local_audio_metadata_containers->append(return_value);
        }
    metadata_container_count_mutex.lock();
        ++metadata_container_count;
    metadata_container_count_mutex.unlock();
        
    unlockMetadata();
    
    return return_value;
}                                                                                                                                                        

void MetadataServer::deleteContainer(int container_id)
{
    //
    //  A collection of metadata is going away
    //
    
    lockMetadata();

    MetadataContainer *target = NULL;
    MetadataContainer *a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == container_id)
        {
            target = a_container;
            break; 
        }
    }
    
    if(target)
    {
        //
        //  Remember what kind it was
        //
        
        MetadataCollectionContentType ex_type = target->getContentType();
        MetadataCollectionLocationType ex_location = target->getLocationType();
        
        //
        //  Adjust counts
        //
        
        local_audio_metadata_count_mutex.lock();
        local_audio_playlist_count_mutex.lock();
            
        if(target->isLocal() && target->isAudio())
        {
            local_audio_metadata_count = local_audio_metadata_count 
                                       - target->getMetadataCount();
            local_audio_playlist_count = local_audio_playlist_count 
                                       - target->getPlaylistCount();
        }
        
        local_audio_playlist_count_mutex.unlock();
        local_audio_metadata_count_mutex.unlock();
        
        //
        //  Remove other references to this container
        //
        
        if(ex_type == MCCT_audio && ex_location == MCLT_host)
        {
            local_audio_metadata_containers->remove(target);
        }
        

        //
        //  Build a set of deltas (all negatives, of course)
        //

        last_destroyed_mutex.lock();

        last_destroyed_container = target->getIdentifier();

        last_destroyed_metadata.clear();
        QIntDict<Metadata> *last_metadata = target->getMetadata();
        if(last_metadata)
        {
            QIntDictIterator<Metadata> iter(*last_metadata);
            for (; iter.current(); ++iter)
            {
                int an_integer = iter.currentKey();
                last_destroyed_metadata.push_back(an_integer);
                if(ex_type == MCCT_audio && ex_location == MCLT_host)
                {
                    //
                    //  Take off the mythdigest dictionary
                    //
                    
                    QString myth_digest = iter.current()->getMythDigest();
                    if(myth_digest.length() > 0)
                    {
                        if(!local_mythdigest_dictionary->remove(myth_digest))
                        {
                            warning("digest for item being destroyed was not "
                                    "in the dictionary");
                        }
                    }
                }
            }
        }
            
        last_destroyed_playlists.clear();
        QIntDict<Playlist> *last_playlists = target->getPlaylists();
        if(last_playlists)
        {
            QIntDictIterator<Playlist> oiter(*last_playlists);
            for (; oiter.current(); ++oiter)
            {
                int an_integer = oiter.currentKey();
                last_destroyed_playlists.push_back(an_integer);
            }
        }

        last_destroyed_mutex.unlock();
            
        //
        //  Take it off our master list, which automatically destructs it
        //
        
        metadata_containers->remove(target);
        
        //
        //  Log the destruction
        //

        log(QString("container %1 has been deleted")
            .arg(container_id), 4);

        //
        //  Finally we bump the relevant generation
        //
        
        if(ex_type == MCCT_audio && ex_location == MCLT_host)
        {
            metadata_local_audio_generation_mutex.lock();
                ++metadata_local_audio_generation;
            metadata_local_audio_generation_mutex.unlock();
        }

        MetadataChangeEvent *mce = new MetadataChangeEvent(last_destroyed_container);
        QApplication::postEvent(parent, mce);    
    }
    else
    {
        warning(QString("asked to delete container "
                        "with bad id: %1")
                       .arg(container_id));
    }
    unlockMetadata();
    
    dealWithHangingUpdates();
}

int MetadataServer::bumpContainerId()
{
    int return_value;

    container_identifier_mutex.lock();
        ++container_identifier;
        return_value = container_identifier;
    container_identifier_mutex.unlock();
    
    return return_value;
}

void MetadataServer::doAtomicDataSwap(
                                        MetadataContainer *which_one,
                                        QIntDict<Metadata>* new_metadata,
                                        QValueList<int> metadata_additions,
                                        QValueList<int> metadata_deletions,
                                        QIntDict<Playlist>* new_playlists,
                                        QValueList<int> playlist_additions,
                                        QValueList<int> playlist_deletions,
                                        bool rewrite_playlists
                                     )
{

    //
    //  Lock the metadata, find the right container, and swap out its data. 
    //  The idea is that a plugin can take as long as it wants to build a
    //  new metadata collection, but this call is fairly quick and all
    //  inside a mutex lock. Thus the name.
    //

    lockMetadata();

        MetadataContainer *target = NULL;
        MetadataContainer *a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container == which_one)
            {
                target = a_container;
                break; 
            }
        }
        
        if(target)
        {
            local_audio_metadata_count_mutex.lock();
                local_audio_playlist_count_mutex.lock();
            
                    if(target->isLocal() && target->isAudio())
                    {
                        local_audio_metadata_count = 
                                local_audio_metadata_count 
                                - target->getMetadataCount();
                        local_audio_playlist_count = 
                                local_audio_playlist_count 
                                - target->getPlaylistCount();

                        //
                        //  Update local mythdigest dictionary
                        //

                        updateDictionary(target, metadata_deletions, new_metadata);
                        
                    }
                    
                    
                    //
                    //  Actually swap the data
                    //
                    
                    target->dataSwap(
                                    new_metadata, 
                                    metadata_additions,
                                    metadata_deletions,
                                    new_playlists,
                                    playlist_additions,
                                    playlist_deletions,
                                    rewrite_playlists
                                    );
                           
                    new_metadata = NULL;
                    new_playlists = NULL;


                    log(QString("container %1 swapped in new data: "
                                "%2 items (+%3/-%4) and "
                                "%5 containers/playlists (+%6/-%7)")
                                .arg(target->getIdentifier())

                                .arg(target->getMetadataCount())
                                .arg(metadata_additions.count())
                                .arg(metadata_deletions.count())
                                
                                .arg(target->getPlaylistCount())
                                .arg(target->getPlaylistAdditions().count())
                                .arg(target->getPlaylistDeletions().count())
                                ,4);
                                
                    if(target->isLocal() && target->isAudio())
                    {
                        local_audio_metadata_count = 
                                local_audio_metadata_count 
                                + target->getMetadataCount();
                        local_audio_playlist_count = 
                                local_audio_playlist_count 
                                + target->getPlaylistCount();
                        metadata_local_audio_generation_mutex.lock();
                            ++metadata_local_audio_generation;
                        metadata_local_audio_generation_mutex.unlock();
                    }
                local_audio_playlist_count_mutex.unlock();
            local_audio_metadata_count_mutex.unlock();
        }
        else
        {
            warning("can not do a an AtomicDataSwap()"
                    " on a Container I don't own");
        }
        
    unlockMetadata();
    
    dealWithHangingUpdates();
}

void MetadataServer::doAtomicDataDelta(
                                        MetadataContainer *which_one,
                                        QIntDict<Metadata>* new_metadata,
                                        QValueList<int> metadata_additions,
                                        QValueList<int> metadata_deletions,
                                        QIntDict<Playlist>* new_playlists,
                                        QValueList<int> playlist_additions,
                                        QValueList<int> playlist_deletions,
                                        bool rewrite_playlists
                                     )
{


    //
    //  Lock the metadata, find the right container, and _delta_ its data. 
    //
    //  The "atomic" idea is the same as above, but here we're adding to
    //  whats already there (for metadata, playlists are still a swap out)
    //

    lockMetadata();

        MetadataContainer *target = NULL;
        MetadataContainer *a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container == which_one)
            {
                target = a_container;
                break; 
            }
        }
        
        if(target)
        {
            local_audio_metadata_count_mutex.lock();
                local_audio_playlist_count_mutex.lock();
            
                    //
                    //  Keep track of how many we have in total
                    //
                    
                    if(target->isLocal() && target->isAudio())
                    {
                        //
                        //  Keep track of how many we have in total
                        //

                        local_audio_metadata_count = 
                                local_audio_metadata_count 
                                - target->getMetadataCount();
                        local_audio_playlist_count = 
                                local_audio_playlist_count 
                                - target->getPlaylistCount();

                        //
                        //  Update local mythdigest dictionary
                        //

                        updateDictionary(target, metadata_deletions, new_metadata);

                    }

                    target->dataDelta(
                                    new_metadata, 
                                    metadata_additions,
                                    metadata_deletions,
                                    new_playlists,
                                    playlist_additions,
                                    playlist_deletions,
                                    rewrite_playlists
                                    );
                                   
                    new_metadata = NULL;
                    new_playlists = NULL;

                    log(QString("container %1 did delta of new data: "
                                "%2 items (+%3/-%4) and "
                                "%5 containers/playlists (+%6/-%7)")
                                .arg(target->getIdentifier())

                                .arg(target->getMetadataCount())
                                .arg(metadata_additions.count())
                                .arg(metadata_deletions.count())
                                
                                .arg(target->getPlaylistCount())
                                .arg(target->getPlaylistAdditions().count())
                                .arg(target->getPlaylistDeletions().count())
                                ,4);

                    if(target->isLocal() && target->isAudio())
                    {
                        local_audio_metadata_count = 
                                local_audio_metadata_count
                                + target->getMetadataCount();
                        local_audio_playlist_count = 
                                local_audio_playlist_count 
                                + target->getPlaylistCount();
                        metadata_local_audio_generation_mutex.lock();
                            ++metadata_local_audio_generation;
                        metadata_local_audio_generation_mutex.unlock();
                    }
                local_audio_playlist_count_mutex.unlock();
            local_audio_metadata_count_mutex.unlock();
        }
        else
        {
            warning("can not do a an AtomicDataDelta()"
                    " on a Container I don't own");
        }
        
    unlockMetadata();

    dealWithHangingUpdates();
    
}

MetadataContainer* MetadataServer::getMetadataContainer(int which_one)
{
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getMetadataContainer() called without "
                "metadata_mutex being locked");
    }

    MetadataContainer *target = NULL;
    MetadataContainer *a_container;
    for (
            a_container = metadata_containers->first(); 
            a_container; 
            a_container = metadata_containers->next()
        )
    {
        if(a_container->getIdentifier() == which_one)
        {
            target = a_container;
            break; 
        }
    }
    return target;        
}

int MetadataServer::getLastDestroyedCollection()
{
    int return_value;
    last_destroyed_mutex.lock();
        return_value = last_destroyed_container;
    last_destroyed_mutex.unlock();
    return return_value;
}

void MetadataServer::updateDictionary(
                                        MetadataContainer *target, 
                                        QValueList<int> metadata_deletions, 
                                        QIntDict<Metadata>* new_metadata
                                     )
{
    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("updateDictionary() called without "
                "metadata_mutex being locked");
    }


    if(!target)
    {
        warning("updateDictionary() called with bad reference to "
                "target container");
        return;
    }


    //
    //  Make sure anything being removed comes off our
    //  myth digest dictionary
    //
                        
    QValueList<int>::iterator d_iter;
    for(
        d_iter = metadata_deletions.begin(); 
        d_iter != metadata_deletions.end(); 
        ++d_iter 
       )
    {
        Metadata *d_metadata = target->getMetadata((*d_iter));
        if(d_metadata)
        {
            QString myth_digest =  d_metadata->getMythDigest();
            if(myth_digest.length() > 0)
            {
                if(!local_mythdigest_dictionary->remove(myth_digest))
                {
                    warning("digest for item to be removed was not "
                            "in the dictionary");
                }
            }
        }
        else
        {
            warning("could not find metadata from "
                    "integer on deletion list");
        }
    }
                        
    //
    //  While we're dealing with the digest dictionary,
    //  put all the new metadata in
    //
                        
    QIntDictIterator<Metadata> a_iter(*new_metadata);
    for (; a_iter.current(); ++a_iter)
    {
        QString myth_digest = a_iter.current()->getMythDigest();
        if(myth_digest.length() > 0)
        {
            local_mythdigest_dictionary->insert(myth_digest, a_iter.current());
        }
    }
}

Metadata* MetadataServer::getLocalEquivalent(Metadata *which_item)
{
    //
    //  Metadata should be locked _before_ calling this method
    //
    
    if(metadata_mutex.tryLock())
    {
        metadata_mutex.unlock();
        warning("getLocalEquivalent() called without "
                "metadata_mutex being locked");
    }

    Metadata* return_value = NULL;
    
    QString myth_digest = which_item->getMythDigest();
    if(myth_digest.length() > 0)
    {
        return_value = local_mythdigest_dictionary->find(myth_digest);
    }
    return return_value;
}


void MetadataServer::sendResponse(HttpInRequest *http_request, MdcapOutput &response)
{

    //
    //  Make sure we don't have any open content code groups in what we're sending
    //
    
    if(response.openGroups())
    {
        warning("asked to send mdcap response, but there are open groups");
        http_request->getResponse()->setError(500);
        return;
    }

    //
    //  Set some header stuff 
    //
    
    http_request->getResponse()->addHeader(
                        QString("MDCAP-Server: MythTV/%1.%1 (Probably Linux)")
                            .arg(MDCAP_PROTOCOL_VERSION_MAJOR)
                            .arg(MDCAP_PROTOCOL_VERSION_MINOR)
                                          );
                        
    http_request->getResponse()->addHeader(
                        "Content-Type: application/x-mdcap-tagged"
                                          );
    
    //
    //  Set the payload
    //
    
    http_request->getResponse()->setPayload(response.getContents());
}

void MetadataServer::sendServerInfo(HttpInRequest *http_request)
{

    MdcapOutput response;

    response.addServerInfoGroup();
       response.addStatus(200); // like http ok!
       response.addServiceName(
                                QString("metadata server on %1")
                                        .arg(mfdContext->getHostName())
                              );
       response.addProtocolVersion();
    response.endGroup();

    sendResponse(http_request, response);
}

void MetadataServer::sendLogin(HttpInRequest *http_request, uint32_t session_id)
{
    MdcapOutput response;
    
    response.addLoginGroup();
        response.addStatus(200);
        response.addSessionId(session_id);
    response.endGroup();

    sendResponse(http_request, response);
}

void MetadataServer::possiblySendUpdate(HttpInRequest *http_request, int client_id)
{
    lockMetadata();

    //
    //  If the client is not up to date with version numbers, send an update
    //
    
    bool send_an_update = false;
    
    QString version_string = http_request->getHeader("MDCAP-Generations");
    if(version_string.length() > 0)
    {
        //
        //  Client sent container versions
        //

        QMap<int, int> client_generations;
        QStringList generation_pairs = QStringList::split(",", version_string);
        for(
            QStringList::Iterator sl_it = generation_pairs.begin(); 
            sl_it != generation_pairs.end(); 
            ++sl_it 
           )
        {
            bool a_ok, b_ok;
            int container_number  = (*sl_it).section("/", 0, 0).toInt(&a_ok);
            int generation_number = (*sl_it).section("/", 1, 1).toInt(&b_ok); 
            if(a_ok && b_ok)
            {
                client_generations.insert(container_number, generation_number);
                
                //
                //  If the client has a reference to container that does not
                //  (ie. no longer) exists, that's a problem ... they need an update
                //
                
                bool bad_reference = true;
                QPtrListIterator<MetadataContainer> it( *metadata_containers );
                MetadataContainer *container;
                while ( (container = it.current()) != 0 ) 
                {
                    ++it;
                    if(container->getIdentifier() == container_number)
                    {
                        bad_reference = false;
                    }                
                }        
                if(bad_reference)
                {
                    send_an_update = true;
                }
                
            }
            else
            {
                warning("error parsing mdcap client generations");
            }
        }

        if(!send_an_update)
        {
            QPtrListIterator<MetadataContainer> it( *metadata_containers );
            MetadataContainer *container;
            while ( (container = it.current()) != 0 ) 
            {
                ++it;
                int what_generation = container->getGeneration();
                if(what_generation != client_generations[container->getIdentifier()])
                {
                    send_an_update = true;
                }
                    
            }    
        }        
    }
    else
    {
        send_an_update = true;
    }
    
    
    if(send_an_update)
    {

        //
        //  We do need to send an update. Send a list of metadata collections with their current version numbers
        //
    
        MdcapOutput response;
        
        buildUpdateResponse(&response);
    
    unlockMetadata();
    sendResponse(http_request, response);

    }
    else
    {
        
        //
        //  Put this client in our hanging updates list
        //
        
        http_request->sendResponse(false);
        unlockMetadata();

        hanging_updates_mutex.lock();
            hanging_updates.append(client_id);
            log(QString("now has %1 client(s) sitting in a hanging mdcap update")
                        .arg(hanging_updates.count()), 8);
        hanging_updates_mutex.unlock();
    }
}

void MetadataServer::sendContainers(HttpInRequest *http_request, MdcapRequest *mdcap_request)
{
    //
    //  This method is called whenever we get an http request for
    //  "/containers". We need to figure out whether it's a request for
    //  items or lists, parse out the container id, and figure out of it's a
    //  full or delta request
    //
    

    bool full_update = true;
    
    metadata_mutex.lock();
    MetadataContainer *container = getMetadataContainer(mdcap_request->getContainerId());
    if(!container)
    {
        warning("bad container id in mdcap request");
        http_request->getResponse()->setError(400);
        metadata_mutex.unlock();
        return;
    }

    if(
        mdcap_request->getDelta() > 0      &&
        mdcap_request->getGeneration() > 0 &&
        mdcap_request->getDelta() + 1 == mdcap_request->getGeneration() &&
        mdcap_request->getGeneration() == container->getGeneration()
       )
    {
        full_update = false;    // delta
    }  
        

    if(mdcap_request->itemRequest())
    {
        //
        //  Are we sending everything, or just deltas?
        //  We send deltas if the client is only off by one.
        //
        
        if(full_update)
        {
            //
            //  Send all the metadata items in this container
            //
        
            QIntDict<Metadata> *metadata = container->getMetadata();
            
            MdcapOutput response;
        
            response.addItemGroup();

                response.addCollectionId(container->getIdentifier());
                response.addCollectionGeneration(container->getGeneration());
                response.addUpdateType(full_update);
                response.addTotalItems(metadata->count());
                response.addAddedItems(metadata->count());
                response.addDeletedItems(0);
                
                response.addAddedItemsGroup();
                    
                    QIntDictIterator<Metadata> it( *metadata ); 
                    Metadata *a_metadata;
                    for ( ; (a_metadata = it.current()); ++it )   
                    {
                        if(a_metadata->getType() == MDT_audio)
                        {
                            AudioMetadata *audio_metadata = (AudioMetadata *)a_metadata;
                            response.addAddedItemGroup();
                                response.addItemType(audio_metadata->getType());
                                response.addItemId(audio_metadata->getId());
                                response.addItemUrl(audio_metadata->getUrl().toString());
                                response.addItemRating(audio_metadata->getRating());
                                response.addItemLastPlayed(audio_metadata->getLastPlayed().toTime_t());
                                response.addItemPlayCount(audio_metadata->getPlayCount());
                                response.addItemArtist(audio_metadata->getArtist());
                                response.addItemAlbum(audio_metadata->getAlbum());
                                response.addItemTitle(audio_metadata->getTitle());
                                response.addItemGenre(audio_metadata->getGenre());
                                response.addItemYear(audio_metadata->getYear());
                                response.addItemTrack(audio_metadata->getTrack());
                                response.addItemLength(audio_metadata->getLength());
                            response.endGroup();
                        }
                        else
                        {
                            warning("don't know how to handle non audio metadata yet");
                        }
                    }
                
                response.endGroup();

            response.endGroup();
    
            sendResponse(http_request, response);
        }
        else
        {
            //
            //  Send the deltas from the last change for items in this
            //  container
            //
        
            QIntDict<Metadata> *metadata = container->getMetadata();
            QValueList<int> metadata_additions = container->getMetadataAdditions();
            QValueList<int> metadata_deletions = container->getMetadataDeletions();
            
            MdcapOutput response;
        
            response.addItemGroup();

                response.addCollectionId(container->getIdentifier());
                response.addCollectionGeneration(container->getGeneration());
                response.addUpdateType(full_update);    // false
                response.addTotalItems(metadata->count());
                response.addAddedItems(metadata_additions.count());
                response.addDeletedItems(metadata_deletions.count());
                
                if(metadata_additions.count() > 0)
                {
                    response.addAddedItemsGroup();
                        QValueList<int>::iterator it;
                        for ( it = metadata_additions.begin(); it != metadata_additions.end(); ++it )
                        {
                            Metadata *a_metadata = metadata->find((*it));
                            if(a_metadata)
                            {
                                if(a_metadata->getType() == MDT_audio)
                                {
                                    AudioMetadata *audio_metadata = (AudioMetadata *)a_metadata;
                                    response.addAddedItemGroup();
                                        response.addItemType(audio_metadata->getType());
                                        response.addItemId(audio_metadata->getId());
                                        response.addItemUrl(audio_metadata->getUrl().toString());
                                        response.addItemRating(audio_metadata->getRating());
                                        response.addItemLastPlayed(audio_metadata->getLastPlayed().toTime_t());
                                        response.addItemPlayCount(audio_metadata->getPlayCount());
                                        response.addItemArtist(audio_metadata->getArtist());
                                        response.addItemAlbum(audio_metadata->getAlbum());
                                        response.addItemTitle(audio_metadata->getTitle());
                                        response.addItemGenre(audio_metadata->getGenre());
                                        response.addItemYear(audio_metadata->getYear());
                                        response.addItemTrack(audio_metadata->getTrack());
                                        response.addItemLength(audio_metadata->getLength());
                                    response.endGroup();
                                }
                                else
                                {
                                    warning("don't know how to handle non audio metadata yet");
                                }
                            }
                            else
                            {
                                warning("metadata addition that doesn't point to a metadata!!");
                            }
                        }
                    response.endGroup();
                }
                
                if(metadata_deletions.count() > 0)
                {
                    response.addDeletedItemsGroup();
                        QValueList<int>::iterator it;
                        for ( it = metadata_deletions.begin(); it != metadata_deletions.end(); ++it )
                        {
                            response.addDeletedItem((*it));
                        }
                    response.endGroup();
                }
                

            response.endGroup();
    
            sendResponse(http_request, response);
        }
        
    }
    else if(mdcap_request->listRequest())
    {
        if(full_update)
        {
            //
            //  Send all the lists in this container
            //
        
            QIntDict<Playlist> *playlists = container->getPlaylists();
            
            MdcapOutput response;
        
            response.addListGroup();

                response.addCollectionId(container->getIdentifier());
                response.addCollectionGeneration(container->getGeneration());
                response.addUpdateType(full_update);
                response.addTotalItems(playlists->count());
                response.addAddedItems(playlists->count());
                response.addDeletedItems(0);
                
                response.addAddedListsGroup();
                    
                    QIntDictIterator<Playlist> it( *playlists ); 
                    Playlist *a_playlist;
                    for ( ; (a_playlist = it.current()); ++it )   
                    {
                        response.addAddedListGroup();
                            response.addListId(a_playlist->getId());
                            response.addListName(a_playlist->getName());
                            QValueList<int> *list_items = a_playlist->getListPtr();
                            QValueList<int>::iterator item_it;
                            for(item_it = list_items->begin(); item_it != list_items->end(); ++item_it)
                            {
                                response.addListItem((*item_it));
                            }
                        response.endGroup();
                    }
                
                response.endGroup();

            response.endGroup();
    
            sendResponse(http_request, response);
        }
        else
        {
            //
            //  Send the deltas from the last change for items in this
            //  container
            //

            QIntDict<Playlist> *playlists = container->getPlaylists();
            QValueList<int> playlist_additions = container->getPlaylistAdditions();
            QValueList<int> playlist_deletions = container->getPlaylistDeletions();
            
            MdcapOutput response;
        
            response.addListGroup();

                response.addCollectionId(container->getIdentifier());
                response.addCollectionGeneration(container->getGeneration());
                response.addUpdateType(full_update);
                response.addTotalItems(playlists->count());
                response.addAddedItems(playlist_additions.count());
                response.addDeletedItems(playlist_deletions.count());
                
                if(playlist_additions.count() > 0)
                {
                    response.addAddedListsGroup();
                        QValueList<int>::iterator it;
                        for ( it = playlist_additions.begin(); it != playlist_additions.end(); ++it )
                        {
                            Playlist *a_playlist = playlists->find((*it));
                            if(a_playlist)
                            {
                                response.addAddedListGroup();
                                    response.addListId(a_playlist->getId());
                                    response.addListName(a_playlist->getName());
                                    QValueList<int> *list_items = a_playlist->getListPtr();
                                    QValueList<int>::iterator item_it;
                                    for(item_it = list_items->begin(); item_it != list_items->end(); ++item_it)
                                    {
                                        response.addListItem((*item_it));
                                    }
                                response.endGroup();
                            }
                            else
                            {
                                warning("playlist addition entry that didn't point to anything");
                            }
                        }
                    response.endGroup();
                }
                if(playlist_deletions.count() > 0)
                {
                    response.addDeletedListsGroup();
                        QValueList<int>::iterator it;
                        for ( it = playlist_deletions.begin(); it != playlist_deletions.end(); ++it )
                        {
                            response.addDeletedList((*it));
                        }
                    response.endGroup();
                }

            response.endGroup();
    
            sendResponse(http_request, response);
        }
        
    }
    else
    {
        warning("bad request type from mdcap client");
        http_request->getResponse()->setError(400);
    }    

    metadata_mutex.unlock();
    
}

void MetadataServer::buildUpdateResponse(MdcapOutput *response)
{
    //
    //  The metadata should already be locked (!)
    //

    response->addUpdateGroup();

    //
    //  How many collections in total
    //

    response->addCollectionCount(metadata_containers->count());

    //
    //  For each, add information about metadata containers
    //

    QPtrListIterator<MetadataContainer> it( *metadata_containers );
    MetadataContainer *container;
    while ( (container = it.current()) != 0 ) 
    {
        ++it;
        response->addCollectionGroup();
            response->addCollectionId(container->getIdentifier());
            if(container->isAudio())
            {
                response->addCollectionType(MDT_audio);
            }
            else if(container->isVideo())
            {
                response->addCollectionType(MDT_video);
            }
            else
            {
                warning("collection that is neither audio not video");
            }
            response->addCollectionCount(container->getMetadataCount());
            response->addCollectionName(container->getName());
            response->addCollectionGeneration(container->getGeneration());
        response->endGroup();
    }
    response->endGroup();
}

void MetadataServer::dealWithHangingUpdates()
{
    //
    //  Make an /update response that reflects current metadata
    //

    MdcapOutput response;
    lockMetadata();
        buildUpdateResponse(&response);
    unlockMetadata();

    //
    //  Put the mdcap response into a properly constructed Http out response
    //
    
    
    HttpOutResponse *hu_response = new HttpOutResponse(this, NULL);
    hu_response->addHeader(
                            QString("MDCAP-Server: MythTV/%1.%1 (Probably Linux)")
                                .arg(MDCAP_PROTOCOL_VERSION_MAJOR)
                                .arg(MDCAP_PROTOCOL_VERSION_MINOR)
                          );
                        
    hu_response->addHeader(
                            "Content-Type: application/x-mdcap-tagged"
                          );

    hu_response->setPayload(response.getContents());

    //
    //  Send it out, and remove all the hanging updates
    //
        
    hanging_updates_mutex.lock();

        QValueList<int>::iterator it;
        for ( it = hanging_updates.begin(); it != hanging_updates.end(); ++it )
        {
            MFDHttpPlugin::sendResponse((*it), hu_response);
        }
        hanging_updates.clear();

    hanging_updates_mutex.unlock();
    
    delete hu_response;
}


MetadataServer::~MetadataServer()
{
    if(metadata_containers)
    {
        delete metadata_containers;
    }
    
    if(local_mythdigest_dictionary)
    {
        delete local_mythdigest_dictionary;
    }
}
