#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/videodev.h>
#include "channel.h"
#include "frequencies.h"
#include "tv.h"

#include <iostream>
using namespace std;

Channel::Channel(TV *parent, const QString &videodevice)
{
    device = videodevice;
    isopen = false;
    videofd = -1;
    curchannelname = "";
    currentcapchannel = 0;
    
    pParent = parent;
    channelorder = "channum + 0";
}

Channel::~Channel(void)
{
    if (isopen)
        close(videofd);
}

bool Channel::Open(void)
{
    videofd = open(device.ascii(), O_RDWR);
    if (videofd > 0)
        isopen = true;
    return isopen;
}

void Channel::Close(void)
{
    if (isopen)
        close(videofd);
    videofd = -1;
}

void Channel::SetFormat(const QString &format)
{
    if (!isopen)
        Open();

    if (!isopen)
        return;
    
    int mode = VIDEO_MODE_AUTO;
    struct video_tuner tuner;

    memset(&tuner, 0, sizeof(tuner));

    ioctl(videofd, VIDIOCGTUNER, &tuner);
    
    if (format == "NTSC")
        mode = VIDEO_MODE_NTSC;
    else if (format == "PAL")
        mode = VIDEO_MODE_PAL;
    else if (format == "SECAM")
        mode = VIDEO_MODE_SECAM;
    else if (format == "PAL-NC")
        mode = 3;
    else if (format == "PAL-M")
        mode = 4;
    else if (format == "PAL-N")
        mode = 5;
    else if (format == "NTSC-JP")
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

        cout << "Probed: " << device << " - " << test.name << endl;
        channelnames[i] = test.name;
    }

    struct video_channel vc;
    memset(&vc, 0, sizeof(vc));
    ioctl(videofd, VIDIOCGCHAN, &vc);
    vc.norm = mode;
    ioctl(videofd, VIDIOCSCHAN, &vc);

    videomode = mode;
}

void Channel::SetFreqTable(const QString &name)
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;

    curList = NULL;
    while (listname != NULL)
    {
        if (name == listname)
        {
            curList = chanlists[i].list;
	    totalChannels = chanlists[i].count;
            break;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }
    if (!curList)
    {
        curList = chanlists[0].list;
	totalChannels = chanlists[0].count;
    }
}
  
bool Channel::SetChannelByString(const QString &chan)
{
    if (!isopen)
        Open();
    if (!isopen)
        return false;

    int finetune = 0;

    if (pParent->CheckChannel(this, chan, finetune))
    {
        if (GetCurrentInput() == "Television")
        {
            int i = GetCurrentChannelNum(chan);
            if (i == -1)
                return false;

            int frequency = curList[i].freq * 16 / 1000 + finetune;
            if (ioctl(videofd, VIDIOCSFREQ, &frequency) == -1)
                perror("channel set:");

            curchannelname = chan;

	    return true;
        }
        else
        {
            if (pParent->ChangeExternalChannel(chan))
            {
                curchannelname = chan;
                return true;
            }
        }
    }

    return false;
}

int Channel::GetCurrentChannelNum(const QString &channame)
{
    bool foundit = false;
    int i;
    for (i = 0; i < totalChannels; i++)
    {
        if (channame == curList[i].name)
        {
            foundit = true;
            break;
        }
    }

    if (foundit)
        return i;
    
    return -1;
}

bool Channel::ChannelUp(void)
{
    QString nextchan = pParent->GetNextChannel(this, true);
    if (SetChannelByString(nextchan))
        return true;

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
            cerr << "Error, couldn't find any available channels.\n";
            cerr << "Your database is most likely setup incorrectly.\n";
            break;
        }
    }

    return finished;
}

bool Channel::ChannelDown(void)
{
    QString nextchan = pParent->GetNextChannel(this, false);
    if (SetChannelByString(nextchan))
        return true;

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
            cerr << "Error, couldn't find any available channels.\n";
            cerr << "Your database is most likely setup incorrectly.\n";
            break;
        }
    }

    return false;
}

QString Channel::GetCurrentName(void)
{
    return curchannelname;
}

QString Channel::GetCurrentInput(void)
{
    return channelnames[currentcapchannel];
}

void Channel::ToggleInputs(void)
{
    currentcapchannel++;
    if (currentcapchannel >= capchannels)
        currentcapchannel = 0;

    struct video_channel set;
    memset(&set, 0, sizeof(set));
    ioctl(videofd, VIDIOCGCHAN, &set);
    set.channel = currentcapchannel;
    set.norm = videomode;
    ioctl(videofd, VIDIOCSCHAN, &set);
}

void Channel::SwitchToInput(const QString &input)
{
    int inputnum = 0;
    for (int i = 0; i < capchannels; i++)
    {
        if (channelnames[i] == input)
            inputnum = i;
    }

    if (inputnum == currentcapchannel)
        return;

    currentcapchannel = inputnum;

    struct video_channel set;
    memset(&set, 0, sizeof(set));
    ioctl(videofd, VIDIOCGCHAN, &set);
    set.channel = currentcapchannel;
    set.norm = videomode;
    ioctl(videofd, VIDIOCSCHAN, &set);
}
