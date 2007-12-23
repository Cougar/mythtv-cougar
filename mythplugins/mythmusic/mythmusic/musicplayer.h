#ifndef MUSICPLAYER_H_
#define MUSICPLAYER_H_

#include <iostream>

#include <mythtv/mythdialogs.h>
#include <mythtv/audiooutput.h>

#include "metadata.h"

class Decoder;
class AudioOutput;
class MainVisual;

class MusicPlayer : public QObject
{
  //Q_OBJECT

  public:

     MusicPlayer(QObject *parent, const QString &dev);
    ~MusicPlayer(void);

    void playFile(const QString &filename);
    void playFile(const Metadata &meta);

    void setListener(QObject *listener);
    void setVisual(MainVisual *visual);
    void setCDDevice(const QString &dev) { m_CDdevice = dev; }

    void mute(void) {};
    void unMute(void) {};
    void setVolume(void) {};

    void play(void);
    void stop(bool stopAll = false);
    void pause(void);
    void next(void);
    void previous(void);

    void nextAuto(void);

    bool isPlaying(void) { return m_isPlaying; }
    bool hasClient(void) { return (m_listener != NULL); }

    void canShowPlayer(bool canShow) { m_canShowPlayer = canShow; }
    bool getCanShowPlayer(void) { return m_canShowPlayer; }

    Decoder     *getDecoder(void) { return m_decoder; }
    AudioOutput *getOutput(void) { return m_output; }

    GenericTree *constructPlaylist(void);
    GenericTree *getPlaylistTree() { return m_playlistTree; }
    void         setCurrentNode(GenericTree *node) { m_currentNode = node; }
    GenericTree *getCurrentNode(void) { return m_currentNode; }

    QString      getRouteToCurrent(void);

    void         savePosition(void);
    void         restorePosition(const QString &position);
    void         seek(int pos);

    Metadata    *getCurrentMetadata(void);
    void         refreshMetadata(void);

    void showMiniPlayer(void);

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
      SHUFFLE_ALBUM,
      SHUFFLE_ARTIST,
      MAX_SHUFFLE_MODES 
    };

    enum ResumeMode
    { RESUME_OFF,
      RESUME_TRACK,
      RESUME_EXACT,
      MAX_RESUME_MODES
    };

    RepeatMode getRepeatMode(void) { return m_repeatMode; }
    void       setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }
    RepeatMode toggleRepeatMode(void);

    ShuffleMode getShuffleMode(void) { return m_shuffleMode; }
    void        setShuffleMode(ShuffleMode mode) { m_shuffleMode = mode; }
    ShuffleMode toggleShuffleMode(void);

    ResumeMode  getResumeMode(void) { return m_resumeMode; }

  protected:
    void customEvent(QCustomEvent *event);

  private:
    void stopDecoder(void);
    void openOutputDevice(void);
    QString getFilenameFromID(int id);
    void updateLastplay(void);

    GenericTree *m_playlistTree;

    GenericTree *m_currentNode;
    Metadata    *m_currentMetadata;
    QString      m_currentFile;
    int          m_currentTime;

    QIODevice   *m_input;
    AudioOutput *m_output;
    Decoder     *m_decoder;

    QObject     *m_listener;
    MainVisual  *m_visual;

    QString      m_CDdevice;

    bool         m_isPlaying;
    bool         m_isAutoplay;
    bool         m_canShowPlayer;
    bool         m_wasPlaying;
    bool         m_updatedLastplay;

    int          m_lastplayDelay;

    ShuffleMode  m_shuffleMode;
    RepeatMode   m_repeatMode;
    ResumeMode   m_resumeMode;
};

// This global variable contains the MusicPlayer instance for the application
extern MPUBLIC MusicPlayer *gPlayer;

#endif
