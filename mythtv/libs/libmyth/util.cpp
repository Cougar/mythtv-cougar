#include <qapplication.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
using namespace std;

#include "util.h"
#include "mythcontext.h"

#if defined(Q_WS_X11)
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
}
#endif

#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qfont.h>

#ifdef USE_LIRC
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenuevent.h"
#endif

#define SOCKET_BUF_SIZE  128000

QString SocDevErrStr(int error)
{
    QString errorMsg = "N/A";
        
    if (error == QSocketDevice::NoError)
        errorMsg = "NoError";
    else if (error == QSocketDevice::AlreadyBound)    
        errorMsg = "AlreadyBound";
    else if (error == QSocketDevice::Inaccessible)    
        errorMsg = "Inaccessible";
    else if (error == QSocketDevice::NoResources)    
        errorMsg = "NoResources";
    else if (error == QSocketDevice::Bug)    
        errorMsg = "Bug";
    else if (error == QSocketDevice::Impossible)    
        errorMsg = "Impossible";
    else if (error == QSocketDevice::NoFiles)    
        errorMsg = "NoFiles";
    else if (error == QSocketDevice::ConnectionRefused)    
        errorMsg = "ConnectionRefused";
    else if (error == QSocketDevice::NetworkFailure)    
        errorMsg = "NetworkFailure";
    else if (error == QSocketDevice::UnknownError)    
        errorMsg = "UnknownError";
        
   return errorMsg;         
}

bool connectSocket(QSocketDevice *socket, const QString &host, int port)
{
    QHostAddress hadr;
    hadr.setAddress(host);
    
    socket->setAddressReusable(true);
    bool result = socket->connect(hadr, port);
  
    if (result)
    {
        socket->setReceiveBufferSize(SOCKET_BUF_SIZE); // defaults to 87k 
                                                       // max is 131k
        //cout << "connectSocket - buffsize: " 
        //     << socket->receiveBufferSize() << "\n";
    }                                          
    else
    {
        VERBOSE(VB_NETWORK, 
             QString("Socket error connecting to host: %1, port: %2, error: %3")
                   .arg(host).arg(port).arg(SocDevErrStr(socket->error())));
    }
    
    return result;
}

bool WriteStringList(QSocketDevice *socket, QStringList &list)
{// QSocketDevice (frontend)
    
    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_ALL, "WriteStringList: Bad socket");
        return false;
    }    
    
    QString str = list.join("[]:[]");
    QCString utf8 = str.utf8();

    QCString payload;
    payload = payload.setNum(utf8.length());
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = payload;

        if (msg.length() > 58)
        {
            msg.truncate(55);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    int btw = payload.length();
    int written = 0;
    
    QTime timer;
    timer.start();

    while (btw > 0)
    {
        int sret = socket->writeBlock(payload.data() + written, btw);
        // cerr << "  written: " << temp << endl; //DEBUG
        if (sret > 0)
        {
            written += sret;
            btw -= sret;
            
            if (btw > 0)
            {
                timer.start();
                VERBOSE(VB_GENERAL, QString("Partial WriteStringList: %1")
                                            .arg(written));
            }                                
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_GENERAL, QString("Error writing stringlist "
                                        "(writeBlock): %1")
                                        .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else
        {
            if (timer.elapsed() > 10000)
            {
                VERBOSE(VB_GENERAL, "WriteStringList timeout");
                return false;
            }  
            
            usleep(50);
        }
    }

    return true;
}

bool ReadStringList(QSocketDevice *socket, QStringList &list, bool quickTimeout)
{// QSocketDevice (frontend)
    list.clear();

    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_ALL, "ReadStringList: Bad socket");
        return false;
    }    

    QTime timer;
    timer.start();
    int elapsed = 0;

    while (socket->waitForMore(5) < 8)
    {
        elapsed = timer.elapsed();
        if (!quickTimeout && elapsed >= 30000)
        {
            VERBOSE(VB_GENERAL, "ReadStringList timeout.");
            socket->close();
            return false;
        }
        else if (quickTimeout && elapsed >= 8000)
        {
            VERBOSE(VB_GENERAL, "ReadStringList timeout (quick).");
            socket->close();
            return false;
        }
        
        usleep(50);
    }

    QCString sizestr(8 + 1);
    if (socket->readBlock(sizestr.data(), 8) < 0)
    {
        VERBOSE(VB_GENERAL, QString("Error reading stringlist (sizestr): %1")
                                    .arg(SocDevErrStr(socket->error())));
        socket->close();
        return false;
    }

    sizestr = sizestr.stripWhiteSpace();
    int btr = sizestr.toInt();

    QCString utf8(btr + 1);

    int read = 0;
    int errmsgtime = 0;
    timer.start();
    
    while (btr > 0)
    {
        int sret = socket->readBlock(utf8.data() + read, btr);
        // cerr << "  read: " << temp << endl; //DEBUG
        if (sret > 0)
        {
            read += sret;
            btr -= sret;
            if (btr > 0)
            {
                usleep(50);
                timer.start();
            }    
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_GENERAL, QString("Error reading stringlist"
                                        " (readBlock): %1")
                                        .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else
        {
            elapsed = timer.elapsed();
            if (elapsed  > 1000)
            {
                if ((elapsed - errmsgtime) > 1000)
                {
                    errmsgtime = elapsed;
                    VERBOSE(VB_GENERAL, QString("Waiting for data: %1 %2")
                                                .arg(read).arg(btr));
                }                            
            }
            
            if (elapsed > 10000)
            {
                VERBOSE(VB_GENERAL, "ReadStringList timeout. (readBlock)");
                return false;
            }
            
            usleep(50);
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str, true);

    return true;
}

bool WriteBlock(QSocketDevice *socket, void *data, int len)
{// QSocketDevice
    
    if (!socket->isOpen() || socket->error())
    {
        VERBOSE(VB_ALL, "WriteBlock: Bad socket");
        return false;
    }    
    
    int written = 0;
    int zerocnt = 0;
    
    while (written < len)
    {
        // Push bytes to client. We may push more than the socket buffer,
        // in which case this call will continue writing until all data has
        // been sent. We're counting on the client thread to be pulling data 
        // from the socket, if not we'll timeout and return false.
        
        int btw = len - written >= SOCKET_BUF_SIZE ? 
                                   SOCKET_BUF_SIZE : len - written;
        
        int sret = socket->writeBlock((char *)data + written, btw);
        if (sret > 0)
        {
            zerocnt = 0;
            written += sret;
            if (written < len)
                usleep(10);
        }
        else if (sret < 0 && socket->error() != QSocketDevice::NoError)
        {
            VERBOSE(VB_IMPORTANT, 
                    QString("Socket write error (writeBlock): %1")
                            .arg(SocDevErrStr(socket->error())));
            socket->close();
            return false;
        }
        else 
        {
            if (++zerocnt > 200)
            {
                VERBOSE(VB_IMPORTANT, "WriteBlock zerocnt timeout");
                return false;
            }    
            usleep(100); // We're waiting on the client.
        }
    }
    
    return true;
}


// QSocket (backend version)
bool WriteStringList(QSocket *socket, QStringList &list)
{
    if (list.size() <= 0)
    {
        VERBOSE(VB_ALL, "invalid stringlist in WriteStringList");
        return false;
    }

    if (!socket || socket->state() != QSocket::Connected)
    {
        VERBOSE(VB_ALL, "writing to unconnected socket in WriteStringList");
        return false;
    }

    QString str = list.join("[]:[]");
    if (str == QString::null)
    {
        VERBOSE(VB_ALL, "joined null string in WriteStringList");
        return false;
    }

    QCString utf8 = str.utf8();

    int size = utf8.length();

    int written = 0;

    QCString payload;

    payload = payload.setNum(size);
    payload += "        ";
    payload.truncate(8);
    payload += utf8;
    size = payload.length();

    if ((print_verbose_messages & VB_NETWORK) != 0)
    {
        QString msg = payload;

        if (msg.length() > 58)
        {
            msg.truncate(55);
            msg += "...";
        }
        VERBOSE(VB_NETWORK, msg);
    }

    unsigned int errorcount = 0;
    bool retval = true;

    while (size > 0)
    {
        if (socket->state() != QSocket::Connected)
        {
            VERBOSE(VB_ALL, "writing to unconnected socket #2");
            return false;
        }

        qApp->lock();
        int temp = socket->writeBlock(payload.data() + written, size);
        qApp->unlock();

        if (temp < 0)
        {
            VERBOSE(VB_ALL, "writeBlock failed!");
            return false;
        }

        // cerr << "  written: " << temp << endl; //DEBUG
        written += temp;
        size -= temp;
        if (size > 0)
        {
            printf("Partial WriteStringList %u\n", written);
            qApp->processEvents();

            if (++errorcount > 50)
            {
                retval = false;
                break;
            }  
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();
    
    return retval;
}

bool ReadStringList(QSocket *socket, QStringList &list)
{// QSocket (backend)
    list.clear();

    qApp->lock();
    while (socket->waitForMore(5) < 8)
    {
        if (socket->state() != QSocket::Connected)
        {
            // dunno if socket->state() wants the app lock.  be safe for now.
            qApp->unlock();
            return false;
        }

        qApp->unlock();
        usleep(50);
        qApp->lock();
    }

    QCString sizestr(8 + 1);
    socket->readBlock(sizestr.data(), 8);

    qApp->unlock();

    sizestr = sizestr.stripWhiteSpace();
    int size = sizestr.toInt();

    QCString utf8(size + 1);

    int read = 0;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->readBlock(utf8.data() + read, size);
        qApp->unlock();
        // cerr << "  read: " << temp << endl; //DEBUG
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                printf("EOF readStringList %u\n", read);
                break; 
            }
            usleep(50);
            qApp->processEvents();

            if (zerocnt == 5)
            {
                printf("Waiting for data: %u %u\n", read, size);
            }
        }
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str, true);

    return true;
}

bool WriteBlock(QSocket *socket, void *data, int len)
{// QSocket (backend)
    int size = len;
    int written = 0;

    unsigned int errorcount = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->writeBlock((char *)data + written, size);
        qApp->unlock();
        written += temp;
        size -= temp;
        if (size > 0)
        {
            printf("Partial WriteBlock %u\n", written);
            qApp->processEvents();

            if (++errorcount > 50)
                return false;
        }
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();
    
    while (socket->bytesToWrite() >= (unsigned) written)
    {
        usleep(50000);
    }    

    return true;
}

int ReadBlock(QSocket *socket, void *data, int maxlen)
{// QSocket (backend)
    int read = 0;
    int size = maxlen;
    unsigned int zerocnt = 0;

    while (size > 0)
    {
        qApp->lock();
        int temp = socket->readBlock((char *)data + read, size);
        qApp->unlock();
        read += temp;
        size -= temp;
        if (size > 0)
        {
            if (++zerocnt >= 100)
            {
                printf("EOF ReadBlock %u\n", read);
                break; 
            }
            usleep(50);
            qApp->processEvents();
        }
    }
    
    return maxlen;
}



void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

long long decodeLongLong(QStringList &list, int offset)
{
    long long retval = 0;

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

long long decodeLongLong(QStringList &list, QStringList::iterator &it)
{
    (void)list;

    long long retval = 0;

    int l1 = (*(it++)).toInt();
    int l2 = (*(it++)).toInt();
 
    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
} 

#if defined(Q_WS_X11)
void GetMythTVGeometry(Display *dpy, int screen_num, int *x, int *y, 
                       int *w, int *h) 
{
    int event_base, error_base;

    if (XineramaQueryExtension(dpy, &event_base, &error_base) &&
        XineramaIsActive(dpy)) 
    {
        XineramaScreenInfo *xinerama_screens;
        XineramaScreenInfo *screen;
        int nr_xinerama_screens;

        int screen_nr = gContext->GetNumSetting("XineramaScreen", 0);

        xinerama_screens = XineramaQueryScreens(dpy, &nr_xinerama_screens);

        printf("Found %d Xinerama Screens.\n", nr_xinerama_screens);

        if (screen_nr > 0 && screen_nr < nr_xinerama_screens)
        {
            screen = &xinerama_screens[screen_nr];
            printf("Using screen %d, %dx%d+%d+%d\n",
                   screen_nr, screen->width, screen->height, screen->x_org, 
                   screen->y_org );
        } 
        else 
        {
            screen = &xinerama_screens[0];
            printf("Using first Xinerama screen, %dx%d+%d+%d\n",
                   screen->width, screen->height, screen->x_org, screen->y_org);
        }

        *w = screen->width;
        *h = screen->height;
        *x = screen->x_org;
        *y = screen->y_org;

        XFree(xinerama_screens);
    } 
    else 
    {
        *w = DisplayWidth(dpy, screen_num);
        *h = DisplayHeight(dpy, screen_num);
        *x = 0; *y = 0;
    }
}
#endif

QRgb blendColors(QRgb source, QRgb add, int alpha)
{
    int sred = qRed(source);
    int sgreen = qGreen(source);
    int sblue = qBlue(source);

    int tmp1 = (qRed(add) - sred) * alpha;
    int tmp2 = sred + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sred = tmp2 & 0xff;

    tmp1 = (qGreen(add) - sgreen) * alpha;
    tmp2 = sgreen + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sgreen = tmp2 & 0xff;

    tmp1 = (qBlue(add) - sblue) * alpha;
    tmp2 = sblue + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sblue = tmp2 & 0xff;

    return qRgb(sred, sgreen, sblue);
}

int myth_system(const QString &command, int flags)
{
#ifdef USE_LIRC
    LircEventLock lirc_lock(!(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC));
#endif

#ifdef USE_JOYSTICK_MENU
    JoystickMenuEventLock joystick_lock(!(flags & MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU));
#endif

    /* Kill warning, I presume */
#if ! defined(USE_LIRC) && ! defined(USE_JOYSTICK_MENU)
    (void)flags;
#endif

    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        perror("fork");
        return -1;
    }
    else if (child == 0)
    {
        /* Child */
        /* Close all open file descriptors except stdout/stderr */
        for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i--)
            close(i);

        /* Attach stdin to /dev/null */
        close(0);
        int fd = open("/dev/null", O_RDONLY);
        dup2(fd, 0);
        if (fd != 0)
            close(fd);

        /* Run command */
        execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        perror("execl");

        /* Failed to exec */
        _exit(127);
    }
    else
    {
        /* Parent */
        int status;

        if (waitpid(child, &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
        return WEXITSTATUS(status);
    }
}

QString cutDownString(QString text, QFont *testFont, int maxwidth)
{
    QFontMetrics fm(*testFont);

    int curFontWidth = fm.width(text);
    if (curFontWidth > maxwidth)
    {
        QString testInfo = "";
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while (curFontWidth < tmaxwidth)
        {
            testInfo = text.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }

        testInfo = testInfo + "...";
        text = testInfo;
    }

    return text;
}

long long stringToLongLong(const QString &str)
{
    long long retval = 0;
    if (str != QString::null)
    {
        retval = strtoll(str.ascii(), NULL, 0);
    }
    return retval;
}

QString longLongToString(long long ll)
{
    char str[21];
    snprintf(str, 20, "%lld", ll);
    str[20] = '\0';
    return str;
}
