#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/time.h>
#include <ctime>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

#include <iostream>
using namespace std;

#include "transcode.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "osdtypes.h"
#include "remoteutil.h"
#include "mythcontext.h"

// This class is to act as a fake audio output device to store the data
// for reencoding.

class AudioReencodeBuffer : public AudioOutput
{
 public:
    AudioReencodeBuffer(int audio_bits, int audio_channels)
    {
        Reset();
        Reconfigure(audio_bits, audio_channels, 0);
        bufsize = 512000;
        audiobuffer = new unsigned char[bufsize];
    }

   ~AudioReencodeBuffer()
    {
        delete [] audiobuffer;
    }

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits,
                        int audio_channels, int audio_samplerate)
    {
        (void)audio_samplerate;
        bits = audio_bits;
        channels = audio_channels;
        bytes_per_sample = bits * channels / 8;
    }

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate)
    {
        eff_audiorate = (dsprate / 100);
    }

    virtual void SetBlocking(bool block) { (void)block; }
    virtual void Reset(void)
    {
        audiobuffer_len = 0;
    }

    // timecode is in milliseconds.
    virtual void AddSamples(char *buffer, int samples, long long timecode)
    {
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        memcpy(audiobuffer + audiobuffer_len, buffer, 
               samples * bytes_per_sample);
        audiobuffer_len += samples * bytes_per_sample;
        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;
    }

    virtual void AddSamples(char *buffers[], int samples, long long timecode)
    {
        int audio_bytes = bits / 8;
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
        {
            for(int chan = 0; chan < channels; chan++)
            {
                audiobuffer[audiobuffer_len++] = buffers[chan][itemp];
                if(bits == 16)
                    audiobuffer[audiobuffer_len++] = buffers[chan][itemp+1];
            }
        }

        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;
    }

    virtual void SetTimecode(long long timecode)
    {
        last_audiotime = timecode;
    }
    virtual bool GetPause(void)
    {
        return false;
    }
    virtual void Pause(bool paused)
    {
        (void)paused;
    }

    virtual int GetAudiotime(void)
    {
        return last_audiotime;
    }

    int bufsize;
    unsigned char *audiobuffer;
    int audiobuffer_len, channels, bits, bytes_per_sample, eff_audiorate;
    long long last_audiotime;
};

Transcode::Transcode(QSqlDatabase *db, ProgramInfo *pginfo)
{
    QString dbname = QString("transcode%1%2").arg(getpid()).arg(rand());
    nvpdb = new MythSqlDatabase(dbname);

    m_proginfo = pginfo;
    m_db = db;
    nvr = NULL;
    nvp = NULL;
    inRingBuffer = NULL;
    outRingBuffer = NULL;
    fifow = NULL;
    kfa_table = NULL;
}
Transcode::~Transcode()
{
    if (nvr)
        delete nvr;
    if (nvp)
        delete nvp;
    if (inRingBuffer)
        delete inRingBuffer;
    if (outRingBuffer)
        delete outRingBuffer;
    if (fifow)
        delete fifow;
    if (kfa_table)
    {
        while(! kfa_table->isEmpty())
        {
           delete kfa_table->last();
           kfa_table->removeLast();
        }
        delete kfa_table;
    }
    if (nvpdb)
        delete nvpdb;
}
void Transcode::ReencoderAddKFA(long curframe, long lastkey, long num_keyframes)
{
    long delta = curframe - lastkey;
    if (delta != 0 && delta != keyframedist)
    {
        struct kfatable_entry *kfate = new struct kfatable_entry;
        kfate->adjust = keyframedist - delta;
        kfate->keyframe_number = num_keyframes;
        kfa_table->append(kfate);
    }
}

bool Transcode::GetProfile(QString profileName, QString encodingType)
{
    if (profileName.lower() == "autodetect")
    {
        bool result = false;
        if (encodingType == "MPEG-2")
            result = profile.loadByGroup(m_db, "MPEG2", "Transcoders");
        if (encodingType == "MPEG-4" || encodingType == "RTjpeg")
            result = profile.loadByGroup(m_db, "RTjpeg/MPEG4",
                                         "Transcoders");
        if (! result)
        {
            cerr << "Couldn't find profile for : " << encodingType << endl;
            return false;
        }
    }
    else
    {
        bool isNum;
        int profileID;
        profileID = profileName.toInt(&isNum);
        // If a bad profile is specified, there will be trouble
        if (isNum && profileID > 0)
            profile.loadByID(m_db, profileID);
        else
        {
            cerr << "Couldn't find profile #: " << profileName << endl;
            return false;
        }
    }
    return true;
}

#define SetProfileOption(profile, name) { \
    int value = profile.byName(name)->getValue().toInt(); \
    nvr->SetOption(name, value); \
}

void TranscodeWriteText(void *ptr, unsigned char *buf, int len, int timecode, int pagenr)
{
  NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)ptr;
  nvr->WriteText(buf, len, timecode, pagenr);
}

int Transcode::TranscodeFile(char *inputname, char *outputname,
                              QString profileName,
                              bool honorCutList, bool framecontrol,
                              bool chkTranscodeDB, QString fifodir)
{ 
    int audioframesize;
    int audioFrame = 0;

    nvp = new NuppelVideoPlayer(nvpdb, m_proginfo);
    nvp->SetNoVideo();

    QDateTime curtime = QDateTime::currentDateTime();
    if (honorCutList && m_proginfo)
    {
        if (m_proginfo->IsEditing(m_db) || m_proginfo->IsCommProcessing(m_db))
            return REENCODE_CUTLIST_CHANGE;
        m_proginfo->SetMarkupFlag(MARK_UPDATED_CUT, false, m_db);
        curtime = curtime.addSecs(60);
    }

    // Input setup
    nvr = new NuppelVideoRecorder(NULL);
    inRingBuffer = new RingBuffer(inputname, false, false);
    nvp->SetRingBuffer(inRingBuffer);

    AudioOutput *audioOutput = new AudioReencodeBuffer(0, 0);
    AudioReencodeBuffer *arb = ((AudioReencodeBuffer*)audioOutput);
    nvp->SetAudioOutput(audioOutput);

    if (nvp->OpenFile(false) < 0)
        return REENCODE_ERROR;

    nvp->ReinitAudio();
    QString encodingType = nvp->GetEncodingType();
    bool copyvideo = false, copyaudio = false;

    QString vidsetting = NULL, audsetting = NULL;

    int video_width = nvp->GetVideoWidth();
    int video_height = nvp->GetVideoHeight();
    float video_frame_rate = nvp->GetFrameRate();

    kfa_table = new QPtrList<struct kfatable_entry>;

    if (fifodir == NULL)
    {
        if (!GetProfile(profileName, encodingType)) {
            return REENCODE_ERROR;
        }
        vidsetting = profile.byName("videocodec")->getValue();
        if (vidsetting == "MPEG-2")
            return REENCODE_MPEG2TRANS;
        // Recorder setup
        nvr->SetFrameRate(video_frame_rate);

        // this is ripped from tv_rec SetupRecording. It'd be nice to merge
        nvr->SetOption("inpixfmt", FMT_YV12);
        nvr->SetOption("width", video_width);
        nvr->SetOption("height", video_height);

        nvr->SetOption("tvformat", gContext->GetSetting("TVFormat"));
        nvr->SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

        if (vidsetting == "MPEG-4")
        {
            nvr->SetOption("codec", "mpeg4");

            SetProfileOption(profile, "mpeg4bitrate");
            SetProfileOption(profile, "mpeg4scalebitrate");
            SetProfileOption(profile, "mpeg4maxquality");
            SetProfileOption(profile, "mpeg4minquality");
            SetProfileOption(profile, "mpeg4qualdiff");
            SetProfileOption(profile, "mpeg4optionvhq");
            SetProfileOption(profile, "mpeg4option4mv");
            nvr->SetupAVCodec();
        }
        else if (vidsetting == "RTjpeg")
        {
            nvr->SetOption("codec", "rtjpeg");
            SetProfileOption(profile, "rtjpegquality");
            SetProfileOption(profile, "rtjpegchromafilter");
            SetProfileOption(profile, "rtjpeglumafilter");
            nvr->SetupRTjpeg();
        }
        else
        {
            cerr << "Unknown video codec: " << vidsetting << endl;
        }

        audsetting = profile.byName("audiocodec")->getValue();
        nvr->SetOption("samplerate", arb->eff_audiorate);
        if (audsetting == "MP3")
        {
            nvr->SetOption("audiocompression", 1);
            SetProfileOption(profile, "mp3quality");
            copyaudio = true;
        }
        else if (audsetting == "Uncompressed")
        {
            nvr->SetOption("audiocompression", 0);
        }
        else
        {
            cerr << "Unknown audio codec: " << audsetting << endl;
        }

        nvr->AudioInit(true);

        outRingBuffer = new RingBuffer(outputname, true, false);
        nvr->SetRingBuffer(outRingBuffer);
        nvr->WriteHeader();
        nvr->StreamAllocate();
    }

    if (vidsetting == encodingType && !framecontrol &&
        fifodir == NULL && honorCutList)
    {
        copyvideo = true;
        VERBOSE(VB_GENERAL, "Reencoding video in 'raw' mode");
    }

    keyframedist = 30;
    nvp->InitForTranscode(copyaudio, copyvideo);

    VideoFrame frame;
    frame.codec = FMT_YV12;
    frame.width = video_width;
    frame.height = video_height;
    frame.size = video_width * video_height * 3 / 2;

    if (fifodir != NULL)
    {
        QString audfifo = fifodir + QString("/audout");
        QString vidfifo = fifodir + QString("/vidout");
        int audio_size = arb->eff_audiorate * arb->bytes_per_sample;
        // framecontrol is true if we want to enforce fifo sync.
        if (framecontrol)
            VERBOSE(VB_GENERAL, "Enforcing sync on fifos");
        fifow = new FIFOWriter::FIFOWriter(2, framecontrol);

        if (!fifow->FIFOInit(0, QString("video"), vidfifo, frame.size, 50) ||
            !fifow->FIFOInit(1, QString("audio"), audfifo, audio_size, 25))
        {
           cerr << "Error initializing fifo writer.  Aborting" << endl;
           unlink(outputname);
           return REENCODE_ERROR;
        }
        VERBOSE(VB_GENERAL, QString("Video %1x%2@%3fps Audio rate: %4")
                                   .arg(video_width).arg(video_height)
                                   .arg(video_frame_rate)
                                   .arg(arb->eff_audiorate));
        VERBOSE(VB_GENERAL, "Created fifos. Waiting for connection.");
    }

    bool forceKeyFrames = (fifow == NULL) ? framecontrol : false;

    QMap<long long, int>::Iterator dm_iter = NULL;
    bool writekeyframe = true;
   
    int num_keyframes = 0;

    int did_ff = 0; 

    long curFrameNum = 0;
    frame.frameNumber = 1;
    long lastKeyFrame = 0;
    long totalAudio = 0;
    int dropvideo = 0;
    long long lasttimecode = 0;

    float rateTimeConv = arb->eff_audiorate * arb->bytes_per_sample / 1000.0;
    float vidFrameTime = 1000.0 / video_frame_rate;
    int wait_recover = 0;
    VideoOutput *videoOutput = nvp->getVideoOutput();
    bool is_key = 0;
    bool first_loop = true;
    while (nvp->TranscodeGetNextFrame(dm_iter, &did_ff, &is_key, honorCutList))
    {
        if (first_loop)
        {
            copyaudio = nvp->GetRawAudioState();
            first_loop = false;
        }
        VideoFrame *lastDecode = videoOutput->GetLastDecodedFrame();
        frame.buf = lastDecode->buf;
        frame.timecode = lastDecode->timecode;

        if (frame.timecode < lasttimecode)
            frame.timecode = (long long)(lasttimecode + vidFrameTime);
        lasttimecode = frame.timecode;

        if (fifow)
        {
            totalAudio += arb->audiobuffer_len;
            int audbufTime = (int)(totalAudio / rateTimeConv);
            int auddelta = arb->last_audiotime - audbufTime;
            int vidTime = (int)(curFrameNum * vidFrameTime + 0.5);
            int viddelta = frame.timecode - vidTime;
            int delta = viddelta - auddelta;
            if (abs(delta) < 500 && abs(delta) >= vidFrameTime)
            {
               QString msg = QString("Audio is %1ms %2 video at # %3")
                          .arg(abs(delta))
                          .arg(((delta > 0) ? "ahead of" : "behind"))
                          .arg((int)curFrameNum);
                VERBOSE(VB_GENERAL, msg);
                dropvideo = (delta > 0) ? 1 : -1;
                wait_recover = 0;
            }
            else if (delta >= 500 && delta < 10000)
            {
                if (wait_recover == 0)
                {
                    dropvideo = 5;
                    wait_recover = 6;
                }
                else if (wait_recover == 1)
                {
                    // Video is badly lagging.  Try to catch up.
                    int count = 0;
                    while (delta > vidFrameTime)
                    {
                        fifow->FIFOWrite(0, frame.buf, frame.size);
                        count++;
                        delta -= (int)vidFrameTime;
                    }
                    QString msg = QString("Added %1 blank video frames")
                                  .arg(count);
                    curFrameNum += count;
                    dropvideo = 0;
                    wait_recover = 0;
                }
                else
                    wait_recover--;
            }
            else
            {
                dropvideo = 0;
                wait_recover = 0;
            }

            // int buflen = (int)(arb->audiobuffer_len / rateTimeConv);
            // cout << curFrameNum << ": video time: " << frame.timecode
            //      << " audio time: " << arb->last_audiotime << " buf: "
            //      << buflen << " exp: "
            //      << audbufTime << " delta: " << delta << endl;
            if (arb->audiobuffer_len)
                fifow->FIFOWrite(1, arb->audiobuffer, arb->audiobuffer_len);
            if (dropvideo < 0)
            {
                dropvideo++;
                curFrameNum--;
            }
            else
            {
                fifow->FIFOWrite(0, frame.buf, frame.size);
                if (dropvideo)
                {
                    fifow->FIFOWrite(0, frame.buf, frame.size);
                    curFrameNum++;
                    dropvideo--;
                }
            }
            videoOutput->DoneDisplayingFrame();
            audioOutput->Reset();
            nvp->FlushTxtBuffers();
        }
        else if (copyaudio)
        {
            // Encoding from NuppelVideo to NuppelVideo with MP3 audio
            // So let's not decode/reencode audio
            if (!nvp->GetRawAudioState()) 
            {
                // The Raw state changed during decode.  This is not good
                unlink(outputname);
                return REENCODE_ERROR;
            }

            if (forceKeyFrames) 
                writekeyframe = true;
            else
            {
                writekeyframe = is_key;
                if (writekeyframe)
                {
                    // Currently, we don't create new sync frames,
                    // (though we do create new 'I' frames), so we mark
                    // the key-frames before deciding whether we need a
                    // new 'I' frame.

                    //need to correct the frame# and timecode here
                    // Question:  Is it necessary to change the timecodes?
                    long sync_offset = nvp->UpdateStoredFrameNum(curFrameNum);
                    nvr->UpdateSeekTable(num_keyframes, false, sync_offset);
                    ReencoderAddKFA(curFrameNum, lastKeyFrame, num_keyframes);
                    num_keyframes++;
                    lastKeyFrame = curFrameNum;

                    if (did_ff)
                        did_ff = 0;
                }
            }

            if (! nvp->WriteStoredData(outRingBuffer, (did_ff == 0)))
            {
                if (did_ff == 1)
                {
                  // Create a new 'I' frame if we just processed a cut.
                  did_ff = 2;
                  writekeyframe = true;
                }
                nvr->WriteVideo(&frame, true, writekeyframe);
            }
            audioOutput->Reset();
            nvp->FlushTxtBuffers();
        } 
        else 
        {
            // audio is fully decoded, so we need to reencode it
            audioframesize = arb->audiobuffer_len;
            if (audioframesize > 0)
            {
                nvr->SetOption("audioframesize", audioframesize);
                int starttime = audioOutput->GetAudiotime();
                nvr->WriteAudio(arb->audiobuffer, audioFrame++, starttime);
                arb->audiobuffer_len = 0;
            }
            nvp->TranscodeWriteText(&TranscodeWriteText, (void *)(nvr));

            if (forceKeyFrames)
                nvr->WriteVideo(&frame, true, true);
            else
                nvr->WriteVideo(&frame);
        }

        if (QDateTime::currentDateTime() > curtime) 
        {
            if (honorCutList && 
                m_proginfo->CheckMarkupFlag(MARK_UPDATED_CUT, m_db)) 
            {
                unlink(outputname);
                return REENCODE_CUTLIST_CHANGE;
            }

            if (chkTranscodeDB)
            {
                QString query = QString("SELECT * FROM transcoding WHERE "
                        "chanid = '%1' AND starttime = '%2' AND "
                        "((status & %3) = %4) AND hostname = '%5';")
                        .arg(m_proginfo->chanid)
                        .arg(m_proginfo->startts.toString("yyyyMMddhhmmss"))
                        .arg(TRANSCODE_STOP)
                        .arg(TRANSCODE_STOP)
                        .arg(gContext->GetHostName());
                MythContext::KickDatabase(m_db);
                QSqlQuery result = m_db->exec(query);
                if (result.isActive() && result.numRowsAffected() > 0)
                {
                    unlink(outputname);
                    return REENCODE_ERROR;
                }
            }
            curtime = QDateTime::currentDateTime();
            curtime = curtime.addSecs(60);
        }

        curFrameNum++;
        frame.frameNumber = 1 + (curFrameNum << 1);
    }

    nvr->WriteSeekTable();
    if (!kfa_table->isEmpty())
        nvr->WriteKeyFrameAdjustTable(kfa_table);
    if (fifow) 
        fifow->FIFODrain();
    return REENCODE_OK;
}

