#ifndef UTIL_H_
#define UTIL_H_

#include <algorithm>
using namespace std;

#include <qstringlist.h>
#include <qdatetime.h>
#include <qcolor.h>
#include <QPixmap>

#include <stdint.h>
#include <time.h>

#include "mythexp.h"
#include "mythtimer.h"
#include "mythsystem.h"

class QPixmap;
class QImage;
class QPainter;
class QFont;
class QFile;

MPUBLIC QDateTime mythCurrentDateTime();
MPUBLIC int calc_utc_offset(void);
MPUBLIC QString getTimeZoneID(void);
MPUBLIC bool checkTimeZone(void);
MPUBLIC bool checkTimeZone(const QStringList &master_settings);

MPUBLIC QRgb blendColors(QRgb source, QRgb add, int alpha);

MPUBLIC QString cutDownString(const QString &text, QFont *testFont, uint maxwidth);

MPUBLIC QDateTime MythUTCToLocal(const QDateTime &utc);
MPUBLIC int MythSecsTo(const QDateTime &from, const QDateTime &to);

MPUBLIC long long stringToLongLong(const QString &str);
MPUBLIC QString longLongToString(long long ll);

MPUBLIC long long getDiskSpace(const QString&,long long&,long long&);
MPUBLIC bool getUptime(time_t &uptime);
MPUBLIC bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM);

MPUBLIC void myth_eject(void);

MPUBLIC bool hasUtf8(const char *str);
#define M_QSTRING_UNICODE(str) hasUtf8(str) ? QString::fromUtf8(str) : str

MPUBLIC bool ping(const QString &host, int timeout);
MPUBLIC bool telnet(const QString &host, int port);

MPUBLIC long long copy(QFile &dst, QFile &src, uint block_size = 0);
MPUBLIC QString createTempFile(QString name_template = "/tmp/mythtv_XXXXXX",
                               bool dir = false);

MPUBLIC double MythGetPixelAspectRatio(void);

MPUBLIC QString getResponse(const QString &query, const QString &def);
MPUBLIC int     intResponse(const QString &query, int def);

MPUBLIC QString getSymlinkTarget(const QString &start_file,
                                 QStringList   *intermediaries = NULL,
                                 unsigned       maxLinks       = 255);

inline float clamp(float val, float minimum, float maximum)
{
    return min(max(val, minimum), maximum);
}
inline int   clamp(int val, int minimum, int maximum)
{
    return min(max(val, minimum), maximum);
}
inline float lerp(float r, float a, float b)
{
    return ((1.0f - r) * a) + (r * b);
}
inline int   lerp(float r, int a, int b)
{
    return (int) lerp(r, (float) a, (float) b);
}
inline float sq(float a) { return a*a; }
inline int   sq(int   a) { return a*a; }

MPUBLIC bool IsMACAddress(QString MAC);
MPUBLIC bool WakeOnLAN(QString MAC);

// CPU Tick timing function
#ifdef MMX
#ifdef _WIN32
#include "compat.h"
inline void rdtsc(uint64_t &x)
{
    QueryPerformanceCounter((LARGE_INTEGER*)(&x));
}
#else
typedef struct {
    uint a;
    uint b;
} timing_ab_t;
inline void rdtsc(uint64_t &x)
{
    timing_ab_t &y = (timing_ab_t&) x;
    asm("rdtsc \n"
        "mov %%eax, %0 \n"
        "mov %%edx, %1 \n"
        : 
        : "m"(y.a), "m"(y.b)
        : "%eax", "%edx");
}
#endif

#else // if !MMX
inline void rdtsc(uint64_t &x) { x = 0ULL; }
#endif // !MMX

#endif // UTIL_H_
