#ifndef MFDCONTENT_H_
#define MFDCONTENT_H_
/*
	mfdcontent.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	thing that holds all content, playlists, tree's etc., and gets handed
	over to client code

*/

#include <map>
using namespace std;

#include <qintdict.h>

class Metadata;
class AudioMetadata;
class ClientPlaylist;
class GenericTree;
class UIListGenericTree;
class UIListTreeType;
class MetadataCollection;
class PlaylistChecker;

class MfdContentCollection
{
  public:
  
     MfdContentCollection(int an_id, int client_screen_width = 800, int client_screen_height = 600);
    ~MfdContentCollection();

    void addMetadata(Metadata *an_item, const QString &collection_name, MetadataCollection *collection);
    void addPlaylist(ClientPlaylist *a_playlist, MetadataCollection *collection);
    void addNewPlaylistAbility(const QString &collection_name);
    void recursivelyAddSubPlaylist(
                                    UIListGenericTree *where_to_add, 
                                    MetadataCollection *collection, 
                                    int playlist_id, 
                                    int spot_counter
                                  );
    void addItemToAudioArtistTree(AudioMetadata *item, GenericTree *starting_point, bool do_checks = false, bool do_map =false);
    void addItemToAudioGenreTree(AudioMetadata *item, GenericTree *starting_point, bool do_checks = false, bool do_map = false);
    void addItemToAudioCollectionTree(AudioMetadata *item, const QString &collection_name);
    void addItemToSelectableTrees(AudioMetadata *item);
    void addPlaylistToSelectableTrees(ClientPlaylist *playlist);
    void tallyPlaylists();
    uint countPlaylistTracks(ClientPlaylist *playlist, uint counter);

    UIListGenericTree* getAudioArtistTree(){     return audio_artist_tree;     }
    UIListGenericTree* getAudioGenreTree(){      return audio_genre_tree;      }
    UIListGenericTree* getAudioPlaylistTree(){   return audio_playlist_tree;   }
    UIListGenericTree* getAudioCollectionTree(){ return audio_collection_tree; }
    UIListGenericTree* getNewPlaylistTree(){     return new_playlist_tree;     }
    UIListGenericTree* getEditablePlaylistTree(){return editable_playlist_tree;}
    
    AudioMetadata*     getAudioItem(int which_collection, int which_id);
    ClientPlaylist*    getAudioPlaylist(int which_collection, int which_id);
    UIListGenericTree* getPlaylistTree(int which_collection, int which_playlist, bool pristine=false);
    UIListGenericTree* constructPlaylistTree(ClientPlaylist *playlist);
    UIListGenericTree* getContentTree(int which_collection, bool pristine=false);
    void               toggleItem(UIListGenericTree *node, bool turn_on);
    void               toggleTree(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on);
    void               alterPlaylist(UIListTreeType *menu, UIListGenericTree *playlist_tree, UIListGenericTree *node, bool turn_on);
    void               checkParent(UIListGenericTree *node);
    bool               crossReferenceExists(ClientPlaylist *subject, ClientPlaylist *object, int depth);
    void               markNodeAsHeld(UIListGenericTree *node, bool held_or_not);
    void               turnOffTree(PlaylistChecker *playlist_checker, UIListGenericTree *content_tree);
    void               processContentTree(PlaylistChecker *playlist_checker, UIListGenericTree *playlist_tree, UIListGenericTree *content_tree);
    void               printTree(UIListGenericTree *node, int depth=0);  //  for debugging

    void sort();
    void setupPixmaps();
    
  private:

    int collection_id;

    QIntDict<AudioMetadata>  audio_item_dictionary;
    QIntDict<ClientPlaylist> audio_playlist_dictionary;

    UIListGenericTree *audio_artist_tree;
    UIListGenericTree *audio_genre_tree;
    UIListGenericTree *audio_playlist_tree;
    UIListGenericTree *audio_collection_tree;
    UIListGenericTree *new_playlist_tree;
    UIListGenericTree *editable_playlist_tree;


    //
    //  The following multimap allows a given playlist entry to find
    //  everything in a content tree that corresponds to that entry
    //

    typedef multimap<long, UIListGenericTree*> SelectableContentMap;
    SelectableContentMap selectable_content_map;


    QIntDict<UIListGenericTree> selectable_content_trees;
    QIntDict<UIListGenericTree> pristine_content_trees;

    QIntDict<UIListGenericTree> editable_playlist_trees;
    QIntDict<UIListGenericTree> pristine_playlist_trees;

    int   client_width;
    int   client_height;
    float client_wmult;
    float client_hmult;
};

#endif
