#ifndef RINGBUFFER
#define RINGBUFFER

#include <qstring.h>
#include <qwaitcondition.h>
#include <qmutex.h>
#include <pthread.h>

class RemoteFile;
class RemoteEncoder;
class ThreadedFileWriter;

class RingBuffer
{
  public:
    RingBuffer(const QString &lfilename, bool write, bool needevents = false);
    RingBuffer(const QString &lfilename, long long size, long long smudge,
               RemoteEncoder *enc = NULL);
    
   ~RingBuffer();

    bool IsOpen(void) { return (tfw || fd2 > 0 || remotefile); }
    
    int Read(void *buf, int count);
    int Write(const void *buf, int count);
    void Sync(void);

    long long Seek(long long pos, int whence);
    long long WriterSeek(long long pos, int whence);

    long long GetReadPosition(void);
    long long GetTotalReadPosition(void);
    long long GetWritePosition(void);
    long long GetTotalWritePosition(void);
    long long GetFileSize(void) { return filesize; }
    long long GetSmudgeSize(void) { return smudgeamount; }

    long long GetFileWritePosition(void);
    
    long long GetFreeSpace(void);

    long long GetFreeSpaceWithReadChange(long long readchange);

    void Reset(void);

    void StopReads(void);
    void StartReads(void);
    bool GetStopReads(void) { return stopreads; }

    bool LiveMode(void) { return !normalfile; }

    const QString GetFilename(void) { return filename; }

    bool IsIOBound(void);

    void Start(void);

    void Pause(void);
    void Unpause(void);
    bool isPaused(void);
    void WaitForPause(void);

    void CalcReadAheadThresh(int estbitrate);

    long long GetRealFileSize(void);

  protected:
    static void *startReader(void *type);
    void ReadAheadThread(void);

  private:
    void Init(void);

    int safe_read(int fd, void *data, unsigned sz);
    int safe_read(RemoteFile *rf, void *data, unsigned sz);

    QString filename;

    ThreadedFileWriter *tfw;
    int fd2;
 
    bool normalfile;
    bool writemode;
    
    long long writepos;
    long long totalwritepos;

    long long readpos;
    long long totalreadpos;

    long long filesize;
    long long smudgeamount;

    long long wrapcount;

    bool stopreads;

    pthread_rwlock_t rwlock;

    int recorder_num;
    RemoteEncoder *remoteencoder;
    RemoteFile *remotefile;

    int ReadFromBuf(void *buf, int count);

    inline int ReadBufFree(void);
    inline int ReadBufAvail(void);

    void StartupReadAheadThread(void);
    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

    QMutex readAheadLock;
    pthread_t reader;

    char *readAheadBuffer;
    bool readaheadrunning;
    bool readaheadpaused;
    bool pausereadthread;
    int rbrpos;
    int rbwpos;
    long long internalreadpos;
    bool ateof;
    bool readsallowed;
    bool wantseek;
    int fill_threshold;
    int fill_min;

    int readblocksize;

    QWaitCondition pauseWait;

    int wanttoread;
    QWaitCondition availWait;
    QMutex availWaitMutex;

    QWaitCondition readsAllowedWait;
};

#endif
