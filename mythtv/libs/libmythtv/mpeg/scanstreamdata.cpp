// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "scanstreamdata.h"
#include "atsctables.h"
#include "dvbtables.h"

ScanStreamData::ScanStreamData()
    : ATSCStreamData(-1,-1, true), dvb(true)
{
    setName("ScanStreamData");

    // MPEG
    connect(&dvb, SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
            SLOT(RelayPAT(const ProgramAssociationTable*)));
    connect(&dvb, SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
            SLOT(RelayPMT(uint, const ProgramMapTable*)));

    // DVB
    connect(&dvb, SIGNAL(UpdateNIT(const NetworkInformationTable*)),
             SLOT(RelayNIT(const NetworkInformationTable*)));
    connect(&dvb, SIGNAL(UpdateSDT(uint, const ServiceDescriptionTable*)),
             SLOT(RelaySDT(uint, const ServiceDescriptionTable*)));
}

ScanStreamData::~ScanStreamData() { ; }

/** \fn ScanStreamData::IsRedundant(uint,const PSIPTable&) const
 *  \brief Returns true if table already seen.
 */
bool ScanStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
    return ATSCStreamData::IsRedundant(pid,psip) || dvb.IsRedundant(pid,psip);
}

/** \fn ScanStreamData::HandleTables(uint, const PSIPTable&)
 *  \brief Processes PSIP tables
 */
bool ScanStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    bool h0 = ATSCStreamData::HandleTables(pid, psip);
    bool h1 = dvb.HandleTables(pid, psip);
    return h0 || h1;
}

void ScanStreamData::Reset(void)
{
    ATSCStreamData::Reset(-1,-1);
    dvb.Reset();

    AddListeningPID(MPEG_PAT_PID);
    AddListeningPID(ATSC_PSIP_PID);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
}

QMap<uint, bool> ScanStreamData::ListeningPIDs(void) const
{
    QMap<uint, bool> a = ATSCStreamData::ListeningPIDs();
    QMap<uint, bool> b = dvb.ListeningPIDs();

    QMap<uint, bool>::const_iterator it = a.begin();
    for (; it != a.end(); ++it)
    {
        if (*it)
            b.remove(it.key());
    }

    for (it = b.begin(); it != b.end(); ++it)
    {
        if (*it)
            a[it.key()] = true;
    }

    return a;
}

void ScanStreamData::ReturnCachedTable(const PSIPTable *psip) const
{
    ATSCStreamData::ReturnCachedTable(psip);
    dvb.ReturnCachedTable(psip);
}

void ScanStreamData::ReturnCachedTables(pmt_vec_t &x) const
{
    ATSCStreamData::ReturnCachedTables(x);
    dvb.ReturnCachedTables(x);
}

void ScanStreamData::ReturnCachedTables(pmt_map_t &x) const
{
    ATSCStreamData::ReturnCachedTables(x);
    dvb.ReturnCachedTables(x);
}
