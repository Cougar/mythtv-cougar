#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"
#include "mythcontext.h"

#include <iostream>
using namespace std;

ChannelBase::ChannelBase(TVRec *parent)
{
    curchannelname = "";
    pParent = parent;
    capchannels = 0;
    currentcapchannel = -1;
    channelorder = "channum + 0";

    //XXX from setformat pParent->RetrieveInputChannels(inputChannel,inputTuneTo,externalChanger);
}

ChannelBase::~ChannelBase(void)
{
}

bool ChannelBase::ChannelUp(void)
{
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_UP);
    if (SetChannelByString(nextchan))
        return true;
    else
        return false;
}

bool ChannelBase::ChannelDown(void)
{
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_DOWN);
    if (SetChannelByString(nextchan))
        return true;
    else
        return false;
}

bool ChannelBase::NextFavorite(void)
{
    QString nextchan = pParent->GetNextChannel(this, 
                                               CHANNEL_DIRECTION_FAVORITE);
    return SetChannelByString(nextchan);
}

QString ChannelBase::GetCurrentName(void)
{
    return curchannelname;
}

QString ChannelBase::GetCurrentInput(void)
{
    return channelnames[currentcapchannel];
}

int ChannelBase::GetCurrentInputNum(void)
{
    return currentcapchannel;
}

void ChannelBase::ToggleInputs(void)
{
    int newcapchannel = currentcapchannel;

    if (capchannels > 0)
    {
        do 
        {
            newcapchannel = (newcapchannel + 1) % capchannels;
        } while (inputTuneTo[newcapchannel].isEmpty());

        SwitchToInput(newcapchannel, true);
    }
}

QString ChannelBase::GetInputByNum(int capchannel)
{
    if (capchannel > capchannels)
        return "";
    return channelnames[capchannel];
}

int ChannelBase::GetInputByName(const QString &input)
{
    for (int i = capchannels-1; i >= 0; i--)
        if (channelnames[i] == input)
            return i;
    return -1;
}

void ChannelBase::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        SwitchToInput(input, true);
    else
        VERBOSE(VB_IMPORTANT, QString("ChannelBase: Could not find input: "
                                      "%1 on card\n").arg(inputname));
}

void ChannelBase::SwitchToInput(const QString &inputname, const QString &chan)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
    {
        SwitchToInput(input, false);
        SetChannelByString(chan);
    }
    else
        VERBOSE(VB_IMPORTANT,
                QString("ChannelBase: Could not find input: %1 on card when "
                        "setting channel %2\n").arg(inputname).arg(chan));
}

bool ChannelBase::ChangeExternalChannel(const QString &channum)
{
    if (externalChanger[currentcapchannel].isEmpty())
        return false;

    QString command = QString("%1 %2 &").arg(externalChanger[currentcapchannel])
                                        .arg(channum);

    VERBOSE(VB_CHANNEL, QString("External channel change: %1").arg(command));
    pid_t child = fork();
    if (child < 0)
    {   // error encountered in creating fork
        QString msg("ChannelBase: fork error -- ");
        msg.append(strerror(errno));
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    else if (child == 0)
    {   // we are the new fork
        for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
            close(i);
        int ret = execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        QString msg("ChannelBase: ");
        if (EACCES == ret) {
            msg.append(QString("Access denied to /bin/sh"
                               " when executing %1\n").arg(command.ascii()));
        }
        msg.append(strerror(errno));
        VERBOSE(VB_IMPORTANT, msg);
        _exit(1); // this exit is ok, we are just exiting from the channel changing fork with an error.
    }
    else
    {   // child contains the pid of the new process
        int status = 0, pid = 0;
        VERBOSE(VB_CHANNEL, "Waiting for External Tuning program to exit");

        bool timed_out = false;
        uint timeout = 30; // how long to wait in seconds
        time_t start_time = time(0);
        while (-1 != pid && !timed_out)
        {
            sleep(1);
            pid = waitpid(child, &status, WUNTRACED|WNOHANG);
            VERBOSE(VB_IMPORTANT, QString("ret_pid(%1) child(%2) status(0x%3)")
                    .arg(pid).arg(child).arg(status,0,16));
            if (pid==child)
                break;
            else if (time(0) > (time_t)(start_time + timeout))
                timed_out = true;
        }
        if (timed_out)
        {
            VERBOSE(VB_IMPORTANT, "External Tuning program timed out, killing");
            kill(child, SIGTERM);
            usleep(500);
            kill(child, SIGKILL);
            return false;
        }

        VERBOSE(VB_CHANNEL, "External Tuning program no longer running");
        if (WIFEXITED(status))
        {   // program exited normally
            int ret = WEXITSTATUS(status);
            if (ret)
            {   // external tuning program returned error value
                VERBOSE(VB_IMPORTANT,
                        QString("ChannelBase: external tuning program "
                                "exited with error %1").arg(ret));
                return false;
            }
            VERBOSE(VB_IMPORTANT, "External Tuning program exited with no error");
        }
        else
        {   // program exited abnormally
            QString msg = QString("ChannelBase: external tuning program "
                                  "encountered error %1 -- ").arg(errno);
            msg.append(strerror(errno));
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }
    }

    return true;
}

void ChannelBase::StoreInputChannels(void)
{
    pParent->StoreInputChannels(inputChannel);
}
