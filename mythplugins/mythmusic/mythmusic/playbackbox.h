#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#include <mythtv/volumecontrol.h>

#include "mainvisual.h"
#include "metadata.h"
#include "playlist.h"

class Output;
class Decoder;

class PlaybackBox : public MythThemedDialog
{
    Q_OBJECT

  public:

    //
    // Now featuring a themed constructor ... YAH!!!
    //
    typedef QValueVector<int> IntVector;
    
    PlaybackBox(MythMainWindow *parent, QString window_name,
                QString theme_filename, PlaylistsContainer *the_playlists,
                AllMusic *the_music, const char *name = 0);

    ~PlaybackBox(void);

    void closeEvent(QCloseEvent *);
    void customEvent(QCustomEvent *);
    void showEvent(QShowEvent *);
    void keyPressEvent(QKeyEvent *e);
    void constructPlaylistTree();

  public slots:

    void play();
    void pause();
    void stop();
    void stopDecoder();
    void previous();
    void next();
    void seekforward();
    void seekback();
    void seek(int);
    void stopAll();
    void setShuffleMode(unsigned int mode);
    void toggleShuffle();
    void increaseRating();
    void decreaseRating();
    void setRepeatMode(unsigned int mode);
    void toggleRepeat();
    void editPlaylist();
    void nextAuto();
    void checkForPlaylists();
    void handleTreeListSignals(int, IntVector*);
    void visEnable();
    void changeVolume(bool up_or_down);
    void toggleMute();
    void resetTimer();
    void hideVolume(){showVolume(false);}
    void showVolume(bool on_or_off);
    void wipeTrackInfo();
    
  signals:
  
    void dummy();   // debugging

  private:

    void wireUpTheme();
    
    void CycleVisualizer(void);

    QIODevice *input;
    Output *output;
    Decoder *decoder;

    QString playfile;
    QString statusString;

    enum RepeatMode
    { REPEAT_OFF = 0,
      REPEAT_TRACK, 
      REPEAT_ALL, 
      MAX_REPEAT_MODES 
    };
    enum ShuffleMode
    { SHUFFLE_OFF = 0, 
      SHUFFLE_RANDOM, 
      SHUFFLE_INTELLIGENT,
      MAX_SHUFFLE_MODES 
    };

    bool listAsShuffled;
    int outputBufferSize;
    int currentTime, maxTime;

    Metadata *curMeta;


    unsigned int shufflemode;
    unsigned int repeatmode;

    bool isplaying;

    MainVisual *mainvisual;

    QString visual_mode;
    int visual_mode_delay;
    QTimer *visual_mode_timer;
    QTimer *lcd_update_timer;
    int visualizer_status;

    bool showrating;
    bool vis_is_big;
    bool tree_is_done;
    bool first_playlist_check;
    
    AllMusic *all_music;
    PlaylistsContainer *all_playlists;

    QTimer  *waiting_for_playlists_timer;
    QTimer  *volume_display_timer;

    GenericTree *playlist_tree;

    bool cycle_visualizer;
    bool show_whole_tree;
    bool keyboard_accelerators;

    VolumeControl   *volume_control;

    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *music_tree_list;

    UITextType            *title_text;
    UITextType            *artist_text;
    UITextType            *album_text;
    UITextType            *time_text;
    UITextType            *info_text;
    UITextType            *current_visualization_text;
    
    UIRepeatedImageType   *ratings_image;
    UIBlackHoleType       *visual_blackhole;

    UIStatusBarType       *volume_status;

    UIPushButtonType      *prev_button;
    UIPushButtonType      *rew_button;
    UIPushButtonType      *pause_button;
    UIPushButtonType      *play_button;
    UIPushButtonType      *stop_button;
    UIPushButtonType      *ff_button;
    UIPushButtonType      *next_button;

    UITextButtonType      *shuffle_button;
    UITextButtonType      *repeat_button;
    UITextButtonType      *pledit_button;
    UITextButtonType      *vis_button;

};


#endif
