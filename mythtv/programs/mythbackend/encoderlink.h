#ifndef ENCODERLINK_H_
#define ENCODERLINK_H_

#include <qstring.h>

#include "tv.h"

class MainServer;
class PlaybackSock;

class EncoderLink
{
  public:
    EncoderLink(int capturecardnum, PlaybackSock *lsock, QString lhostname);
    EncoderLink(int capturecardnum, TVRec *ltv);

   ~EncoderLink();

    void setSocket(PlaybackSock *lsock) { sock = lsock; }

    PlaybackSock *getSocket() { return sock; }
    QString getHostname() { return hostname; }

    QString recordfileprefix;

    TVRec *getTV() { return tv; }

    bool isLocal() { return local; }

    bool isConnected();
    int getCardId() { return m_capturecardnum; }

    int LockTuner();
    void FreeTuner();
    bool isTunerLocked();
    
    void cacheFreeSpace();
    bool isLowOnFreeSpace();
    int getFreeSpace() { return freeSpace; }

    bool IsBusy(void);
    bool IsBusyRecording(void);

    TVState GetState();
    bool isRecording(ProgramInfo *rec); // scheduler call only.

    bool MatchesRecording(ProgramInfo *rec);
    void RecordPending(ProgramInfo *rec, int secsleft);
    int StartRecording(ProgramInfo *rec);
    void StopRecording(void);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(void);
    bool WouldConflict(ProgramInfo *rec);

    bool IsReallyRecording(void);
    ProgramInfo *GetRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void StopPlaying(void);
    void SetupRingBuffer(QString &path, long long &filesize,
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleInputs(void);
    void ToggleChannelFavorite(void);
    void ChangeChannel(int channeldirection);
    void SetChannel(QString name);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeColour(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(QString name);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetInputName(QString &inputname);

    void SpawnReadThread(QSocket *rsock);
    void KillReadThread(void);
    QSocket *GetReadThreadSocket(void);

    void RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    char *GetScreenGrab(ProgramInfo *pginfo, const QString &filename, 
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height);

    bool isParsingCommercials(ProgramInfo *pginfo);

  private:
    int m_capturecardnum;

    PlaybackSock *sock;
    QString hostname;

    int freeSpace;

    TVRec *tv;

    bool local;
    bool locked;

    QDateTime endRecordingTime;
    QDateTime startRecordingTime;
    QString chanid;
};

#endif
