#include <qapplication.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>
#include <qsocket.h>

#include <iostream>
using namespace std;

#include "tv_rec.h"
#include "osd.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "recordingprofile.h"
#include "util.h"
#include "programinfo.h"
#include "recorderbase.h"
#include "NuppelVideoRecorder.h"
#include "hdtvrecorder.h"
#include "NuppelVideoPlayer.h"
#include "channel.h"
#include "mythdbcon.h"
#include "jobqueue.h"
#include "scheduledrecording.h"

#ifdef USING_IVTV
#include "mpegrecorder.h"
#endif

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbrecorder.h"
#endif

void *SpawnEncode(void *param)
{
    RecorderBase *nvr = (RecorderBase *)param;
    nvr->StartRecording();
    return NULL;
}

TVRec::TVRec(int capturecardnum) 
{
    db_conn = NULL;
    channel = NULL;
    rbuffer = NULL;
    encode = static_cast<pthread_t>(0);
    nvr = NULL;
    readthreadSock = NULL;
    readthreadlive = false;
    m_capturecardnum = capturecardnum;
    ispip = false;
    curRecording = NULL;
    pendingRecording = NULL;
    profileName = "";
    autoTranscode = 0;

    pthread_mutex_init(&db_lock, NULL);

    ConnectDB(capturecardnum);

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");

    audiosamplerate = -1;
    skip_btaudio = false;

    QString inputname, startchannel;

    GetDevices(capturecardnum, videodev, vbidev, audiodev, audiosamplerate,
               inputname, startchannel, cardtype, dvb_options, skip_btaudio);

    if (cardtype == "DVB")
    {
#ifdef USING_DVB
        channel = new DVBChannel(videodev.toInt(), this);
        channel->Open();
        if (inputname.isEmpty())
            channel->SetChannelByString(startchannel);
        else
            channel->SwitchToInput(inputname, startchannel);
        channel->SetChannelOrdering(chanorder);
        // don't close this channel, otherwise we cannot read data
        if (dvb_options.dvb_on_demand) 
            ((DVBChannel *)channel)->CloseDVB();
#else
        VERBOSE(VB_IMPORTANT, "ERROR: DVB Card configured, "
                              "but no DVB support compiled in!");
        VERBOSE(VB_IMPORTANT, "Remove the card from configuration, "
                              "or recompile MythTV.");
        exit(-20);
#endif
    }
    else if ((cardtype == "MPEG") && (videodev.lower().left(5) == "file:"))
    {
        channel = NULL;
    }
    else // "V4L" or "MPEG", ie, analog TV, or "HDTV"
    {
        Channel *achannel = new Channel(this, videodev, (cardtype == "HDTV"));
        channel = achannel; // here for SetFormat()->RetrieveInputChannels()
        channel->Open();
        achannel->SetFormat(gContext->GetSetting("TVFormat"));
        achannel->SetDefaultFreqTable(gContext->GetSetting("FreqTable"));
        if (inputname.isEmpty())
            channel->SetChannelByString(startchannel);
        else
            channel->SwitchToInput(inputname, startchannel);
        channel->SetChannelOrdering(chanorder);
        channel->Close();
    }
}

void TVRec::Init(void)
{
    inoverrecord = false;
    overrecordseconds = gContext->GetNumSetting("RecordOverTime");

    nvr = NULL;
    rbuffer = NULL;

    internalState = nextState = kState_None; 

    runMainLoop = false;
    changeState = false;

    askAllowRecording = false;
    recordPending = false;
    frontendReady = false;
    cancelNextRecording = false;

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);

    curRecording = NULL;
    prevRecording = NULL;
    pendingRecording = NULL;
}

TVRec::~TVRec(void)
{
    runMainLoop = false;
    pthread_join(event, NULL);

    if (channel)
        delete channel;
    if (rbuffer)
        delete rbuffer;
    if (nvr)
        delete nvr;
    if (db_conn)
        DisconnectDB();
}

TVState TVRec::GetState(void)
{
    if (changeState)
        return kState_ChangingState;
    return internalState;
}

ProgramInfo *TVRec::GetRecording(void)
{
    ProgramInfo *tmppginfo = NULL;

    if (curRecording && !changeState)
        tmppginfo = new ProgramInfo(*curRecording);
    else
        tmppginfo = new ProgramInfo();

    return tmppginfo;
}

void TVRec::RecordPending(ProgramInfo *rcinfo, int secsleft)
{
    if (pendingRecording)
        delete pendingRecording;

    pendingRecording = new ProgramInfo(*rcinfo);
    recordPendingStart = QDateTime::currentDateTime().addSecs(secsleft);
    recordPending = true;
    askAllowRecording = true;
}

int TVRec::StartRecording(ProgramInfo *rcinfo)
{
    int retval = 0; //  0 = recording canceled, 1 = recording started
                    // -1 = other state/already recording

    recordPending = false;
    askAllowRecording = false;

    QString recprefix = gContext->GetSetting("RecordFilePrefix");

    if (inoverrecord)
    {
        nextState = kState_None;
        changeState = true;

        while (changeState)
            usleep(50);
    }

    if (changeState)
    {
        VERBOSE(VB_RECORD, "backend still changing state, waiting..");
        while (changeState)
            usleep(50);
        VERBOSE(VB_RECORD, "changing state finished, starting now");
    }

    if (internalState == kState_WatchingLiveTV && !cancelNextRecording)
    {
        QString message = QString("QUIT_LIVETV %1").arg(m_capturecardnum);

        MythEvent me(message);
        gContext->dispatch(me);

        QTime timer;
        timer.start();

        while (internalState != kState_None && timer.elapsed() < 10000)
            usleep(100);

        if (internalState != kState_None)
        {
            gContext->dispatch(me);

            timer.restart();

            while (internalState != kState_None && timer.elapsed() < 10000)
                usleep(100);
        }

        if (internalState != kState_None)
        {
            exitPlayer = true;
            timer.restart();
            while (internalState != kState_None && timer.elapsed() < 5000)
                usleep(100);
        }
    }

    if (internalState == kState_None)
    {
        overrecordseconds = gContext->GetNumSetting("RecordOverTime");

        outputFilename = rcinfo->GetRecordFilename(recprefix);
        recordEndTime = rcinfo->recendts;
        curRecording = new ProgramInfo(*rcinfo);

        nextState = kState_RecordingOnly;
        changeState = true;
        retval = 1;
    }
    else if (!cancelNextRecording)
    {
        cerr << QDateTime::currentDateTime().toString() 
             << ":  wanted to record: " << endl;
        cerr << rcinfo->title << " " << rcinfo->chanid << " " 
             << rcinfo->startts.toString() << endl;
        cerr << "But current state is: " << internalState << endl;
        if (curRecording)
        {
            cerr << "currently: " << curRecording->title << " " 
                 << curRecording->chanid << " " 
                 << curRecording->startts.toString() << " " 
                 << curRecording->endts.toString() << endl;
        }

        retval = -1;
    }

    if (cancelNextRecording)
        cancelNextRecording = false;

    return retval;
}

void TVRec::StopRecording(void)
{
    if (StateIsRecording(internalState))
    {
        nextState = RemoveRecording(internalState);
        changeState = true;
        prematurelystopped = false;

        while (changeState)
            usleep(50);
    }
}

void TVRec::StateToString(TVState state, QString &statestr)
{
    switch (state) {
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded: statestr = "WatchingPreRecorded";
                                         break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
}

bool TVRec::StateIsRecording(TVState state)
{
    return (state == kState_RecordingOnly);
}

bool TVRec::StateIsPlaying(TVState state)
{
    return (state == kState_WatchingPreRecorded);
}

TVState TVRec::RemoveRecording(TVState state)
{
    if (StateIsRecording(state))
    {
        if (state == kState_RecordingOnly)
            return kState_None;
        else
        {
            cerr << "Unknown state in RemoveRecording: " << state << endl;
            return kState_Error;
        }
    }
    return kState_Error;
}

TVState TVRec::RemovePlaying(TVState state)
{
    if (StateIsPlaying(state))
    {
        if (state == kState_WatchingPreRecorded)
            return kState_None;
        return kState_RecordingOnly;
    }
    return kState_Error;
}

void TVRec::StartedRecording(void)
{
    if (!curRecording)
        return;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    curRecording->StartedRecording(db_conn);

    if (curRecording->chancommfree != 0)
        curRecording->SetCommFlagged(COMM_FLAG_COMMFREE, db_conn);

    pthread_mutex_unlock(&db_lock);

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
}

void TVRec::FinishedRecording(void)
{
    if (!curRecording)
        return;
    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    curRecording->FinishedRecording(db_conn, prematurelystopped);
    pthread_mutex_unlock(&db_lock);
}

void TVRec::HandleStateChange(void)
{
    TVState tmpInternalState = internalState;

    frontendReady = false;
    askAllowRecording = true;
    cancelNextRecording = false;

    bool changed = false;
    bool startRecorder = false;
    bool closeRecorder = false;
    bool killRecordingFile = false;

    QString statename;
    StateToString(nextState, statename);
    QString origname;
    StateToString(tmpInternalState, origname);

    if (nextState == kState_Error)
    {
        VERBOSE(VB_IMPORTANT, "TVRec: Attempting to set to an error state, exiting");
        exit(-21);
    }

    if (tmpInternalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        tmpInternalState = nextState;
        changed = true;

        startRecorder = true;
    }
    else if (tmpInternalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        if (channel)
            channel->StoreInputChannels();

        closeRecorder = true;

        tmpInternalState = nextState;
        changed = true;

        killRecordingFile = true;
    }
    else if (tmpInternalState == kState_None && 
             nextState == kState_RecordingOnly) 
    {
        SetChannel(true);  
        rbuffer = new RingBuffer(outputFilename, true);

        StartedRecording();

        tmpInternalState = nextState;
        nextState = kState_None;
        changed = true;

        startRecorder = true;
    }   
    else if (tmpInternalState == kState_RecordingOnly && 
             nextState == kState_None)
    {
        FinishedRecording();
        closeRecorder = true;
        tmpInternalState = nextState;
        changed = true;
        inoverrecord = false;
    }
    else if (tmpInternalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
    {
        VERBOSE(VB_IMPORTANT, QString("Unknown state transition: %1 to %2")
                .arg(origname).arg(statename));
    }
    else
    {
        VERBOSE(VB_GENERAL, QString("Changing from %1 to %2")
                .arg(origname).arg(statename));
    }
 
    if (startRecorder)
    {
        pthread_mutex_lock(&db_lock);

        MythContext::KickDatabase(db_conn);

        RecordingProfile profile;

        prematurelystopped = false;

        if (curRecording)
        {
            profileName = curRecording->GetScheduledRecording(
                                                db_conn)->getProfileName();
            bool foundProf = false;
            if (profileName != NULL)
                foundProf = profile.loadByCard(db_conn, profileName,
                                               m_capturecardnum);

            if (!foundProf)
            {
                profileName = QString("Default");
                profile.loadByCard(db_conn, profileName, m_capturecardnum);
            }
        }
        else
        {
            profileName = "Live TV";
            profile.loadByCard(db_conn, profileName, m_capturecardnum);
        }

        QString msg = QString("Using profile '%1' to record").arg(profileName);
        VERBOSE(VB_RECORD, msg);

        pthread_mutex_unlock(&db_lock);

        // Determine whether to automatically run the transcoder or not
        autoTranscode = profile.byName("autotranscode")->getValue().toInt();

        bool error = false;

        SetupRecorder(profile);
        if (channel != NULL)
        {
            // If there is a tuner make sure this is a valid station
            // hdtv will block for a long time if we try to get
            // mpeg ts packets from outer space.
            bool success = channel->Open();
            if (success)
            {
                success = channel->CheckSignalFull();
                if (!success)
                {
                    VERBOSE(VB_IMPORTANT, "Signal level too low?");
                    error = true;
                }
                channel->Close();
            } 
            else
                error = true;
        }

        if (!error)
        {
            nvr->SetRecording(curRecording);
            nvr->SetDB(db_conn, &db_lock);
            if (channel != NULL)
            {
                nvr->ChannelNameChanged(channel->GetCurrentName());

                SetVideoFiltersForChannel(channel, channel->GetCurrentName());
                if (channel->Open())
                {
                    channel->SetBrightness();
                    channel->SetContrast();
                    channel->SetColour();
                    channel->SetHue();
                    channel->Close();
                }
            }
            pthread_create(&encode, NULL, SpawnEncode, nvr);

            while (!nvr->IsRecording() && !nvr->IsErrored())
                usleep(50);
        } 
        else
            VERBOSE(VB_IMPORTANT, "Tuning Error -- aborting recording");

        if (nvr->IsRecording())
        {
            // evil.
            if (channel)
                channel->SetFd(nvr->GetVideoFd());
            frameRate = nvr->GetFrameRate();
        }
        else
        {
            if (error || nvr->IsErrored()) {
                VERBOSE(VB_IMPORTANT, "TVRec: Recording Prematurely Stopped");

                QString message = QString("QUIT_LIVETV %1").arg(m_capturecardnum);
                MythEvent me(message);
                gContext->dispatch(me);

                prematurelystopped = true;
            }
            FinishedRecording();
            killRecordingFile = true;
            closeRecorder = true;
            tmpInternalState = kState_None;
        }
    }

    if (closeRecorder)
    {
        TeardownRecorder(killRecordingFile);
        if (channel)
            channel->SetFd(-1);
    }

    internalState = tmpInternalState;
    changeState = false;
}

void TVRec::SetOption(RecordingProfile &profile, const QString &name)
{
    int value = profile.byName(name)->getValue().toInt();
    nvr->SetOption(name, value);
}

void TVRec::SetupRecorder(RecordingProfile &profile) 
{
    if (cardtype == "MPEG")
    {
#ifdef USING_IVTV
        nvr = new MpegRecorder();
        nvr->SetRingBuffer(rbuffer);

        nvr->SetOptionsFromProfile(&profile, videodev, audiodev, vbidev, ispip);

        nvr->Initialize();
#else
        cerr << "Compiled without ivtv support\n";
#endif
        return;
    }
    else if (cardtype == "HDTV")
    {
        rbuffer->SetWriteBufferSize(4*1024*1024);
        nvr = new HDTVRecorder();
        nvr->SetRingBuffer(rbuffer);

        nvr->SetOptionsFromProfile(&profile, videodev, audiodev, vbidev, ispip);

        nvr->Initialize();
        return;
    }
    else if (cardtype == "DVB")
    {
#ifdef USING_DVB
        nvr = new DVBRecorder(dynamic_cast<DVBChannel*>(channel));
        nvr->SetRingBuffer(rbuffer);

        nvr->SetOptionsFromProfile(&profile, videodev, audiodev, vbidev, ispip);

        nvr->SetOption("dvb_on_demand", dvb_options.dvb_on_demand);
        nvr->SetOption("swfilter", dvb_options.swfilter);
        nvr->SetOption("recordts", dvb_options.recordts);
        nvr->SetOption("wait_for_seqstart", dvb_options.wait_for_seqstart);
        nvr->SetOption("dmx_buf_size", dvb_options.dmx_buf_size);
        nvr->SetOption("pkt_buf_size", dvb_options.pkt_buf_size);
        nvr->SetOption("signal_monitor_interval", 
                       gContext->GetNumSetting("DVBMonitorInterval", 0));
        nvr->SetOption("expire_data_days",
                       gContext->GetNumSetting("DVBMonitorRetention", 3));
        nvr->Initialize();
#endif
        return;
    }

    // V4L/MJPEG from here on

    nvr = new NuppelVideoRecorder(channel);

    nvr->SetRingBuffer(rbuffer);

    nvr->SetOption("skipbtaudio", skip_btaudio);
    nvr->SetOptionsFromProfile(&profile, videodev, audiodev, vbidev, ispip);
 
    nvr->Initialize();
}

void TVRec::TeardownRecorder(bool killFile)
{
    QMap<long long, int> blank_frame_map;
    QString oldProfileName = profileName;

    int filelen = -1;

    if (curRecording)
        prevRecording = new ProgramInfo(*curRecording);
    else
        prevRecording = NULL;

    ispip = false;

    if (nvr)
    {
        filelen = (int)(((float)nvr->GetFramesWritten() / frameRate));

        QString message = QString("DONE_RECORDING %1 %2").arg(m_capturecardnum)
                                                         .arg(filelen);
        MythEvent me(message);
        gContext->dispatch(me);

        nvr->StopRecording();
        profileName = "";

        if (prevRecording && !killFile)
            nvr->GetBlankFrameMap(blank_frame_map);

        if (encode)
            pthread_join(encode, NULL);
        encode = static_cast<pthread_t>(0);
        delete nvr;
    }
    nvr = NULL;

    if (rbuffer)
    {
        rbuffer->StopReads();
        readthreadLock.lock();
        delete rbuffer;
        readthreadlive = false;
        rbuffer = NULL;
        readthreadLock.unlock();
    }

    if (killFile)
    {
        unlink(outputFilename.ascii());
        if (curRecording)
        {
            delete curRecording;
            curRecording = NULL;
        }
        outputFilename = "";
    }
    else if (curRecording)
    {
        delete curRecording;
        curRecording = NULL;

        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);
    }

    if ((prevRecording) && (!killFile))
    {
        pthread_mutex_lock(&db_lock);
        MythContext::KickDatabase(db_conn);
        prevRecording->SetBlankFrameList(blank_frame_map, db_conn);
        pthread_mutex_unlock(&db_lock);

        if (!prematurelystopped)
        {
            int jobTypes;

            pthread_mutex_lock(&db_lock);
            jobTypes = prevRecording->GetAutoRunJobs(db_conn);
            pthread_mutex_unlock(&db_lock);

            if (prevRecording->chancommfree)
                jobTypes = jobTypes & (~JOB_COMMFLAG);

            if (autoTranscode)
                jobTypes |= JOB_TRANSCODE;

            if (jobTypes)
            {
                QString jobHost = "";

                if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                    jobHost = gContext->GetHostName();

                pthread_mutex_lock(&db_lock);
                JobQueue::QueueJobs(db_conn, jobTypes,
                                    prevRecording->chanid,
                                    prevRecording->recstartts, jobHost);
                pthread_mutex_unlock(&db_lock);
            }
        }
    }

    if (prevRecording)
    {
        delete prevRecording;
        prevRecording = NULL;
    }
}    

char *TVRec::GetScreenGrab(ProgramInfo *pginfo, const QString &filename, 
                           int secondsin, int &bufferlen,
                           int &video_width, int &video_height)
{
    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    QString name = QString("screen%1%2").arg(getpid()).arg(rand());

    MythSqlDatabase *screendb = new MythSqlDatabase(name);

    if (!screendb || !screendb->isOpen())
        return NULL;

    NuppelVideoPlayer *nupvidplay = new NuppelVideoPlayer(screendb, pginfo);
    nupvidplay->SetRingBuffer(tmprbuf);
    nupvidplay->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));

    char *retbuf = nupvidplay->GetScreenGrab(secondsin, bufferlen, video_width,
                                             video_height);

    delete nupvidplay;
    delete tmprbuf;
    delete screendb;

    return retbuf;
}

void TVRec::SetChannel(bool needopen)
{
    if (needopen && channel)
        channel->Open();

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT channel.channum,cardinput.inputname "
                               "FROM channel,capturecard,cardinput WHERE "
                               "channel.chanid = %1 AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "cardinput.sourceid = %2 AND "
                               "capturecard.cardid = %3;")
                               .arg(curRecording->chanid)
                               .arg(curRecording->sourceid)
                               .arg(curRecording->cardid);

    QSqlQuery query = db_conn->exec(thequery);
    
    QString inputname = "";
    QString chanstr = "";
  
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        chanstr = query.value(0).toString();
        inputname = query.value(1).toString();
    } else {
        MythContext::DBError("SetChannel", query);
    }

    pthread_mutex_unlock(&db_lock);

    if (channel)
        channel->SwitchToInput(inputname, chanstr);

    if (needopen && channel)
        channel->Close();
}

void *TVRec::EventThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->RunTV();

    return NULL;
}

void TVRec::RunTV(void)
{ 
    paused = false;

    runMainLoop = true;
    exitPlayer = false;
    finishRecording = false;

    while (runMainLoop)
    {
        if (changeState)
            HandleStateChange();

        usleep(1000);

        if (recordPending && askAllowRecording && frontendReady)
        {
            askAllowRecording = false;

            int timeuntil = QDateTime::currentDateTime()
                                .secsTo(recordPendingStart);

            QString query = QString("ASK_RECORDING %1 %2").arg(m_capturecardnum)
                                                          .arg(timeuntil);
            QStringList messages;
            messages << pendingRecording->title
                    << pendingRecording->chanstr
                    << pendingRecording->chansign
                    << pendingRecording->channame;
            
            MythEvent me(query, messages);

            gContext->dispatch(me);
        }

        if (StateIsRecording(internalState))
        {
            if (QDateTime::currentDateTime() > recordEndTime || finishRecording)
            {
                if (!inoverrecord && overrecordseconds > 0)
                {
                    recordEndTime = recordEndTime.addSecs(overrecordseconds);
                    inoverrecord = true;
                    VERBOSE(VB_RECORD, QString("switching to overrecord for " 
                                               "%1 more seconds")
                                               .arg(overrecordseconds));
                }
                else
                {
                    nextState = RemoveRecording(internalState);
                    changeState = true;
                }
                finishRecording = false;
            }
        }

        if (exitPlayer)
        {
            if (internalState == kState_WatchingLiveTV)
            {
                nextState = kState_None;
                changeState = true;
            }
            else if (StateIsPlaying(internalState))
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
            }
            exitPlayer = false;
        }
    }
  
    nextState = kState_None;
    HandleStateChange();
}

void TVRec::GetChannelInfo(ChannelBase *chan, QString &title, QString &subtitle,
                           QString &desc, QString &category, 
                           QString &starttime, QString &endtime, 
                           QString &callsign, QString &iconpath, 
                           QString &channelname, QString &chanid,
                           QString &seriesid, QString &programid,
                           QString &outputFilters, QString &repeat, QString &airdate,
                           QString &stars)
{
    title = "";
    subtitle = "";
    desc = "";
    category = "";
    starttime = "";
    endtime = "";
    callsign = "";
    iconpath = "";
    channelname = "";
    chanid = "";
    seriesid = "";
    programid = "";
    outputFilters = "";
    repeat = "";
    airdate = "";
    stars = "";
    
    if (!db_conn)
        return;
    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    char curtimestr[128];
    time_t curtime;
    struct tm *loctime;

    if (!chan)
        return;

    curtime = time(NULL);
    loctime = localtime(&curtime);

    strftime(curtimestr, 128, "%Y%m%d%H%M%S", loctime);
   
    channelname = chan->GetCurrentName();
    QString channelinput = chan->GetCurrentInput();
 
    QString thequery = QString("SELECT starttime,endtime,title,subtitle,"
                               "description,category,callsign,icon,"
                               "channel.chanid, seriesid, programid, "
                               "channel.outputfilters, previouslyshown, originalairdate, stars "
                               "FROM program,channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" "
                               "AND starttime < %2 AND endtime > %3 AND "
                               "program.chanid = channel.chanid AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%4\" AND "
                               "capturecard.hostname = \"%5\";")
                               .arg(channelname).arg(curtimestr).arg(curtimestr)
                               .arg(m_capturecardnum).arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        starttime = query.value(0).toString();
        endtime = query.value(1).toString();
        title = QString::fromUtf8(query.value(2).toString());
        subtitle = QString::fromUtf8(query.value(3).toString());
        desc = QString::fromUtf8(query.value(4).toString());
        category = QString::fromUtf8(query.value(5).toString());
        callsign = QString::fromUtf8(query.value(6).toString());
        iconpath = query.value(7).toString();
        chanid = query.value(8).toString();
        seriesid = query.value(9).toString();
        programid = query.value(10).toString();
        outputFilters = query.value(11).toString();
        
        repeat = query.value(12).toString();
        airdate = query.value(13).toString();
        stars = query.value(14).toString();
    }
    else
    {
        // couldn't find a matching program for the current channel.
        // get the information about the channel anyway
        QString thequery = QString("SELECT callsign,icon, channel.chanid, "
                                   "channel.outputfilters "
                                   "FROM channel,capturecard,cardinput "
                                   "WHERE channel.channum = \"%1\" AND "
                                   "channel.sourceid = cardinput.sourceid AND "
                                   "cardinput.cardid = capturecard.cardid AND "
                                   "capturecard.cardid = \"%2\" AND "
                                   "capturecard.hostname = \"%3\";")
                                   .arg(channelname)
                                   .arg(m_capturecardnum)
                                   .arg(gContext->GetHostName());

        QSqlQuery query = db_conn->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            callsign = QString::fromUtf8(query.value(0).toString());
            iconpath = query.value(1).toString();
            chanid = query.value(2).toString();
            outputFilters = query.value(3).toString();
        }
     }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::ConnectDB(int cardnum)
{
    QString name = QString("TV%1%2").arg(cardnum).arg(rand());

    pthread_mutex_lock(&db_lock);

    db_conn = QSqlDatabase::addDatabase("QMYSQL3", name);
    if (!db_conn)
    {
        pthread_mutex_unlock(&db_lock);
        printf("Couldn't initialize mysql connection\n");
        return;
    }
    if (!gContext->OpenDatabase(db_conn))
    {
        printf("Couldn't open database\n");
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::DisconnectDB(void)
{
    pthread_mutex_lock(&db_lock);

    if (db_conn)
    {
        db_conn->close();
        delete db_conn;
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::GetDevices(int cardnum, QString &video, QString &vbi, 
                       QString &audio, int &rate, QString &defaultinput,
                       QString &startchan, QString &type, 
                       dvb_options_t &dvb_opts, bool &skip_bt)
{
    video = "";
    vbi = "";
    audio = "";
    defaultinput = "Television";
    startchan = "3";
    type = "V4L";

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT videodevice,vbidevice,audiodevice,"
                               "audioratelimit,defaultinput,cardtype,"
                               "dvb_swfilter, dvb_recordts,"
                               "dvb_wait_for_seqstart,dvb_dmx_buf_size,"
                               "dvb_pkt_buf_size, skipbtaudio, dvb_on_demand "
                               "FROM capturecard WHERE cardid = %1;")
                              .arg(cardnum);

    QSqlQuery query = db_conn->exec(thequery);

    int testnum = 0;

    QString test;
    if (!query.isActive())
        MythContext::DBError("getdevices", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            video = QString::fromUtf8(test);
        test = query.value(1).toString();
        if (test != QString::null)
            vbi = QString::fromUtf8(test);
        test = query.value(2).toString();
        if (test != QString::null)
            audio = QString::fromUtf8(test);
        testnum = query.value(3).toInt();
        if (testnum > 0)
            rate = testnum;
        else
            rate = -1;

        test = query.value(4).toString();
        if (test != QString::null)
            defaultinput = QString::fromUtf8(test);
        test = query.value(5).toString();
        if (test != QString::null)
            type = QString::fromUtf8(test);

        dvb_opts.swfilter = query.value(6).toInt();
        dvb_opts.recordts = query.value(7).toInt();
        dvb_opts.wait_for_seqstart = query.value(8).toInt();
        dvb_opts.dmx_buf_size = query.value(9).toInt();
        dvb_opts.pkt_buf_size = query.value(10).toInt();

        skip_bt = query.value(11).toInt();
        dvb_opts.dvb_on_demand = query.value(12).toInt();
    }

    thequery = QString("SELECT if(startchan!='', startchan, '3') "
                       "FROM capturecard,cardinput WHERE inputname = \"%1\" "
                       "AND capturecard.cardid = %2 "
                       "AND capturecard.cardid = cardinput.cardid;")
                       .arg(defaultinput).arg(cardnum);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getstartchan", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            startchan = QString::fromUtf8(test);
    }

    pthread_mutex_unlock(&db_lock);
}

bool TVRec::CheckChannel(QString name)
{
    if (!channel)
        return false;

    QSqlDatabase* dummy1;
    pthread_mutex_t* dummy2;
    QString dummyID;
    return CheckChannel(channel, name, dummy1, dummy2, dummyID);
}

bool TVRec::CheckChannel(ChannelBase *chan, const QString &channum, 
                         QSqlDatabase *&a_db_conn, pthread_mutex_t *&a_db_lock, QString& inputName)
{
    if (!chan)
        return false;

    if (!db_conn)
        return true;

    inputName = "";
    
    a_db_conn = db_conn;
    a_db_lock = &db_lock;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();

    QString thequery = QString("SELECT channel.chanid FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput)
                               .arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("checkchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        pthread_mutex_unlock(&db_lock);
        return true;
    }
    VERBOSE( VB_CHANNEL, QString("Failed to find channel(%1) on current input (%2) of card (%3).")
                         .arg(channum).arg(channelinput).arg(m_capturecardnum) );


    // We didn't find it on the current input let's widen the search
    thequery = QString("SELECT channel.chanid, cardinput.inputname FROM "
                       "channel,capturecard,cardinput "
                       "WHERE channel.channum = \"%1\" AND "
                       "channel.sourceid = cardinput.sourceid AND "
                       "cardinput.cardid = capturecard.cardid AND "
                       "capturecard.cardid = \"%2\" AND "
                       "capturecard.hostname = \"%3\";")
                       .arg(channum)
                       .arg(m_capturecardnum)
                       .arg(gContext->GetHostName());

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("checkchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        
        query.next();
        QString test = query.value(1).toString();
        if (test != QString::null)
            inputName = QString::fromUtf8(test);

        VERBOSE( VB_CHANNEL, QString("Found channel(%1) on another input (%2) of card (%3).")
                             .arg(channum).arg(inputName).arg(m_capturecardnum) );

        pthread_mutex_unlock(&db_lock);
        return true;
    }

    VERBOSE( VB_CHANNEL, QString("Failed to find channel(%1) on any input of card (%2).")
                         .arg(channum).arg(m_capturecardnum) );

                                                                  
    
    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    pthread_mutex_unlock(&db_lock);

    return ret;
}

/*
 * Returns true if name is either a valid channel name or a valid channel
 * prefix.  If name is a valid channel name and not a valid channel prefix
 * unique is set to true.
 * For example, if name == "36" and "36", "360", "361", "362", and "363" are
 * valid channel names, this function would return true but set *unique to
 * false.  However if name == "361" it would both return true and set *unique
 * to true.
 */
bool TVRec::CheckChannelPrefix(QString name, bool &unique)
{
    if (!channel)
        return false;

    if (!db_conn)
        return true;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    bool ret = false;
    unique = false;

    QString channelinput = channel->GetCurrentInput();

    QString thequery = QString("SELECT channel.chanid FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum LIKE \"%1%%\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(name).arg(channelinput)
                               .arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("checkchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        pthread_mutex_unlock(&db_lock);

        if (query.numRowsAffected() == 1)
        {
            unique = CheckChannel(name);
        }

        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0) 
    {
        unique = true;
        ret = true;
    }

    pthread_mutex_unlock(&db_lock);

    return ret;
}

bool TVRec::SetVideoFiltersForChannel(ChannelBase *chan, const QString &channum)
{
    if (!chan)
        return false;

    if (!db_conn)
        return true;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();
    QString videoFilters = "";

    QString thequery = QString("SELECT channel.videofilters FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput)
                               .arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("setvideofilterforchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        videoFilters = query.value(0).toString();

        if (nvr != NULL)
        {
            nvr->SetVideoFilters(videoFilters);
        }

        pthread_mutex_unlock(&db_lock);
        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    pthread_mutex_unlock(&db_lock);

    return ret;
}

int TVRec::GetChannelValue(const QString &channel_field, ChannelBase *chan, 
                           const QString &channum)
{
    if (!chan)
        return -1;

    int retval = -1;

    if (!db_conn)
        return retval;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString channelinput = chan->GetCurrentInput();
   
    QString thequery = QString("SELECT channel.%1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%2\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%3\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%4\" AND "
                               "capturecard.hostname = \"%5\";")
                               .arg(channel_field).arg(channum)
                               .arg(channelinput).arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getchannelvalue", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        retval = query.value(0).toInt();
    }

    pthread_mutex_unlock(&db_lock);
    return retval;
}

void TVRec::SetChannelValue(QString &field_name,int value, ChannelBase *chan,
                            const QString &channum)
{
    if (!chan)
        return;

    if (!db_conn)
        return;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString channelinput = chan->GetCurrentInput();

    // Only mysql 4.x can do multi table updates so we need two steps to get 
    // the sourceid from the table join.
    QString thequery = QString("SELECT channel.sourceid FROM "
                               "channel,cardinput,capturecard "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput)
                               .arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);
    int sourceid = -1;

    if (!query.isActive())
        MythContext::DBError("setchannelvalue", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();
        sourceid = query.value(0).toInt();
    }

    if (sourceid != -1)
    {
        thequery = QString("UPDATE channel SET channel.%1=\"%2\" "
                           "WHERE channel.channum = \"%3\" AND "
                           "channel.sourceid = \"%4\";")
                           .arg(field_name).arg(value).arg(channum)
                           .arg(sourceid);
        query = db_conn->exec(thequery);
    }

    pthread_mutex_unlock(&db_lock);
}

QString TVRec::GetNextChannel(ChannelBase *chan, int channeldirection)
{
    QString ret = "";

    if (!chan)
        return ret;

    // Get info on the current channel we're on
    QString channum = chan->GetCurrentName();
    QString chanid = "";

    DoGetNextChannel(channum, chan->GetCurrentInput(), m_capturecardnum,
                     chan->GetOrdering(), channeldirection, chanid);

    return channum;
}

QString TVRec::GetNextRelativeChanID(QString channum, int channeldirection)
{
    // Get info on the current channel we're on
    QString channum_out = channum;
    QString chanid = "";

    if (!channel)
        return chanid;

    DoGetNextChannel(channum_out, channel->GetCurrentInput(), 
                     m_capturecardnum, channel->GetOrdering(),
                     channeldirection, chanid);

    return chanid;
}

void TVRec::DoGetNextChannel(QString &channum, QString channelinput,
                             int cardid, QString channelorder,
                             int channeldirection, QString &chanid)
{
    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    if (channum[0].isLetter() && channelorder == "channum + 0")
    {
        cerr << "Your channel ordering method \"channel number (numeric)\"\n"
             << "will not work with channels like: " << channum << endl;
        cerr << "Consider switching to order by \"database order\" or \n"
             << "\"channel number (alpha)\" in the general settings section\n"
             << "of the frontend setup\n";
        channelorder = "channum";
    }

    QString thequery = QString("SELECT %1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%2\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channelorder).arg(channum)
                               .arg(cardid)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    QString id = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        id = query.value(0).toString();
    }
    else
    {
        cerr << "Channel: \'" << channum << "\' was not found in the database.";
        cerr << "\nMost likely, the default channel set for this input\n";
        cerr << "(" << cardid << " " << channelinput << " )\n";
        cerr << "in setup is wrong\n";

        thequery = QString("SELECT %1 FROM channel,capturecard,cardinput "
                           "WHERE channel.sourceid = cardinput.sourceid AND "
                           "cardinput.cardid = capturecard.cardid AND "
                           "capturecard.cardid = \"%2\" AND "
                           "capturecard.hostname = \"%3\" ORDER BY %4 "
                           "LIMIT 1;").arg(channelorder)
                           .arg(cardid).arg(gContext->GetHostName())
                           .arg(channelorder);
       
        query = db_conn->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            id = query.value(0).toString();
        }
    }

    if (id == QString::null) {
        pthread_mutex_unlock(&db_lock);
        cerr << "Couldn't find any channels in the database, please make sure "
             << "\nyour inputs are associated properly with your cards.\n";
        channum = "";
        chanid = "";
        return;
    }

    // Now let's try finding the next channel in the desired direction
    QString comp = ">";
    QString ordering = "";
    QString fromfavorites = "";
    QString wherefavorites = "";

    if (channeldirection == CHANNEL_DIRECTION_DOWN)
    {
        comp = "<";
        ordering = " DESC ";
    }
    else if (channeldirection == CHANNEL_DIRECTION_FAVORITE)
    {
        fromfavorites = ",favorites";
        wherefavorites = "AND favorites.chanid = channel.chanid";
    }
    else if (channeldirection == CHANNEL_DIRECTION_SAME)
    {
        comp = "=";
    }

    QString wherepart = QString("cardinput.cardid = capturecard.cardid AND "
                                "capturecard.cardid = \"%1\" AND "
                                "capturecard.hostname = \"%2\" AND "
                                "cardinput.sourceid = channel.sourceid ")
                                .arg(cardid)
                                .arg(gContext->GetHostName());

    thequery = QString("SELECT channel.channum, channel.chanid "
                       "FROM channel,capturecard,"
                       "cardinput%1 WHERE "
                       "channel.%2 %3 \"%4\" %5 AND %6 "
                       "ORDER BY channel.%7 %8 LIMIT 1;")
                       .arg(fromfavorites).arg(channelorder)
                       .arg(comp).arg(id).arg(wherefavorites)
                       .arg(wherepart).arg(channelorder).arg(ordering);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getnextchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        channum = query.value(0).toString();
        chanid = query.value(1).toString();
    }
    else
    {
        // Couldn't find the channel going in the desired direction, 
        // so loop around and find it on the flip side...
        comp = "<";
        if (channeldirection == CHANNEL_DIRECTION_DOWN) 
            comp = ">";

        // again, %9 is the limit for this
        thequery = QString("SELECT channel.channum, channel.chanid "
                           "FROM channel,capturecard,"
                           "cardinput%1 WHERE "
                           "channel.%2 %3 \"%4\" %5 AND %6 "
                           "ORDER BY channel.%7 %8 LIMIT 1;")
                           .arg(fromfavorites).arg(channelorder)
                           .arg(comp).arg(id).arg(wherefavorites)
                           .arg(wherepart).arg(channelorder).arg(ordering);

        query = db_conn->exec(thequery);
 
        if (!query.isActive())
            MythContext::DBError("getnextchannel", query);
        else if (query.numRowsAffected() > 0)
        { 
            query.next();

            channum = query.value(0).toString();
            chanid = query.value(1).toString();
        }
    }

    pthread_mutex_unlock(&db_lock);

    return;
}

bool TVRec::IsReallyRecording(void)
{
    if (nvr && nvr->IsRecording())
        return true;

    return false;
}

bool TVRec::IsBusy(void)
{
    bool retval = (GetState() != kState_None);

    if (recordPending && 
        QDateTime::currentDateTime().secsTo(recordPendingStart) <= 5)
    {
        retval = true;
    }

    return retval;
}

float TVRec::GetFramerate(void)
{
    return frameRate;
}

long long TVRec::GetFramesWritten(void)
{
    if (nvr)
        return nvr->GetFramesWritten();
    return -1;
}

long long TVRec::GetFilePosition(void)
{
    if (rbuffer)
        return rbuffer->GetTotalWritePosition();
    return -1;
}

long long TVRec::GetKeyframePosition(long long desired)
{
    if (nvr)
        return nvr->GetKeyframePosition(desired);
    return -1;
}

long long TVRec::GetFreeSpace(long long totalreadpos)
{
    if (rbuffer)
        return totalreadpos + rbuffer->GetFileSize() - 
               rbuffer->GetTotalWritePosition() - rbuffer->GetSmudgeSize();

    return -1;
}

void TVRec::StopPlaying(void)
{
    exitPlayer = true;
}

void TVRec::SetupRingBuffer(QString &path, long long &filesize, 
                            long long &fillamount, bool pip)
{
    ispip = pip;
    filesize = gContext->GetNumSetting("BufferSize", 5);
    fillamount = gContext->GetNumSetting("MaxBufferFill", 50);

    path = gContext->GetSetting("LiveBufferDir") + QString("/ringbuf%1.nuv")
                                                       .arg(m_capturecardnum);

    outputFilename = path;

    filesize = filesize * 1024 * 1024 * 1024;
    fillamount = fillamount * 1024 * 1024;

    rbuffer = new RingBuffer(path, filesize, fillamount);
    rbuffer->SetWriteBufferMinWriteSize(1);
}

void TVRec::SpawnLiveTV(void)
{
    nextState = kState_WatchingLiveTV;
    changeState = true;

    while (changeState)
        usleep(50);
}

void TVRec::StopLiveTV(void)
{
    nextState = kState_None;
    changeState = true;

    while (changeState)
        usleep(50);
}

void TVRec::PauseRecorder(void)
{
    if (!nvr)
        return;

    nvr->Pause();
} 

void TVRec::ToggleInputs(void)
{
    if (!nvr || !channel || !rbuffer)
        return;

    nvr->WaitForPause();

    PauseClearRingBuffer();

    if (!rbuffer)
    {
        UnpauseRingBuffer();
        return;
    }

    rbuffer->Reset();

    channel->ToggleInputs();

    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

void TVRec::ChangeChannel(int channeldirection)
{
    if (!nvr || !channel || !rbuffer)
        return;

    nvr->WaitForPause();

    PauseClearRingBuffer();

    if (!rbuffer)
    {
        UnpauseRingBuffer();
        return;
    }

    rbuffer->Reset();

    if (channeldirection == CHANNEL_DIRECTION_FAVORITE)
        channel->NextFavorite();
    else if (channeldirection == CHANNEL_DIRECTION_UP)
        channel->ChannelUp();
    else
        channel->ChannelDown();

    nvr->ChannelNameChanged(channel->GetCurrentName());
    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

void TVRec::ToggleChannelFavorite(void)
{
    if (!channel)
        return;

    // Get current channel id...
    QString channum = channel->GetCurrentName();
    QString channelinput = channel->GetCurrentInput();

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT channel.chanid FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.cardid = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput)
                               .arg(m_capturecardnum)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    QString chanid = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        chanid = query.value(0).toString();
    }
    else
    {
        pthread_mutex_unlock(&db_lock);
        cerr << "Channel: \'" << channum << "\' was not found in the database.";
        cerr << "\nMost likely, your DefaultTVChannel setting is wrong.";
        cerr << "\nCould not toggle favorite.\n";
        return;
    }

    // Check if favorite exists for that chanid...
    thequery = QString("SELECT favorites.favid FROM favorites WHERE "
                       "favorites.chanid = \"%1\""
                       "LIMIT 1;")
                       .arg(chanid);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("togglechannelfavorite", query);
    else if (query.numRowsAffected() > 0)
    {
        // We have a favorites record...Remove it to toggle...
        query.next();
        QString favid = query.value(0).toString();

        thequery = QString("DELETE FROM favorites WHERE favid = \"%1\"")
                           .arg(favid);

        query = db_conn->exec(thequery);
        VERBOSE(VB_RECORD, "Removing Favorite.");
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        thequery = QString("INSERT INTO favorites (chanid) VALUES (\"%1\")")
                           .arg(chanid);

        query = db_conn->exec(thequery);
        VERBOSE(VB_RECORD, "Adding Favorite.");
    }
    pthread_mutex_unlock(&db_lock);
}

int TVRec::ChangeContrast(bool direction)
{
    if (!channel)
        return -1;

    int ret = channel->ChangeContrast(direction);
    return ret;
}

int TVRec::ChangeBrightness(bool direction)
{
    if (!channel)
        return -1;

    int ret = channel->ChangeBrightness(direction);
    return ret;
}

int TVRec::ChangeColour(bool direction)
{
    if (!channel)
        return -1;

    int ret = channel->ChangeColour(direction);
    return ret;
}

int TVRec::ChangeHue(bool direction)
{
    if (!channel)
        return -1;

    int ret = channel->ChangeHue(direction);
    return ret;
}

void TVRec::SetChannel(QString name)
{
    if (!nvr || !channel || !rbuffer)
        return;

    nvr->WaitForPause();

    PauseClearRingBuffer();

    if (!rbuffer)
    {
        UnpauseRingBuffer();
        return;
    }

    rbuffer->Reset();

    QString chan = name.stripWhiteSpace();
    QString prevchan = channel->GetCurrentName();

    if (!channel->SetChannelByString(chan))
        channel->SetChannelByString(prevchan);

    nvr->ChannelNameChanged(channel->GetCurrentName());
    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

void TVRec::GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid)
{
    QString querystr;
    QString nextchannum = channelname;
    QString compare = "<";
    QString sortorder = "";

    querystr = QString("SELECT title, subtitle, description, category, "
                       "starttime, endtime, callsign, icon, channum, "
                       "program.chanid, seriesid, programid "
                       "FROM program, channel "
                       "WHERE program.chanid = channel.chanid ");

    switch (direction)
    {
        case BROWSE_SAME:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_SAME);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_UP:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_UP);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_DOWN:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_DOWN);
                compare = "<=";
                sortorder = "desc";
                break;
        case BROWSE_LEFT:
                compare = "<";
                sortorder = "desc";
                break;
        case BROWSE_RIGHT:
                compare = ">";
                sortorder = "asc";
                break;
        case BROWSE_FAVORITE:
                chanid = GetNextRelativeChanID(channelname,
                                               CHANNEL_DIRECTION_FAVORITE);
                compare = "<=";
                sortorder = "desc";
                break;
    }

    querystr += QString( "and channel.chanid = '%1' "
                         "and starttime %3 '%2' "
                     "order by starttime %4 limit 1;")
                     .arg(chanid).arg(starttime).arg(compare).arg(sortorder);

    pthread_mutex_lock(&db_lock);

    QSqlQuery sqlquery = db_conn->exec(querystr);

    if ((sqlquery.isActive()) && (sqlquery.numRowsAffected() > 0))
    {
        if (sqlquery.next())
        {
            title = QString::fromUtf8(sqlquery.value(0).toString());
            subtitle = QString::fromUtf8(sqlquery.value(1).toString());
            desc = QString::fromUtf8(sqlquery.value(2).toString());
            category = QString::fromUtf8(sqlquery.value(3).toString());
            starttime =  sqlquery.value(4).toString();
            endtime = sqlquery.value(5).toString();
            callsign = sqlquery.value(6).toString();
            iconpath = sqlquery.value(7).toString();
            channelname = sqlquery.value(8).toString();
            chanid = sqlquery.value(9).toString();
            seriesid = sqlquery.value(10).toString();
            programid = sqlquery.value(11).toString();
        }
    }
    else
    {
        // Couldn't get program info, so get the channel info and clear 
        // everything else.
        starttime = "";
        endtime = "";
        title = "";
        subtitle = "";
        desc = "";
        category = "";
        seriesid = "";
        programid = "";

        querystr = QString("SELECT channum, callsign, icon, chanid FROM "
                           "channel WHERE chanid = %1;")
                           .arg(chanid);
        sqlquery = db_conn->exec(querystr);

        if (sqlquery.isActive() && sqlquery.numRowsAffected() > 0 && 
            sqlquery.next())
        {
            channelname = sqlquery.value(0).toString();
            callsign = sqlquery.value(1).toString();
            iconpath = sqlquery.value(2).toString();
            chanid = sqlquery.value(3).toString();
        }
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid,
                        QString &outputFilters, QString &repeat, QString &airdate,
                        QString &stars)
{
    if (!channel)
        return;

    GetChannelInfo(channel, title, subtitle, desc, category, starttime,
                   endtime, callsign, iconpath, channelname, chanid,
                   seriesid, programid, outputFilters, repeat, airdate, stars);
}

void TVRec::GetInputName(QString &inputname)
{
    if (!channel)
        return;

    inputname = channel->GetCurrentInput();
}

void TVRec::UnpauseRingBuffer(void)
{
    if (rbuffer)
        rbuffer->StartReads();
    readthreadLock.unlock();
}

void TVRec::PauseClearRingBuffer(void)
{
    readthreadLock.lock();

    if (!rbuffer)
        return;

    rbuffer->StopReads();
}

long long TVRec::SeekRingBuffer(long long curpos, long long pos, int whence)
{
    PauseClearRingBuffer();

    if (!rbuffer || !readthreadlive)
    {
        UnpauseRingBuffer();
        return -1;
    }

    if (whence == SEEK_CUR)
    {
        long long realpos = rbuffer->GetTotalReadPosition();

        pos = pos + curpos - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    UnpauseRingBuffer();
    return ret;
}

QSocket *TVRec::GetReadThreadSocket(void)
{
    return readthreadSock;
}

void TVRec::SetReadThreadSock(QSocket *sock)
{
    if ((readthreadlive && sock) || (!readthreadlive && !sock))
        return;

    if (sock)
    {
        readthreadSock = sock;
        readthreadlive = true;
    }
    else
    {
        readthreadlive = false;
        if (rbuffer)
            rbuffer->StopReads();
        readthreadLock.lock();
        readthreadLock.unlock();
    }
}

int TVRec::RequestRingBufferBlock(int size)
{
    int tot = 0;
    int ret = 0;

    readthreadLock.lock();

    if (!readthreadlive || !rbuffer)
    {
        readthreadLock.unlock();
        return -1;
    }

    while (tot < size && !rbuffer->GetStopReads() && readthreadlive)
    {
        int request = size - tot;

        if (request > 256000)
            request = 256000;
 
        ret = rbuffer->Read(requestBuffer, request);
        
        if (rbuffer->GetStopReads() || ret <= 0)
            break;
        
        if (!WriteBlock(readthreadSock->socketDevice(), requestBuffer, ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < request)
            break; // we hit eof
    }
    readthreadLock.unlock();

    if (ret < 0)
        tot = -1;

    return tot;
}

void TVRec::RetrieveInputChannels(map<int, QString> &inputChannel,
                                  map<int, QString> &inputTuneTo,
                                  map<int, QString> &externalChanger,
                                  map<int, QString> &sourceid)
{
    if (!channel)
        return;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    QString query = QString("SELECT inputname, trim(externalcommand), "
                            "if(tunechan='', 'Undefined', tunechan), "
                            "if(startchan, startchan, ''), sourceid "
                            "FROM capturecard, cardinput "
                            "WHERE capturecard.cardid = %1 "
                            "AND capturecard.cardid = cardinput.cardid;")
                            .arg(m_capturecardnum);

    QSqlQuery result = db_conn->exec(query);

    if (!result.isActive())
        MythContext::DBError("RetrieveInputChannels", result);
    else if (!result.numRowsAffected())
    {
        cerr << "Error getting inputs for the capturecard.  Perhaps you have\n"
                "forgotten to bind video sources to your card's inputs?\n";
    }
    else
    {
        int cap;

        while (result.next())
        {
            cap = channel->GetInputByName(result.value(0).toString());
            externalChanger[cap] = result.value(1).toString();
            inputTuneTo[cap] = result.value(2).toString();
            inputChannel[cap] = result.value(3).toString();
            sourceid[cap] = result.value(4).toString();
        }
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::StoreInputChannels(map<int, QString> &inputChannel)
{
    if (!channel)
        return;

    QString query, input;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    for (int i = 0;; i++)
    {
        input = channel->GetInputByNum(i);
        if (input.isEmpty())
            break;

        query = QString("UPDATE cardinput set startchan = '%1' "
                        "WHERE cardid = %2 AND inputname = '%3';")
                        .arg(inputChannel[i]).arg(m_capturecardnum).arg(input);

        QSqlQuery result = db_conn->exec(query);

        if (!result.isActive())
            MythContext::DBError("StoreInputChannels", result);
    }

    pthread_mutex_unlock(&db_lock);
}

