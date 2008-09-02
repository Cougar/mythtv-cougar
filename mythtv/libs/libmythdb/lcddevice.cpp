/*
    lcddevice.cpp
    
    a MythTV project object to control an
    LCDproc server
    
    (c) 2002, 2003 Thor Sigvaldason, Dan Morphis and Isaac Richards
*/

// ANSI C headers
#include <cstdlib>
#include <cmath>

// POSIX headers
#include <unistd.h>

// Qt headers
#include <QApplication>
#include <QRegExp>
#include <QTextStream>
#include <QTextCodec>
#include <QByteArray>

// MythTV headers
#include "lcddevice.h"
#include "mythverbose.h"
#include "compat.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythevent.h"

static QString LOC = "lcddevice: ";

LCD::LCD()
    : QObject(),
      socket(NULL),                 socketLock(QMutex::Recursive),
      hostname("localhost"),        port(6545),
      bConnected(false),

      retryTimer(new QTimer(this)), LEDTimer(new QTimer(this)),

      send_buffer(""),              last_command(QString::null),

      lcd_width(0),                 lcd_height(0),

      lcd_ready(false),             lcd_showtime(false),
      lcd_showmenu(false),          lcd_showgeneric(false),
      lcd_showmusic(false),         lcd_showchannel(false),
      lcd_showvolume(false),        lcd_showrecstatus(false),
      lcd_backlighton(false),       lcd_heartbeaton(false),

      lcd_popuptime(0),

      lcd_showmusic_items(QString::null),
      lcd_keystring(QString::null),

      GetLEDMask(NULL)
{

    setObjectName("LCD");

    // Constructor for LCD
    //
    // Note that this does *not* include opening the socket and initiating 
    // communications with the LDCd daemon.

    VERBOSE(VB_GENERAL|VB_EXTRA, LOC + "An LCD object now exists "
                                      "(LCD() was called)");

    connect(retryTimer, SIGNAL(timeout()),   this, SLOT(restartConnection()));
    connect(LEDTimer,   SIGNAL(timeout()),   this, SLOT(outputLEDs()));
}

bool LCD::m_enabled = false;
bool LCD::m_server_unavailable = false;
class LCD *LCD::m_lcd = NULL;

class LCD * LCD::Get(void)
{
    if (m_enabled && m_lcd == NULL && m_server_unavailable == false)
        m_lcd = new LCD;
    return m_lcd;
}

void LCD::SetupLCD (void) 
{
    QString lcd_host;
    int lcd_port;

    if (m_lcd) 
    {
        delete m_lcd;
        m_lcd = NULL;
        m_server_unavailable = false;
    }

    lcd_host = GetMythDB()->GetSetting("LCDServerHost", "localhost");
    lcd_port = GetMythDB()->GetNumSetting("LCDServerPort", 6545);
    m_enabled = GetMythDB()->GetNumSetting("LCDEnable", 0);

    if (m_enabled && lcd_host.length() > 0 && lcd_port > 1024)
    {
        class LCD * lcd = LCD::Get();
        if (lcd->connectToHost(lcd_host, lcd_port) == false) 
        {
            delete m_lcd;
            m_lcd = NULL;
            m_server_unavailable = false;
        }
    }
}

bool LCD::connectToHost(const QString &lhostname, unsigned int lport)
{
    QMutexLocker locker(&socketLock);

    VERBOSE(VB_NETWORK|VB_EXTRA, QString(LOC + "connecting to host: %1 - port: %2")
            .arg(lhostname).arg(lport));

    // Open communications
    // Store the hostname and port in case we need to reconnect.
    hostname = lhostname;
    port = lport;

    // Don't even try to connect if we're currently disabled.
    if (!(m_enabled = GetMythDB()->GetNumSetting("LCDEnable", 0)))
    {
        bConnected = false;
        m_server_unavailable = true;
        return bConnected;
    }

    // check if the 'mythlcdserver' is running
    int res = system("ret=`ps cax | grep -c mythlcdserver`; exit $ret");
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);

    if (res == 0)
    {
        // we need to start the mythlcdserver 
        VERBOSE(VB_GENERAL, "Starting mythlcdserver");
        system(qPrintable(GetInstallPrefix() + "/bin/mythlcdserver -v none&"));
        usleep(500000);
    }

    if (!bConnected)
    {
        int count = 0;
        do
        {
            ++count;

            VERBOSE(VB_GENERAL, QString("Connecting to lcd server: " 
                    "%1:%2 (try %3 of 10)").arg(hostname).arg(port)
                                           .arg(count));

            if (socket)
                socket->DownRef();

            socket = new MythSocket();
            socket->setCallbacks(this);

            if (socket->connect(hostname, port))
            {
                lcd_ready = false;
                bConnected = true;
                QTextStream os(socket);
                os << "HELLO\n";

                // HACK make sure the ready read thread is awake
                usleep(1000);
                socket->Lock();
                socket->Unlock();

                break;
            }

            usleep(500000);
        }
        while (count < 10 && !bConnected);
    }

    if (bConnected == false)
        m_server_unavailable = true;

    return bConnected;
}

void LCD::sendToServer(const QString &someText)
{
    QMutexLocker locker(&socketLock);

    if (!socket || !lcd_ready)
        return;

    // Check the socket, make sure the connection is still up
    if (socket->state() == MythSocket::Idle)
    {
        lcd_ready = false;

        // Ack, connection to server has been severed try to re-establish the 
        // connection
        retryTimer->setSingleShot(false);
        retryTimer->start(10000);
        VERBOSE(VB_IMPORTANT, "lcddevice: Connection to LCDServer died unexpectedly.\n\t\t\t"
                         "Trying to reconnect every 10 seconds. . .");

        bConnected = false;
        return;
    }

    QTextStream os(socket);
    os.setCodec(QTextCodec::codecForName("ISO 8859-1"));

    last_command = someText;

    if (bConnected)
    {
        VERBOSE(VB_NETWORK|VB_EXTRA, QString(LOC + "Sending to Server: %1").arg(someText));

        // Just stream the text out the socket
        os << someText << "\n";
    }
    else
    {
        // Buffer this up in the hope that the connection will open soon

        send_buffer += someText;
        send_buffer += "\n";
    }
}

void LCD::restartConnection()
{
    // Reset the flag
    lcd_ready = false;
    bConnected = false;
    m_server_unavailable = false;

    // Retry to connect. . .  Maybe the user restarted LCDd?
    connectToHost(hostname, port);
}

void LCD::readyRead(MythSocket *sock)
{
    (void) sock;

    QMutexLocker locker(&socketLock);

    QString lineFromServer, tempString;
    QStringList aList;
    QStringList::Iterator it;

    // This gets activated automatically by the MythSocket class whenever
    // there's something to read.
    //
    // We currently spend most of our time (except for the first line sent 
    // back) ignoring it.

    int dataSize = socket->bytesAvailable() + 1; 
    QByteArray data(dataSize + 1, 0);

    socket->readBlock(data.data(), dataSize);

    lineFromServer = data;
    lineFromServer = lineFromServer.replace( QRegExp("\n"), " " );
    lineFromServer = lineFromServer.replace( QRegExp("\r"), " " );
    lineFromServer = lineFromServer.simplified();

    // Make debugging be less noisy
    if (lineFromServer != "OK")
        VERBOSE(VB_NETWORK|VB_EXTRA, QString(LOC + "Received from server: %1")
                .arg(lineFromServer));

    aList = lineFromServer.split(" ");
    if (aList[0] == "CONNECTED")
    {
        // We got "CONNECTED", which is a response to "HELLO"
        // get lcd width & height
        if (aList.count() != 3)
        {
            VERBOSE(VB_IMPORTANT, LOC + "received bad no. of arguments "
                            "in CONNECTED response from LCDServer");
        }

        bool bOK;
        lcd_width = aList[1].toInt(&bOK);
        if (!bOK)
        {
            VERBOSE(VB_IMPORTANT, LOC + "received bad int for width"
                            "in CONNECTED response from LCDServer");
        }

        lcd_height = aList[2].toInt(&bOK);
        if (!bOK)
        {
            VERBOSE(VB_IMPORTANT, LOC + "received bad int for height"
                            "in CONNECTED response from LCDServer");
        }

        init();
    }
    else if (aList[0] == "HUH?")
    {
        VERBOSE(VB_IMPORTANT, LOC + "WARNING: Something is getting passed"
                        "to LCDServer that it doesn't understand");
        VERBOSE(VB_IMPORTANT, QString(LOC + "last command: %1").arg( last_command ));
    }
    else if (aList[0] == "KEY")
        handleKeyPress(aList.last().trimmed());
}

void LCD::handleKeyPress(QString key_pressed)
{
    int key = 0;

    QChar mykey = key_pressed.at(0);
    if (mykey == lcd_keystring.at(0))
        key = Qt::Key_Up;
    else if (mykey == lcd_keystring.at(1))
        key = Qt::Key_Down;
    else if (mykey == lcd_keystring.at(2))
        key = Qt::Key_Left;
    else if (mykey == lcd_keystring.at(3))
        key = Qt::Key_Right;
    else if (mykey == lcd_keystring.at(4))
        key = Qt::Key_Space;
    else if (mykey == lcd_keystring.at(5))
        key = Qt::Key_Escape;

    QApplication::postEvent((QObject *)(QApplication::activeWindow()),
                            new ExternalKeycodeEvent(key));
}

void LCD::init()
{
    // Stop the timer
    retryTimer->stop();

    // Get LCD settings
    lcd_showmusic = (GetMythDB()->GetSetting("LCDShowMusic", "1") == "1");
    lcd_showtime = (GetMythDB()->GetSetting("LCDShowTime", "1") == "1");
    lcd_showchannel = (GetMythDB()->GetSetting("LCDShowChannel", "1") == "1");
    lcd_showgeneric = (GetMythDB()->GetSetting("LCDShowGeneric", "1") == "1");
    lcd_showvolume = (GetMythDB()->GetSetting("LCDShowVolume", "1") == "1");
    lcd_showmenu = (GetMythDB()->GetSetting("LCDShowMenu", "1") == "1");
    lcd_showrecstatus = (GetMythDB()->GetSetting("LCDShowRecStatus", "1") == "1");
    lcd_keystring = GetMythDB()->GetSetting("LCDKeyString", "ABCDEF");    

    bConnected = true;
    lcd_ready = true;

    // send buffer if there's anything in there
    if (send_buffer.length() > 0)
    {
        sendToServer(send_buffer);
        send_buffer = "";
    }
}

void LCD::connectionClosed(MythSocket *sock)
{
    (void) sock;
    bConnected = false;
}

void LCD::connectionFailed(MythSocket *sock)
{
    QMutexLocker locker(&socketLock);
    QString err = sock->errorToString();
    VERBOSE(VB_IMPORTANT, QString("Could not connect to LCDServer: %1").arg(err));
}

void LCD::stopAll()
{
    if (!lcd_ready)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, "lcddevice: stopAll");

    sendToServer("STOP_ALL");
}

void LCD::setChannelProgress(float value)
{
    if (!lcd_ready || !lcd_showchannel)
        return;

    value = min(max(0.0f, value), 1.0f);
    sendToServer(QString("SET_CHANNEL_PROGRESS %1").arg(value));
}

void LCD::setGenericProgress(float value)
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

    value = min(max(0.0f, value), 1.0f);
    sendToServer(QString("SET_GENERIC_PROGRESS 0 %1").arg(value));
}

void LCD::setGenericBusy()
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

    sendToServer("SET_GENERIC_PROGRESS 1 0.0");
}

void LCD::setMusicProgress(QString time, float value)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    value = min(max(0.0f, value), 1.0f);
    sendToServer("SET_MUSIC_PROGRESS " + quotedString(time) + " " + 
            QString().setNum(value));    
}

void LCD::setMusicShuffle(int shuffle)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP SHUFFLE %1").arg(shuffle));
}

void LCD::setMusicRepeat(int repeat)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    sendToServer(QString("SET_MUSIC_PLAYER_PROP REPEAT %1").arg(repeat));
}

void LCD::setVolumeLevel(float value)
{
    if (!lcd_ready || !lcd_showvolume)
        return;

    if (value < 0.0)
        value = 0.0;
    else if (value > 1.0)
        value = 1.0;

    sendToServer("SET_VOLUME_LEVEL " + QString().setNum(value));    
}

void LCD::setupLEDs(int(*LedMaskFunc)(void))
{ 
    GetLEDMask = LedMaskFunc;
    // update LED status every 10 seconds
    LEDTimer->setSingleShot(false);
    LEDTimer->start(10000); 
}

void LCD::outputLEDs()
{
    if (!lcd_ready)
        return;

    QString aString;
    int mask = 0;
    if (0 && GetLEDMask)
        mask = GetLEDMask();
    aString = "UPDATE_LEDS ";
    aString += QString::number(mask);
    sendToServer(aString);
}

void LCD::switchToTime()
{
    if (!lcd_ready || !lcd_showtime)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToTime");

    sendToServer("SWITCH_TO_TIME");
}

void LCD::switchToMusic(const QString &artist, const QString &album, const QString &track)
{
    if (!lcd_ready || !lcd_showmusic)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToMusic");

    sendToServer("SWITCH_TO_MUSIC " + quotedString(artist) + " " 
            + quotedString(album) + " " 
            + quotedString(track));
}

void LCD::switchToChannel(QString channum, QString title, QString subtitle)
{
    if (!lcd_ready || !lcd_showchannel)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToChannel");

    sendToServer("SWITCH_TO_CHANNEL " + quotedString(channum) + " " 
            + quotedString(title) + " " 
            + quotedString(subtitle));
}

void LCD::switchToMenu(QList<LCDMenuItem> &menuItems, QString app_name,
                       bool popMenu)
{
    if (!lcd_ready || !lcd_showmenu)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToMenu");

    if (menuItems.isEmpty())
        return;

    QString s = "SWITCH_TO_MENU ";

    s += quotedString(app_name);
    s += " " + QString(popMenu ? "TRUE" : "FALSE");


    QListIterator<LCDMenuItem> it(menuItems);
    const LCDMenuItem *curItem;

    while (it.hasNext())
    {
        curItem = &(it.next());
        s += " " + quotedString(curItem->ItemName());

        if (curItem->isChecked() == CHECKED)
            s += " CHECKED";    
        else if (curItem->isChecked() == UNCHECKED) 
            s += " UNCHECKED";
        else if (curItem->isChecked() == NOTCHECKABLE)
            s += " NOTCHECKABLE";

        s += " " + QString(curItem->isSelected() ? "TRUE" : "FALSE");
        s += " " + QString(curItem->Scroll() ? "TRUE" : "FALSE");
        QString sIndent;
        sIndent.setNum(curItem->getIndent());
        s += " " + sIndent;
    }

    sendToServer(s);
}

void LCD::switchToGeneric(QList<LCDTextItem> &textItems)
{
    if (!lcd_ready || !lcd_showgeneric)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToGeneric ");

    if (textItems.isEmpty())
        return;

    QString s = "SWITCH_TO_GENERIC";

    QListIterator<LCDTextItem> it(textItems);
    const LCDTextItem *curItem;

    while (it.hasNext())
    {
        curItem = &(it.next());

        QString sRow;
        sRow.setNum(curItem->getRow());
        s += " " + sRow;

        if (curItem->getAlignment() == ALIGN_LEFT)
            s += " ALIGN_LEFT";    
        else if (curItem->getAlignment() == ALIGN_RIGHT) 
            s += " ALIGN_RIGHT";
        else if (curItem->getAlignment() == ALIGN_CENTERED)
            s += " ALIGN_CENTERED";

        s += " " + quotedString(curItem->getText());
        s += " " + quotedString(curItem->getScreen());
        s += " " + QString(curItem->getScroll() ? "TRUE" : "FALSE");
    }

    sendToServer(s);
}

void LCD::switchToVolume(QString app_name)
{
    if (!lcd_ready || !lcd_showvolume)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToVolume ");

    sendToServer("SWITCH_TO_VOLUME " + quotedString(app_name));
}

void LCD::switchToNothing()
{
    if (!lcd_ready)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "switchToNothing");

    sendToServer("SWITCH_TO_NOTHING");
}

void LCD::shutdown()
{
    QMutexLocker locker(&socketLock);

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "shutdown");

    if (socket)
        socket->close();

    lcd_ready = false;
    bConnected = false;
}

void LCD::resetServer()
{
    QMutexLocker locker(&socketLock);

    if (!lcd_ready)
        return;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "RESET");

    sendToServer("RESET");
}

LCD::~LCD()
{
    m_lcd = NULL;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "An LCD device is being snuffed out of "
                    "existence (~LCD() was called)");

    if (socket)
    {
        socket->DownRef();
        lcd_ready = false;
    }
}

// this does nothing
void LCD::setLevels(int numbLevels, float *values)
{
    numbLevels = numbLevels;
    values = values;

    VERBOSE(VB_IMPORTANT|VB_EXTRA, LOC + "setLevels");
}

QString LCD::quotedString(const QString &s)
{
    QString sRes = s;
    sRes.replace(QRegExp("\""), QString("\"\""));
    sRes = "\"" + sRes + "\"";

    return(sRes);
}
