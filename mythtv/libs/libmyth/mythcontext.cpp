#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qdir.h>
#include <qpainter.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qurl.h>
#include <qnetwork.h>

#include <qsocketdevice.h>
#include <qhostaddress.h>

#include <cmath>
#include <queue>

#include "mythcontext.h"
#include "oldsettings.h"
#include "themedmenu.h"
#include "util.h"
#include "remotefile.h"
#include "lcddevice.h"
#include "dialogbox.h"
#include "mythdialogs.h"
#include "mythplugin.h"
#include "screensaver.h"
#include "DisplayRes.h"

MythContext *gContext = NULL;

int print_verbose_messages = VB_IMPORTANT | VB_GENERAL;

class MythContextPrivate
{
  public:
    MythContextPrivate(MythContext *lparent);
   ~MythContextPrivate();

    void Init(bool gui, bool lcd);

    void LoadLogSettings(void);

    MythContext *parent;

    Settings *m_settings;
    Settings *m_qtThemeSettings;

    QString m_installprefix;

    bool m_themeloaded;
    QString m_menuthemepathname;
    QString m_themepathname;
    QPixmap *m_backgroundimage;
    QPalette m_palette;

    int m_xbase, m_ybase;
    int m_height, m_width;

    QString m_localhostname;

    QMutex serverSockLock;

    QPtrList<QObject> listeners;

    QSqlDatabase* m_db;
    QMutex dbLock;

    QMap<QString, QImage> imageCache;

    LCD *lcd_device;

    QString language;

    MythMainWindow *mainWindow;

    float m_wmult, m_hmult;
    int m_screenwidth, m_screenheight;

    QString themecachedir;

    int bigfontsize, mediumfontsize, smallfontsize;

    QSocketDevice *serverSock;
    QSocket *eventSock;

    bool disablelibrarypopup;

    MythPluginManager *pluginmanager;

    ScreenSaverControl *screensaver;

    int m_logenable, m_logmaxcount, m_logprintlevel;
    QMap<QString,int> lastLogCounts;
    QMap<QString,QString> lastLogStrings;

    bool screensaverEnabled;

    DisplayRes *display_res;

    QMutex *m_priv_mutex;
    queue<MythPrivRequest> m_priv_requests;
};

MythContextPrivate::MythContextPrivate(MythContext *lparent)
{
    pluginmanager = NULL;

    parent = lparent;

    char *tmp_installprefix = getenv("MYTHTVDIR");
    if (tmp_installprefix)
        m_installprefix = tmp_installprefix;
    else
        m_installprefix = PREFIX;

    m_settings = new Settings;
    m_qtThemeSettings = new Settings;

    language = "";
    m_themeloaded = false;
    m_backgroundimage = NULL;

    m_db = QSqlDatabase::addDatabase("QMYSQL3", "MythContext");
    screensaver = new ScreenSaverControl();
}

void MythContextPrivate::Init(bool gui, bool lcd)
{
    if (!parent->LoadSettingsFiles("mysql.txt"))
        cerr << "Unable to read configuration file mysql.txt" << endl;

    m_localhostname = m_settings->GetSetting("LocalHostName", NULL);
    if (m_localhostname == NULL)
    {
        char localhostname[1024];
        if (gethostname(localhostname, 1024))
        {
            perror("gethostname");
            VERBOSE(VB_IMPORTANT, "MCP: Could not determine host name, exiting");
            exit(-29);
        }
        m_localhostname = localhostname;
    }

    m_xbase = m_ybase = 0;

    if (gui)
    {
        m_height = QApplication::desktop()->height();
        m_width = QApplication::desktop()->width();
    }
    else
    {
        m_height = m_width = 0;
    }

    int tmpwidth = parent->GetNumSetting("GuiWidth");
    int tmpheight = parent->GetNumSetting("GuiHeight");

    if (tmpwidth > 0)
        m_width = tmpwidth;
    if (tmpheight > 0)
        m_height = tmpheight;

    serverSock = NULL;
    eventSock = new QSocket(0);

    m_logenable = -1;
    m_logmaxcount = -1;
    m_logprintlevel = -1;

    mainWindow = NULL;

    if (lcd)
        lcd_device = new LCD();
    else
        lcd_device = NULL;

    disablelibrarypopup = false;

    display_res = NULL;

    m_priv_mutex = new QMutex(true);
}

MythContextPrivate::~MythContextPrivate()
{
    imageCache.clear();

    if (m_settings)
        delete m_settings;
    if (m_qtThemeSettings)
        delete m_qtThemeSettings;
    if (serverSock)
        delete serverSock;
    if (eventSock)
        delete eventSock;
    if (lcd_device)
        delete lcd_device;
    if (m_priv_mutex)
        delete m_priv_mutex;
}

void MythContextPrivate::LoadLogSettings(void)
{
    m_logenable = parent->GetNumSetting("LogEnabled", 0);
    m_logmaxcount = parent->GetNumSetting("LogMaxCount", 0);
    m_logprintlevel = parent->GetNumSetting("LogPrintLevel", LP_ERROR);
}

MythContext::MythContext(const QString &binversion, bool gui, bool lcd)
           : QObject()
{
    qInitNetworkProtocols();

    if (binversion != MYTH_BINARY_VERSION)
    {
        VERBOSE(VB_IMPORTANT, QString("This app was compiled against libmyth version: %1")
                .arg(binversion));
        VERBOSE(VB_IMPORTANT, QString("but the library is version: %1").arg(MYTH_BINARY_VERSION));
        VERBOSE(VB_IMPORTANT, "You probably want to recompile everything, and do a");
        VERBOSE(VB_IMPORTANT, "'make distclean' first. exiting.");
        exit(-30);
    }

    d = new MythContextPrivate(this);

    d->Init(gui, lcd);

    connect(d->eventSock, SIGNAL(connected()), 
            this, SLOT(EventSocketConnected()));
    connect(d->eventSock, SIGNAL(readyRead()), 
            this, SLOT(EventSocketRead()));
    connect(d->eventSock, SIGNAL(connectionClosed()), 
            this, SLOT(EventSocketClosed()));
}

MythContext::~MythContext()
{
    if (d)
        delete d;
}

bool MythContext::ConnectToMasterServer(void)
{
    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);
    return ConnectServer(server, port);
}

bool MythContext::ConnectServer(const QString &hostname, int port)
{
    if (d->serverSock)
        return true;

    int cnt = 0;

    int sleepTime = GetNumSetting("WOLbackendReconnectWaitTime", 0);
    int maxConnTry = GetNumSetting("WOLbackendConnectRetry", 1);

    do
    {
        VERBOSE(VB_GENERAL, QString("Connecting to backend server: "
                                    "%1:%2 (try %3 of %4)")
                                    .arg(hostname).arg(port).arg(cnt+1)
                                    .arg(maxConnTry));

        d->serverSock = new QSocketDevice(QSocketDevice::Stream);

        if (!connectSocket(d->serverSock, hostname, port))
        {
            delete d->serverSock;
            d->serverSock = NULL;
        
            // only inform the user of a failure if WOL is disabled 
            if (sleepTime <= 0)
            {
                cerr << "Connection timed out.\n";
                cerr << "You probably should modify the Master Server "
                     << "settings\nin the setup program and set the proper "
                     << "IP address.\n";
                if (d->m_height && d->m_width)
                {
                    MythPopupBox::showOkPopup(d->mainWindow, 
                                              "connection failure",
                                              tr("Could not connect to the "
                                                 "master backend server -- is "
                                                 "it running?  Is the IP "
                                                 "address set for it in the "
                                                 "setup program correct?"));
                }

                return false;
            }
            else
            {
                VERBOSE(VB_GENERAL, "Trying to wake up the MasterBackend "
                                    "now.");
                QString wol_cmd = GetSetting("WOLbackendCommand",
                                             "echo \'would run the "
                                             "WakeServerCommand now, if "
                                             "set!\'");
                system(wol_cmd.ascii());

                VERBOSE(VB_GENERAL, QString("Waiting for %1 seconds until I "
                                            "try to reconnect again.")
                                            .arg(sleepTime));
                sleep(sleepTime);
                ++cnt;
            }
        }
        else
            break;
    }
    while (cnt <= maxConnTry);

    if (!CheckProtoVersion(d->serverSock))
    {
        delete d->serverSock;
        d->serverSock = NULL;
        return false;
    }

    QString str = QString("ANN Playback %1 %2")
                         .arg(d->m_localhostname).arg(false);
    QStringList strlist = str;
    WriteStringList(d->serverSock, strlist);
    ReadStringList(d->serverSock, strlist, true);
    
    d->eventSock->connectToHost(hostname, port);
    
    return true;
}

bool MythContext::IsConnectedToMaster(void)
{
    return d->serverSock;
}

QString MythContext::GetMasterHostPrefix(void)
{
    QString ret = "";

    if (!d->serverSock)
        ConnectToMasterServer();
    
    if (d->serverSock)
        ret = QString("myth://%1:%2/")
                     .arg(d->serverSock->peerAddress().toString())
                     .arg(d->serverSock->peerPort());
    return ret;
}

QString MythContext::GetHostName(void)
{
    return d->m_localhostname; 
}

QString MythContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

QString MythContext::GetInstallPrefix(void) 
{ 
    return d->m_installprefix; 
}

QString MythContext::GetShareDir(void) 
{ 
    return d->m_installprefix + "/share/mythtv/"; 
}

bool MythContext::LoadSettingsFiles(const QString &filename)
{
    return d->m_settings->LoadSettingsFiles(filename, d->m_installprefix);
}

void MythContext::LoadQtConfig(void)
{
    d->language = "";
    d->themecachedir = "";

    d->m_xbase = GetNumSetting("GuiOffsetX", 0);
    d->m_ybase = GetNumSetting("GuiOffsetY", 0);

    d->m_width = GetNumSetting("GuiWidth", 0);
    d->m_height = GetNumSetting("GuiHeight", 0);

    if ((d->display_res = DisplayRes::getDisplayRes()))
    {
        // Make sure DisplayRes has current context info
        d->display_res->Initialize();

        // Switch to desired GUI resolution
        d->display_res->switchToGUI();

        d->m_width = d->display_res->Width();
        d->m_height = d->display_res->Height();
    }
    else if (d->m_width <= 0 || d->m_height <= 0)
    {
        d->m_height = QApplication::desktop()->height();
        d->m_width = QApplication::desktop()->width();
    }

    if (d->m_qtThemeSettings)
        delete d->m_qtThemeSettings;

    d->m_qtThemeSettings = new Settings;

    QString style = GetSetting("Style", "");
    if (style != "")
        qApp->setStyle(style);

    QString themename = GetSetting("Theme");
    QString themedir = FindThemeDir(themename);
    
    d->m_themepathname = themedir + "/";
    
    themedir += "/qtlook.txt";
    d->m_qtThemeSettings->ReadSettings(themedir);
    d->m_themeloaded = false;

    if (d->m_backgroundimage)
        delete d->m_backgroundimage;
    d->m_backgroundimage = NULL;

    themename = GetSetting("MenuTheme");
    d->m_menuthemepathname = FindThemeDir(themename) +"/";
    
    InitializeScreenSettings();
}

void MythContext::RefreshBackendConfig(void)
{
    d->LoadLogSettings();
}

void MythContext::UpdateImageCache(void)
{
    d->imageCache.clear();

    ClearOldImageCache();
    CacheThemeImages();
}

void MythContext::ClearOldImageCache(void)
{
    QString cachedirname = QDir::homeDirPath() + "/.mythtv/themecache/";

    d->themecachedir = cachedirname + GetSetting("Theme") + "." + 
                       QString::number(d->m_screenwidth) + "." + 
                       QString::number(d->m_screenheight);

    QDir dir(cachedirname);

    if (!dir.exists())
        dir.mkdir(cachedirname);

    QString themecachedir = d->themecachedir;

    d->themecachedir += "/";

    dir.setPath(themecachedir);
    if (!dir.exists())
        dir.mkdir(themecachedir);

    dir.setPath(cachedirname);

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && !fi->isSymLink())
        {
            if (fi->absFilePath() == themecachedir)
                continue;
            RemoveCacheDir(fi->absFilePath());
        }
    }
}

void MythContext::RemoveCacheDir(const QString &dirname)
{
    QString cachedirname = QDir::homeDirPath() + "/.mythtv/themecache/";

    if (!dirname.startsWith(cachedirname))
        return;

    cout << "removing stale cache dir: " << dirname << endl;

    QDir dir(dirname);

    if (!dir.exists())
        return;

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isFile() && !fi->isSymLink())
        {
            QFile file(fi->absFilePath());
            file.remove();
        }
        else if (fi->isDir() && !fi->isSymLink())
        {
            RemoveCacheDir(fi->absFilePath());
        }
    }

    dir.rmdir(dirname);
}
    
void MythContext::CacheThemeImages(void)
{
    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    if (d->m_screenwidth == 800 && d->m_screenheight == 600)
        return;

    CacheThemeImagesDirectory(d->m_themepathname);
    CacheThemeImagesDirectory(baseDir);
}

void MythContext::CacheThemeImagesDirectory(const QString &dirname,
                                            const QString &subdirname)
{
    QDir dir(dirname);
    
    if (!dir.exists())
        return;

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    MythProgressDialog *caching = NULL;
    if (subdirname.length() == 0)
        caching = new MythProgressDialog(QObject::tr("Pre-scaling theme images"),
                                         list->count());

    int progress = 0;

    QString destdir = d->themecachedir;
    if (subdirname.length() > 0)
        destdir += subdirname + "/";

    while ((fi = it.current()) != 0)
    {
        if (caching)
            caching->setProgress(progress);
        progress++;

        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        if (fi->isDir() && subdirname.length() == 0)
        {
            QString newdirname = fi->fileName();
            QDir newsubdir(d->themecachedir + newdirname);
            if (!newsubdir.exists())
                newsubdir.mkdir(d->themecachedir + newdirname);

            CacheThemeImagesDirectory(dirname + "/" + newdirname,
                                      newdirname);
            continue;
        }
        else if (fi->isDir())
            continue;

        if (fi->extension().lower() != "png" && 
            fi->extension().lower() != "jpg" &&
            fi->extension().lower() != "gif" &&
            fi->extension().lower() != "jpeg")
            continue;

        QString filename = fi->fileName();
        QFileInfo cacheinfo(destdir + filename);

        if (!cacheinfo.exists() ||
            (cacheinfo.lastModified() < fi->lastModified()))
        {
            VERBOSE(VB_FILE, QString("generating cache image for: %1")
                    .arg(fi->absFilePath()));

            QImage *tmpimage = LoadScaleImage(fi->absFilePath(), false);

            if (tmpimage && tmpimage->width() > 0 && tmpimage->height() > 0)
            {
                if (!tmpimage->save(destdir + filename, "PNG"))
                {
                    cerr << "Couldn't save cache cache image: "
                         << d->themecachedir + filename << endl;
                }
             
                delete tmpimage;
            }
        }
    }

    if (caching)
    {
        caching->Close();
        delete caching;
    }
}

void MythContext::GetScreenSettings(float &wmult, float &hmult)
{
    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::GetScreenSettings(int &width, float &wmult, 
                                    int &height, float &hmult)
{
    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::GetScreenSettings(int &xbase, int &width, float &wmult, 
                                    int &ybase, int &height, float &hmult)
{
    xbase  = d->m_xbase;
    ybase  = d->m_ybase;
    
    height = d->m_screenheight;
    width = d->m_screenwidth;

    wmult = d->m_wmult;
    hmult = d->m_hmult;
}

void MythContext::InitializeScreenSettings()
{
    int x = 0, y = 0, w = 0, h = 0;

#if defined(Q_WS_X11)
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    d->m_xbase = x + GetNumSetting("GuiOffsetX", 0);
    d->m_ybase = y + GetNumSetting("GuiOffsetY", 0);

    int height = GetNumSetting("GuiHeight", d->m_height);
    int width = GetNumSetting("GuiWidth", d->m_width);

    if (width == 0 && height == 0 && d->display_res)
    {
        // If using custom, full-screen resolution, note the size
        height = d->display_res->Height();
        width  = d->display_res->Width();
    }
    else
    {
        if (w != 0 && h != 0 && height == 0 && width == 0)
        {
            width = w;
            height = h;
        }
        else
        {
            if (height == 0)
                height = d->m_height;
            if (width == 0)
                width = d->m_width;
        }
    }

    
    if (height < 160 || width < 160)
    {
        cerr << "Somehow, your screen size settings are bad.\n";
        cerr << "GuiHeight: " << GetNumSetting("GuiHeight") << endl;
        cerr << "GuiWidth: " << GetNumSetting("GuiWidth") << endl;
        cerr << "m_height: " << d->m_height << endl;
        cerr << "m_width: " << d->m_width << endl;
        cerr << "Falling back to 640x480\n";

        width = 640;
        height = 480;
    }

    d->m_wmult = width / 800.0;
    d->m_hmult = height / 600.0;
    d->m_screenwidth = width;   
    d->m_screenheight = height;

    d->bigfontsize = GetNumSetting("QtFontBig", 25);
    d->mediumfontsize = GetNumSetting("QtFontMedium", 16);
    d->smallfontsize = GetNumSetting("QtFontSmall", 12);
}

QString MythContext::FindThemeDir(const QString &themename)
{
    QString testdir = QDir::homeDirPath() + "/.mythtv/themes/" + themename;

    QDir dir(testdir);
    if (dir.exists())
        return testdir;

    testdir = d->m_installprefix + "/share/mythtv/themes/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;

    testdir = "../menutest/" + themename;
    dir.setPath(testdir);
    if (dir.exists())
        return testdir;


    // Don't complain about the "default" theme being missing
    if (themename == QObject::tr("Default"))
    {
        testdir = d->m_installprefix + "/share/mythtv/";
        dir.setPath(testdir);
        if (dir.exists())
            return testdir;
    }
    
    cerr << "Could not find theme: " << themename << endl;
    return "";
}
QString MythContext::GetMenuThemeDir(void)
{
    return d->m_menuthemepathname;
}

QString MythContext::GetThemeDir(void)
{
    return d->m_themepathname;
}

int MythContext::OpenDatabase(QSqlDatabase *db)
{
    d->dbLock.lock();
    if (!d->m_db->isOpen()) {
        d->m_db->setDatabaseName(d->m_settings->GetSetting("DBName"));
        d->m_db->setUserName(d->m_settings->GetSetting("DBUserName"));
        d->m_db->setPassword(d->m_settings->GetSetting("DBPassword"));
        d->m_db->setHostName(d->m_settings->GetSetting("DBHostName"));
        d->m_db->open();
    }
    d->dbLock.unlock();
        
    db->setDatabaseName(d->m_settings->GetSetting("DBName"));
    db->setUserName(d->m_settings->GetSetting("DBUserName"));
    db->setPassword(d->m_settings->GetSetting("DBPassword"));
    db->setHostName(d->m_settings->GetSetting("DBHostName"));
 
    int res = db->open();
    if (!res)
        cerr << "Unable to connect to database!" << endl
             << DBErrorMessage(db->lastError()) << endl;
    return res;
}

void MythContext::KickDatabase(QSqlDatabase *db)
{
    // Some explanation is called for.  This exists because the mysql
    // driver does not gracefully handle the situation where a TCP
    // socketconnection is dropped (for example due to a timeout).  If
    // a Unix domain socket connection is lost, the driver
    // transparently reestablishes the connection and we don't even
    // notice.  However, when this happens with a TCP connection, the
    // driver returns an error for the next query to be executed, and
    // THEN reestablishes the connection (so the second query succeeds
    // with no intervention).
    // mdz, 2003/08/11

    QString query("SELECT NULL;");
    for (unsigned int i = 0 ; i < 2 ; ++i, usleep(50000)) 
    {
        QSqlQuery result = db->exec(query);
        if (result.isActive())
            break;
        else
            DBError("KickDatabase", result);
    }
}

void MythContext::DBError(const QString &where, const QSqlQuery& query) 
{
    if (query.lastError().type())
    {
        cerr << "DB Error (" << where << "):" << endl;
    }

    cerr << "Query was:" << endl
         << query.lastQuery() << endl
         << DBErrorMessage(query.lastError()) << endl;
}

QString MythContext::DBErrorMessage(const QSqlError& err)
{
    if (!err.type())
        return "No error type from QSqlError?  Strange...";

    return QString("Driver error was [%1/%2]:\n"
                   "%3\n"
                   "Database error was:\n"
                   "%4\n")
        .arg(err.type())
        .arg(err.number())
        .arg(err.driverText())
        .arg(err.databaseText());
}

Settings *MythContext::settings(void)
{
    return d->m_settings;
}

Settings *MythContext::qtconfig(void)
{
    return d->m_qtThemeSettings;
}

void MythContext::SaveSetting(const QString &key, int newValue)
{
    QString strValue = QString("%1").arg(newValue);

    SaveSetting(key, strValue);
}

void MythContext::SaveSetting(const QString &key, const QString &newValue)
{
    d->dbLock.lock();

    if (d->m_db->isOpen()) 
    {
        KickDatabase(d->m_db);

        QString querystr = QString("DELETE FROM settings WHERE value = \"%1\" "
                                   "AND hostname = \"%2\";")
                                  .arg(key).arg(d->m_localhostname);

        QSqlQuery result = d->m_db->exec(querystr);
        if (!result.isActive())
            MythContext::DBError("Clear setting", querystr);

        querystr = QString("INSERT settings ( value, data, hostname ) "
                           "VALUES ( \"%1\", \"%2\", \"%3\" );")
                           .arg(key).arg(newValue).arg(d->m_localhostname);

        result = d->m_db->exec(querystr);
        if (!result.isActive())
            MythContext::DBError("Save new setting", querystr);
    }

    d->dbLock.unlock();
}

QString MythContext::GetSetting(const QString &key, const QString &defaultval) 
{
    bool found = false;
    QString value;

    d->dbLock.lock();

    if (d->m_db->isOpen()) 
    {
        KickDatabase(d->m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1' "
                                "AND hostname = '%2';")
                               .arg(key).arg(d->m_localhostname);

        QSqlQuery result = d->m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0) 
        {
            result.next();
            value = QString::fromUtf8(result.value(0).toString());
            found = true;
        }
        else
        {
            query = QString("SELECT data FROM settings WHERE value = '%1' AND "
                            "hostname IS NULL;").arg(key);

            result = d->m_db->exec(query);

            if (result.isActive() && result.numRowsAffected() > 0) 
            {
                result.next();
                value = QString::fromUtf8(result.value(0).toString());
                found = true;
            }
        }
    }
    d->dbLock.unlock();

    if (found)
        return value;
    return d->m_settings->GetSetting(key, defaultval); 
}

int MythContext::GetNumSetting(const QString &key, int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSetting(key, val);

    return retval.toInt();
}

QString MythContext::GetSettingOnHost(const QString &key, const QString &host,
                                      const QString &defaultval)
{
    bool found = false;
    QString value = defaultval;

    d->dbLock.lock();

    if (d->m_db->isOpen())
    {
        KickDatabase(d->m_db);

        QString query = QString("SELECT data FROM settings WHERE value = '%1' "
                                "AND hostname = '%2';").arg(key).arg(host);

        QSqlQuery result = d->m_db->exec(query);

        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            value = result.value(0).toString();
            found = true;
        }
    }

    d->dbLock.unlock();

    return value;
}

int MythContext::GetNumSettingOnHost(const QString &key, const QString &host,
                                     int defaultval)
{
    QString val = QString::number(defaultval);
    QString retval = GetSettingOnHost(key, host, val);

    return retval.toInt();
}

void MythContext::SetPalette(QWidget *widget)
{
    QPalette pal = widget->palette();

    QColorGroup active = pal.active();
    QColorGroup disabled = pal.disabled();
    QColorGroup inactive = pal.inactive();

    const QString names[] = { "Foreground", "Button", "Light", "Midlight",  
                              "Dark", "Mid", "Text", "BrightText", "ButtonText",
                              "Base", "Background", "Shadow", "Highlight",
                              "HighlightedText" };

    QString type = "Active";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Active, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Disabled";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Disabled, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    type = "Inactive";
    for (int i = 0; i < 13; i++)
    {
        QString color = d->m_qtThemeSettings->GetSetting(type + names[i]);
        if (color != "")
            pal.setColor(QPalette::Inactive, (QColorGroup::ColorRole) i,
                         QColor(color));
    }

    widget->setPalette(pal);
}

void MythContext::ThemeWidget(QWidget *widget)
{
    if (d->m_themeloaded)
    {
        widget->setPalette(d->m_palette);
        if (d->m_backgroundimage && d->m_backgroundimage->width() > 0)
        {
            widget->setPaletteBackgroundPixmap(*(d->m_backgroundimage));
        }
        return;
    }

    SetPalette(widget);
    d->m_palette = widget->palette();

    QPixmap *bgpixmap = NULL;

    if (d->m_qtThemeSettings->GetSetting("BackgroundPixmap") != "")
    {
        QString pmapname = d->m_themepathname +
                           d->m_qtThemeSettings->GetSetting("BackgroundPixmap");
 
        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            widget->setBackgroundOrigin(QWidget::AncestorOrigin);
            widget->setPaletteBackgroundPixmap(*bgpixmap);
            d->m_backgroundimage = new QPixmap(*bgpixmap);
        }
    }
    else if (d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap") != "")
    {
        QString pmapname = d->m_themepathname +
                      d->m_qtThemeSettings->GetSetting("TiledBackgroundPixmap");

        int width, height;
        float wmult, hmult;

        GetScreenSettings(width, wmult, height, hmult);

        bgpixmap = LoadScalePixmap(pmapname);
        if (bgpixmap)
        {
            QPixmap background(width, height);
            QPainter tmp(&background);

            tmp.drawTiledPixmap(0, 0, width, height, *bgpixmap);
            tmp.end();

            d->m_backgroundimage = new QPixmap(background);
            widget->setBackgroundOrigin(QWidget::AncestorOrigin);
            widget->setPaletteBackgroundPixmap(background);
        }
    }

    d->m_themeloaded = true;

    if (bgpixmap)
        delete bgpixmap;
}

QImage *MythContext::LoadScaleImage(QString filename, bool fromcache)
{
    if (filename.left(5) == "myth:")
        return NULL;

    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);
    QFileInfo fi(filename);

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath = d->themecachedir + fi.fileName();
        QFile cachecheck(cachefilepath);
        if (cachecheck.exists())
        {
            QImage *ret = new QImage(cachefilepath);
            if (ret)
                return ret;
        }

        if (filename.length() > d->m_themepathname.length())
        {
            QString tmpfilename = filename;
            tmpfilename.remove(0, d->m_themepathname.length());
            cachefilepath = d->themecachedir + tmpfilename;
            cachecheck.setName(cachefilepath);
            if (cachecheck.exists())
            {
                QImage *ret = new QImage(cachefilepath);
                if (ret)
                    return ret;
            }
        }
    }

    if (!checkFile.exists())
    {
        QFileInfo fi(filename);
        filename = d->m_themepathname + fi.fileName();
        checkFile.setName(filename);
        if (!checkFile.exists())
            filename = baseDir + fi.fileName();
    }

    QImage *ret = NULL;

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != 800 || height != 600)
    {
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret = new QImage(tmp2);
    }
    else
    {
        ret = new QImage(filename);
        if (ret->width() == 0)
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}

QPixmap *MythContext::LoadScalePixmap(QString filename, bool fromcache) 
{
    if (filename.left(5) == "myth:")
        return NULL;

    QString baseDir = d->m_installprefix + "/share/mythtv/themes/default/";

    QFile checkFile(filename);
    QFileInfo fi(filename);

    if (d->themecachedir != "" && fromcache)
    {
        QString cachefilepath = d->themecachedir + fi.fileName();
        QFile cachecheck(cachefilepath);
        if (cachecheck.exists())
        {
            QPixmap *ret = new QPixmap(cachefilepath);
            if (ret)
                return ret;
        }

        if (filename.length() > d->m_themepathname.length())
        {
            QString tmpfilename = filename;
            tmpfilename.remove(0, d->m_themepathname.length());
            cachefilepath = d->themecachedir + tmpfilename;
            cachecheck.setName(cachefilepath);
            if (cachecheck.exists())
            {
                QPixmap *ret = new QPixmap(cachefilepath);
                if (ret)
                    return ret;
            }
        }
    }

    if (!checkFile.exists())
    {
        QFileInfo fi(filename);
        filename = d->m_themepathname + fi.fileName();
        checkFile.setName(filename);
        if (!checkFile.exists())
            filename = baseDir + fi.fileName();
    }
        
    QPixmap *ret = new QPixmap();

    int width, height;
    float wmult, hmult;

    GetScreenSettings(width, wmult, height, hmult);

    if (width != 800 || height != 600)
    {           
        QImage tmpimage;

        if (!tmpimage.load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
        QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                           (int)(tmpimage.height() * hmult));
        ret->convertFromImage(tmp2);
    }       
    else
    {
        if (!ret->load(filename))
        {
            cerr << "Error loading image file: " << filename << endl;
            delete ret;
            return NULL;
        }
    }

    return ret;
}

QImage *MythContext::CacheRemotePixmap(const QString &url)
{
    QUrl qurl = url;
    if (qurl.host() == "")
        return NULL;

    if (d->imageCache.contains(url))
    {
        return &(d->imageCache[url]);
    }

    RemoteFile *rf = new RemoteFile(url);

    QByteArray data;
    bool ret = rf->SaveAs(data);

    delete rf;

    if (ret)
    {
        QImage image(data);
        if (image.width() > 0)
        {
            d->imageCache[url] = image;
            return &(d->imageCache[url]);
        }
    }
    
    return NULL;
}

void MythContext::SetSetting(const QString &key, const QString &newValue)
{
    d->m_settings->SetSetting(key, newValue);
}



bool MythContext::SendReceiveStringList(QStringList &strlist, bool quickTimeout)
{
    d->serverSockLock.lock();
    
    if (!d->serverSock)
        ConnectToMasterServer();

    bool ok = false;
    
    if (d->serverSock)
    {
        WriteStringList(d->serverSock, strlist);
        ok = ReadStringList(d->serverSock, strlist, quickTimeout);

        // this should be obsolete...
        while (ok && strlist[0] == "BACKEND_MESSAGE")
        {
            // oops, not for us
            cerr << "SRSL you shouldn't see this!!";
            QString message = strlist[1];
            QString extra = strlist[2];

            MythEvent me(message, extra);
            dispatch(me);

            ok = ReadStringList(d->serverSock, strlist, quickTimeout);
        }
        // .

        if (!ok)
        {
            qApp->lock();
            cout << "Connection to backend server lost\n";
            MythPopupBox::showOkPopup(d->mainWindow, "connection failure",
                             tr("The connection to the master backend "
                                "server has gone away for some reason.. "
                                "Is it running?"));

            delete d->serverSock;
            d->serverSock = NULL;
            qApp->unlock();
        }
    }    

    d->serverSockLock.unlock();
    
    return ok;
}

void MythContext::EventSocketRead(void)
{
    while (d->eventSock->state() == QSocket::Connected &&
           d->eventSock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!ReadStringList(d->eventSock, strlist))
            continue;

        QString prefix = strlist[0];
        QString message = strlist[1];
        
        if (prefix != "BACKEND_MESSAGE")
        {
            cerr << "Received a: " << prefix << " message from the backend\n";
            cerr << "But I don't know what to do with it.\n";
        }
        else
        {
            strlist.pop_front();
            strlist.pop_front();
            MythEvent me(message, strlist);
            dispatch(me);
        }
    }
}

bool MythContext::CheckProtoVersion(QSocketDevice* socket)
{
    QStringList strlist = QString("MYTH_PROTO_VERSION %1")
                                 .arg(MYTH_PROTO_VERSION);
    WriteStringList(socket, strlist);
    ReadStringList(socket, strlist, true);
        
    if (strlist[0] == "REJECT")
    {
        VERBOSE(VB_GENERAL, QString("Protocol version mismatch (frontend=%1,"
                                    "backend=%2)\n")
                                    .arg(MYTH_PROTO_VERSION).arg(strlist[1]));

        if (d->m_height && d->m_width)
        {
            qApp->lock();
            MythPopupBox::showOkPopup(d->mainWindow, 
                                      "Connection failure",
                                      tr(QString("The server uses network "
                                                 "protocol version %1, "
                                                 "but this client only "
                                                 "understands version %2.  "
                                                 "Make sure you are running "
                                                 "compatible versions of "
                                                 "the backend and frontend."))
                                                 .arg(strlist[1])
                                                 .arg(MYTH_PROTO_VERSION));
            qApp->unlock();
        }
        return false;
    }
    else if (strlist[0] == "ACCEPT")
    {
        VERBOSE(VB_ALL, QString("Using protocol version %1")
                               .arg(MYTH_PROTO_VERSION));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Unexpected response to MYTH_PROTO_VERSION: %1")
                               .arg(strlist[0]));
    return false;
}

bool MythContext::CheckProtoVersion(QSocket* socket) 
{
    return CheckProtoVersion(socket->socketDevice());
}

void MythContext::EventSocketConnected(void)
{
    if (!CheckProtoVersion(d->eventSock))
        return;

    QString str = QString("ANN Playback %1 %2")
                         .arg(d->m_localhostname).arg(true);
    QStringList strlist = str;
    WriteStringList(d->eventSock, strlist);
    ReadStringList(d->eventSock, strlist);
}

void MythContext::EventSocketClosed(void)
{
    cerr << "Errm, event socket just closed.\n";
}

void MythContext::addListener(QObject *obj)
{
    if (d->listeners.find(obj) == -1)
        d->listeners.append(obj);
}

void MythContext::removeListener(QObject *obj)
{
    if (d->listeners.find(obj) != -1)
        d->listeners.remove(obj);
}

void MythContext::dispatch(MythEvent &e)
{
    QObject *obj = d->listeners.first();
    while (obj)
    {
        QApplication::postEvent(obj, new MythEvent(e));
        obj = d->listeners.next();
    }
}

void MythContext::dispatchNow(MythEvent &e)
{
    QObject *obj = d->listeners.first();
    while (obj)
    {
        QApplication::sendEvent(obj, &e);
        obj = d->listeners.next();
    }
}

QFont MythContext::GetBigFont(void)
{
    return QFont("Arial", (int)ceil(d->bigfontsize * d->m_hmult), QFont::Bold);
}

QFont MythContext::GetMediumFont(void)
{
    return QFont("Arial", (int)ceil(d->mediumfontsize * d->m_hmult), 
                 QFont::Bold);
}

QFont MythContext::GetSmallFont(void)
{
    return QFont("Arial", (int)ceil(d->smallfontsize * d->m_hmult), 
                 QFont::Bold);
}

QString MythContext::GetLanguage(void)
{
    if (d->language == QString::null || d->language == "")
        d->language = GetSetting("Language", "EN").lower();

    return d->language;
}

void MythContext::SetMainWindow(MythMainWindow *mainwin)
{
    d->mainWindow = mainwin;
}

MythMainWindow *MythContext::GetMainWindow(void)
{
    return d->mainWindow;
}

LCD *MythContext::GetLCDDevice(void)
{
    return d->lcd_device;
}

bool MythContext::TestPopupVersion(const QString &name, 
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    QString err = "The " + name + " plugin was compiled against libmyth " +
                  "version: " + pluginversion + ", but the installed " +
                  "libmyth is at version: " + libversion + ".  You probably " +
                  "want to recompile the " + name + " plugin after doing a " +
                  "make distclean.";

    if (GetMainWindow() && !d->disablelibrarypopup)
    {    
        DialogBox dbox(gContext->GetMainWindow(), err);
        dbox.AddButton("OK");
        dbox.exec();
    }

    return false;
}

void MythContext::SetDisableLibraryPopup(bool check)
{
    d->disablelibrarypopup = check;
}

void MythContext::SetPluginManager(MythPluginManager *pmanager)
{
    d->pluginmanager = pmanager;
}

MythPluginManager *MythContext::getPluginManager(void)
{
    return d->pluginmanager;
}


void MythContext::DisableScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetDisable));
}

void MythContext::RestoreScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetRestore));
}

void MythContext::ResetScreensaver(void)
{
    QApplication::postEvent(GetMainWindow(),
            new ScreenSaverEvent(ScreenSaverEvent::ssetReset));
}

void MythContext::DoDisableScreensaver(void)
{
    d->screensaver->Disable();
    d->screensaverEnabled = false;
}

void MythContext::DoRestoreScreensaver(void)
{
    d->screensaver->Restore();
    d->screensaverEnabled = true;
}

void MythContext::DoResetScreensaver(void)
{
    d->screensaver->Reset();
    d->screensaverEnabled = false;
}

bool MythContext::GetScreensaverEnabled(void)
{
    return d->screensaverEnabled;
}


void MythContext::LogEntry(const QString &module, int priority,
                           const QString &message, const QString &details)
{
    unsigned int logid;
    int howmany;

    if (d->m_logenable == -1) // Haven't grabbed the settings yet
        d->LoadLogSettings();
    if (d->m_logenable == 1)
    {
        if (message.left(21) != "Last message repeated")
        {
            if (message == d->lastLogStrings[module])
            {
                d->lastLogCounts[module] += 1;
                return;
            }
            else
            {
                if (0 < d->lastLogCounts[module])
                {
                    LogEntry(module, priority, 
                             QString("Last message repeated %1 times")
                                    .arg(d->lastLogCounts[module]),
                             d->lastLogStrings[module]);
                }

                d->lastLogCounts[module] = 0;
                d->lastLogStrings[module] = message;
            }
        }

        d->dbLock.lock();
    
        if (d->m_db->isOpen())
        {
            KickDatabase(d->m_db);
    
            QString querystr = QString("INSERT INTO mythlog (module, priority, "
                                       "logdate, host, message, details) "
                                       "values ( '%1', %2, now(), '%3', "
                                       "'%4','%5' );") 
                                       .arg(module) 
                                       .arg(priority)
                                       .arg(d->m_localhostname)
                                       .arg(message).arg(details);
    
            QSqlQuery result = d->m_db->exec(querystr);
            if (!result.isActive())
                MythContext::DBError("LogEntry", querystr);

            if (d->m_logmaxcount > 0)
            {
                querystr = QString("SELECT logid FROM mythlog WHERE "
                                   "module='%1' ORDER BY logdate ASC") 
                                   .arg(module);
                result = d->m_db->exec(querystr);
                howmany = result.size();
                if (howmany > d->m_logmaxcount)
                {
                    while (howmany > d->m_logmaxcount)
                    {
                        result.next();
                        logid = result.value(0).toUInt();
                        querystr = QString("DELETE FROM mythlog WHERE "
                                           "logid=%1").arg(logid);
                        d->m_db->exec(querystr);
                        howmany--;
                    }
                }
            }
        }
    
        d->dbLock.unlock();

        if (priority <= d->m_logprintlevel)
            VERBOSE(VB_ALL, module + ": " + message);
    }
}

void MythContext::addPrivRequest(MythPrivRequest::Type t, void *data)
{
    QMutexLocker lockit(d->m_priv_mutex);
    d->m_priv_requests.push(MythPrivRequest(t, data));
}

bool MythContext::hasPrivRequest() const
{
    QMutexLocker lockit(d->m_priv_mutex);
    return !d->m_priv_requests.empty();
}

MythPrivRequest MythContext::popPrivRequest()
{
    QMutexLocker lockit(d->m_priv_mutex);
    MythPrivRequest ret_val(MythPrivRequest::PrivEnd, NULL);
    if (!d->m_priv_requests.empty()) {
        ret_val = d->m_priv_requests.front();
        d->m_priv_requests.pop();
    }
    return ret_val;
}
