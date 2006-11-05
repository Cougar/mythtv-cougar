// -*- Mode: c++ -*-

#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbtypes.h"

static QString mod2str(fe_modulation mod);
static QString mod2dbstr(fe_modulation mod);
static QString coderate(fe_code_rate_t coderate);

bool equal_qpsk(const struct dvb_fe_params &p,
                const struct dvb_fe_params &op, uint range)
{
    return
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range   &&
        p.inversion          == op.inversion           &&
        p.u.qpsk.symbol_rate == op.u.qpsk.symbol_rate  &&
        p.u.qpsk.fec_inner   == op.u.qpsk.fec_inner;
}

#ifdef FE_GET_EXTENDED_INFO
bool equal_dvbs2(const struct dvb_frontend_parameters_new &p,
                const struct dvb_frontend_parameters_new &op, uint range)
{
    return
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range   &&
        p.inversion          == op.inversion           &&
        p.u.qpsk.symbol_rate == op.u.qpsk.symbol_rate  &&
        p.u.qpsk.fec_inner   == op.u.qpsk.fec_inner;
}
#endif

bool equal_atsc(const struct dvb_fe_params &p,
                const struct dvb_fe_params &op, uint range)
{
    bool ok =
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range;
#ifdef USE_ATSC
    ok &= (p.u.vsb.modulation == op.u.vsb.modulation);
#endif // USE_ATSC
    return ok;
}

bool equal_qam(const struct dvb_fe_params &p,
               const struct dvb_fe_params &op, uint range)
{
    return
        p.frequency + range  >= op.frequency            &&
        p.frequency          <= op.frequency + range    &&
        p.inversion          == op.inversion            &&
        p.u.qam.symbol_rate  == op.u.qam.symbol_rate    &&
        p.u.qam.fec_inner    == op.u.qam.fec_inner      &&
        p.u.qam.modulation   == op.u.qam.modulation;
}

bool equal_ofdm(const struct dvb_fe_params &p,
                const struct dvb_fe_params &op,
                uint range)
{
    return 
        p.frequency + range            >= op.frequency                &&
        p.frequency                    <= op.frequency + range        &&
        p.inversion                    == op.inversion                &&
        p.u.ofdm.bandwidth             == op.u.ofdm.bandwidth         &&
        p.u.ofdm.code_rate_HP          == op.u.ofdm.code_rate_HP      &&
        p.u.ofdm.code_rate_LP          == op.u.ofdm.code_rate_LP      &&
        p.u.ofdm.constellation         == op.u.ofdm.constellation     &&
        p.u.ofdm.guard_interval        == op.u.ofdm.guard_interval    &&
        p.u.ofdm.transmission_mode     == op.u.ofdm.transmission_mode &&
        p.u.ofdm.hierarchy_information == op.u.ofdm.hierarchy_information;
}

bool equal_type(const struct dvb_fe_params &p,
                const struct dvb_fe_params &op,
                fe_type_t type, uint freq_range)
{
    if (FE_QAM == type)
        return equal_qam(p, op, freq_range);
    if (FE_OFDM == type)
        return equal_ofdm(p, op, freq_range);
    if (FE_QPSK == type)
        return equal_qpsk(p, op, freq_range);
    if (FE_ATSC == type)
        return equal_atsc(p, op, freq_range);
#ifdef FE_GET_EXTENDED_INFO
    if (FE_DVB_S2 == type)
        return equal_dvbs2(p, op, freq_range);
#endif
    return false;
}

#define LOC_WARN QString("DVBTuning Warning: ")
#define LOC_ERR QString("DVBTuning Error: ")

char DVBTuning::InversionChar() const
{
    switch (params.inversion)
    {
    case INVERSION_ON:
        return '1';
    case INVERSION_OFF:
        return '0';
    default:
        return 'a';
    }
}

char DVBTuning::PolarityChar() const
{
    return polariz;
}

char DVBTuning::TransmissionModeChar() const
{
    switch (params.u.ofdm.transmission_mode)
    {
    case TRANSMISSION_MODE_2K:
        return '2';
    case TRANSMISSION_MODE_8K:
        return '8';
    default:
        return 'a';
    }
}

char DVBTuning::BandwidthChar() const
{
    switch (params.u.ofdm.bandwidth)
    {
       case BANDWIDTH_8_MHZ:
           return '8';
       case BANDWIDTH_7_MHZ:
           return '7';
       case BANDWIDTH_6_MHZ:
           return '6';
       default:
           return 'a';
    }
}

char DVBTuning::HierarchyChar() const
{
    switch (params.u.ofdm.hierarchy_information)
    {
       case HIERARCHY_NONE:
               return 'n';
       case HIERARCHY_1:
               return '1';
       case HIERARCHY_2:
               return '2';
       case HIERARCHY_4:
               return '4';
       default:
               return 'a';
    }
}

QString DVBTuning::ConstellationDB() const
{
    return mod2dbstr(params.u.ofdm.constellation);
}

QString DVBTuning::ModulationDB() const
{
    fe_modulation mod = QAM_AUTO;
#ifdef USE_ATSC
    mod = params.u.vsb.modulation;
#endif // USE_ATSC
    return mod2dbstr(mod);
}

QString DVBTuning::InversionString() const
{
    switch (params.inversion)
    {
        case INVERSION_ON:
            return "On";
        case INVERSION_OFF:
            return "Off";
        default:
            return "Auto";
    }
}

QString DVBTuning::ModulationString() const
{
    return mod2str(params.u.qam.modulation);
}

QString DVBTuning::ConstellationString() const
{
    return mod2str(params.u.ofdm.constellation);
}

QString DVBTuning::BandwidthString() const
{
    switch (params.u.ofdm.bandwidth)
    {
        case BANDWIDTH_AUTO:  return "Auto"; break;
        case BANDWIDTH_8_MHZ: return "8MHz"; break;
        case BANDWIDTH_7_MHZ: return "7MHz"; break;
        case BANDWIDTH_6_MHZ: return "6MHz"; break;
    }
    return "Unknown";
}

QString DVBTuning::TransmissionModeString() const
{
    //TransmissionMode
    switch (params.u.ofdm.transmission_mode)
    {
    case TRANSMISSION_MODE_2K:
        return "2K";
    case TRANSMISSION_MODE_8K:
        return "8K";
    default:
        return "Auto";
    }
}

QString DVBTuning::HPCodeRateString() const
{
    return coderate(params.u.ofdm.code_rate_HP);
}

QString DVBTuning::LPCodeRateString() const
{
    return coderate(params.u.ofdm.code_rate_LP);
}

QString DVBTuning::QAMInnerFECString() const
{
    return coderate(params.u.qam.fec_inner);
}

QString DVBTuning::QPSKInnerFECString() const
{
    return coderate(params.u.qpsk.fec_inner);
}

QString DVBTuning::GuardIntervalString() const
{
    //Guard Interval
    switch (params.u.ofdm.guard_interval)
    {
    case GUARD_INTERVAL_1_32:
        return "1/32";
    case GUARD_INTERVAL_1_16:
        return "1/16";
    case GUARD_INTERVAL_1_8:
        return "1/8";
    case GUARD_INTERVAL_1_4:
        return "1/4";
    default:
        return "auto";
    }
}

QString DVBTuning::HierarchyString() const
{
   // Hierarchy
    switch (params.u.ofdm.hierarchy_information)
    {
       case HIERARCHY_NONE:
               return "None";
       case HIERARCHY_1:
               return "1";
       case HIERARCHY_2:
               return "2";
       case HIERARCHY_4:
               return "4";
       default:
               return "Auto";
    }
}

QString DVBTuning::toString(fe_type_t type) const
{
    QString msg("");

    if (FE_QPSK == type)
    {
        msg = QString("Frequency: %1 Symbol Rate: %2 Pol: %3 Inv: %4")
            .arg(Frequency())
            .arg(QPSKSymbolRate())
            .arg(PolarityChar())
            .arg(InversionString());
    }
    else if (FE_QAM == type)
    {
        msg = QString("Frequency: %1 Symbol Rate: %2 Inversion: %3 Inner FEC: %4")
            .arg(Frequency())
            .arg(QAMSymbolRate())
            .arg(InversionString())
            .arg(QAMInnerFECString());
    }
    else if (FE_ATSC == type)
    {
        msg = QString("Frequency: %1 Modulation: %2")
            .arg(Frequency())
            .arg(ModulationString());
    }
    else if (FE_OFDM == type)
    {
        msg = QString("Frequency: %1 BW: %2 HP: %3 LP: %4"
                      "C: %5 TM: %6 H: %7 GI: %8")
            .arg(Frequency())
            .arg(BandwidthString())
            .arg(HPCodeRateString())
            .arg(LPCodeRateString())
            .arg(ConstellationString())
            .arg(TransmissionModeString())
            .arg(HierarchyString())
            .arg(GuardIntervalString());
    }
#ifdef FE_GET_EXTENDED_INFO
    else if (FE_DVB_S2 == type)
    {
        msg = QString("Frequency: %1 Symbol Rate: %2 Pol: %3 Inv: %4 Mod: %5")
            .arg(Frequency())
            .arg(QPSKSymbolRate())
            .arg(PolarityChar())
            .arg(InversionString())
            .arg(ModulationString());
    }
#endif
    return msg;
}

bool DVBTuning::FillFromDB(fe_type_t type, uint mplexid)
{
    MSqlQuery q(MSqlQuery::InitCon());

    // Query for our DVBTuning params
    q.prepare(
        "SELECT frequency,         inversion,      symbolrate, "
        "       fec,               polarity, "
        "       hp_code_rate,      lp_code_rate,   constellation, "
        "       transmission_mode, guard_interval, hierarchy, "
        "       modulation,        bandwidth "
        "FROM dtv_multiplex "
        "WHERE dtv_multiplex.mplexid = :MPLEXID");
    q.bindValue(":MPLEXID", mplexid);

    if (!q.exec() || !q.isActive())
    {
        MythContext::DBError("DVBTuning::FillFromDB", q);
        return false;
    }

    if (!q.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not find dvb tuning parameters for " +
                QString("mplex %1").arg(mplexid));

        return false;
    }

    // Parse the query into our DVBTuning class
    return ParseTuningParams(
        type,
        q.value(0).toString(),  q.value(1).toString(), q.value(2).toString(),
        q.value(3).toString(),  q.value(4).toString(),
        q.value(5).toString(),  q.value(6).toString(), q.value(7).toString(),
        q.value(8).toString(),  q.value(9).toString(), q.value(10).toString(),
        q.value(11).toString(), q.value(12).toString());
}

bool DVBTuning::ParseTuningParams(
    fe_type_t type,
    QString frequency,    QString inversion,      QString symbolrate,
    QString fec,          QString polarity,
    QString hp_code_rate, QString lp_code_rate,   QString constellation,
    QString trans_mode,   QString guard_interval, QString hierarchy,
    QString modulation,   QString bandwidth)
{
    if (FE_QPSK == type)
        return parseQPSK(
            frequency,       inversion,     symbolrate,   fec,   polarity);
    else if (FE_QAM == type)
        return parseQAM(
            frequency,       inversion,     symbolrate,   fec,   modulation);
    else if (FE_OFDM == type)
        return parseOFDM(
            frequency,       inversion,     bandwidth,    hp_code_rate,
            lp_code_rate,    constellation, trans_mode,   guard_interval,
            hierarchy);
    else if (FE_ATSC == type)
        return parseATSC(frequency, modulation);
#ifdef FE_GET_EXTENDED_INFO
    else if (FE_DVB_S2 == type)
        return parseDVBS2(
            frequency,       inversion,     symbolrate,   fec,   polarity,
            modulation);
#endif // FE_GET_EXTENDED_INFO
   
    return false;
}

bool DVBTuning::parseATSC(const QString& frequency, const QString modulation)
{
    (void) frequency;
    (void) modulation;

#ifdef USE_ATSC
    bool ok = false;
    params.frequency = frequency.toInt();
    dvb_vsb_parameters& p = params.u.vsb;

    p.modulation = parseModulation(modulation, ok);
    if (QAM_AUTO == p.modulation)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Invalid modulationulation parameter '%1', "
                        "falling back to '8-VSB'.").arg(modulation));

        p.modulation = VSB_8;
        ok = true;
    }
    return ok;
#else // if !USE_ATSC
    return false;
#endif //!USE_ATSC
}

bool DVBTuning::parseOFDM(const QString& frequency,   const QString& inversion,
                          const QString& bandwidth,   const QString& coderate_hp,
                          const QString& coderate_lp, const QString& constellation,
                          const QString& trans_mode,  const QString& guard_interval,
                          const QString& hierarchy)
{
    bool ok = true;

    dvb_ofdm_parameters& p = params.u.ofdm;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, aborting, falling back to 'auto'.");
    }

    p.bandwidth             = parseBandwidth(bandwidth,          ok);
    p.code_rate_HP          = parseCodeRate(coderate_hp,         ok);
    p.code_rate_LP          = parseCodeRate(coderate_lp,         ok);
    p.constellation         = parseModulation(constellation,     ok);
    p.transmission_mode     = parseTransmission(trans_mode,      ok);
    p.hierarchy_information = parseHierarchy(hierarchy,          ok);
    p.guard_interval        = parseGuardInterval(guard_interval, ok);

    return true;
}

#ifdef FE_GET_EXTENDED_INFO
bool DVBTuning::parseDVBS2(
    const QString& frequency,   const QString& inversion,
    const QString& symbol_rate, const QString& fec_inner,
    const QString& pol,         const QString& modulation)
{
    bool ok = true;

    dvb_dvbs2_parameters& p = params.u.qpsk2;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, aborting, falling back to 'auto'");

    p.symbol_rate = symbol_rate.toInt();
    if (!p.symbol_rate)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid symbol rate " +
                QString("parameter '%1', aborting.").arg(symbol_rate));
        return false;
    }

    if (!pol.isEmpty())
        polariz = QChar(pol[0]).lower();

    p.fec_inner = parseCodeRate(fec_inner, ok);

    p.modulation = parseModulation(modulation, ok);

    return true;
}
#endif

bool DVBTuning::parseQPSK(const QString& frequency,   const QString& inversion,
                          const QString& symbol_rate, const QString& fec_inner,
                          const QString& pol)
{
    bool ok = true;

    dvb_qpsk_parameters& p = params.u.qpsk;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, aborting, falling back to 'auto'");
    }

    p.symbol_rate = symbol_rate.toInt();
    if (!p.symbol_rate)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid symbol rate " +
                QString("parameter '%1', aborting.").arg(symbol_rate));

        return false;
    }

    if (!pol.isEmpty())
        polariz = QChar(pol[0]).lower();

    p.fec_inner = parseCodeRate(fec_inner, ok);

    return true;
}

bool DVBTuning::parseQAM(const QString& frequency, const QString& inversion,
                         const QString& symbol_rate, const QString& fec_inner,
                         const QString& modulation)
{
    bool ok = true;

    dvb_qam_parameters& p = params.u.qam;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, aborting, falling back to 'auto'");
    }

    p.symbol_rate = symbol_rate.toInt();
    if (!p.symbol_rate)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid symbol rate " +
                QString("parameter '%1', aborting.").arg(symbol_rate));

        return false;
    }

    p.fec_inner  = parseCodeRate(fec_inner, ok);
    p.modulation = parseModulation(modulation, ok);

    return true;
}

fe_bandwidth DVBTuning::parseBandwidth(const QString &bw, bool &ok)
{
    char bandwidth = QChar(bw[0]).lower();
    ok = true;
    switch (bandwidth)
    {
        case 'a': return BANDWIDTH_AUTO;
        case '8': return BANDWIDTH_8_MHZ;
        case '7': return BANDWIDTH_7_MHZ;
        case '6': return BANDWIDTH_6_MHZ;
    }
    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid bandwidth parameter '%1', "
                    "falling back to 'auto'.").arg(bandwidth));

    return BANDWIDTH_AUTO;
}

fe_guard_interval DVBTuning::parseGuardInterval(const QString &gi, bool &ok)
{
    QString guard_interval = gi.lower();
    ok = true;
    if (guard_interval == "auto")      return GUARD_INTERVAL_AUTO;
    else if (guard_interval == "1/4")  return GUARD_INTERVAL_1_4;
    else if (guard_interval == "1/8")  return GUARD_INTERVAL_1_8;
    else if (guard_interval == "1/16") return GUARD_INTERVAL_1_16;
    else if (guard_interval == "1/32") return GUARD_INTERVAL_1_32;

    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid guard interval parameter '%1', "
                    "falling back to 'auto'.").arg(gi));

    return GUARD_INTERVAL_AUTO;
}

fe_transmit_mode DVBTuning::parseTransmission(const QString &tm, bool &ok)
{
    char transmission_mode = QChar(tm[0]).lower();
    ok = true;
    switch (transmission_mode)
    {
        case 'a': return TRANSMISSION_MODE_AUTO;
        case '2': return TRANSMISSION_MODE_2K;
        case '8': return TRANSMISSION_MODE_8K;
    }
    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid transmission mode parameter '%1', "
                    "falling back to 'auto'.").arg(tm));

    return TRANSMISSION_MODE_AUTO;
}

fe_hierarchy DVBTuning::parseHierarchy(const QString &hier, bool &ok)
{
    char hierarchy = QChar(hier[0]).lower();
    ok = true;
    switch (hierarchy)
    {
        case 'a': return HIERARCHY_AUTO;
        case 'n': return HIERARCHY_NONE;
        case '1': return HIERARCHY_1;
        case '2': return HIERARCHY_2;
        case '4': return HIERARCHY_4;
    }
    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid hierarchy parameter '%1', "
                    "falling back to 'auto'.").arg(hier));

    return HIERARCHY_AUTO;
}

fe_spectral_inversion DVBTuning::parseInversion(const QString &inv, bool &ok)
{
    char inversion = QChar(inv[0]).lower();
    ok = true;
    switch (inversion)
    {
        case '1': return INVERSION_ON;
        case '0': return INVERSION_OFF;
        case 'a': return INVERSION_AUTO;
    }
    ok = false;
    return INVERSION_AUTO;
}

fe_code_rate DVBTuning::parseCodeRate(const QString &cr, bool &ok)
{
    QString code_rate = cr.lower();
    ok = true;
    if (   code_rate == "none") return FEC_NONE;
    else if (code_rate == "auto") return FEC_AUTO;
    else if (code_rate ==  "8/9") return FEC_8_9;
    else if (code_rate ==  "7/8") return FEC_7_8;
    else if (code_rate ==  "6/7") return FEC_6_7;
    else if (code_rate ==  "5/6") return FEC_5_6;
    else if (code_rate ==  "4/5") return FEC_4_5;
    else if (code_rate ==  "3/4") return FEC_3_4;
    else if (code_rate ==  "2/3") return FEC_2_3;
    else if (code_rate ==  "1/2") return FEC_1_2;

    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid code rate parameter '%1', "
                    "falling back to 'auto'.").arg(cr));

    return FEC_AUTO;
}

fe_modulation DVBTuning::parseModulation(const QString &mod, bool &ok)
{
    QString modulation = mod.lower();
    ok = true;
    if (   modulation ==    "qpsk") return QPSK; 
    else if (modulation ==    "auto") return QAM_AUTO;
    else if (modulation ==       "a") return QAM_AUTO;
    else if (modulation =="qam_auto") return QAM_AUTO;
    else if (modulation == "qam_256") return QAM_256;
    else if (modulation == "qam_128") return QAM_128;
    else if (modulation ==  "qam_64") return QAM_64;
    else if (modulation ==  "qam_32") return QAM_32;
    else if (modulation ==  "qam_16") return QAM_16;
    else if (modulation ==    "8vsb") return VSB_8;
    else if (modulation ==   "16vsb") return VSB_16;
    else if (modulation == "qam-256") return QAM_256;
    else if (modulation == "qam-128") return QAM_128;
    else if (modulation ==  "qam-64") return QAM_64;
    else if (modulation ==  "qam-32") return QAM_32;
    else if (modulation ==  "qam-16") return QAM_16;
    else if (modulation ==   "8-vsb") return VSB_8;
    else if (modulation ==  "16-vsb") return VSB_16;
#ifdef FE_GET_EXTENDED_INFO
    else if (modulation ==    "8psk") return MOD_8PSK;
#endif
    ok = false;

    VERBOSE(VB_GENERAL, LOC_WARN +
            QString("Invalid constellation/modulation parameter '%1', "
                    "falling back to 'auto'.").arg(mod));

    return QAM_AUTO;
}
#undef LOC_WARN
#undef LOC_ERR

static QString mod2str(fe_modulation mod)
{
    switch (mod)
    {
        case QPSK:     return    "QPSK";
        case QAM_AUTO: return    "Auto";
        case QAM_256:  return "QAM-256";
        case QAM_128:  return "QAM-128";
        case QAM_64:   return  "QAM-64";
        case QAM_32:   return  "QAM-32";
        case QAM_16:   return  "QAM-16";
        case VSB_8:    return   "8-VSB";
        case VSB_16:   return  "16-VSB";
#ifdef FE_GET_EXTENDED_INFO
	case MOD_8PSK: return    "8PSK";
#endif
        default:      return "auto";
    }
    return "Unknown";
}

static QString mod2dbstr(fe_modulation mod)
{
    switch (mod)
    {
        case QPSK:     return "qpsk";
        case QAM_AUTO: return "auto";
        case QAM_16:   return "qam_16";
        case QAM_32:   return "qam_32";
        case QAM_64:   return "qam_64";
        case QAM_128:  return "qam_128";
        case QAM_256:  return "qam_256";
        case VSB_8:    return "8vsb";
        case VSB_16:   return "16vsb";
#ifdef FE_GET_EXTENDED_INFO
	case MOD_8PSK: return "8psk";
#endif
        default:       return "auto";
    }
}

static QString coderate(fe_code_rate_t coderate)
{
    switch (coderate)
    {
        case FEC_NONE: return "none";
        case FEC_1_2:  return "1/2";
        case FEC_2_3:  return "2/3";
        case FEC_3_4:  return "3/4";
        case FEC_4_5:  return "4/5";
        case FEC_5_6:  return "5/6";
        case FEC_6_7:  return "6/7";
        case FEC_7_8:  return "7/8";
        case FEC_8_9:  return "8/9";
        default:       return "auto";
    }
}

QString toString(const fe_type_t type)
{
    if (FE_QPSK == type)
        return "QPSK";
    else if (FE_QAM == type)
        return "QAM";
    else if (FE_OFDM == type)
        return "OFDM";
    else if (FE_ATSC == type)
        return "ATSC";
#ifdef FE_GET_EXTENDED_INFO
    else if (FE_DVB_S2 == type)
        return "DVB_S2";
#endif
    return "Unknown";
}

QString toString(const struct dvb_fe_params &params,
                 const fe_type_t type)
{
    return QString("freq(%1) type(%2)")
        .arg(params.frequency).arg(toString(type));
}

QString toString(fe_status status)
{
    QString str("");
    if (FE_HAS_SIGNAL  & status) str += "Signal,";
    if (FE_HAS_CARRIER & status) str += "Carrier,";
    if (FE_HAS_VITERBI & status) str += "FEC Stable,";
    if (FE_HAS_SYNC    & status) str += "Sync,";
    if (FE_HAS_LOCK    & status) str += "Lock,";
    if (FE_TIMEDOUT    & status) str += "Timed Out,";
    if (FE_REINIT      & status) str += "Reinit,";
    return str;
}

QString toString(const struct dvb_frontend_event &event,
                 const fe_type_t type)
{
    const struct dvb_fe_params *params;
    params = (const struct dvb_fe_params *)(&event.parameters);
    return QString("Event status(%1) %2")
        .arg(toString(event.status))
        .arg(toString(*params, type));
}
