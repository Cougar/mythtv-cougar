#include "frequencytables.h"
#include "channelutil.h"

freq_table_map_t frequencies;

static void init_freq_tables(freq_table_map_t&);

TransportScanItem::TransportScanItem()
    : mplexid(-1),        standard("dvb"),      FriendlyName(""),
      friendlyNum(0),     SourceID(0),          UseTimer(false),
      scanning(false),    timeoutTune(1000)
{ 
    bzero(freq_offsets, sizeof(int)*3);

#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));
#else
    frequency  = 0;
    modulation = 0;
#endif
}

TransportScanItem::TransportScanItem(int sourceid,
                                     const QString &std,
                                     const QString &fn,
                                     int _mplexid,
                                     uint tuneTO)
    : mplexid(_mplexid),  standard(std),        FriendlyName(fn),
      friendlyNum(0),     SourceID(sourceid),   UseTimer(false),
      scanning(false),    timeoutTune(tuneTO)
{
    bzero(freq_offsets, sizeof(int)*3);

#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));
#else
    frequency  = 0;
    modulation = 0;
#endif
}

#ifdef USING_DVB
TransportScanItem::TransportScanItem(int            _sourceid,
                                     const QString &_std,
                                     const QString &_name,
                                     DVBTuning     &_tuning,
                                     uint           _timeoutTune)
    : mplexid(-1),         standard(_std),
      FriendlyName(_name), friendlyNum(0),
      SourceID(_sourceid), UseTimer(false),
      scanning(false),     timeoutTune(_timeoutTune)
{
    bzero(freq_offsets, sizeof(int) * 3);
    tuning = _tuning;
}
#endif // USING_DVB


TransportScanItem::TransportScanItem(int                 _sourceid,
                                     const QString      &_std,
                                     const QString      &_name,
                                     const QString      &_cardtype,
                                     const DTVTransport &_tuning,
                                     uint                _timeoutTune)
    : mplexid(-1),         standard(_std),
      FriendlyName(_name), friendlyNum(0),
      SourceID(_sourceid), UseTimer(false),
      scanning(false),     timeoutTune(_timeoutTune)
{
    (void) _cardtype;

    bzero(freq_offsets, sizeof(int) * 3);
    expectedChannels = _tuning.channels;

#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));

    fe_type type = FE_QPSK;

    type = (_cardtype.upper() == "QAM")    ? FE_QAM    : type;
    type = (_cardtype.upper() == "OFDM")   ? FE_OFDM   : type;
    type = (_cardtype.upper() == "ATSC")   ? FE_ATSC   : type;
#ifdef FE_GET_EXTENDED_INFO
    type = (_cardtype.upper() == "DVB_S2") ? FE_DVB_S2 : type;
#endif

    tuning.ParseTuningParams(
        type,
        QString::number(_tuning.frequency),  _tuning.inversion.toString(),
        QString::number(_tuning.symbolrate), _tuning.fec.toString(),
        _tuning.polarity.toString(),         _tuning.hp_code_rate.toString(),
        _tuning.lp_code_rate.toString(),     _tuning.constellation.toString(),
        _tuning.trans_mode.toString(),       _tuning.guard_interval.toString(),
        _tuning.hierarchy.toString(),        _tuning.modulation.toString(),
        _tuning.bandwidth.toString());

#else
    frequency  = _tuning.frequency;
    modulation = _tuning.modulation;
#endif
}

TransportScanItem::TransportScanItem(int sourceid,
                                     const QString &std,
                                     const QString &fn,
                                     uint fnum,
                                     uint freq,
                                     const FrequencyTable &ft,
                                     uint tuneTO)
    : mplexid(-1),        standard(std),        FriendlyName(fn),
      friendlyNum(fnum),  SourceID(sourceid),   UseTimer(false),
      scanning(false),    timeoutTune(tuneTO)
{
    bzero(freq_offsets, sizeof(int)*3);
#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));

    // setup tuning params
    tuning.params.frequency                    = freq;
    const DVBFrequencyTable *dvbft =
        dynamic_cast<const DVBFrequencyTable*>(&ft);
    if (standard == "dvb" && dvbft)
    {
        tuning.params.inversion                    = dvbft->inversion;
        freq_offsets[1]                            = dvbft->offset1;
        freq_offsets[2]                            = dvbft->offset2;
        tuning.params.u.ofdm.bandwidth             = dvbft->bandwidth;
        tuning.params.u.ofdm.code_rate_HP          = dvbft->coderate_hp;
        tuning.params.u.ofdm.code_rate_LP          = dvbft->coderate_lp;
        tuning.params.u.ofdm.constellation         = dvbft->constellation;
        tuning.params.u.ofdm.transmission_mode     = dvbft->trans_mode;
        tuning.params.u.ofdm.guard_interval        = dvbft->guard_interval;
        tuning.params.u.ofdm.hierarchy_information = dvbft->hierarchy;
    }
    else if (standard == "atsc")
    {
#if (DVB_API_VERSION_MINOR == 1)
        tuning.params.u.vsb.modulation = (fe_modulation) ft.modulation;
#endif
        if (dvbft)
        {
            freq_offsets[1]                        = dvbft->offset1;
            freq_offsets[2]                        = dvbft->offset2;
        }
    }
#else
    frequency  = freq;
    modulation = ft.modulation;
#endif // USING_DVB
    mplexid = GetMultiplexIdFromDB();
}

/** \fn TransportScanItem::GetMultiplexIdFromDB() const
 *  \brief Fetches mplexid if it exists, based on the frequency and sourceid
 */
int TransportScanItem::GetMultiplexIdFromDB() const
{
    int mplexid = -1;

    for (uint i = 0; (i < offset_cnt()) && (mplexid <= 0); i++)
        mplexid = ChannelUtil::GetMplexID(SourceID, freq_offset(i));

    return mplexid;
}

uint TransportScanItem::freq_offset(uint i) const
{
#ifdef USING_DVB
    int freq = (int) tuning.params.frequency;
#else
    int freq = (int) frequency;
#endif

    return (uint) (freq + freq_offsets[i]);
}

QString TransportScanItem::ModulationDB(void) const
{
#ifdef USING_DVB
    return tuning.ModulationDB();
#else
    switch (modulation)
    {
//        case QPSK:     return "qpsk";
        case QAM_AUTO: return "auto";
        case QAM_16:   return "qam_16";
        case QAM_32:   return "qam_32";
        case QAM_64:   return "qam_64";
        case QAM_128:  return "qam_128";
        case QAM_256:  return "qam_256";
        case VSB_8:    return "8vsb";
        case VSB_16:   return "16vsb";
        default:       return "auto";
    }
#endif
}


QString TransportScanItem::toString() const
{
    QString str = QString("Transport Scan Item '%1' #%2\n")
        .arg(FriendlyName).arg(friendlyNum);
    str += QString("\tmplexid(%1) standard(%2) sourceid(%3)\n")
        .arg(mplexid).arg(standard).arg(SourceID);
    str += QString("\tUseTimer(%1) scanning(%2)\n")
        .arg(UseTimer).arg(scanning);
    str += QString("\ttimeoutTune(%3 msec)\n").arg(timeoutTune);
#ifdef USING_DVB
#if (DVB_API_VERSION_MINOR == 1)
    if (standard == "atsc")
    {
        str += QString("\tfrequency(%1) modulation(%2)\n")
            .arg(tuning.params.frequency)
            .arg(tuning.params.u.vsb.modulation);
    }
    else
#endif // (DVB_API_VERSION_MINOR == 1)
    {
        str += QString("\tfrequency(%1) constellation(%2)\n")
            .arg(tuning.params.frequency)
            .arg(tuning.params.u.ofdm.constellation);
        str += QString("\t  inv(%1) bandwidth(%2) hp(%3) lp(%4)\n")
            .arg(tuning.params.inversion)
            .arg(tuning.params.u.ofdm.bandwidth)
            .arg(tuning.params.u.ofdm.code_rate_HP)
            .arg(tuning.params.u.ofdm.code_rate_LP);
        str += QString("\t  trans_mode(%1) guard_int(%2) hierarchy(%3)\n")
            .arg(tuning.params.u.ofdm.transmission_mode)
            .arg(tuning.params.u.ofdm.guard_interval)
            .arg(tuning.params.u.ofdm.hierarchy_information);
    }
#else
    str += QString("\tfrequency(%1) modulation(%2)")
        .arg(frequency).arg(modulation);
#endif // USING_DVB
    str += QString("\t offset[0..2]: %1 %2 %3")
        .arg(freq_offsets[0]).arg(freq_offsets[1]).arg(freq_offsets[2]);
    return str;
}

bool init_freq_tables()
{
    static bool statics_initialized = false;
    static QMutex statics_lock;
    statics_lock.lock();
    if (!statics_initialized)
    {
        init_freq_tables(frequencies);
        statics_initialized = true;
    }
    statics_lock.unlock();

    return true;
}
bool just_here_to_force_init = init_freq_tables();

freq_table_list_t get_matching_freq_tables(
    QString format, QString modulation, QString country)
{
    const freq_table_map_t &fmap = frequencies;

    freq_table_list_t list;

    QString lookup = QString("%1_%2_%3%4")
        .arg(format).arg(modulation).arg(country);

    freq_table_map_t::const_iterator it = fmap.begin();
    for (uint i = 0; it != fmap.end(); i++)
    {
        it = fmap.find(lookup.arg(i));
        if (it != fmap.end())
            list.push_back(*it);
    }

    return list;
}

long long get_center_frequency(
    QString format, QString modulation, QString country, int freqid)
{
    freq_table_list_t list =
        get_matching_freq_tables(format, modulation, country);

    for (uint i = 0; i < list.size(); ++i)
    {
        int min_freqid = list[i]->name_offset;
        int max_freqid = min_freqid +
            ((list[i]->frequencyEnd - list[i]->frequencyStart) /
             list[i]->frequencyStep);

        if ((min_freqid <= freqid) && (freqid <= max_freqid))
            return list[i]->frequencyStart +
                list[i]->frequencyStep * (freqid - min_freqid);
    }
    return -1;
}

int get_closest_freqid(
    QString format, QString modulation, QString country, long long centerfreq)
{
    modulation = (modulation == "8vsb") ? "vsb8" : modulation;

    freq_table_list_t list =
        get_matching_freq_tables(format, modulation, country);
    
    for (uint i = 0; i < list.size(); ++i)
    {
        int min_freqid = list[i]->name_offset;
        int max_freqid = min_freqid +
            ((list[i]->frequencyEnd - list[i]->frequencyStart) /
             list[i]->frequencyStep);
        int freqid =
            ((centerfreq - list[i]->frequencyStart) /
             list[i]->frequencyStep) + min_freqid;

        if ((min_freqid <= freqid) && (freqid <= max_freqid))
            return freqid;
    }
    //VERBOSE(VB_IMPORTANT, "get_closest_freqid("<<format<<", "
    //        <<modulation<<", "<<country<<", "<<centerfreq
    //        <<") Failed sz("<<list.size()<<")");
    return -1;
}


static void init_freq_tables(freq_table_map_t &fmap)
{
#ifdef USING_DVB
    // United Kingdom
    fmap["dvbt_ofdm_uk0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "" , 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_2K,
        GUARD_INTERVAL_1_32, HIERARCHY_NONE, QAM_AUTO, 166670, -166670);

    // Finland
    fmap["dvbt_ofdm_fi0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 0, 0);

    // Sweden
    fmap["dvbt_ofdm_se0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 0, 0);

    // Australia
    fmap["dvbt_ofdm_au0"] = new DVBFrequencyTable(
        177500000, 226500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // VHF 6-12
    fmap["dvbt_ofdm_au1"] = new DVBFrequencyTable(
        529500000, 816500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 28-69

    // Germany (Deuschland)
    fmap["dvbt_ofdm_de0"] = new DVBFrequencyTable(
        177500000, 226500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // VHF 6-12
    fmap["dvbt_ofdm_de1"] = new DVBFrequencyTable(
        474000000, 826000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 21-65

    // Spain
    fmap["dvbt_ofdm_es0"] = new DVBFrequencyTable(
        474000000, 858000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 21-69
#endif // USING_DVB

//#define DEBUG_DVB_OFFSETS
#ifdef DEBUG_DVB_OFFSETS
    // UHF 14-69
    fmap["atsc_vsb8_us0"] = new DVBFrequencyTable(
        533000000, 803000000, 6000000, "xATSC Channel %1", 24, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, VSB_8, -100000, 100000);
#else
    // USA Terrestrial (center frequency, subtract 1.75 Mhz for visual carrier)
    // VHF 2-4
    fmap["atsc_vsb8_us0"] = new FrequencyTable(
        "ATSC Channel %1",  2,  57000000,  69000000, 6000000, VSB_8);
    // VHF 5-6
    fmap["atsc_vsb8_us1"] = new FrequencyTable(
        "ATSC Channel %1",  5,  79000000,  85000000, 6000000, VSB_8);
    // VHF 7-13
    fmap["atsc_vsb8_us2"] = new FrequencyTable(
        "ATSC Channel %1",  7, 177000000, 213000000, 6000000, VSB_8);
    // UHF 14-69
    fmap["atsc_vsb8_us3"] = new FrequencyTable(
        "ATSC Channel %1", 14, 473000000, 803000000, 6000000, VSB_8);
    // UHF 70-83
    fmap["atsc_vsb8_us4"] = new FrequencyTable(
        "ATSC Channel %1", 70, 809000000, 887000000, 6000000, VSB_8);
#endif // USING_DVB

    QString modStr[] = { "vsb8",  "qam256",   "qam128",   "qam64",   };
    uint    mod[]    = { VSB_8,    QAM_256,    QAM_128,    QAM_64,   };
    QString desc[]   = { "ATSC ", "QAM-256 ", "QAM-128 ", "QAM-64 ", };

#define FREQ(A,B, C,D, E,F,G, H, I) \
    fmap[QString("atsc_%1_us%2").arg(A).arg(B)] = \
        new FrequencyTable(C+D, E, F, G, H, I);

    for (uint i = 0; i < 4; i++)
    {
        // USA Cable, ch 2 to 159 and T.7 to T.14
        FREQ(modStr[i], "cable0", desc[i], "Channel %1",
             2,    57000000,   69000000, 6000000, mod[i]); // 2-4
        FREQ(modStr[i], "cable1", desc[i], "Channel %1",
             5,    79000000,   85000000, 6000000, mod[i]); // 5-6
        FREQ(modStr[i], "cable2", desc[i], "Channel %1",
             7,   177000000,  213000000, 6000000, mod[i]); // 7-13
        FREQ(modStr[i], "cable3", desc[i], "Channel %1",
             14,  123000000,  171000000, 6000000, mod[i]); // 14-22
        FREQ(modStr[i], "cable4", desc[i], "Channel %1",
             23,  219000000,  645000000, 6000000, mod[i]); // 23-94
        FREQ(modStr[i], "cable5", desc[i], "Channel %1",
             95,   93000000,  117000000, 6000000, mod[i]); // 95-99
        // The center frequency of any EIA-542 std cable channel over 99 is
        // Frequency_MHz = ( 6 * ( 8 + channel_designation ) ) + 3
        FREQ(modStr[i], "cable6", desc[i], "Channel %1",
             100, 651000000, 1005000000, 6000000, mod[i]); // 100-159
        FREQ(modStr[i], "cable7", desc[i], "Channel T-%1",
             7,    8750000,   50750000, 6000000, mod[i]); // T7-14

        // USA Cable, QAM 256 ch 78 to 159
        FREQ(modStr[i], "cablehigh0", desc[i], "Channel %1",
             78,  549000000,  645000000, 6000000, mod[i]); // 78-94
        FREQ(modStr[i], "cablehigh1", desc[i], "Channel %1",
             100, 651000000, 1005000000, 6000000, mod[i]); // 100-159

        // USA Cable HRC, ch 1 to 125
        FREQ(modStr[i], "hrc0", desc[i], "HRC %1",
             1,    73753600,  73753601, 6000300, mod[i]); // 1
        FREQ(modStr[i], "hrc1", desc[i], "HRC %1",
             2,    55752700,  67753300, 6000300, mod[i]); // 2-4
        FREQ(modStr[i], "hrc2", desc[i], "HRC %1",
             5,    79753900,  85754200, 6000300, mod[i]); // 5-6
        FREQ(modStr[i], "hrc3", desc[i], "HRC %1",
             7,   175758700, 211760500, 6000300, mod[i]); // 7-13
        FREQ(modStr[i], "hrc4", desc[i], "HRC %1",
             14,  121756000, 169758400, 6000300, mod[i]); // 14-22
        FREQ(modStr[i], "hrc5", desc[i], "HRC %1",
             23,  217760800, 643782100, 6000300, mod[i]); // 23-94
        FREQ(modStr[i], "hrc6", desc[i], "HRC %1",
             95,   91754500, 115755700, 6000300, mod[i]); // 95-99
        // The center frequency of any EIA-542 HRC cable channel over 99 is
        // Frequency_MHz = ( 6.0003 * ( 8 + channel_designation ) ) + 1.75
        FREQ(modStr[i], "hrc7", desc[i], "HRC %1",
             100, 649782400, 799789900, 6000300, mod[i]); // 100-125

        // USA Cable HRC, ch 76-94 and 100-125
        // Channels 95-99 are low frequency despite high channel numbers
        FREQ(modStr[i], "hrchigh0", desc[i], "HRC %1",
             76,  535776700, 643782100, 6000300, mod[i]); // 76-94
        FREQ(modStr[i], "hrchigh1", desc[i], "HRC %1",
             100, 649782400, 799789900, 6000300, mod[i]); // 100-125

        // USA Cable IRC, ch 1 to 125
        FREQ(modStr[i], "irc0", desc[i], "IRC %1",
             1,    75012500,  75012501, 6000000, mod[i]); // 1
        FREQ(modStr[i], "irc1", desc[i], "IRC %1",
             2,    57012500,  69012500, 6000000, mod[i]); // 2-4
        FREQ(modStr[i], "irc2", desc[i], "IRC %1",
             5,    81012500,  87012500, 6000000, mod[i]); // 5-6
        FREQ(modStr[i], "irc3", desc[i], "IRC %1",
             7,   177012500, 213012500, 6000000, mod[i]); // 7-13
        FREQ(modStr[i], "irc4", desc[i], "IRC %1",
             14,  123012500, 171012500, 6000000, mod[i]); // 14-22
        FREQ(modStr[i], "irc5", desc[i], "IRC %1",
             23,  219012500, 327012500, 6000000, mod[i]); // 23-41
        FREQ(modStr[i], "irc6", desc[i], "IRC %1",
             42,  333025000, 333025001, 6000000, mod[i]); // 42
        FREQ(modStr[i], "irc7", desc[i], "IRC %1",
             43,  339012500, 645012500, 6000000, mod[i]); // 43-94
        FREQ(modStr[i], "irc8", desc[i], "IRC %1",
             95,   93012500, 105012500, 6000000, mod[i]); // 95-97
        FREQ(modStr[i], "irc9", desc[i], "IRC %1",
             98,  111025000, 117025000, 6000000, mod[i]); // 98-99
        // The center frequency of any EIA-542 IRC cable channel over 99 is
        // Frequency_MHz = ( 6 * ( 8 + channel_designation ) ) + 3.0125
        FREQ(modStr[i], "irc10", desc[i], "IRC %1",
             100, 651012500, 801012500, 6000000, mod[i]); // 100-125

        // USA Cable IRC, ch 76-94 and 100-125
        // Channels 95-99 are low frequency despite high channel numbers
        FREQ(modStr[i], "irchigh0", desc[i], "IRC %1",
             76,  537012500, 645012500, 6000000, mod[i]); // 76-94
        FREQ(modStr[i], "irchigh1", desc[i], "IRC %1",
             100, 651012500, 801012500, 6000000, mod[i]); // 100-125
    }
}
