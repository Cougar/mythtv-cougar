// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVBSTREAMDATA_H_
#define DVBSTREAMDATA_H_

#include "mpegstreamdata.h"

typedef NetworkInformationTable* nit_ptr_t;
typedef vector<const nit_ptr_t>  nit_vec_t;
typedef QMap<uint, nit_ptr_t>    nit_cache_t;

typedef ServiceDescriptionTable* sdt_ptr_t;
typedef vector<const ServiceDescriptionTable*>  sdt_vec_t;
typedef QMap<uint, sdt_ptr_t>    sdt_cache_t;

typedef vector<DVBMainStreamListener*>   dvb_main_listener_vec_t;
typedef vector<DVBOtherStreamListener*>  dvb_other_listener_vec_t;
typedef vector<DVBEITStreamListener*>    dvb_eit_listener_vec_t;

class DVBStreamData : virtual public MPEGStreamData
{
  public:
    DVBStreamData(bool cacheTables = false);
    ~DVBStreamData();

    void Reset();

    // Table processing
    bool HandleTables(uint pid, const PSIPTable&);
    bool IsRedundant(uint pid, const PSIPTable&) const;

    // Table versions
    void SetVersionNIT(int version, uint last_section)
    {
        if (_nit_version == version)
            return;
        _nit_version = version;
        init_sections(_nit_section_seen, last_section);
    }
    int  VersionNIT() const { return _nit_version; }

    void SetVersionNITo(int version, uint last_section)
    {
        if (_nito_version == version)
            return;
        _nito_version = version;
        init_sections(_nito_section_seen, last_section);
    }
    int  VersionNITo() const { return _nito_version; }

    void SetVersionSDT(uint tsid, int version, uint last_section)
    {
        if (VersionSDT(tsid) == version)
            return;
        _sdt_versions[tsid] = version;
        init_sections(_sdt_section_seen[tsid], last_section);
    }
    int VersionSDT(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _sdt_versions.find(tsid);
        if (it == _sdt_versions.end())
            return -1;
        return *it;
    }

    void SetVersionSDTo(uint tsid, int version, uint last_section)
    {
        if (_sdto_versions[tsid] == version)
            return;
        _sdt_versions[tsid] = version;
        init_sections(_sdt_section_seen[tsid], last_section);
    }
    int VersionSDTo(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _sdto_versions.find(tsid);
        if (it == _sdto_versions.end())
            return -1;
        return *it;
    }

    void SetVersionEIT(uint tableid, uint serviceid, int version)
    {
        if (VersionEIT(tableid, serviceid) == version)
            return;
        uint key = (tableid << 16) | serviceid;
        _eit_section_seen[key].clear();
        _eit_section_seen[key].resize(32, 0);
    }

    int VersionEIT(uint tableid, uint serviceid) const
    {
        uint key = (tableid << 16) | serviceid;
        const QMap<uint, int>::const_iterator it = _eit_version.find(key);
        if (it == _eit_version.end())
            return -1;
        return *it;
    }

    // Sections seen
    void SetNITSectionSeen(uint section);
    bool NITSectionSeen(uint section) const;
    bool HasAllNITSections(void) const;

    void SetNIToSectionSeen(uint section);
    bool NIToSectionSeen(uint section) const;
    bool HasAllNIToSections(void) const;

    void SetSDTSectionSeen(uint tsid, uint section);
    bool SDTSectionSeen(uint tsid, uint section) const;
    bool HasAllSDTSections(uint tsid) const;

    void SetSDToSectionSeen(uint tsid, uint section);
    bool SDToSectionSeen(uint tsid, uint section) const;
    bool HasAllSDToSections(uint tsid) const;

    void SetEITSectionSeen(uint tableid, uint serviceid, uint section);
    bool EITSectionSeen(uint tableid, uint serviceid, uint section) const;

    // Caching
    bool HasCachedAnyNIT(bool current = true) const;
    bool HasCachedAllNIT(bool current = true) const;
    bool HasCachedAnySDT(uint tsid, bool current = true) const;
    bool HasCachedAllSDT(uint tsid, bool current = true) const;
    bool HasCachedSDT(bool current = true) const;
    bool HasCachedAllSDTs(bool current = true) const;

    const nit_ptr_t GetCachedNIT(uint section_num, bool current = true) const;
    const sdt_ptr_t GetCachedSDT(uint tsid, uint section_num,
                                 bool current = true) const;
    const sdt_vec_t GetAllCachedSDTs(bool current = true) const;

    void ReturnCachedSDTTables(sdt_vec_t&) const;

    void AddDVBMainListener(DVBMainStreamListener*);
    void AddDVBOtherListener(DVBOtherStreamListener*);
    void AddDVBEITListener(DVBEITStreamListener*);

    void RemoveDVBMainListener(DVBMainStreamListener*);
    void RemoveDVBOtherListener(DVBOtherStreamListener*);
    void RemoveDVBEITListener(DVBEITStreamListener*);

  private:
    // Caching
    void CacheNIT(NetworkInformationTable*);
    void CacheSDT(ServiceDescriptionTable*);

  private:
    // Signals
    dvb_main_listener_vec_t   _dvb_main_listeners;
    dvb_other_listener_vec_t  _dvb_other_listeners;
    dvb_eit_listener_vec_t    _dvb_eit_listeners;

    // Table versions
    int                       _nit_version;
    QMap<uint, int>           _sdt_versions;
    sections_t                _nit_section_seen;
    sections_map_t            _sdt_section_seen;
    QMap<uint, int>           _eit_version;
    sections_map_t            _eit_section_seen;

    int                       _nito_version;
    QMap<uint, int>           _sdto_versions;
    sections_t                _nito_section_seen;
    sections_map_t            _sdto_section_seen;

    // Caching
    nit_cache_t               _cached_nit;  // section -> sdt
    sdt_cache_t               _cached_sdts; // tsid+section -> sdt
};

#endif // DVBSTREAMDATA_H_
