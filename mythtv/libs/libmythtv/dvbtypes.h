/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBTYPES_H
#define DVBTYPES_H

// POSIX headers
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdint.h>
#include <unistd.h>

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <qdatetime.h>
#include <qstringlist.h>
#include <qmutex.h>

#include <linux/dvb/version.h>
#if (DVB_API_VERSION != 3)
#error "DVB driver includes with API version 3 not found!"
#endif

#ifndef DVB_API_VERSION_MINOR
#define DVB_API_VERSION_MINOR 0
#endif

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#if (DVB_API_VERSION >= 3 && DVB_API_VERSION_MINOR >= 1)
#    define USE_ATSC
#else
#warning DVB API version < 3.1
#warning ATSC will not be supported using the Linux DVB drivers
#    define FE_ATSC       (FE_OFDM+1)
#    define FE_CAN_8VSB   0x200000
#    define FE_CAN_16VSB  0x400000
#    define VSB_8         (fe_modulation)(QAM_AUTO+1)
#    define VSB_16        (fe_modulation)(QAM_AUTO+2)
#endif

#include "mpegdescriptors.h"
#include "mpegtables.h"

#define MPEG_TS_PKT_SIZE 188
#define DEF_DMX_BUF_SIZE  64 * 1024
#define MAX_SECTION_SIZE 4096
#define DMX_DONT_FILTER 0x2000

#ifdef FE_GET_EXTENDED_INFO
  #define dvb_fe_params dvb_frontend_parameters_new
#else
  #define dvb_fe_params dvb_frontend_parameters
#endif

QString toString(const fe_type_t);
QString toString(const struct dvb_fe_params&, const fe_type_t);
QString toString(fe_status);
QString toString(const struct dvb_frontend_event&, const fe_type_t);


typedef vector<uint16_t> dvb_pid_t;
// needs to add provider id so dvbcam doesnt require parsing
// of the pmt and or the pmtcache
typedef vector<uint16_t> dvb_caid_t;

extern bool equal_qpsk(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
#ifdef FE_GET_EXTENDED_INFO
extern bool equal_dvbs2(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
#endif
extern bool equal_atsc(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_qam(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_ofdm(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_type(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op,
    fe_type_t type, uint freq_range);

class DVBTuning
{
  public:
    DVBTuning() : polariz('v')
    {
        bzero(&params, sizeof(dvb_fe_params));
    }

    struct dvb_fe_params params;
    char                 polariz;

    bool equalQPSK(const DVBTuning& other, uint range = 0) const
        { return equal_qpsk(params, other.params, range);  }
    bool equalATSC(const DVBTuning& other, uint range = 0) const
        { return equal_atsc(params, other.params, range);  }
    bool equalQAM( const DVBTuning& other, uint range = 0) const
        { return equal_qam(params, other.params, range);   }
    bool equalOFDM(const DVBTuning& other, uint range = 0) const
        { return equal_ofdm(params, other.params, range);  }
    bool equal(fe_type_t type, const DVBTuning& other, uint range = 0) const
        { return equal_type(params, other.params, type, range); }

    // Helper functions to get the paramaters as DB friendly strings
    char InversionChar() const;
    char PolarityChar() const;
    char TransmissionModeChar() const;
    char BandwidthChar() const;
    char HierarchyChar() const;
    QString ConstellationDB() const;
    QString ModulationDB() const;

    // Helper functions to parse params from DB friendly strings
    static fe_bandwidth      parseBandwidth(    const QString&, bool &ok);
    static fe_guard_interval parseGuardInterval(const QString&, bool &ok);
    static fe_transmit_mode  parseTransmission( const QString&, bool &ok);
    static fe_hierarchy      parseHierarchy(    const QString&, bool &ok);
    static fe_spectral_inversion parseInversion(const QString&, bool &ok);
    static fe_code_rate      parseCodeRate(     const QString&, bool &ok);
    static fe_modulation     parseModulation(   const QString&, bool &ok);

    // Helper functions for UI and DB
    uint Frequency()      const { return params.frequency; }
    uint QPSKSymbolRate() const { return params.u.qpsk.symbol_rate; }
    uint QAMSymbolRate()  const { return params.u.qam.symbol_rate; }
    QString GuardIntervalString() const;
    QString InversionString() const;
    QString BandwidthString() const;
    QString TransmissionModeString() const;
    QString HPCodeRateString() const;
    QString LPCodeRateString() const;
    QString QAMInnerFECString() const;
    QString QPSKInnerFECString() const;
    QString ModulationString() const;
    QString ConstellationString() const;
    QString HierarchyString() const;
    QString toString(fe_type_t type) const;

    bool FillFromDB(fe_type_t type, uint mplexid);

    bool parseATSC(const QString& frequency,      const QString modulation);

    bool parseOFDM(const QString& frequency,      const QString& inversion,
                   const QString& bandwidth,      const QString& coderate_hp,
                   const QString& coderate_lp,    const QString& constellation,
                   const QString& trans_mode,     const QString& guard_interval,
                   const QString& hierarchy);

    bool parseQPSK(const QString& frequency,      const QString& inversion,
                   const QString& symbol_rate,    const QString& fec_inner,
                   const QString& pol);

    bool parseQAM(const QString& frequency,       const QString& inversion,
                  const QString& symbol_rate,     const QString& fec_inner,
                  const QString& modulation);

#ifdef FE_GET_EXTENDED_INFO
    bool equalDVBS2(const DVBTuning& other, uint range = 0) const
        { return equal_dvbs2(params, other.params, range);  }
    uint DVBS2SymbolRate() const { return params.u.qpsk2.symbol_rate; }
    bool parseDVBS2(const QString& frequency,      const QString& inversion,
                    const QString& symbol_rate,    const QString& fec_inner,
                    const QString& pol,            const QString& modulation);
#endif

  private:
    bool ParseTuningParams(
        fe_type_t type,
        QString frequency,    QString inversion,      QString symbolrate,
        QString fec,          QString polarity,
        QString hp_code_rate, QString lp_code_rate,   QString constellation,
        QString trans_mode,   QString guard_interval, QString hierarchy,
        QString modulation,   QString bandwidth);
};

#endif // DVB_TYPES_H
