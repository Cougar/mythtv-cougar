#ifndef RECORDERBASE_H_
#define RECORDERBASE_H_

#include <qstring.h>
#include <qmap.h>
#include <qsqldatabase.h>

#include <pthread.h>

class RingBuffer;
class ProgramInfo;

class RecorderBase
{
  public:
    RecorderBase();
    virtual ~RecorderBase();

    void SetRingBuffer(RingBuffer *rbuf);
    void SetRecording(ProgramInfo *pginfo);
    void SetDB(QSqlDatabase *db, pthread_mutex_t *lock);

    float GetFrameRate(void) { return video_frame_rate; }
    void SetFrameRate(float rate) { video_frame_rate = rate; }

    virtual void SetOption(const QString &opt, const QString &value);
    virtual void SetOption(const QString &opt, int value);
    virtual void SetVideoFilters(QString &filters) = 0;

    virtual void Initialize(void) = 0;
    virtual void StartRecording(void) = 0;
    virtual void StopRecording(void) = 0;
    virtual void Reset(void) = 0;   

    virtual void Pause(bool clear = true) = 0;
    virtual void Unpause(void) = 0;
    virtual bool GetPause(void) = 0;

    virtual bool IsRecording(void) = 0;

    virtual long long GetFramesWritten(void) = 0;
    
    virtual int GetVideoFd(void) = 0;
    
    virtual long long GetKeyframePosition(long long desired) = 0;
    virtual void GetBlankFrameMap(QMap<long long, int> &blank_frame_map) = 0;

  protected:
    RingBuffer *ringBuffer;
    bool weMadeBuffer;

    QString codec;
    QString audiodevice;
    QString videodevice;
    QString vbidevice;

    char vbimode;
    int ntsc;
    double video_frame_rate;

    ProgramInfo *curRecording;

    QSqlDatabase *db_conn;
    pthread_mutex_t *db_lock;
};

#endif
