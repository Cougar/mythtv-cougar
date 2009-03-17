#ifndef _PLAYER_CONTEXT_H_
#define _PLAYER_CONTEXT_H_

#include <vector>
#include <deque>
using namespace std;

#include <pthread.h>

// Qt headers
#include <QApplication>
#include <QMutex>
#include <QString>
#include <QVector>
#include <QMap>

#include "mythdeque.h"
#include "tv.h"
#include "videoouttypes.h"
#include "util.h"

class TV;
class RemoteEncoder;
class NuppelVideoPlayer;
class RingBuffer;
class ProgramInfo;
class LiveTVChain;
class MythDialog;

typedef enum
{
    kPseudoNormalLiveTV  = 0,
    kPseudoChangeChannel = 1,
    kPseudoRecording     = 2,
} PseudoState;

typedef deque<QString>         StringDeque;
typedef QMap<QString,QString>  InfoMap;

class MPUBLIC PlayerContext
{
  public:
    PlayerContext(const QString &inUseID = QString("Unknown"));
    ~PlayerContext();

    // Actions
    bool CreateNVP(TV *tv, QWidget *widget,
                   TVState desiredState,
                   WId embedwinid, const QRect *embedBounds);
    void TeardownPlayer(void);
    bool StartDecoderThread(int maxWait = -1);
    bool StartOSD(TV *tv);
    void UpdateTVChain(void);
    bool ReloadTVChain(void);
    void CreatePIPWindow(const QRect&, int pos = -1, 
                        QWidget *widget = NULL);
    void ResizePIPWindow(const QRect&);
    bool StartPIPPlayer(TV *tv, TVState desiredState);
    void PIPTeardown(void);
    void DrawARGBFrame(QPainter *p);
    void SetNullVideo(bool setting) { useNullVideo = setting; }
    bool StartEmbedding(WId wid, const QRect&);
    void StopEmbedding(void);
    void    PushPreviousChannel(void);
    QString PopPreviousChannel(void);

    void ChangeState(TVState newState);
    void ForceNextStateNone(void);
    TVState DequeueNextState(void);

    void ResizePIPWindow(void);
    bool HandleNVPSpeedChangeFFRew(void);
    bool HandleNVPSpeedChangeEOF(void);

    // Locking
    void LockState(void) const;
    void UnlockState(void) const;

    void LockPlayingInfo(const char *file, int line) const;
    void UnlockPlayingInfo(const char *file, int line) const;

    void LockDeleteNVP(const char *file, int line) const;
    void UnlockDeleteNVP(const char *file, int line) const;

    // Sets
    void SetInitialTVState(bool islivetv);
    void SetNVP(NuppelVideoPlayer *new_nvp);
    void SetRecorder(RemoteEncoder *rec);
    void SetTVChain(LiveTVChain *chain);
    void SetRingBuffer(RingBuffer *buf);
    void SetPlayingInfo(const ProgramInfo *info);
    void SetPlayGroup(const QString &group);
    void SetPseudoLiveTV(const ProgramInfo *pi, PseudoState new_state);
    void SetPIPLocation(int loc) { pipLocation = loc; }
    void SetPIPState(PIPState change) { pipState = change; }
    void SetNVPChangingBuffers(bool val) { nvpUnsafe = val; }
    void SetNoHardwareDecoders(void) { nohardwaredecoders = true; }

    // Gets
    QRect    GetStandAlonePIPRect(void);
    PIPState GetPIPState(void) const { return pipState; }
    QString  GetPreviousChannel(void) const;
    bool     CalcNVPSliderPosition(struct StatusPosInfo &posInfo,
                                   bool paddedFields = false) const;
    uint     GetCardID(void) const { return last_cardid; }
    QString  GetFilters(const QString &baseFilters) const;
    QString  GetPlayMessage(void) const;
    TVState  GetState(void) const;

    // Boolean Gets
    bool IsPIPSupported(void) const;
    bool IsPBPSupported(void) const;
    bool IsOSDFullScreen(void) const;
    bool IsPIP(void) const
        { return (kPIPonTV == pipState) || (kPIPStandAlone == pipState); }
    bool IsPBP(void) const
        { return (kPBPLeft == pipState) || (kPBPRight      == pipState); }
    bool IsAudioNeeded(void) const
        { return (kPIPOff  == pipState) || (kPBPLeft       == pipState); }
    bool IsNullVideoDesired(void)   const { return useNullVideo; }
    bool IsNVPChangingBuffers(void) const { return nvpUnsafe;    }
    bool IsEmbedding(void) const;
    bool HasNVP(void) const;
    bool IsNVPErrored(void) const;
    bool IsNVPRecoverable(void) const;
    bool IsNVPDecoderErrored(void) const;
    bool IsNVPPlaying(void) const;
    bool IsRecorderErrored(void) const;
    bool InStateChange(void) const;
    /// This is set if the player encountered some irrecoverable error.
    bool IsErrored(void) const { return errored; }
    bool IsSameProgram(const ProgramInfo &p) const;
    bool IsValidLiveTV(void) const
        { return nvp && tvchain && recorder && buffer; }

  public:
    QString             recUsage;
    NuppelVideoPlayer  *nvp;
    volatile bool       nvpUnsafe;
    RemoteEncoder      *recorder;
    LiveTVChain        *tvchain;
    RingBuffer         *buffer;
    ProgramInfo        *playingInfo; ///< Currently playing info
    long long           playingLen;  ///< Initial CalculateLength()
    bool                nohardwaredecoders; // < Disable use of VDPAU decoding
    bool                decoding;    ///< Video decoder thread started
    pthread_t           decode;      ///< Video decoder thread
    int                 last_cardid; ///< CardID of current/last recorder
    float               last_framerate; ///< Estimated framerate from recorder
    /// 0 == normal, +1 == fast forward, -1 == rewind
    int                 ff_rew_state;
    /// Index into ff_rew_speeds for FF and Rewind speeds
    int                 ff_rew_index;
    /// Caches value of ff_rew_speeds[ff_rew_index]
    int                 ff_rew_speed;
    bool                paused;
    TVState             playingState;

    bool                errored;

    // Previous channel functionality state variables
    StringDeque         prevChan; ///< Previous channels

    // Recording to play next, after LiveTV
    ProgramInfo        *pseudoLiveTVRec;
    PseudoState         pseudoLiveTVState;

    int                 fftime;
    int                 rewtime;
    int                 jumptime;
    /** \brief Time stretch speed, 1.0f for normal playback.
     *
     *  Begins at 1.0f meaning normal playback, but can be increased
     *  or decreased to speedup or slowdown playback.
     *  Ignored when doing Fast Forward or Rewind.
     */
    float               ts_normal; 
    float               ts_alt;

    mutable QMutex      playingInfoLock;
    mutable QMutex      deleteNVPLock;
    mutable QMutex      stateLock;

    // Signal info
    mutable QStringList lastSignalMsg;
    mutable MythTimer   lastSignalMsgTime;
    mutable InfoMap     lastSignalUIInfo;
    mutable MythTimer   lastSignalUIInfoTime;

    // tv state related
    MythDeque<TVState>  nextState;

    // Picture-in-Picture related
    PIPState            pipState;
    QRect               pipRect;
    QWidget            *parentWidget;
    /// Position of PIP on TV screen
    int                 pipLocation;
    /// True iff software scaled PIP should be used
    bool                useNullVideo;

    // Embedding related
    WId   embedWinID;       ///< Window ID when embedded in another widget
    QRect embedBounds;      ///< Bounds when embedded in another widget

    /// Timeout after last Signal Monitor message for ignoring OSD when exiting.
    static const uint kSMExitTimeout;
    static const uint kMaxChannelHistory;
};

#endif // _PLAYER_CONTEXT_H_
