#include <qapplication.h>
#include <qstring.h>
#include <unistd.h>
#include <stdlib.h>

#include <QByteArray>
#include <QMutex>
#include <QHostInfo>

#include "mythtimer.h"
#include "mythsocket.h"
#include "mythverbose.h"
#include "compat.h"

#ifdef USING_MINGW
#include <winsock2.h>
#else
#include <sys/select.h>
#endif
#include <cassert>

#define SLOC(a) QString("MythSocket(%1:%2): ").arg((unsigned long)a, 0, 16)\
                    .arg(a->socket())
#define LOC SLOC(this)

const uint MythSocket::kSocketBufferSize = 128000;

MythSocketThread MythSocket::m_readyread_thread;

MythSocket::MythSocket(int socket, MythSocketCBs *cb)
    : MSocketDevice(MSocketDevice::Stream),            m_cb(cb),
      m_state(Idle),         m_addr(),                 m_port(0),
      m_ref_count(0),        m_notifyread(0)
{
    VERBOSE(VB_SOCKET, LOC + "new socket");
    if (socket > -1)
        setSocket(socket);

    if (m_cb)
        m_readyread_thread.AddToReadyRead(this);
}

MythSocket::~MythSocket()
{
    close();
    VERBOSE(VB_SOCKET, LOC + "delete socket");
}

void MythSocket::setCallbacks(MythSocketCBs *cb)
{
    if (m_cb && cb)
    {
       m_cb = cb;
       return;
    }

    m_cb = cb;

    if (m_cb)
        m_readyread_thread.AddToReadyRead(this);
    else
        m_readyread_thread.RemoveFromReadyRead(this);
}

void MythSocket::UpRef(void)
{
    m_ref_lock.lock();
    m_ref_count++;
    m_ref_lock.unlock();
    VERBOSE(VB_SOCKET, LOC + QString("UpRef: %1").arg(m_ref_count));
}

bool MythSocket::DownRef(void)
{
    m_ref_lock.lock();
    int ref = --m_ref_count;
    m_ref_lock.unlock();

    VERBOSE(VB_SOCKET, LOC + QString("DownRef: %1").arg(m_ref_count));

    if (m_cb && ref == 0)
    {
        m_cb = NULL;
        m_readyread_thread.RemoveFromReadyRead(this);
        // thread will downref & delete obj
        return true;
    }
    else if (ref < 0)
    {
        delete this;
        return true;
    }

    return false;
}

MythSocket::State MythSocket::state(void)
{
    return m_state;
}

void MythSocket::setState(const State state)
{
    if (state != m_state)
    {
        VERBOSE(VB_SOCKET, LOC + QString("state change %1 -> %2")
                .arg(stateToString(m_state)).arg(stateToString(state)));

        m_state = state;
    }
}

QString MythSocket::stateToString(const State state)
{
    switch(state)
    {
        case Connected:
            return "Connected";
        case Connecting:
            return "Connecting";
        case HostLookup:
            return "HostLookup";
        case Idle:
            return "Idle";
        default:
            return QString("Invalid State: %1").arg(state);
    }
}

QString MythSocket::errorToString(const Error error)
{
    switch(error)
    {
        case NoError:
            return "NoError";
        case AlreadyBound:
            return "AlreadyBound";
        case Inaccessible:
            return "Inaccessible";
        case NoResources:
            return "NoResources";
        case InternalError:
            return "InternalError";
        case Impossible:
            return "Impossible";
        case NoFiles:
            return "NoFiles";
        case ConnectionRefused:
            return "ConnectionRefused";
        case NetworkFailure:
            return "NetworkFailure";
        case UnknownError:
            return "UnknownError";
        default:
            return QString("Invalid error: %1").arg(error);
    }
}

void MythSocket::setSocket(int socket, Type type)
{
    VERBOSE(VB_SOCKET, LOC + QString("setSocket: %1").arg(socket));
    if (socket < 0)
    {
        VERBOSE(VB_SOCKET, LOC + "setSocket called with invalid socket");
        return;
    }

    if (state() == Connected)
    {
        VERBOSE(VB_SOCKET, LOC +
                "setSocket called while in Connected state, closing");
        close();
    }

    MSocketDevice::setSocket(socket, type);
    setBlocking(false);
    setState(Connected);
}

void MythSocket::close(void)
{
    setState(Idle);
    MSocketDevice::close();
}

qint64 MythSocket::readBlock(char *data, quint64 len)
{
    // VERBOSE(VB_SOCKET, LOC + "readBlock called");
    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "readBlock called while not in "
                "connected state");
        return -1;
    }

    m_notifyread = false;

    qint64 rval = MSocketDevice::readBlock(data, len);
    if (rval == 0)
    {
        close();
        if (m_cb)
        {
            m_cb->connectionClosed(this);
            VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
        }
    }
    return rval;
}

/**
 *  \brief Attempt to write 'len' bytes to socket
 *  \return actual bytes written
 */
qint64 MythSocket::writeBlock(const char *data, quint64 len)
{
    //VERBOSE(VB_SOCKET, LOC + "writeBlock called");
    if (state() != Connected)
    {
        VERBOSE(VB_SOCKET, LOC + "writeBlock called while not in "
                "connected state");
        return -1;
    }

    qint64 rval = MSocketDevice::writeBlock(data, len);

    // see if socket went away
    if (!isValid() || error() != MSocketDevice::NoError)
    {
        close();
        if (m_cb)
        {
            VERBOSE(VB_SOCKET, LOC + "cb->connectionClosed()");
            m_cb->connectionClosed(this);
        }
        return -1;
    }
    return rval;
}

bool MythSocket::writeStringList(QStringList &list)
{
    if (list.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, invalid string list.");
        return false;
    }

    if (state() != Connected)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, called with unconnected socket.");
        return false;
    }

    QString str = list.join("[]:[]");
    if (str.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeStringList: Error, joined null string.");
        return false;
    }

    QByteArray utf8 = str.toUtf8();
    int size = utf8.length();
    int written = 0;

    QByteArray payload;
    payload = payload.setNum(size);
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    size = payload.length();

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("write -> %1 %2")
            .arg(socket(), 2).arg((const char *)payload);

        if (((print_verbose_messages & VB_EXTRA) == 0) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    while (size > 0)
    {
        if (state() != Connected)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "writeStringList: Error, socket went unconnected.");
            return false;
        }

        int temp = writeBlock(payload.data() + written, size);
        if (temp > 0)
        {
            written += temp;
            size -= temp;
        }
        else if (temp < 0 && error() != MSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("writeStringList: Error, writeBlock failed. (%1)")
                    .arg(errorToString()));
            return false;
        }
        else if (temp <= 0)
        {
            errorcount++;
            if (errorcount > 5000)
            {
                VERBOSE(VB_GENERAL, LOC +
                        "writeStringList: No data written on writeBlock");
                return false;
            }
            usleep(1000);
        }
    }
    return true;
}

/**
 *  \brief Write len bytes to data to socket
 *  \return true if entire len of data is written
 */
bool MythSocket::writeData(const char *data, quint64 len)
{
    if (state() != Connected)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "writeData: Error, called with unconnected socket.");
        return false;
    }

    quint64 written = 0;
    uint zerocnt = 0;

    while (written < len)
    {
        qint64 btw = len - written >= kSocketBufferSize ?
                                       kSocketBufferSize : len - written;
        qint64 sret = writeBlock(data + written, btw);
        if (sret > 0)
        {
            zerocnt = 0;
            written += sret;
        }
        else if (!isValid())
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "writeData: Error, socket went unconnected");
            close();
            return false;
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("writeData: Error, writeBlock: %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else
        {
            zerocnt++;
            if (zerocnt > 5000)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        "writeData: Error, zerocnt timeout");
                return false;
            }
            usleep(1000);
         }
    }
    return true;
}

bool MythSocket::readStringList(QStringList &list, bool quickTimeout)
{
    list.clear();

    if (state() != Connected)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "readStringList: Error, called with unconnected socket.");
        return false;
    }

    MythTimer timer;
    timer.start();
    int elapsed = 0;

    while (waitForMore(5) < 8)
    {
        elapsed = timer.elapsed();
        if (!quickTimeout && elapsed >= 30000)
        {
            VERBOSE(VB_GENERAL, LOC + "readStringList: Error, timeout.");
            close();
            if (m_cb)
            {
                m_cb->connectionClosed(this);
                VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
            }
            return false;
        }
        else if (quickTimeout && elapsed >= 7000)
        {
            VERBOSE(VB_GENERAL, LOC +
                    "readStringList: Error, timeout (quick).");
            close();
            if (m_cb)
            {
                m_cb->connectionClosed(this);
                VERBOSE(VB_SOCKET, LOC + "calling cb->connectionClosed()");
            }
            return false;
        }

        if (state() != Connected)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "readStringList: Connection died.");
            return false;
        }

        {
            struct timeval tv;
            int maxfd;
            fd_set rfds;

            FD_ZERO(&rfds);
            FD_SET(socket(), &rfds);
            maxfd = socket();

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            int rval = select(maxfd + 1, &rfds, NULL, NULL, &tv);
            if (rval)
            {
                if (bytesAvailable() == 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC +
                            "readStringList: Connection died (select).");
                    return false;
                }
            }
        }
    }

    QByteArray sizestr(8 + 1, '\0');
    if (readBlock(sizestr.data(), 8) < 0)
    {
        VERBOSE(VB_GENERAL, LOC +
                QString("readStringList: Error, readBlock return error (%1)")
                .arg(errorToString()));
        close();
        return false;
    }

    QString sizes = sizestr;
    qint64 btr = sizes.trimmed().toInt();

    if (btr < 1)
    {
        int pending = bytesAvailable();
        QByteArray dump(pending + 1, 0);
        readBlock(dump.data(), pending);
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Protocol error: '%1' is not a valid size "
                        "prefix. %2 bytes pending.")
                        .arg(sizestr.data()).arg(pending));
        return false;
    }

    QByteArray utf8(btr + 1, 0);

    qint64 read = 0;
    int errmsgtime = 0;
    timer.start();

    while (btr > 0)
    {
        qint64 sret = readBlock(utf8.data() + read, btr);
        if (sret > 0)
        {
            read += sret;
            btr -= sret;
            if (btr > 0)
            {
                timer.start();
            }
        }
        else if (sret < 0 && error() != MSocketDevice::NoError)
        {
            VERBOSE(VB_GENERAL, LOC +
                    QString("readStringList: Error, readBlock %1")
                    .arg(errorToString()));
            close();
            return false;
        }
        else if (!isValid())
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "readStringList: Error, socket went unconnected");
            close();
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
                    VERBOSE(VB_GENERAL, LOC +
                            QString("readStringList: Waiting for data: %1 %2")
                            .arg(read).arg(btr));
                }
            }

            if (elapsed > 100000)
            {
                VERBOSE(VB_GENERAL, LOC +
                        "Error, readStringList timeout (readBlock)");
                return false;
            }

            usleep(500);
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    QByteArray payload;
    payload = payload.setNum(str.length());
    payload += "        ";
    payload.truncate(8);
    payload += str;

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = QString("read  <- %1 %2").arg(socket(), 2)
                                               .arg((const char *)payload);

        if (((print_verbose_messages & VB_EXTRA) == 0) && msg.length() > 88)
        {
            msg.truncate(85);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    list = str.split("[]:[]");

    m_notifyread = false;
    m_readyread_thread.WakeReadyReadThread();
    return true;
}

void MythSocket::Lock(void)
{
    m_lock.lock();
    m_readyread_thread.WakeReadyReadThread();
}

void MythSocket::Unlock(void)
{
    m_lock.unlock();
    m_readyread_thread.WakeReadyReadThread();
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QString &host, quint16 port)
{
    QHostAddress hadr;
    if (!hadr.setAddress(host))
    {
        QHostInfo info = QHostInfo::fromName(host);
        if (!info.addresses().isEmpty())
        {
            hadr = info.addresses().first();
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + QString("Unable to lookup: %1")
                    .arg(host));
            return false;
        }
    }

    return MythSocket::connect(hadr, port);
}

/**
 *  \brief connect to host
 *  \return true on success
 */
bool MythSocket::connect(const QHostAddress &addr, quint16 port)
{
    if (state() == Connected)
    {
        VERBOSE(VB_SOCKET, LOC +
                "connect() called with already open socket, closing");
        close();
    }

    VERBOSE(VB_SOCKET, LOC + QString("attempting connect() to (%1:%2)")
            .arg(addr.toString()).arg(port));

    if (!MSocketDevice::connect(addr, port))
    {
        VERBOSE(VB_SOCKET, LOC + QString("connect() failed (%1)")
                .arg(errorToString()));
        setState(Idle);
        return false;
    }

    setReceiveBufferSize(kSocketBufferSize);
    setAddressReusable(true);
    if (state() == Connecting)
    {
        setState(Connected);
        if (m_cb)
        {
            VERBOSE(VB_SOCKET, LOC + "cb->connected()");
            m_cb->connected(this);
            m_readyread_thread.WakeReadyReadThread();
        }
    }
    else
    {
        setState(Connected);
    }

    return true;
}

void ShutdownRRT(void)
{
    MythSocket::m_readyread_thread.ShutdownReadyReadThread();
}

void MythSocketThread::ShutdownReadyReadThread(void)
{
    m_readyread_run = false;
    WakeReadyReadThread();
    wait();

#ifdef USING_MINGW
    if (readyreadevent) {
        ::CloseHandle(readyreadevent);
        readyreadevent = NULL;
    }
#else
    ::close(m_readyread_pipe[0]);
    ::close(m_readyread_pipe[1]);
#endif
}

void MythSocketThread::StartReadyReadThread(void)
{
    if (m_readyread_run == false)
    {
        QMutexLocker locker(&m_readyread_lock);
        if (m_readyread_run == false)
        {
#ifdef USING_MINGW
            readyreadevent = ::CreateEvent(NULL, false, false, NULL);
            assert(readyreadevent);
#else
            int ret = pipe(m_readyread_pipe);
            (void) ret;
            assert(ret >= 0);
#endif

            m_readyread_run = true;
            start();

            atexit(ShutdownRRT);
        }
    }
}

void MythSocketThread::AddToReadyRead(MythSocket *sock)
{
    if (sock->socket() == -1)
    {
        VERBOSE(VB_SOCKET, "MythSocket: attempted to insert invalid socket to ReadyRead");
        return;
    }
    StartReadyReadThread();

    sock->UpRef();
    m_readyread_lock.lock();
    m_readyread_addlist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocketThread::RemoveFromReadyRead(MythSocket *sock)
{
    m_readyread_lock.lock();
    m_readyread_dellist.append(sock);
    m_readyread_lock.unlock();

    WakeReadyReadThread();
}

void MythSocketThread::WakeReadyReadThread(void)
{
    if (!isRunning())
	    return;

#ifdef USING_MINGW
    if (readyreadevent) ::SetEvent(readyreadevent);
#else
    if (m_readyread_pipe[1] >= 0)
    {
        char buf[1] = { '0' };
        ::write(m_readyread_pipe[1], &buf, 1);
    }
#endif
}

void MythSocketThread::iffound(MythSocket *sock)
{
    VERBOSE(VB_SOCKET, SLOC(sock) + "socket is readable");
    if (sock->bytesAvailable() == 0)
    {
        VERBOSE(VB_SOCKET, SLOC(sock) + "socket closed");
        sock->close();

        if (sock->m_cb)
        {
            VERBOSE(VB_SOCKET, SLOC(sock) + "cb->connectionClosed()");
            sock->m_cb->connectionClosed(sock);
        }
    }
    else if (sock->m_cb)
    {
        sock->m_notifyread = true;
        VERBOSE(VB_SOCKET, SLOC(sock) + "cb->readyRead()");
        sock->m_cb->readyRead(sock);
    }
}

bool MythSocketThread::isLocked(QMutex &mutex)
{
    bool isLocked = true;
    if (mutex.tryLock())
    {
        mutex.unlock();
        isLocked = false;
    }
    return isLocked;
}

void MythSocketThread::run(void)
{
    VERBOSE(VB_SOCKET, "MythSocket: readyread thread start");

    fd_set rfds;
    MythSocket *sock;
    int maxfd;
    bool found;

    while (m_readyread_run)
    {
        m_readyread_lock.lock();
        while (m_readyread_dellist.size() > 0)
        {
            sock = m_readyread_dellist.takeFirst();
            bool del = m_readyread_list.removeAll(sock);

            if (del)
            {
                m_readyread_lock.unlock();
                sock->DownRef();
                m_readyread_lock.lock();
            }
        }

        while (m_readyread_addlist.count() > 0)
        {
            sock = m_readyread_addlist.takeFirst();
            //sock->UpRef();  Did upref in AddToReadyRead()
            m_readyread_list.append(sock);
        }
        m_readyread_lock.unlock();

#ifdef USING_MINGW

        int n = m_readyread_list.count() + 1;
        HANDLE *hEvents = new HANDLE[n];
        memset(hEvents, 0, sizeof(HANDLE) * n);
        unsigned *idx = new unsigned[n];
        n = 0;

        for (unsigned i = 0; i < m_readyread_list.count(); i++)
        {
            sock = m_readyread_list.at(i);
            if (sock->state() == MythSocket::Connected
                && !sock->m_notifyread && !isLocked(sock->m_lock))
            {
                HANDLE hEvent = ::CreateEvent(NULL, false, false, NULL);
                if (!hEvent)
                {
                    VERBOSE(VB_IMPORTANT, "MythSocket: CreateEvent failed");
                }
                else
                {
                    if (SOCKET_ERROR != ::WSAEventSelect(
                            sock->socket(), hEvent,
                            FD_READ | FD_CLOSE))
                    {
                        hEvents[n] = hEvent;
                        idx[n++] = i;
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, QString(
                                    "MythSocket: CreateEvent, "
                                    "WSAEventSelect(%1, %2) failed")
                                .arg(sock->socket()));
                        ::CloseHandle(hEvent);
                    }
                }
            }
        }

        hEvents[n++] = readyreadevent;
        int rval = ::WaitForMultipleObjects(n, hEvents, false, INFINITE);

        for (int i = 0; i < (n - 1); i++)
            ::CloseHandle(hEvents[i]);

        delete[] hEvents;

        if (rval == WAIT_FAILED)
        {
            VERBOSE(VB_IMPORTANT,
                    "MythSocket: WaitForMultipleObjects returned error");
            delete[] idx;
        }
        else if (rval >= WAIT_OBJECT_0 && rval < (WAIT_OBJECT_0 + n))
        {
            rval -= WAIT_OBJECT_0;

            if (rval < (n - 1))
            {
                rval = idx[rval];
                sock = m_readyread_list.at(rval);
                found = (sock->state() == MythSocket::Connected)
                            && !isLocked(sock->m_lock);
            }
            else
            {
                found = false;
            }

            delete[] idx;

            if (found)
                iffound(sock);

            ::ResetEvent(readyreadevent);
        }
        else if (rval >= WAIT_ABANDONED_0 && rval < (WAIT_ABANDONED_0 + n))
        {
            VERBOSE(VB_SOCKET, "MythSocket: abandoned");
        }
        else
        {
//          VERBOSE(VB_SOCKET, SLOC + "select timeout");
        }

#else /* if !USING_MINGW */

        // add check for bad fd?
        FD_ZERO(&rfds);
        maxfd = m_readyread_pipe[0];

        FD_SET(m_readyread_pipe[0], &rfds);

        QList<MythSocket*>::const_iterator it = m_readyread_list.begin();
        while (it != m_readyread_list.end())
        {
            sock = *it;
            if (sock->state() == MythSocket::Connected &&
                sock->m_notifyread == false &&
                !isLocked(sock->m_lock))
            {
                FD_SET(sock->socket(), &rfds);
                maxfd = std::max(sock->socket(), maxfd);
            }
            ++it;
        }

        int rval = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (rval == -1)
        {
            VERBOSE(VB_SOCKET, "MythSocket: select returned error");
        }
        else if (rval)
        {
            found = false;
            QList<MythSocket*>::const_iterator it = m_readyread_list.begin();
            while (it != m_readyread_list.end())
            {
                sock = *it;
                if (sock->state() == MythSocket::Connected &&
                    FD_ISSET(sock->socket(), &rfds) &&
                    !isLocked(sock->m_lock))
                {
                    found = true;
                    break;
                }
                ++it;
            }

            if (found)
                iffound(sock);

            if (FD_ISSET(m_readyread_pipe[0], &rfds))
            {
                char buf[128];
                ::read(m_readyread_pipe[0], buf, 128);
            }
        }
        else
        {
//          VERBOSE(VB_SOCKET, SLOC + "select timeout");
        }

#endif /* !USING_MINGW */

    }

    VERBOSE(VB_SOCKET, "MythSocket: readyread thread exit");
}

