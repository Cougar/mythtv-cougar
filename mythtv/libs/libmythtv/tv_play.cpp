#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qfile.h>
#include <qtimer.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "osd.h"
#include "osdtypes.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "remoteencoder.h"
#include "remoteutil.h"
#include "guidegrid.h"
#include "volumecontrol.h"
#include "NuppelVideoPlayer.h"
#include "programinfo.h"
#include "avformat.h"

enum SeekSpeeds {
  SSPEED_NORMAL_WITH_DISPLAY = 0,
  SSPEED_SLOW_1,
  SSPEED_SLOW_2,
  SSPEED_NORMAL,
  SSPEED_FAST_1,
  SSPEED_FAST_2,
  SSPEED_FAST_3,
  SSPEED_FAST_4,
  SSPEED_FAST_5,
  SSPEED_FAST_6,
  SSPEED_MAX
};

struct SeekSpeedInfo {
    QString   dispString;
    float  scaling;
    float ff_repos;
    float rew_repos;
};

SeekSpeedInfo seek_speed_array[] =
{
    {"",     0.00,   0.00,  0.00},
    {"1/4X", 0.25,   0.00,  0.00},
    {"1/2X", 0.50,   4.00,  4.00},
    {"1X",   1.00,  12.00,  9.00},
    {"1.5X", 1.50,  16.00, 11.50},
    {"2X",   2.24,  20.00, 14.00},
    {"3X",   3.34,  28.00, 19.00},
    {"8X",   7.48,  68.00, 44.00},
    {"10X", 11.18,  84.00, 54.00},
    {"16X", 16.72, 132.00, 84.00}
};

const int kMuteTimeout = 800;

void *SpawnDecode(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    nvp->StopPlaying();
    return NULL;
}

TV::TV(QSqlDatabase *db)
  : QObject()
{
    m_db = db;

    dialogname = "";
    playbackinfo = NULL;
    editmode = false;
    queuedTranscode = false;
    browsemode = false;
    prbuffer = NULL;
    nvp = NULL;
    osd = NULL;
    requestDelete = false;
    endOfRecording = false;
    volumeControl = NULL;
    embedid = 0;
    times_pressed = 0;
    last_channel = "";
    picAdjustment = 0;

    getRecorderPlaybackInfo = false;
    recorderPlaybackInfo = NULL;
    lastRecorderNum = -1;
    wantsToQuit = true;

    myWindow = NULL;

    gContext->addListener(this);

    PrevChannelVector channame_vector(30);

    prevChannelTimer = new QTimer(this);
    connect(prevChannelTimer, SIGNAL(timeout()), SLOT(SetPreviousChannel()));

    muteTimer = new QTimer(this);
    connect(muteTimer, SIGNAL(timeout()), SLOT(UnMute()));

    keyrepeatTimer = new QTimer(this);
    connect(keyrepeatTimer, SIGNAL(timeout()), SLOT(KeyRepeatOK()));

    browseTimer = new QTimer(this);
    connect(browseTimer, SIGNAL(timeout()), SLOT(BrowseEndTimer()));
}

void TV::Init(bool createWindow)
{
    fftime = gContext->GetNumSetting("FastForwardAmount", 30);
    rewtime = gContext->GetNumSetting("RewindAmount", 5);
    jumptime = gContext->GetNumSetting("JumpAmount", 10);

    recorder = piprecorder = activerecorder = NULL;
    nvp = pipnvp = activenvp = NULL;
    prbuffer = piprbuffer = activerbuffer = NULL;

    menurunning = false;

    internalState = nextState = kState_None; 

    runMainLoop = false;
    changeState = false;

    keyRepeat = true;

    if (createWindow)
    {
        myWindow = new MythDialog(gContext->GetMainWindow(), "tv playback");
        myWindow->installEventFilter(this);
        myWindow->setNoErase();
        myWindow->show();
        myWindow->setBackgroundColor(Qt::black);
        qApp->processEvents();
    }

    if (gContext->GetNumSetting("MythControlsVolume", 1))
        volumeControl = new VolumeControl(true);

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);
}

TV::~TV(void)
{
    gContext->removeListener(this);

    runMainLoop = false;
    pthread_join(event, NULL);

    if (prbuffer)
        delete prbuffer;
    if (nvp)
        delete nvp;
    if (volumeControl)
        delete volumeControl;
    if (myWindow)
        delete myWindow;
    if (recorderPlaybackInfo)
        delete recorderPlaybackInfo;
}

TVState TV::GetState(void)
{
    if (changeState)
        return kState_ChangingState;
    return internalState;
}

int TV::LiveTV(bool showDialogs)
{
    if (internalState == kState_None)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();

        if (!testrec)
            return 0;

        if (!testrec->IsValidRecorder())
        {
            if (showDialogs)
            {
                QString title = tr("MythTV is already using all available "
                                   "inputs for recording.  If you want to "
                                   "watch an in-progress recording, select one "
                                   "from the playback menu.  If you want to "
                                   "watch live TV, cancel one of the "
                                   "in-progress recordings from the delete "
                                   "menu.");
    
                DialogBox diag(gContext->GetMainWindow(), title);
                diag.AddButton(tr("Cancel and go back to the TV menu"));
                diag.exec();
            }        

            delete testrec;
 
            return 0;
        }

        activerecorder = recorder = testrec;
        lastRecorderNum = recorder->GetRecorderNumber();
        nextState = kState_WatchingLiveTV;
        changeState = true;
    }

    return 1;
}

void TV::FinishRecording(void)
{
    if (!IsRecording())
        return;

    activerecorder->FinishRecording();
}

void TV::AskAllowRecording(const QString &message, int timeuntil)
{
    //TODO: integrate this dialog so that event doesn't have to wait
    if (GetState() != kState_WatchingLiveTV)
       return;

    dialogname = "allowrecordingbox";

    while (!osd)
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(1000);
        qApp->lock();
    }

    QStringList options;
    options += tr("Record and watch while it records");
    options += tr("Let it record and go back to the Main Menu");
    options += tr("Don't let it record, I want to watch TV");

    osd->NewDialogBox(dialogname, message, options, timeuntil); 

    while (osd->DialogShowing(dialogname))
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(1000);
        qApp->lock();
    }

    int result = osd->GetDialogResponse(dialogname);
    dialogname = "";

    if (result == 2)
        StopLiveTV();
    else if (result == 3)
        recorder->CancelNextRecording();
}

int TV::Playback(ProgramInfo *rcinfo)
{
    if (internalState != kState_None)
        return 0;

    inputFilename = rcinfo->pathname;

    playbackLen = rcinfo->CalculateLength();
    playbackinfo = rcinfo;

    int overrecordseconds = gContext->GetNumSetting("RecordOverTime");
    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime endts = rcinfo->endts.addSecs(overrecordseconds);

    if (curtime < endts)
        nextState = kState_WatchingRecording;
    else
        nextState = kState_WatchingPreRecorded;

    changeState = true;

    return 1;
}

int TV::PlayFromRecorder(int recordernum)
{
    int retval = 0;

    if (recorder)
    {
        cerr << "PlayFromRecorder : recorder already exists!";
        return -1;
    }

    recorder = RemoteGetExistingRecorder(recordernum);
    if (!recorder)
        return -1;

    if (recorder->IsValidRecorder())
    {
        // let the mainloop get the programinfo from encoder,
        // connecting to encoder won't work from here
        getRecorderPlaybackInfo = true;
        while (getRecorderPlaybackInfo)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(1000);
            qApp->lock();
        }
    }

    delete recorder;
    recorder = NULL;

    if (recorderPlaybackInfo)
    {
        bool fileexists = false;
        if (recorderPlaybackInfo->pathname.left(7) == "myth://")
            fileexists = RemoteCheckFile(recorderPlaybackInfo);
        else
        {
            QFile checkFile(recorderPlaybackInfo->pathname);
            fileexists = checkFile.exists();
        }

        if (fileexists)
        {
            Playback(recorderPlaybackInfo);
            retval = 1;
        }
    }

    return retval;
}

void TV::StateToString(TVState state, QString &statestr)
{
    switch (state) {
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded: statestr = "WatchingPreRecorded";
                                         break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
}

bool TV::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly || 
            state == kState_WatchingRecording);
}

bool TV::StateIsPlaying(TVState state)
{
    return (state == kState_WatchingPreRecorded || 
            state == kState_WatchingRecording);
}

TVState TV::RemoveRecording(TVState state)
{
    if (StateIsRecording(state))
    {
        if (state == kState_RecordingOnly)
            return kState_None;
        return kState_WatchingPreRecorded;
    }
    return kState_Error;
}

TVState TV::RemovePlaying(TVState state)
{
    if (StateIsPlaying(state))
        return kState_None;
    return kState_Error;
}

void TV::HandleStateChange(void)
{
    bool changed = false;

    TVState tmpInternalState = internalState;

    QString statename;
    StateToString(nextState, statename);
    QString origname;
    StateToString(tmpInternalState, origname);

    if (nextState == kState_Error)
    {
        printf("ERROR: Attempting to set to an error state.\n");
        exit(-1);
    }

    if (internalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        long long filesize = 0;
        long long smudge = 0;
        QString name = "";

        recorder->Setup();
        recorder->SetupRingBuffer(name, filesize, smudge);

        prbuffer = new RingBuffer(name, filesize, smudge, recorder);

        tmpInternalState = nextState;
        changed = true;

        persistentbrowsemode =
            gContext->GetNumSetting("PersistentBrowseMode", 0);

        recorder->SpawnLiveTV();

        StartPlayerAndRecorder(true, true);
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        tmpInternalState = nextState;
        changed = true;

        StopPlayerAndRecorder(true, true);
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_WatchingPreRecorded)
    {
        tmpInternalState = nextState;
        changed = true;
    }
    else if ((internalState == kState_None && 
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_None &&
              nextState == kState_WatchingRecording))
    {
        prbuffer = new RingBuffer(inputFilename, false);

        if (nextState == kState_WatchingRecording)
        {
            recorder = RemoteGetExistingRecorder(playbackinfo);
            if (!recorder || !recorder->IsValidRecorder())
            {
                cerr << "ERROR: couldn't find recorder for in-progress "
                     << "recording\n";
                nextState = kState_WatchingPreRecorded;
                if (recorder)
                    delete recorder;
                activerecorder = recorder = NULL;
            }
            else
            {
                activerecorder = recorder;
                recorder->Setup();
            }
        }

        tmpInternalState = nextState;
        changed = true;

        StartPlayerAndRecorder(true, false);
    }
    else if ((internalState == kState_WatchingPreRecorded && 
              nextState == kState_None) || 
             (internalState == kState_WatchingRecording &&
              nextState == kState_None))
    {
        if (internalState == kState_WatchingRecording)
            recorder->StopPlaying();

        tmpInternalState = nextState;
        changed = true;

        StopPlayerAndRecorder(true, false);
    }
    else if (internalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
        printf("Unknown state transition: %d to %d\n", internalState,
                                                       nextState);
    else
        printf("Changing from %s to %s\n", origname.ascii(), statename.ascii());

    internalState = tmpInternalState;
    changeState = false;

    if (recorder)
        recorder->FrontendReady();
}

void TV::StartPlayerAndRecorder(bool startPlayer, bool startRecorder)
{ 
    if (startRecorder)
    {
        while (!recorder->IsRecording())
            usleep(50);

        frameRate = recorder->GetFrameRate();
    }

    if (startPlayer)
    {
        SetupPlayer();
        pthread_create(&decode, NULL, SpawnDecode, nvp);

        while (!nvp->IsPlaying() && nvp->IsDecoderThreadAlive())
            usleep(50);

        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;

        frameRate = nvp->GetFrameRate();
        osd = nvp->GetOSD();
        osd->SetUpOSDClosedHandler(this);
    }
}

void TV::StopPlayerAndRecorder(bool closePlayer, bool closeRecorder)
{
    if (closePlayer)
    {
        if (prbuffer)
        {
            prbuffer->StopReads();
            prbuffer->Pause();
            while (!prbuffer->isPaused())
                usleep(50);
        }

        if (nvp)
            nvp->StopPlaying();

        if (piprbuffer)
        {
            piprbuffer->StopReads();
            piprbuffer->Pause();
            while (!piprbuffer->isPaused())
                usleep(50);
        }

        if (pipnvp)
            pipnvp->StopPlaying();
    }

    if (closeRecorder)
    {
        recorder->StopLiveTV();
        if (piprecorder > 0)
            piprecorder->StopLiveTV();
    }

    if (closePlayer)
    {
        TeardownPlayer();
        if (pipnvp)
            TeardownPipPlayer();
    }
}

void TV::SetupPlayer(void)
{
    if (nvp)
    { 
        printf("Attempting to setup a player, but it already exists.\n");
        return;
    }

    QString filters = "";
    nvp = new NuppelVideoPlayer(m_db, playbackinfo);
    nvp->SetParentWidget(myWindow);
    nvp->SetRingBuffer(prbuffer);
    nvp->SetRecorder(recorder);
    nvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                        gContext->GetSetting("OSDCCFont"),
                        gContext->GetInstallPrefix()); 
    nvp->SetOSDThemeName(gContext->GetSetting("OSDTheme"));
    nvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    nvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    nvp->SetLength(playbackLen);
    nvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));
    nvp->SetAutoCommercialSkip(gContext->GetNumSetting("AutoCommercialSkip"));
    nvp->SetCommercialSkipMethod(gContext->GetNumSetting("CommercialSkipMethod"));

    osd_display_time = gContext->GetNumSetting("OSDDisplayTime");

    if (gContext->GetNumSetting("DefaultCCMode"))
        nvp->ToggleCC();

    if (gContext->GetNumSetting("Deinterlace"))
    {
        if (filters.length() > 1)
            filters += ",";
        filters += "linearblend";
    }

    nvp->SetVideoFilters(filters);

    if (embedid > 0)
        nvp->EmbedInWidget(embedid, embx, emby, embw, embh);

    if (nextState == kState_WatchingRecording)
        nvp->SetWatchingRecording(true);

    osd = NULL;
}

void TV::SetupPipPlayer(void)
{
    if (pipnvp)
    {
        printf("Attempting to setup a pip player, but it already exists.\n");
        return;
    }

    pipnvp = new NuppelVideoPlayer();
    pipnvp->SetAsPIP();
    pipnvp->SetRingBuffer(piprbuffer);
    pipnvp->SetRecorder(piprecorder);
    pipnvp->SetOSDFontName(gContext->GetSetting("OSDFont"),
                           gContext->GetSetting("OSDCCFont"),
                           gContext->GetInstallPrefix());
    pipnvp->SetOSDThemeName(gContext->GetSetting("OSDTheme"));
    pipnvp->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));
    pipnvp->SetAudioDevice(gContext->GetSetting("AudioOutputDevice"));
    pipnvp->SetExactSeeks(gContext->GetNumSetting("ExactSeeking"));

    pipnvp->SetLength(playbackLen);
}

void TV::TeardownPlayer(void)
{
    if (nvp)
    {
        pthread_join(decode, NULL);
        delete nvp;
    }

    paused = false;
    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;
    speed_index = 0;

    nvp = NULL;
    osd = NULL;
 
    playbackinfo = NULL;
 
    if (recorder) 
        delete recorder; 

    recorder = activerecorder = NULL;
 
    if (prbuffer)
    {
        delete prbuffer;
        prbuffer = NULL;
    }
}

void TV::TeardownPipPlayer(void)
{
    if (pipnvp)
    {
        pthread_join(pipdecode, NULL);
        delete pipnvp;
    }
    pipnvp = NULL;

    if (piprecorder)
        delete piprecorder;
    piprecorder = NULL;

    delete piprbuffer;
    piprbuffer = NULL;
}

void *TV::EventThread(void *param)
{
    TV *thetv = (TV *)param;
    thetv->RunTV();

    return NULL;
}

void TV::RunTV(void)
{ 
    paused = false;
    int keypressed;

    stickykeys = gContext->GetNumSetting("StickyKeys");
    ff_rew_repos = gContext->GetNumSetting("FFRewRepos", 1);

    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;
    speed_index = 0;

    int updatecheck = 0;
    update_osd_pos = false;

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelKeys[4] = 0;
    channelkeysstored = 0;    

    runMainLoop = true;
    exitPlayer = false;

    while (runMainLoop)
    {
        if (changeState)
            HandleStateChange();

        usleep(1000);

        if (getRecorderPlaybackInfo)
        {
            if (recorderPlaybackInfo)
            {
                delete recorderPlaybackInfo;
                recorderPlaybackInfo = NULL;
            }

            recorder->Setup();

            if (recorder->IsRecording())
            {
                recorderPlaybackInfo = recorder->GetRecording();
                RemoteFillProginfo(recorderPlaybackInfo, 
                                   gContext->GetHostName());
            }

            getRecorderPlaybackInfo = false;
        }

        if (nvp)
        {
            if (keyList.size() > 0)
            { 
                keyListLock.lock();
                keypressed = keyList.front();
                keyList.pop_front();
                keyListLock.unlock();

                ProcessKeypress(keypressed);
            }
            else
                RepeatFFRew();
        }

        if (StateIsPlaying(internalState))
        {
            if (!nvp->IsPlaying())
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
                endOfRecording = true;
                VERBOSE(VB_PLAYBACK, ">> Player timeout");
            }
        }

        if (exitPlayer)
        {
            while(osd->DialogShowing(dialogname))
            {
                osd->DialogAbort(dialogname);
                usleep(500);
            }
            nextState = kState_None;
            changeState = true;
            exitPlayer = false;
        }

        if (++updatecheck >= 20)
        {
            if (osd && osd->Visible() && update_osd_pos &&
                (internalState == kState_WatchingLiveTV || 
                 internalState == kState_WatchingRecording ||
                 internalState == kState_WatchingPreRecorded))
            {
                QString desc = "";
                int pos = nvp->calcSliderPos(0, desc);
                osd->UpdatePause(pos, desc);
            }

            updatecheck = 0;
        }

        if (internalState == kState_WatchingLiveTV ||
            internalState == kState_WatchingRecording)
        {
            if (channelqueued && nvp->GetOSD() && !osd->Visible())
                ChannelCommit();
        }
    }
  
    nextState = kState_None;
    HandleStateChange();
}

bool TV::eventFilter(QObject *o, QEvent *e)
{
    (void)o;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *k = (QKeyEvent *)e;
  
            // can't process these events in the Qt event loop. 
            keyListLock.lock();
            keyList.push_back(k->key());
            keyListLock.unlock();

            return true;
        }
        case QEvent::Paint:
        {
            if (nvp)
                nvp->ExposeEvent();
            return true;
        }
        default:
            return false;
    }
}

void TV::ProcessKeypress(int keypressed)
{
    bool was_doing_ff_rew = false;

    if (editmode)
    {   
        nvp->DoKeypress(keypressed);
        if (keypressed == Key_Escape || 
            keypressed == Key_E || keypressed == Key_M)
            editmode = nvp->GetEditMode();
        return;
    }

    if (browsemode)
    {
        int passThru = 0;
        switch (keypressed)
        {
            case Key_Up: BrowseDispInfo(BROWSE_UP); break;
            case Key_Down: BrowseDispInfo(BROWSE_DOWN); break;
            case Key_Left: BrowseDispInfo(BROWSE_LEFT); break;
            case Key_Right: BrowseDispInfo(BROWSE_RIGHT); break;

            case Key_Escape: BrowseEnd(false); break;

            case Key_Space: case Key_Enter:
            case Key_Return: BrowseEnd(true); break;

            case Key_R: BrowseToggleRecord(); break;

            case Key_BracketLeft: case Key_BracketRight:
            case Key_Bar: passThru = 1; break;

            case Key_W: passThru = 1; break;
        }

        if (!passThru)
            return;
    }

    if (nvp->GetOSD() && osd->DialogShowing(dialogname))
    {
        switch (keypressed)
        {
            case Key_Up: osd->DialogUp(dialogname); break;
            case Key_Down: osd->DialogDown(dialogname); break;
            case Key_Escape: osd->DialogAbort(dialogname);
                // fall through
            case Key_Space: case Key_Enter: case Key_Return: 
            {
                osd->TurnDialogOff(dialogname);
                if (dialogname == "alreadybeingedited")
                {
                    int result = osd->GetDialogResponse(dialogname);
                    dialogname = "";
                    if (result == 1) 
                    {
                       playbackinfo->SetEditing(false, m_db);
                       editmode = nvp->EnableEdit();
                    }
                }
                if (dialogname == "exitplayoptions") 
                {
                    int result = osd->GetDialogResponse(dialogname);
                    dialogname = "";

                    switch (result)
                    {
                        case 1:
                            nvp->SetBookmark();
                            wantsToQuit = true;
                            exitPlayer = true;
                            break;
                        case 3: case 0:
                            nvp->Unpause();
                            break;
                        case 4:
                            wantsToQuit = true;
                            exitPlayer = true;
                            requestDelete = true;
                            break;
                        default:
                            wantsToQuit = true;
                            exitPlayer = true;
                            break;
                    }
                }
                break;
            }
            default: break;
        }
        
        return;
    }

    switch (keypressed) 
    {
        case Key_T:
        {
            nvp->ToggleCC();
            break;
        }
        case Key_Z:
        {
            DoSkipCommercials(1);
            break;
        }
        case Key_X:
        {
            DoQueueTranscode();
            break;
        }
        case Key_Q:
        {
            DoSkipCommercials(-1);
            break;
        }
        case Key_S: case Key_P: 
        {
            DoPause();
            break;
        }
        case Key_U:
        {
            ChangeSpeed(1);
            break;
        }
        case Key_J:
        {
            ChangeSpeed(-1);
            break;
        }
        case Key_F:
        {
            DoTogglePictureAttribute();
            break;
        }
        case Key_Right: case Key_D: 
        {
            if (picAdjustment)
                DoChangePictureAttribute(true);
            else if (paused)
                DoSeek(1.001 / frameRate, tr("Forward"));
            else if (!stickykeys)
                DoSeek(fftime, tr("Skip Ahead"));
            else
                ChangeFFRew(1);
            break;
        }
        case Key_Greater: case Key_Period:
        {
            if (paused)
                DoSeek(1.0, tr("Forward"));
            else
                ChangeFFRew(1);
            break;
        }
        case Key_Left: case Key_A:
        {
            if (picAdjustment)
                DoChangePictureAttribute(false);
            else if (paused)
                DoSeek(-1.001 / frameRate, tr("Rewind"));
            else if (!stickykeys)
                DoSeek(-rewtime, tr("Skip Back"));
            else
                ChangeFFRew(-1);
            break;
        }
        case Key_Less: case Key_Comma:
        {
            if (paused)
                DoSeek(-1.0, tr("Rewind"));
            else
                ChangeFFRew(-1);
            break;
        }
        case Key_PageUp:
        {
            DoSeek(-jumptime * 60, tr("Jump Back"));
            break;
        }
        case Key_PageDown:
        {
            DoSeek(jumptime * 60, tr("Jump Ahead"));
            break;
        }
        case Key_Escape:
        {
            StopFFRew();

            if (StateIsPlaying(internalState) && 
                gContext->GetNumSetting("PlaybackExitPrompt") == 1) 
            {
                nvp->Pause();

                QString message = tr("You are exiting this recording");

                QStringList options;
                options += tr("Save this position and go to the menu");
                options += tr("Do not save, just exit to the menu");
                options += tr("Keep watching");
                options += tr("Delete this recording");

                dialogname = "exitplayoptions";
                osd->NewDialogBox(dialogname, message, options, 0); 
            } 
            else 
            {
                if (gContext->GetNumSetting("PlaybackExitPrompt") == 2)
                    nvp->SetBookmark();
                exitPlayer = true;
                wantsToQuit = true;
                break;
            }
            break;
        }
        case Key_BracketLeft: case Key_F10: ChangeVolume(false); break;
        case Key_BracketRight: case Key_F11: ChangeVolume(true); break;
        case Key_Bar: case Key_F9: ToggleMute(); break;
        case Key_W: ToggleLetterbox(); break;

        default: 
        {
            if (doing_ff_rew)
            {
                switch (keypressed)
                {
                    case Key_0: ff_rew_index = SSPEED_NORMAL_WITH_DISPLAY; break;
                    case Key_1: ff_rew_index = SSPEED_SLOW_1; break;
                    case Key_2: ff_rew_index = SSPEED_SLOW_2; break;
                    case Key_3: ff_rew_index = SSPEED_NORMAL; break;
                    case Key_4: ff_rew_index = SSPEED_FAST_1; break;
                    case Key_5: ff_rew_index = SSPEED_FAST_2; break;
                    case Key_6: ff_rew_index = SSPEED_FAST_3; break;
                    case Key_7: ff_rew_index = SSPEED_FAST_4; break;
                    case Key_8: ff_rew_index = SSPEED_FAST_5; break;
                    case Key_9: ff_rew_index = SSPEED_FAST_6; break;

                    default:
                       float time = StopFFRew();
                       UpdatePosOSD(time, tr("Play"));
                       was_doing_ff_rew = true;
                       break;
                }
            }
            if (speed_index)
            {
                NormalSpeed();
                UpdatePosOSD(0.0, tr("Play"));
                was_doing_ff_rew = true;
            }
            break;
        }
    }

    if (internalState == kState_WatchingLiveTV)
    {
        switch (keypressed)
        {
            case Key_I: ToggleOSD(); break;

            case Key_Up:
                     if (persistentbrowsemode)
                         BrowseDispInfo(BROWSE_UP);
                     else
                         ChangeChannel(CHANNEL_DIRECTION_UP);
                     break;
            case Key_Down:
                     if (persistentbrowsemode)
                         BrowseDispInfo(BROWSE_DOWN);
                     else
                         ChangeChannel(CHANNEL_DIRECTION_DOWN);
                     break;
            case Key_Slash: ChangeChannel(CHANNEL_DIRECTION_FAVORITE); break;

            case Key_Question: ToggleChannelFavorite(); break;

            case Key_C: ToggleInputs(); break;

            case Key_0: case Key_1: case Key_2: case Key_3: case Key_4:
            case Key_5: case Key_6: case Key_7: case Key_8: case Key_9:
                     ChannelKey(keypressed); break;

            case Key_Space: case Key_Enter: case Key_Return: 
                     ChannelCommit(); break;

            case Key_M: LoadMenu(); break;

            case Key_V: TogglePIPView(); break;
            case Key_B: ToggleActiveWindow(); break;
            case Key_N: SwapPIP(); break;

            case Key_F1: ChangeContrast(false, true); break;
            case Key_F2: ChangeContrast(true, true); break;
            case Key_F3: ChangeBrightness(false, true); break;
            case Key_F4: ChangeBrightness(true, true); break;
            case Key_F5: ChangeColour(false, true); break;
            case Key_F6: ChangeColour(true, true); break;
            case Key_F7: ChangeHue(false, true); break;
            case Key_F8: ChangeHue(true, true); break;

            case Key_O: BrowseStart(); break;
            case Key_H: PreviousChannel(); break;

            default: break;
        }
    }
    else if (StateIsPlaying(internalState))
    {
        switch (keypressed)
        {
            case Key_I: DoInfo(); break;
            case Key_Space: case Key_Enter: case Key_Return: 
            {
                if (!was_doing_ff_rew)
                {
                    if (gContext->GetNumSetting("AltClearSavedPosition", 1)
                        && nvp->GetBookmark())
                        nvp->ClearBookmark(); 
                    else
                        nvp->SetBookmark(); 
                }
                break;
            }
            case Key_E: case Key_M: 
            {
               if (playbackinfo->IsEditing(m_db))
               {
                   nvp->Pause();

                   dialogname = "alreadybeingedited";

                   QString message = tr("This program is currently being edited");

                   QStringList options;
                   options += tr("Continue Editing");
                   options += tr("Do not edit");

                   osd->NewDialogBox(dialogname, message, options, 0); 
                   break;
               }
            editmode = nvp->EnableEdit();
            break;        
            }
            case Key_Up:
            {
                DoSeek(-jumptime * 60, tr("Jump Back"));
                break;
            }
            case Key_Down:
            {
                DoSeek(jumptime * 60, tr("Jump Ahead"));
                break;
            }
            default: break;
        }
    }
}

void TV::TogglePIPView(void)
{
    if (!pipnvp)
    {
        RemoteEncoder *testrec = RemoteRequestRecorder();
        
        if (!testrec)
            return;

        if (!testrec->IsValidRecorder())
        {
            delete testrec;
            return;
        }

        piprecorder = testrec;

        QString name = "";
        long long filesize = 0;
        long long smudge = 0;

        piprecorder->Setup();
        piprecorder->SetupRingBuffer(name, filesize, smudge, true);

        piprbuffer = new RingBuffer(name, filesize, smudge, piprecorder);

        piprecorder->SpawnLiveTV();

        while (!piprecorder->IsRecording())
            usleep(50);
    
        SetupPipPlayer();
        pthread_create(&pipdecode, NULL, SpawnDecode, pipnvp);

        while (!pipnvp->IsPlaying())
            usleep(50);

        nvp->SetPipPlayer(pipnvp);        
    }
    else
    {
        if (activenvp != nvp)
            ToggleActiveWindow();

        nvp->SetPipPlayer(NULL);
        while (!nvp->PipPlayerSet())
            usleep(50);

        piprbuffer->StopReads();
        piprbuffer->Pause();
        while (!piprbuffer->isPaused())
            usleep(50);

        pipnvp->StopPlaying();

        piprecorder->StopLiveTV();

        TeardownPipPlayer();        
    }
}

void TV::ToggleActiveWindow(void)
{
    if (!pipnvp)
        return;

    if (activenvp == nvp)
    {
        activenvp = pipnvp;
        activerbuffer = piprbuffer;
        activerecorder = piprecorder;
    }
    else
    {
        activenvp = nvp;
        activerbuffer = prbuffer;
        activerecorder = recorder;
    }
}

void TV::SwapPIP(void)
{
    if (!pipnvp)
        return;

    QString dummy;
    QString pipchanname;
    QString bigchanname;

    pipchanname = piprecorder->GetCurrentChannel();
    bigchanname = recorder->GetCurrentChannel();

    if (activenvp != nvp)
        ToggleActiveWindow();

    ChangeChannelByString(pipchanname);

    ToggleActiveWindow();

    ChangeChannelByString(bigchanname);

    ToggleActiveWindow();
}

void TV::DoPause(void)
{
    NormalSpeed();
    float time = StopFFRew();

    paused = activenvp->TogglePause();

    if (activenvp != nvp)
        return;

    if (paused)
        UpdatePosOSD(time, tr("Paused"));
    else
        osd->EndPause();
}

void TV::DoInfo(void)
{
    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath;
    OSDSet *oset;

    if (paused)
        return;

    oset = osd->GetSet("status");
    if ((oset) && (oset->Displaying()))
    {
        oset->Display(false);

        QMap<QString, QString> regexpMap;
        playbackinfo->ToMap(m_db, regexpMap);
        osd->ClearAllText("program_info");
        osd->SetTextByRegexp("program_info", regexpMap, osd_display_time);
    }
    else
    {
        oset = osd->GetSet("program_info");
        if ((oset) && (oset->Displaying()))
            oset->Display(false);

        QString desc = "";
        int pos = nvp->calcSliderPos(0, desc);
        osd->StartPause(pos, false, tr("Position"), desc, osd_display_time);
        update_osd_pos = true;
    }
}

bool TV::UpdatePosOSD(float time, const QString &mesg)
{
    bool muted = false;

    if (volumeControl && !volumeControl->GetMute())
    {
        volumeControl->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        QString desc = "";
        int pos = nvp->calcSliderPos(time, desc);
        bool slidertype = (internalState == kState_WatchingLiveTV);
        int disptime = (mesg == tr("Paused")) ? -1 : 2;
        osd->StartPause(pos, slidertype, mesg, desc, disptime);
        update_osd_pos = true;
    }

    bool res;

    if (time > 0.0)
        res = activenvp->FastForward(time);
    else
        res = activenvp->Rewind(-time);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);

    return res;
}

void TV::DoSeek(float time, const QString &mesg)
{
    if (!keyRepeat)
        return;

    NormalSpeed();
    time += StopFFRew();
    UpdatePosOSD(time, mesg);

    if (activenvp->GetLimitKeyRepeat())
    {
        keyRepeat = false;
        keyrepeatTimer->start(300, true);
    }
}

void TV::NormalSpeed(void)
{
    if (!speed_index)
        return;

    speed_index = 0;
    activenvp->SetPlaySpeed(1.0);
}

void TV::ChangeSpeed(int direction)
{
    int old_speed = speed_index;

    if (paused)
        speed_index = -4;

    speed_index += direction;

    float time = StopFFRew();
    float speed;
    QString mesg;

    switch (speed_index)
    {
        case  3: speed = 5.0;      mesg = QString(tr("Speed 5X"));    break;
        case  2: speed = 3.0;      mesg = QString(tr("Speed 3X"));    break;
        case  1: speed = 2.0;      mesg = QString(tr("Speed 2X"));    break;
        case  0: speed = 1.0;      mesg = QString(tr("Play"));        break;
        case -1: speed = 1.0 / 3;  mesg = QString(tr("Speed 1/3X"));  break;
        case -2: speed = 1.0 / 8;  mesg = QString(tr("Speed 1/8X"));  break;
        case -3: speed = 1.0 / 16; mesg = QString(tr("Speed 1/16X")); break;
        case -4: DoPause(); return; break;
        default: speed_index = old_speed; return; break;
    }

    activenvp->SetPlaySpeed(speed);
    UpdatePosOSD(time, mesg);

    if (paused)
        paused = activenvp->TogglePause();
}

float TV::StopFFRew(void)
{
    float time = 0.0;

    if (!doing_ff_rew)
        return time;

    if (ff_rew_repos)
    {
        if (doing_ff_rew > 0)
            time = -seek_speed_array[ff_rew_index].ff_repos;
        else
            time = seek_speed_array[ff_rew_index].rew_repos;
    }

    doing_ff_rew = 0;
    ff_rew_index = SSPEED_NORMAL;

    return time;
}

void TV::ChangeFFRew(int direction)
{
    if (doing_ff_rew == direction)
        ff_rew_index = (++ff_rew_index % SSPEED_MAX);
    else
    {
        NormalSpeed();

        doing_ff_rew = direction;
        ff_rew_index = SSPEED_NORMAL;

        if (paused)
            paused = activenvp->TogglePause();
    }
}

void TV::RepeatFFRew(void)
{
    if (!doing_ff_rew)
        return;

    float time;
    QString mesg;
    if (doing_ff_rew > 0)
    {
        time = seek_speed_array[ff_rew_index].scaling;
        mesg = tr("Forward ") + seek_speed_array[ff_rew_index].dispString;
    }
    else
    {
        time = -seek_speed_array[ff_rew_index].scaling;
        mesg = tr("Rewind ") + seek_speed_array[ff_rew_index].dispString;
    }

    if (UpdatePosOSD(time, mesg))
    {
        StopFFRew();
        UpdatePosOSD(time, tr("Play"));
    }
    else if (ff_rew_index > SSPEED_NORMAL)
        usleep(50000);
}

void TV::DoQueueTranscode(void)
{
    if (internalState == kState_WatchingPreRecorded)
    {
        bool stop = false, abort = false;
        if (queuedTranscode)
            stop = true;
        else
        {
            QString query = QString("SELECT * FROM transcoding WHERE "
                                    "chanid = '%1' AND starttime = '%2';")
                                   .arg(playbackinfo->chanid)
                         .arg(playbackinfo->startts.toString("yyyyMMddhhmmss"));
            MythContext::KickDatabase(m_db);
            QSqlQuery result = m_db->exec(query);
            if (result.isActive() && result.numRowsAffected() > 0)
            {
                stop = true;
                abort = true;
            }
        }
        if (stop)
        {
            RemoteQueueTranscode(playbackinfo, TRANSCODE_STOP);
            queuedTranscode = false;
            if (activenvp == nvp)
                osd->SetSettingsText(tr("Stopping Transcode"), 3);
        }
        else
        {
            RemoteQueueTranscode(playbackinfo, TRANSCODE_QUEUED |
                                               TRANSCODE_USE_CUTLIST);
            queuedTranscode = true;
            if (activenvp == nvp)
                osd->SetSettingsText(tr("Transcoding"), 3);
        }
    }
}

void TV::DoSkipCommercials(int direction)
{
    NormalSpeed();
    StopFFRew();

    bool muted = false;

    if (volumeControl && !volumeControl->GetMute())
    {
        volumeControl->ToggleMute();
        muted = true;
    }

    bool slidertype = false;
    if (internalState == kState_WatchingLiveTV)
        slidertype = true;

    if (activenvp == nvp)
    {
        QString dummy = "";
        QString desc = tr("Searching...");
        int pos = nvp->calcSliderPos(0, dummy);
        osd->StartPause(pos, slidertype, tr("Skip"), desc, 6);
        update_osd_pos = false;
    }

    activenvp->SkipCommercials(direction);

    if (muted) 
        muteTimer->start(kMuteTimeout, true);
}

void TV::ToggleInputs(void)
{
    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause(false);

    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ToggleInputs();

    activenvp->ResetPlaying();
    activenvp->Unpause(false);

    if (activenvp == nvp)
        UpdateOSDInput();
}

void TV::ToggleChannelFavorite(void)
{
    activerecorder->ToggleChannelFavorite();
}

void TV::ChangeChannel(int direction)
{
    bool muted = false;

    if (volumeControl && !volumeControl->GetMute() && activenvp == nvp)
    {
        volumeControl->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause(false);
    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();    

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0 && activenvp == nvp)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->ChangeChannel(direction);

    activenvp->ResetPlaying();
    activenvp->Unpause(false);

    if (activenvp == nvp)
    {
        UpdateOSD();
        AddPreviousChannel();
    }

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelkeysstored = 0;

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);
}

void TV::ChannelKey(int key)
{
    char thekey = key;

    if (key > 256)
        thekey = key - 256 - 0xb0 + '0';

    if (channelkeysstored == 4)
    {
        channelKeys[0] = channelKeys[1];
        channelKeys[1] = channelKeys[2];
        channelKeys[2] = channelKeys[3];
        channelKeys[3] = thekey;
    }
    else
    {
        channelKeys[channelkeysstored] = thekey; 
        channelkeysstored++;
    }
    channelKeys[4] = 0;

    if (activenvp == nvp && osd)
    {
        QMap<QString, QString> regexpMap;

        regexpMap["channum"] = channelKeys;
        regexpMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetTextByRegexp("channel_number", regexpMap, 2);
    }

    channelqueued = true;
}

void TV::ChannelCommit(void)
{
    if (!channelqueued)
        return;

    for (int i = 0; i < channelkeysstored; i++)
    {
        if (channelKeys[i] == '0')
            channelKeys[i] = ' ';
        else
            break;
    }

    QString chan = QString(channelKeys).stripWhiteSpace();
    ChangeChannelByString(chan);

    channelqueued = false;
    channelKeys[0] = channelKeys[1] = channelKeys[2] = channelKeys[3] = ' ';
    channelkeysstored = 0;
}

void TV::ChangeChannelByString(QString &name)
{
    bool muted = false;

    if (!channame_vector.empty() && channame_vector.back() == name)
        return;

    if (!activerecorder->CheckChannel(name))
        return;

    if (volumeControl && !volumeControl->GetMute() && activenvp == nvp)
    {
        volumeControl->ToggleMute();
        muted = true;
    }

    if (activenvp == nvp)
    {
        if (paused)
            osd->EndPause();
        paused = false;
    }

    activenvp->Pause(false);
    // all we care about is the ringbuffer being paused, here..
    activerbuffer->WaitForPause();

    // Save the current channel if this is the first time
    if (channame_vector.size() == 0)
        AddPreviousChannel();

    activerecorder->Pause();
    activerbuffer->Reset();
    activerecorder->SetChannel(name);

    activenvp->ResetPlaying();
    activenvp->Unpause(false);

    if (activenvp == nvp)
    {
        UpdateOSD();
        AddPreviousChannel();
    }

    if (muted)
        muteTimer->start(kMuteTimeout * 2, true);
}

void TV::AddPreviousChannel(void)
{
    // Don't store more than thirty channels.  Remove the first item
    if (channame_vector.size() > 29)
    {
        PrevChannelVector::iterator it;
        it = channame_vector.begin();
        channame_vector.erase(it);
    }

    QString chan_name = activerecorder->GetCurrentChannel();

    // This method builds the stack of previous channels
    channame_vector.push_back(chan_name);
}

void TV::PreviousChannel(void)
{
    // Save the channel if this is the first time, and return so we don't
    // change chan to the current chan
    if (channame_vector.size() == 0)
        return;

    // Increment the times_pressed counter so we know how far to jump
    times_pressed++;

    //Figure out the vector the desired channel is in
    int vector = (channame_vector.size() - times_pressed - 1) % 
                 channame_vector.size();

    // Display channel name in the OSD for up to 1 second.
    if (activenvp == nvp && osd)
    {
        osd->HideSet("program_info");

        QMap<QString, QString> regexpMap;

        regexpMap["channum"] = channame_vector[vector];
        regexpMap["callsign"] = "";
        osd->ClearAllText("channel_number");
        osd->SetTextByRegexp("channel_number", regexpMap, 1);
    }

    // Reset the timer
    prevChannelTimer->stop();
    prevChannelTimer->start(750);
}

void TV::SetPreviousChannel()
{
    // Stop the timer
    prevChannelTimer->stop();

    // Figure out the vector the desired channel is in
    int vector = (channame_vector.size() - times_pressed - 1) % 
                 channame_vector.size();

    // Reset the times_pressed counter
    times_pressed = 0;

    // Only change channel if channame_vector[vector] != current channel
    QString chan_name = activerecorder->GetCurrentChannel();

    if (chan_name != channame_vector[vector].latin1())
    {
        // Populate the array with the channel
        for(uint i = 0; i < channame_vector[vector].length(); i++)
        {
            channelKeys[i] = (int)*channame_vector[vector].mid(i, 1).latin1();
        }

        channelqueued = true;
    }

    //Turn off the OSD Channel Num so the channel changes right away
    if (activenvp == nvp && osd)
        osd->HideSet("channel_number");
}

void TV::ToggleOSD(void)
{
    if (osd->Visible())
    {
        osd->HideSet("program_info");
        osd->HideSet("channel_number");
    }
    else
        UpdateOSD();
}

void TV::UpdateOSD(void)
{
    QMap<QString, QString> regexpMap;

    GetChannelInfo(activerecorder, regexpMap);

    osd->ClearAllText("program_info");
    osd->SetTextByRegexp("program_info", regexpMap, osd_display_time);
    osd->ClearAllText("channel_number");
    osd->SetTextByRegexp("channel_number", regexpMap, osd_display_time);
}

void TV::UpdateOSDInput(void)
{
    QString dummy = "";
    QString name = "";

    activerecorder->GetInputName(name);

    osd->SetInfoText(name, dummy, dummy, dummy, dummy, dummy, dummy, dummy, 
                     osd_display_time);
}

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QMap<QString, QString> &regexpMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    starttime = regexpMap["dbstarttime"];
    chanid = regexpMap["chanid"];
    channum = regexpMap["channum"];

    enc->GetNextProgram(direction,
                        title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid);

    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    regexpMap["dbstarttime"] = starttime;
    regexpMap["dbendtime"] = endtime;
    regexpMap["title"] = title;
    regexpMap["subtitle"] = subtitle;
    regexpMap["description"] = description;
    regexpMap["category"] = category;
    regexpMap["callsign"] = callsign;
    regexpMap["starttime"] = startts.toString(tmFmt);
    regexpMap["startdate"] = startts.toString(dtFmt);
    regexpMap["endtime"] = endts.toString(tmFmt);
    regexpMap["enddate"] = endts.toString(dtFmt);
    regexpMap["channum"] = channum;
    regexpMap["chanid"] = chanid;
    regexpMap["iconpath"] = iconpath;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    regexpMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    regexpMap["lentime"] = length;
}

void TV::GetNextProgram(RemoteEncoder *enc, int direction,
                        QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetNextProgram(direction,
                        title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid);
}

void TV::GetChannelInfo(RemoteEncoder *enc, QMap<QString, QString> &regexpMap)
{
    if (!enc)
        enc = activerecorder;

    QString title, subtitle, description, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    enc->GetChannelInfo(title, subtitle, description, category, starttime,
                        endtime, callsign, iconpath, channum, chanid);

    QString tmFmt = gContext->GetSetting("TimeFormat");
    QString dtFmt = gContext->GetSetting("ShortDateFormat");
    QDateTime startts = QDateTime::fromString(starttime, Qt::ISODate);
    QDateTime endts = QDateTime::fromString(endtime, Qt::ISODate);
    QString length;
    int hours, minutes, seconds;

    regexpMap["dbstarttime"] = starttime;
    regexpMap["dbendtime"] = endtime;
    regexpMap["title"] = title;
    regexpMap["subtitle"] = subtitle;
    regexpMap["description"] = description;
    regexpMap["category"] = category;
    regexpMap["callsign"] = callsign;
    regexpMap["starttime"] = startts.toString(tmFmt);
    regexpMap["startdate"] = startts.toString(dtFmt);
    regexpMap["endtime"] = endts.toString(tmFmt);
    regexpMap["enddate"] = endts.toString(dtFmt);
    regexpMap["channum"] = channum;
    regexpMap["chanid"] = chanid;
    regexpMap["iconpath"] = iconpath;

    seconds = startts.secsTo(endts);
    minutes = seconds / 60;
    regexpMap["lenmins"] = QString("%1").arg(minutes);
    hours   = minutes / 60;
    minutes = minutes % 60;
    length.sprintf("%d:%02d", hours, minutes);
    regexpMap["lentime"] = length;
}

void TV::GetChannelInfo(RemoteEncoder *enc, QString &title, QString &subtitle, 
                        QString &desc, QString &category, QString &starttime, 
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid)
{
    if (!enc)
        enc = activerecorder;

    enc->GetChannelInfo(title, subtitle, desc, category, starttime, endtime, 
                        callsign, iconpath, channelname, chanid);
}

void TV::EmbedOutput(unsigned long wid, int x, int y, int w, int h)
{
    embedid = wid;
    embx = x;
    emby = y;
    embw = w;
    embh = h;

    if (nvp)
        nvp->EmbedInWidget(wid, x, y, w, h);
}

void TV::StopEmbeddingOutput(void)
{
    if (nvp)
        nvp->StopEmbedding();
    embedid = 0;
}

void TV::doLoadMenu(void)
{
    QString dummy;
    QString channame = "3";

    if (activerecorder)
       channame = activerecorder->GetCurrentChannel();

    QString chanstr = RunProgramGuide(channame, true, this);

    if (chanstr != "")
    {
        chanstr = chanstr.left(4);
        sprintf(channelKeys, "%s", chanstr.ascii());
        channelqueued = true; 
    }

    StopEmbeddingOutput();

    menurunning = false;
}

void *TV::MenuHandler(void *param)
{
    TV *obj = (TV *)param;
    obj->doLoadMenu();

    return NULL;
}

void TV::LoadMenu(void)
{
    if (menurunning == true)
        return;
    menurunning = true;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, TV::MenuHandler, this);
}

void TV::ChangeBrightness(bool up, bool recorder)
{
    int brightness;
    int osdPauseType;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            brightness = activerecorder->ChangeBrightness(up);
            osdPauseType =  kOSDFunctionalType_Default;
            text = QString(tr("Brightness (REC) %1 %")).arg(brightness);
        }
        else
        {
            brightness = nvp->getVideoOutput()->ChangeBrightness(up);
            gContext->SaveSetting("PlaybackBrightness", brightness);
            osdPauseType = kOSDFunctionalType_PictureAdjust;
            text = QString(tr("Brightness %1 %")).arg(brightness);
        }

        osd->StartPause(brightness * 10, true, tr("Adjust Picture"), text, 5, 
                        osdPauseType);
        update_osd_pos = false;
    }
}

void TV::ChangeContrast(bool up, bool recorder)
{
    int contrast;
    int osdPauseType;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            contrast = activerecorder->ChangeContrast(up);
            osdPauseType = kOSDFunctionalType_Default;
            text = QString(tr("Contrast (REC) %1 %")).arg(contrast);
        }
        else
        {
            contrast = nvp->getVideoOutput()->ChangeContrast(up);
            gContext->SaveSetting("PlaybackContrast", contrast);
            osdPauseType = kOSDFunctionalType_PictureAdjust;
            text = QString(tr("Contrast %1 %")).arg(contrast);
        }

        osd->StartPause(contrast * 10, true, tr("Adjust Picture"), text, 5, 
                        osdPauseType);
        update_osd_pos = false;
    }
}

void TV::ChangeColour(bool up, bool recorder)
{
    int colour;
    int osdPauseType;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            colour = activerecorder->ChangeColour(up);
            osdPauseType = kOSDFunctionalType_Default;
            text = QString(tr("Colour (REC) %1 %")).arg(colour);
        }
        else
        {
            colour = nvp->getVideoOutput()->ChangeColour(up);
            gContext->SaveSetting("PlaybackColour", colour);
            osdPauseType = kOSDFunctionalType_PictureAdjust;
            text = QString(tr("Colour %1 %")).arg(colour);
        }

        osd->StartPause(colour * 10, true, tr("Adjust Picture"), text, 5, 
                        osdPauseType);
        update_osd_pos = false;
    }
}

void TV::ChangeHue(bool up, bool recorder)
{
    int hue;
    int osdPauseType;
    QString text;

    if (osd)
    {
        if (recorder)
        {
            hue = activerecorder->ChangeHue(up);
            osdPauseType = kOSDFunctionalType_Default;
            text = QString(tr("Hue (REC) %1 %")).arg(hue);
        }
        else
        {
            hue = nvp->getVideoOutput()->ChangeHue(up);
            gContext->SaveSetting("PlaybackHue", hue);
            osdPauseType = kOSDFunctionalType_PictureAdjust;
            text = QString(tr("Hue %1 %")).arg(hue);
        }

        osd->StartPause(hue * 10, true, tr("Adjust Picture"), text, 5, 
                        osdPauseType);
        update_osd_pos = false;
    }
}

void TV::ChangeVolume(bool up)
{
    if (!volumeControl)
        return;

    if (up)
        volumeControl->AdjustCurrentVolume(2);
    else 
        volumeControl->AdjustCurrentVolume(-2);

    int curvol = volumeControl->GetCurrentVolume();
    QString text = QString(tr("Volume %1 %")).arg(curvol);

    if (osd && !browsemode)
    {
        osd->StartPause(curvol * 10, true, tr("Adjust Volume"), text, 5, 
                        kOSDFunctionalType_PictureAdjust);
        update_osd_pos = false;
    }
}

void TV::ToggleMute(void)
{
    if (!volumeControl)
        return;

    volumeControl->ToggleMute();
    bool muted = volumeControl->GetMute();

    QString text;

    if (muted)
        text = tr("Mute On");
    else
        text = tr("Mute Off");
 
    if (osd && !browsemode)
        osd->SetSettingsText(text, 5);
}

void TV::ToggleLetterbox(void)
{
    nvp->ToggleLetterbox();
    int letterbox = nvp->GetLetterbox();
    QString text;

    switch (letterbox)
    {
        case 2: text = tr("4:3 Zoom"); break;
        case 1: text = tr("16:9"); break;
        default: text = tr("4:3"); break;
    }

    if (osd && !browsemode )
        osd->SetSettingsText(text, 3);
}

void TV::EPGChannelUpdate(QString chanstr)
{
    if (chanstr != "")
    {
        chanstr = chanstr.left(4);
        sprintf(channelKeys, "%s", chanstr.ascii());
        channelqueued = true; 
    }
}

void TV::KeyRepeatOK(void)
{
    keyRepeat = true;
}

void TV::UnMute(void)
{
    // If muted, unmute
    if (volumeControl && volumeControl->GetMute())
        volumeControl->ToggleMute();
}

void TV::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (GetState() == kState_WatchingRecording &&
            message.left(14) == "DONE_RECORDING")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int filelen = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                nvp->SetWatchingRecording(false);
                nvp->SetLength(filelen);
                nextState = kState_WatchingPreRecorded;
                changeState = true;
            }
        }
        else if (GetState() == kState_WatchingLiveTV && 
                 message.left(14) == "ASK_RECORDING ")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();
            int timeuntil = tokens[2].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                menurunning = false;
                AskAllowRecording(me->ExtraData(), timeuntil);
            }
        }
        else if (GetState() == kState_WatchingLiveTV &&
                 message.left(11) == "QUIT_LIVETV")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            int cardnum = tokens[1].toInt();

            if (cardnum == recorder->GetRecorderNumber())
            {
                menurunning = false;
                wantsToQuit = false;
                exitPlayer = true;
            }
        }
    }
}

void TV::BrowseStart(void)
{
    if (activenvp != nvp)
        return;

    if (paused)
        return;

    OSDSet *oset = osd->GetSet("browse_info");
    if (!oset)
        return;

    browsemode = true;

    QString title, subtitle, desc, category, starttime, endtime;
    QString callsign, iconpath, channum, chanid;

    GetChannelInfo(activerecorder, title, subtitle, desc, category, 
                   starttime, endtime, callsign, iconpath, channum, chanid);

    browsechannum = channum;
    browsechanid = chanid;
    browsestarttime = starttime;

    BrowseDispInfo(BROWSE_SAME);

    browseTimer->start(30000, true);
}

void TV::BrowseEnd(bool change)
{
    if (!browsemode)
        return;

    browseTimer->stop();

    osd->HideSet("browse_info");

    if (change)
    {
        ChangeChannelByString(browsechannum);
    }

    browsemode = false;
}

void TV::BrowseDispInfo(int direction)
{
    if (!browsemode)
        BrowseStart();

    QDateTime curtime = QDateTime::currentDateTime();
    QDateTime maxtime = curtime.addSecs(60 * 60 * 4);
    QDateTime lastprogtime =
                  QDateTime::fromString(browsestarttime, Qt::ISODate);
    QMap<QString, QString> regexpMap;

    if (paused)
        return;

    browseTimer->changeInterval(30000);

    if (lastprogtime < curtime)
        browsestarttime = curtime.toString("yyyyMMddhhmm") + "00";

    if ((lastprogtime > maxtime) &&
        (direction == BROWSE_RIGHT))
        return;

    regexpMap["channum"] = browsechannum;
    regexpMap["dbstarttime"] = browsestarttime;
    regexpMap["chanid"] = browsechanid;

    if (direction != BROWSE_SAME)
        GetNextProgram(activerecorder, direction, regexpMap);
    else
        GetChannelInfo(activerecorder, regexpMap);

    browsechannum = regexpMap["channum"];
    browsechanid = regexpMap["chanid"];
    browsestarttime = regexpMap["dbstarttime"];

    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);
    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(m_db,
                                                                  browsechanid,
                                                                  startts);
    if (program_info)
        program_info->ToMap(m_db, regexpMap);

    osd->ClearAllText("browse_info");
    osd->SetTextByRegexp("browse_info", regexpMap, -1);

    delete program_info;
}

void TV::BrowseToggleRecord(void)
{
    if (!browsemode)
        return;

    QMap<QString, QString> regexpMap;
    QDateTime startts = QDateTime::fromString(browsestarttime, Qt::ISODate);

    ProgramInfo *program_info = ProgramInfo::GetProgramAtDateTime(m_db,
                                                                  browsechanid,
                                                                  startts);
    program_info->ToggleRecord(m_db);

    program_info->ToMap(m_db, regexpMap);

    osd->ClearAllText("browse_info");
    osd->SetTextByRegexp("browse_info", regexpMap, -1);

    delete program_info;
}

void TV::HandleOSDClosed(int osdType)
{
    switch (osdType)
    {
        case kOSDFunctionalType_PictureAdjust:
            picAdjustment = 0;
            break;
        case kOSDFunctionalType_Default:
            break;
    }
}

void TV::DoTogglePictureAttribute(void)
{
    OSDSet *oset;
    int value = 0;
    oset = osd->GetSet("status");

    picAdjustment = (picAdjustment % 5) + 1;

    if (osd)
    {
        char *title = "Adjust Picture";
        QString picName;

        switch (picAdjustment)
        {
            case 1:
                value = (volumeControl) ? (volumeControl->GetCurrentVolume()) 
                        : 99; 
                title = "Adjust Volume";
                picName = QString(tr("Volume %1 %")).arg(value);
                break;
            case 2:
                value = nvp->getVideoOutput()->GetCurrentBrightness();
                picName = QString(tr("Brightness %1 %")).arg(value);
                break;
            case 3:
                value = nvp->getVideoOutput()->GetCurrentContrast();
                picName = QString(tr("Contrast %1 %")).arg(value);
                break;
            case 4:
                value = nvp->getVideoOutput()->GetCurrentColour();
                picName = QString(tr("Colour %1 %")).arg(value);
                break;
            case 5:
                value = nvp->getVideoOutput()->GetCurrentHue();
                picName = QString(tr("Hue %1 %")).arg(value);
                break;
        }
        osd->StartPause(value*10, true, tr(title), picName, 5, 
                        kOSDFunctionalType_PictureAdjust);
        update_osd_pos = false;
    }
}

void TV::DoChangePictureAttribute(bool up)
{
    switch (picAdjustment)
    {
        case 1:
            ChangeVolume(up);
            break;
        case 2:
            ChangeBrightness(up, false);
            break;
        case 3:
            ChangeContrast(up, false);
            break;
        case 4:
            ChangeColour(up, false);
            break;
        case 5:
            ChangeHue(up, false);
            break;
    }
}
