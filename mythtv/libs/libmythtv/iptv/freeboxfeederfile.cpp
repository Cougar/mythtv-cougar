/** -*- Mode: c++ -*-
 *  FreeboxFeederFile
 *
 *  Please, don't submit bug reports if it
 *  can't read your file. Just use MythVideo!
 *
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxfeederfile.h"

// Qt headers
#include <qurl.h>

// Live555 headers
#include <BasicUsageEnvironment.hh>
#include <Groupsock.hh>
#include <GroupsockHelper.hh>
#include <ByteStreamFileSource.hh>
#include <TunnelEncaps.hh>

// MythTV headers
#include "freeboxmediasink.h"
#include "mythcontext.h"
#include "tspacket.h"

#define LOC QString("FbFeedFile: ")


FreeboxFeederFile::FreeboxFeederFile() :
    _source(NULL),
    _sink(NULL)
{
}

FreeboxFeederFile::~FreeboxFeederFile()
{
    Close();
}

bool FreeboxFeederFile::CanHandle(const QString &url) const
{
    return url.startsWith("file:", false);
}

bool FreeboxFeederFile::Open(const QString &url)
{
    VERBOSE(VB_RECORD, LOC + QString("Open(%1) -- begin").arg(url));

    QMutexLocker locker(&_lock);

    if (_source)
    {
        VERBOSE(VB_RECORD, LOC + "Open() -- end 1");
        return true;
    }
        
    QUrl parse(url);
    if (!parse.hasPath() || !parse.isLocalFile())
    {
        VERBOSE(VB_RECORD, LOC + "Open() -- end 2");
        return false;
    }
        
    // Begin by setting up our usage environment:
    if (!InitEnv())
        return false;
    
    _source = ByteStreamFileSource::createNew(
        *_live_env, parse.path().local8Bit());
    if (!_source)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Failed to create Live File Source.");
        FreeEnv();
        return false;
    }

    _sink = FreeboxMediaSink::CreateNew(*_live_env, TSPacket::SIZE * 128*1024);
    if (!_sink)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Freebox # Failed to create sink: %1")
                .arg(_live_env->getResultMsg()));

        Medium::close(_source);
        _source = NULL;
        FreeEnv();

        return false;
    }

    _sink->startPlaying(*_source, NULL, NULL);
    vector<IPTVListener*>::iterator it = _listeners.begin();
    for (; it != _listeners.end(); ++it)
        _sink->AddListener(*it);
        
    VERBOSE(VB_RECORD, LOC + "Open() -- end");

    return true;
}

void FreeboxFeederFile::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");
    Stop();

    QMutexLocker locker(&_lock);

    if (_sink)
    {
        Medium::close(_sink);
        _sink = NULL;
    }

    if (_source)
    {
        Medium::close(_source);
        _source = NULL;
    }

    FreeEnv();

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void FreeboxFeederFile::AddListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");
    if (!item)
    {
        VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
        return;
    }

    // avoid duplicates
    RemoveListener(item);

    // add to local list
    QMutexLocker locker(&_lock);
    _listeners.push_back(item);
    
    if (_sink)
        _sink->AddListener(item);

    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end");
}

void FreeboxFeederFile::RemoveListener(IPTVListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin");
    QMutexLocker locker(&_lock);
    vector<IPTVListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 1");
        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);

    if (_sink)
        _sink->RemoveListener(item);

    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 2");
}
