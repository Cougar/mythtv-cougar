#include <unistd.h>
#include <iostream>

using namespace std;

#include "encoderlink.h"
#include "playbacksock.h"
#include "tv.h"
#include "programinfo.h"

#include <sys/stat.h>
#include <sys/vfs.h>

#include "libmyth/mythcontext.h"

EncoderLink::EncoderLink(int capturecardnum, PlaybackSock *lsock, 
                         QString lhostname)
{
    sock = lsock;
    hostname = lhostname;
    tv = NULL;
    local = false;
    m_capturecardnum = capturecardnum;

    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;
    chanid = "";
}

EncoderLink::EncoderLink(int capturecardnum, TVRec *ltv)
{
    sock = NULL;
    tv = ltv;
    local = true;
    m_capturecardnum = capturecardnum;

    recordfileprefix = gContext->GetSetting("RecordFilePrefix");

    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;
    chanid = "";
}

EncoderLink::~EncoderLink()
{
    if (local)
        delete tv;
}

bool EncoderLink::IsBusy(void)
{
    if (local)
        return tv->IsBusy();

    if (sock)
        return sock->IsBusy(m_capturecardnum);

    return false;
}

bool EncoderLink::IsBusyRecording(void)
{
    bool retval = false;

    TVState state = GetState();

    if (state == kState_RecordingOnly || state == kState_WatchingRecording ||
        state == kState_WatchingLiveTV)
    {
        retval = true;
    }

    return retval;
}

bool EncoderLink::isConnected(void)
{
    if (local)
        return true;
       
    if (sock)
        return true;

    return false;
}

TVState EncoderLink::GetState(void)
{
    TVState retval = kState_Error;

    if (local)
        retval = tv->GetState();
    else if (sock)
        retval = (TVState)sock->GetEncoderState(m_capturecardnum);
    else
        cerr << "Broken for card: " << m_capturecardnum << endl;

    return retval;
}

bool EncoderLink::isRecording(ProgramInfo *rec)
{
    bool retval = false;

    if (rec->chanid == chanid && rec->startts == startRecordingTime)
        retval = true;

    return retval;
}

bool EncoderLink::MatchesRecording(ProgramInfo *rec)
{
    bool retval = false;
    ProgramInfo *tvrec = NULL;

    if (local)
    {
        while (kState_ChangingState == GetState())
            usleep(100);

        if (IsBusyRecording())
            tvrec = tv->GetRecording();

        if (tvrec)
        {
            if (tvrec->chanid == rec->chanid && 
                tvrec->startts == rec->startts &&
                tvrec->endts == rec->endts)
            {
                retval = true;
            }

            delete tvrec;
        }
    }
    else
    {
        if (sock)
            retval = sock->EncoderIsRecording(m_capturecardnum, rec);
    }

    return retval;
}

void EncoderLink::RecordPending(ProgramInfo *rec, int secsleft)
{
    if (local)
        tv->RecordPending(rec, secsleft);
    else if (sock)
        sock->RecordPending(m_capturecardnum, rec, secsleft);
}

bool EncoderLink::WouldConflict(ProgramInfo *rec)
{
    if (!isConnected())
        return true;

    if (rec->startts < endRecordingTime)
        return true;

    return false;
}

void EncoderLink::cacheFreeSpace()
{
    freeSpace = -1;

    if (local)
    {
        struct statfs statbuf;
        if (statfs(recordfileprefix.ascii(), &statbuf) == 0)
        {
            freeSpace = statbuf.f_bfree / (1024*1024/statbuf.f_bsize);
        }
    }
    else if (sock)
    {
        int totalspace = 0;
        int usedspace = 0;

        sock->GetFreeSpace(totalspace, usedspace);

        if (totalspace)
        {
            freeSpace = totalspace - usedspace;
        }
        else
        {
            struct statfs statbuf;
            if (statfs(recordfileprefix.ascii(), &statbuf) == 0)
            {
                freeSpace = statbuf.f_bfree / (1024*1024/statbuf.f_bsize);
            }
        }
    }
    else
    {
        freeSpace = 0;
    }
}

bool EncoderLink::isLowOnFreeSpace()
{
    if (!isConnected())
        return true;
   
    if ((freeSpace >= 0) && (freeSpace < 250))
        return true;

    return false;
}

int EncoderLink::StartRecording(ProgramInfo *rec)
{
    int retval = 0;

    endRecordingTime = rec->endts;
    startRecordingTime = rec->startts;
    chanid = rec->chanid;

    if (local)
        retval = tv->StartRecording(rec);
    else if (sock)
        retval = sock->StartRecording(m_capturecardnum, rec);
    else
        cerr << "Wanted to start recording at: " << m_capturecardnum
             << "\nbut the server's not here anymore\n";

    if (!retval)
    {
        endRecordingTime = QDateTime::currentDateTime().addDays(-2);
        startRecordingTime = endRecordingTime;
        chanid = "";
    }

    return retval;
}

void EncoderLink::StopRecording(void)
{
    endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    startRecordingTime = endRecordingTime;
    chanid = "";

    if (local)
    {
        tv->StopRecording();
        return;
    }
}

void EncoderLink::FinishRecording(void)
{
    if (local)
    {
        tv->FinishRecording();
        return;
    }
    else
    {
        endRecordingTime = QDateTime::currentDateTime().addDays(-2);
    }
}

bool EncoderLink::IsReallyRecording(void)
{
    if (local)
        return tv->IsReallyRecording();

    cerr << "Should be local only query: IsReallyRecording\n";
    return false;
}

float EncoderLink::GetFramerate(void)
{
    if (local)
        return tv->GetFramerate();

    cerr << "Should be local only query: GetFramerate\n";
    return -1;
}

long long EncoderLink::GetFramesWritten(void)
{
    if (local)
        return tv->GetFramesWritten();

    cerr << "Should be local only query: GetFramesWritten\n";
    return -1;
}

long long EncoderLink::GetFilePosition(void)
{
    if (local)
        return tv->GetFilePosition();

    cerr << "Should be local only query: GetFilePosition\n";
    return -1;
}

long long EncoderLink::GetFreeSpace(long long totalreadpos)
{
    if (local)
        return tv->GetFreeSpace(totalreadpos);

    cerr << "Should be local only query: GetFreeSpace\n";
    return -1;
}

long long EncoderLink::GetKeyframePosition(long long desired)
{
    if (local)
        return tv->GetKeyframePosition(desired);

    cerr << "Should be local only query: GetKeyframePosition\n";
    return -1;
}

void EncoderLink::FrontendReady(void)
{
    if (local)
        return tv->FrontendReady();

    cerr << "Should be local only query: FrontendReady\n";
}

void EncoderLink::CancelNextRecording(void)
{
    if (local)
    {
        tv->CancelNextRecording();
        return;
    }

    cerr << "Should be local only query: CancelNextRecording\n";
}

ProgramInfo *EncoderLink::GetRecording(void)
{
    if (local)
        return tv->GetRecording();

    cerr << "Should be local only query: GetRecording\n";
    return NULL;
}

void EncoderLink::StopPlaying(void)
{
    if (local)
    {
        tv->StopPlaying();
        return;
    }

    cerr << "Should be local only query: StopPlaying\n";
}

void EncoderLink::SetupRingBuffer(QString &path, long long &filesize,
                                  long long &fillamount, bool pip)
{
    if (local)
    {
        tv->SetupRingBuffer(path, filesize, fillamount, pip);
        return;
    }

    cerr << "Should be local only query: SetupRingBuffer\n";
}

void EncoderLink::SpawnLiveTV(void)
{
    if (local)
    {
        tv->SpawnLiveTV();
        return;
    }

    cerr << "Should be local only query: SpawnLiveTV\n";
}

void EncoderLink::StopLiveTV(void)
{
    if (local)
    {
        tv->StopLiveTV();
        return;
    }

    cerr << "Should be local only query: StopLiveTV\n";
}

void EncoderLink::PauseRecorder(void)
{
    if (local)
    {
        tv->PauseRecorder();
        return;
    }

    cerr << "Should be local only query: PauseRecorder\n";
}

void EncoderLink::ToggleInputs(void)
{
    if (local)
    {
        tv->ToggleInputs();
        return;
    }

    cerr << "Should be local only query: ToggleInputs\n";
}

void EncoderLink::ToggleChannelFavorite(void)
{
    if (local)
    {
        tv->ToggleChannelFavorite();
        return;
    }

    cerr << "Should be local only query: ToggleChannelFavorite\n";
}

void EncoderLink::ChangeChannel(int channeldirection)
{
    if (local)
    {
        tv->ChangeChannel(channeldirection);
        return;
    }

    cerr << "Should be local only query: ChangeChannel\n";
}

void EncoderLink::SetChannel(QString name)
{
    if (local)
    {
        tv->SetChannel(name);
        return;
    }

    cerr << "Should be local only query: SetChannel\n";
}

int EncoderLink::ChangeContrast(bool direction)
{
    int ret = 0;

    if (local)
        ret = tv->ChangeContrast(direction);
    else
        cerr << "Should be local only query: ChangeContrast\n";

    return ret;
}

int EncoderLink::ChangeBrightness(bool direction)
{
    int ret = 0;

    if (local)
        ret = tv->ChangeBrightness(direction);
    else
        cerr << "Should be local only query: ChangeBrightness\n";

    return ret;
}

int EncoderLink::ChangeColour(bool direction)
{
    int ret = 0;

    if (local)
        ret = tv->ChangeColour(direction);
    else
        cerr << "Should be local only query: ChangeColor\n";

    return ret;
}

int EncoderLink::ChangeHue(bool direction)
{
    int ret = 0;

    if (local)
        ret = tv->ChangeHue(direction);
    else
        cerr << "Should be local only query: ChangeHue\n";

    return ret;
}

bool EncoderLink::CheckChannel(QString name)
{
    if (local)
        return tv->CheckChannel(name);

    cerr << "Should be local only query: CheckChannel\n";
    return false;
}

void EncoderLink::GetNextProgram(int direction,
                                 QString &title, QString &subtitle, 
                                 QString &desc, QString &category, 
                                 QString &starttime, QString &endtime, 
                                 QString &callsign, QString &iconpath,
                                 QString &channelname, QString &chanid)
{
    if (local)
    {
        tv->GetNextProgram(direction,
                           title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname, chanid);
        return;
    }

    cerr << "Should be local only query: GetNextProgram\n";
}

void EncoderLink::GetChannelInfo(QString &title, QString &subtitle, 
                                 QString &desc, QString &category, 
                                 QString &starttime, QString &endtime, 
                                 QString &callsign, QString &iconpath,
                                 QString &channelname, QString &chanid)
{
    if (local)
    {
        tv->GetChannelInfo(title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname, chanid);
        return;
    }

    cerr << "Should be local only query: GetChannelInfo\n";
}

void EncoderLink::GetInputName(QString &inputname)
{
    if (local)
    {
        tv->GetInputName(inputname);
        return;
    }

    cerr << "Should be local only query: GetInputName\n";
}

void EncoderLink::SpawnReadThread(QSocket *rsock)
{
    if (local)
    {
        tv->SpawnReadThread(rsock);
        return;
    }

    cerr << "Should be local only query: SpawnReadThread\n";
}

void EncoderLink::KillReadThread(void)
{
    if (local)
    {
        tv->KillReadThread();
        return;
    }

    cerr << "Should be local only query: KillReadThread\n";
}

QSocket *EncoderLink::GetReadThreadSocket(void)
{
    if (local)
        return tv->GetReadThreadSocket();

    cerr << "Should be local only query: GetReadThreadSocket\n";
    return NULL;
}

void EncoderLink::PauseRingBuffer(void)
{
    if (local)
    {
        tv->PauseRingBuffer();
        return;
    }

    cerr << "Should be local only query: PauseRingBuffer\n";
}

void EncoderLink::UnpauseRingBuffer(void)
{
    if (local)
    {
        tv->UnpauseRingBuffer();
        return;
    }

    cerr << "Should be local only query: UnpauseRingBuffer\n";
}

void EncoderLink::PauseClearRingBuffer(void)
{
    if (local)
    {
        tv->PauseClearRingBuffer();
        return;
    }

    cerr << "Should be local only query: PauseClearRingBuffer\n";
}

void EncoderLink::RequestRingBufferBlock(int size)
{
    if (local)
    {
        tv->RequestRingBufferBlock(size);
        return;
    }

    cerr << "Should be local only query: RequestRingBufferBlock\n";
}

long long EncoderLink::SeekRingBuffer(long long curpos, long long pos, 
                                      int whence)
{
    if (local)
        return tv->SeekRingBuffer(curpos, pos, whence);

    cerr << "Should be local only query: SeekRingBuffer\n";
    return -1;
}

char *EncoderLink::GetScreenGrab(ProgramInfo *pginfo, const QString &filename, 
                                 int secondsin, int &bufferlen, 
                                 int &video_width, int &video_height)
{
    if (local)
        return tv->GetScreenGrab(pginfo, filename, secondsin, bufferlen, 
                                 video_width, video_height);

    cerr << "Should be local only query: GetScreenGrab\n";
    return NULL;
}

bool EncoderLink::isParsingCommercials(ProgramInfo *pginfo)
{
    if (local)
        return tv->isParsingCommercials(pginfo);

    return false;
}

