#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <qsqldatabase.h>

#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"
#include "mythcontext.h"

#include <iostream>
using namespace std;

Channel::Channel(TVRec *parent, const QString &videodevice, bool strength)
       : ChannelBase(parent)
{
    device = videodevice;
    isopen = false;
    videofd = -1;
    curList = 0;
    defaultFreqTable = 1;
    totalChannels = 0;
    usingv4l2 = false;
    videomode = VIDEO_MODE_NTSC;
    currentFormat = "";
    usingATSCstrength = strength;
}

Channel::~Channel(void)
{
    Close();
}

bool Channel::Open(void)
{
    if (isopen)
        return true;

    videofd = open(device.ascii(), O_RDWR);
    if (videofd > 0)
        isopen = true;
    else
    {
         VERBOSE(VB_IMPORTANT, QString("Channel::Open(): Can't open: %1")
			              .arg(device));
         perror(device.ascii());
         return false;
    }

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
        usingv4l2 = false;
    else
    {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }

    return isopen;
}

void Channel::Close(void)
{
    if (isopen)
        close(videofd);
    isopen = false;
    videofd = -1;
}

void Channel::SetFd(int fd)
{
    if (fd > 0)
    {
        videofd = fd;
        isopen = true;
    }
    else
    {
        videofd = -1;
        isopen = false;
    }
}

void Channel::SetFormat(const QString &format)
{
    if (!Open())
        return;

    if (currentFormat == format)
        return;

    currentFormat = format;
  
    QString fmt;
    if (format == "Default")
        fmt = gContext->GetSetting("TVFormat");
    else
        fmt = format;
 
    if (usingv4l2)
    {
        struct v4l2_input vin;
        memset(&vin, 0, sizeof(vin));
        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0)
        {
            QString msg = QString("Probed: %1 - %2").arg(device)
                            .arg((char *)vin.name);
            VERBOSE(VB_CHANNEL, msg);
            channelnames[vin.index] = (char *)vin.name;
            inputChannel[vin.index] = "";
            inputTuneTo[vin.index] = "";
            externalChanger[vin.index] = "";
            sourceid[vin.index] = "";
            vin.index++;

            capchannels = vin.index;
        }

        if (fmt == "NTSC")
            videomode = V4L2_STD_NTSC;
        else if (fmt == "ATSC")
            videomode = V4L2_STD_ATSC_8_VSB;
        else if (fmt == "PAL")
            videomode = V4L2_STD_PAL;
        else if (fmt == "SECAM")
            videomode = V4L2_STD_SECAM;
        else if (fmt == "PAL-NC")
            videomode = V4L2_STD_PAL_Nc;
        else if (fmt == "PAL-M")
            videomode = V4L2_STD_PAL_M;
        else if (fmt == "PAL-N")
            videomode = V4L2_STD_PAL_N;
        else if (fmt == "NTSC-JP")
            videomode = V4L2_STD_NTSC_M_JP;

        pParent->RetrieveInputChannels(inputChannel, inputTuneTo, 
                                       externalChanger, sourceid);
        return;
    }
 
    int mode = VIDEO_MODE_AUTO;
    struct video_tuner tuner;

    memset(&tuner, 0, sizeof(tuner));

    ioctl(videofd, VIDIOCGTUNER, &tuner);
    
    if (fmt == "NTSC")
        mode = VIDEO_MODE_NTSC;
    else if (fmt == "ATSC")
        mode = VIDEO_MODE_ATSC;
    else if (fmt == "PAL")
        mode = VIDEO_MODE_PAL;
    else if (fmt == "SECAM")
        mode = VIDEO_MODE_SECAM;
    else if (fmt == "PAL-NC")
        mode = 3;
    else if (fmt == "PAL-M")
        mode = 4;
    else if (fmt == "PAL-N")
        mode = 5;
    else if (fmt == "NTSC-JP")
        mode = 6;

    tuner.mode = mode;
    ioctl(videofd, VIDIOCSTUNER, &tuner);

    struct video_capability vidcap;
    memset(&vidcap, 0, sizeof(vidcap));
    ioctl(videofd, VIDIOCGCAP, &vidcap);

    capchannels = vidcap.channels;
    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        memset(&test, 0, sizeof(test));
        test.channel = i;
        ioctl(videofd, VIDIOCGCHAN, &test);

        VERBOSE(VB_CHANNEL, QString("Probed: %1 - %2").arg(device)
                            .arg(test.name));
        channelnames[i] = test.name;
        inputChannel[i] = "";
        inputTuneTo[i] = "";
        externalChanger[i] = "";
        sourceid[i] = "";
    }

    struct video_channel vc;
    memset(&vc, 0, sizeof(vc));
    ioctl(videofd, VIDIOCGCHAN, &vc);
    vc.norm = mode;
    ioctl(videofd, VIDIOCSCHAN, &vc);

    videomode = mode;

    pParent->RetrieveInputChannels(inputChannel, inputTuneTo, externalChanger,
                                   sourceid);
}

int Channel::SetDefaultFreqTable(const QString &name)
{
    defaultFreqTable = SetFreqTable(name);
    return defaultFreqTable;
}

void Channel::SetFreqTable(const int index)
{
    curList = chanlists[index].list;
    totalChannels = chanlists[index].count;
}

int Channel::SetFreqTable(const QString &name)
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;

    curList = NULL;
    while (listname != NULL)
    {
        if (name == listname)
        {
            SetFreqTable(i);
            return i;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }

    VERBOSE(VB_CHANNEL, QString("Invalid frequency table name %1, using %2.")
                        .arg(name).arg((char *)chanlists[1].name));
    SetFreqTable(1);
    return 1;
}

int Channel::GetCurrentChannelNum(const QString &channame)
{
    // remove part after '-' for (HDTV subchannels)
    QString real_channame = channame;
    int pos = channame.find('-');
    if (pos != -1)
        real_channame.truncate(pos);

    for (int i = 0; i < totalChannels; i++)
    {
        if (real_channame == curList[i].name)
            return i;
    }

    return -1;
}

bool Channel::ChannelUp(void)
{
    if (ChannelBase::ChannelUp())
        return true;

    QString nextchan;
    bool finished = false;
    int chancount = 0;
    int curchannel = GetCurrentChannelNum(curchannelname);

    while (!finished)
    {
        curchannel++;
        chancount++;

        if (curchannel == totalChannels)
            curchannel = 0;

        nextchan = curList[curchannel].name;

        finished = SetChannelByString(nextchan);

        if (chancount > totalChannels)
        {
            VERBOSE(VB_IMPORTANT, "Error, couldn't find any available "
                                  "channels.");
            VERBOSE(VB_IMPORTANT, "Your database is most likely setup "
                                  "incorrectly.");
            break;
        }
    }

    return finished;
}

bool Channel::ChannelDown(void)
{
    if (ChannelBase::ChannelDown())
        return true;

    QString nextchan;
    bool finished = false;
    int chancount = 0;
    int curchannel = GetCurrentChannelNum(curchannelname);

    while (!finished)
    {
        curchannel--;
        chancount++;

        if (curchannel < 0)
            curchannel = totalChannels - 1;

        nextchan = curList[curchannel].name;

        finished = SetChannelByString(nextchan);

        if (chancount > totalChannels)
        {
            VERBOSE(VB_IMPORTANT, "Error, couldn't find any available "
                                  "channels.");
            VERBOSE(VB_IMPORTANT, "Your database is most likely setup "
                                  "incorrectly.");
            break;
        }
    }

    return finished;
}

bool Channel::SetChannelByString(const QString &chan)
{
    QString inputName;
    
    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, "Channel object wasn't open, can't change "
                              "channels");
        return false;
    }

    QSqlDatabase *db_conn;
    pthread_mutex_t *db_lock;

    if (!pParent->CheckChannel(this, chan, db_conn, db_lock, inputName))
    {
        VERBOSE(VB_IMPORTANT, QString("CheckChannel failed. Please verify "
                "channel \"%1\" in the \"setup\" Channel Editor.").arg(chan));
        return false;
    }

    // If CheckChannel filled in the inputName then we need to change inputs
    // and return, since the act of changing inputs will change the channel as well.
    if (!inputName.isEmpty())
    {
cout << "switch to input\n";
        ChannelBase::SwitchToInput(inputName, chan);
        return true;
    }

    pthread_mutex_lock(db_lock);
    QString thequery = QString("SELECT finetune, freqid, tvformat, "
                                   "freqtable, commfree "
                               "FROM channel "
                               "LEFT JOIN videosource USING (sourceid) "
                               "WHERE channum = \"%1\" AND "
                               "channel.sourceid = \"%2\";")
                               .arg(chan)
                               .arg(sourceid[currentcapchannel]);

    QSqlQuery query = db_conn->exec(thequery);
    if (!query.isActive())
        MythContext::DBError("fetchtuningparams", query);
    if (query.numRowsAffected() <= 0)
    {
        pthread_mutex_unlock(db_lock);
        return false;
    }
    query.next();

    int finetune = query.value(0).toInt();
    QString freqid = query.value(1).toString();
    QString tvformat = query.value(2).toString();
    QString freqtable = query.value(3).toString();
    commfree = query.value(4).toBool();

    if (freqtable == "default" || freqtable.isNull() || freqtable.isEmpty())
        SetFreqTable(defaultFreqTable);
    else
        SetFreqTable(freqtable);

    if (tvformat.isNull() || tvformat.isEmpty())
        tvformat = "Default";

    pthread_mutex_unlock(db_lock);

    // Determine if freqid is id or frequency
    //  freqid is a frequency if and only if it is a valid integer and 
    //  larger than 50 MHz
    bool ok;
    int frequency = freqid.toInt(&ok);
    bool isFrequency = ok && frequency > 50000;

    SetFormat(tvformat);

    curchannelname = chan;

    pParent->SetVideoFiltersForChannel(this, chan);
cout << "setting contrast\n";
    SetContrast();
    SetColour();
    SetBrightness();
    SetHue();

    inputChannel[currentcapchannel] = curchannelname;

    // Tune
    if (externalChanger[currentcapchannel].isEmpty())
    {
        if (isFrequency)
        {
            frequency = frequency * 16 / 1000 + finetune;
            if (!TuneToFrequency(frequency))
                return false;
        }
        else
        {
            if (!TuneTo(freqid, finetune))
                return false;
        }
    }
    else if (!ChangeExternalChannel(freqid))
        return false;

    return true;
}

template<typename V>
V clamp(V val, V minv, V maxv) { return std::min(maxv, std::max(minv, val)); }

// Returns ATSC signal strength 0-100. >75 is good.
int signalStrengthATSC(int device, int input) 
{
    struct video_signal vsig;

    int ioctlval = ioctl(device,VIDIOCGSIGNAL,&vsig);
    if (ioctlval == -1) 
    {
        perror("VIDIOCGSIGNAL problem in channel.h's signalStrengthATSC()");
        return 0;
    }

    int signal = (input == 0) ? vsig.strength : vsig.aux;

    if ((signal & 0xff) != 0x43) 
        return 0;
    else 
        return clamp(101 - (signal >> 9), 0, 100);
}

// Returns ATSC signal strength 0-100. >75 is good. Using v4l2.
int signalStrengthATSC_v4l2(int device, int input)
{
    struct v4l2_tuner vsig;
    vsig.index = input;

    int ioctlval = ioctl(device,VIDIOC_G_TUNER, &vsig);
    if (ioctlval == -1)
    {
	perror("VIDIOC_G_TUNER");
        return 0;
    }
    VERBOSE(VB_CHANNEL, QString("signal strength: %1").arg(vsig.signal));

    return clamp(vsig.signal, 0, 100);
}

bool Channel::CheckSignal(int msecTotal, int reqSignal, int input) 
{
    int msecSleep = 500, maxSignal = 0, i = 0;

    msecTotal = max(msecSleep, msecTotal);

    if (usingATSCstrength) 
    {
        VERBOSE(VB_CHANNEL, QString("CheckSignal(%1, %2, %3) usingv4l2(%4)")
                .arg(msecTotal).arg(reqSignal).arg(input).arg(usingv4l2));

        for (i = 0; i < (msecTotal / msecSleep) + 1; i++) 
        {
            if (i != 0) 
                usleep(msecSleep);
            if (usingv4l2)
                maxSignal = max(maxSignal, signalStrengthATSC_v4l2(videofd, 
                                input));
            else
                maxSignal = max(maxSignal, signalStrengthATSC(videofd, input));


            if (maxSignal >= reqSignal) 
                break;
        }
        VERBOSE(VB_IMPORTANT, QString("Maximum signal strength detected: %1\% "
                                      "after %2 msec wait")
                                      .arg(maxSignal).arg(i * msecSleep));

        if (maxSignal < reqSignal) 
            return false;
    }

    return true;
}

bool Channel::TuneTo(const QString &channum, int finetune)
{
    int i = GetCurrentChannelNum(channum);
    if (i == -1)
        return false;

    int frequency = curList[i].freq * 16 / 1000 + finetune;

    VERBOSE(VB_CHANNEL, QString("TuneTo(%1) curList[i].freq(%2)")
                                .arg(channum).arg(curList[i].freq));

    return TuneToFrequency(frequency);
}

bool Channel::CheckSignalFull(void)
{
    int signalThresholdWait = 5000;
    int signalThreshold = 65;
    if (usingATSCstrength)
    {
        signalThresholdWait = gContext->GetNumSetting("ATSCCheckSignalWait",
                                                      5000);
        signalThreshold = gContext->GetNumSetting("ATSCCheckSignalThreshold",
                                                  65);
    }

    return CheckSignal(signalThresholdWait, signalThreshold, currentcapchannel);
}

bool Channel::TuneToFrequency(int frequency)
{
    VERBOSE(VB_CHANNEL, QString("TuneToFrequency(%1)").arg(frequency));

    int signalThresholdWait = 5000;
    int signalThreshold = 65;
    if (usingATSCstrength) 
    {
        signalThresholdWait = gContext->GetNumSetting("ATSCCheckSignalWait", 
                                                      5000);
        signalThreshold = gContext->GetNumSetting("ATSCCheckSignalThreshold", 
                                                  65);
    }

    if (usingv4l2)
    {
        struct v4l2_frequency vf;
        memset(&vf, 0, sizeof(vf));
        vf.frequency = frequency;
        vf.type = V4L2_TUNER_ANALOG_TV;

        if (ioctl(videofd, VIDIOC_S_FREQUENCY, &vf) < 0)
        {
            perror("VIDIOC_S_FREQUENCY");
            return false;
        }
        

        return CheckSignal(signalThresholdWait, signalThreshold,
                           currentcapchannel);
    }

    if (ioctl(videofd, VIDIOCSFREQ, &frequency) == -1)
    {
        perror("VIDIOCSFREQ");
        return false;
    }

    return CheckSignal(signalThresholdWait, signalThreshold, currentcapchannel);
}

void Channel::SwitchToInput(int newcapchannel, bool setstarting)
{
cout << "switching to input: " << newcapchannel << endl;
    if (usingv4l2)
    {
        if (ioctl(videofd, VIDIOC_S_INPUT, &newcapchannel) < 0)
            perror("VIDIOC_S_INPUT");
   
        if (ioctl(videofd, VIDIOC_S_STD, &videomode) < 0)
            perror("VIDIOC_S_STD");
    }
    else
    {
        struct video_channel set;
        memset(&set, 0, sizeof(set));
        ioctl(videofd, VIDIOCGCHAN, &set);
        set.channel = newcapchannel;
        set.norm = videomode;
        if (ioctl(videofd, VIDIOCSCHAN, &set) < 0)
           perror("VIDIOCSCHAN");
    }

    currentcapchannel = newcapchannel;
    curchannelname = "";

    if (inputTuneTo[currentcapchannel] != "Undefined")
        TuneTo(inputTuneTo[currentcapchannel], 0);

    if (setstarting && !inputChannel[currentcapchannel].isEmpty())
        SetChannelByString(inputChannel[currentcapchannel]);
}

unsigned short *Channel::GetV4L1Field(int attrib, struct video_picture &vid_pic)
{
    switch (attrib)
    {
        case V4L2_CID_CONTRAST:
            return &vid_pic.contrast;
        case V4L2_CID_BRIGHTNESS:
            return &vid_pic.brightness;
        case V4L2_CID_SATURATION:
            return &vid_pic.colour;
        case V4L2_CID_HUE:
            return &vid_pic.hue;
        default:
            fprintf(stderr, "Channel::SetColourAttribute(): invalid attribute argument: %d\n", attrib);
    }
    return NULL;
}

void Channel::SetColourAttribute(int attrib, const char *name)
{
    QString field_name = name;
    int field = pParent->GetChannelValue(field_name, this, curchannelname);

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        if (field != -1)
        {
            ctrl.id = qctrl.id = attrib;
            if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
            {
                return;
            }
            ctrl.value = (int)((qctrl.maximum - qctrl.minimum) 
                               / 65535.0 * field + qctrl.minimum);
            ctrl.value = ctrl.value > qctrl.maximum
                              ? qctrl.maximum
                                  : ctrl.value < qctrl.minimum
                                       ? qctrl.minimum
                                            : ctrl.value;
            if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
            {
                return;
            }
        }
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            return;
        }
        setfield = GetV4L1Field(attrib, vid_pic);
        if (field != -1 && setfield)
        {
            *setfield = field;
            if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
            {
                return;
            }
        }
    }
    return;
}

void Channel::SetContrast(void)
{
    SetColourAttribute(V4L2_CID_CONTRAST, "contrast");
    return;
}

void Channel::SetBrightness()
{
    SetColourAttribute(V4L2_CID_BRIGHTNESS, "brightness");
    return;
}

void Channel::SetColour()
{
    SetColourAttribute(V4L2_CID_SATURATION, "colour");
    return;
}

void Channel::SetHue(void)
{
    SetColourAttribute(V4L2_CID_HUE, "hue");
    return;
}

int Channel::ChangeColourAttribute(int attrib, const char *name, bool up)
{
    int newvalue;    // The int should have ample space to avoid overflow
                     // in the case that we're just over or under 65535

    QString channel_field = name;
    int current_value = pParent->GetChannelValue(channel_field, this, 
                                                 curchannelname);

    int card_value;

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        ctrl.id = qctrl.id = attrib;
        if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
        {
            perror("VIDIOC_QUERYCTRL");
            return -1;
        }

        if (ioctl(videofd, VIDIOC_G_CTRL, &ctrl) < 0)
        {
            perror("VIDIOC_G_CTRL");
            return -1;
        }
        card_value = (int)(65535.0 / (qctrl.maximum - qctrl.minimum) * 
                           ctrl.value);
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            perror("VIDIOCGPICT");
            return -1;
        }

        setfield = GetV4L1Field(attrib, vid_pic);
        if (!setfield)
        {
            return -1;
        }

        card_value = *setfield;
    }

    if (current_value < 0) // Couldn't get from database
    {
        if (up)
        {
            newvalue = card_value + 655;
            newvalue = (newvalue > 65535) ? (65535) : (newvalue);
        }
        else
        {
            newvalue = card_value - 655;
            newvalue = (newvalue < 0) ? (0) : (newvalue);
        }
    }
    else
    {
        if (up)
        {
            newvalue = current_value + 655;
            newvalue = (newvalue > 65535) ? (65535) : (newvalue);
        }
        else
        {
            newvalue = current_value - 655;
            newvalue = (newvalue < 0) ? (0) : (newvalue);
        }

        pParent->SetChannelValue(channel_field, newvalue, this, curchannelname);
    }

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        ctrl.id = qctrl.id = attrib;
        if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
        {
            perror("VIDIOC_QUERYCTRL");
            return -1;
        }
        ctrl.value = (int)((qctrl.maximum - qctrl.minimum) / 65535.0 * 
                           newvalue + qctrl.minimum);
        ctrl.value = ctrl.value > qctrl.maximum
                        ? qctrl.maximum
                            : ctrl.value < qctrl.minimum
                                ? qctrl.minimum
                                    : ctrl.value;
        if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
        {
            perror("VIDIOC_S_CTRL");
            return -1;
        }
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            perror("VIDIOCGPICT");
            return -1;
        }
        setfield = GetV4L1Field(attrib, vid_pic);
        if (newvalue != -1 && setfield)
        {
            *setfield = newvalue;
            if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
            {
                perror("VIDIOCSPICT");
                return -1;
            }
        }
        else
        {
            // ???
            return -1;
        }
    }

    return newvalue / 655;
}

int Channel::ChangeContrast(bool up)
{
    return ChangeColourAttribute(V4L2_CID_CONTRAST, "contrast", up);
}

int Channel::ChangeBrightness(bool up)
{
    return ChangeColourAttribute(V4L2_CID_BRIGHTNESS, "brightness", up);
}

int Channel::ChangeColour(bool up)
{
    return ChangeColourAttribute(V4L2_CID_SATURATION, "colour", up);
}

int Channel::ChangeHue(bool up)
{
    return ChangeColourAttribute(V4L2_CID_HUE, "hue", up);
}

