#ifndef CHANNELBASE_H
#define CHANNELBASE_H

#include <map>
#include <qstring.h>
#include <qsqldatabase.h>
#include "frequencies.h"

using namespace std;

class TVRec;

/* Abstract class providing a generic interface to various channel
   implementations (for analog TV, DVB etc.). Also implements many generic
   functions needed by most derived classes.
   It is responsible for tuning, i.e. switching channels.
   It does not represent a single channel, but tuning hardware. */

class ChannelBase
{
 public:
    ChannelBase(TVRec *parent);
    virtual ~ChannelBase();

    virtual bool Open() = 0;
    virtual void Close() = 0;

    virtual void SetChannelOrdering(QString chanorder)
                                                  { channelorder = chanorder; }

    virtual bool SetChannelByString(const QString &chan) = 0;
    virtual bool ChannelUp(void);
    virtual bool ChannelDown(void);
    virtual bool NextFavorite(void);
    virtual bool CheckSignalFull(void) { return true; }
    virtual bool CheckSignal(int /*msecWait = 5000*/, 
                             int /*requiredSignalPercentage = 70*/,
                             int /*input = 0*/) { return true; }

    virtual int ChangeColour(bool up) { (void)up; return 0; };
    virtual int ChangeBrightness(bool up) { (void)up; return 0; };
    virtual int ChangeContrast(bool up) { (void)up; return 0; };
    virtual int ChangeHue(bool up) { (void)up; return 0; };
    virtual void SetContrast() {};
    virtual void SetBrightness() {};
    virtual void SetColour() {};
    virtual void SetHue() {};

    virtual void ToggleInputs(void);
    virtual void SwitchToInput(const QString &input);
    virtual void SwitchToInput(const QString &input, const QString &chan);
    virtual void SwitchToInput(int newcapchannel, bool setstarting) = 0;

    virtual int GetInputByName(const QString &input);
    virtual QString GetInputByNum(int capchannel);

    virtual void StoreInputChannels(void);
 
    virtual QString GetCurrentName(void);
    virtual QString GetCurrentInput(void);

    virtual int GetCurrentInputNum(void);

    virtual void SetFd(int fd) { (void)fd; }

    virtual QString GetOrdering() { return channelorder; }

    void SetDB(QSqlDatabase* _db_conn, pthread_mutex_t* _db_lock)
        { db_conn = _db_conn; db_lock = _db_lock; };

  protected:
    TVRec *pParent;
    QString curchannelname;
    int capchannels;
    int currentcapchannel;
    map<int, QString> channelnames;
    map<int, QString> inputChannel;
    map<int, QString> inputTuneTo;
    map<int, QString> externalChanger;
    map<int, QString> sourceid;

    QString channelorder;

    QSqlDatabase* db_conn;
    pthread_mutex_t* db_lock;

    bool ChangeExternalChannel(const QString &newchan);
};

#endif
