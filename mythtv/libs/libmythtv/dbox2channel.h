/**
 *  Dbox2Channel
 *  Copyright (c) 2005 by Levent Gündogdu
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef DBOX2CHANNEL_H
#define DBOX2CHANNEL_H

#include <qstring.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "tv_rec.h"
#include "channelbase.h"
#include "sitypes.h"
#include "dbox2epg.h"

class DBox2Recorder;
class DBox2Channel;
class DBox2EPG;

class DBox2CRelay : public QObject
{
    Q_OBJECT

  public:
    DBox2CRelay(DBox2Channel *ch) : m_ch(ch) {}
    void SetChannel(DBox2Channel*);

  public slots:
    void HttpChannelChangeDone(bool error);
    void HttpRequestDone(bool error);

  private:
    DBox2Channel *m_ch;
    QMutex        m_lock;
};

class DBox2Channel : public ChannelBase
{
    friend class DBox2CRelay;
  public:
    DBox2Channel(TVRec *parent, DBox2DBOptions *dbox2_options, int cardid);
    ~DBox2Channel(void) { TeardownAll(); }

    bool SetChannelByString(const QString &chan);
    bool Open();
    bool IsOpen(void) const { return m_recorderAlive; }
    void Close();
    void SwitchToLastChannel();
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; return false; }

    QString GetChannelNameFromNumber(const QString&);
    QString GetChannelNumberFromName(const QString& channelName);
    QString GetChannelID(const QString&);

    void EPGFinished();
    void RecorderAlive(bool);

    void SetRecorder(DBox2Recorder*);

  private:
    void HttpChannelChangeDone(bool error);
    void HttpRequestDone(bool error);
    void TeardownAll(void);
    void Log(QString string);
    void LoadChannels();
    void RequestChannelChange(QString);
    QString GetDefaultChannel();

  private:
    DBox2DBOptions   *m_dbox2options;
    int               m_cardid;
    bool              m_channelListReady;
    QString           m_lastChannel;
    QString           m_requestChannel;
    DBox2EPG         *m_epg;
    bool              m_recorderAlive;

    QHttp            *http;
    QHttp            *httpChanger;
    DBox2CRelay      *m_relay;

    int               m_dbox2channelcount;
    QMap<int,QString> m_dbox2channelids;
    QMap<int,QString> m_dbox2channelnames;

    DBox2Recorder    *m_recorder;
    QMutex            m_lock;
};

#endif
