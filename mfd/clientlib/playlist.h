#ifndef PLAYLIST_H_
#define PLAYLIST_H_
/*
	playlist.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	a list of things to play

*/

#include <qstring.h>
#include <qvaluelist.h>

#include "playlistentry.h"




class ClientPlaylist
{
  public:
    
    typedef QValueList<PlaylistEntry> PlaylistEntryList;
      
    ClientPlaylist(
                int l_collection_id, 
                QString new_name, 
                PlaylistEntryList *my_entries, 
                uint my_id
            );

    virtual ~ClientPlaylist();

    uint                       getId(){return id;}
    uint                       getCollectionId(){return collection_id;}    
    QString                    getName(){return name;}
    uint                       getCount(){return entries.count();}
    void                       setActualTrackCount(int x){ actual_track_count = x;}
    uint                       getActualTrackCount(){ return actual_track_count; }
    QValueList<PlaylistEntry>* getListPtr(){return &entries;}
    bool                       containsItem(int item_id);
    bool                       isEditable(){return is_editable;}
    void                       isEditable(bool y_or_n){ is_editable = y_or_n; }

  private:
  
    QString           name;
    PlaylistEntryList entries;
    uint              id;
    uint              collection_id;
    uint              actual_track_count;
    bool              is_editable;
};

#endif
