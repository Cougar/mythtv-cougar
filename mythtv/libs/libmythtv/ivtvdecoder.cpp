#include <iostream>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

#include "ivtvdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcontext.h"

#include "videoout_ivtv.h"
#include "videodev_myth.h"

IvtvDecoder::IvtvDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                         ProgramInfo *pginfo)
           : DecoderBase(parent)
{
    m_db = db;
    m_playbackinfo = pginfo;

    framesPlayed = 0;
    framesRead = 0;

    hasFullPositionMap = false;

    keyframedist = 15;

    exitafterdecoded = false;
    ateof = false;
    gopset = false;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;
    validvpts = false;

    lastKey = 0;

    prvpkt[0] = prvpkt[1] = prvpkt[2] = 0;
}

IvtvDecoder::~IvtvDecoder()
{
}

void IvtvDecoder::SeekReset(int skipframes)
{
    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();
    videoout->Reopen(skipframes, framesPlayed);
}

void IvtvDecoder::Reset(void)
{
    SeekReset();

    positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;
}

bool IvtvDecoder::CanHandle(char testbuf[2048], const QString &filename)
{
    // remove to enable..
    return false;

    int testfd = open("/dev/video16", O_RDWR);
    if (testfd <= 0)
        return false;

    bool ok = false;

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(testfd, VIDIOC_QUERYCAP, &vcap) >= 0)
    {
        if (vcap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
            ok = true;
    }

    // probe for pal/ntsc while we're here..
    
    close(testfd);

    if (!ok)
        return false;

    av_register_all();

    AVProbeData probe;

    probe.filename = (char *)(filename.ascii());
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = 2048;

    AVInputFormat *fmt = av_probe_input_format(&probe, true);

    if (!strcmp(fmt->name, "mpeg"))
        return true;
    return false;
}

int IvtvDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                              char testbuf[2048])
{
    (void)novideo;
    (void)testbuf;

    ringBuffer = rbuffer;

    m_parent->ForceVideoOutputType(kVideoOutput_IVTV);

    // need to probe pal/ntsc
    m_parent->SetVideoParams(720, 480, 29.97, 15, 1.5);
             
    ringBuffer->CalcReadAheadThresh(8000);

    if (m_playbackinfo && m_db)
    {
        m_playbackinfo->GetPositionMap(positionMap, MARK_GOP_START, m_db);
        if (positionMap.size() && !livetv && !watchingrecording)
        {
            long long totframes = positionMap.size() * keyframedist;
            int length = (int)((totframes * 1.0) / fps);
            m_parent->SetFileLength(length, totframes);            
            hasFullPositionMap = true;
            gopset = true;
        }
    }

    if (!hasFullPositionMap)
    {
        int bitrate = 8000;
        float bytespersec = (float)bitrate / 8 / 2;
        float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;

        m_parent->SetFileLength((int)(secs), (int)(secs * fps));
    }

    if (hasFullPositionMap)
        VERBOSE(VB_PLAYBACK, "Position map found");

    return hasFullPositionMap;
}

#define VID_START     0x000001e0
#define VID_END       0x000001ef

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void IvtvDecoder::MpegPreProcessPkt(unsigned char *buf, int len, 
                                    long long startpos)
{
    unsigned char *bufptr = buf;
    unsigned int state = 0xFFFFFFFF, v = 0;
    
    int prvcount = -1;

    while (bufptr < buf + len)
    {
        if (++prvcount < 3)
            v = prvpkt[prvcount];
        else
            v = *bufptr++;

        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                continue;

            if (state >= VID_START && state <= VID_END)
            {
                laststartpos = (bufptr - buf) + startpos - 4;
                continue;
            }

            switch (state)
            {
                case GOP_START:
                {
                    int frameNum = framesRead;

                    if (!gopset && frameNum > 0)
                    {
                        if (firstgoppos > 0)
                        {
                            keyframedist = frameNum - firstgoppos;
                            gopset = true;
                            m_parent->SetVideoParams(-1, -1, -1, keyframedist);
                        }
                        else
                            firstgoppos = frameNum;
                    }

                    lastKey = frameNum;
                    if (!hasFullPositionMap)
                        positionMap[lastKey / keyframedist] = laststartpos;

                    break;
                }
                case PICTURE_START:
                {
                    framesRead++;
                    if (exitafterdecoded)
                        gotvideo = 1;
                    break;
                }
                default:
                    break;
            }
            continue;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }

    memcpy(prvpkt, buf + len - 3, 3);
}

void IvtvDecoder::GetFrame(int onlyvideo)
{
    gotvideo = false;
    frame_decoded = 0;

    bool allowedquit = false;

    unsigned char buf[256001];

    VideoOutputIvtv *videoout = (VideoOutputIvtv *)m_parent->getVideoOutput();
    long long startpos = ringBuffer->GetReadPosition();
    int count = 0;
    int newframes = 0;

    while (!allowedquit)
    {
        count = ringBuffer->Read(buf, 131072);

        MpegPreProcessPkt(buf, count, startpos);
        if (onlyvideo >= 0)
            newframes = videoout->WriteBuffer(buf, count);

        allowedquit = true;
    }                    

    if (newframes > framesPlayed)
        framesPlayed = newframes;

    m_parent->SetFramesPlayed(framesPlayed);
}

bool IvtvDecoder::DoRewind(long long desiredFrame)
{
    lastKey = (framesPlayed / keyframedist) * keyframedist;
    long long storelastKey = lastKey;

    while (lastKey > desiredFrame)
    {
        lastKey -= keyframedist;
    }

    if (lastKey < 0)
        lastKey = 0;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = positionMap[lastKey / keyframedist];
    long long curPosition = ringBuffer->GetTotalReadPosition();
    long long diff = keyPos - curPosition;

    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += keyframedist;
        keyPos = positionMap[lastKey / keyframedist];
        if (keyPos == 0)
            continue;
        diff = keyPos - curPosition;
        normalframes = 0;
        if (lastKey > storelastKey)
        {
            lastKey = storelastKey;
            diff = 0;
            normalframes = 0;
            return false;
        }
    }

    int iter = 0;

    while (keyPos == 0 && lastKey > 1 && iter < 4)
    {
        VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                                     .arg((int)lastKey));

        lastKey -= keyframedist;
        keyPos = positionMap[lastKey / keyframedist];
        diff = keyPos - curPosition;
        normalframes += keyframedist;

        if (keyPos != 0)
           VERBOSE(VB_PLAYBACK, QString("Using seek position: %1")
                                       .arg((int)lastKey));

        iter++;
    }

    if (keyPos == 0)
    {
        VERBOSE(VB_GENERAL, QString("Unknown seek position: %1")
                                    .arg((int)lastKey));

        lastKey = storelastKey;
        diff = 0;
        normalframes = 0;

        return false;
    }

    ringBuffer->Seek(diff, SEEK_CUR);

    framesPlayed = lastKey;
    framesRead = lastKey;

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    SeekReset(normalframes);

    m_parent->SetFramesPlayed(framesPlayed);
    return true;
}

bool IvtvDecoder::DoFastForward(long long desiredFrame)
{
    lastKey = (framesPlayed / keyframedist) * keyframedist;
    long long number = desiredFrame - framesPlayed;
    long long desiredKey = lastKey;

    while (desiredKey <= desiredFrame)
    {
        desiredKey += keyframedist;
    }
    desiredKey -= keyframedist;
   
    int normalframes = desiredFrame - desiredKey;

    if (desiredKey == lastKey)
        normalframes = number;

    long long keyPos = -1;

    long long tmpKey = desiredKey;
    int tmpIndex = tmpKey / keyframedist;

    while (keyPos == -1 && tmpKey >= lastKey)
    {
        if (positionMap.find(tmpIndex) != positionMap.end())
        {
            keyPos = positionMap[tmpIndex];
        }
        else if (livetv || (watchingrecording && nvr_enc &&
                            nvr_enc->IsValidRecorder()))
        {
            for (int i = lastKey / keyframedist; i <= tmpIndex; i++)
            {
                if (positionMap.find(i) == positionMap.end())
                    nvr_enc->FillPositionMap(i, tmpIndex, positionMap);
            }
            keyPos = positionMap[tmpIndex];
        }

        if (keyPos == -1 && (hasFullPositionMap || livetv || 
                             (watchingrecording && nvr_enc)))
        {
            VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                    .arg((int)tmpKey));

            tmpKey -= keyframedist;
            tmpIndex--;
        }
        else if (keyPos == -1)
        {
            break;
        }
        else
        {
            desiredKey = tmpKey;
        }
    }

    if (keyPos == -1 && desiredKey != lastKey && !livetv && !watchingrecording)
    {
        VERBOSE(VB_IMPORTANT, "Did not find keyframe position.");

        VERBOSE(VB_PLAYBACK, QString("lastKey: %1 desiredKey: %2")
                .arg((int)lastKey).arg((int)desiredKey));

        while (framesRead < desiredKey + 1 || 
               !positionMap.contains(desiredKey / keyframedist))
        {
            exitafterdecoded = true;
            GetFrame(-1);
            exitafterdecoded = false;

            if (ateof)
                return false;
        }

        keyPos = positionMap[desiredKey / keyframedist];
    }

    if (keyPos == -1)
    {
        cerr << "Didn't find keypos, time to bail ff\n";
        return false;
    }

    lastKey = desiredKey;
    long long diff = keyPos - ringBuffer->GetTotalReadPosition();

    ringBuffer->Seek(diff, SEEK_CUR);

    framesPlayed = framesRead = lastKey;

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    SeekReset(normalframes);

    m_parent->SetFramesPlayed(framesPlayed);
    return true;
}

void IvtvDecoder::SetPositionMap(void)
{
    if (m_playbackinfo && m_db)
        m_playbackinfo->SetPositionMap(positionMap, MARK_GOP_START, m_db);
}

