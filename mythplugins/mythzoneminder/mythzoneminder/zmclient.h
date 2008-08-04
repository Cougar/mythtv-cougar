#ifndef ZMCLIENT_H_
#define ZMCLIENT_H_

using namespace std;

#include <iostream>
#include <vector>

// qt
#include <qobject.h>
#include <qstringlist.h>
#include <q3valuevector.h>
#include <q3socket.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <qmutex.h>
#include <qimage.h>

// myth
#include "mythtv/mythsocket.h"
#include "mythtv/mythexp.h"

// zm
#include "zmdefines.h"

class MPUBLIC ZMClient : public QObject
{
    Q_OBJECT

  protected:
    ZMClient();

    static bool m_server_unavailable;
    static class ZMClient *m_zmclient;

  public:
    ~ZMClient();

    static class ZMClient *get(void);
    static bool setupZMClient (void);

    // Used to actually connect to an ZM server
    bool connectToHost(const QString &hostname, unsigned int port);
    bool connected(void) { return m_bConnected; }

    bool checkProtoVersion(void);

    // If you want to be pleasant, call shutdown() before deleting your ZMClient 
    // device
    void shutdown();

    void getServerStatus(QString &status, QString &cpuStat, QString &diskStat);
    void getMonitorStatus(vector<Monitor*> *monitorList);
    void getEventList(const QString &monitorName, bool oldestFirst,
                      QString date, vector<Event*> *eventList);
    void getEventFrame(int monitorID, int eventID, int frameNo, QImage &image);
    void getAnalyseFrame(int monitorID, int eventID, int frameNo, QImage &image);
    int  getLiveFrame(int monitorID, QString &status, unsigned char* buffer, int bufferSize);
    void getFrameList(int eventID, vector<Frame*> *frameList);
    void deleteEvent(int eventID);
    void deleteEventList(vector<Event*> *eventList);

    void getCameraList(QStringList &cameraList);
    void getMonitorList(vector<Monitor*> *monitorList);
    void getEventDates(const QString &monitorName, bool oldestFirst, 
                       QStringList &dateList);
    void setMonitorFunction(const int monitorID, const QString &function, const int enabled);

  private slots:
    void restartConnection(void);  // Try to re-establish the connection to 
                                   // ZMServer every 10 seconds
  private:
    bool readData(unsigned char *data, int dataSize);
    bool sendReceiveStringList(QStringList &strList);

    MythSocket       *m_socket;
    QMutex            m_socketLock;
    QString           m_hostname;
    uint              m_port;
    bool              m_bConnected;
    QTimer           *m_retryTimer;
    bool              m_zmclientReady;
};

#endif
