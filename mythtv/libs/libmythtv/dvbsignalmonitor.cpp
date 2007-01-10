// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>
#include <cmath>

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"
#include "dvbchannel.h"
#include "dvbstreamdata.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "cardutil.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "DVBSM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

#define LOC QString("DVBSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("DVBSM(%1), Error: ").arg(channel->GetDevice())

/** \fn DVBSignalMonitor::DVBSignalMonitor(int,DVBChannel*,uint,const char*)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel DVBChannel for card
 *  \param _flags   Flags to start with
 *  \param _name    Instance name for Qt signal/slot debugging
 */
DVBSignalMonitor::DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                                   uint _flags, const char *_name)
    : DTVSignalMonitor(db_cardnum, _channel, _flags, _name),
      // This snr setup is incorrect for API 3.x but works better 
      // than int16_t range in practice, however this is correct 
      // for the 4.0 DVB API which uses a uint16_t for the snr
      signalToNoise    (tr("Signal To Noise"),    "snr",
                        0,      true,      0, 65535, 0),
      bitErrorRate     (tr("Bit Error Rate"),     "ber",
                        65535,  false,     0, 65535, 0),
      uncorrectedBlocks(tr("Uncorrected Blocks"), "ucb",
                        65535,  false,     0, 65535, 0),
      rotorPosition    (tr("Rotor Progress"),     "pos",
                        100,    true,      0,   100, 0),
      useSectionReader(false),
      dtvMonitorRunning(false)
{
    // These two values should probably come from the database...
    int wait = 3000; // timeout when waiting on signal
    int threshold = 0; // signal strength threshold

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);

    // This is incorrect for API 3.x but works better than int16_t range
    // in practice, however this is correct for the 4.0 DVB API
    signalStrength.SetRange(0, 65535);

    // We use uint16_t for sig & snr because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t sig = 0, snr     = 0;
    uint32_t ber = 0, ublocks = 0;
    fe_status_t  status;
    bzero(&status, sizeof(status));

    uint newflags = 0;
    QString msg = QString("DVBSignalMonitor(%1)::constructor(%2,%3): %4")
        .arg(channel->GetDevice()).arg(capturecardnum);

#define DVB_IO(WHAT,WHERE,ERRMSG,FLAG) \
    if (ioctl(_channel->GetFd(), WHAT, WHERE)) \
        VERBOSE(VB_IMPORTANT, msg.arg(ERRMSG).arg(strerror(errno))); \
    else newflags |= FLAG;

    DVB_IO(FE_READ_SIGNAL_STRENGTH, &sig,
           "Warning, can not measure Signal Strength", kDTVSigMon_WaitForSig);
    DVB_IO(FE_READ_SNR, &snr,
           "Warning, can not measure S/N", kDVBSigMon_WaitForSNR);
    DVB_IO(FE_READ_BER, &ber,
           "Warning, can not measure Bit Error Rate", kDVBSigMon_WaitForBER);
    DVB_IO(FE_READ_UNCORRECTED_BLOCKS, &ublocks,
           "Warning, can not count Uncorrected Blocks", kDVBSigMon_WaitForUB);
    DVB_IO(FE_READ_STATUS, &status, "Error, can not read status!", 0);
#undef DVB_IO
    AddFlags(newflags);
    DBG_SM("constructor()", QString("initial flags 0x%1").arg(newflags,0,16));

    minimum_update_rate = _channel->GetMinSignalMonitorDelay();
    if (minimum_update_rate > 30)
        usleep(minimum_update_rate * 1000);
}

/** \fn DVBSignalMonitor::~DVBSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
DVBSignalMonitor::~DVBSignalMonitor()
{
    Stop();
}

void DVBSignalMonitor::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    Stop();
    DTVSignalMonitor::deleteLater();
}

// documented in dtvsignalmonitor.h
void DVBSignalMonitor::SetRotorTarget(float target)
{
    QMutexLocker locker(&statusLock);
    rotorPosition.SetThreshold((int)roundf(100 * target));
}

/** \fn DVBSignalMonitor::GetDVBCardNum(void) const
 *  \brief Returns DVB Card Number from DVBChannel.
 */
int DVBSignalMonitor::GetDVBCardNum(void) const
{
    return dynamic_cast<DVBChannel*>(channel)->GetCardNum();
}

/** \fn DVBSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void DVBSignalMonitor::Stop(void)
{
    DBG_SM("Stop", "begin");
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    DBG_SM("Stop", "end");
}

QStringList DVBSignalMonitor::GetStatusList(bool kick)
{
    QStringList list = DTVSignalMonitor::GetStatusList(kick);
    statusLock.lock();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        list<<signalToNoise.GetName()<<signalToNoise.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForBER))
        list<<bitErrorRate.GetName()<<bitErrorRate.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForUB))
        list<<uncorrectedBlocks.GetName()<<uncorrectedBlocks.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForPos))
        list<<rotorPosition.GetName()<<rotorPosition.GetStatus();
    statusLock.unlock();
    return list;
}

void *DVBSignalMonitor::TableMonitorThread(void *param)
{
    DVBSignalMonitor *mon = (DVBSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

const int buffer_size = TSPacket::SIZE * 1500;

bool DVBSignalMonitor::AddPIDFilter(uint pid)
{ 
    DBG_SM(QString("AddPIDFilter(0x%1)").arg(pid, 0, 16), "");

    QString demux_fn = CardUtil::GetDeviceName(DVB_DEV_DEMUX, GetDVBCardNum());
    int mux_fd = open(demux_fn.ascii(), O_RDWR | O_NONBLOCK);
    if (mux_fd == -1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to open demux device %1 for filter on pid %2")
                .arg(demux_fn).arg(pid));
        return false;
    }

    if (!useSectionReader)
    {
        struct dmx_pes_filter_params pesFilterParams;
        bzero(&pesFilterParams, sizeof(struct dmx_pes_filter_params));
        pesFilterParams.pid      = (__u16) pid;
        pesFilterParams.input    = DMX_IN_FRONTEND;
        pesFilterParams.output   = DMX_OUT_TS_TAP;
        pesFilterParams.flags    = DMX_IMMEDIATE_START;
        pesFilterParams.pes_type = DMX_PES_OTHER;

        if (ioctl(mux_fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Failed to set TS filter (pid %1)").arg(pid));
            close(mux_fd);
            return false;
        }
    }
    else
    {
        struct dmx_sct_filter_params sctFilterParams;
        bzero(&sctFilterParams, sizeof(struct dmx_sct_filter_params));
        switch ( (__u16) pid )
        {
            case 0x0: // PAT
                sctFilterParams.filter.filter[0] = 0;
                sctFilterParams.filter.mask[0]   = 0xff;
                break;
            case 0x0010: // assume this is for an NIT, NITo, PMT
                // This filter will give us table ids 0x00-0x03, 0x40-0x43
                // we expect to see table ids 0x02, 0x40 and 0x41 on this PID
                // NOTE: In theory, this will break with ATSC when PID 0x10
                //       is used for ATSC/MPEG tables. This is frowned upon,
                //       but PMTs have been seen on in the wild.
                sctFilterParams.filter.filter[0] = 0x00;
                sctFilterParams.filter.mask[0]   = 0xbc;
                break;
            case 0x0011: // assume this is for an SDT, SDTo, PMT
                // This filter will give us table ids 0x02, 0x06, 0x42 and 0x46
                // All but 0x06 are ones we want to see.
                // NOTE: In theory this will break with ATSC when pid 0x11
                //       is used for random ATSC tables. In practice only
                //       video data has been seen on 0x11.
                sctFilterParams.filter.filter[0] = 0x02;
                sctFilterParams.filter.mask[0]   = 0xbb;
                break;
            case 0x1ffb: // assume this is for various ATSC tables
                // MGT 0xC7, Terrestrial VCT 0xC8, Cable VCT 0xC9, RRT 0xCA,
                // STT 0xCD, DCCT 0xD3, DCCSCT 0xD4, Caption 0x86
                sctFilterParams.filter.filter[0] = 0x80;
                sctFilterParams.filter.mask[0]   = 0xa0;
                break;
            default:
                // otherwise assume it could be any table
                sctFilterParams.filter.filter[0] = 0x00;
                sctFilterParams.filter.mask[0]   = 0x00;
                break;
        }
        sctFilterParams.pid            = (__u16) pid;
        sctFilterParams.timeout        = 0;
        sctFilterParams.flags          = DMX_IMMEDIATE_START; 

        if (ioctl(mux_fd, DMX_SET_FILTER, &sctFilterParams) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + 
                    "Failed to set \"section\" filter " +
                    QString("(pid %1) (filter %2)").arg(pid)
                    .arg(sctFilterParams.filter.filter[0]));
            close(mux_fd);
            return false;
        }
    }

    filters[pid] = mux_fd;

    return true;
}

bool DVBSignalMonitor::RemovePIDFilter(uint pid)
{
    DBG_SM(QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16), "");
    if (filters.find(pid)==filters.end())
        return false;

    int mux_fd = filters[pid];
    filters.erase(filters.find(pid));
    int err = close(mux_fd);
    if (err < 0)
        VERBOSE(VB_IMPORTANT, QString("Failed to close mux (pid 0x%1)")
                .arg(pid, 0, 16));

    return err >= 0;
}

bool DVBSignalMonitor::UpdateFiltersFromStreamData(void)
{
    if (!GetStreamData())
        return false;

    UpdateListeningForEIT();

    const QMap<uint, bool> &listening = GetStreamData()->ListeningPIDs();
    vector<uint> add_pids, del_pids;

    // PIDs that need to be added..
    QMap<uint, bool>::const_iterator lit = listening.constBegin();
    for (; lit != listening.constEnd(); ++lit)
        if (lit.data() && (filters.find(lit.key()) == filters.end()))
            add_pids.push_back(lit.key());

    // PIDs that need to be removed..
    FilterMap::const_iterator fit = filters.constBegin();
    for (; fit != filters.constEnd(); ++fit)
        if (listening.find(fit.key()) == listening.end())
            del_pids.push_back(fit.key());

    // Remove PIDs
    bool ok = true;
    vector<uint>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    // Add PIDs
    vector<uint>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
        ok &= AddPIDFilter(*ait);

    return ok;
}

/** \fn DVBSignalMonitor::RunTableMonitorTS(void)
 *  \brief Uses TS filtering monitor to monitor a DVB device for tables
 *
 *  This supports all types of MPEG based stream data, but is extreemely
 *  slow with DVB over USB 1.0 devices which for efficiency reasons buffer 
 *  a stream until a full block transfer buffer full of the requested 
 *  tables is available. This takes a very long time when you are just
 *  waiting for a PAT or PMT table, and the buffer is hundreds of packets
 *  in size.
 */
void DVBSignalMonitor::RunTableMonitorTS(void)
{
    int remainder = 0;
    int buffer_size = TSPacket::SIZE * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;
    bzero(buffer, buffer_size);

    QString dvr_fname = CardUtil::GetDeviceName(DVB_DEV_DVR, GetDVBCardNum());
    int dvr_fd = open(dvr_fname.ascii(), O_RDONLY | O_NONBLOCK);
    if (dvr_fd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to open DVR device %1 : %2")
                .arg(dvr_fname).arg(strerror(errno)));
        delete[] buffer;
        return;
    }

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorTS(): " +
            QString("begin (# of pids %1)")
            .arg(GetStreamData()->ListeningPIDs().size()));

    fd_set fd_select_set;
    FD_ZERO(        &fd_select_set);
    FD_SET (dvr_fd, &fd_select_set);
    while (dtvMonitorRunning && GetStreamData())
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        // timeout gets reset by select, so we need to create new one
        struct timeval timeout = { 0, 50 /* ms */ * 1000 /* -> usec */ };
        select(dvr_fd+1, &fd_select_set, NULL, NULL, &timeout);

        long long len = read(
            dvr_fd, &(buffer[remainder]), buffer_size - remainder);

        if ((0 == len) || (-1 == len))
        {
            usleep(100);
            continue;
        }

        len += remainder;

        if (len < 10) // 10 bytes = 4 bytes TS header + 6 bytes PES header
        {
            remainder = len;
            continue;
        }

        remainder = GetStreamData()->ProcessData(buffer, len);
        if (remainder > 0 && (len > remainder)) // leftover bytes
            memmove(buffer, &(buffer[len - remainder]), remainder);
    }
    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorTS(): " + "shutdown");

    if (GetStreamData())
    {
        vector<int> del_pids;
        FilterMap::iterator it = filters.begin();
        for (; it != filters.end(); ++it)
            del_pids.push_back(it.key());

        vector<int>::iterator dit = del_pids.begin();
        for (; dit != del_pids.end(); ++dit)
            RemovePIDFilter(*dit);
    }

    close(dvr_fd);
    delete[] buffer;

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorTS(): " + "end");
}

/** \fn DVBSignalMonitor::RunTableMonitorSR(void)
 *  \brief Uses "Section" monitor to monitor a DVB device for tables
 *
 *  This currently only supports DVB streams, ATSC and the raw MPEG
 *  streams used by some cable and satelite providers is not supported.
 */
void DVBSignalMonitor::RunTableMonitorSR(void)
{
    int buffer_size = 4192;  // maximum size of Section we handle
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorSR(): " +
            QString("begin (# of pids %1)")
            .arg(GetStreamData()->ListeningPIDs().size()));

    while (dtvMonitorRunning && GetStreamData())
    {
        RetuneMonitor();
        UpdateFiltersFromStreamData();

        bool readSomething = false;
        FilterMap::const_iterator fit = filters.begin();
        for (; fit != filters.end(); ++fit)
        {
            int len = read(fit.data() /* mux_fd */, buffer, buffer_size);
            if (len <= 0)
                continue;

            readSomething = true;

            const PESPacket pes = PESPacket::ViewData(buffer);
            const PSIPTable psip(pes);

            if (psip.SectionSyntaxIndicator())
                GetStreamData()->HandleTables(fit.key() /* pid */, psip);
        }

        if (!readSomething)
            usleep(3000);
    }
    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorSR(): " + "shutdown");

    if (GetStreamData())
    {
        vector<int> del_pids;
        FilterMap::iterator it = filters.begin();
        for (; it != filters.end(); ++it)
            del_pids.push_back(it.key());

        vector<int>::iterator dit = del_pids.begin();
        for (; dit != del_pids.end(); ++dit)
            RemovePIDFilter(*dit);
    }

    delete[] buffer;

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitorSR(): " + "end");
}

// for caching TS monitoring supported value.
static QMap<uint,bool> _rec_supports_ts_monitoring;
static QMutex          _rec_supports_ts_monitoring_lock;

/** \fn DVBSignalMonitor::SupportsTSMonitoring(void)
 *  \brief Returns true if TS monitoring is supported.
 *
 *   NOTE: If you are using a DEC2000-t device you need to
 *   apply the patches provided by Peter Beutner for it, see
 *   http://www.gossamer-threads.com/lists/mythtv/dev/166172
 *   These patches should make it in to Linux 2.6.15 or 2.6.16.
 */
bool DVBSignalMonitor::SupportsTSMonitoring(void)
{
    const uint pat_pid = 0x0;

    {
        QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
        QMap<uint,bool>::const_iterator it;
        it = _rec_supports_ts_monitoring.find(GetDVBCardNum());
        if (it != _rec_supports_ts_monitoring.end())
            return *it;
    }

    QString dvr_fname = CardUtil::GetDeviceName(DVB_DEV_DVR, GetDVBCardNum());
    int dvr_fd = open(dvr_fname.ascii(), O_RDONLY | O_NONBLOCK);
    if (dvr_fd < 0)
    {
        QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
        _rec_supports_ts_monitoring[GetDVBCardNum()] = false;
        return false;
    }

    bool supports_ts = false;
    if (AddPIDFilter(pat_pid))
    {
        supports_ts = true;
        RemovePIDFilter(pat_pid);
    }

    close(dvr_fd);

    QMutexLocker locker(&_rec_supports_ts_monitoring_lock);
    _rec_supports_ts_monitoring[GetDVBCardNum()] = supports_ts;
    return supports_ts;
}

void DVBSignalMonitor::RetuneMonitor(void)
{
    DVBChannel *dvbchan = dynamic_cast<DVBChannel*>(channel);

    // Rotor position
    if (HasFlags(kDVBSigMon_WaitForPos))
    {
        const DiSEqCDevRotor *rotor = dvbchan->GetRotor();
        if (rotor)
        {
            bool was_moving, is_moving;
            {
                QMutexLocker locker(&statusLock);
                was_moving = rotorPosition.GetValue() < 100;
                int pos    = (int)truncf(rotor->GetProgress() * 100);
                rotorPosition.SetValue(pos);
                is_moving  = rotorPosition.GetValue() < 100;
            }
            
            // Retune if move completes normally
            if (was_moving && !is_moving)
            {
                DBG_SM("UpdateValues", "Retuning for rotor completion");
                dvbchan->Retune();

                // (optionally) No need to wait for SDT anymore...
                // RemoveFlags(kDTVSigMon_WaitForSDT);
            }
        }
        else 
        {
            // If no rotor is present, pretend the movement is completed
            QMutexLocker locker(&statusLock);
            rotorPosition.SetValue(100);
        }
    }
}

void DVBSignalMonitor::HandlePMT(uint program_num, const ProgramMapTable *pmt)
{
    DTVSignalMonitor::HandlePMT(program_num, pmt);

    if (pmt->ProgramNumber() == (uint)programNumber)
        GetDVBChannel()->SetPMT(pmt);
}

void DVBSignalMonitor::HandleSTT(const SystemTimeTable *stt)
{
    DTVSignalMonitor::HandleSTT(stt);
    GetDVBChannel()->SetTimeOffset(GetStreamData()->TimeOffset());
}

void DVBSignalMonitor::HandleTDT(const TimeDateTable *tdt)
{
    DTVSignalMonitor::HandleTDT(tdt);
    GetDVBChannel()->SetTimeOffset(GetStreamData()->TimeOffset());
}

DVBChannel *DVBSignalMonitor::GetDVBChannel(void)
{
    return dynamic_cast<DVBChannel*>(channel);
}

void DVBSignalMonitor::RunTableMonitor(void)
{
    dtvMonitorRunning = true;

    if (useSectionReader = !SupportsTSMonitoring())
        RunTableMonitorSR();
    else
        RunTableMonitorTS();
}

/** \fn DVBSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This function uses five ioctl's FE_READ_SNR, FE_READ_SIGNAL_STRENGTH
 *   FE_READ_BER, FE_READ_UNCORRECTED_BLOCKS, and FE_READ_STATUS to obtain
 *   statistics from the frontend.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void DVBSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (dtvMonitorRunning)
    {
        EmitDVBSignals();
        if (IsAllGood())
            emit AllGood();
        // TODO dtv signals...

        update_done = true;
        return;
    }

    RetuneMonitor();

    bool wasLocked = false, isLocked = false;
    // We use uint16_t for sig & snr because this is correct for DVB API 4.0,
    // and works better than the correct int16_t for the 3.x API
    uint16_t sig = 0, snr     = 0;
    uint32_t ber = 0, ublocks = 0;
    fe_status_t  status;
    bzero(&status, sizeof(status));

    // Get info from card
    int fd_frontend = channel->GetFd();
    ioctl(fd_frontend, FE_READ_STATUS, &status);
    if (HasFlags(kDTVSigMon_WaitForSig))
        ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &sig);
    if (HasFlags(kDVBSigMon_WaitForSNR))
        ioctl(fd_frontend, FE_READ_SNR, &snr);
    if (HasFlags(kDVBSigMon_WaitForBER))
        ioctl(fd_frontend, FE_READ_BER, &ber);
    if (HasFlags(kDVBSigMon_WaitForUB))
        ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &ublocks);

    // Set SignalMonitorValues from info from card.
    {
        QMutexLocker locker(&statusLock);

        // BER and UB are actually uint32 values, but we 
        // clamp them at 64K. This is because these values
        // are acutally cumulative, but we don't try to 
        // normalize these to a time period.

        wasLocked = signalLock.IsGood();
        signalLock.SetValue((status & FE_HAS_LOCK) ? 1 : 0);
        isLocked = signalLock.IsGood();

        if (HasFlags(kDTVSigMon_WaitForSig))
            signalStrength.SetValue(sig);
        if (HasFlags(kDVBSigMon_WaitForSNR))
            signalToNoise.SetValue(snr);
        if (HasFlags(kDVBSigMon_WaitForBER))
            bitErrorRate.SetValue(ber);
        if (HasFlags(kDVBSigMon_WaitForUB))
            uncorrectedBlocks.SetValue(ublocks);
    }

    // Debug output
    if (wasLocked != isLocked)
        DBG_SM("UpdateValues", "Signal "<<(isLocked ? "Locked" : "Lost"));

    EmitDVBSignals();
    if (IsAllGood())
        emit AllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        (!HasFlags(kDVBSigMon_WaitForPos) || rotorPosition.IsGood()) &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        pthread_create(&table_monitor_thread, NULL,
                       TableMonitorThread, this);
        DBG_SM("UpdateValues", "Waiting for table monitor to start");
        while (!dtvMonitorRunning)
            usleep(50);
        DBG_SM("UpdateValues", "Table monitor started");
    }

    update_done = true;
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

/** \fn DVBSignalMonitor::EmitDVBSignals(void)
 *  \brief Emits signals for lock, signal strength, etc.
 */
void DVBSignalMonitor::EmitDVBSignals(void)
{
    // Emit signals..
    EMIT(StatusSignalLock, signalLock); 
    if (HasFlags(kDTVSigMon_WaitForSig))
        EMIT(StatusSignalStrength, signalStrength);
    if (HasFlags(kDVBSigMon_WaitForSNR))
        EMIT(StatusSignalToNoise, signalToNoise);
    if (HasFlags(kDVBSigMon_WaitForBER))
        EMIT(StatusBitErrorRate, bitErrorRate);
    if (HasFlags(kDVBSigMon_WaitForUB))
        EMIT(StatusUncorrectedBlocks, uncorrectedBlocks);
    if (HasFlags(kDVBSigMon_WaitForPos))
        EMIT(StatusRotorPosition, rotorPosition);
}

#undef EMIT
