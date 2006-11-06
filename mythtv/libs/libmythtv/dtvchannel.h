/* -*- Mode: c++ -*-
 *  DTVChannel
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Contains base class for digital channels.
 */

#ifndef _DTVCHANNEL_H_
#define _DTVCHANNEL_H_

// POSIX headers
#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// MythTV headers
#include "channelbase.h"

typedef pair<uint,uint> pid_cache_item_t;
typedef vector<pid_cache_item_t> pid_cache_t;

class QString;
class TVRec;

/** \class DTVChannel
 *  \brief Class providing a generic interface to digital tuning hardware.
 */
class DTVChannel : public ChannelBase
{
  public:
    DTVChannel::DTVChannel(TVRec *parent);
    virtual ~DTVChannel() {}

    /** \brief Returns cached MPEG PIDs for last tuned channel.
     *  \param pid_cache List of PIDs with their TableID
     *                   types is returned in pid_cache. */
    virtual void GetCachedPids(pid_cache_t &pid_cache) const
        { (void) pid_cache; }
    /// \brief Saves MPEG PIDs to cache to database
    /// \param pid_cache List of PIDs with their TableID types to be saved.
    virtual void SaveCachedPids(const pid_cache_t &pid_cache) const
        { (void) pid_cache; }
    /// \brief Returns program number in PAT, -1 if unknown.
    virtual int GetProgramNumber(void) const
        { return currentProgramNum; };
    /// \brief Returns major channel, 0 if unknown.
    virtual uint GetMajorChannel(void) const
        { return currentATSCMajorChannel; };
    /// \brief Returns minor channel, 0 if unknown.
    virtual uint GetMinorChannel(void) const
        { return currentATSCMinorChannel; };
    /// \brief Returns DVB original_network_id, 0 if unknown.
    virtual uint GetOriginalNetworkID(void) const
        { return currentOriginalNetworkID; };
    /// \brief Returns DVB transport_stream_id, 0 if unknown.
    virtual uint GetTransportID(void) const
        { return currentTransportID; };

    /// \brief To be used by the channel scanner and possibly the EIT scanner.
    virtual bool TuneMultiplex(uint mplexid, QString inputname) = 0;

  protected:
    virtual void SetCachedATSCInfo(const QString &chan);
    static void GetCachedPids(int chanid, pid_cache_t&);
    static void SaveCachedPids(int chanid, const pid_cache_t&);

  protected:
    int     currentProgramNum;
    uint    currentATSCMajorChannel;
    uint    currentATSCMinorChannel;
    uint    currentTransportID;
    uint    currentOriginalNetworkID;
};

#endif // _DTVCHANNEL_H_
