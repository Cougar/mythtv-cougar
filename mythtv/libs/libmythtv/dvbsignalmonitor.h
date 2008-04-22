// -*- Mode: c++ -*-

#ifndef DVBSIGNALMONITOR_H
#define DVBSIGNALMONITOR_H

#include "dtvsignalmonitor.h"
#include "qstringlist.h"

class DVBChannel;
class DVBStreamHandler;
class DVBSignalMonitorListener;

class DVBSignalMonitor: public DTVSignalMonitor
{
  public:
    DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                     uint64_t _flags =
                     kSigMon_WaitForSig    | kDVBSigMon_WaitForSNR |
                     kDVBSigMon_WaitForBER | kDVBSigMon_WaitForUB);
    virtual ~DVBSignalMonitor();

    virtual QStringList GetStatusList(bool kick);
    void Stop(void);

    virtual void SetRotorTarget(float target);
    virtual void GetRotorStatus(bool &was_moving, bool &is_moving);
    virtual void SetRotorValue(int)
    {
        QMutexLocker locker(&statusLock);
        rotorPosition.SetValue(100);
    }

    virtual void EmitStatus(void);

    // MPEG
    virtual void HandlePMT(uint, const ProgramMapTable*);

    // ATSC Main
    virtual void HandleSTT(const SystemTimeTable*);

    // DVB Main
    virtual void HandleTDT(const TimeDateTable*);

  protected:
    DVBSignalMonitor(void);
    DVBSignalMonitor(const DVBSignalMonitor&);

    virtual void UpdateValues(void);
    void EmitDVBSignals(void);

    DVBChannel *GetDVBChannel(void);

  protected:
    SignalMonitorValue signalToNoise;
    SignalMonitorValue bitErrorRate;
    SignalMonitorValue uncorrectedBlocks;
    SignalMonitorValue rotorPosition;

    bool               streamHandlerStarted;
    DVBStreamHandler  *streamHandler;
};

#endif // DVBSIGNALMONITOR_H
