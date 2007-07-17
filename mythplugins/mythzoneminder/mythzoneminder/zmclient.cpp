/*
    zmclient.cpp
*/

#include <unistd.h>
#include <sys/wait.h>
#include <cmath>
#include <netinet/in.h>
#include <assert.h>

// qt
#include <qapplication.h>
#include <qregexp.h>

//myth
#include "mythtv/mythcontext.h"
#include "mythtv/mythdialogs.h"
#include "mythtv/util.h"

//zoneminder
#include "zmclient.h"

// the protocol version we understand
#define ZM_PROTOCOL_VERSION "4"

#define BUFFER_SIZE  (2048*1536*3)

ZMClient::ZMClient()
    : QObject(NULL, "ZMClient"),
      m_socket(NULL),
      m_socketLock(true),
      m_hostname("localhost"),
      m_port(6548),
      m_bConnected(false),
      m_retryTimer(new QTimer(this)),
      m_zmclientReady(false)
{
    connect(m_retryTimer, SIGNAL(timeout()),   this, SLOT(restartConnection()));
}

bool  ZMClient::m_server_unavailable = false;
class ZMClient *ZMClient::m_zmclient = NULL;

class ZMClient *ZMClient::get(void)
{
    if (m_zmclient == NULL && m_server_unavailable == false)
        m_zmclient = new ZMClient;
    return m_zmclient;
}

bool ZMClient::setupZMClient(void) 
{
    QString zmserver_host;
    int zmserver_port;

    if (m_zmclient) 
    {
        delete m_zmclient;
        m_zmclient = NULL;
        m_server_unavailable = false;
    }

    zmserver_host = gContext->GetSetting("ZoneMinderServerIP", "localhost");
    zmserver_port = gContext->GetNumSetting("ZoneMinderServerPort", 6548);

    class ZMClient *zmclient = ZMClient::get();
    if (zmclient->connectToHost(zmserver_host, zmserver_port) == false) 
    {
        delete m_zmclient;
        m_zmclient = NULL;
        m_server_unavailable = false;
        return false;
    }

    return true;
}

bool ZMClient::connectToHost(const QString &lhostname, unsigned int lport)
{
    QMutexLocker locker(&m_socketLock);

    m_hostname = lhostname;
    m_port = lport;

    m_bConnected = false;
    int count = 0;
    do
    {
        ++count;

        VERBOSE(VB_GENERAL, QString("Connecting to zm server: " 
                "%1:%2 (try %3 of 10)").arg(m_hostname).arg(m_port)
                                       .arg(count));
        if (m_socket)
        {
            m_socket->DownRef();
            m_socket = NULL;
        }

        m_socket = new MythSocket();
        //m_socket->setCallbacks(this);
        if (!m_socket->connect(m_hostname, m_port))
        {
            m_socket->DownRef();
            m_socket = NULL;
        }
        else
        {
            m_zmclientReady = true;
            m_bConnected = true;
        }

        usleep(500000);

    } while (count < 10 && !m_bConnected);

    if (!m_bConnected)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "Connection failure",
                              tr("Cannot connect to the mythzmerver - Is it running? " 
                                 "Have you set the correct IP and port in the settings?"));
    }

    // check the server uses the same protocol as us
    if (m_bConnected && !checkProtoVersion())
    {
        m_zmclientReady = false;
        m_bConnected = false;
    }

    if (m_bConnected == false)
        m_server_unavailable = true;

    return m_bConnected;
}

bool ZMClient::sendReceiveStringList(QStringList &strList)
{
    bool ok = false;
    if (m_bConnected)
    {
        m_socket->writeStringList(strList);
        ok = m_socket->readStringList(strList, true);
    }

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, "Connection to mythzmserver lost");

        if (!connectToHost(m_hostname, m_port))
        {
            VERBOSE(VB_IMPORTANT, "Re connection to mythzmserver failed");
            return false;
        }

        // try to resend 
        m_socket->writeStringList(strList);
        ok = m_socket->readStringList(strList, true);
        if (!ok)
        {
            m_bConnected = false;
            return false;
        }
    }

    // the server sends "UNKNOWN_COMMAND" if it did not recognise the command
    if (strList[0] == "UNKNOWN_COMMAND")
    {
        VERBOSE(VB_IMPORTANT, "Somethings is getting passed to the server that it doesn't understand");
        return false;
    }

    // the server sends "ERROR" if it failed to process the command
    if (strList[0].startsWith("ERROR"))
    {
        VERBOSE(VB_IMPORTANT, QString("The server failed to process the command. "
                                      "The error was:- \n\t\t\t%1").arg(strList[0]));
        return false;
    }

    // we should get "OK" from the server if everything is OK
    if (strList[0] != "OK")
        return false;

    return true;
}

bool ZMClient::checkProtoVersion(void)
{
    QStringList strList = "HELLO";
    if (!sendReceiveStringList(strList))
    {
        VERBOSE(VB_IMPORTANT, QString("Server didn't respond to 'HELLO'!!"));

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "Connection failure",
            tr(QString("The mythzmerver didn't respond to our request to get the protocol version!!")));
        return false;
    }

    if (strList[1] != ZM_PROTOCOL_VERSION)
    {
        VERBOSE(VB_IMPORTANT, QString("Protocol version mismatch (plugin=%1, "
                "mythzmserver=%2)").arg(ZM_PROTOCOL_VERSION).arg(strList[1]));

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "Connection failure",
                         tr(QString("The mythzmerver uses protocol version %1, "
                                    "but this client only understands version %2. "
                                    "Make sure you are running compatible versions of "
                                    "both the server and plugin.")
                                    .arg(strList[1]).arg(ZM_PROTOCOL_VERSION)));
        return false;
    }

    VERBOSE(VB_IMPORTANT, QString("Using protocol version %1").arg(ZM_PROTOCOL_VERSION));
    return true;
}

void ZMClient::restartConnection()
{
    // Reset the flag
    m_zmclientReady = false;
    m_bConnected = false;
    m_server_unavailable = false;

    // Retry to connect. . .  Maybe the user restarted mythzmserver?
    connectToHost(m_hostname, m_port);
}

void ZMClient::shutdown()
{
    QMutexLocker locker(&m_socketLock);

    if (m_socket)
        m_socket->close();

    m_zmclientReady = false;
    m_bConnected = false;
}

ZMClient::~ZMClient()
{
    m_zmclient = NULL;

    if (m_socket)
    {
        m_socket->DownRef();
        m_zmclientReady = false;
    }

    if (m_retryTimer)
        delete m_retryTimer;
}

void ZMClient::getServerStatus(QString &status, QString &cpuStat, QString &diskStat)
{
    QStringList strList = "GET_SERVER_STATUS";
    if (!sendReceiveStringList(strList))
        return;

    status = strList[1];
    cpuStat = strList[2];
    diskStat = strList[3];
}

void ZMClient::getMonitorStatus(vector<Monitor*> *monitorList)
{
    monitorList->clear();

    QStringList strList = "GET_MONITOR_STATUS";
    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getMonitorStatus()");
        return;
    }

    for (int x = 0; x < monitorCount; x++)
    {
        Monitor *item = new Monitor;
        item->name = strList[x * 4 + 2];
        item->zmcStatus = strList[x * 4 + 3];
        item->zmaStatus = strList[x * 4 + 4];
        item->events = strList[x * 4 + 5].toInt();
        monitorList->push_back(item);
    }
}

void ZMClient::getEventList(const QString &monitorName, bool oldestFirst, 
                            QString date, vector<Event*> *eventList)
{
    eventList->clear();

    QStringList strList = "GET_EVENT_LIST";
    strList << monitorName << (oldestFirst ? "1" : "0") ;
    strList << date;

    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int eventCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getEventList()");
        return;
    }

    // sanity check 
    if ((int)(strList.size() - 2) / 6 != eventCount)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient got a mismatch between the number of events and "
                "the expected number of stringlist items in getEventList()");
        return;
    }

    QString dateFormat = gContext->GetSetting("ZoneMinderDateFormat", "ddd - dd/MM");
    QString timeFormat = gContext->GetSetting("ZoneMinderTimeFormat", "hh:mm:ss");

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < eventCount; x++)
    {
        Event *item = new Event;
        item->eventID = (*it++).toInt();
        item->eventName = *it++;
        item->monitorID = (*it++).toInt();
        item->monitorName = *it++;
        QString sDate = *it++;
        QDateTime dt = QDateTime::fromString(sDate, Qt::ISODate);
        item->startTime = dt.toString(dateFormat + " " + timeFormat);
        item->length = *it++;
        eventList->push_back(item);
    }
}

void ZMClient::getEventDates(const QString &monitorName, bool oldestFirst,
                            QStringList &dateList)
{
    dateList.clear();

    QStringList strList = "GET_EVENT_DATES";
    strList << monitorName << (oldestFirst ? "1" : "0") ;

    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int dateCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getEventDates()");
        return;
    }

    // sanity check 
    if ((int)(strList.size() - 3) != dateCount)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient got a mismatch between the number of dates and "
                "the expected number of stringlist items in getEventDates()");
        return;
    }

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < dateCount; x++)
    {
        dateList.append(*it++);
    }
}

void ZMClient::getFrameList(int eventID, vector<Frame*> *frameList)
{
    frameList->clear();

    QStringList strList = "GET_FRAME_LIST";
    strList << QString::number(eventID);
    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int frameCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getFrameList()");
        return;
    }

    // sanity check
    if ((int)(strList.size() - 2) / 2 != frameCount)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient got a mismatch between the number of frames and "
                "the expected number of stringlist items in getFrameList()");
        return;
    }

    QStringList::Iterator it = strList.begin();
    it++; it++;
    for (int x = 0; x < frameCount; x++)
    {
        Frame *item = new Frame;
        item->type = *it++;
        item->delta = (*it++).toDouble();
        frameList->push_back(item);
    }
}

void ZMClient::deleteEvent(int eventID)
{
    QStringList strList = "DELETE_EVENT";
    strList << QString::number(eventID);
    sendReceiveStringList(strList);
}

void ZMClient::deleteEventList(vector<Event*> *eventList)
{
    QStringList strList = "DELETE_EVENT_LIST";

    vector<Event*>::iterator it;
    for (it = eventList->begin(); it != eventList->end(); it++)
    {
       strList << QString::number((*it)->eventID);
    }

    sendReceiveStringList(strList);
}

bool ZMClient::readData(unsigned char *data, int dataSize)
{
    Q_LONG read = 0;
    int errmsgtime = 0;
    MythTimer timer;
    timer.start();
    int elapsed;

    while (dataSize > 0)
    {
        Q_LONG sret = m_socket->readBlock((char*) data + read, dataSize);
        if (sret > 0)
        {
            read += sret;
            dataSize -= sret;
            if (dataSize > 0)
            {
                timer.start();
            }
        }
        else if (sret < 0 && m_socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_GENERAL, QString("readData: Error, readBlock %1")
                            .arg(m_socket->errorToString()));
            m_socket->close();
            return false;
        }
        else if (!m_socket->isValid())
        {
            VERBOSE(VB_IMPORTANT, "readData: Error, socket went unconnected");
            m_socket->close();
            return false;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 10000)
            {
                if ((elapsed - errmsgtime) > 10000)
                {
                    errmsgtime = elapsed;
                    VERBOSE(VB_GENERAL, QString("m_socket->: Waiting for data: %1 %2")
                                    .arg(read).arg(dataSize));
                }
            }

            if (elapsed > 100000)
            {
                VERBOSE(VB_GENERAL, "Error, readData timeout (readBlock)");
                return false;
            }

            usleep(500);
        }
    }

    return true;
}

void ZMClient::getEventFrame(int monitorID, int eventID, int frameNo, QImage &image)
{
    QStringList strList = "GET_EVENT_FRAME";
    strList << QString::number(monitorID);
    strList << QString::number(eventID);
    strList << QString::number(frameNo);
    if (!sendReceiveStringList(strList))
    {
        image = QImage();
        return;
    }

    // get frame length from data
    int imageSize = strList[1].toInt();

    // grab the image data
    unsigned char *data = new unsigned char[imageSize];
    if (!readData(data, imageSize))
    {
        VERBOSE(VB_GENERAL, "ZMClient::getEventFrame(): Failed to get image data");
        image = QImage();
    }

    // extract the image data and create a QImage from it
    if (!image.loadFromData(data, imageSize, "JPEG"))
    {
        VERBOSE(VB_GENERAL, "ZMClient::getEventFrame(): Failed to load image from data");
        image = QImage();
    }
}

void ZMClient::getAnalyseFrame(int monitorID, int eventID, int frameNo, QImage &image)
{
    QStringList strList = "GET_ANALYSE_FRAME";
    strList << QString::number(monitorID);
    strList << QString::number(eventID);
    strList << QString::number(frameNo);
    if (!sendReceiveStringList(strList))
    {
        image = QImage();
        return;
    }

    // get frame length from data
    int imageSize = strList[1].toInt();

    // grab the image data
    unsigned char *data = new unsigned char[imageSize];
    if (!readData(data, imageSize))
    {
        VERBOSE(VB_GENERAL, "ZMClient::getAnalyseFrame(): Failed to get image data");
        image = QImage();
    }

    // extract the image data and create a QImage from it
    if (!image.loadFromData(data, imageSize, "JPEG"))
    {
        VERBOSE(VB_GENERAL, "ZMClient::getAnalyseFrame(): Failed to load image from data");
        image = QImage();
    }
}

int ZMClient::getLiveFrame(int monitorID, QString &status, unsigned char* buffer, int bufferSize)
{
    QStringList strList = "GET_LIVE_FRAME";
    strList << QString::number(monitorID);
    if (!sendReceiveStringList(strList))
    {
        // the server sends a "WARNING" message if there is no new frame available
        // we can safely ignore it
        if (strList[0].startsWith("WARNING"))
            return 0;
        else
        {
            status = strList[0];
            return 0;
        }
    }

    // get status
    status = strList[2];

    // get frame length from data
    int imageSize = strList[3].toInt();

    assert(bufferSize > imageSize);

    // read the frame data
    if (imageSize == 0)
        return 0;

    if (!readData(buffer, imageSize))
    {
        VERBOSE(VB_GENERAL, "ZMClient::getLiveFrame(): Failed to get image data");
        return 0;
    }

    return imageSize;
}

void ZMClient::getCameraList(QStringList &cameraList)
{
    cameraList.clear();

    QStringList strList = "GET_CAMERA_LIST";
    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int cameraCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getCameraList()");
        return;
    }

    for (int x = 0; x < cameraCount; x++)
    {
        cameraList.append(strList[x + 2]);
    }
}

void ZMClient::getMonitorList(vector<Monitor*> *monitorList)
{
    monitorList->clear();

    QStringList strList = "GET_MONITOR_LIST";
    if (!sendReceiveStringList(strList))
        return;

    bool bOK;
    int monitorCount = strList[1].toInt(&bOK);
    if (!bOK)
    {
        VERBOSE(VB_IMPORTANT, "ZMClient received bad int in getMonitorList()");
        return;
    }

    for (int x = 0; x < monitorCount; x++)
    {
        Monitor *item = new Monitor;
        item->id = strList[x * 5 + 2].toInt();
        item->name = strList[x * 5 + 3];
        item->width = strList[x * 5 + 4].toInt();
        item->height = strList[x * 5 + 5].toInt();
        item->palette = strList[x * 5 + 6].toInt();
        item->zmcStatus = "";
        item->zmaStatus = "";
        item->events = 0;
        item->status = "";
        monitorList->push_back(item);
        VERBOSE(VB_IMPORTANT, QString("Monitor: %1 (%2) is using palette: %3")
                .arg(item->name).arg(item->id).arg(item->palette));
    }
}
