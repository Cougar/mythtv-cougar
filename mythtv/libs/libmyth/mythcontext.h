#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qevent.h>
#include <qmutex.h>
#include <qstringlist.h>
#include <qnetwork.h> 
#include <qmap.h>

#include <cerrno>
#include <iostream>
#include <sstream>
#include <vector>

#include "mythobservable.h"
#include "mythsocket.h"
#include "mythexp.h"

using namespace std;

#if (QT_VERSION < 0x030300)
#error You need Qt version >= 3.3.0 to compile MythTV.
#endif

class Settings;
class QSqlDatabase;
class QSqlQuery;
class QSqlError;
class MythMainWindow;
class MythPluginManager;
class MediaMonitor;
class MythMediaDevice;
class DisplayRes;
class MDBManager;
class MythContextPrivate;

/// This MAP is for the various VERBOSITY flags, used to select which
/// messages we want printed to the console.
///
/// The 5 fields are:
///     enum
///     enum value
///     "-v" arg string
///     additive flag (explicit = 0, additive = 1)
///     help text for "-v help"
///
/// To create a new VB_* flag, this is the only piece of code you need to
/// modify, then you can start using the new flag and it will automatically be
/// processed by the parse_verbose_arg() function and help info printed when
/// "-v help" is used.

#define VERBOSE_MAP(F) \
    F(VB_ALL,       0xffffffff, "all",       0, "ALL available debug output")              \
    F(VB_MOST,      0x7ffeffff, "most",      0, "Most debug (nodatabase,notimestamp)")     \
    F(VB_IMPORTANT, 0x00000001, "important", 0, "Errors or other very important messages") \
    F(VB_GENERAL,   0x00000002, "general",   1, "General info")                            \
    F(VB_RECORD,    0x00000004, "record",    1, "Recording related messages")              \
    F(VB_PLAYBACK,  0x00000008, "playback",  1, "Playback related messages")               \
    F(VB_CHANNEL,   0x00000010, "channel",   1, "Channel related messages")                \
    F(VB_OSD,       0x00000020, "osd",       1, "On-Screen Display related messages")      \
    F(VB_FILE,      0x00000040, "file",      1, "File and AutoExpire related messages")    \
    F(VB_SCHEDULE,  0x00000080, "schedule",  1, "Scheduling related messages")             \
    F(VB_NETWORK,   0x00000100, "network",   1, "Network protocol related messages")       \
    F(VB_COMMFLAG,  0x00000200, "commflag",  1, "Commercial Flagging related messages")    \
    F(VB_AUDIO,     0x00000400, "audio",     1, "Audio related messages")                  \
    F(VB_LIBAV,     0x00000800, "libav",     1, "Enables libav debugging")                 \
    F(VB_JOBQUEUE,  0x00001000, "jobqueue",  1, "JobQueue related messages")               \
    F(VB_SIPARSER,  0x00002000, "siparser",  1, "Siparser related messages")               \
    F(VB_EIT,       0x00004000, "eit",       1, "EIT related messages")                    \
    F(VB_VBI,       0x00008000, "vbi",       1, "VBI related messages")                    \
    F(VB_DATABASE,  0x00010000, "database",  1, "Display all SQL commands executed")       \
    F(VB_DSMCC,     0x00020000, "dsmcc",     1, "DSMCC carousel related messages")         \
    F(VB_MHEG,      0x00040000, "mheg",      1, "MHEG debugging messages")                 \
    F(VB_UPNP,      0x00080000, "upnp",      1, "upnp debugging messages")                 \
    F(VB_SOCKET,    0x00100000, "socket",    1, "socket debugging messages")               \
    F(VB_XMLTV,     0x00200000, "xmltv",     1, "xmltv output and related messages")       \
    F(VB_DVBCAM,    0x00400000, "dvbcam",    1, "DVB CAM debugging messages")              \
    F(VB_MEDIA,     0x00800000, "media",     1, "Media Manager debugging messages")        \
    F(VB_IDLE,      0x01000000, "idle",      1, "System idle messages")                    \
    F(VB_TIMESTAMP, 0x80000000, "timestamp", 1, "Conditional data driven messages")        \
    F(VB_NONE,      0x00000000, "none",      0, "NO debug output")

enum VerboseMask
{
#define VERBOSE_ENUM(ARG_ENUM, ARG_VALUE, ARG_STRING, ARG_ADDITIVE, ARG_HELP)\
    ARG_ENUM = ARG_VALUE ,
    VERBOSE_MAP(VERBOSE_ENUM)
    VB_UNUSED_END // keep at end
};

/// This global variable is set at startup with the flags 
/// of the verbose messages we want to see.
extern MPUBLIC unsigned int print_verbose_messages;
extern MPUBLIC QString verboseString;

MPUBLIC int parse_verbose_arg(QString arg);


/// These are the database logging priorities used for filterig the logs.
enum LogPriorities
{
    LP_EMERG     = 0,
    LP_ALERT     = 1,
    LP_CRITICAL  = 2,
    LP_ERROR     = 3,
    LP_WARNING   = 4,
    LP_NOTICE    = 5,
    LP_INFO      = 6,
    LP_DEBUG     = 7
};

/// Structure containing the basic Database parameters
struct MPUBLIC DatabaseParams
{
    QString dbHostName;         ///< database server
    bool    dbHostPing;         ///< Can we test connectivity using ping?
    int     dbPort;             ///< database port
    QString dbUserName;         ///< DB user name 
    QString dbPassword;         ///< DB password
    QString dbName;             ///< database name
    QString dbType;             ///< database type (MySQL, Postgres, etc.)
    
    bool    localEnabled;       ///< true if localHostName is not default
    QString localHostName;      ///< name used for loading/saving settings
    
    bool    wolEnabled;         ///< true if wake-on-lan params are used
    int     wolReconnect;       ///< seconds to wait for reconnect
    int     wolRetry;           ///< times to retry to reconnect
    QString wolCommand;         ///< command to use for wake-on-lan
};
    
// The verbose_mutex lock is a recursive lock so it is possible (while
// not recommended) to use a VERBOSE macro within another VERBOSE macro.
// But waiting for another thread to do something is not safe within a 
// VERBOSE macro, since those threads may wish to use the VERBOSE macro
// and this will cause a deadlock.
#ifdef DEBUG

#define VERBOSE(mask,args...) \
do { \
    if ((print_verbose_messages & (mask)) == (mask)) \
    { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        MythContext::verbose_mutex.lock(); \
        cout << dtime << " " << args << endl; \
        MythContext::verbose_mutex.unlock(); \
    } \
} while (0)

#else // if !DEBUG

// use a slower non-deadlockable version in release builds
#define VERBOSE(mask,args...) \
do { \
    if ((print_verbose_messages & (mask)) == (mask)) \
    { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        ostringstream verbose_macro_tmp; \
        verbose_macro_tmp << dtime << " " << args; \
        MythContext::verbose_mutex.lock(); \
        cout << verbose_macro_tmp.str() << endl; \
        MythContext::verbose_mutex.unlock(); \
    } \
} while (0)

#endif // DEBUG

/// This can be appended to the VERBOSE args with either
/// "+" (with QStrings) or "<<" (with c strings). It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the 
/// next line in the verbose output.
#define ENO QString("\n\t\t\teno: ") + safe_eno_to_string(errno)

/// Verbose helper function for ENO macro
MPUBLIC QString safe_eno_to_string(int errnum);

MPUBLIC void GetInstallPrefixPath( QString &sInstallPrefix, QString &sInstallLibDir );

/** \class MythPrivRequest
 *  \brief Container for requests that require privledge escalation.
 *
 *   Currently this is used for just one thing, increasing the
 *   priority of the video output thread to a real-time priority.
 *   These requests are made by calling gContext->addPrivRequest().
 *
 *  \sa NuppelVideoPlayer::StartPlaying(void)
 *  \sa MythContext:addPrivRequest(MythPrivRequest::Type, void*)
 */
class MPUBLIC MythPrivRequest
{
  public:
    typedef enum { MythRealtime, MythExit, PrivEnd } Type;
    MythPrivRequest(Type t, void *data) : m_type(t), m_data(data) {}
    Type getType() const { return m_type; }
    void *getData() const { return m_data; }
  private:
    Type m_type;
    void *m_data;
};

/// Update this whenever the plug-in API changes.
/// Including changes in the libmythtv class methods used by plug-ins.
#define MYTH_BINARY_VERSION "0.21.20071104-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value and any corresponding changes to the
 *   ProgramInfo network protocol layout in the following files:
 *
 *   MythWeb
 *       mythplugins/mythweb/includes/mythbackend.php (version number)
 *       mythplugins/mythweb/modules/tv/includes/objects/Program.php (layout)
 *
 *   MythTV Perl Bindings
 *       mythtv/bindings/perl/MythTV.pm (version number)
 *       mythtv/bindings/perl/MythTV/Program.pm (layout)
 *
 *   MythVideo 
 *      mythplugins/mythvideo/mythvideo/scripts/MythTV.py (version number)
 *      mythplugins/mythvideo/mythvideo/scripts/MythTV.py (layout)
 */
#define MYTH_PROTO_VERSION "36"

/** \class MythContext
 *  \brief This class contains the runtime context for MythTV.
 *
 *   This class can be used to query for and set global and host
 *   settings, and is used to communicate between the frontends
 *   and backends. It also contains helper functions for theming
 *   and for getting system defaults, parsing the command line, etc.
 *   It also contains support for database error printing, and
 *   database message logging.
 */
class MPUBLIC MythContext : public QObject, public MythObservable,
    public MythSocketCBs
{
    Q_OBJECT
  public:
    MythContext(const QString &binversion);
    virtual ~MythContext();

    bool Init(bool gui = true, DatabaseParams *pParams = NULL );

    QString GetMasterHostPrefix(void);

    QString GetHostName(void);

    void ClearSettingsCache(QString myKey = "", QString newVal = "");
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &newValue);

    bool ConnectToMasterServer(bool blockingClient = true);
    MythSocket *ConnectServer(MythSocket *eventSocket,
                              const QString &hostname,
                              int port,
                              bool blockingClient = false);
    bool IsConnectedToMaster(void);
    void SetBackend(bool backend);

    bool IsBackend(void);         // is this process a backend process
    bool IsFrontendOnly(void);    // is there there only a frontend running on this host
    bool IsMasterHost(void);      // is this the same host as the master
    bool IsMasterBackend(void);   // is this the actuall mbe process
    bool BackendIsRunning(void);  // a backend process is running on this host

    void BlockShutdown(void);
    void AllowShutdown(void);

    QString GetInstallPrefix(void);
    QString GetShareDir(void);
    QString GetLibraryDir(void);
    static QString GetConfDir(void);

    QString GetFilePrefix(void);

    void LoadQtConfig(void);
    void UpdateImageCache(void);

    void RefreshBackendConfig(void);

    // Note that these give the dimensions for the GUI,
    // which the user may have set to be different from the raw screen size
    void GetScreenSettings(float &wmult, float &hmult);
    void GetScreenSettings(int &width, float &wmult,
                           int &height, float &hmult);
    void GetScreenSettings(int &xbase, int &width, float &wmult,
                           int &ybase, int &height, float &hmult);

    // This returns the raw (drawable) screen size
    void GetScreenBounds(int &xbase, int &ybase, int &width, int &height);

    // Parse an X11 style command line (-geometry) string
    bool ParseGeometryOverride(const QString geometry);

    QString FindThemeDir(const QString &themename);
    QString FindMenuThemeDir(const QString &menuname);
    QString GetThemeDir(void);
    QValueList<QString> GetThemeSearchPath(void);

    QString GetMenuThemeDir(void);

    QString GetThemesParentDir(void);

    QString GetPluginsDir(void);
    QString GetPluginsNameFilter(void);
    QString FindPlugin(const QString &plugname);

    QString GetTranslationsDir(void);
    QString GetTranslationsNameFilter(void);
    QString FindTranslation(const QString &translation);

    QString GetFontsDir(void);
    QString GetFontsNameFilter(void);
    QString FindFont(const QString &fontname);

    QString GetFiltersDir(void);

    MDBManager *GetDBManager(void);
    static void DBError(const QString &where, const QSqlQuery &query);
    static QString DBErrorMessage(const QSqlError& err);

    DatabaseParams GetDatabaseParams(void);
    bool SaveDatabaseParams(const DatabaseParams &params);
    
    void LogEntry(const QString &module, int priority,
                  const QString &message, const QString &details);

    Settings *settings(void);
    Settings *qtconfig(void);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    bool SaveSettingOnHost(const QString &key, const QString &newValue,
                           const QString &host);

    // Convenience setting query methods
    int GetNumSetting(const QString &key, int defaultval = 0);
    double GetFloatSetting(const QString &key, double defaultval = 0.0);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              double& forced_aspect, short &refreshrate,
                              int index=-1);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              int index=-1);

    QString GetSettingOnHost(const QString &key, const QString &host,
                             const QString &defaultval = "");
    int GetNumSettingOnHost(const QString &key, const QString &host,
                            int defaultval = 0);
    double GetFloatSettingOnHost(const QString &key, const QString &host,
                                 double defaultval = 0.0);

    void SetSetting(const QString &key, const QString &newValue);

    QFont GetBigFont();
    QFont GetMediumFont();
    QFont GetSmallFont();

    QString GetLanguage(void);
    QString GetLanguageAndVariant(void);

    void ThemeWidget(QWidget *widget);

    bool FindThemeFile(QString &filename);
    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true); 
    QImage *LoadScaleImage(QString filename, bool fromcache = true);

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false, 
                               bool block = true);

    QImage *CacheRemotePixmap(const QString &url, bool reCache = false);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    int  PromptForSchemaUpgrade(const QString &dbver, const QString &current);
    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);

    void SetDisableLibraryPopup(bool check);

    void SetPluginManager(MythPluginManager *pmanager);
    MythPluginManager *getPluginManager(void);

    bool CheckProtoVersion(MythSocket* socket);

    // event wrappers
    void DisableScreensaver(void);
    void RestoreScreensaver(void);
    // Reset screensaver idle time, for input events that X doesn't see
    // (e.g., lirc)
    void ResetScreensaver(void);

    // actually do it
    void DoDisableScreensaver(void);
    void DoRestoreScreensaver(void);
    void DoResetScreensaver(void);

    // get the current status
    bool GetScreensaverEnabled(void);
    bool GetScreenIsAsleep(void);

    void addPrivRequest(MythPrivRequest::Type t, void *data);
    void waitPrivRequest() const;
    MythPrivRequest popPrivRequest();

    void addCurrentLocation(QString location);
    QString removeCurrentLocation(void);
    QString getCurrentLocation(void);

    void dispatch(MythEvent &event);
    void dispatchNow(MythEvent &event);

    static void SetX11Display(const QString &display);
    static QString GetX11Display(void);

    static QMutex verbose_mutex;
    static QString x11_display;

  private:
    void SetPalette(QWidget *widget);
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void CacheThemeImages(void);
    void CacheThemeImagesDirectory(const QString &dirname, 
                                   const QString &subdirname = "");
    void RemoveCacheDir(const QString &dirname);

    void connected(MythSocket *sock);
    void connectionClosed(MythSocket *sock);
    void readyRead(MythSocket *sock);
    void connectionFailed(MythSocket *sock) { (void)sock; }

    MythContextPrivate *d;
    QString app_binary_version;

    QMutex locationLock;
    QValueList <QString> currentLocation;
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

/// This global variable is used to makes certain calls to avlib threadsafe.
extern MPUBLIC QMutex avcodeclock;

/// Return values for PromptForSchemaUpgrade()
enum MythSchemaUpgrade
{
    MYTH_SCHEMA_EXIT         = 1,
    MYTH_SCHEMA_ERROR        = 2,
    MYTH_SCHEMA_UPGRADE      = 3,
    MYTH_SCHEMA_USE_EXISTING = 4
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
