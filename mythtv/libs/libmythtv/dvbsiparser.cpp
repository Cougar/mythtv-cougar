/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4: 
 *
 * DVBSIParser.cpp: Digital Video Broadcast - Section/Table Parser
 *
 * Author(s):  Kenneth Aafloy (ke-aa@frisurf.no)
 *                - Wrote original code
 *             Taylor Jacob (rtjacob@earthlink.net)
 *                - Added NIT/EIT/SDT Scaning Code
 *                - Rewrote major portion of code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The project's page is at http://www.mythtv.org
 *
 */

#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <qvaluelist.h>
#include <qvaluestack.h>
using namespace std;

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "recorderbase.h"

#include "dvbdev.h"
#include "dvbsiparser.h"

#include "dvbchannel.h"
#include "dvbrecorder.h"

DVBSIParser::DVBSIParser(int cardNum)
    : cardnum(cardNum)
{

    pollArray = NULL;
    pollLength = 0;
    exitSectionThread = false;
    filterChange = false;

    pthread_mutex_init(&poll_lock, NULL);
    GENERAL("DVB SI Table Parser Started");
}

DVBSIParser::~DVBSIParser()
{
    pthread_mutex_destroy(&poll_lock);
}

void DVBSIParser::AddPid(uint16_t pid,uint8_t mask,uint8_t filter, bool CheckCRC)
{

    struct dmx_sct_filter_params params;
    int fd;

    /* Set flag so other processes can get past poll_lock */
    filterChange = true;
    pthread_mutex_lock(&poll_lock);

    filterChange = false;

    PIDFDMap::Iterator it;

    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       if (it.data().pid == pid)
       {
          pthread_mutex_unlock(&poll_lock);
          return;
       }
    }

    memset(&params, 0, sizeof(struct dmx_sct_filter_params));
    params.flags = DMX_IMMEDIATE_START;
    if (CheckCRC)
        params.flags |= DMX_CHECK_CRC;
    params.pid = pid;
    params.filter.filter[0] = filter;
    params.filter.mask[0] = mask;

    fd = open(dvbdevice(DVB_DEV_DEMUX, cardnum), O_RDWR | O_NONBLOCK);

    if (fd == -1)
    {
        ERRNO(QString("Failed to open section filter (pid %1)").arg(pid));
        pthread_mutex_unlock(&poll_lock);
        return;
    }

    if (ioctl(fd, DMX_SET_FILTER, &params) < 0)
    {
        ERRNO(QString("Failed to set section filter (pid %1)").arg(pid));
        pthread_mutex_unlock(&poll_lock);
        return;
    }

    /* Add filter to the Manager Object */
    PIDfilterManager[fd].pid = pid;

    /* Re-allocate memory and add to the pollarray */
    pollArray = (pollfd*)realloc((void*)pollArray, sizeof(pollfd) * (pollLength+1));
    pollArray[pollLength].fd = fd;
    pollArray[pollLength].events = POLLIN | POLLPRI;
    pollArray[pollLength].revents = 0;
    pollLength++;

    pthread_mutex_unlock(&poll_lock);

}

void DVBSIParser::DelPid(int pid)
{
    PIDFDMap::Iterator it;
    int x = 0;

    filterChange = true;

    pthread_mutex_lock(&poll_lock);

    filterChange = false;

    QValueStack<PIDFDMap::Iterator> stack;
    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       if (it.data().pid == pid)
       {
          stack.push(it);
          pollLength--;
       }
    }

    if (stack.isEmpty())
       return;

    while (!stack.isEmpty())
    {
         it = stack.pop();
         close(it.key());
         PIDfilterManager.remove(it);
    }
    pollArray = (pollfd*)realloc((void*)pollArray, sizeof(pollfd) * (pollLength));
    /* Re-construct the pollarray */
    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       pollArray[x].fd = it.key();
       pollArray[x].events = POLLIN | POLLPRI;
       pollArray[x].revents = 0;
       x++;
    }

    pthread_mutex_unlock(&poll_lock);
}

void DVBSIParser::DelAllPids()
{
    PIDFDMap::Iterator it;

    filterChange = true;
    pthread_mutex_lock(&poll_lock);
    filterChange = false;

    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
        close(it.key());

    PIDfilterManager.clear();
    free(pollArray);
    pollLength = 0;
    pollArray = NULL;

    pthread_mutex_unlock(&poll_lock);

}

void DVBSIParser::StopSectionReader()
{

   SIPARSER("Stopping DVB Section Reader");
   exitSectionThread = true;
   DelAllPids();
   filterChange = true;
   pthread_mutex_lock(&poll_lock);
   filterChange = false;
   free(pollArray);
   pthread_mutex_unlock(&poll_lock);

}

void DVBSIParser::StartSectionReader()
{
    uint8_t    buffer[MAX_SECTION_SIZE];
    bool       processed = false;

    SIPARSER("Starting DVB Section Reader thread");

    sectionThreadRunning = true;

    while (!exitSectionThread)
    {

        CheckTrackers();

        if (!processed || filterChange)
           usleep(250);
        processed = false;

        pthread_mutex_lock(&poll_lock);

        int ret = poll(pollArray, pollLength, 1000);

        if (ret < 0)
        {
            if ((errno != EAGAIN) && (errno != EINTR))
                ERRNO("Poll failed while waiting for Section");
        }
        else if (ret > 0)
        {
            for (int i=0; i<pollLength; i++)
            {
                // FIXME: Handle POLLERR
                if (! (pollArray[i].revents & POLLIN || pollArray[i].revents & POLLPRI) )
                    continue;

                int rsz = read(pollArray[i].fd, &buffer, MAX_SECTION_SIZE);

                if (rsz > 0)
                {
                    ParseTable(buffer,rsz,PIDfilterManager[pollArray[i].fd].pid);
                    processed = true;
                    continue;
                }

                if (rsz == -1 && errno == EAGAIN)
                {
                    i--;
                    continue;
                }

                if (rsz < 0)
                {
                    ERRNO("Reading Section.");
                }

                pollArray[i].events = POLLIN | POLLPRI;
                pollArray[i].revents = 0;
            }
        }

        pthread_mutex_unlock(&poll_lock);
    }

    sectionThreadRunning = false;
    SIPARSER("DVB Section Reader thread stopped");

}

