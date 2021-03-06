/*
 * Class DVBCam
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      Jesper Sorensen
 *          - Changed to work with Taylor Jacob's DVB rewrite
 *      Kenneth Aafloy
 *          - General Implementation
 *
 * Description:
 *      This Class has been developed from bits n' pieces of other
 *      projects.
 *
 *
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
 */

#include <qdatetime.h>
#include <qvariant.h>

#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
using namespace std;

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/dvb/ca.h>

#include <dvbci.h>

#include "recorderbase.h"

#include "cardutil.h"

#include "dvbcam.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

#define LOC_ERR QString("DVB#%1 CA Error: ").arg(device)
#define LOC QString("DVB#%1 CA: ").arg(device)

DVBCam::DVBCam(const QString &aDevice)
    : device(aDevice),        numslots(0),
      ciHandler(NULL),
      exitCiThread(false),    ciThreadRunning(false),
      ciHandlerThread(pthread_t()),
      have_pmt(false),        pmt_sent(false),
      pmt_updated(false),     pmt_added(false)
{
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_CA, device);
    QByteArray dev = dvbdev.toAscii();
    int cafd = open(dev.constData(), O_RDWR);
    if (cafd >= 0)
    {
        ca_caps_t caps;
        ioctl(cafd, CA_GET_CAP, &caps);
        numslots = caps.slot_num;
        close(cafd);
    }
}

DVBCam::~DVBCam()
{
    Stop();
}

bool DVBCam::Start()
{
    if (numslots == 0)
        return false;

    exitCiThread = false;
    have_pmt     = false;
    pmt_sent     = false;
    pmt_updated  = false;
    pmt_added    = false;

    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_CA, device);
    QByteArray dev = dvbdev.toAscii();
    ciHandler = cCiHandler::CreateCiHandler(dev.constData());
    if (!ciHandler)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to initialize CI handler");
        return false;
    }

    if (pthread_create(&ciHandlerThread, NULL, CiHandlerThreadHelper, this))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create CI handler thread");
        return false;
    }

    ciThreadRunning = true;

    VERBOSE(VB_DVBCAM, LOC + "CI handler successfully initialized!");

    return true;
}

bool DVBCam::Stop()
{    
    if (ciThreadRunning)
    {
        exitCiThread = true;
        pthread_join(ciHandlerThread, NULL);
    }

    if (ciHandler)
    {
        delete ciHandler;
        ciHandler = NULL;
    }

    QMutexLocker locker(&pmt_lock);
    pmt_list_t::iterator it;

    for (it = PMTList.begin(); it != PMTList.end(); ++it)
        delete *it;
    PMTList.clear();

    for (it = PMTAddList.begin(); it != PMTAddList.end(); ++it)
        delete *it;
    PMTAddList.clear();

    return true;
}

void *DVBCam::CiHandlerThreadHelper(void *dvbcam)
{
    ((DVBCam*)dvbcam)->CiHandlerLoop();
    return NULL;
}

void DVBCam::HandleUserIO(void)
{
    cCiEnquiry* enq = ciHandler->GetEnquiry();
    if (enq != NULL)
    {
        if (enq->Text() != NULL)
            VERBOSE(VB_DVBCAM, LOC + "CAM: Received message: " +
                    enq->Text());
        delete enq;
    }

    cCiMenu* menu = ciHandler->GetMenu();
    if (menu != NULL)
    {
        if (menu->TitleText() != NULL)
            VERBOSE(VB_DVBCAM, LOC + "CAM: Menu Title: " +
                    menu->TitleText());
        if (menu->SubTitleText() != NULL)
            VERBOSE(VB_DVBCAM, LOC + "CAM: Menu SubTitle: " +
                    menu->SubTitleText());
        if (menu->BottomText() != NULL)
            VERBOSE(VB_DVBCAM, LOC + "CAM: Menu BottomText: " +
                    menu->BottomText());

        for (int i=0; i<menu->NumEntries(); i++)
            if (menu->Entry(i) != NULL)
                VERBOSE(VB_DVBCAM, LOC + "CAM: Menu Entry: " +
                        menu->Entry(i));

        if (menu->Selectable())
        {
            VERBOSE(VB_CHANNEL, LOC + "CAM: Menu is selectable");
        }

        if (menu->NumEntries() > 0)
        {
            VERBOSE(VB_DVBCAM, LOC +
                    "CAM: Selecting first entry");
            menu->Select(0);
        }
        else
        {
            VERBOSE(VB_DVBCAM, LOC + "CAM: Cancelling menu");
        }

        delete menu;
    }
}

void DVBCam::HandlePMT(void)
{
    VERBOSE(VB_DVBCAM, LOC + "CiHandler needs CA_PMT");
    QMutexLocker locker(&pmt_lock);

    if (pmt_sent && pmt_added && !pmt_updated)
    {
        // Send added PMT
        while (PMTAddList.size() > 0)
        {
            pmt_list_t::iterator it = PMTAddList.begin();
            const ChannelBase *chan = it.key();
            ProgramMapTable *pmt = (*it);
            PMTList[chan] = pmt;
            PMTAddList.erase(it);
            SendPMT(*pmt, CPLM_ADD);
        }

        pmt_updated = false;
        pmt_added   = false;
        return;
    }

    // Grab any added PMT
    while (PMTAddList.size() > 0)
    {
        pmt_list_t::iterator it = PMTAddList.begin();
        const ChannelBase *chan = it.key();
        ProgramMapTable *pmt = (*it);
        PMTList[chan] = pmt;
        PMTAddList.erase(it);
    }

    uint length = PMTList.size();
    uint count  = 0;

    pmt_list_t::const_iterator pmtit;
    for (pmtit = PMTList.begin(); pmtit != PMTList.end(); ++pmtit)
    {
        uint cplm = (count     == 0)      ? CPLM_FIRST : CPLM_MORE;
        cplm      = (count + 1 == length) ? CPLM_LAST  : cplm;
        cplm      = (length    == 1)      ? CPLM_ONLY  : cplm;

        SendPMT(**pmtit, cplm);

        count++;
    }

    pmt_sent    = true;
    pmt_updated = false;
    pmt_added   = false;
}

void DVBCam::CiHandlerLoop()
{
    VERBOSE(VB_DVBCAM, LOC + "CI handler thread running");

    while (!exitCiThread)
    {
        if (ciHandler->Process())
        {
            if (ciHandler->HasUserIO())
                HandleUserIO();

            bool handle_pmt  = pmt_sent && (pmt_updated || pmt_added);
            handle_pmt      |= have_pmt && ciHandler->NeedCaPmt();

            if (handle_pmt)
                HandlePMT();
        }
        usleep(10 * 1000);
    }
    
    ciThreadRunning = false;
    VERBOSE(VB_DVBCAM, LOC + "CiHandler thread stopped");
}

void DVBCam::SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt)
{
    QMutexLocker locker(&pmt_lock);

    pmt_list_t::iterator it = PMTList.find(chan);
    pmt_list_t::iterator it2 = PMTAddList.find(chan);
    if (!pmt && (it != PMTList.end()))
    {
        delete *it;
        PMTList.erase(it);
        pmt_updated = true;
    }
    else if (!pmt && (it2 != PMTAddList.end()))
    {
        delete *it2;
        PMTAddList.erase(it2);
        pmt_added = !PMTAddList.empty();
    }
    else if (pmt && (PMTList.empty() || (it != PMTList.end())))
    {
        if (it != PMTList.end())
            delete *it;
        PMTList[chan] = new ProgramMapTable(*pmt);
        have_pmt    = true;
        pmt_updated = true;
    }
    else if (pmt && (it == PMTList.end()))
    {
        PMTAddList[chan] = new ProgramMapTable(*pmt);
        pmt_added = true;
    }
}

void DVBCam::SetTimeOffset(double offset_in_seconds)
{
    ciHandler->SetTimeOffset(offset_in_seconds);
}

static const char *cplm_info[] =
{
    "CPLM_MORE",
    "CPLM_FIRST",
    "CPLM_LAST",
    "CPLM_ONLY",
    "CPLM_ADD",
    "CPLM_UPDATE"
};

cCiCaPmt CreateCAPMT(const ProgramMapTable&, const unsigned short*, uint);

/*
 * Send a CA_PMT object to the CAM (see EN50221, section 8.4.3.4)
 */
void DVBCam::SendPMT(const ProgramMapTable &pmt, uint cplm)
{
    bool success = false;

    for (uint s = 0; s < (uint)ciHandler->NumSlots(); s++)
    {
        const unsigned short *casids = ciHandler->GetCaSystemIds(s);

        if (!casids)
        {
            VERBOSE(success ? VB_DVBCAM : VB_IMPORTANT, LOC_ERR +
                    "GetCaSystemIds returned NULL! " +
                    QString("(Slot #%1)").arg(s));
            continue;
        }

        if (!casids[0])
        {
            VERBOSE(success ? VB_DVBCAM : VB_IMPORTANT, LOC_ERR + "CAM supports no CA systems! " +
                    QString("(Slot #%1)").arg(s));
            continue;
        }

        VERBOSE(VB_DVBCAM, LOC + "Creating CA_PMT, ServiceID = "
                << pmt.ProgramNumber());

        cCiCaPmt capmt = CreateCAPMT(pmt, casids, cplm);

        VERBOSE(VB_DVBCAM, LOC +
                QString("Sending CA_PMT with %1 to CI slot #%2")
                .arg(cplm_info[cplm]).arg(s));

        if (!ciHandler->SetCaPmt(capmt, s))
            VERBOSE(success ? VB_DVBCAM : VB_IMPORTANT, LOC + "CA_PMT send failed!");
        else
            success = true;
    }
}

void process_desc(cCiCaPmt &capmt,
                  const unsigned short *casids,
                  const desc_list_t &desc)
{
    desc_list_t::const_iterator it;
    for (it = desc.begin(); it != desc.end(); ++it)
    {
        ConditionalAccessDescriptor cad(*it);
        for (uint q = 0; casids[q]; q++)
        {
            if (cad.SystemID() != casids[q])
                continue;

            VERBOSE(VB_DVBCAM,
                    QString("Adding CA descriptor: "
                            "CASID(0x%2), ECM PID(0x%3)")
                    .arg(cad.SystemID(),0,16).arg(cad.PID(),0,16));

            capmt.AddCaDescriptor(cad.SystemID(), cad.PID(),
                                  cad.DataSize(), cad.Data());
        }
    }

}

cCiCaPmt CreateCAPMT(const ProgramMapTable &pmt,
                     const unsigned short *casids,
                     uint cplm)
{
    cCiCaPmt capmt(pmt.ProgramNumber(), cplm);

    // Add CA descriptors for the service
    desc_list_t gdesc = MPEGDescriptor::ParseOnlyInclude(
        pmt.ProgramInfo(), pmt.ProgramInfoLength(),
        DescriptorID::conditional_access);

    process_desc(capmt, casids, gdesc);

    // Add elementary streams + CA descriptors
    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        VERBOSE(VB_DVBCAM,
                QString("Adding elementary stream: %1, pid(0x%2)")
                .arg(pmt.StreamDescription(i, "dvb"))
                .arg(pmt.StreamPID(i),0,16));
            
        capmt.AddElementaryStream(pmt.StreamType(i), pmt.StreamPID(i));

        desc_list_t desc = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::conditional_access);

        process_desc(capmt, casids, desc);
    }
    return capmt;
}
