// -*- Mode: c++ -*-

// C includes
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

// Qt includes
#include <qmutex.h>

// MythTV includes - General
#include "siscan.h"
#include "scheduledrecording.h"
#include "frequencies.h"
#include "mythdbcon.h"
#include "channelbase.h"
#include "channelutil.h"
#include "cardutil.h"

#ifdef USING_V4L
#include "channel.h"
#endif // USING_V4L

// MythTV includes - DTV
#include "dtvsignalmonitor.h"
#include "scanstreamdata.h"

// MythTV includes - ATSC
#include "atsctables.h"

// MythTV includes - DVB
#include "dvbsignalmonitor.h"
#include "dvbtables.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbtypes.h"
#endif // USING_DVB

#ifdef USING_HDHOMERUN
#include "hdhrchannel.h"
#endif // USING_HDHOMERUN

QString SIScan::loc(const SIScan *siscan)
{
    if (siscan && siscan->channel)
        return QString("SIScan(%1)").arg(siscan->channel->GetDevice());
    return "SIScan(u)";
}

#define LOC     (SIScan::loc(this) + ": ")
#define LOC_ERR (SIScan::loc(this) + ", Error: ")

/** \class SIScan
 *  \brief Scanning class for cards that support a SignalMonitor class.
 *
 *   Currently both SIParser and ScanStreamData are being used in
 *   this class. The SIParser is being phased out, so that is not
 *   described here.
 *
 *   With ScanStreamData, we call ScanTransport() on each transport 
 *   and frequency offset in the list of transports. This list is 
 *   created from a FrequencyTable object.
 *
 *   Each ScanTransport() call resets the ScanStreamData and the
 *   SignalMonitor, then tunes to a new frequency and notes the tuning
 *   time in the "timer" QTime object.
 *
 *   HandleActiveScan is called every time through the event loop
 *   and is what calls ScanTransport(), as well as checking when
 *   the current time is "timeoutTune" or "channelTimeout" milliseconds
 *   ahead of "timer". When the "timeoutTune" is exceeded we check
 *   to see if we have a signal lock on the channel, if we don't we
 *   check the next transport. When the larger "channelTimeout" is
 *   exceeded we do nothing unless "waitingForTables" is still true,
 *   if so we check if we at least got a PAT and if so we insert
 *    a channel based on that by calling HandleMPEGDBInsertion().
 *
 *   Meanwhile the ScanStreamData() emits several signals. For 
 *   the UI it emits signal quality signals. For SIScan it emits
 *   UpdateMGT, UpdateVCT, UpdateNIT, and UpdateSDT signals. We
 *   connect these to the HandleMGT, HandleVCT, etc. These in
 *   turn just call HandleATSCDBInsertion() or
 *   HandleDVBDBInsertion() depending on the type of table.
 *
 *   HandleATSCDBInsertion() first checks if we have all the VCTs 
 *   described in the MGT. If we do we call UpdateVCTinDB() for each
 *   TVCT and CVCT in the stream. UpdateVCTinDB() inserts the actual
 *   channels. Then we set "waitingForTables" to false, set the
 *   scanOffsetIt to 99 and updates the UI to reflect the added channel.
 *   HandleDVBDBInsertion() and HandleMPEGDBInsertion() are similar.
 */

/** \fn SIScan::SIScan(QString,ChannelBase*,int,uint,uint)
 */
SIScan::SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID,
               uint signal_timeout, uint channel_timeout)
    : // Set in constructor
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      scanMode(IDLE),
      signalTimeout(signal_timeout),
      channelTimeout(channel_timeout),
      // Settable
      ignoreAudioOnlyServices(false),
      ignoreDataServices(false),
      ignoreEncryptedServices(false),
      forceUpdate(false),
      renameChannels(false),
      channelFormat("%1_%2"),
      // State
      threadExit(false),
      waitingForTables(false),
      // Transports List
      transportsScanned(0),
      // Misc
      scanner_thread_running(false)
{
    // Initialize statics
    init_freq_tables();
    current = scanTransports.end();

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        VERBOSE(VB_SIPARSER, LOC + "Connecting up DTVSignalMonitor");
        ScanStreamData *data = new ScanStreamData();

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                            kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT);

        data->AddMPEGListener(this);
        data->AddATSCMainListener(this);
        data->AddDVBMainListener(this);
    }
}

SIScan::~SIScan(void)
{
    StopScanner();
    VERBOSE(VB_SIPARSER, LOC + "SIScanner Stopped");
    if (signalMonitor)
        delete signalMonitor;
}

/** \fn SIScan::ScanServicesSourceID(int)
 *  \brief If we are not already scanning a frequency table, this creates
 *         a new frequency table from database and begins scanning it.
 *
 *   This is used by DVB to scan for channels we are told about from other
 *   channels.
 *
 *   Note: Something similar could be used with ATSC when EIT for other
 *   channels is available on another ATSC channel, as encouraged by the
 *   ATSC specification.
 */
bool SIScan::ScanServicesSourceID(int SourceID)
{
    if (scanMode == TRANSPORT_LIST)
        return false;

    nextIt = scanTransports.end();

    MSqlQuery query(MSqlQuery::InitCon());
    // Run DB query to get transports on sourceid SourceID
    // connected to this card
    QString theQuery = QString(
        "SELECT sourceid, mplexid, sistandard, transportid "
        "FROM dtv_multiplex WHERE sourceid = %1").arg(SourceID);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Get Transports for SourceID", query);
        return false;
    }

    if (query.size() <= 0)
    {
        VERBOSE(VB_SIPARSER, LOC + "Unable to find any transports for " +
                QString("sourceid %1").arg(sourceID));

        return false;
    }

    while (query.next())
    {
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);

        VERBOSE(VB_SIPARSER, LOC + "Adding " + fn);

        TransportScanItem item(sourceid, std, fn, mplexid, signalTimeout);
        scanTransports += item;
    }

    waitingForTables  = false;
    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
}

void SIScan::HandlePAT(const ProgramAssociationTable *pat)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Program Association Table for %1")
            .arg((*current).FriendlyName));

    // Add pmts to list, so we can do MPEG scan properly.
    ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    for (uint i = 0; i < pat->ProgramCount(); i++)
    {
        if (pat->ProgramPID(i)) // don't add NIT "program", MPEG/ATSC safe.
            sd->AddListeningPID(pat->ProgramPID(i));
    }
}

void SIScan::HandleVCT(uint, const VirtualChannelTable*)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got a Virtual Channel Table for %1")
            .arg((*current).FriendlyName));

    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMGT(const MasterGuideTable*)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got the Master Guide for %1")
            .arg((*current).FriendlyName));

    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleSDT(uint, const ServiceDescriptionTable* sdt)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Service Description Table for %1")
            .arg((*current).FriendlyName));
    VERBOSE(VB_SIPARSER, LOC + sdt->toString());

    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleNIT(const NetworkInformationTable *nit)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Network Information Table for %1")
            .arg((*current).FriendlyName));
    VERBOSE(VB_SIPARSER, LOC + nit->toString());

    dvbChanNums.clear();

    //emit TransportScanUpdateText(tr("Optimizing transport frequency"));
    //OptimizeNITFrequencies(&nit);

    if (nit->TransportStreamCount())
    {
        emit TransportScanUpdateText(
            tr("Network %1 Processing").arg(nit->NetworkName()));

        vector<uint> mp;
        mp = ChannelUtil::CreateMultiplexes(sourceID, nit);
        VERBOSE(VB_SIPARSER, QString("Created %1 multiplexes from NIT")
                .arg(mp.size()));

        // Get channel numbers from UK Frequency List Descriptors
        for (uint i = 0; i < nit->TransportStreamCount(); i++)
        {
            const desc_list_t& list =
                MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                      nit->TransportDescriptorsLength(i));

            const unsigned char* desc =
                MPEGDescriptor::Find(list, DescriptorID::dvb_uk_channel_list);

            if (desc)
            {
                UKChannelListDescriptor uklist(desc);
                for (uint j = 0; j < uklist.ChannelCount(); j++)
                    dvbChanNums[uklist.ServiceID(j)] = uklist.ChannelNumber(j);
            }
        }
    }

    const ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    const DVBStreamData &dsd = *sd;
    if (dsd.HasAllNITSections())
    {
        emit TransportScanUpdateText(tr("Finished processing Transport List"));
        emit TransportScanComplete();
    }

    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMPEGDBInsertion(const ScanStreamData *sd, bool)
{
    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    if ((*current).mplexid <= 0)
        return;

    int     mplexid = (*current).mplexid;
    int     freqid  = (*current).friendlyNum;
    QString fn      = (*current).FriendlyName;

    pat_vec_t pats = sd->GetCachedPATs();
    pmt_map_t pmt_map = sd->GetCachedPMTMap();
    for (uint i = 0; i < pats.size(); i++)
    {
        UpdatePATinDB(mplexid, fn, freqid, pats[i], pmt_map,
                      (*current).expectedChannels, true);
    }
    sd->ReturnCachedPMTTables(pmt_map);
    sd->ReturnCachedPATTables(pats);

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
}

void SIScan::HandleATSCDBInsertion(const ScanStreamData *sd,
                                   bool wait_until_complete)
{
    if (wait_until_complete && !sd->HasCachedAllVCTs())
        return;

    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    if ((*current).mplexid <= 0)
        return;

    int     mplexid = (*current).mplexid;
    int     freqid  = (*current).friendlyNum;
    QString fn      = (*current).FriendlyName;

    // Insert Terrestrial VCTs
    tvct_vec_t tvcts = sd->GetAllCachedTVCTs();
    for (uint i = 0; i < tvcts.size(); i++)
    {
        UpdateVCTinDB(mplexid, fn, freqid, tvcts[i],
                      (*current).expectedChannels, true);
    }
    sd->ReturnCachedTVCTTables(tvcts);

    // Insert Cable VCTs
    cvct_vec_t cvcts = sd->GetAllCachedCVCTs();
    for (uint i = 0; i < cvcts.size(); i++)
    {
        UpdateVCTinDB(mplexid, fn, freqid, cvcts[i],
                      (*current).expectedChannels, true);
    }
    sd->ReturnCachedCVCTTables(cvcts);

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
}

void SIScan::HandleDVBDBInsertion(const ScanStreamData *sd,
                                  bool wait_until_complete)
{
    const DVBStreamData &dsd = (const DVBStreamData &)(*sd);
    if (wait_until_complete && !(dsd.HasCachedSDT() && dsd.HasCachedAllNIT()))
        return;

    emit ServiceScanUpdateText(tr("Updating Services"));

    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    vector<const ServiceDescriptionTable*> sdts = sd->GetAllCachedSDTs();
    for (uint i = 0; i < sdts.size(); i++)
    {
        UpdateSDTinDB((*current).mplexid, sdts[i],
                      (*current).expectedChannels, forceUpdate);
    }
    sd->ReturnCachedSDTTables(sdts);

    emit ServiceScanUpdateText(tr("Finished processing Services"));

    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
    else
    {
        emit PctServiceScanComplete(100);
        emit ServiceScanComplete();
    }    
}

/** \fn SIScan::HandlePostInsertion(void)
 *  \brief Insert channels based on any partial tables we do have.
 *  \return true if we saw any tables
 */
bool SIScan::HandlePostInsertion(void)
{
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (!dtvSigMon)
        return false;

    const ScanStreamData *sd = dtvSigMon->GetScanStreamData();

    VERBOSE(VB_SIPARSER, LOC + "HandlePostInsertion() " +
            QString("pat(%1)").arg(sd->HasCachedAnyPAT()));

    const MasterGuideTable *mgt = sd->GetCachedMGT();
    if (mgt)
    {
        VERBOSE(VB_IMPORTANT, mgt->toString());
        HandleATSCDBInsertion(sd, false);
        sd->ReturnCachedTable(mgt);
        return true;
    }

    const NetworkInformationTable *nit = sd->GetCachedNIT(0);
    sdt_vec_t sdts = sd->GetAllCachedSDTs();
    if (nit || sdts.size())
    {
        if (nit)
            VERBOSE(VB_IMPORTANT, nit->toString());
        HandleDVBDBInsertion(sd, false);
        sd->ReturnCachedSDTTables(sdts);
        sd->ReturnCachedTable(nit);
        return true;
    }

    if (sd->HasCachedAnyPAT())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Post insertion found PAT..");
        HandleMPEGDBInsertion(sd, false);
        return true;
    }
    return false;
}

DTVSignalMonitor* SIScan::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(signalMonitor);
}

DVBSignalMonitor* SIScan::GetDVBSignalMonitor(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBSignalMonitor*>(signalMonitor);
#else
    return NULL;
#endif
}

DVBChannel *SIScan::GetDVBChannel(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif
}

HDHRChannel *SIScan::GetHDHRChannel(void)
{
#ifdef USING_HDHOMERUN
    return dynamic_cast<HDHRChannel*>(channel);
#else
    return NULL;
#endif
}

Channel *SIScan::GetChannel(void)
{
#ifdef USING_V4L
    return dynamic_cast<Channel*>(channel);
#else
    return NULL;
#endif
}

/** \fn SIScan::SpawnScanner(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call SIScan::RunScanner().
 */
void *SIScan::SpawnScanner(void *param)
{
    SIScan *scanner = (SIScan *)param;
    scanner->RunScanner();
    return NULL;
}

/** \fn SIScan::StartScanner(void)
 *  \brief Starts the SIScan event loop.
 */
void SIScan::StartScanner(void)
{
    pthread_create(&scanner_thread, NULL, SpawnScanner, this);
}

/** \fn SIScan::RunScanner(void)
 *  \brief This runs the event loop for SIScan until 'threadExit' is true.
 */
void SIScan::RunScanner(void)
{
    VERBOSE(VB_SIPARSER, LOC + "Starting SIScanner");

    scanner_thread_running = true;
    threadExit = false;

    while (!threadExit)
    {
        if (scanMode == TRANSPORT_LIST)
            HandleActiveScan();

        usleep(250);
    }
    scanner_thread_running = false;
}

// See if we have timed out
bool SIScan::HasTimedOut(void)
{
    if (!waitingForTables)
        return true;

    QString offset_str = current.offset() ?
        QObject::tr(" offset %2").arg(current.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);
    QString time_out_table_str =
        QObject::tr("Timeout Scanning %1 -- no tables").arg(cur_chan);
    QString time_out_sig_str =
        QObject::tr("Timeout Scanning %1 -- no signal").arg(cur_chan);

    // have the tables have timed out?
    if (timer.elapsed() > (int)channelTimeout)
    { 
        emit ServiceScanUpdateText(time_out_table_str);
        VERBOSE(VB_SIPARSER, LOC + time_out_table_str);
        return true;
    }

    // ok the tables haven't timed out, but have we hit the signal timeout?
    if (timer.elapsed() > (int)(*current).timeoutTune)
    {
        // If we don't have a signal in timeoutTune msec, continue..
        SignalMonitor *sm = GetSignalMonitor();
        if (NULL == sm || sm->HasSignalLock())
            return false;

        emit ServiceScanUpdateText(time_out_sig_str);
        VERBOSE(VB_SIPARSER, LOC + time_out_sig_str);

        return true;
    }
    return false;
}

/** \fn SIScan::HandleActiveScan(void)
 *  \brief Handles the TRANSPORT_LIST SIScan mode.
 */
void SIScan::HandleActiveScan(void)
{
    bool do_post_insertion = waitingForTables;

    if (!HasTimedOut())
        return;
    
    if (0 == nextIt.offset() && nextIt != scanTransports.begin())
    {
        // Stop signal monitor for previous transport
        signalMonitor->Stop();

        if (do_post_insertion)
            HandlePostInsertion();

        transportsScanned++;
        UpdateScanPercentCompleted();
    }

    current = nextIt; // Increment current

    if (current != scanTransports.end())
    {
        ScanTransport(current);

        // Increment nextIt
        nextIt = current;
        ++nextIt;
    }
    else
    {
        emit ServiceScanComplete();
        scanMode = IDLE;
        scanTransports.clear();
        current = nextIt = scanTransports.end();
    }
}

bool SIScan::Tune(const transport_scan_items_it_t transport)
{
    const TransportScanItem &item = *transport;
    const uint freq = item.freq_offset(transport.offset());
    bool ok = false;

#ifdef USING_DVB
    // Tune to multiplex
    if (GetDVBChannel())
    {
        // always wait for rotor to finish
        GetDVBSignalMonitor()->AddFlags(kDVBSigMon_WaitForPos);
        GetDVBSignalMonitor()->SetRotorTarget(1.0f);

        if (item.mplexid > 0)
        {
            ok = GetDVBChannel()->TuneMultiplex(item.mplexid, item.SourceID);
        }
        else
        {
            DVBTuning tuning = item.tuning;
            tuning.params.frequency = freq;
            ok = GetDVBChannel()->Tune(tuning, true, item.SourceID);
        }
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetChannel())
    {
        if (item.mplexid > 0)
            ok = GetChannel()->TuneMultiplex(item.mplexid);
        else 
        {
            const uint freq_vis = freq - 1750000; // to visual carrier
            QString inputname = ChannelUtil::GetInputName(item.SourceID);
            ok = GetChannel()->Tune(freq_vis, inputname,
                                    item.ModulationDB());
        }
    }
#endif // USING_V4L

#ifdef USING_HDHOMERUN
    if (GetHDHRChannel())
    {
        if (item.mplexid > 0)
            ok = GetHDHRChannel()->TuneMultiplex(item.mplexid);
        else 
        {
            QString inputname = ChannelUtil::GetInputName(item.SourceID);
            ok = GetHDHRChannel()->Tune(freq, inputname,
                                        item.ModulationDB());
        }        
    }
#endif // USING_HDHOMERUN

    return ok;
}

void SIScan::ScanTransport(const transport_scan_items_it_t transport)
{
    QString offset_str = (transport.offset()) ?
        QObject::tr(" offset %2").arg(transport.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);
    QString tune_msg_str =
        QObject::tr("Tuning to %1 mplexid(%2)")
        .arg(cur_chan).arg((*current).mplexid);

    const TransportScanItem &item = *transport;

    if (transport.offset() && 
        (item.freq_offset(transport.offset()) == item.freq_offset(0)))
    {
        waitingForTables = false;
        return; // nothing to do
    }

    emit ServiceScanUpdateStatusText(cur_chan);
    VERBOSE(VB_SIPARSER, LOC + tune_msg_str);

    if (!Tune(transport))
    {   // If we did not tune successfully, bail with message
        UpdateScanPercentCompleted();
        VERBOSE(VB_SIPARSER, LOC +
                QString("Failed to tune %1 mplexid(%2) at offset %3")
                .arg(item.FriendlyName).arg(item.mplexid)
                .arg(transport.offset()));
        return;
    }

    // If we have a DTV Signal Monitor, perform table scanner reset
    if (GetDTVSignalMonitor() && GetDTVSignalMonitor()->GetScanStreamData())
    {
        GetDTVSignalMonitor()->GetScanStreamData()->Reset();
        GetDTVSignalMonitor()->SetChannel(-1,-1);
    }

    // Start signal monitor for this channel
    signalMonitor->Start();

    timer.start();
    waitingForTables = true;
}

/** \fn SIScan::StopScanner(void)
 *  \brief Stops the SIScan event loop and the signal monitor,
 *         blocking until both exit.
 */
void SIScan::StopScanner(void)
{
    VERBOSE(VB_SIPARSER, LOC + "Stopping SIScanner");

    threadExit = true;

    if (scanner_thread_running)
        pthread_join(scanner_thread, NULL);

    if (signalMonitor)
        signalMonitor->Stop();
}

/** \fn SIScan::ScanTransports(int,const QString,const QString,const QString)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanMode to TRANSPORT_LIST.
 */
bool SIScan::ScanTransports(int SourceID,
                            const QString std,
                            const QString modulation,
                            const QString country)
{
    QString si_std = (std.lower() != "atsc") ? "dvb" : "atsc";
    QString name("");
    if (scanMode == TRANSPORT_LIST)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

    freq_table_list_t tables =
        get_matching_freq_tables(std, modulation, country);

    VERBOSE(VB_SIPARSER, LOC + QString("Looked up freq table (%1, %2, %3)")
            .arg(std).arg(modulation).arg(country));

    freq_table_list_t::iterator it = tables.begin();
    for (; it != tables.end(); ++it)
    {
        const FrequencyTable &ft = **it;
        int     name_num         = ft.name_offset;
        QString strNameFormat    = ft.name_format;
        uint    freq             = ft.frequencyStart;
        while (freq <= ft.frequencyEnd)
        {
            if (strNameFormat.length() >= 2)
                name = strNameFormat.arg(name_num);

            TransportScanItem item(SourceID, si_std, name, name_num,
                                   freq, ft, signalTimeout);
            scanTransports += item;

            VERBOSE(VB_SIPARSER, LOC + item.toString());

            name_num++;
            freq += ft.frequencyStep;
        }
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

bool SIScan::ScanForChannels(uint sourceid,
                             const QString &std,
                             const QString &cardtype,
                             const DTVChannelList &channels)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    DTVChannelList::const_iterator it = channels.begin();
    for (uint i = 0; it != channels.end(); ++it, i++)
    {
        TransportScanItem item(sourceid, std, QString::number(i),
                               cardtype, *it, signalTimeout);

        scanTransports += item;

        VERBOSE(VB_SIPARSER, LOC + item.toString());
    }

    if (scanTransports.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "ScanForChannels() no transports");
        return false;
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

/** \fn SIScan::ScanTransportsStartingOn(int,const QMap<QString,QString>&)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanMode to TRANSPORT_LIST.
 */
bool SIScan::ScanTransportsStartingOn(int sourceid,
                                      const QMap<QString,QString> &startChan)
{
    QMap<QString,QString>::const_iterator it;

    if (startChan.find("std")        == startChan.end() ||
        startChan.find("modulation") == startChan.end())
    {
        return false;
    }

    QString std    = *startChan.find("std");
    QString mod    = *startChan.find("modulation");
    QString si_std = (std.lower() != "atsc") ? "dvb" : "atsc";
    QString name   = "";
    bool    ok     = false;

    if (scanMode == TRANSPORT_LIST)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

#ifdef USING_DVB
    DVBTuning tuning;
    bzero(&tuning, sizeof(DVBTuning));
    if (std == "dvb" && mod == "ofdm")
    {
        ok = tuning.parseOFDM(
            startChan["frequency"],   startChan["inversion"],
            startChan["bandwidth"],   startChan["coderate_hp"],
            startChan["coderate_lp"], startChan["constellation"],
            startChan["trans_mode"],  startChan["guard_interval"],
            startChan["hierarchy"]);
    }
    if (std == "dvb" && mod == "qpsk")
    {
        ok = tuning.parseQPSK(
            startChan["frequency"],   startChan["inversion"],
            startChan["symbolrate"],  startChan["fec"],
            startChan["polarity"]);
    }
    else if (std == "dvb" && mod.left(3) == "qam")
    {
        ok = tuning.parseQAM(
            startChan["frequency"],   startChan["inversion"],
            startChan["symbolrate"],  startChan["fec"],
            startChan["modulation"]);
    }

    if (ok)
    {
        scanTransports += TransportScanItem(
            sourceid, si_std, tr("Frequency %1").arg(startChan["frequency"]),
            tuning, signalTimeout);
    }
#endif // USING_DVB

    if (!ok)
        return false;

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

bool SIScan::ScanTransport(int mplexid)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    MSqlQuery query(MSqlQuery::InitCon());
    // Run DB query to get transports on sourceid SourceID
    // connected to this card
    QString theQuery = QString(
        "SELECT sourceid, mplexid, sistandard, transportid "
        "FROM dtv_multiplex WHERE mplexid = %2").arg(mplexid);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Get Transports for SourceID", query);
        return false;
    }

    if (query.size() <= 0)
    {
        VERBOSE(VB_SIPARSER, LOC + "Unable to find transport to scan.");
        return false;
    }

    while (query.next())
    {
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);
        
        VERBOSE(VB_SIPARSER, LOC + "Adding " + fn);

        TransportScanItem item(sourceid, std, fn, mplexid, signalTimeout);
        scanTransports += item;
    }

    timer.start();
    waitingForTables  = false;

    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
}

/** \fn SIScan::OptimizeNITFrequencies(NetworkInformationTable *nit)
 *  \brief Checks that we can tune to a transports described in NIT.
 *
 *   For each transport freqency list entry, we try to tune to it
 *   if it works use it as the frequency.
 *
 *  \todo We should probably try and work out the strongest signal.
 */
void SIScan::OptimizeNITFrequencies(NetworkInformationTable *nit)
{
    (void) nit;
#if 0
    dvb_channel_t chan_opts;
    DVBTuning    &tuning = chan_opts.tuning;

    QValueList<TransportObject>::iterator it;
    for (it = NIT.Transport.begin() ; it != NIT.Transport.end() ; ++it )
    {
        const TransportObject& transport = *it;
        // Parse the default transport object
        bool ok = false;
        if (transport.Type == "DVB-T")
            ok = tuning.parseOFDM(transport);
        else if (transport.Type == "DVB-C")
            ok = tuning.parseQAM(transport);

        // Find the best frequency from those listed in transport object.
        if (ok)
        {
            (*it).Frequency = scan_for_best_freq(
                transport, GetDVBChannel(), chan_opts);
        }
    }
#endif // USING_DVB
}

/** \fn SIScan::CheckImportedList(const DTVChannelInfoList&,uint,QString&,QString&,QString&)
 *  \brief If we as scanning a dvb-utils import verify channel is in list..
 */
bool SIScan::CheckImportedList(const DTVChannelInfoList &channels,
                               uint mpeg_program_num,
                               QString &service_name,
                               QString &callsign,
                               QString &common_status_info)
{
    if (channels.empty())
        return true;

    bool found = false;
    for (uint i = 0; i < channels.size(); i++)
    {
        VERBOSE(VB_IMPORTANT,
                QString("comparing %1 %2 against %3 %4")
                .arg(channels[i].serviceid).arg(channels[i].name)
                .arg(mpeg_program_num).arg(common_status_info));

        if (channels[i].serviceid == mpeg_program_num)
        {
            found = true;
            if (!channels[i].name.isEmpty())
            {
                service_name = QDeepCopy<QString>(channels[i].name);
                callsign     = QDeepCopy<QString>(channels[i].name);
            }
        }
    }

    if (found)
    {
        common_status_info += QString(" %1 %2")
            .arg(tr("as")).arg(service_name);
    }
    else
    {
        emit ServiceScanUpdateText(
            tr("Skipping %1, not in imported channel map")
            .arg(common_status_info));
    }

    return found;
}

// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////

void SIScan::UpdatePMTinDB(
    int db_source_id,
    int db_mplexid, const QString &friendlyName, int freqid,
    int pmt_indx, const ProgramMapTable *pmt,
    const DTVChannelInfoList &channels, bool /*force_update*/)
{
    // See if service already in database based on program number
    int chanid = ChannelUtil::GetChanID(
        db_mplexid, -1, -1, -1, pmt->ProgramNumber());

    QString chan_num = ChannelUtil::GetChanNum(chanid);
    if (chan_num.isEmpty() || renameChannels)
    {
        chan_num = QString("%1#%2")
            .arg((freqid) ? freqid : db_mplexid).arg(pmt_indx);
    }
        
    QString callsign = ChannelUtil::GetCallsign(chanid);
    QString service_name = ChannelUtil::GetServiceName(chanid);

    if (callsign.isEmpty())
    {
        callsign = QObject::tr("UNKNOWN%1", "Synthesized callsign")
            .arg(chan_num);
    }
    else if (service_name.isEmpty())
        service_name = callsign; // only do this for real callsigns

    QString common_status_info = tr("%1%2%3 on %4 (%5)")
        .arg(service_name)
        .arg(service_name.isEmpty() ? "" : QString(" %1 ").arg(tr("as")))
        .arg(chan_num)
        .arg(friendlyName).arg(freqid);

    if (!CheckImportedList(channels, pmt->ProgramNumber(),
                           service_name, chan_num, common_status_info))
    {
        return;
    }

    if (chanid < 0)
    {   // The service is not in database, add it
        emit ServiceScanUpdateText(
            tr("Adding %1").arg(common_status_info));
        chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
        ChannelUtil::CreateChannel(
            db_mplexid, db_source_id, chanid,
            callsign,
            service_name,
            chan_num,
            pmt->ProgramNumber(),
            0, 0,
            false, false, false, QString::number(freqid));
    }
    else
    {   // The service is in database, update it
        emit ServiceScanUpdateText(
            tr("Updating %1").arg(common_status_info));
        ChannelUtil::UpdateChannel(
            db_mplexid, db_source_id, chanid,
            callsign,
            service_name,
            chan_num,
            pmt->ProgramNumber(),
            0, 0,
            false, false, false, QString::number(freqid));
    }
}

void SIScan::UpdatePATinDB(
    int db_mplexid, const QString &friendlyName, int freqid,
    const ProgramAssociationTable *pat, const pmt_map_t &pmt_map,
    const DTVChannelInfoList &channels, bool force_update)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("UpdatePATinDB(): tsid: 0x%1  mplex: %2")
            .arg(pat->TransportStreamID(),0,16).arg(db_mplexid));

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);

    for (uint i = 0; i < pat->ProgramCount(); i++)
    {
        pmt_map_t::const_iterator it = pmt_map.find(pat->ProgramNumber(i));
        if (it == pmt_map.end())
        {
            VERBOSE(VB_SIPARSER,
                   QString("UpdatePATinDB(): PMT for Program #%1 is missing")
                   .arg(pat->ProgramNumber(i)));
            continue;
        }
        pmt_vec_t::const_iterator vit = (*it).begin();
        for (; vit != (*it).end(); ++vit)
        {
            VERBOSE(VB_SIPARSER,
                    QString("UpdatePATinDB(): Prog %1 PID %2: PMT @")
                    .arg(pat->ProgramNumber(i))
                    .arg(pat->ProgramPID(i)) << *vit);

            // ignore all services without PMT, and
            // ignore services we have decided to ignore
            if (!(*vit))
                continue;
            else if ((*vit)->StreamCount() <= 0)
                continue;
            else if (ignoreAudioOnlyServices && (*vit)->IsStillPicture())
                continue;
            else if (ignoreEncryptedServices && (*vit)->IsEncrypted())
                continue;

            UpdatePMTinDB(db_source_id, db_mplexid, friendlyName, freqid,
                          i, *vit, channels, force_update);
        }
    }    
}

/** \fn SIScan::UpdateVCTinDB(int,const QString&,int,const VirtualChannelTable*,bool)
 */
void SIScan::UpdateVCTinDB(int db_mplexid,
                           const QString &friendlyName, int freqid,
                           const VirtualChannelTable *vct,
                           const DTVChannelInfoList &channels,
                           bool force_update)
{
    (void) force_update;

    VERBOSE(VB_SIPARSER, LOC +
            QString("UpdateVCTinDB(): tsid: 0x%1  mplex: %1")
            .arg(vct->TransportStreamID(),0,16).arg(db_mplexid));

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);

    for (uint i = 0; i < vct->ChannelCount(); i++)
    {
        if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
            vct->ServiceType(i)    == 0x01 /* Analog TV */)
        {
            continue;
        }

        if (vct->ServiceType(i) == 0x04 && ignoreDataServices)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Data Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;            
        }

        if (vct->ServiceType(i) == 0x03 && ignoreAudioOnlyServices)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Radio Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;            
        }

        if (vct->IsAccessControlled(i) && ignoreEncryptedServices)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Encrypted Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;
        }

        // See if service already in database
        int chanid = ChannelUtil::GetChanID(
            db_mplexid, vct->ChannelTransportStreamID(i),
            vct->MajorChannel(i), vct->MinorChannel(i),
            vct->ProgramNumber(i));

        QString chan_num = ChannelUtil::GetChanNum(chanid);
        if (chan_num.isEmpty() || renameChannels)
        {
            chan_num = channelFormat
                .arg(vct->MajorChannel(i))
                .arg(vct->MinorChannel(i));
        }

        // try to find an extended channel name, fallback to short name.
        QString longName = vct->GetExtendedChannelName(i);
        if (longName.isEmpty())
            longName = vct->ShortChannelName(i);

        QString common_status_info = tr("%1 %2-%3 as %4 on %5 (%6)")
            .arg(vct->ShortChannelName(i))
            .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i))
            .arg(chan_num).arg(friendlyName).arg(freqid);

        bool use_eit = !vct->IsHidden(i) ||
            (vct->IsHidden(i) && !vct->IsHiddenInGuide(i));

        QString callsign = vct->ShortChannelName(i);

        if (!CheckImportedList(channels, vct->ProgramNumber(i),
                               longName, callsign, common_status_info))
        {
            continue;
        }

        QString msg = "";
        if (chanid < 0)
        {   // The service is not in database, add it
            msg = tr("Adding %1").arg(common_status_info);
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            if (chanid > 0)
            {
                ChannelUtil::CreateChannel(
                    db_mplexid,
                    db_source_id,
                    chanid,
                    callsign,
                    longName,
                    chan_num,
                    vct->ProgramNumber(i),
                    vct->MajorChannel(i), vct->MinorChannel(i),
                    use_eit,
                    vct->IsHidden(i), vct->IsHiddenInGuide(i),
                    QString::number(freqid));
            }
        }
        else
        {   // The service is in database, update it
            msg = tr("Updating %1").arg(common_status_info);
            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                callsign,
                longName,
                chan_num,
                vct->ProgramNumber(i),
                vct->MajorChannel(i), vct->MinorChannel(i),
                use_eit,
                vct->IsHidden(i), vct->IsHiddenInGuide(i),
                QString::number(freqid));
        }
        emit ServiceScanUpdateText(msg);
        VERBOSE(VB_SIPARSER, msg);
    }
}

/** \fn SIScan::UpdateSDTinDB(int,const ServiceDescriptionTable*,bool)
 *  \brief Inserts channels from service description table.
 */
void SIScan::UpdateSDTinDB(int /*mplexid*/, const ServiceDescriptionTable *sdt,
                           const DTVChannelInfoList &channels,
                           bool force_update)
{
    if (!sdt->ServiceCount())
        return;

    int db_mplexid = ChannelUtil::GetMplexID(
        sourceID, sdt->TSID(), sdt->OriginalNetworkID());

    // HACK beg -- special exception for this network (dbver == "1067")
    bool force_guide_present = (sdt->OriginalNetworkID() == 70);
    // HACK end -- special exception for this network

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "SDT: Error determing what transport this "
                "service table is associated with so failing");
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
        return;
    }

    int db_source_id = ChannelUtil::GetSourceID(db_mplexid);

    /* This will be fixed post .17 to be more elegant */
    bool upToDate = (ChannelUtil::GetServiceVersion(db_mplexid) ==
                     (int)sdt->Version());
    if (upToDate && !force_update)
    {
        emit ServiceScanUpdateText("Channels up to date");
        return;
    }
    if (!upToDate)
        ChannelUtil::SetServiceVersion(db_mplexid, sdt->Version());

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        // Figure out best service name...
        ServiceDescriptor *desc = sdt->GetServiceDescriptor(i);
        QString service_name = "";
        if (desc)
            service_name = desc->ServiceName();

        if (service_name.stripWhiteSpace().isEmpty())
            service_name = QString("%1 %2")
                .arg(sdt->ServiceID(i)).arg(db_mplexid);

        // Figure out best channel number
        QString chan_num = QString::number(sdt->ServiceID(i));
        bool have_uk_chan_num =
            dvbChanNums.find(sdt->ServiceID(i)) != dvbChanNums.end();
        if (have_uk_chan_num)
            chan_num = QString::number(dvbChanNums[sdt->ServiceID(i)]);

        // Skip to next if this is a service we don't care for
        if (desc && desc->IsDigitalAudio() && ignoreAudioOnlyServices)
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - Radio Service").arg(service_name));
            continue;
        }
        else if (desc && !desc->IsDTV() && !desc->IsDigitalAudio() &&
                 ignoreDataServices)
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - not a Television or "
                                          "Radio Service").arg(service_name));
            continue;
        }
        else if (ignoreEncryptedServices && sdt->IsEncrypted(i))
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - Encrypted Service")
                .arg(service_name));
            continue;
        }

        QString common_status_info = service_name;

        if (!CheckImportedList(channels, sdt->ServiceID(i),
                               service_name, service_name, common_status_info))
        {
            continue;
        }

        // See if service already in database based on service ID
        int chanid = ChannelUtil::GetChanID(db_mplexid, -1, -1, -1,
                                            sdt->ServiceID(i));

        if (chanid < 0)
        {   // The service is not in database, add it
            emit ServiceScanUpdateText(tr("Adding %1").arg(service_name));
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            if (chanid > 0)
            {
                ChannelUtil::CreateChannel(
                    db_mplexid, db_source_id, chanid,
                    service_name,
                    service_name,
                    chan_num,
                    sdt->ServiceID(i),
                    0, 0,
                    sdt->HasEITSchedule(i) ||
                    sdt->HasEITPresentFollowing(i) ||
                    force_guide_present,
                    false, false, QString::null);
            }
        }
        else if (force_update || (desc && have_uk_chan_num))
        {   // The service is in database & we have good info, update it
            emit ServiceScanUpdateText(tr("Updating %1").arg(service_name));

            if (!renameChannels)
                chan_num = ChannelUtil::GetChanNum(chanid);

            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                service_name,
                service_name,
                chan_num,
                sdt->ServiceID(i),
                0, 0, 
                sdt->HasEITSchedule(i) ||
                sdt->HasEITPresentFollowing(i) ||
                force_guide_present,
                false, false, QString::null);
        }
        else
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - already in DB, and "
                   "we don't have better data.")
                .arg(service_name));
        }

        if (desc)
            delete desc;
    }
}

// FindBestMplexFreq()
//  - Examines the freq in the DB against the curent tuning frequency and
//    it's offset frequencies. If the frequency in the DB is any of the
//    tuning frequencies or offsets then use the DB frequency.
uint SIScan::FindBestMplexFreq(const uint tuning_freq,
    const transport_scan_items_it_t transport, const uint sourceid,
    const uint transportid, const uint networkid)
{
    uint64_t    db_freq;
    QString     tmp_modulation;
    uint        tmp_transportid, tmp_networkid;
    int         mplexid;

    mplexid = ChannelUtil::GetMplexID(sourceid, transportid, networkid);
    if (mplexid < 0)
        return tuning_freq;

    if (!ChannelUtil::GetTuningParams((uint)mplexid, tmp_modulation,
        db_freq, tmp_transportid, tmp_networkid))
    {
        return tuning_freq;
    }

    for (uint i = 0; i < (*transport).offset_cnt(); i++)
    {
        if ((uint)db_freq == (*transport).freq_offset(i))
            return (uint)db_freq;
    }
    return tuning_freq;
}

int SIScan::InsertMultiplex(const transport_scan_items_it_t transport)
{
    int mplexid = -1;

#ifdef USING_DVB
    if (GetDVBChannel())
    {
        DVBSignalMonitor *sm = GetDVBSignalMonitor();
        // Try to read the actual values back from the card
        DVBTuning tuning;
        if (!GetDVBChannel()->GetTuningParams(tuning))
            tuning = (*transport).tuning;

        uint bestFrequency = FindBestMplexFreq(
            tuning.Frequency(),
            transport,
            (*transport).SourceID,
            sm->GetDetectedTransportID(),
            sm->GetDetectedNetworkID());

        // Write the best info we have to the DB
        if (FE_OFDM == GetDVBChannel()->GetCardType())
            mplexid = ChannelUtil::CreateMultiplex(
                (*transport).SourceID,      (*transport).standard,
                bestFrequency,              tuning.ModulationDB(),
                sm->GetDetectedTransportID(),
                sm->GetDetectedNetworkID(),
                -1 /* symbol rate */,       tuning.BandwidthChar(),
                -1 /* polarity */,          tuning.InversionChar(),
                tuning.TransmissionModeChar(),
                QString::null /*inner FEC*/,tuning.ConstellationDB(),
                tuning.HierarchyChar(),     tuning.HPCodeRateString(),
                tuning.LPCodeRateString(),  tuning.GuardIntervalString());
        else if (FE_QPSK == GetDVBChannel()->GetCardType()) 
            mplexid = ChannelUtil::CreateMultiplex( 
                (*transport).SourceID,      (*transport).standard, 
                (*transport).tuning.Frequency(),
                (*transport).ModulationDB(), 
                sm->GetDetectedTransportID(), 
                sm->GetDetectedNetworkID(), 
                tuning.QPSKSymbolRate(),    -1, 
                tuning.PolarityChar(),      tuning.InversionChar(), 
                -1, 
                tuning.QPSKInnerFECString(),QString::null, 
                -1,                         QString::null, 
                QString::null,              QString::null); 
        else if (FE_QAM == GetDVBChannel()->GetCardType())
            mplexid = ChannelUtil::CreateMultiplex(
                (*transport).SourceID,      (*transport).standard,
                (*transport).tuning.Frequency(),
                tuning.ModulationString(),
                sm->GetDetectedTransportID(),
                sm->GetDetectedNetworkID(),
                tuning.QAMSymbolRate(),     -1,
                -1 /* polarity */,          tuning.InversionChar(),
                -1 /* transmission mode */,
                tuning.QAMInnerFECString(), QString::null,
                -1,                         QString::null,
                QString::null,              QString::null);
        else
            mplexid = ChannelUtil::CreateMultiplex(
                (*transport).SourceID,      (*transport).standard,
                tuning.Frequency(),         (*transport).ModulationDB(),
                sm->GetDetectedTransportID(),
                sm->GetDetectedNetworkID());
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetChannel())
    {
        const uint freq = (*transport).freq_offset(transport.offset());
        const uint freq_vis = freq - 1750000; // convert to visual carrier
        mplexid = ChannelUtil::CreateMultiplex(
            (*transport).SourceID, (*transport).standard,
            freq_vis, (*transport).ModulationDB());
    }
#endif // USING_V4L

#ifdef USING_HDHOMERUN
    if (GetHDHRChannel())
    {
        const uint freq = (*transport).freq_offset(transport.offset());
        mplexid = ChannelUtil::CreateMultiplex(
            (*transport).SourceID, (*transport).standard,
            freq, (*transport).ModulationDB());
    }
#endif

    return mplexid;
}
