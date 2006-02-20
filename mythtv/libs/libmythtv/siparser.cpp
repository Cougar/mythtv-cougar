// Std C++ headers
#include <algorithm>

// Qt headers
#include <qdatetime.h>
#include <qtextcodec.h>
#include <qregexp.h>
#include <qptrvector.h>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "util.h"
#include "iso639.h"
#include "atsc_huffman.h"
#include "pespacket.h"
#include "atsctables.h"
#include "mpegdescriptors.h"

// MythTV DVB headers
#include "siparser.h"
#include "dvbtypes.h"

//QMap<uint,uint> SIParser::sourceid_to_channel;

/// \TODO Remove this bcd2int conversion if possible to clean up the date
/// functions since this is used by the dvbdatetime function
#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

// Set EIT_DEBUG_SID to a valid serviceid to enable EIT debugging
// #define EIT_DEBUG_SID 1602

#define LOC QString("SIParser: ")
#define LOC_ERR QString("SIParser, Error: ")

static uint huffman2_to_string(const unsigned char *compressed,
                               uint length, uint table,
                               QString &decompressed);

/** \class SIParser
 *  This class parses DVB SI and ATSC PSIP tables.
 *
 *  This class is generalized so it can be used with DVB Cards with a simple
 *  sct filter, sending the read data into this class, and the PCHDTV card by
 *  filtering the TS packets through another class to convert it into tables,
 *  and passing this data into this class as well.
 *
 *  Both DVB and ATSC are combined into this class since ATSC over DVB is 
 *  present in some place.  (One example is PBS on AMC3 in North America).
 *  Argentenia has also has announced their Digital TV Standard will be 
 *  ATSC over DVB-T
 *
 *  Implementation of OpenCable or other MPEG-TS based standards (DirecTV?)
 *  is also possible with this class if their specs are ever known.
 *
 */
SIParser::SIParser(const char *name) :
    QObject(NULL, name),
    PAT_ready(false),               PMT_ready(false),
    // Common Variables
    SIStandard(SI_STANDARD_AUTO),
    CurrentTransport(0),            RequestedServiceID(0),
    RequestedTransportID(0),        NITPID(0),
    // Mutex Locks
    pmap_lock(false),
    // State variables
    ThreadRunning(false),           exitParserThread(false),
    ParserInReset(false),           standardChange(false),
    PrivateTypesLoaded(false)
{
    /* Set the PrivateTypes to default values */
    PrivateTypes.reset();

    /* Initialize the Table Handlers */
    Table[PAT]      = new PATHandler();
    Table[PMT]      = new PMTHandler();
    Table[MGT]      = new MGTHandler();
    Table[STT]      = new STTHandler();
    Table[SERVICES] = new ServiceHandler();
    Table[NETWORK]  = new NetworkHandler();
#ifdef USING_DVB_EIT
    Table[EVENTS]   = new EventHandler();
#endif //USING_DVB_EIT

    InitializeCategories();

    Reset();

    // Get a list of wanted languages and set up their priorities
    // (Lowest number wins)
    QStringList langPref = iso639_get_language_list();
    QStringList::Iterator plit;
    uint prio = 1;
    for (plit = langPref.begin(); plit != langPref.end(); ++plit)
    {
        if (!(*plit).isEmpty())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Added initial preferred language '%1' "
                            "with priority %2").arg(*plit).arg(prio));
            LanguagePriority[*plit] = prio++;
        }
    }
}

SIParser::~SIParser()
{
    pmap_lock.lock();
    for (uint i = 0; i < NumHandlers; ++i)
    {
        if (Table[i])
            delete Table[i];
    }
    pmap_lock.unlock();
}

void SIParser::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    PrintDescriptorStatistics();
    QObject::deleteLater();
}

/* Resets all trackers, and closes all section filters */
void SIParser::Reset()
{
    ParserInReset = true;

    // Close all pids that are open

    VERBOSE(VB_SIPARSER, LOC + "About to do a reset");

    PrintDescriptorStatistics();

    Table[MGT]->Reset();

    VERBOSE(VB_SIPARSER, LOC + "Closing all PIDs");
    DelAllPids();

    PrivateTypesLoaded = false;
    PrivateTypes.reset();

    VERBOSE(VB_SIPARSER, LOC + "Resetting all Table Handlers");
    pmap_lock.lock();

    for (int x = 0; x < NumHandlers ; x++)
        Table[x]->Reset();

    pmap_lock.unlock();

    VERBOSE(VB_SIPARSER, LOC + "SIParser Reset due to channel change");
}

void SIParser::CheckTrackers()
{

    uint16_t pid;
    uint8_t filter,mask;

    pmap_lock.lock();

    /* Check Dependencies and update if necessary */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->Complete())
        {
            VERBOSE(VB_SIPARSER, LOC + QString("Table[%1]->Complete() == true")
                    .arg((tabletypes) x));
            for (int y = 0 ; y < NumHandlers ; y++)
                Table[y]->DependencyMet((tabletypes) x);
// TODO: Emit completion here for tables to siscan
        }
    }

    /* Handle opening any PIDs in this loop only */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->RequirePIDs())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Table[%1]->RequirePIDs() == true")
                    .arg((tabletypes) x));
            while (Table[x]->GetPIDs(pid,filter,mask))
            {
                int bufferFactor = 10;
#ifdef USING_DVB_EIT
                if ((SIStandard == SI_STANDARD_DVB) && (x == EVENTS))
                    bufferFactor = 1000;
#endif // USING_DVB_EIT
                AddPid(pid, mask, filter, true, bufferFactor);
            }
        }
    }

    uint16_t key0 = 0,key1 = 0;

    /* See if any new objects require an emit */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->EmitRequired())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Table[%1]->EmitRequired() == true")
                    .arg((tabletypes) x));
            switch (x)
            {
                case PMT:
                    while (Table[PMT]->GetEmitID(key0, key1))
                    {
                        PMTHandler *hdl = (PMTHandler*) Table[PMT];
                        const PMTObject &pmt = hdl->pmt[key0];
                        emit UpdatePMT(&pmt);
                    }
                    break;
#ifdef USING_DVB_EIT
                case EVENTS:
                    while (Table[EVENTS]->GetEmitID(key0,key1))
                    {
                        EventHandler *hdl = (EventHandler*) Table[EVENTS];
                        emit EventsReady(&(hdl->Events[key0]));
                    }
                    break;
#endif // USING_DVB_EIT
                case NETWORK:
                    while(Table[NETWORK]->GetEmitID(key0,key1))
                        emit FindTransportsComplete();
                    break;
                default:
                    break;
            }
        }
    }

    pmap_lock.unlock();


}

void SIParser::LoadPrivateTypes(uint16_t NetworkID)
{
    QString STD;

    switch (SIStandard)
    {
        case SI_STANDARD_DVB:    STD = "dvb";
                                 break;
        case SI_STANDARD_ATSC:   STD = "atsc";
                                 break;
        /* If you don't know the SI Standard yet you need to bail out */
        default:
                                 return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery =
        QString("SELECT private_type, private_value "
                "FROM dtv_privatetypes "
                "WHERE networkid = %1 AND sitype = '%2'")
        .arg(NetworkID).arg(STD);

    query.prepare(theQuery);

    if (!query.exec())
        MythContext::DBError("Loading Private Types for SIParser", query);

    if (!query.isActive())
        MythContext::DBError("Loading Private Types for SIParser", query);

    if (query.size() > 0)
    {
        query.next();
        for (int x = 0 ; x < query.size() ; x++)
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Private Type %1 = %2 defined for NetworkID %3")
                    .arg(query.value(0).toString())
                    .arg(query.value(1).toString())
                    .arg(NetworkID) );

            if (QString(query.value(0).toString()) == "sdt_mapping")
            {
                PrivateTypes.SDTMapping = true;
                VERBOSE(VB_SIPARSER, LOC +
                        "SDT Mapping Incorrect for this Service Fixup Loaded");
            }
            if (QString(query.value(0).toString()) == "channel_numbers")
            {
                PrivateTypes.ChannelNumbers = query.value(1).toInt();
                VERBOSE(VB_SIPARSER, LOC +
                        QString("ChannelNumbers Present using Descriptor %1")
                        .arg(PrivateTypes.ChannelNumbers));
            }
            if (QString(query.value(0).toString()) == "force_guide_present")
            {
                if (query.value(1).toString() == "yes")
                {
                    PrivateTypes.ForceGuidePresent = true;
                    VERBOSE(VB_SIPARSER, LOC + "Forcing Guide Present");
                }
            }
            if (QString(query.value(0).toString()) == "guide_fixup")
            {
                PrivateTypes.EITFixUp = query.value(1).toInt();
                VERBOSE(VB_SIPARSER, LOC +
                        QString("Using Guide Fixup Scheme #%1")
                        .arg(PrivateTypes.EITFixUp));
            }
            if (QString(query.value(0).toString()) == "guide_ranges")
            {
                PrivateTypes.CustomGuideRanges = true;
                QStringList temp =
                    QStringList::split(",", query.value(1).toString());
                PrivateTypes.CurrentTransportTableMin = temp[0].toInt();
                PrivateTypes.CurrentTransportTableMax = temp[1].toInt();
                PrivateTypes.OtherTransportTableMin = temp[2].toInt();
                PrivateTypes.OtherTransportTableMax = temp[3].toInt();
                
                VERBOSE(VB_SIPARSER, LOC +
                        QString("Using Guide Custom Range; "
                                "CurrentTransport: %1-%2, "
                                "OtherTransport: %3-%4")
                        .arg(PrivateTypes.CurrentTransportTableMin,2,16)
                        .arg(PrivateTypes.CurrentTransportTableMax,2,16)
                        .arg(PrivateTypes.OtherTransportTableMin,2,16)
                        .arg(PrivateTypes.OtherTransportTableMax,2,16));
            }
            if (QString(query.value(0).toString()) == "tv_types")
            {
                PrivateTypes.TVServiceTypes.clear();
                QStringList temp =
                    QStringList::split(",", query.value(1).toString());
                QStringList::Iterator i;
                for (i = temp.begin() ; i != temp.end() ; i++)
                {
                    PrivateTypes.TVServiceTypes[(*i).toInt()] = 1;
                    VERBOSE(VB_SIPARSER, LOC +
                            QString("Added TV Type %1").arg((*i).toInt()));
                }
            }
#ifdef USING_DVB_EIT
            if (QString(query.value(0).toString()) == "parse_subtitle_list")
            {
                eitfixup.clearSubtitleServiceIDs();
                QStringList temp =
                    QStringList::split(",", query.value(1).toString());
                for (QStringList::Iterator i = temp.begin();i!=temp.end();i++)
                {
                    eitfixup.addSubtitleServiceID((*i).toUInt());
                    VERBOSE(VB_SIPARSER, LOC + 
                            QString("Added ServiceID %1 to list of "
                                    "channels to parse subtitle from")
                            .arg((*i).toInt()));
                }
            }
#endif //USING_DVB_EIT
            query.next();
        }
    }
    else
        VERBOSE(VB_SIPARSER, LOC +
                QString("No Private Types defined for NetworkID %1")
                .arg(NetworkID));

    PrivateTypesLoaded = true;
}

bool SIParser::GetTransportObject(NITObject &NIT)
{
    pmap_lock.lock();
    NIT = ((NetworkHandler*) Table[NETWORK])->NITList;
    pmap_lock.unlock();
    return true;
}

bool SIParser::GetServiceObject(QMap_SDTObject &SDT)
{

    pmap_lock.lock();
    SDT = ((ServiceHandler*) Table[SERVICES])->Services[0];
    pmap_lock.unlock();
    return true;
}

void SIParser::AddPid(uint16_t pid, uint8_t mask, uint8_t filter,
                      bool CheckCRC, int bufferFactor)
{
    (void) pid;
    (void) mask;
    (void) filter;
    (void) CheckCRC;
    (void) bufferFactor;
    VERBOSE(VB_SIPARSER, LOC + "AddPid() does nothing");
}

void SIParser::DelPid(int pid)
{
    (void) pid;
    VERBOSE(VB_SIPARSER, LOC + "DelPid() does nothing");
}

void SIParser::DelAllPids()
{
    VERBOSE(VB_SIPARSER, LOC + "DelAllPids does nothing");
}

/** \fn SIParser::FillPMap(SISTANDARD _SIStandard)
 *  \brief Adds basic PID's corresponding to standard to the list
 *         of PIDs we are listening to.
 *
 *   For ATSC this adds the PAT (0) and MGT (0x1ffb) pids.
 *
 *   For DVB this adds the PAT(0), SDT (0x11), STT (?), and
 *       NIT (0x10) pids.
 *
 *   Note: This actually adds all of the above so as to simplify channel
 *         scanning, but this may change as this can break ATSC.
 */
bool SIParser::FillPMap(SISTANDARD _SIStandard)
{
    VERBOSE(VB_SIPARSER, QString("FillPMap(SIS %1)")
            .arg((SI_STANDARD_ATSC == _SIStandard) ? "atsc":"dvb"));

    pmap_lock.lock();
    VERBOSE(VB_SIPARSER, LOC + "Requesting PAT");

    /* By default open only the PID for PAT */
    Table[PAT]->Request(0);
    Table[SERVICES]->Request(0);
    Table[MGT]->Request(0);
    Table[STT]->Request(0);
    Table[NETWORK]->Request(0);

    for (int x = 0 ; x < NumHandlers ; x++)
        Table[x]->SetSIStandard(_SIStandard);

    SIStandard = _SIStandard;

    pmap_lock.unlock();

    return true;
}

/** \fn SIParser::FillPMap(const QString&)
 *  \brief Adds basic PID's corresponding to standard to the list
 *         of PIDs we are listening to.
 *
 *   This is a convenience function that calls SIParser::FillPMap(SISTANDARD)
 */
bool SIParser::FillPMap(const QString &si_std)
{
    VERBOSE(VB_SIPARSER, QString("FillPMap(str %1)").arg(si_std));
    bool is_atsc = si_std.lower() == "atsc";
    return FillPMap((is_atsc) ? SI_STANDARD_ATSC : SI_STANDARD_DVB);
}

/** \fn SIParser::AddPMT(uint16_t ServiceID)
 *  \brief Adds the pid associated with program number 'ServiceID' to the
 *         listening list if this is an ATSC stream and adds the pid associated
 *         with service with service id 'ServiceID' to the listending list if
 *         this is a DVB stream.
 *
 *   If the table standard is ATSC then this adds the pid of the PMT listed
 *   as program number 'ServiceID' in the PAT to the list of pids we want.
 *
 *   If the table standard is DVB then this looks up the program number
 *   of the service listed with the 'ServiceID' in the SDT, then it looks
 *   up that program number in the PAT table and adds the associated pid
 *   to the list of pids we want.
 *
 *   Further the pid is identified as carrying a PMT, so a UpdatePMT Qt
 *   signal is emitted when a table is seen on this pid. Normally this
 *   signal is connected to the DVBChannel class, which relays the update
 *   to the DVBCam class which handles decrypting encrypted services.
 *
 *  \param ServiceID Either the the MPEG Program Number or the DVB Service ID
 *  \return true on success, false on failure.
 */
bool SIParser::AddPMT(uint16_t ServiceID)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Adding PMT program number #%1 to the request list")
            .arg(ServiceID));

    pmap_lock.lock();
    Table[PMT]->RequestEmit(ServiceID);
    pmap_lock.unlock();

    return true;
}

/** \fn SIParser::ReinitSIParser(const QString&, uint)
 *  \brief Convenience function that calls FillPMap(SISTANDARD) and
 *         AddPMT(uint)
 */
bool SIParser::ReinitSIParser(const QString &si_std, uint service_id)
{
    VERBOSE(VB_SIPARSER, LOC + QString("ReinitSIParser(std %1, %2 #%3)")
             .arg(si_std)
             .arg((si_std.lower() == "atsc") ? "program" : "service")
             .arg(service_id));

    return FillPMap(si_std) & AddPMT(service_id);
}


bool SIParser::FindTransports()
{
    Table[NETWORK]->RequestEmit(0);
    return true;
}

bool SIParser::FindServices()
{
    Table[SERVICES]->RequestEmit(0);
    return true;
}

/*------------------------------------------------------------------------
 *   COMMON PARSER CODE
 *------------------------------------------------------------------------*/

void SIParser::ParseTable(uint8_t *buffer, int size, uint16_t pid)
{
    pmap_lock.lock();

#ifndef USING_DVB_EIT
    (void) pid;
#endif

    if (!(buffer[1] & 0x80))
    {
        VERBOSE(VB_SIPARSER, LOC + 
                QString("SECTION_SYNTAX_INDICATOR = 0 - Discarding Table (%1)")
                .arg(buffer[0],2,16));
        pmap_lock.unlock();
        return;
    }

    tablehead_t head = ParseTableHead(buffer,size);

    /* Parse Common Tables (PAT/CAT/PMT) regardless of SI Standard */
    switch(head.table_id)
    {
        case 0x00:  ParsePAT(pid, &head, &buffer[8], size-8);
                    break;

        case 0x01:  ParseCAT(pid, &head, &buffer[8], size-8);
                    break;

        case 0x02:  ParsePMT(pid, &head, &buffer[8], size-8);
                    break;

    }

    /* In Detection mode determine what SIStandard you are using */
    if (SIStandard == SI_STANDARD_AUTO)
    {
        switch(head.table_id)
        {
        case 0x46:
        case 0x42:  
            SIStandard = SI_STANDARD_DVB;
            VERBOSE(VB_SIPARSER, LOC + "SI Standard Detected: DVB");
            standardChange = true;
        break;
        case 0xC7:  
            SIStandard = SI_STANDARD_ATSC;
            VERBOSE(VB_SIPARSER, LOC + "SI Standard Detected: ATSC");
            standardChange = true;
        break;
        }
    }

    /* Parse DVB Specific Tables Here */
    if (SIStandard == SI_STANDARD_DVB)
    {
        switch(head.table_id)
        {
        case 0x40:
            /* Network Information Table(s) */
            ParseNIT(pid, &head, &buffer[8], size-8);
            break;
        case 0x42: /* Service Table(s) */
        case 0x46:  
            ParseSDT(pid, &head, &buffer[8], size-8);
            break;
#ifdef USING_DVB_EIT
        case 0x4E ... 0x4F:
            /* Standard Now/Next Event Information Table(s) */
        case 0x50 ... 0x6F:
            /* Standard Future Event Information Table(s) */
            ParseDVBEIT(pid, &head, &buffer[8], size-8);
            break;
#endif
        }
    }

    if (SIStandard == SI_STANDARD_ATSC)
    {
        switch (head.table_id)
        {
            case TableID::MGT:
                ParseMGT(&head, &buffer[8], size-8);
                break;
            case TableID::TVCT:  
            case TableID::CVCT:
            {
                emit TableLoaded();

                if (Table[SERVICES]->AddSection(&head,0,0))
                    break;

                const PESPacket pes = PESPacket::ViewData(buffer);
                const PSIPTable psip(pes);
                const VirtualChannelTable vct(psip);
                AddToServices(vct);
            }
            break;
            case TableID::RRT:
                ParseRRT(&head, &buffer[8], size-8);
                break;
#ifdef USING_DVB_EIT
            case TableID::EIT:
                ParseATSCEIT(&head, &buffer[8], size-8, pid);
                break;
            case TableID::ETT:
                ParseETT(&head, &buffer[8], size-8, pid);
                break;
#endif // USING_DVB_EIT
            case TableID::STT:
                ParseSTT(&head, &buffer[8], size-8);
                break;
            case TableID:: DCCT:
                ParseDCCT(&head, &buffer[8], size-8);
                break;
            case TableID::DCCSCT:
                ParseDCCSCT(&head, &buffer[8], size-8);
                break;
        }
    }

    pmap_lock.unlock();

    return;
}

tablehead_t SIParser::ParseTableHead(uint8_t *buffer, int size)
{
// TODO: Maybe handle the size but should be OK if CRC passes

    (void) size;
    tablehead_t head;

    head.table_id       = buffer[0];
    head.section_length = ((buffer[1] & 0x0f)<<8) | buffer[2];
    head.table_id_ext   = (buffer[3] << 8) | buffer[4];
    head.current_next   = (buffer[5] & 0x1);
    head.version        = ((buffer[5] >> 1) & 0x1f);
    head.section_number = buffer[6];
    head.section_last   = buffer[7];

    return head;

}

void SIParser::ParsePAT(uint /*pid*/, tablehead_t *head,
                        uint8_t *buffer, uint size)
{
    // Check to see if you have already loaded all of the PAT sections
    // ISO 13818-1 state that PAT can be segmented, although it rarely is
    if (((PATHandler*) Table[PAT])->Tracker.AddSection(head))
        return;

    VERBOSE(VB_SIPARSER, LOC + QString("PAT Version = %1").arg(head->version));
    PrivateTypes.CurrentTransportID = head->table_id_ext;
    VERBOSE(VB_SIPARSER, LOC + QString("Tuned to TransportID: %1")
             .arg(PrivateTypes.CurrentTransportID));

    int pos = -1;
    while (pos < ((int)size - 4))
    {
        if (pos+4 >= ((int)size-4))
        {
            break;
        }
        uint16_t program = (buffer[pos + 1] << 8) | buffer[pos + 2];
        uint16_t pid = ((buffer[pos + 3] & 0x1f)<<8) | buffer[pos + 4];
        if (program != 0)
        {
            VERBOSE(VB_SIPARSER, LOC + QString("PMT pn(%1) on PID 0x%2")
                    .arg(program).arg(pid,0,16));
            ((PATHandler*) Table[PAT])->pids[program]=pid;
            Table[PMT]->AddKey(pid,0);
            ((PMTHandler*) Table[PMT])->pmt[program].PMTPID = pid;
        }
        else
        {
            // Program 0 in the PAT represents the location of the NIT in DVB
            NITPID = pid;
            VERBOSE(VB_SIPARSER, LOC +
                    QString("NIT Present on this transport on PID 0x%1")
                    .arg(NITPID,0,16));
        }
        pos += 4;

    }

    QString ProgramList = QString("Services on this Transport: ");
    QMap_uint16_t::Iterator p;

    for (p = ((PATHandler*) Table[PAT])->pids.begin();
         p != ((PATHandler*) Table[PAT])->pids.end() ; ++p)
        ProgramList += QString("%1 ").arg(p.key());
    VERBOSE(VB_SIPARSER, LOC + ProgramList);
}

void SIParser::ParseCAT(uint /*pid*/, tablehead_t *head,
                        uint8_t *buffer, uint size)
{
    (void) head;

    CAPMTObject c;

    int pos = -1;
    while (pos < ((int)size - 4))
    {
        if (buffer[pos+1] == 0x09)
        {
            c = ParseDescCA(&buffer[pos+1],buffer[pos+2]);
            VERBOSE(VB_SIPARSER, LOC + QString("CA System 0x%1, EMM PID = %2")
                    .arg(c.CASystemID,0,16).arg(c.PID));
        }
        pos += buffer[pos+2] + 2;
    }
}

void SIParser::ParsePMT(uint pid, tablehead_t *head,
                        uint8_t *buffer, uint size)
{
    // TODO: Catch serviceMove descriptor and send a signal when you get one
    //       to retune to correct transport or send an error tuning the channel

    if (Table[PMT]->AddSection(head,head->table_id_ext,0))
        return;

    VERBOSE(VB_SIPARSER, LOC + QString("PMT pn(%1) version(%2)")
            .arg(head->table_id_ext).arg(head->version));

    // Create a PMTObject and populate it
    PMTObject p;
    uint8_t descriptors_length;
    uint16_t pidescriptors_length = (((buffer[2] & 0x0F) << 8) | buffer[3]);

    p.PCRPID = (buffer[0] & 0x1F) << 8 | buffer[1];
    p.ServiceID = head->table_id_ext;

    // Process Program Info Descriptors
    uint16_t pos = 4;
    uint16_t tempPos = pos + pidescriptors_length;
    while (pos < tempPos)
    {
        if (buffer[pos] == 0x09)
        {
            // Conditional Access Descriptor
            CAPMTObject cad = ParseDescCA(&buffer[pos], buffer[pos + 1]);
            p.CA.append(cad);
            p.hasCA = true;
        }
        else
        {
            p.Descriptors.append(
                Descriptor(&buffer[pos], buffer[pos + 1] + 2));
            ProcessUnusedDescriptor(pid, &buffer[pos], buffer[pos + 1] + 2);
        }
        pos += buffer[pos+1] + 2;
    }

    // Process streams
    while ((pos + 4) < (int)size)
    {

        ElementaryPIDObject e;

        // Reset Elementary PID object
        e.Reset();

        // Grab PIDs, and set PID Type
        e.PID = (buffer[pos+1] & 0x1F) << 8 | buffer[pos+2];
        e.Orig_Type = buffer[pos];
        VERBOSE(VB_SIPARSER, LOC + QString("PID: 0x%1").arg(e.PID,0,16));

        // The stream type can be detected in two ways:
        // - by stream type field in PMT, if known
        // - by a descriptor
        switch (e.Orig_Type)
        {
            case 0x01:
                e.Type = ES_TYPE_VIDEO_MPEG1;
                break;
            case 0x80:  // OpenCable Mpeg2
            case 0x02:  // DVB/ATSC Mpeg2
                e.Type = ES_TYPE_VIDEO_MPEG2;
                break;
            case 0x03:
                e.Type = ES_TYPE_AUDIO_MPEG1;
                break;
            case 0x04:
                e.Type = ES_TYPE_AUDIO_MPEG2;
                break;
            case 0x08:
            case 0x0B:
                e.Type = ES_TYPE_DATA;
                break;
            case 0x0F:
                e.Type = ES_TYPE_AUDIO_AAC;
                break;
            case 0x81:
                // Where ATSC Puts the AC3 Descriptor
                e.Type = ES_TYPE_AUDIO_AC3;
                break;

        }

        descriptors_length = (buffer[pos+3] & 0x0F) << 8 | buffer[pos+4];
        pos += 5;
        tempPos = pos + descriptors_length;

        while (pos < tempPos)
        {
            uint8_t *descriptor = &buffer[pos];
            int descriptor_tag = descriptor[0];
            int descriptor_len = descriptor[1];

            if (descriptor_tag == 0x09) // Conditional Access Descriptor
            {
                // Note: the saved streams have already been 
                // descrambled by the CAM so any CA descriptors 
                // should *not* be added to the descriptor list.
                // We need a CAPMTObject to send to the CAM though.
                CAPMTObject cad = ParseDescCA(descriptor, descriptor_len);
                e.CA.append(cad);
                p.hasCA = true;
            }
            else
            {
                e.Descriptors.append(
                    Descriptor(descriptor, descriptor_len + 2));

                switch (descriptor_tag)
                {
                    case 0x05: // Registration Descriptor
                        {
                            QString format = QString::fromLatin1(
                                    (const char*) descriptor + 2, 4);
                            if (format == "DTS1")
                                e.Type = ES_TYPE_AUDIO_DTS;
                        }
                        break;

                    case 0x0A: // ISO 639 Language Descriptor
                        e.Language =
                            ParseDescLanguage(descriptor+2, descriptor_len);
                        break;

                    case 0x56: // Teletext Descriptor
                        ParseDescTeletext(descriptor, descriptor_len);
                        e.Type = ES_TYPE_TELETEXT;
                        break;

                    case 0x59: // Subtitling Descriptor
                        ParseDescSubtitling(descriptor, descriptor_len);
                        e.Type = ES_TYPE_SUBTITLE;
                        break;

                    case 0x6A: // AC3 Descriptor
                        e.Type = ES_TYPE_AUDIO_AC3;
                        break;

                    default:
                        break;
                }
            }
            
            pos += descriptor_len + 2;
        }

        switch (e.Type)
        {
            case ES_TYPE_VIDEO_MPEG1:
                e.Description = QString("MPEG-1 Video");
                p.hasVideo = true;
                break;
            case ES_TYPE_VIDEO_MPEG2:
                e.Description = QString("MPEG-2 Video");
                p.hasVideo = true;
                break;
            case ES_TYPE_AUDIO_MPEG1:
                e.Description = QString("MPEG-1 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_MPEG2:
                e.Description = QString("MPEG-2 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_AC3:
                e.Description = QString("AC3 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_DTS:
                e.Description = QString("DTS Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_AAC:
                e.Description = QString("AAC Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_TELETEXT:
                e.Description = QString("Teletext");
                break;
            case ES_TYPE_SUBTITLE:
                e.Description = QString("Subtitle");
                break;
            case ES_TYPE_DATA:
                e.Description = QString("Data");
                break;
            default:
                e.Description = QString("Unknown type: %1").arg(e.Orig_Type);
                break;
        }

        if (!e.Language.isEmpty())
            e.Description += QString(" (%1").arg(e.Language);

        p.Components += e;
    }

    ((PMTHandler*) Table[PMT])->pmt[head->table_id_ext] = p;
}

void SIParser::ProcessUnusedDescriptor(uint pid, const uint8_t *data, uint)
{
    if (print_verbose_messages & VB_SIPARSER)
    {
        QMutexLocker locker(&descLock);
        descCount[pid<<8 | data[0]]++;
    }
}

/*------------------------------------------------------------------------
 *   COMMON HELPER FUNCTIONS
 *------------------------------------------------------------------------*/

// Decode a text string according to ETSI EN 300 468 Annex A
QString SIParser::DecodeText(const uint8_t *src, uint length)
{
    QString result;

    if (length <= 0)
        return QString("");

    unsigned char buf[length];
    memcpy(buf, src, length);

    if ((buf[0] <= 0x10) || (buf[0] >= 0x20))
    {
        // Strip formatting characters
        for (uint p = 0; p < length; p++)
        {
            if ((buf[p] >= 0x80) && (buf[p] <= 0x9F))
                memmove(buf + p, buf + p + 1, --length - p);
        }

        if (buf[0] >= 0x20)
        {
            result = QString::fromLatin1((const char*)buf, length);
        }
        else if ((buf[0] >= 0x01) && (buf[0] <= 0x0B))
        {
            QString coding = "ISO8859-" + QString::number(4 + buf[0]);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 1, length - 1);
        }
        else if (buf[0] == 0x10)
        {
            // If the first byte of the text field has a value "0x10"
            // then the following two bytes carry a 16-bit value (uimsbf) N
            // to indicate that the remaining data of the text field is
            // coded using the character code table specified by
            // ISO Standard 8859, parts 1 to 9

            uint code = 1;
            swab(buf + 1, &code, 2);
            QString coding = "ISO8859-" + QString::number(code);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*)buf + 3, length - 3);
        }
        else
        {
            // Unknown/invalid encoding - assume local8Bit
            result = QString::fromLocal8Bit((const char*)buf + 1, length - 1);
        }
    }
    else
    {
        // TODO: Handle multi-byte encodings

        VERBOSE(VB_SIPARSER, LOC + "Multi-byte coded text - not supported!");
        result = "N/A";
    }

    return result;
}

/*------------------------------------------------------------------------
 *   DVB HELPER FUNCTIONS
 *------------------------------------------------------------------------*/

QDateTime SIParser::ConvertDVBDate(const uint8_t *dvb_buf)
{
// TODO: clean this up some since its sort of a mess right now

    // This function taken from dvbdate.c in linuxtv-apps code

    int i;
    int year, month, day, hour, min, sec;
    long int mjd;

    mjd = (dvb_buf[0] & 0xff) << 8;
    mjd += (dvb_buf[1] & 0xff);
    hour = bcdtoint(dvb_buf[2] & 0xff);
    min  = bcdtoint(dvb_buf[3] & 0xff);
    sec  = bcdtoint(dvb_buf[4] & 0xff);

/*
 * Use the routine specified in ETSI EN 300 468 V1.4.1,
 * "Specification for Service Information in Digital Video Broadcasting"
 * to convert from Modified Julian Date to Year, Month, Day.
 */

    year = (int) ((mjd - 15078.2) / 365.25);
    month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
    day = mjd - 14956 - (int) (year * 365.25) - (int) (month * 30.6001);
    if (month == 14 || month == 15)
        i = 1;
    else
        i = 0;
    year += i;
    month = month - 1 - i * 12;

    year += 1900;

    QDateTime UTCTime = QDateTime(QDate(year,month,day),QTime(hour,min,sec));

    // Convert to localtime
    return MythUTCToLocal(UTCTime);
}

/*------------------------------------------------------------------------
 *   DVB TABLE PARSERS
 *------------------------------------------------------------------------*/


void SIParser::ParseNIT(uint pid, tablehead_t *head,
                        uint8_t *buffer, uint size)
{
    // Only process current network NITs for now
    if (head->table_id != 0x40)
        return;

    // Check to see if you already pulled this table section
    if (Table[NETWORK]->AddSection(head,0,0))
        return;

    // TODO: Emit a table load here for scanner

    TransportObject t;

    QMap_uint16_t ChannelNumbers;

    uint16_t network_descriptors_length  = (buffer[0] & 0x0F) << 8 | buffer[1];
    uint16_t transport_stream_loop_length = 0;
    uint16_t transport_descriptors_length = 0;

    uint16_t pos = 2;

    // Parse Network Descriptors (Name, Linkage)
    if (network_descriptors_length > 0)
    {
       // Create a Network Object
       NetworkObject n;
       while ((network_descriptors_length) > (pos-2)) {

           switch (buffer[pos])
           {
               case 0x40:
                          ParseDescNetworkName(&buffer[pos],buffer[pos+1],n);
                          break;
               case 0x4A:
                          ParseDescLinkage(&buffer[pos],buffer[pos+1],n);
                          break;
               default:
                          ProcessUnusedDescriptor(pid, &buffer[pos],
                                                  buffer[pos+1] + 2);
                          break;
           }
           pos += (buffer[pos+1] + 2);
       }
       ((NetworkHandler*) Table[NETWORK])->NITList.Network += n;

    }

    transport_stream_loop_length = (buffer[pos] & 0x0F) << 8 | buffer[pos+1];
    pos += 2;

    // transport desctiptors parser
    uint16_t dpos = 0;

    // Safe to assume that you can run until the end
    while (pos < size - 4) {

        if (PrivateTypesLoaded == false)
        {
            t.NetworkID = buffer[pos+2] << 8 | buffer[pos+3];
            LoadPrivateTypes(t.NetworkID);
        }

        transport_descriptors_length =
            (buffer[pos+4] & 0x0F) << 8 | buffer[pos+5];
        dpos=0;
        while ((transport_descriptors_length) > (dpos))
        {
            switch (buffer[pos + 6 + dpos])
            {
                // DVB-C - Descriptor Parser written by Ian Caulfield
                case 0x44:
                    t = ParseDescCable(
                        &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                    break;

                // DVB-T - Verified thanks to adante in #mythtv 
                // allowing me access to his DVB-T card
                case 0x5A:
                    t = ParseDescTerrestrial(
                        &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                    break;
                // DVB-S
                case 0x43:
                    t = ParseDescSatellite(
                        &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                    break;
                case 0x62:
                    ParseDescFrequencyList(
                        &buffer[pos+6+dpos],buffer[pos + 7 + dpos],t);
                    break;
                case 0x83:
                    if (PrivateTypes.ChannelNumbers == 0x83)
                        ParseDescUKChannelList(
                            &buffer[pos+6+dpos],buffer[pos + 7 + dpos],
                            ChannelNumbers);
                    break;
                default:
                    ProcessUnusedDescriptor(
                        pid,
                        &buffer[pos + 6 + dpos], buffer[pos + 7 + dpos] + 2);
                    break;
             }
             dpos += (buffer[pos + 7 + dpos] + 2);
        }

        // Get TransportID and NetworkID
        t.TransportID = buffer[pos] << 8 | buffer[pos+1];
        t.NetworkID = buffer[pos+2] << 8 | buffer[pos+3];

        Table[SERVICES]->Request(t.TransportID);
    
        if (PrivateTypes.ChannelNumbers && !(ChannelNumbers.empty()))
        {
            QMap_uint16_t::Iterator c;
            for (c = ChannelNumbers.begin() ; c != ChannelNumbers.end() ; ++c)
                ((ServiceHandler*) Table[SERVICES])->
                    Services[t.TransportID][c.key()].ChanNum = c.data();
        }

        ((NetworkHandler*) Table[NETWORK])->NITList.Transport += t;

        pos += transport_descriptors_length + 6;
    }
}

void SIParser::ParseSDT(uint pid, tablehead_t *head,
                        uint8_t *buffer, uint size)
{
    /* Signal to keep scan wizard bars moving */
    emit TableLoaded();

    int CurrentTransport = 0;

    uint16_t network_id = buffer[0] << 8 | buffer[1];
    // TODO: Handle Network Specifics here if they aren't set

    if (PrivateTypesLoaded == false)
        LoadPrivateTypes(network_id);

    if (PrivateTypes.SDTMapping)
    {
        if (PrivateTypes.CurrentTransportID == head->table_id_ext)
            CurrentTransport = 1;
    }
    else
    {
        if (head->table_id == 0x42)
            CurrentTransport = 1;
    }

    if (CurrentTransport)
    {
        if (Table[SERVICES]->AddSection(head,0,0))
            return;
    }
    else
    {
        if (Table[SERVICES]->AddSection(head,head->table_id_ext,0))
            return;
    }

    uint16_t len = 0;
    uint16_t pos = 3;
    uint16_t lentotal = 0;
    uint16_t descriptors_loop_length = 0;
    SDTObject s;

    VERBOSE(VB_SIPARSER, LOC + QString("SDT: NetworkID=%1 TransportID=%2")
            .arg(network_id).arg(head->table_id_ext));

    while (pos < (size-4))
    {
        s.ServiceID = buffer[pos] << 8 | buffer[pos+1];
        s.TransportID = head->table_id_ext;
        s.NetworkID = network_id;
        // EIT is present if either EIT_schedule_flag or 
        // EIT_present_following_flag is set
	s.EITPresent  = (PrivateTypes.ForceGuidePresent) ? 1 : 0;
        s.EITPresent |= (buffer[pos+2] & 0x03)           ? 1 : 0;
        s.RunningStatus = (buffer[pos+3] & 0xE0) >> 5;
        s.CAStatus = (buffer[pos+3] & 0x10) >> 4;
        s.Version = head->version;

        if (((ServiceHandler*) Table[SERVICES])->
            Services[s.TransportID].contains(s.ServiceID))
            s.ChanNum =
                ((ServiceHandler*) Table[SERVICES])->
                Services[s.TransportID][s.ServiceID].ChanNum;

        descriptors_loop_length = (buffer[pos+3] & 0x0F) << 8 | buffer[pos+4];
        lentotal = 0;

        while ((descriptors_loop_length) > (lentotal))
        {
            switch(buffer[pos + 5 + lentotal])
            {
            case 0x48:
                ParseDescService(&buffer[pos + 5 + lentotal], 
                     buffer[pos + 6 + lentotal], s);
                break;
            default:
                ProcessUnusedDescriptor(pid, &buffer[pos + 5 + lentotal],
                                        buffer[pos + 6 + lentotal] + 2);
                break;
            }
            len = buffer[pos + 6 + lentotal];
            lentotal += (len + 2);
        }

        bool eit_requested = false;

#ifdef USING_DVB_EIT
        if ((s.EITPresent) && 
            (s.ServiceType == SDTObject::TV ||
             s.ServiceType == SDTObject::RADIO) && 
            ((!PrivateTypes.GuideOnSingleTransport) ||
            ((PrivateTypes.GuideOnSingleTransport) && 
            (PrivateTypes.GuideTransportID == 
             PrivateTypes.CurrentTransportID)))) 
        {
            Table[EVENTS]->RequestEmit(s.ServiceID);
            eit_requested = true;
        }
#endif

        VERBOSE(VB_SIPARSER, LOC +
                QString("SDT: sid=%1 type=%2 eit_present=%3 "
                        "eit_requested=%4 name=%5")
                .arg(s.ServiceID).arg(s.ServiceType)
                .arg(s.EITPresent).arg(eit_requested)
                .arg(s.ServiceName));

        if (CurrentTransport)
            ((ServiceHandler*) Table[SERVICES])->
                Services[0][s.ServiceID] = s;
        else
            ((ServiceHandler*) Table[SERVICES])->
                Services[s.TransportID][s.ServiceID] = s;
        s.Reset();  
        pos += (descriptors_loop_length + 5);
    }

    if (CurrentTransport)
        emit FindServicesComplete();

#ifdef USING_DVB_EIT
    // TODO: This is temp
    Table[EVENTS]->DependencyMet(SERVICES);
    //Table[EVENTS]->AddPid(0x12,0x00,0x00,true); // see ticket #755
    Table[EVENTS]->AddPid(0x12,0x7F,0x80,0x12); // see ticket #755
#endif // USING_DVB_EIT
}

/** \fn SIParser::GetLanguagePriority(const QString&)
 *  \brief Returns the desirability of a particular language to the user.
 *
 *   The lower the returned number the more preferred the language is.
 *
 *   If the language does not exist in our list of languages it is
 *   inserted as a less desirable language than any of the 
 *   previously inserted languages.
 *
 *  \param language Three character ISO-639 language code.
 */
uint SIParser::GetLanguagePriority(const QString &language)
{
    QMap<QString,uint>::const_iterator it;
    it = LanguagePriority.find(language);

    // if this is an unknown language, check if the canonical version exists
    if (it == LanguagePriority.end())
    {
        QString clang = iso639_str_to_canonoical_str(language);
        it = LanguagePriority.find(clang);
        if (it != LanguagePriority.end())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Added preferred language '%1' with same "
                            "priority as '%2'").arg(language).arg(clang));
            LanguagePriority[language] = *it;
        }
    }

    // if this is an unknown language, insert into priority list
    if (it == LanguagePriority.end())
    {
        // find maximum existing priority
        uint max_priority = 0;
        it = LanguagePriority.begin();
        for (;it != LanguagePriority.end(); ++it)
            max_priority = max(*it, max_priority);
        // insert new language, and point iterator to it
        VERBOSE(VB_SIPARSER, LOC +
                QString("Added preferred language '%1' with priority %2")
                .arg(language).arg(max_priority + 1));
        LanguagePriority[language] = max_priority + 1;
        it = LanguagePriority.find(language);
    }

    // return the priority
    return *it;
}

void SIParser::ParseDVBEIT(uint pid, tablehead_t *head,
                           uint8_t *buffer, uint size)
{
    (void) pid;
    (void) head;
    (void) buffer;
    (void) size;
#ifdef USING_DVB_EIT
    uint8_t last_segment_number  = buffer[4];
    uint8_t last_table_id        = buffer[5];

    if (!((EventHandler*) Table[EVENTS])->TrackerSetup[head->table_id_ext])
    {
        if (PrivateTypes.CustomGuideRanges)
        {
            if ((head->table_id >= PrivateTypes.CurrentTransportTableMin)
                && (head->table_id <= PrivateTypes.CurrentTransportTableMax))
            {
                for (int x = PrivateTypes.CurrentTransportTableMin;
                     x <= PrivateTypes.CurrentTransportTableMax; x++)
                    ((EventHandler*) Table[EVENTS])->
                        Tracker[head->table_id_ext][x].Reset();
            }
            else if ((head->table_id >= PrivateTypes.OtherTransportTableMin)
                && (head->table_id <= PrivateTypes.OtherTransportTableMax))
            {
                for (int x = PrivateTypes.OtherTransportTableMin;
                     x <= PrivateTypes.OtherTransportTableMax; x++)
                    ((EventHandler*) Table[EVENTS])->
                        Tracker[head->table_id_ext][x].Reset();
            }
            
        }
        else
        {
            if ((head->table_id & 0xF0) == 0x50)
            {
                for (int x = 0x50 ; x < (last_table_id & 0x0F) + 0x50 ; x++)
                   ((EventHandler*) Table[EVENTS])->
                       Tracker[head->table_id_ext][x].Reset();
            }

            if ((head->table_id & 0xF0) == 0x60)
            {
                for (int x = 0x60 ; x < (last_table_id & 0x0F) + 0x60 ; x++)
                    ((EventHandler*) Table[EVENTS])->
                        Tracker[head->table_id_ext][x].Reset();
            }
        }
        ((EventHandler*) Table[EVENTS])->
            TrackerSetup[head->table_id_ext] = true;
    }

    if (Table[EVENTS]->AddSection(head,head->table_id_ext,head->table_id))
        return;

    if (last_segment_number != head->section_last)
    {
        for (int x = (last_segment_number+1);
             x < ((head->section_number&0xF8)+8); x++)
            ((EventHandler*) Table[EVENTS])->
                Tracker[head->table_id_ext][head->table_id].MarkUnused(x);
    }

    uint pos                = 6;
    uint des_pos            = 0;
    uint descriptors_length = 0;
    uint bestPrioritySE     = UINT_MAX;
    uint bestPriorityEE     = UINT_MAX;

    // Loop through table (last 4 bytes are CRC)
    while (pos + 4 < size)
    {
        // Event to use temporarily to fill in data
        Event event;
        event.ServiceID   = head->table_id_ext;
        event.TransportID = buffer[0] << 8 | buffer[1];
        event.NetworkID   = buffer[2] << 8 | buffer[3];
        event.EventID     = buffer[pos] << 8 | buffer[pos+1];
        event.StartTime   = ConvertDVBDate(&buffer[pos+2]);

        uint lenInSeconds = ((bcdtoint(buffer[pos+7] & 0xFF) * 3600) +
                             (bcdtoint(buffer[pos+8] & 0xFF) * 60) +
                             (bcdtoint(buffer[pos+9] & 0xFF)));

        event.EndTime     = event.StartTime.addSecs(lenInSeconds);

#ifdef EIT_DEBUG_SID
        if (event.ServiceID == EIT_DEBUG_SID)
        {
            VERBOSE(VB_EIT, "SIParser: DVB Events: " +
                    QString("ServiceID %1 EventID: %2   Time: %3 - %4")
                    .arg(event.ServiceID).arg(event.EventID)
                    .arg(event.StartTime.toString(QString("MM/dd hh:mm")))
                    .arg(event.EndTime.toString(QString("hh:mm"))));
        }
#endif

        // Hold short & extended event information from descriptors.
        const unsigned char *bestDescriptorSE = NULL;
        vector<const unsigned char*> bestDescriptorsEE;

        // Parse descriptors
        descriptors_length = ((buffer[pos+10] & 0x0F) << 8) | buffer[pos+11];
        pos += 12;
        des_pos = pos;

        // Pick out EIT descriptors for later parsing, and parse others.
        while ((des_pos < (pos + descriptors_length)) && (des_pos <= size)) 
        { 
            des_pos += ProcessDVBEventDescriptors(
                pid,
                &buffer[des_pos],
                bestPrioritySE, bestDescriptorSE,
                bestPriorityEE, bestDescriptorsEE, event);
        }

        // Parse extended event descriptions for the most preferred language
        for (uint i = 0; i < bestDescriptorsEE.size(); ++i)
        {
            if (!bestDescriptorsEE[i])
                continue;

            uint8_t *desc    = (uint8_t*) bestDescriptorsEE[i];
            uint     descLen = desc[1];
            ProcessExtendedEventDescriptor(desc, descLen + 2, event);
        }

        // Parse short event descriptor for the most preferred language
        if (bestDescriptorSE)
        {
            uint8_t *desc    = (uint8_t*) bestDescriptorSE;
            uint     descLen = desc[1];
            ProcessShortEventDescriptor(desc, descLen + 2, event);
        }

        eitfixup.Fix(event, PrivateTypes.EITFixUp);

#ifdef EIT_DEBUG_SID
        if (event.ServiceID == EIT_DEBUG_SID)
        {
            VERBOSE(VB_EIT, "SIParser: DVB Events: " +
                    QString("LanguageCode='%1' "
                            "\n\t\t\tEvent_Name='%2' Description='%3'")
                    .arg(event.LanguageCode).arg(event.Event_Name)
                    .arg(event.Description));
        }
#endif

        QMap2D_Events &events = ((EventHandler*) Table[EVENTS])->Events;
        events[head->table_id_ext][event.EventID] = event;
        pos += descriptors_length;
    }
#endif //USING_DVB_EIT
}

/*------------------------------------------------------------------------
 *   COMMON DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/
// Descriptor 0x09 - Conditional Access Descriptor
CAPMTObject SIParser::ParseDescCA(uint8_t *buffer, int size)
{
    (void) size;
    CAPMTObject retval;

    retval.CASystemID = buffer[2] << 8 | buffer[3];
    retval.PID = (buffer[4] & 0x1F) << 8 | buffer[5];
    retval.Data_Length = buffer[1] - 4;
    if (retval.Data_Length > 0)
    {
        memcpy(retval.Data, &buffer[6], retval.Data_Length);
    }

    return retval;
}

/*------------------------------------------------------------------------
 *   DVB DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/
// Descriptor 0x40 - NetworkName
void SIParser::ParseDescNetworkName(uint8_t *buffer, int, NetworkObject &n)
{
    n.NetworkName = DecodeText(buffer + 2, buffer[1]);
}

// Descriptor 0x4A - Linkage - NIT
void SIParser::ParseDescLinkage(uint8_t *buffer, int, NetworkObject &n)
{
    n.LinkageTransportID = buffer[2] << 8 | buffer[3];
    n.LinkageNetworkID = buffer[4] << 8 | buffer[5];
    n.LinkageServiceID = buffer[6] << 8 | buffer[7];
    n.LinkageType = buffer[8];
    n.LinkagePresent = 1;

    // See ticket #778. Breaks "DVB-S in Germany with Astra 19.2E"
#if 0
    if (n.LinkageType == 4)
    {
        PrivateTypes.GuideOnSingleTransport = true;
        PrivateTypes.GuideTransportID = n.LinkageTransportID;
    }
#endif
}

// Descriptor 0x62 - Frequency List - NIT
void SIParser::ParseDescFrequencyList(uint8_t *buffer, int size,
                                      TransportObject &t)
{
    int i = 2;
    uint8_t coding = buffer[i++] & 0x3;
    unsigned frequency;
    QString FrequencyTemp;
    for (; i < size ; i+=4)
    {
         switch (coding)
         {
         case 0x3:  //DVB-T
             frequency*=10;
             frequency = (((buffer[i] << 24) | (buffer[i+1] << 16) |
                           (buffer[i+2] << 8 ) | buffer[i+3]))*10;
             break;
         default:
              FrequencyTemp = QString("%1%2%3%4%5%6%7%800")
             .arg((buffer[i] & 0xF0) >> 4)
             .arg( buffer[i] & 0x0F)
             .arg((buffer[i+1] & 0xF0) >> 4)
             .arg( buffer[i+1] & 0x0F)
             .arg((buffer[i+2] & 0xF0) >> 4)
             .arg( buffer[i+2] & 0x0F)
             .arg((buffer[i+3] & 0xF0) >> 4)
             .arg( buffer[i+3] & 0x0F);
             frequency=FrequencyTemp.toInt();
         }
         t.frequencies+=frequency;
    }
}

//Descriptor 0x83 UK specific channel list
void SIParser::ParseDescUKChannelList(uint8_t *buffer, int size,
                                      QMap_uint16_t &numbers)
{
    int i = 2;

    for (; i < size ; i+=4)
    {

        uint16_t service_id = (buffer[i]<<8)|(buffer[i+1]&0xff);
        uint16_t channel_num = (buffer[i+2]&0x03<<8)|(buffer[i+3]&0xff);

        numbers[service_id] = channel_num;
    }
}

// Desctiptor 0x48 - Service - SDT
void SIParser::ParseDescService(uint8_t *buffer, int, SDTObject &s)
{
    uint8_t tempType = buffer[2];

    if (PrivateTypes.TVServiceTypes.contains(tempType))
        s.ServiceType = PrivateTypes.TVServiceTypes[tempType];
    else
        s.ServiceType = tempType;

    buffer += 3;
    s.ProviderName = DecodeText(buffer + 1, buffer[0]);
    buffer += buffer[0] + 1;
    s.ServiceName = DecodeText(buffer + 1, buffer[0]);
}

// Descriptor 0x5A - DVB-T Transport - NIT
TransportObject SIParser::ParseDescTerrestrial(uint8_t *buffer, int)
{
    TransportObject retval;

    retval.Type = QString("DVB-T");

    retval.Frequency = ((buffer[2] << 24) | (buffer[3] << 16) |
                        (buffer[4] << 8 ) | buffer[5]) * 10;

    // Bandwidth
    switch ((buffer[6] & 0xE0) >> 5) {
       case 0:
               retval.Bandwidth = "8";
               break;
       case 1:
               retval.Bandwidth = "7";
               break;
       case 2:
               retval.Bandwidth = "6";
               break;
       default:
               retval.Bandwidth = "auto";
               break;
    }

    // Consetellation
    switch ((buffer[7] & 0xC0) >> 6) {
       case 0:
               retval.Constellation = "qpsk";
               break;
       case 1:
               retval.Constellation = "qam_16";
               break;
       case 2:
               retval.Constellation = "qam_64";
               break;
       default:
               retval.Constellation = "auto";
               break;
    }

    // Heiarchy
    switch ((buffer[7] & 0x38) >> 3) {
       case 0:
               retval.Hiearchy = "n";
               break;
       case 1:
               retval.Hiearchy = "1";
               break;
       case 2:
               retval.Hiearchy = "2";
               break;
       case 3:
               retval.Hiearchy = "4";
               break;
       default:
               retval.Hiearchy = "a";
               break;
    }

    // CoderateHP
    switch (buffer[7] & 0x03) 
    {
    case 0:
        retval.CodeRateHP = "1/2";
        break;
    case 1:
        retval.CodeRateHP = "2/3";
        break;
    case 2:
        retval.CodeRateHP = "3/4";
        break;
    case 3:
        retval.CodeRateHP = "5/6";
        break;
    case 4:
        retval.CodeRateHP = "7/8";
        break;
    default:
        retval.CodeRateHP = "auto";
    }

    // CoderateLP
    switch ((buffer[8] & 0xC0) >> 6) 
    {
    case 0:
        retval.CodeRateLP = "1/2";
        break;
    case 1:
        retval.CodeRateLP = "2/3";
        break;
    case 2:
        retval.CodeRateLP = "3/4";
        break;
    case 3:
        retval.CodeRateLP = "5/6";
        break;
    case 4:
        retval.CodeRateLP = "7/8";
        break;
    default:
        retval.CodeRateLP = "auto";
    }    
    
    //Guard Interval
    switch ((buffer[8] & 0x18) >> 3) 
    {
    case 0:
        retval.GuardInterval = "1/32";
        break;
    case 1:
        retval.GuardInterval = "1/16";
        break;
    case 2:
        retval.GuardInterval = "1/8";
        break;
    case 3:
        retval.GuardInterval = "1/4";
        break;
    } 
       
    //TransmissionMode
    switch ((buffer[8] & 0x06) >> 1) 
    {
    case 0:
        retval.TransmissionMode = "2";
        break;
    case 1:
        retval.TransmissionMode = "8";
        break;
    default:
        retval.TransmissionMode = "auto";
    }    
    return retval;
}

// Desctiptor 0x43 - Satellite Delivery System - NIT
TransportObject SIParser::ParseDescSatellite(uint8_t *buffer, int)
{
    TransportObject retval;

    retval.Type = QString("DVB-S");

    QString FrequencyTemp = QString("%1%2%3%4%5%6%7%80")
             .arg((buffer[2] & 0xF0) >> 4)
             .arg( buffer[2] & 0x0F)
             .arg((buffer[3] & 0xF0) >> 4)
             .arg( buffer[3] & 0x0F)
             .arg((buffer[4] & 0xF0) >> 4)
             .arg( buffer[4] & 0x0F)
             .arg((buffer[5] & 0xF0) >> 4)
             .arg( buffer[5] & 0x0F);

    // TODO: Use real BCD conversion on Frequency
    retval.Frequency=FrequencyTemp.toInt();

    retval.OrbitalLocation = QString("%1%2.%3")
        .arg( buffer[6], 0, 16)
        .arg((buffer[7] & 0xF0) >> 4)
        .arg( buffer[7] & 0x0F);

    // This isn't reported correctly by some carriers
    switch ((buffer[8] & 0x80) >> 7)
    {
       case 0:
           retval.OrbitalLocation += " West";
           break;
       case 1:
           retval.OrbitalLocation += " East";
           break;
    }

    switch ((buffer[8] & 0x60) >> 5)
    {
        case 0:
            retval.Polarity = "h";
            break;
        case 1:
            retval.Polarity = "v";
            break;
        case 2:
            retval.Polarity = "l";
            break;
        case 3:
            retval.Polarity = "r";
            break;
    }

    switch (buffer[8] & 0x1F)
    {
        case 0:  // Some SAT Providers use this for QPSK for some reason
            // Bell ExpressVu is an example
        case 1:
            retval.Modulation = "qpsk";
            break;
        case 2:
            retval.Modulation = "qpsk_8";
            break;
        case 3:
            retval.Modulation = "qam_16";
            break;
        default:
            retval.Modulation = "auto";
    }

    QString SymbolRateTemp = QString("%1%2%3%4%5%6%700")
        .arg((buffer[9] & 0xF0) >> 4)
        .arg( buffer[9] & 0x0F)
        .arg((buffer[10] & 0xF0) >> 4)
        .arg( buffer[10] & 0x0F)
        .arg((buffer[11] & 0xF0) >> 4)
        .arg( buffer[11] & 0x0F)
        .arg((buffer[12] & 0xF0) >> 4);

    retval.SymbolRate = SymbolRateTemp.toInt();

    switch (buffer[12] & 0x0F)
    {
        case 1:
            retval.FEC_Inner = "1/2";
            break;
        case 2:
            retval.FEC_Inner = "2/3";
            break;
        case 3:
            retval.FEC_Inner = "3/4";
            break;
        case 4:
            retval.FEC_Inner = "5/6";
            break;
        case 5:
            retval.FEC_Inner = "7/8";
            break;
        case 6:
            retval.FEC_Inner = "8/9";
            break;
        case 0x0F:
            retval.FEC_Inner = "none";
            break;
        default:
            retval.FEC_Inner = "auto";
    }

    return retval;
}

// Descriptor 0x44 - Cable Delivery System - NIT
TransportObject SIParser::ParseDescCable(uint8_t *buffer, int)
{
     TransportObject retval;

     retval.Type = QString("DVB-C");

     QString FrequencyTemp = QString("%1%2%3%4%5%6%7%800")
         .arg((buffer[2] & 0xF0) >> 4)
         .arg( buffer[2] & 0x0F)
         .arg((buffer[3] & 0xF0) >> 4)
         .arg( buffer[3] & 0x0F)
         .arg((buffer[4] & 0xF0) >> 4)
         .arg( buffer[4] & 0x0F)
         .arg((buffer[5] & 0xF0) >> 4)
         .arg( buffer[5] & 0x0F);

     // TODO: Use real BCD conversion on Frequency
     retval.Frequency=FrequencyTemp.toInt();

     switch (buffer[7] & 0x0F)
     {
         case 1:
             retval.FEC_Outer = "None";
             break;
         case 2:
             retval.FEC_Outer = "RS(204/188)";
             break;
         default:
             retval.FEC_Outer = "unknown";
             break;
     }

     switch (buffer[8])
     {
         case 1:
             retval.Modulation = "qam_16";
             break;
         case 2:
             retval.Modulation = "qam_32";
             break;
         case 3:
             retval.Modulation = "qam_64";
             break;
         case 4:
             retval.Modulation = "qam_128";
             break;
         case 5:
             retval.Modulation = "qam_256";
             break;
         default:
             retval.Modulation = "auto";
             break;
     }

     QString SymbolRateTemp=QString("%1%2%3%4%5%6%700")
         .arg((buffer[9] & 0xF0) >> 4)
         .arg( buffer[9] & 0x0F)
         .arg((buffer[10] & 0xF0) >> 4)
         .arg( buffer[10] & 0x0F)
         .arg((buffer[11] & 0xF0) >> 4)
         .arg( buffer[11] & 0x0F)
         .arg((buffer[12] & 0xF0) >> 4);

     retval.SymbolRate = SymbolRateTemp.toInt();

     switch (buffer[12] & 0x0F)
     {
         case 1:
             retval.FEC_Inner = "1/2";
             break;
         case 2:
             retval.FEC_Inner = "2/3";
             break;
         case 3:
             retval.FEC_Inner = "3/4";
             break;
         case 4:
             retval.FEC_Inner = "5/6";
             break;
         case 5:
             retval.FEC_Inner = "7/8";
             break;
         case 6:
             retval.FEC_Inner = "8/9";
             break;
         case 0x0F:
             retval.FEC_Inner = "none";
             break;
         default:
             retval.FEC_Inner = "auto";
             break;
     }

     return retval;
}

#ifdef USING_DVB_EIT
/** \fn SIParser::ProcessDVBEventDescriptors(uint,const unsigned char*,const unsigned char*,uint,vector<const unsigned char*>&,Event&)
 *  \brief Processes non-language dependent DVB Event descriptors, and caches
 *         language dependent DVB Event descriptors for the most preferred
 *         language.
 * \returns descriptor lenght + 2, aka total descriptor size
 */
uint SIParser::ProcessDVBEventDescriptors(
    uint                         pid,
    const unsigned char          *data,
    uint                         &bestPrioritySE,
    const unsigned char*         &bestDescriptorSE, 
    uint                         &bestPriorityEE,
    vector<const unsigned char*> &bestDescriptorsEE,
    Event                        &event)
{
    uint    descriptorTag    = data[0];
    uint    descriptorLength = data[1];

    switch (descriptorTag)
    {
        case DescriptorID::short_event:
        {
            QString language = ParseDescLanguage(data+2, descriptorLength);

            uint    priority = GetLanguagePriority(language);
            bestPrioritySE   = min(bestPrioritySE, priority);

            // add the descriptor, and update the language
            if (priority == bestPrioritySE)
                bestDescriptorSE = data;
        }
        break;

        case DescriptorID::extended_event:
        {
            uint last_desc_number =  data[2]     & 0xf;
            uint desc_number      = (data[2]>>4) & 0xf;

            // Break if the descriptor is out of bounds
            if (desc_number > last_desc_number)
                break;

            QString language = ParseDescLanguage(data+3, descriptorLength-1);
            uint    priority = GetLanguagePriority(language);

            // lower priority number is a higher priority...
            if (priority < bestPriorityEE)
            {
                // found a language with better priority
                // don't keep things from the wrong language
                bestDescriptorsEE.clear();
                bestPriorityEE = priority;
            }

            if (priority == bestPriorityEE)
            {
                // make sure the vector is big enough
                bestDescriptorsEE.resize(last_desc_number + 1);
                // add the descriptor, and update the language
                bestDescriptorsEE[desc_number] = data;
            }
        }
        break;

        case DescriptorID::component:
            ProcessComponentDescriptor(data, descriptorLength + 2, event);
            break;

        case DescriptorID::content:
            ProcessContentDescriptor(data, descriptorLength + 2, event);
            break;

        default:
            ProcessUnusedDescriptor(pid, data, descriptorLength + 2);
            break;
    }
    return descriptorLength + 2;
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
/**
 *  \brief DVB Descriptor 0x54 - Process Content Descriptor - EIT
 *
 *  \TODO Add all types, possibly just lookup from a big 
 *        array that is an include file?
 */
void SIParser::ProcessContentDescriptor(const uint8_t *buf, uint size, Event& e)
{
    (void) size; // TODO validate size

    uint8_t content = buf[2];
    if (content)
    {
        e.ContentDescription = m_mapCategories[content];
        switch (content)
        {
        case 0x10 ... 0x1f:
            e.CategoryType = "movie";
            break;
        case 0x40 ... 0x4f:
            e.CategoryType = "sports";
            break;
        default:
            e.CategoryType = "tvshow";
        }     
    }
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
/** \fn SIParser::ProcessShortEventDescriptor(const uint8_t*,uint,Event&)
 *  \brief Processes DVB Descriptor 0x4D - Short Event Descriptor - EIT
 */
void SIParser::ProcessShortEventDescriptor(
    const uint8_t *data, uint size, Event &e)
{
    // TODO validate size

    uint name_len     = data[5];
    uint subtitle_len = data[6 + name_len];
    e.LanguageCode    = ParseDescLanguage(data+2, size);
    e.Event_Name      = DecodeText(&data[6],            name_len);
    e.Event_Subtitle  = DecodeText(&data[7 + name_len], subtitle_len);

    if (e.Event_Subtitle == e.Event_Name)
        e.Event_Subtitle = "";
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
/** \fn SIParser::ProcessExtendedEventDescriptor(const uint8_t*,uint,Event&)
 *  \brief Processes DVB Descriptor 0x4E - Extended Event - EIT
 */
void SIParser::ProcessExtendedEventDescriptor(
    const uint8_t *data, uint size, Event &e)
{
    (void) size; // TODO validate size

    if (data[6] == 0)
        e.Description += DecodeText(&data[8], data[7]);
}
#endif //USING_DVB_EIT

QString SIParser::ParseDescLanguage(const uint8_t *data, uint size)
{
    (void) size; // TODO validate size

    return QString::fromLatin1((const char*)data, 3);
}

#ifdef USING_DVB_EIT
void SIParser::ProcessComponentDescriptor(
    const uint8_t *buf, uint size, Event &e)
{
   (void) size; // TODO validate size

   switch (buf[2] & 0x0f)
   {
   case 0x1: //Video
       if ((buf[3] >= 0x9) && (buf[3] <= 0x10))
           e.HDTV = true;
       break;
   case 0x2: //Audio
       if ((buf[3] == 0x3) || (buf[3] == 0x5))
           e.Stereo = true;
       break;
   case 0x3: //Teletext/Subtitles
       switch (buf[3])
       {
           case 0x1:
           case 0x3:
           case 0x10 ... 0x13:
           case 0x20 ... 0x23:
               e.SubTitled = true;
       }
       break;
   }
}
#endif //USING_DVB_EIT

void SIParser::ParseDescTeletext(uint8_t *buffer, int size)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Teletext Descriptor"));

    buffer += 2;

    while (size >= 5)
    {
        QString language = QString::fromLatin1((const char*) buffer, 3);
        uint8_t teletext_type = buffer[3] >> 3;
        uint8_t teletext_magazine_number = buffer[3] & 0x07;
        uint8_t teletext_page_number = buffer[4];

        VERBOSE(VB_SIPARSER, LOC + "ParseDescTT(): " +
                QString("lang: %1, type: %2, mag: %3, page: %4")
                .arg(language)
                .arg(teletext_type)
                .arg(teletext_magazine_number)
                .arg(teletext_page_number));

        buffer += 5;
        size -= 5;
    }
}

void SIParser::ParseDescSubtitling(uint8_t *buffer, int size)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Subtitling Descriptor"));

    buffer += 2;

    while (size >= 8)
    {
        QString language = QString::fromLatin1((const char*) buffer, 3);
        uint8_t subtitling_type = buffer[3];
        uint16_t composition_page_id = (buffer[4] << 8) | buffer[5];
        uint16_t ancillary_page_id = (buffer[6] << 8) | buffer[7];

        VERBOSE(VB_SIPARSER, LOC + "ParseDescSub(): " +
                QString("lang: %1, type: %2, comp: %3, anc: %4")
                .arg(language).arg(subtitling_type)
                .arg(composition_page_id).arg(ancillary_page_id));

        buffer += 8;
        size -= 8;
    }
}

/*------------------------------------------------------------------------
 *   ATSC HELPER PARSERS
 *------------------------------------------------------------------------*/

QDateTime SIParser::ConvertATSCDate(uint32_t offset)
{
// TODO: Clean this up its a mess right now to get it to localtime
    QDateTime ATSCEPOC = QDateTime(QDate(1980,1,6));
    // Get event time and add on GPS Second Difference
    // QDateTime UTCTime = 
    //     ATSCEPOC.addSecs(offset - (((STTHandler*) Table[STT])->GPSOffset));
    QDateTime UTCTime = ATSCEPOC.addSecs(offset - 13 );

    // Convert to localtime
    return MythUTCToLocal(UTCTime);
}

/*
 * Global ATSC Multiple String Format Parser into QString(s)
 */
QString SIParser::ParseMSS(uint8_t *buffer, int size)
{
// TODO: Check size
//       Deal with multiple strings.
//       Handle hufmann encoded text

   (void) size;

   QString retval;

   // Language bytes 1,2,3 - Use DVB Language Function
   // Segments 4
   // Compression Type = 5
   // Mode = 6
   // Bytes = 7

    if (buffer[4] > 1)
        VERBOSE(VB_SIPARSER, LOC + "MSS WITH MORE THAN 1 SEGMENT");

    switch (buffer[5])
    {
        case 0:
            for (int z=0; z < buffer[7]; z++)
                retval += QChar(buffer[8+z]);
            break;
        case 1:
            retval = HuffmanToQString(&buffer[8],buffer[7],ATSC_C5);
            break;
        case 2:
            retval = HuffmanToQString(&buffer[8],buffer[7],ATSC_C7);
            break;
        default:
            retval = QString("Unknown compression");
            break;
    }
    return retval;

}

/*------------------------------------------------------------------------
 *   ATSC TABLE PARSERS
 *------------------------------------------------------------------------*/

/*
 *  ATSC Table 0xC7 - Master Guide Table - PID 0x1FFB
 */
void SIParser::ParseMGT(tablehead_t *head, uint8_t *buffer, int)
{
    if (Table[MGT]->AddSection(head,0,0))
        return;

    uint16_t pos = 3;
    uint16_t tables_defined = buffer[1] << 8 | buffer[2];;

    for (int x = 0 ; x < tables_defined ; x++)
    {
        uint16_t table_type = buffer[pos] << 8 | buffer[pos+1];
        uint16_t table_type_pid = (buffer[pos+2] & 0x1F) << 8 | buffer[pos+3];
        uint32_t size = (buffer[pos+5] << 24) |
            (buffer[pos+6] << 16) |
            (buffer[pos+7] << 8) |
            (buffer[pos+8]);

        uint16_t descriptors_length =
            (buffer[pos+9] & 0x0F) << 8 | buffer[pos+10];

        switch (table_type)
        {
            case 0x00 ... 0x03:
                TableSourcePIDs.ServicesPID = table_type_pid;
                TableSourcePIDs.ServicesMask = 0xFF;
                if (table_type == 0x02)
                {
                    TableSourcePIDs.ServicesTable = TableID::CVCT;
                    VERBOSE(VB_SIPARSER, LOC +
                            QString("CVCT Present on PID 0x%1 (%2)")
                            .arg(table_type_pid,0,16).arg(size));
                }
                if (table_type == 0x00)
                {
                    TableSourcePIDs.ServicesTable = TableID::TVCT;
                    VERBOSE(VB_SIPARSER, LOC +
                            QString("TVCT Present on PID 0x%1 (%2)")
                            .arg(table_type_pid,0,16).arg(size));
                }
                break;
            case 0x04:
                TableSourcePIDs.ChannelETT = table_type_pid;
                VERBOSE(VB_SIPARSER, LOC +
                        QString("Channel ETT Present on PID 0x%1 (%2)")
                        .arg(table_type_pid,0,16).arg(size));
                break;

#ifdef USING_DVB_EIT
            case 0x100 ... 0x17F:
                VERBOSE(VB_SIPARSER, LOC +
                        QString("EIT-%1 Present on PID 0x%2")
                        .arg(table_type - 0x100)
                        .arg(table_type_pid,0,16));
                Table[EVENTS]->AddPid(table_type_pid,0xCB,0xFF,
                                      table_type - 0x100);
                break;

            case 0x200 ... 0x27F:
                VERBOSE(VB_SIPARSER, LOC +
                        QString("ETT-%1 Present on PID 0x%2")
                        .arg(table_type - 0x200)
                        .arg(table_type_pid,0,16));
                Table[EVENTS]->AddPid(table_type_pid,0xCC,0xFF,
                                      table_type - 0x200);
                break;
#endif // USING_DVB_EIT

            default:
                VERBOSE(VB_SIPARSER, LOC +
                        QString("Unknown Table %1 in MGT on PID 0x%2")
                        .arg(table_type).arg(table_type_pid,0,16));
                break;
        }                    
        pos += 11;
        pos += descriptors_length;
    }
}

void SIParser::AddToServices(const VirtualChannelTable &vct)
{
    for (uint chan_idx = 0; chan_idx < vct.ChannelCount() ; chan_idx++)
    {
        // Do not add in Analog Channels in the VCT
        if (1 == vct.ModulationMode(chan_idx))
            continue;

        // Create SDTObject from info in VCT
        SDTObject s;
        s.Version      = vct.Version();
        s.ServiceType  = 1;
        s.EITPresent   = !vct.IsHiddenInGuide(chan_idx);

        s.ServiceName  = vct.ShortChannelName(chan_idx);
        s.ChanNum      =(vct.MajorChannel(chan_idx) * 10 +
                         vct.MinorChannel(chan_idx));
        s.TransportID  = vct.ChannelTransportStreamID(chan_idx);
        s.CAStatus     = vct.IsAccessControlled(chan_idx);
        s.ServiceID    = vct.ProgramNumber(chan_idx);
        s.ATSCSourceID = vct.SourceID(chan_idx);

        if (!PrivateTypesLoaded)
            LoadPrivateTypes(s.TransportID);

#ifdef USING_DVB_EIT
        if (!vct.IsHiddenInGuide(chan_idx))
        {
            VERBOSE(VB_EIT, LOC + "Adding Source #"<<s.ATSCSourceID
                    <<" ATSC chan "<<vct.MajorChannel(chan_idx)
                    <<"-"<<vct.MinorChannel(chan_idx));
            sourceid_to_channel[s.ATSCSourceID] =
                vct.MajorChannel(chan_idx) << 8 | vct.MinorChannel(chan_idx);
            Table[EVENTS]->RequestEmit(s.ATSCSourceID);
        }
        else
        {
            VERBOSE(VB_EIT, LOC + "ATSC chan "<<vct.MajorChannel(chan_idx)
                    <<"-"<<vct.MinorChannel(chan_idx)<<" is hidden in guide");
        }
#endif

        ((ServiceHandler*) Table[SERVICES])->Services[0][s.ServiceID] = s;
    }

#ifdef USING_DVB_EIT
// TODO REMOVE THIS WHEN SERVIVES SET
    Table[EVENTS]->DependencyMet(SERVICES);
#endif // USING_DVB_EIT

    emit FindServicesComplete();
}

/**
 * \brief ATSC Table 0xCA - Rating Region Table - PID 0x1FFB
 * \TODO Decide what to do with this. 
 *       There currently is no field in Myth for a rating
 */
void SIParser::ParseRRT(tablehead_t *head, uint8_t *buffer, int size)
{
    (void)head;
    (void)buffer;
    (void)size;

    return;

    uint16_t pos = 0;

    QString temp = ParseMSS(&buffer[2],buffer[1]);

    pos += buffer[1] + 2;
    uint8_t dimensions = buffer[pos++];

    for (int x = 0 ; x < dimensions ; x++)
    {
        QString Name = ParseMSS(&buffer[pos + 1],buffer[pos]);
        /* Skip past Dimesion Name */
        pos += buffer[pos] + 1;

        /* Skip past Values Defined */
        uint8_t values = buffer[pos] & 0x0F;
        pos++;

        for (int y = 0 ; y < values ; y++)
        {
            QString Value_Text = ParseMSS(&buffer[pos + 1],buffer[pos]);
            pos += buffer[pos] + 1;

            QString Rating_Value_Text = ParseMSS(&buffer[pos + 1],buffer[pos]);
            pos += buffer[pos] + 1;

        }

    }
}

/*
 *  ATSC Table 0xCB - Event Information Table - PID Varies
 */
void SIParser::ParseATSCEIT(tablehead_t *head, uint8_t *buffer,
                            int size, uint16_t pid)
{
    (void) head;
    (void) buffer;
    (void) size;
    (void) pid;
#ifdef USING_DVB_EIT

    if (Table[EVENTS]->AddSection(head,head->table_id_ext,pid))
        return;

    Event e;

    uint8_t num_events = buffer[1];
    uint16_t pos = 2, lentotal = 0, len = 0;

    int atsc_src_id = sourceid_to_channel[head->table_id_ext];
    if (!atsc_src_id)
    {
        VERBOSE(VB_EIT, LOC + "ParseATSCEIT(): " +
                QString("Ignoring data. Source %1 not in map.")
                .arg(head->table_id_ext));
        return;
    }
    else
    {
        VERBOSE(VB_EIT, LOC + "ParseATSCEIT(): " +
                QString("Adding data. ATSC Channel is %1_%2.")
                .arg(atsc_src_id >> 8).arg(atsc_src_id & 0xff));
    }   

    for (int z = 0; z < num_events; z++)
    {
        e.Reset();
        e.SourcePID = pid;
        uint16_t event_id = ((buffer[pos] & 0x3F) << 8) | buffer[pos+1];
        uint32_t start_time_offset =
            buffer[pos+2] << 24 | buffer[pos+3] << 16 |
            buffer[pos+4] << 8  | buffer[pos+5];

        e.StartTime = ConvertATSCDate(start_time_offset);
        e.ETM_Location = (buffer[pos+6] & 0x30 ) >> 4;

        uint32_t length_in_seconds =
            (buffer[pos+6] & 0x0F) << 16 | buffer[pos+7] << 8 | buffer[pos+8];
        e.EndTime = e.StartTime.addSecs(length_in_seconds);

        e.ServiceID = atsc_src_id;
        e.ATSC = true;

        uint8_t title_length = buffer[pos+9];
        e.Event_Name = ParseMSS(&buffer[pos+10], title_length);

#ifdef EIT_DEBUG_SID
        VERBOSE(VB_EIT, LOC + "ParseATSCEIT(): " +
                QString("[%1][%2]: %3\t%4 - %5")
                .arg(atsc_src_id).arg(event_id)
                .arg(e.Event_Name.ascii(), 20)
                .arg(e.StartTime.toString("MM/dd hh:mm"))
                .arg(e.EndTime.toString("hh:mm")));
#endif
        pos += (title_length + 10);

        uint16_t descriptors_length =
            (buffer[pos] & 0x0F) << 8 | buffer[pos+1];

        lentotal = 0;
        while (descriptors_length > lentotal)
        {
            switch (buffer[pos + 2 + lentotal])
            {
            case 0x86:    //TODO: ATSC Caption Descriptor
                break;
            case 0x87:   // Content Advisory Decriptor
                ParseDescATSCContentAdvisory(
                    &buffer[pos + 2 + lentotal],
                    buffer[pos + 3 + lentotal]);
            break;
            default:
                ProcessUnusedDescriptor(
                    pid,
                    &buffer[pos + 2 + lentotal],
                    buffer[pos + 3 + lentotal] + 2);
                break;
            }
            len = buffer[pos + 3 + lentotal];
            lentotal += (len + 2);
        }

        pos += (descriptors_length + 2);
        eitfixup.Fix(e, PrivateTypes.EITFixUp);

        EventHandler *eh = (EventHandler*) Table[EVENTS];
        eh->Events[head->table_id_ext][event_id] = e;
    }
#endif //USING_DVB_EIT
}

/*
 *  ATSC Table 0xCC - Extended Text Table - PID Varies
 */
void SIParser::ParseETT(tablehead_t */*head*/, uint8_t *buffer,
                        int size, uint16_t /*pid*/)
{
    (void) buffer;
    (void) size;
#ifdef USING_DVB_EIT
    int source_id = buffer[1] << 8 | buffer[2];
    int etm_id    = buffer[3] << 8 | buffer[4];
    int etm_id2   = etm_id >> 2;

    if ((etm_id & 0x03) != 2)
        return;

    int atsc_src_id = sourceid_to_channel[source_id];
    if (!atsc_src_id)
        return;

    EventHandler *eh = (EventHandler*) Table[EVENTS];
    if (eh->Events[source_id].contains(etm_id2) &&
        eh->Events[source_id][etm_id2].ETM_Location)
    {
        eh->Events[source_id][etm_id2].Description =
            ParseMSS(&buffer[5], size - 5);

        eitfixup.Fix(eh->Events[source_id][etm_id2], PrivateTypes.EITFixUp);

        eh->Events[source_id][etm_id2].ETM_Location = 0;
    }
#endif //USING_DVB_EIT
}

/*
 *  ATSC Table 0xCD - System Time Table - PID 0x1FFB
 */
void SIParser::ParseSTT(tablehead_t *head, uint8_t *buffer, int size)
{
    (void)head;
    (void)size;

    if (Table[STT]->AddSection(head,0,0))
        return;

    ((STTHandler*) Table[STT])->GPSOffset = buffer[5];

    VERBOSE(VB_SIPARSER, LOC +
            QString("GPS time offset is %1 seconds").arg(buffer[5]));
}


/*
 *  ATSC Table 0xD3 - Directed Channel Change Table - PID 0x1FFB
 */
void SIParser::ParseDCCT(tablehead_t *head, uint8_t *buffer, int size)
{
    (void)head;
    (void)buffer;
    (void)size;

}

/*
 *  ATSC Table 0xD4 - Directed Channel Change Selection Code Table - PID 0x1FFB
 */
void SIParser::ParseDCCSCT(tablehead_t *head, uint8_t *buffer, int size)
{
    (void)head;
    (void)buffer;
    (void)size;
}

/*------------------------------------------------------------------------
 *   ATSC DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/

QString SIParser::ParseATSCExtendedChannelName(uint8_t *buffer, int size)
{

    return ParseMSS(&buffer[2],size);

}

// TODO: Use this descriptor parser
void SIParser::ParseDescATSCContentAdvisory(uint8_t *buffer, int size)
{

    (void) buffer;
    (void) size;

    return;

    uint8_t pos = 3;
    QString temp = "";

    for (int x = 0 ; x < (buffer[2] & 0x3F) ; x++)
    {
        uint8_t dimensions = buffer[pos+1];
        pos += 2;
        for (int y = 0 ; y < dimensions ; y++)
            pos += 2;
        if (buffer[pos] > 0)
            temp = ParseMSS(&buffer[pos+1],buffer[pos]);
        pos += buffer[pos] + 1;
    }

}

/*------------------------------------------------------------------------
 * Huffman Text Decompressors - 1 and 2 level routines. Tables defined in
 * atsc_huffman.h
 *------------------------------------------------------------------------*/

/* returns the root for character Input from table Table[] */
int SIParser::HuffmanGetRootNode(uint8_t Input, uint8_t Table[])
{
    if (Input > 127)
        return -1;
    else
        return (Table[Input * 2] << 8) | Table[(Input * 2) + 1];
}

/* Returns the bit number bit from string test[] */
bool SIParser::HuffmanGetBit(uint8_t test[], uint16_t bit)
{
    return (test[(bit - (bit % 8)) / 8] >> (7-(bit % 8))) & 0x01;
}

QString SIParser::HuffmanToQString(uint8_t test[], uint16_t size,
                                   uint8_t Table[])
{

    QString retval;

    int totalbits = size * 8;
    int bit = 0;
    int root = HuffmanGetRootNode(0,Table);
    int node = 0;
    bool thebit;
    uint8_t val;

    while (bit < totalbits)
    {
        thebit = HuffmanGetBit(test,bit);
        val = thebit? Table[root+(node*2)+1] : Table[root+(node*2)];

        if (val & 0x80)
        {
            /* Got a Null Character so return */
            if ((val & 0x7F) == 0)
            {
                return retval;
            }
            /* Escape character so next character is uncompressed */
            if ((val & 0x7F) == 27)
            {
                uint8_t val2 = 0;
                for (int i = 0 ; i < 7 ; i++)
                {
                    val2 |= (HuffmanGetBit(test,bit+i+2) << 6-i);

                }
                retval += QChar(val2);
                bit += 8;
                root = HuffmanGetRootNode(val2,Table);
            }
            /* Standard Character */
            else
            {
                root = HuffmanGetRootNode(val & 0x7F,Table);
                retval += QChar(val & 0x7F);
            }
            node = 0;
        }
        else
            node = val;
        bit++;
    }
    /* If you get here something went wrong so just return a blank string */
    return QString("");
}

static inline int huffman2_get_bit(unsigned char &bitpos,
                                   const unsigned char **bufptr)
{
   int ret = ((**bufptr & bitpos) != 0);
   bitpos >>= 1;
   if (!bitpos)
   {
       bitpos = 0x80;
       (*bufptr)++;
   }
   return ret;
}

static inline void huffman2_set_pos(unsigned char &bitpos,
                                    const unsigned char **bufptr,
                                    const unsigned char *buffer,
                                    uint pos)
{
    *bufptr = buffer + (pos >> 3);
    bitpos  = 0x80 >> (pos & 0x7);
}

static uint huffman2_to_string(const unsigned char *compressed,
                               uint length, uint table,
                               QString &decompressed)
{
    decompressed = "";

    unsigned char        bitpos;
    const unsigned char *bufptr;
    huffman2_set_pos(bitpos, &bufptr, compressed, 0);

    // Determine which huffman table to use
    struct huffman_table *ptrTable;
    const unsigned char  *lookup;
    uint                  min_size;
    uint                  max_size;
    if (table == 1)
    {
        ptrTable = Table128;
        lookup   = Huff2Lookup128;
        min_size = 3;
        max_size = 12;
    }
    else
    {
        ptrTable = Table255;
        lookup   = Huff2Lookup256;
        min_size = 2;
        max_size = 14;
    }

    // walk thru all the bits in the byte array, finding each sequence in the
    // list and decoding it to a character.
    uint total_bits  = length << 3;
    uint current_bit = 0;
    uint count       = 0;
    while (current_bit + 3 < total_bits)
    {
        uint cur_size = 0;
        uint bits     = 0;

        for (; cur_size < min_size; cur_size++)
            bits = (bits << 1) | huffman2_get_bit(bitpos, &bufptr);

        while (cur_size < max_size)
        {
            uint key = lookup[bits];
            if (key && (ptrTable[key].number_of_bits == cur_size))
            {
                decompressed += ptrTable[key].character;
                current_bit += cur_size;
                break;
            }
            bits = (bits << 1) | huffman2_get_bit(bitpos, &bufptr);
            cur_size++;
        }

        if (cur_size == max_size)
            huffman2_set_pos(bitpos, &bufptr, compressed, ++current_bit);
    }

    return count;
}

#ifdef USING_DVB_EIT
/* Huffman Text Decompression Routines used by some Nagra Providers */
void SIParser::ProcessDescHuffmanEventInfo(
    const unsigned char *buf, uint /*sz*/, Event &e)
{
    QString decompressed;

    if ((buf[4] & 0xF8) == 0x80)
       huffman2_to_string(buf+5, buf[1]-3 , 2, decompressed);
    else
       huffman2_to_string(buf+4, buf[1]-2 , 2, decompressed);

    QStringList SplitValues = QStringList::split("}{",decompressed);

    uint8_t switchVal = 0;
    QString temp;

    for (QStringList::Iterator it = SplitValues.begin();
         it != SplitValues.end(); ++it)
    {
        (*it).replace( "{" , "" );
        (*it).replace( "}" , "" );
        switchVal = (*it).left(1).toInt();
        temp = (*it).mid(2);

        switch (switchVal)
        {
            case 1:
                /* Sub Title */
                e.Event_Subtitle = temp;
                break;
            case 2:
                /* Category */
                e.ContentDescription = temp;
                break;
            case 4:
                e.Year = temp;
                break;
            case 5:
                e.Description = temp;
                break;
            case 7:
                /* Audio */
                break;
            case 6: 
                /* Subtitles */
                break;
            default:
                break;
        }
    }
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
/* Used by some Nagra Systems for Huffman Copressed Guide */
QString SIParser::ProcessDescHuffmanText(
    const unsigned char *buf, uint /*sz*/)
{
    QString decompressed;

    if ((buf[3] & 0xF8) == 0x80)
       huffman2_to_string(buf+4, buf[1]-2, 2, decompressed);
    else
       huffman2_to_string(buf+3, buf[1]-1, 2, decompressed);

    return decompressed;
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
QString SIParser::ProcessDescHuffmanTextLarge(
    const unsigned char *buf, uint /*sz*/)
{
    QString decompressed;

    if ((buf[4] & 0xF8) == 0x80)
       huffman2_to_string(buf+5, buf[1]-3 , 2, decompressed);
    else
       huffman2_to_string(buf+4, buf[1]-2 , 2, decompressed);

    return decompressed;
}
#endif //USING_DVB_EIT

void SIParser::InitializeCategories()
{
    m_mapCategories[0x10] = tr("Movie");
    m_mapCategories[0x11] = tr("Movie") + " - " +
        tr("Detective/Thriller");
    m_mapCategories[0x12] = tr("Movie")+ " - " +
        tr("Adventure/Western/War");
    m_mapCategories[0x13] = tr("Movie")+ " - " +
        tr("Science Fiction/Fantasy/Horror");
    m_mapCategories[0x14] = tr("Movie")+ " - " +
        tr("Comedy");
    m_mapCategories[0x15] = tr("Movie")+ " - " +
        tr("Soap/melodrama/folkloric");
    m_mapCategories[0x16] = tr("Movie")+ " - " +
        tr("Romance");
    m_mapCategories[0x17] = tr("Movie")+ " - " +
        tr("Serious/Classical/Religious/Historical Movie/Drama");
    m_mapCategories[0x18] = tr("Movie")+ " - " +
        tr("Adult", "Adult Movie");
    
    m_mapCategories[0x20] = tr("News");
    m_mapCategories[0x21] = tr("News/weather report");
    m_mapCategories[0x22] = tr("News magazine");
    m_mapCategories[0x23] = tr("Documentary");
    m_mapCategories[0x24] = tr("Intelligent Programmes");
    
    m_mapCategories[0x30] = tr("Show/game Show");
    m_mapCategories[0x31] = tr("Game Show");
    m_mapCategories[0x32] = tr("Variety Show");
    m_mapCategories[0x33] = tr("Talk Show");
    
    m_mapCategories[0x40] = tr("Sports");
    m_mapCategories[0x41] = tr("Special Events (World Cup, World Series..)");
    m_mapCategories[0x42] = tr("Sports Magazines");
    m_mapCategories[0x43] = tr("Football (Soccer)");
    m_mapCategories[0x44] = tr("Tennis/Squash");
    m_mapCategories[0x45] = tr("Misc. Team Sports"); // not football/soccer
    m_mapCategories[0x46] = tr("Athletics");
    m_mapCategories[0x47] = tr("Motor Sport");
    m_mapCategories[0x48] = tr("Water Sport");
    m_mapCategories[0x49] = tr("Winter Sports");
    m_mapCategories[0x4A] = tr("Equestrian");
    m_mapCategories[0x4B] = tr("Martial Sports");
    
    m_mapCategories[0x50] = tr("Kids");
    m_mapCategories[0x51] = tr("Pre-School Children's Programmes");
    m_mapCategories[0x52] = tr("Entertainment Programmes for 6 to 14");
    m_mapCategories[0x53] = tr("Entertainment Programmes for 10 to 16");
    m_mapCategories[0x54] = tr("Informational/Educational");
    m_mapCategories[0x55] = tr("Cartoons/Puppets");
    
    m_mapCategories[0x60] = tr("Music/Ballet/Dance");
    m_mapCategories[0x61] = tr("Rock/Pop");
    m_mapCategories[0x62] = tr("Classical Music");
    m_mapCategories[0x63] = tr("Folk Music");
    m_mapCategories[0x64] = tr("Jazz");
    m_mapCategories[0x65] = tr("Musical/Opera");
    m_mapCategories[0x66] = tr("Ballet");

    m_mapCategories[0x70] = tr("Arts/Culture");
    m_mapCategories[0x71] = tr("Performing Arts");
    m_mapCategories[0x72] = tr("Fine Arts");
    m_mapCategories[0x73] = tr("Religion");
    m_mapCategories[0x74] = tr("Popular Culture/Traditional Arts");
    m_mapCategories[0x75] = tr("Literature");
    m_mapCategories[0x76] = tr("Film/Cinema");
    m_mapCategories[0x77] = tr("Experimental Film/Video");
    m_mapCategories[0x78] = tr("Broadcasting/Press");
    m_mapCategories[0x79] = tr("New Media");
    m_mapCategories[0x7A] = tr("Arts/Culture Magazines");
    m_mapCategories[0x7B] = tr("Fashion");
    
    m_mapCategories[0x80] = tr("Social/Policical/Economics");
    m_mapCategories[0x81] = tr("Magazines/Reports/Documentary");
    m_mapCategories[0x82] = tr("Economics/Social Advisory");
    m_mapCategories[0x83] = tr("Remarkable People");
    
    m_mapCategories[0x90] = tr("Education/Science/Factual");
    m_mapCategories[0x91] = tr("Nature/animals/Environment");
    m_mapCategories[0x92] = tr("Technology/Natural Sciences");
    m_mapCategories[0x93] = tr("Medicine/Physiology/Psychology");
    m_mapCategories[0x94] = tr("Foreign Countries/Expeditions");
    m_mapCategories[0x95] = tr("Social/Spiritual Sciences");
    m_mapCategories[0x96] = tr("Further Education");
    m_mapCategories[0x97] = tr("Languages");
    
    m_mapCategories[0xA0] = tr("Leisure/Hobbies");
    m_mapCategories[0xA1] = tr("Tourism/Travel");
    m_mapCategories[0xA2] = tr("Handicraft");
    m_mapCategories[0xA3] = tr("Motoring");
    m_mapCategories[0xA4] = tr("Fitness & Health");
    m_mapCategories[0xA5] = tr("Cooking");
    m_mapCategories[0xA6] = tr("Advertizement/Shopping");
    m_mapCategories[0xA7] = tr("Gardening");
    // Special
    m_mapCategories[0xB0] = tr("Original Language");
    m_mapCategories[0xB1] = tr("Black & White");
    m_mapCategories[0xB2] = tr("\"Unpublished\" Programmes");
    m_mapCategories[0xB3] = tr("Live Broadcast");
    // UK Freeview custom id
    m_mapCategories[0xF0] = tr("Drama");
}

void SIParser::PrintDescriptorStatistics(void) const
{
    if (!(print_verbose_messages & VB_SIPARSER))
        return;

    QMutexLocker locker(&descLock);
    VERBOSE(VB_SIPARSER, LOC + "Descriptor Stats -- begin");
    QMap<uint,uint>::const_iterator it = descCount.begin();
    for (;it != descCount.end(); ++it)
    {
        uint pid = it.key() >> 8;
        uint cnt = (*it);
        unsigned char sid = it.key() & 0xff;
        VERBOSE(VB_SIPARSER, LOC +
                QString("On PID 0x%1: Found %2, %3 Descriptor%4")
                .arg(pid,0,16).arg(cnt)
                .arg(MPEGDescriptor(&sid).DescriptorTagString())
                .arg((cnt>1) ? "s" : ""));
    }
    VERBOSE(VB_SIPARSER, LOC + "Descriptor Stats -- end");
}

/* vim: set sw=4 expandtab: */
