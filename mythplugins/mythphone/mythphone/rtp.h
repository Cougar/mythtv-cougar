#ifndef RTP_H_
#define RTP_H_

#include <q3socketdevice.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <q3ptrlist.h>
#include <qthread.h>
#include <qdatetime.h>

#include <QEvent>
#include <qwaitcondition.h>

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <sstream>
#include <mmsystem.h>
#endif

#include <dtmffilter.h>
#include <audiodrv.h>


#define IP_MAX_MTU                1500     // Max size of rxed RTP packet
#define IP_MTU                    1290     // Max size of txed RTP packet. Standard MTU is 1500, leave some room for IPSec etc
#define TX_USER_STREAM_SIZE       4096
#define MINBYTESIZE               80
#define RTP_HEADER_SIZE           12
#define UDP_HEADER_SIZE           28
#define	MESSAGE_SIZE              80       // bytes to send per 10ms
#define	ULAW_BYTES_PER_MS         8        // bytes to send per 1ms

#define RTP_STATS_INTERVAL        1 // Seconds between sending statistics 

class rtp;

class RtpEvent : public QEvent
{
public:
    enum Type { RxVideoFrame = (QEvent::User + 300), RtpDebugEv, RtpStatisticsEv, RtpRtcpStatsEv };

    RtpEvent(Type t, QString s="") : QEvent((QEvent::Type)t) { text=s; }
    RtpEvent(Type t, rtp *r, QTime tm, int ms, int s1, int s2, int s3, int s4, int s5, int s6, 
             int s7, int s8, int s9, int s10, int s11, int s12, int s13, int s14, int s15, int s16)
              : QEvent((QEvent::Type)t)
             { rtpThread=r; timestamp=tm; msPeriod = ms; pkIn=s1; pkOut=s2; pkMiss=s3; pkLate=s4; 
               pkInDisc=s5; pkOutDrop=s6;
               byteIn=s7; byteOut=s8; bytePlayed=s9; framesIn=s10; framesOut=s11; framesInDisc=s12; 
               framesOutDisc=s13; minPlayout=s14; avgPlayout=s15; maxPlayout=s16; }
    RtpEvent(Type t, rtp *r, QTime tm, int ms, int s1, int s2) : QEvent((QEvent::Type)t)
             { rtpThread=r; timestamp=tm; msPeriod = ms; rtcpFractionLoss=s1; rtcpTotalLoss=s2;}    
    ~RtpEvent()                 {  }
    QString msg()               { return text;}
    rtp *owner()                { return rtpThread; }
    int getPkIn()               { return pkIn; }
    int getPkMissed()           { return pkMiss; }
    int getPkLate()             { return pkLate; }
    int getPkOut()              { return pkOut; }
    int getPkInDisc()           { return pkInDisc; }
    int getPkOutDrop()          { return pkOutDrop; }
    int getBytesIn()            { return byteIn; }
    int getBytesOut()           { return byteOut; }
    int getFramesIn()           { return framesIn; }
    int getFramesOut()          { return framesOut; }
    int getFramesInDiscarded()  { return framesInDisc; }
    int getFramesOutDiscarded() { return framesOutDisc; }
    int getPeriod()             { return msPeriod; }
    int getRtcpFractionLoss()   { return rtcpFractionLoss; }
    int getRtcpTotalLoss()      { return rtcpTotalLoss; }
    int getMinPlayout()         { return minPlayout; }
    int getAvgPlayout()         { return avgPlayout; }
    int getMaxPlayout()         { return maxPlayout; }

private:
    QString text;
    
    rtp *rtpThread;
    QTime timestamp;
    int msPeriod;
    int pkIn;
    int pkOut;
    int pkOutDrop;
    int pkMiss;
    int pkLate;
    int pkInDisc;
    int framesIn;
    int framesOut;
    int framesInDisc;
    int framesOutDisc;
    int byteIn;
    int byteOut;
    int bytePlayed;
    int rtcpFractionLoss;
    int rtcpTotalLoss;
    int minPlayout;
    int avgPlayout;
    int maxPlayout;
};



typedef struct RTPPACKET
{
    int     len;                       // Not part of the RTP frame itself
    uchar   RtpVPXCC;
    uchar   RtpMPT;
    ushort  RtpSequenceNumber;
    ulong   RtpTimeStamp;
    ulong   RtpSourceID;
    uchar   RtpData[IP_MAX_MTU-RTP_HEADER_SIZE-UDP_HEADER_SIZE];
} RTPPACKET;

#define RTCP_PERIOD           10 // Seconds
#define MAX_RTCP_SR_BLOCKS    1

typedef struct RTCP_SR_BLOCK
{
    ulong   Ssrc;
    uchar   fractionLost;
    uchar   totPacketLostMsb;
    ushort  totPacketLostLsw;
    ulong   extHighestSeqNumRx;
    ulong   arrivalJitter;
    ulong   lastSR;
    ulong   delayLastSR;
} RTCP_SR_BLOCK;

typedef struct RTCP_SR // Sender Report
{
    ulong   RtcpSsrc;
    ulong   RtcpNtpTimestampMsw;
    ulong   RtcpNtpTimestampLsw;
    ulong   RtcpTimestamp;
    ulong   RtcpSendPkCount;
    ulong   RtcpSendOctetCount;
    RTCP_SR_BLOCK srBlock[MAX_RTCP_SR_BLOCKS];
} RTCP_SR;

typedef struct RTCPPACKET
{
    uchar   RtcpVPRC;
    uchar   RtcpPT;
    ushort  RtcpLen;
    union
    {
        RTCP_SR sr;
    } d;
    char padding[1500]; // Room used on receiving RTCP packets, in case they contain more blocks than we cater for
} RTCPPACKET;

typedef struct 
{
    uchar  dtmfDigit;
    uchar  dtmfERVolume;
    short  dtmfDuration;
} DTMF_RFC2833;

typedef struct 
{
    ulong h263hdr;
} H263_RFC2190_HDR;


#define RTCP_SENDER_REPORT      200
#define RTCP_RECEIVER_REPORT    201
#define RTCP_SOURCES_DESCR      202
#define RTCP_GOODBYE            203
#define RTCP_APP_DEFINED        204

#define H263HDR(s)              ((s)<<13)
#define H263HDR_GETSZ(h)        (((h)>>13) & 0x7)
#define H263HDR_GETSBIT(h)      (((h)>>3) & 0x7)
#define H263HDR_GETEBIT(h)      ((h) & 0x7)

#define H263_SRC_SQCIF          1
#define H263_SRC_QCIF           2
#define H263_SRC_CIF            3
#define H263_SRC_4CIF           4
#define H263_SRC_16CIF          5


#define MAX_VIDEO_LEN 256000
typedef struct VIDEOBUFFER
{
  int     len;
  int     w,h;
	uchar	  video[MAX_VIDEO_LEN];
} VIDEOBUFFER;

// Values for RTP Payload Type
#define RTP_PAYLOAD_G711U	0x00
#define RTP_PAYLOAD_G711A    	0x08
#define RTP_PAYLOAD_COMF_NOISE  0x0D
#define RTP_PAYLOAD_G729	0x12
#define RTP_PAYLOAD_GSM    	0x03
#define RTP_PAYLOAD_MARKER_BIT	0x80
#define PAYLOAD(r)              (((r)->RtpMPT) & (~RTP_PAYLOAD_MARKER_BIT))
#define RTP_DTMF_EBIT           0x80
#define RTP_DTMF_VOLUMEMASK     0x3F
#define JITTERQ_SIZE	          512
#define PKLATE(c,r)             (((r)<(c)) && (((c)-(r))<32000))    // check if rxed seq-number is less than current but handle wrap
#define H263SPACE               (IP_MTU-RTP_HEADER_SIZE-UDP_HEADER_SIZE-sizeof(H263_RFC2190_HDR))

#define DTMF_STAR 10
#define DTMF_HASH 11
#define DTMF2CHAR(d) ((d)>DTMF_HASH ? '?' : ((d)==DTMF_STAR ? '*' : ((d) == DTMF_HASH ? '#' : ((d)+'0'))))
#define CHAR2DTMF(c) ((c)=='#' ? DTMF_HASH : ((c)=='*' ? DTMF_STAR : ((c).latin1())))



class codec
{
public:
    codec();
    virtual ~codec();
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou();
    virtual int bandwidth() { return 0; };
private:
};

#define JB_REASON_OK          0   // Buffer which matches seq-number returned
#define JB_REASON_EMPTY       1   // No buffers queued
#define JB_REASON_MISSING     2   // Buffers exist, but none which match your seq-num
#define JB_REASON_SEQERR      3   // Buffers exist, but their seq-numbers are nowhere near yours
#define JB_REASON_DTMF        4   // Buffer which matches seq-number contained DTMF
#define JB_REASON_DUPLICATE   5   // Got the same sequence number twice

class Jitter : public Q3PtrList<RTPPACKET>
{
public:
    Jitter();
    ~Jitter();
    RTPPACKET *GetJBuffer();
    void       FreeJBuffer(RTPPACKET *Buf);
    void       InsertDTMF(RTPPACKET *Buffer);
    void       InsertJBuffer(RTPPACKET *Buffer);
    RTPPACKET *DequeueJBuffer(ushort seqNum, int &reason);  
    int        DumpAllJBuffers(bool StopAtMarkerBit);
    virtual int compareItems(Q3PtrCollection::Item s1, Q3PtrCollection::Item s2);
    int        AnyData() { return count(); };
    bool       isPacketQueued(ushort Seq);
    int        countPacketsInFrontOf(ushort Seq);
    int        GotAllBufsInFrame(ushort seq, int offset);
    void       CountMissingPackets(ushort seq, ushort &cntValid, ushort &cntMissing);
    void       Debug();


private:
    Q3PtrList<RTPPACKET> FreeJitterQ;
};


class TxShaper
{
public:
    TxShaper(int bw, int period, int granularity);
    ~TxShaper();
    bool OkToSend();
    void Send(int Bytes);
    void setMaxBandwidth(int bytesPerSec) {maxBandwidth = bytesPerSec; }
    
private:    
    void flushHistory();
    int historySize;
    int *txHistory;
    int txGranularity;
    int txWindowTotal;
    int maxBandwidth;
    int itTail;
    int itHead;
    QTime timestamp;
    int timeLastFlush;
    int timeLastSend;
};


enum rtpTxMode { RTP_TX_AUDIO_FROM_BUFFER=1, RTP_TX_AUDIO_FROM_MICROPHONE=2, RTP_TX_AUDIO_SILENCE=3, RTP_TX_VIDEO=4 };
enum rtpRxMode { RTP_RX_AUDIO_TO_BUFFER=1,   RTP_RX_AUDIO_TO_SPEAKER=2,      RTP_RX_AUDIO_DISCARD=3, RTP_RX_VIDEO=4 };

class rtpListener : public QThread
{
public:
    rtpListener(Q3SocketDevice *s, QWaitCondition *w) { sock=s; cond=w; killThread=false; start(); }
    ~rtpListener() { killThread=true; wait(); }
    virtual void run() { while (!killThread) { if (sock->waitForMore(2000) > 0) cond->wakeAll(); }}

private:
    Q3SocketDevice *sock;
    QWaitCondition *cond;
    bool killThread;
};



class rtp : public QThread
{

public:
    rtp(QWidget *callingApp, int localPort, QString remoteIP, int remotePort, int mediaPay, int playout, int dtmfPay, QString micDev, QString spkDev, rtpTxMode txm=RTP_TX_AUDIO_FROM_MICROPHONE, rtpRxMode rxm=RTP_RX_AUDIO_TO_SPEAKER);
    ~rtp();
	virtual void run();

    void Transmit(short *pcmBuffer, int Samples);
    void Transmit(int ms);
    void Record(short *pcmBuffer, int Samples);
    void StopTransmitRecord() { rtpMutex.lock(); txMode=RTP_TX_AUDIO_SILENCE; rxMode=RTP_RX_AUDIO_DISCARD; if (txBuffer) delete txBuffer; txBuffer=0; recBuffer=0; recBufferMaxLen=0; rtpMutex.unlock(); };
    bool Finished()           { rtpMutex.lock(); bool b = ((txBuffer == 0) && (recBuffer == 0)); rtpMutex.unlock(); return b;};
    int  GetRecordSamples()   { rtpMutex.lock(); int s = recBufferLen; rtpMutex.unlock(); return s;};
    bool checkDtmf()          { rtpMutex.lock(); bool b=(dtmfIn[0] != 0); rtpMutex.unlock(); return b; }
    QString getDtmf()         { rtpMutex.lock(); QString s = dtmfIn; dtmfIn = ""; rtpMutex.unlock(); return s; }
    void sendDtmf(char d)     { rtpMutex.lock(); dtmfOut.append(d); rtpMutex.unlock(); }
    bool toggleMute()         { micMuted = !micMuted; return micMuted; }
    void getPower(short &m, short &s) { m = micPower; s = spkPower; micPower = 0; spkPower = 0; }
    bool queueVideo(VIDEOBUFFER *v) { bool res=false; rtpMutex.lock(); if (videoToTx==0) {videoToTx=v; if (eventCond) eventCond->wakeAll(); res=true; } else framesOutDiscarded++; rtpMutex.unlock(); return res; }
    bool readyForVideo() { bool res=true; rtpMutex.lock(); if (pTxShaper) res = pTxShaper->OkToSend(); rtpMutex.unlock(); return res; }
    VIDEOBUFFER *getRxedVideo()     { rtpMutex.lock(); VIDEOBUFFER *b=rxedVideoFrames.take(0); rtpMutex.unlock(); return b; }
    VIDEOBUFFER *getVideoBuffer(int len=0);
    void freeVideoBuffer(VIDEOBUFFER *Buf);
    void PlayToneToSpeaker(short *tone, int Samples);
    void setMaxBandwidth(int kbps) { rtpMutex.lock(); if (pTxShaper) pTxShaper->setMaxBandwidth(kbps*1000/8); rtpMutex.unlock(); }
    int  getCodecBandwidth() { int res=0; rtpMutex.lock(); if (Codec) res = Codec->bandwidth(); rtpMutex.unlock(); return res; }
    void changePlayoutDelay(int ms) { rtpMutex.lock(); adjustPlayoutDelay = ms; rtpMutex.unlock(); }

private:
    void rtpThreadWorker();
    void rtpAudioThreadWorker();
    void rtpVideoThreadWorker();
    void rtpInitialise();
    void OpenSocket();
    void CloseSocket();
    void StreamInVideo();
    int  appendVideoPacket(VIDEOBUFFER *picture, int curLen, RTPPACKET *JBuf, int mLen);
    void StreamInAudio();
    void PlayOutAudio();
    bool isSpeakerHungry();
    void recordInPacket(short *data, int dataBytes);
    void HandleRxDTMF(RTPPACKET *RTPpacket);
    void SendWaitingDtmf();
    void StreamOut(void* pData, int nLen);
    void StreamOut(RTPPACKET &RTPpacket);
    void fillPacketwithSilence(RTPPACKET &RTPpacket);
    bool fillPacketfromMic(RTPPACKET &RTPpacket);
    void fillPacketfromBuffer(RTPPACKET &RTPpacket);
    void initVideoBuffers(int Num);
    void destroyVideoBuffers();
    void transmitQueuedVideo();
    void AddToneToAudio(short *buffer, int Samples);
    void Debug(QString dbg);
    void CheckSendStatistics();
    void SendRtcpStatistics();
    void RtcpSendReceive(bool forceSend=false);
    void sendRtcpSenderReport(uint txPkCnt, uint txOctetCnt, long rxSsrc, uint rxPkCnt, uint rxPkLost, ushort highestRxSeq);
    void parseRtcpMessage(RTCPPACKET *rtcpPacket, int len);
    int  GetAdjustPlayout();
    int  measurePlayoutDelay(int seqNum);
    bool CheckAnyAudioQueued(int i);


    AudioDriver *audio;
    DtmfFilter *DTMFFilter;
    
    QObject *eventWindow;
    QMutex rtpMutex;
    Q3SocketDevice *rtpSocket;
    Q3SocketDevice *rtcpSocket;
    QWaitCondition *eventCond;
    QTime timeNextRtcpTx;
    codec   *Codec;
    Jitter *pJitter;
    TxShaper *pTxShaper;
    int rxMsPacketSize;
    int txMsPacketSize;
    int rxPCMSamplesPerPacket;
    int txPCMSamplesPerPacket;
    ushort rxSeqNum;
    bool rxFirstFrame;
    ushort txSequenceNumber;
    ulong txTimeStamp;
    uchar rtpMPT;
    uchar rtpMarker;
    ulong peerSsrc;
    QHostAddress yourIP;
    int myPort, yourPort, myRtcpPort, yourRtcpPort;
    rtpTxMode txMode;
    rtpRxMode rxMode;
    bool oobError;
    bool killRtpThread;
    short *txBuffer;
    int txBufferLen, txBufferPtr;
    ulong lastDtmfTimestamp;
    QString dtmfIn;
    QString dtmfOut;
    short *recBuffer;
    int recBufferLen, recBufferMaxLen;
    int audioPayload;
    int dtmfPayload;
    bool micMuted;
    int adjustPlayoutDelay;

    short *ToneToSpk;
    int ToneToSpkSamples;
    int ToneToSpkPlayed;

    // Video
    int videoPayload;
    Q3PtrList<VIDEOBUFFER> FreeVideoBufferQ;
    Q3PtrList<VIDEOBUFFER> rxedVideoFrames;
    VIDEOBUFFER *videoToTx;
    int videoFrameFirstSeqNum;

    // Stats
    QTime timeNextStatistics;
    QTime timeLastStatistics;
    QTime timeLastRtcpStatistics;
    int pkIn;
    int pkOut;
    int pkMissed;
    int pkLate;
    int pkOutDrop;
    int pkInDisc;
    int bytesIn;
    int bytesOut;
    int bytesToSpeaker;
    int framesIn;
    int framesOut;
    int framesInDiscarded;
    int framesOutDiscarded;
    short micPower;
    short spkPower;
    uint prevRxPkLost;
    uint prevRxPkCnt;    
    int rtcpFractionLoss;
    int rtcpTotalLoss;
    
    int minPlayout;
    int maxPlayout;
    int totPlayout; // These two used to find avg
    int cntPlayout;
    int avgPlayout;
    int nominalPlayout;
};




#endif
