#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
#include "minilzo.h"
#include "XJ.h"
#include "effects.h"

NuppelVideoPlayer::NuppelVideoPlayer(void)
{
    playing = false;
    audiofd = -1;
    filename = "output.nuv";

    eof = 0;
    buf = NULL;
    buf2 = NULL;
    lastct = '1';

    gf = NULL;
    rtjd = NULL;
 
    lastaudiolen = 0;
    strm = NULL;
    wpos = rpos = 0;
    audiolen = 0;
    fafterseek = 0;

    paused = 0;
    deinterlace = 0;

    audiodevice = "/dev/dsp";
    audiodelay = 0;

    ringBuffer = NULL;
    weMadeBuffer = false;

    osd = NULL;
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (gf)
        lame_close(gf);
    if (rtjd)
        delete rtjd;
    if (weMadeBuffer)
        delete ringBuffer;
    if (osd)
        delete osd;
}

void NuppelVideoPlayer::InitSound(void)
{
    int bits = 16, stereo = 1, speed = 44100;

    audiofd = open(audiodevice.c_str(), O_WRONLY);
    if (audiofd == -1)
    {
        fprintf(stderr, "Can't open audio device\n");
	return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &bits) < 0)
    {
        fprintf(stderr, "problem setting sample size\n");
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) < 0) 
    {
        fprintf(stderr, "problem setting to stereo\n");
        close(audiofd);
        audiofd = -1;
        return;
    }

    if (ioctl(audiofd, SNDCTL_DSP_SPEED, &speed) < 0) 
    {
        fprintf(stderr,"problem setting sample rate\n");
        close(audiofd);
        audiofd = -1;
        return;
    }
}

void NuppelVideoPlayer::WriteAudio(unsigned char *aubuf, int size)
{
    if (audiofd <= 0)
        return;

    unsigned char *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) && 
           ((lw = write(audiofd, tmpbuf, size - written)) > 0))
    {
        if (lw == -1)
        {
            fprintf(stderr, "Error writing to audio device\n");
            close(audiofd);
            audiofd = -1;
            return;
        }
        written += lw;
        tmpbuf += lw;
    }
}

int NuppelVideoPlayer::InitSubs(void)
{
    gf = lame_init();
    lame_set_decode_only(gf, 1);
    lame_decode_init();
    lame_init_params(gf);

    int i;
    rtjd = new RTjpeg();
    i = RTJ_YUV420;
    rtjd->SetFormat(&i);

    if (lzo_init() != LZO_E_OK)
    {
        fprintf(stderr, "lzo_init() failed");
        return -1;
    }

    positionMap = new map<long long, long long>;
    return 0;
}

int NuppelVideoPlayer::OpenFile(bool skipDsp)
{
    struct rtframeheader frameheader;
    int    startpos;
    int    foundit=0;
    char   ftype;
    char   *space;

    if (!skipDsp)
    {
        if (!ringBuffer)
        {
            ringBuffer = new RingBuffer(filename.c_str(), true, false);
            weMadeBuffer = true;
	    livetv = false;
        }
        else 
            livetv = true;

        if (!ringBuffer->IsOpen())
        {
            fprintf(stderr, "File not found: %s\n", filename.c_str());
            return -1;
        }
    }
    ringBuffer->Read(&fileheader, FILEHEADERSIZE);

    if (!skipDsp)
    {
        video_width = fileheader.width;
        video_height = fileheader.height;
        video_frame_rate = fileheader.fps;
        eof = 0;
    }

    space = new char[video_width * video_height * 3 / 2];

    if (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, FRAMEHEADERSIZE))
    {
        fprintf(stderr, "File not big enough for a header\n");
        return -1;
    }
    if (frameheader.frametype != 'D') 
    {
        fprintf(stderr, "Illegal file format\n");
        return -1;
    }
    if (frameheader.packetlength != ringBuffer->Read(space, 
                                                     frameheader.packetlength))
    {
        fprintf(stderr, "File not big enough for first frame data\n");
        return -1;
    }

    if ((video_height & 1) == 1)
    {
        video_height--;
        fprintf(stderr, "Incompatible video height, reducing to %d\n", 
                video_height);
    }

    if (skipDsp)
    {
        delete [] space;
        return 0;
    }

    startpos = ringBuffer->Seek(0, SEEK_CUR);

    ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);

    foundit = 0;
    effdsp = 44100;

    while (!foundit) 
    {
        ftype = ' ';
        if (frameheader.frametype == 'S') 
        {
            if (frameheader.comptype == 'A')
            {
                effdsp = frameheader.timecode;
                if (effdsp > 0)
                {
                    foundit = 1;
                    continue;
                }
            }
        }
        if (frameheader.frametype != 'R' && frameheader.packetlength != 0)
        {
            if (frameheader.packetlength != ringBuffer->Read(space, 
                                                 frameheader.packetlength))
            {
                foundit = 1;
                continue;
            }
        }

        foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, 
                                                       FRAMEHEADERSIZE));
    }

    delete [] space;

    ringBuffer->Seek(startpos, SEEK_SET);

    return 0;
}
      
unsigned char *NuppelVideoPlayer::DecodeFrame(struct rtframeheader *frameheader,
                                              unsigned char *lstrm)
{
    int r;
    unsigned int out_len;
    int compoff = 0;

    if (!buf2)
    {
        buf2 = new unsigned char[video_width * video_height * 3 / 2];
        planes[0] = buf;
        planes[1] = planes[0] + video_width * video_height;
        planes[2] = planes[1] + (video_width * video_height) / 4;
    }

    if (frameheader->frametype == 'V') {
        if (frameheader->comptype == 'N') {
            memset(buf, 0,  video_width * video_height);
            memset(buf + video_width * video_height, 127,
                   (video_width * video_height)/2);
            return(buf);
        }
        if (frameheader->comptype == 'L') {
            switch(lastct) {
                case '0':
                case '3': return(buf2); break;
                case '1':
                case '2': return(buf); break;
                default: return(buf);
            }
        }
    }

    //keyframe = frameheader->keyframe==0;
    //if (keyframe) {
    //}

    if (frameheader->comptype == '0' || frameheader->comptype == '1') 
        compoff=1;
    else if (frameheader->comptype == '2' || frameheader->comptype == '3') 
        compoff=0;

    lastct = frameheader->comptype;

    if (!compoff) 
    {
        r = lzo1x_decompress(lstrm, frameheader->packetlength, buf2, &out_len,
                              NULL);
        if (r != LZO_E_OK) 
        {
            fprintf(stderr,"\nminilzo: can't decompress illegal data, "
                    "ft='%c' ct='%c' len=%d tc=%d\n",
                    frameheader->frametype, frameheader->comptype,
                    frameheader->packetlength, frameheader->timecode);
        }
    }

    if (frameheader->frametype=='V' && frameheader->comptype == '0') 
    {
        memcpy(buf2, lstrm, (int)(video_width * video_height*1.5));
        return(buf2);
    }

    if (frameheader->frametype=='V' && frameheader->comptype == '3') 
        return(buf2);

    if (compoff) 
        rtjd->Decompress((int8_t*)lstrm, planes);
    else
        rtjd->Decompress((int8_t*)buf2, planes);

    return(buf);
}

int NuppelVideoPlayer::ComputeByteShift(void)
{
    int tcshift;
    int byteshift;
    int audio_delay, video_delay;

    /* We want to calculate 'tcshift'.  The audio is playing 'tcshift'
       milliseconds ahead of the video.  Negative tcshift means the video 
       is ahead of the audio.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just 
       been written into the buffer.

       'audiolen' is the # of bytes stored in the audio buffer

       'timecodes[rpos]' is the timecode of video about to be read 
       out of the buffer, ideally the timecode of audio about to be read
       out of the buffer will match this...

       Begin algebra:
 
       tcshift = <timecode of audio about to be read from buffer> - 
                 <timecode of video about to be read from buffer>

       tcshift = (audiotimecode - (audiolen * (ms/byte))) -
                 timecodes[rpos]

       'ms/byte' is given by '25000/effdsp'...

       tcshift = (audiotimecode - (audiolen * 25000 / effdsp)) -
                 timecodes[rpos]
    */

    tcshift = (int)((double)audiotimecode - 
              (double)audiolen * 25000.0 / (double)effdsp -
              (double)timecodes[rpos]);

    /* The tcshift computed above describes the time shift between audio and
       video output from GetFrame().  Now we adjust this to find the time shift
       between audio and video data leaving the computer.  Fortunately, each
       stream has a predictable delay, between leaving GetFrame() and 
       reaching a DAC...
    */

    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &audio_delay); // bytes
    // convert bytes to ms :
    audio_delay = (int)((double)audio_delay * 25000.0 / (double)effdsp);
    video_delay = usepre * 1000 / (int)video_frame_rate; // ms stuck in prebuf
 
    tcshift -= audio_delay;
    tcshift += video_delay;

    byteshift = 4 * (int)(tcshift * (double)effdsp / (100000.0));

    if (0)
    {
        printf("audio_delay = '%d' video_delay = '%d' tcshift = '%d' "
               "byteshift = '%d'\n", audio_delay, video_delay, tcshift,
               byteshift);
    }

    return byteshift;
}

unsigned char *NuppelVideoPlayer::GetFrame(int *timecode, int onlyvideo,
                                           unsigned char **audiodata, int *alen)
{
    unsigned char *ret;
    int gotvideo = 0;
    int gotaudio = 0;
    int bytesperframe;

    int shiftcorrected = 0;
    int i;
    int seeked = 0;

    if (weseeked)
    {
        seeked = 1;
        weseeked = 0;
    }

    if (buf==NULL) {
        buf = new unsigned char[video_width *video_height * 3 / 2];
        strm = new unsigned char[video_width * video_height * 2];

        // initialize and purge buffers
        for (i = 0; i < MAXVBUFFER; i++) {
            vbuffer[i] = new unsigned char[video_width * video_height * 3 / 2];
            bufstat[i]=0;
            timecodes[i]=0;
        }
        wpos = 0;
        rpos = 0;
        audiolen = 0;
        audiotimecode = 0;
        seeked = 1;
    }

    bytesperframe = 4 * (int)((1.0/video_frame_rate) * 
                    ((double)effdsp/100.0)+0.5);

    gotvideo = 0;
    if (onlyvideo > 0)
        gotaudio = 1;
    else
        gotaudio = 0;

    while (!gotvideo || !gotaudio)
    {
        if (!gotvideo && bufstat[rpos] == 1)
            gotvideo = 1;
 
        if (!gotaudio && (audiolen >= bytesperframe))
            gotaudio = 1;
    
        if (gotvideo && gotaudio)
        {
            if (shiftcorrected || onlyvideo > 0)
                continue;
            if (!seeked) 
            {
                bytesperframe -= (ComputeByteShift() >> 5) << 2; // div by 8
                // which corrects 1/8 of the shift on each frame.
                // Also rounds to multiple of 4.
                
                if (bytesperframe < 0) 
                    bytesperframe = 0;
                if (bytesperframe > audiolen)
                    bytesperframe = audiolen;

                //printf("bytesperframe = '%d' effdsp = '%d'\n", 
                //       bytesperframe, effdsp);
            } 
            else
            {
                // Typically, after a seek, the calculated 'byteshift' becomes
                // very large.  We'd like to get back to steady state ASAP,
                // to avoid pops in the sound, or jerky video...
                int byteshift = ComputeByteShift();

                if (byteshift > 0)
                {
                    audiolen += byteshift;
                    if (audiolen > AUDBUFSIZE)
                        audiolen = AUDBUFSIZE;
                    memset(audiobuffer, 0, audiolen);

                    //printf("Inserting silence\n");
                }
            }
            shiftcorrected=1;  
            if (audiolen >= bytesperframe) 
                continue;
            else
                gotaudio = 0;
        }

	long long currentposition = ringBuffer->GetReadPosition();

        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        {
            eof=1;
            return (buf);
        }

        if (frameheader.frametype == 'R') 
            continue; // the R-frame has no data packet

        if (frameheader.frametype == 'S') 
	{
            if (frameheader.comptype == 'A')
	    {
                if (frameheader.timecode > 0 && frameheader.timecode < 5500000)
		{
                    effdsp = frameheader.timecode;
		}
	    }
	    else if (frameheader.comptype == 'V')
            {
		(*positionMap)[framesPlayed / 30] = currentposition;
		lastKey = framesPlayed;
	    }
        }
	    
        if (frameheader.packetlength!=0) {
            if (ringBuffer->Read(strm, frameheader.packetlength) != 
                frameheader.packetlength) 
            {
                eof = 1;
                return (buf);
            }
        }

        if (frameheader.frametype=='V') 
        {
            if (onlyvideo>= 0) 
            {
                ret = DecodeFrame(&frameheader, strm);
            } 
            else 
            {
                ret = vbuffer[0];
            }
            memcpy(vbuffer[wpos], ret, (int)(video_width*video_height * 1.5));
            timecodes[wpos] = frameheader.timecode;
            bufstat[wpos]=1;
            wpos = (wpos+1) % MAXVBUFFER;
            continue;
        }

        if (frameheader.frametype=='A' && onlyvideo<=0) 
        {
            if (frameheader.comptype=='N' && lastaudiolen!=0) 
            {
                memset(strm, 0, lastaudiolen);
            }
            else if (frameheader.comptype=='3') 
            {
                int lameret = 0;
                short pcmlbuffer[44100]; 
                short pcmrbuffer[44100];
                int sampleswritten = 0;
                int audiobufferpos = audiolen;
                int packetlen = frameheader.packetlength;

                do 
                {
                    lameret = lame_decode(strm, packetlen, pcmlbuffer, 
                                          pcmrbuffer);

                    if (lameret > 0) 
                    {
                        int itemp = 0;

                        for (itemp = 0; itemp < lameret && 
                             audiobufferpos < AUDBUFSIZE; itemp++)
                        {
                            memcpy(&audiobuffer[audiobufferpos], 
                                   &pcmlbuffer[itemp], sizeof(short int));
                            audiobufferpos += 2;

                            memcpy(&audiobuffer[audiobufferpos], 
                                   &pcmrbuffer[itemp], sizeof(short int));
                            audiobufferpos += 2;
                        }
                        sampleswritten += lameret;
                    }
                    else if (lameret < 0)
                    {
                        printf("lame decode error\n");
                        exit(-1);
                    }
                    packetlen = 0;
                } while (lameret > 0);

                audiotimecode = frameheader.timecode + audiodelay; // untested

                audiolen += sampleswritten * 4;
                if (audiolen > AUDBUFSIZE)
                    audiolen = AUDBUFSIZE;
                lastaudiolen = audiolen;
            } 
            else 
            {
                if (audiolen + frameheader.packetlength < AUDBUFSIZE)
                {
                    memcpy(audiobuffer+audiolen, strm, 
                           frameheader.packetlength);
                    audiotimecode = frameheader.timecode + audiodelay;

                    audiolen += frameheader.packetlength;
                    lastaudiolen = audiolen;
                }
            }
        }
    }

    if (onlyvideo>0) 
        *alen = 0;
    else 
    {
        *alen = bytesperframe;
        memcpy(tmpaudio, audiobuffer, audiolen);
        audiolen -= bytesperframe;
        memcpy(audiobuffer, tmpaudio+bytesperframe, audiolen);
    }

    *audiodata = tmpaudio;

    fafterseek++;

    ret = vbuffer[rpos];
    bufstat[rpos] = 0; // clear flag
    rpos = (rpos+1) % MAXVBUFFER;

    return (ret);
}

#define MAXPREBUFFERS 60
void NuppelVideoPlayer::StartPlaying(void)
{
    unsigned char *X11videobuf;
    unsigned char *videobuf, *videobuf3;

    long tf;
    unsigned char *audiodata;
    int audiodatalen;

    int timecode;

    int prebuffered = 0;
    int actpre = 0;
    int fnum = 0;

    unsigned char *prebuf[MAXPREBUFFERS];
    
    int videosize; 
   
    usepre = 8;
 
    InitSubs();
    OpenFile();

    videosize = video_width * video_height * 3 / 2;

    if (fileheader.audioblocks != 0)
        InitSound();
  
    X11videobuf = XJ_init(video_width, video_height, "Mythical Convergence", 
                          "MC-TV");
    videobuf3 = new unsigned char[videosize];
    
    osd = new OSD(video_width, video_height, osdfilename);

    playing = true;
    killplayer = false;
  
    framesPlayed = 0;
    framesSkipped = 0;

    audiotimecode = 0;

    weseeked = 0;
    rewindtime = 0;
    fftime = 0;

    resetplaying = false;
    
    struct timeval startt, nowt;
    int framesdisplayed = 0;

    while (!eof && !killplayer)
    {
	if (resetplaying)
	{
            ClearAfterSeek();
            prebuffered = 0;
            actpre = 0;
	    framesPlayed = 0;
	    //OpenFile(true);
	    delete positionMap;
	    positionMap = new map<long long, long long>;

	    resetplaying = false;
	    actuallyreset = true;
        }
	    
        if (paused)
	{ 
            actuallypaused = true;
            if (livetv && ringBuffer->GetFreeSpace() < -1000)
            {
                paused = false;
		printf("forced unpause\n");
	    }
	    else
            {
                usleep(50);
                continue;
            }
	}
	
	if (rewindtime > 0)
	{
	    rewindtime *= video_frame_rate;
            if (DoRewind())
            {
                prebuffered = 0;
                actpre = 0;
            }
            rewindtime = 0;
	}
	if (fftime > 0)
	{
            CalcMaxFFTime();

	    fftime *= video_frame_rate;
            if (DoFastForward())
            {
                prebuffered = 0;
                actpre = 0;
            }
            fftime = 0;
	}

        videobuf = GetFrame(&timecode, audiofd <= 0, &audiodata, &audiodatalen);
        // GetFrame has used SNDCTL_DSP_GETODELAY to figure out the latency
        // through the sound output system, and it also considered whether we
        // are prebuffering.  All we do here is display the video frame first,
        // and then play the audio.  We play the audio second because it might
        // block.

	if (eof)
            continue;

        if (usepre != 0)
        {
            if (prebuffered < usepre)
            {
                prebuf[prebuffered] = new unsigned char[videosize];
                memcpy(prebuf[prebuffered], videobuf, videosize);
                prebuffered++;
                fnum++;
		framesPlayed++;
                continue;
            }
            else
            {
                memcpy(videobuf3, prebuf[actpre], videosize);
                memcpy(prebuf[actpre], videobuf, videosize);
                actpre++;
                if (actpre >= usepre)
                    actpre = 0;
            }
        }
        else
        {
            memcpy(videobuf3, videobuf, videosize);
        }

        if (deinterlace)
            linearBlendYUV420(videobuf3, video_width, video_height);
  
        memcpy(X11videobuf, videobuf3, videosize); 
        osd->Display(X11videobuf);

        XJ_show(video_width, video_height);
        framesPlayed++;

        // play audio
        if (audiodatalen != 0)
            WriteAudio(audiodata, audiodatalen);

        if (framesdisplayed == 60)
        {
            gettimeofday(&nowt, NULL);

            //double timediff = (nowt.tv_sec - startt.tv_sec) * 1000000 +
            //                 (nowt.tv_usec - startt.tv_usec);

            //printf("FPS played: %f\n", 60000000.0 / timediff);

            startt.tv_sec = nowt.tv_sec;
            startt.tv_usec = nowt.tv_usec;

            framesdisplayed = 0;
        }
        framesdisplayed++;
	
        tf++;
        fnum++;
    }

    playing = false;
    XJ_exit();
}

bool NuppelVideoPlayer::DoRewind(void)
{
    if (rewindtime < 5)
        return false;

    int number = (int)rewindtime;

    long long desiredFrame = framesPlayed - number;
    while (lastKey > desiredFrame)
    {
        lastKey -= 30;
    }
    if (lastKey < 1)
        lastKey = 1;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = (*positionMap)[lastKey / 30];
    long long curPosition = ringBuffer->GetReadPosition();
    long long diff = keyPos - curPosition;
  
    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += 30;
        keyPos = (*positionMap)[lastKey / 30];
        diff = keyPos - curPosition;
        normalframes = 0;
    }

    ringBuffer->Seek(diff, SEEK_CUR);
    framesPlayed = lastKey;

    int fileend = 0;
    
    while (normalframes > 0)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        if (fileend)
            continue;
        if (frameheader.frametype == 'R')
            continue;

        if (frameheader.frametype == 'S') 
        {
            if (frameheader.comptype == 'A')
            {
                if (frameheader.timecode > 0)
                {
                    effdsp = frameheader.timecode;
                }
            }
        }

        fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                                    frameheader.packetlength);

        if (fileend)
            continue;
        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;

            DecodeFrame(&frameheader, strm);
        }
    }
    ClearAfterSeek();

    return true;
}


void NuppelVideoPlayer::CalcMaxFFTime(void)
{
    if (livetv)
    {
        float behind = (float)(nvr->GetFramesWritten() - framesPlayed) / 
                       video_frame_rate;
	if (behind < 1.0)
	    fftime = 0.0;
	else if (behind - fftime <= 1.0)
	{
            fftime = behind - 1.0;
	}
    }
    else
    {
    }
}

bool NuppelVideoPlayer::DoFastForward(void)
{
    if (fftime < 5)
        return false;

    int number = (int)fftime;

    long long desiredFrame = framesPlayed + number;
    long long desiredKey = lastKey;

    while (desiredKey < desiredFrame)
    {
        desiredKey += 30;
    }
    desiredKey -= 30;

    int normalframes = desiredFrame - desiredKey;
    int fileend = 0;

    if (positionMap->find(desiredKey / 30) != positionMap->end())
    {
        lastKey = desiredKey;
        long long keyPos = (*positionMap)[lastKey / 30];
        long long diff = keyPos - ringBuffer->GetReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        framesPlayed = lastKey;
    }
    else  
    {
        while (lastKey < desiredKey && !fileend)
        {
            fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

            if (frameheader.frametype == 'S')
            {
                if (frameheader.comptype == 'V')
                {
                    (*positionMap)[framesPlayed / 30] = 
                                                 ringBuffer->GetReadPosition();
                    lastKey = framesPlayed;
                }
                if (frameheader.comptype == 'A')
                    if (frameheader.timecode > 0)
                    {
                        effdsp = frameheader.timecode;
                    }
            }
            else if (frameheader.frametype == 'V')
            {
                framesPlayed++;
            }

            if (frameheader.frametype != 'R' && frameheader.packetlength > 0)
            {
                fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                           frameheader.packetlength);
            }
        }
    } 

    while (normalframes > 0 && !fileend)
    {
        fileend = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        if (fileend)
            continue;
        if (frameheader.frametype == 'R')
            continue;
        else if (frameheader.frametype == 'S') 
        {
            if (frameheader.comptype == 'A')
            {
                if (frameheader.timecode > 0)
                {
                    effdsp = frameheader.timecode;
                }
            }
        }

        fileend = (ringBuffer->Read(strm, frameheader.packetlength) !=
                                    frameheader.packetlength);

        if (frameheader.frametype == 'V')
        {
            framesPlayed++;
            normalframes--;
            DecodeFrame(&frameheader, strm);
        }
    }

    ClearAfterSeek();

    return true;
}

void NuppelVideoPlayer::ClearAfterSeek(void)
{
    int i;
    for (i = 0; i < MAXVBUFFER; i++)
    {
        bufstat[i] = 0;
        timecodes[i] = 0;
    }

    wpos = 0;
    rpos = 0;
    audiolen = 0;
    weseeked = 1;
    fafterseek = 0;
    audiotimecode = 0;
}

void NuppelVideoPlayer::SetInfoText(const string &text, const string &subtitle,
                                    const string &desc, const string &category,
                                    const string &start, const string &end,
                                    int secs)
{
    osd->SetInfoText(text, subtitle, desc, category, start, end, 
                     (int)(secs * ceil(video_frame_rate)));
}

void NuppelVideoPlayer::SetChannelText(const string &text, int secs)
{
    osd->SetChannumText(text, (int)(secs * ceil(video_frame_rate)));
}

void NuppelVideoPlayer::ShowLastOSD(int secs)
{
    osd->ShowLast((int)(secs * ceil(video_frame_rate)));
}

