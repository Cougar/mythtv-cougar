#ifndef MDSERVER_H_
#define MDSERVER_H_
/*
	mdserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a threaded object that opens a server
	port and explains the state of the metadata to any
	client that asks

*/

#include <qdeepcopy.h>
#include <qvaluelist.h>

#include "../mfdlib/mfd_plugin.h"
#include "../mdcaplib/mdcapoutput.h"
#include "../mdcaplib/markupcodes.h"

#include "mdcapsession.h"

class MFD;
class MdcapRequest;

class MetadataServer : public MFDHttpPlugin
{
    //
    //  Don't be confused by the fact that this inherits from
    //  MFDHttpPlugin. It is not dynamically loaded, but is part of the
    //  mfd core (just so happens that MFDHttpPlugin has most of the bits
    //  and pieces we need for a metadata serving object already built in).
    //

  public:
  
    MetadataServer(MFD *owner, int port);
    ~MetadataServer();

    void    run();
    void    handleIncoming(HttpInRequest *http_request, int client_id);

    void                         lockMetadata();
    void                         unlockMetadata();

    QPtrList<MetadataContainer>* getMetadataContainers(){return metadata_containers;}
    QPtrList<MetadataContainer>* getLocalAudioMetadataContainers(){return local_audio_metadata_containers;}
    MetadataContainer*           getMetadataContainer(int which_one);
    uint                         getMetadataLocalAudioGeneration();
    uint                         getMetadataContainerCount();
    uint                         getAllLocalAudioMetadataCount();
    uint                         getAllLocalAudioPlaylistCount();

    Metadata*                    getMetadataByUniversalId(uint id);
    Metadata*                    getMetadataByContainerAndId(int container_id, int metadata_id);
    Metadata*                    getLocalEquivalent(Metadata *which_item);
    Playlist*                    getPlaylistByUniversalId(uint id);
    Playlist*                    getPlaylistByContainerAndId(int container_id, int playlist_id);
    


    MetadataContainer*           createContainer(
                                                    MetadataCollectionContentType content_type, 
                                                    MetadataCollectionLocationType location_type
                                                );

    void                         deleteContainer(int container_id);

    void                         doAtomicDataSwap(
                                                    MetadataContainer *which_one,
                                                    QIntDict<Metadata>* new_metadata,
                                                    QValueList<int> metadata_additions,
                                                    QValueList<int> metadata_deletions,
                                                    QIntDict<Playlist>* new_playlists,
                                                    QValueList<int> playlist_additions,
                                                    QValueList<int> playlist_deletions,
                                                    bool rewrite_playlists = false
                                                 );


    void                         doAtomicDataDelta(
                                                    MetadataContainer *which_one,
                                                    QIntDict<Metadata>* new_metadata,
                                                    QValueList<int> metadata_additions,
                                                    QValueList<int> metadata_deletions,
                                                    QIntDict<Playlist>* new_playlists,
                                                    QValueList<int> playlist_additions,
                                                    QValueList<int> playlist_deletions,
                                                    bool rewrite_playlists = false
                                                 );

    int                         getLastDestroyedCollection();
    QValueList<int>             getLastDestroyedMetadataList(){return last_destroyed_metadata;}
    QValueList<int>             getLastDestroyedPlaylistList(){return last_destroyed_playlists;}


    //
    //  mdcap related serving functions
    //
    
    void sendServerInfo(HttpInRequest *http_request);
    void sendLogin(HttpInRequest *http_request, uint32_t session_id);
    void possiblySendUpdate(HttpInRequest *http_request, int client_id);
    void sendContainers(HttpInRequest *http_request, MdcapRequest *mdcap_request);

    void sendResponse(HttpInRequest *http_request, MdcapOutput &response);

  private:

    void                        updateDictionary(
                                                    MetadataContainer *target, 
                                                    QValueList<int> metadata_deletions, 
                                                    QIntDict<Metadata>* new_metadata
                                                );

    int                         bumpContainerId();


    QPtrList<MetadataContainer> *metadata_containers;
    QPtrList<MetadataContainer> *local_audio_metadata_containers;
    QMutex                      metadata_mutex;

    int                         metadata_container_count;
    QMutex                      metadata_container_count_mutex;

    QMutex                      metadata_local_audio_generation_mutex;
    uint                        metadata_local_audio_generation;

    int                         container_identifier;
    QMutex                      container_identifier_mutex;


    int                         local_audio_metadata_count;
    QMutex                      local_audio_metadata_count_mutex;
    
    int                         local_audio_playlist_count;
    QMutex                      local_audio_playlist_count_mutex;

    QMutex                      last_destroyed_mutex;
    int                         last_destroyed_container;
    QValueList<int>             last_destroyed_metadata;
    QValueList<int>             last_destroyed_playlists;

    QDict<Metadata>             *local_mythdigest_dictionary;

    MdcapSessions               mdcap_sessions;
};


#endif
