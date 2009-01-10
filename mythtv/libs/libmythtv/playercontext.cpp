#include <math.h>
#include <qtimer.h>
#include <qpainter.h>

#include "playercontext.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "livetvchain.h"
#include "RingBuffer.h"
#include "playgroup.h"
#include "dialogbox.h"
#include "util-osx-cocoa.h"
#include "videoouttypes.h"

#define LOC QString("playCtx: ")
#define LOC_ERR QString("playCtx, Error: ")

const uint PlayerContext::kSMExitTimeout     = 2000;
const uint PlayerContext::kMaxChannelHistory = 30;

static void *SpawnDecode(void *param)
{
    // OS X needs a garbage collector allocated..
    void *decoder_thread_pool = CreateOSXCocoaPool();
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    if (nvp->StartPlaying(false))
        nvp->StopPlaying();
    DeleteOSXCocoaPool(decoder_thread_pool);
    return NULL;
}

PlayerContext::PlayerContext() :
    nvp(NULL), nvpUnsafe(false), recorder(NULL),
    tvchain(NULL), buffer(NULL), playingInfo(NULL),
    decoding(false), last_cardid(-1), last_framerate(30.0f),
    // Fast forward state
    ff_rew_state(0), ff_rew_index(0), ff_rew_speed(0),
    // Other state
    paused(false), playingState(kState_None),
    errored(false),
    // pseudo states 
    pseudoLiveTVRec(NULL), pseudoLiveTVState(kPseudoNormalLiveTV),
    // DB values
    fftime(0), rewtime(0),
    jumptime(0), ts_normal(1.0f), ts_alt(1.5f),
    // locks
    playingInfoLock(QMutex::Recursive), deleteNVPLock(QMutex::Recursive),
    stateLock(QMutex::Recursive),
    // pip
    pipState(kPIPOff), pipRect(0,0,0,0), parentWidget(NULL), 
    useNullVideo(false),
    // embedding
    embedWinID(0), embedBounds(0,0,0,0)
{
    lastSignalMsgTime.start();
    lastSignalMsgTime.addMSecs(-2 * kSMExitTimeout);
}

PlayerContext::~PlayerContext()
{
    TeardownPlayer();

    nextState.clear();
}

void PlayerContext::TeardownPlayer(void)
{
    ff_rew_state = 0;
    ff_rew_index = 0;
    ff_rew_speed = 0;
    ts_normal    = 1.0f;

    SetNVP(NULL);
    SetRecorder(NULL);
    SetRingBuffer(NULL);
    SetTVChain(NULL);
    SetPlayingInfo(NULL);
}

/**
 * \brief determine initial tv state and playgroup for the recording
 * \param islivetv: true if recording is livetv
 */
void PlayerContext::SetInitialTVState(bool islivetv)
{
    TVState newState = kState_None;
    QString newPlaygroup("Default");

    LockPlayingInfo(__FILE__, __LINE__);
    if (islivetv)
    {
        SetTVChain(new LiveTVChain());
        newState = kState_WatchingLiveTV;
    }
    else if (playingInfo)
    {
        int overrecordseconds = gContext->GetNumSetting("RecordOverTime");
        QDateTime curtime = QDateTime::currentDateTime();
        QDateTime recendts = playingInfo->recendts.addSecs(overrecordseconds);

        if (curtime < recendts && !playingInfo->isVideo)
            newState = kState_WatchingRecording;
        else
            newState = kState_WatchingPreRecorded;

        newPlaygroup = playingInfo->playgroup;
    }
    UnlockPlayingInfo(__FILE__, __LINE__);

    ChangeState(newState);
    SetPlayGroup(newPlaygroup);
}

/**
 * \brief Check if PIP is supported for current video
 * renderer running. Current support written for XV and Opengl.
 * Not sure about ivtv.
 */
bool PlayerContext::IsPIPSupported(void) const
{
    bool supported = false;
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        const VideoOutput *vid = nvp->getVideoOutput();
        if (vid && 
            (vid->hasXVAcceleration() ||
             vid->hasOpenGLAcceleration()) ||
             vid->hasVDPAUAcceleration())
        {
            supported = true;
        }
    }
    return supported;
}

bool PlayerContext::IsOSDFullScreen(void) const
{
    // Note: This is to allow future OSD implementations to cover
    // two or more PBP screens.
    return false;
}

void PlayerContext::CreatePIPWindow(const QRect &rect, int pos,
                    QWidget *widget)
{
    QString name;
    if (pos > -1)
    {
        pipLocation = pos;
        name = QString("pip player %1").arg(toString((PIPLocation)pos));
    }
    else
        name = "pip player";
    
    if (widget)
        parentWidget = widget;

    pipRect = QRect(rect);
}

/**
 * \brief Get PIP more accurate display size for standalone PIP
 * by factoring the aspect ratio of the video.
 */
QRect PlayerContext::GetStandAlonePIPRect(void)
{
    QRect rect = QRect(0, 0, 0, 0);
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        rect = QRect(pipRect);

        float saspect = (float)rect.width() / (float)rect.height();
        float vaspect = nvp->GetVideoAspect();

        // Calculate new height or width according to relative aspect ratio
        if ((int)((saspect + 0.05) * 10) > (int)((vaspect + 0.05) * 10))
        {
            rect.setWidth((int) ceil(rect.width() * (vaspect / saspect)));
        }
        else if ((int)((saspect + 0.05) * 10) < (int)((vaspect + 0.05) * 10))
        {
            rect.setHeight((int) ceil(rect.height() * (saspect / vaspect)));
        }

        rect.setHeight(((rect.height() + 7) / 8) * 8);
        rect.setWidth( ((rect.width()  + 7) / 8) * 8);
    }
    return rect;
}
/**
 * \brief draw a frame for standalone embedded tv playback
 * used for software scaling in non TV related PIP like the playbackbox
 * PIP window must be resized before this function works properly
 */
void PlayerContext::DrawARGBFrame(QPainter *p)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp && nvp->UsingNullVideo())
    {
        QRect tmpRect = GetStandAlonePIPRect();
        QSize tmpSize = QSize(tmpRect.size());
        const QImage &img = nvp->GetARGBFrame(tmpSize);

        int video_y = 0;
        int video_x = 0;

        // Centre video in the y axis
        if (img.height() < pipRect.height())
            video_y = pipRect.y() + 
                            (pipRect.height() - img.height()) / 2;
        else
            video_y = pipRect.y();

        // Centre video in the x axis
        if (img.width() < pipRect.width())
            video_x = pipRect.x() + 
                            (pipRect.width() - img.width()) / 2;
        else
            video_x = pipRect.x();
       
        p->drawImage(video_x, video_y, img);
    }
}

bool PlayerContext::StartPIPPlayer(TV *tv, TVState desiredState)
{
    bool ok = false;

    if (!useNullVideo && parentWidget)
    {
        const QRect rect = QRect(pipRect);
        ok = CreateNVP(tv, parentWidget, desiredState,
                     parentWidget->winId(), &rect);
    }

    if (useNullVideo || !ok) 
    { 
        SetNVP(NULL);
        useNullVideo = true;
        ok = CreateNVP(tv, NULL, desiredState,
                        0, NULL);
    }

    return ok;
}


/**
 * \brief stop NVP but pause the ringbuffer. used in PIP/PBP swap or 
 * switching from PIP <-> PBP or enabling PBP
 */

void PlayerContext::PIPTeardown(void)
{
    if (buffer)
    {
        buffer->Pause();
        buffer->WaitForPause();
    }

    {
        QMutexLocker locker(&deleteNVPLock);
        if (nvp)
            nvp->StopPlaying();
    }

    SetNVP(NULL);

    useNullVideo = false;
    parentWidget = NULL;
}

/**
 * \brief Resize PIP Window
 */
void PlayerContext::ResizePIPWindow(const QRect &rect)
{
    if (!IsPIP())
        return;

    QRect tmpRect;
    if (pipState == kPIPStandAlone)
        tmpRect = GetStandAlonePIPRect();
    else
        tmpRect = QRect(rect);

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp && nvp->getVideoOutput())
        nvp->getVideoOutput()->ResizeDisplayWindow(tmpRect, false);
    UnlockDeleteNVP(__FILE__, __LINE__);

    pipRect = QRect(rect);
}

bool PlayerContext::StartEmbedding(WId wid, const QRect &embedRect)
{
    embedWinID = 0;

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
    {
        embedWinID = wid;
        embedBounds = embedRect;
        nvp->EmbedInWidget(
            embedRect.topLeft().x(), embedRect.topLeft().y(),
            embedRect.width(),       embedRect.height(),
            embedWinID);
    }
    UnlockDeleteNVP(__FILE__, __LINE__);

    return embedWinID;
}

bool PlayerContext::IsEmbedding(void) const
{
    bool ret = false;
    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
        ret = nvp->IsEmbedding();
    UnlockDeleteNVP(__FILE__, __LINE__);
    return ret;
}

void PlayerContext::StopEmbedding(void)
{
    embedWinID = 0;

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
        nvp->StopEmbedding();
    UnlockDeleteNVP(__FILE__, __LINE__);
}

bool PlayerContext::HasNVP(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp;
}

bool PlayerContext::IsNVPErrored(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsErrored();
}

bool PlayerContext::IsNVPPlaying(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsPlaying();
}

bool PlayerContext::HandleNVPSpeedChangeFFRew(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if ((ff_rew_state || ff_rew_speed) && nvp && nvp->AtNormalSpeed())
    {
        ff_rew_speed = 0;
        ff_rew_state = 0;
        ff_rew_index = TV::kInitFFRWSpeed;
        return true;
    }
    return false;
}

bool PlayerContext::HandleNVPSpeedChangeEOF(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp && (nvp->GetNextPlaySpeed() != ts_normal) &&
        nvp->AtNormalSpeed() && !nvp->PlayingSlowForPrebuffer())
    {
        // Speed got changed in NVP since we are close to the end of file
        ts_normal = 1.0f;
        return true;
    }
    return false;
}

bool PlayerContext::CalcNVPSliderPosition(
    struct StatusPosInfo &posInfo, bool paddedFields) const
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        nvp->calcSliderPos(posInfo, paddedFields);
        return true;
    }
    return false;
}

bool PlayerContext::IsRecorderErrored(void) const
{
    return recorder && recorder->GetErrorStatus();
}

bool PlayerContext::CreateNVP(TV *tv, QWidget *widget,
                              TVState desiredState,
                              WId embedwinid, const QRect *embedbounds)
{
    int exact_seeking = gContext->GetNumSetting("ExactSeeking", 0);

    if (HasNVP())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Attempting to setup a player, but it already exists.");
        return false;
    }

    NuppelVideoPlayer *_nvp = new NuppelVideoPlayer("player");

    _nvp->SetPlayerInfo(tv, widget, exact_seeking, this);
    _nvp->SetAudioInfo(gContext->GetSetting("AudioOutputDevice"),
                       gContext->GetSetting("PassThruOutputDevice"),
                       gContext->GetNumSetting("AudioSampleRate", 44100));
    _nvp->SetAudioStretchFactor(ts_normal);
    _nvp->SetLength(playingLen);

    if (useNullVideo)
    {
        _nvp->SetNullVideo();
        _nvp->SetVideoFilters("onefield");
    }

    if (!IsAudioNeeded())
        _nvp->SetNoAudio();
    else
    {
        _nvp->LoadExternalSubtitles(buffer->GetFilename());
    }
        
    if ((embedwinid > 0) && embedbounds)
    {
        _nvp->EmbedInWidget(
            embedbounds->x(), embedbounds->y(),
            embedbounds->width(), embedbounds->height(), embedwinid);
    }

    bool isWatchingRecording = (desiredState == kState_WatchingRecording);
    _nvp->SetWatchingRecording(isWatchingRecording);

    SetNVP(_nvp);

    if (nvp->OpenFile() < 0)
        return false;

    if (pipState == kPIPOff || pipState == kPBPLeft)
    {
        int ret = 1;
        if (nvp->HasAudioOut() ||
            (nvp->IsIVTVDecoder() &&
            !gContext->GetNumSetting("PVR350InternalAudioOnly")))
        {
            QString errMsg = nvp->ReinitAudio();
            /*
            if ((errMsg != QString::null) &&
                gContext->GetNumSetting("AudioNag", 1))
            {
                DialogBox *dlg = new DialogBox(gContext->GetMainWindow(), errMsg);

                QString noaudio  = QObject::tr("Continue WITHOUT AUDIO!");
                QString dontask  = noaudio + " " +
                        QObject::tr("And, never ask again.");
                QString neverask = noaudio + " " +
                        QObject::tr("And, don't ask again in this session.");
                QString quit     = QObject::tr("Return to menu.");

                dlg->AddButton(noaudio);
                dlg->AddButton(dontask);
                dlg->AddButton(neverask);
                dlg->AddButton(quit);

                qApp->lock();
                ret = dlg->exec();
                dlg->deleteLater();
                qApp->unlock();
            }

            if (kDialogCodeButton1 == ret)
                gContext->SaveSetting("AudioNag", 0);
            if (kDialogCodeButton2 == ret)
                gContext->SetSetting("AudioNag", 0);
            else if ((kDialogCodeButton3 == ret) || 
                    (kDialogCodeRejected == ret))
            {
                return false;
            } */
        }
        
    }
    else if (pipState == kPBPRight)
        nvp->SetMuted(true);

    int maxWait = -1;
    //if (isPIP())
    //   maxWait = 1000; 

    return StartDecoderThread(maxWait);
}

/** \fn PlayerContext::StartDecoderThread(int)
 *  \brief Starts player, must be called after StartRecorder().
 *  \param maxWait How long to wait for NuppelVideoPlayer to start playing.
 *  \return true when successful, false otherwise.
 */
bool PlayerContext::StartDecoderThread(int maxWait)
{
    if (decoding) // note not thread-safe, but this is just to catch programmer error..
        return false;

    decoding = true;

    if (pthread_create(&decode, NULL, SpawnDecode, nvp))
    {
        decoding = false;
        return false;
    }

    maxWait = (maxWait <= 0) ? 20000 : maxWait;
#ifdef USING_VALGRIND
    maxWait = (1<<30);
#endif // USING_VALGRIND
    MythTimer t;
    t.start();

    while (!nvp->IsPlaying(50, true) &&
           nvp->IsDecoderThreadAlive() &&
           (t.elapsed() < maxWait))
    {
        ReloadTVChain();
    }

    if (nvp->IsPlaying())
    {
        VERBOSE(VB_PLAYBACK, LOC + "StartDecoderThread(): took "<<t.elapsed()
                <<" ms to start player.");

        nvp->ResetCaptions();
        nvp->ResetTeletext();
        return true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "StartDecoderThread() "
                "Failed to startdecoder");
        nvp->StopPlaying();
        pthread_join(decode, NULL);
        decoding = false;
        return false;
    }
}

/** \fn PlayerContext::StartOSD(TV *tv)
 *  \brief Initializes the on screen display.
 *
 *   If the NuppelVideoPlayer already exists we grab it's OSD via
 *   NuppelVideoPlayer::GetOSD().
 */
bool PlayerContext::StartOSD(TV *tv)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        last_framerate = nvp->GetFrameRate();

        OSD *osd = nvp->GetOSD();
        if (osd)
        {
            osd->SetUpOSDClosedHandler(tv);
            return true;
        }
    }
    return false;
}

void PlayerContext::UpdateTVChain(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if (tvchain && nvp)
    {
        tvchain->ReloadAll();
        nvp->CheckTVChain();
    }
}

bool PlayerContext::ReloadTVChain(void)
{
    if (!tvchain)
        return false;

    tvchain->ReloadAll();
    ProgramInfo *pinfo = tvchain->GetProgramAt(-1);
    if (pinfo)
    {
        SetPlayingInfo(pinfo);
        delete pinfo;
    }
    return (bool) pinfo;
}

/**
 * \brief most recently selected channel to the previous channel list
 */
void PlayerContext::PushPreviousChannel(void)
{
    if (!tvchain)
        return;

    // Don't store more than kMaxChannelHistory channels. Remove the first item
    if (prevChan.size() >= kMaxChannelHistory)
        prevChan.pop_front();

    // This method builds the stack of previous channels
    QString curChan = tvchain->GetChannelName(-1);
    if (prevChan.size() == 0 || 
        curChan != prevChan[prevChan.size() - 1])
    {
        QString chan = curChan;
        prevChan.push_back(chan);
    }
}

QString PlayerContext::PopPreviousChannel(void)
{
    if (prevChan.empty())
        return QString::null;

    QString curChan = tvchain->GetChannelName(-1);
    if ((curChan == prevChan.back()) && !prevChan.empty())
        prevChan.pop_back();

    if (prevChan.empty())
        return QString::null;

    QString chan = prevChan.back();
    chan.detach();
    return chan;
}

QString PlayerContext::GetPreviousChannel(void) const
{
    if (prevChan.empty())
        return QString::null;

    QString curChan = tvchain->GetChannelName(-1);
    QString preChan = QString::null;
    if (curChan != prevChan.back() || prevChan.size() < 2)
        preChan = prevChan.back();
    else
        preChan = prevChan[prevChan.size()-2];
    preChan.detach();
    return preChan;
}

void PlayerContext::LockPlayingInfo(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("LockPlayingInfo(%1,%2)")
    //        .arg(file).arg(line));
    playingInfoLock.lock();
}

void PlayerContext::UnlockPlayingInfo(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("UnlockPlayingInfo(%1,%2)")
    //        .arg(file).arg(line));
    playingInfoLock.unlock();
}

/**
 * \brief prevent NVP from been deleted
 *        used to ensure nvp can only be deleted after
 *        osd in TV() is unlocked.
 */
void PlayerContext::LockDeleteNVP(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("LockDeleteNVP(%1,%2)")
    //        .arg(file).arg(line));
    deleteNVPLock.lock();
}

/**
 * \brief allow NVP to be deleted.
 */
void PlayerContext::UnlockDeleteNVP(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("UnlockDeleteNVP(%1,%2)")
    //        .arg(file).arg(line));
    deleteNVPLock.unlock();
}

void PlayerContext::LockState(void) const
{
    stateLock.lock();
}

void PlayerContext::UnlockState(void) const
{
    stateLock.unlock();
}

bool PlayerContext::InStateChange(void) const
{
    if (!stateLock.tryLock())
        return true;
    bool inStateChange = nextState.size() > 0;
    stateLock.unlock();
    return inStateChange;
}

/**
*   \brief Puts a state change on the nextState queue.
*/
void PlayerContext::ChangeState(TVState newState)
{
    QMutexLocker locker(&stateLock);
    nextState.enqueue(newState);
}

TVState PlayerContext::DequeueNextState(void)
{
    QMutexLocker locker(&stateLock);
    return nextState.dequeue();
}

/**
 * \brief Removes any pending state changes, and puts kState_None on the queue.
 */
void PlayerContext::ForceNextStateNone(void)
{
    QMutexLocker locker(&stateLock);
    nextState.clear();
    nextState.push_back(kState_None);
}

TVState PlayerContext::GetState(void) const
{
    QMutexLocker locker(&stateLock);
    return playingState;
}

bool PlayerContext::IsSameProgram(const ProgramInfo &p) const
{
    bool ret = false;
    LockPlayingInfo(__FILE__, __LINE__);
    if (playingInfo)
        ret = playingInfo->IsSameProgram(p);
    UnlockPlayingInfo(__FILE__, __LINE__);
    return ret;
}

QString PlayerContext::GetFilters(const QString &baseFilters) const
{
    QString filters     = baseFilters;
    QString chanFilters = QString::null;

    LockPlayingInfo(__FILE__, __LINE__);
    if (playingInfo) // Recordings have this info already.
    {
        chanFilters = playingInfo->chanOutputFilters;
        chanFilters.detach();
    }
    UnlockPlayingInfo(__FILE__, __LINE__);

    if (!chanFilters.isEmpty())
    {
        if ((chanFilters[0] != '+'))
        {
            filters = chanFilters;
        }
        else 
        {
            if (!filters.isEmpty() && (filters.right(1) != ","))
                filters += ",";

            filters += chanFilters.mid(1);
        }
    }

    VERBOSE(VB_CHANNEL, LOC +
            QString("Output filters for this channel are: '%1'")
                    .arg(filters));

    filters.detach();
    return filters;
}

QString PlayerContext::GetPlayMessage(void) const
{
    QString mesg = QObject::tr("Play");
    if (ts_normal != 1.0)
        mesg += QString(" %1X").arg(ts_normal);

    if (0)
    {
        QMutexLocker locker(&deleteNVPLock);
        FrameScanType scan = nvp->GetScanType();
        if (is_progressive(scan) || is_interlaced(scan))
            mesg += " (" + toString(scan, true) + ")";
    }

    return mesg;
}

void PlayerContext::SetNVP(NuppelVideoPlayer *new_nvp)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        NuppelVideoPlayer *xnvp = nvp;
        nvp = NULL;

        if (decoding)
        {
            xnvp->StopPlaying();
            pthread_join(decode, NULL);
            decoding = false;
        }

        delete xnvp;

        nvp = new_nvp;
    }
    else
    {
        nvp = new_nvp;
    }
}

void PlayerContext::SetRecorder(RemoteEncoder *rec)
{
    if (recorder)
    {
        delete recorder;
        recorder = NULL;
    }

    if (rec)
    {
        recorder = rec;
        last_cardid = recorder->GetRecorderNumber();
    }
}

void PlayerContext::SetTVChain(LiveTVChain *chain)
{
    if (tvchain)
    {
        tvchain->DestroyChain();
        delete tvchain;
        tvchain = NULL;
    }

    tvchain = chain;

    if (tvchain)
    {
        QString seed = QString("");

        if (IsPIP())
            seed = "PIP";

        seed += gContext->GetHostName();

        tvchain->InitializeNewChain(gContext->GetHostName());
    }
}

void PlayerContext::SetRingBuffer(RingBuffer *buf)
{
    if (buffer)
    {
        delete buffer;
        buffer = NULL;
    }

    buffer = buf;
}

/**
 * \brief assign programinfo to the context
 */
void PlayerContext::SetPlayingInfo(const ProgramInfo *info)
{
    QMutexLocker locker(&playingInfoLock);

    if (playingInfo)
    {
        playingInfo->MarkAsInUse(false);
        delete playingInfo;
        playingInfo = NULL;
    }

    if (info)
    {
        playingInfo = new ProgramInfo(*info);
        playingLen  = playingInfo->CalculateLength();
    }
}

void PlayerContext::SetPlayGroup(const QString &group)
{
    fftime       = PlayGroup::GetSetting(group, "skipahead", 30);
    rewtime      = PlayGroup::GetSetting(group, "skipback", 5);
    jumptime     = PlayGroup::GetSetting(group, "jump", 10);
    ts_normal    = PlayGroup::GetSetting(group, "timestretch", 100) * 0.01f;
    ts_alt       = (ts_normal == 1.0f) ? 1.5f : 1.0f;
}

void PlayerContext::SetPseudoLiveTV(
    const ProgramInfo *pi, PseudoState new_state)
{
    ProgramInfo *old_rec = pseudoLiveTVRec;
    ProgramInfo *new_rec = NULL;

    if (pi)
    {
        new_rec = new ProgramInfo(*pi);
        QString msg = QString("Wants to record: %1 %2 %3 %4")
            .arg(new_rec->title).arg(new_rec->chanstr)
            .arg(new_rec->recstartts.toString())
            .arg(new_rec->recendts.toString());
        VERBOSE(VB_PLAYBACK, LOC + msg);
    }

    pseudoLiveTVRec   = new_rec;
    pseudoLiveTVState = new_state;

    if (old_rec)
    {
        QString msg = QString("Done to recording: %1 %2 %3 %4")
            .arg(old_rec->title).arg(old_rec->chanstr)
            .arg(old_rec->recstartts.toString())
            .arg(old_rec->recendts.toString());
        VERBOSE(VB_PLAYBACK, LOC + msg);
        delete old_rec;
    }
}
