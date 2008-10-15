// C++ headers
#include <iostream>

#include "mythcontext.h"

using namespace std;

// C headers
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// System specific C headers
#include "compat.h"

#ifdef linux
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#ifdef CONFIG_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/mount.h>  // for struct statfs
#include <sys/sysctl.h>
#include <sys/stat.h>   // for umask()
#endif


// Qt headers
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QFont>
#include <QFile>
#include <QDir>
#include <QFileInfo>

// Myth headers
#include "mythconfig.h"
#include "exitcodes.h"
#include "util.h"
#include "util-x11.h"
#include "mythmediamonitor.h"
#include "libmythdb/mythverbose.h"

/** \fn mythCurrentDateTime()
 *  \brief Returns the current QDateTime object, stripped of its msec component
 */
QDateTime mythCurrentDateTime()
{
    QDateTime rettime = QDateTime::currentDateTime();
    QTime orig = rettime.time();
    rettime.setTime(orig.addMSecs(-orig.msec()));
    return rettime;
}

int calc_utc_offset(void)
{
    QDateTime loc = QDateTime::currentDateTime();
    QDateTime utc = QDateTime::currentDateTime().toUTC();

    int utc_offset = MythSecsTo(utc, loc);

    // clamp to nearest minute if within 10 seconds
    int off = utc_offset % 60;
    if (abs(off) < 10)
        utc_offset -= off;
    if (off < -50 && off > -60)
        utc_offset -= 60 + off;
    if (off > +50 && off < +60)
        utc_offset += 60 - off;

    return utc_offset;
}

#ifndef USING_MINGW
/* Helper function for getSystemTimeZoneID() that compares the
   zoneinfo_file_path (regular) file with files in the zoneinfo_dir_path until
   it finds a match.  The matching file's name is used to determine the time
   zone ID. */
static QString findZoneinfoFile(QString zoneinfo_file_path,
                                QString zoneinfo_dir_path)
{
    QString zone_id("UNDEF");
    QDir zoneinfo_dir(zoneinfo_dir_path);
    QFileInfoList dirlist = zoneinfo_dir.entryInfoList();
    QFileInfo info;
    QString basename;
    QFile zoneinfo_file(zoneinfo_file_path);
    qint64 zoneinfo_file_size = zoneinfo_file.size();

    for (QFileInfoList::const_iterator it = dirlist.begin();
         it != dirlist.end(); it++)
    {
        info = *it;
         // Skip '.' and '..' and other files starting with "." and
         // skip localtime (which is often a link to zoneinfo_file_path)
        basename = info.baseName();
        if (basename.isEmpty() || (basename == "localtime")) {
            continue;
        }
        if (info.isDir())
        {
            zone_id = findZoneinfoFile(zoneinfo_file_path,
                                       info.absoluteFilePath());
            if (zone_id != "UNDEF")
                return zone_id;
        }
        else if (info.isFile() && (info.size() == zoneinfo_file_size) &&
                 info.isReadable())
        {
            // sanity check - zoneinfo files should typically be less than
            // about 4kB, but leave room for growth
            if (zoneinfo_file_size > 200 * 1024)
                continue;
            QFile this_file(info.absoluteFilePath());
            QByteArray zoneinfo_file_data;
            zoneinfo_file_data.resize(zoneinfo_file_size);
            QByteArray this_file_data;
            this_file_data.resize(zoneinfo_file_size);
            if (zoneinfo_file.open(QIODevice::ReadOnly))
            {
                QDataStream in(&zoneinfo_file);
                if (in.readRawData(zoneinfo_file_data.data(),
                                   zoneinfo_file_size) != zoneinfo_file_size)
                {
                    zoneinfo_file.close();
                    return zone_id;
                }
                zoneinfo_file.close();
            }
            if (this_file.open(QIODevice::ReadOnly))
            {
                QDataStream in(&this_file);
                if (in.readRawData(this_file_data.data(),
                                   zoneinfo_file_size) != zoneinfo_file_size)
                {
                    this_file.close();
                    return zone_id;
                }
                this_file.close();
            }
            if (zoneinfo_file_data == this_file_data)
            {
                zone_id = info.absoluteFilePath();
                break;
            }
        }
    }
    return zone_id;
}
#endif

/* helper fuction to read time zone id from a file
   Debian's /etc/timezone or Red Hat's /etc/sysconfig/clock */
static bool read_time_zone_id(QString filename, QString& zone_id)
{
    bool found = false;
    QFile file(filename);
    QFileInfo info(file);
    if (info.exists() && info.isFile() && info.isReadable())
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString line;
            QTextStream in(&file);
            // Handle whitespace and quotes
            QRegExp re("^(?:ZONE\\s*=)?\\s*(['\"]?)([\\w\\s/-\\+]+)\\1\\s*$");
            re.setPatternSyntax(QRegExp::RegExp2);
            while (!in.atEnd())
            {
                line = in.readLine();
                if (re.indexIn(line) != -1)
                {
                    zone_id = re.cap(2);
                    found = true;
                    break;
                }
            }
            file.close();
        }
    }
    return found;
}

/* Helper function for getTimeZoneID() that provides an unprocessed time zone
   id obtained using system-dependent means of identifying the system's time
   zone. */
static QString getSystemTimeZoneID(void)
{
    QString zone_id("UNDEF");
#ifndef USING_MINGW
    // Try to determine the time zone information by inspecting the system
    // configuration
    QString time_zone_file_path("/etc/timezone");
    QString clock_file_path("/etc/sysconfig/clock");
    QString zoneinfo_file_path("/etc/localtime");
    QString zoneinfo_dir_path("/usr/share/zoneinfo");

    // First, check time_zone_file_path (used by Debian-based systems)
    if (read_time_zone_id(time_zone_file_path, zone_id))
        return zone_id;

    // Next, look for the ZONE entry in clock_file_path (used by Red Hat-based
    // systems)
    if (read_time_zone_id(clock_file_path, zone_id))
        return zone_id;

    // Next check zoneinfo_file_path
    QFile zoneinfo_file(zoneinfo_file_path);
    QFileInfo info(zoneinfo_file);

    if (info.exists() && info.isFile())
    {
        QString tz;
        if (info.isSymLink())
        {
            // The symlink refers to a file whose name contains the zone ID
            tz = info.symLinkTarget();
        }
        else
        {
            // The zoneinfo_file is a copy of the file in the
            // zoneinfo_dir_path, so search for the same file in
            // zoneinfo_dir_path
            tz = findZoneinfoFile(zoneinfo_file_path, zoneinfo_dir_path);
        }
        if (tz != "UNDEF")
        {
            int pos = 0;
            // Get the zone ID from the filename
            // Look for the basename of zoneinfo_dir_path in case it's a
            // relative link
            QString zoneinfo_dirname = zoneinfo_dir_path.section('/', -1);
            if ((pos = tz.indexOf(zoneinfo_dirname)) != -1)
            {
                zone_id = tz.right(tz.size() - (pos + 1) -
                                   zoneinfo_dirname.size());
            }
        }
        else
        {
            // If we still haven't found a time zone, try localtime_r() to at
            // least get the zone name/abbreviation (as opposed to the
            // identifier for the set of rules governing the zone)
            char name[64];
            time_t t;
            struct tm *result = (struct tm *)malloc(sizeof(*result));

            t = time(NULL);
            localtime_r(&t, result);

            if (result != NULL)
            {
                if (strftime(name, sizeof(name), "%Z", result) > 0)
                    zone_id = name;
                free(result);
            }
        }
    }

#endif
    return zone_id;
}

/** \fn getTimeZoneID()
 *  \brief Returns the zoneinfo time zone ID or as much time zone information
 *         as possible
 */
QString getTimeZoneID(void)
{
    QString zone_id("UNDEF");
#ifndef USING_MINGW
    // First, try the TZ environment variable to check for environment-specific
    // overrides
    QString tz = getenv("TZ");
    if (tz.isEmpty())
    {
        // No TZ, so attempt to determine the system-configured time zone ID
        tz = getSystemTimeZoneID();
    }

    if (!tz.isEmpty())
    {
        zone_id = tz;
        if (zone_id.startsWith("\"") || zone_id.startsWith("'"))
                zone_id.remove(0, 1);
        if (zone_id.endsWith("\"") || zone_id.endsWith("'"))
                zone_id.chop(1);
        if (zone_id.startsWith(":"))
            zone_id.remove(0, 1);
        // the "posix/" subdirectory typically contains the same files as the
        // "zoneinfo/" parent directory, but are not typically what are in use
        if (zone_id.startsWith("posix/"))
            zone_id.remove(0, 6);
    }

#endif
    return zone_id;
}

static void print_timezone_info(QString master_zone_id, QString local_zone_id,
                                int master_utc_offset, int local_utc_offset,
                                QString master_time, QString local_time)
{
    VERBOSE(VB_IMPORTANT, QString("Detected time zone settings:\n"
"    Master: Zone ID: '%1', UTC Offset: '%2', Current Time: '%3'\n"
"     Local: Zone ID: '%4', UTC Offset: '%5', Current Time: '%6'\n")
            .arg(master_zone_id).arg(master_utc_offset).arg(master_time)
            .arg(local_zone_id).arg(local_utc_offset).arg(local_time));
}

/** \fn checkTimeZone()
 *  \brief Verifies the time zone settings on this system agree with those
 *         on the master backend
 */
bool checkTimeZone(void)
{
    if (gContext->IsMasterBackend())
        return true;

    QStringList master_settings(QString("QUERY_TIME_ZONE"));
    if (!gContext->SendReceiveStringList(master_settings))
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine master backend time zone "
                              "settings.  If those settings differ from local "
                              "settings, some functionality will fail.");
        return true;
    }

    QDateTime local_time = mythCurrentDateTime();
    QString local_time_string = local_time.toString(Qt::ISODate);

    bool have_zone_IDs = true;

    QString master_time_zone_ID = master_settings[0];
    int master_utc_offset       = master_settings[1].toInt();
    QString master_time_string  = master_settings[2];
    QString local_time_zone_ID  = getTimeZoneID();
    int local_utc_offset        = calc_utc_offset();

    if (master_time_zone_ID == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine master backend time zone "
                              "settings. If local time zone settings differ "
                              "from master backend settings, some "
                              "functionality will fail.");
        have_zone_IDs = false;
    }
    if (local_time_zone_ID == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine local time zone settings. "
                              "If local time zone settings differ from "
                              "master backend settings, some functionality "
                              "will fail.");
        have_zone_IDs = false;
    }

    // Some distros use spaces rather than underscores in the zone ID, so
    // allow matches where the only difference is space vs. underscore.
    // Rather than modify the original zone ID's, modify a copy for the
    // comparison so the error message will show a difference in zone ID
    // as well as offset/current time in case the definitions differ.
    QString master_zone_compare = master_time_zone_ID;
    QString local_zone_compare = local_time_zone_ID;
    master_zone_compare.replace(' ', '_');
    local_zone_compare.replace(' ', '_');
    if (have_zone_IDs && (master_zone_compare != local_zone_compare))
    {
        VERBOSE(VB_IMPORTANT, "Time zone settings on the master backend "
                              "differ from those on this system.");
        print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                            master_utc_offset, local_utc_offset,
                            master_time_string, local_time_string);
        return false;
    }

    // Verify offset
    if (master_utc_offset != local_utc_offset)
    {
        VERBOSE(VB_IMPORTANT, "UTC offset on the master backend differs "
                              "from offset on this system.");
        print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                            master_utc_offset, local_utc_offset,
                            master_time_string, local_time_string);
        return false;
    }

    // Verify current time
    if (master_time_string == "UNDEF")
    {
        VERBOSE(VB_IMPORTANT, "Unable to determine current time on the master "
                              "backend . If local time or time zone settings "
                              "differ from those on the master backend, some "
                              "functionality will fail.");
    }
    else
    {
        QDateTime master_time = QDateTime::fromString(master_time_string,
                                                      Qt::ISODate);
        uint timediff = abs(master_time.secsTo(local_time));
        if (timediff > 300)
        {
            VERBOSE(VB_IMPORTANT, "Current time on the master backend "
                                  "differs from time on this system.");
            print_timezone_info(master_time_zone_ID, local_time_zone_ID,
                                master_utc_offset, local_utc_offset,
                                master_time_string, local_time_string);
            return false;
        }
        else if (timediff > 20)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Warning! Time difference between the master "
                            "backend and this system is %1 seconds.")
                    .arg(timediff));
        }
    }

    return true;
}

/** \fn encodeLongLong(QStringList&,long long)
 *  \brief Encodes a long for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *   We encode the long long as strings representing two signed
 *   32 bit integers.
 *
 *  \sa decodeLongLong(QStringList&,uint)
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

/** \fn decodeLongLong(QStringList&,uint)
 *  \brief Inefficiently decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *   Note: This decode performs two O(n) linear searches of the list,
 *         The iterator decode function is much more efficient.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param offset Offset in list where to find first 32 bits of
 *                long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
long long decodeLongLong(QStringList &list, uint offset)
{
    long long retval = 0;
    if (offset >= (uint)list.size())
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with offset >= list size.");
        return retval;
    }

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

/** \fn decodeLongLong(QStringList&,QStringList::const_iterator&)
 *  \brief Decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param it     Iterator pointing to first 32 bits of long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,uint)
 */
long long decodeLongLong(QStringList &list, QStringList::const_iterator &it)
{
    (void)list;

    long long retval = 0;

    bool ok = true;
    int l1=0, l2=0;

    if (it == list.end())
        ok = false;
    else
        l1 = (*(it++)).toInt();

    if (it == list.end())
        ok = false;
    else
        l2 = (*(it++)).toInt();

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with the iterator too close "
                "to the end of the list.");
        return 0;
    }

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

/** \fn blendColors(QRgb source, QRgb add, int alpha)
 *  \brief Inefficient alpha blending function.
 */
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

/** \fn cutDownString(const QString&, QFont*, uint)
 *  \brief Returns a string based on "text" that fits within "maxwidth" pixels.
 */
QString cutDownString(const QString &text, QFont *testFont, uint maxwidth)
{
    QFontMetrics fm(*testFont);

    uint curFontWidth = fm.width(text);
    if (curFontWidth > maxwidth)
    {
        QString testInfo;
        curFontWidth = fm.width(testInfo);
        int tmaxwidth = maxwidth - fm.width("LLL");
        int count = 0;

        while ((int)curFontWidth < tmaxwidth)
        {
            testInfo = text.left(count);
            curFontWidth = fm.width(testInfo);
            count = count + 1;
        }

        testInfo = testInfo + "...";
        return testInfo;
    }

    return text;
}

/** \fn MythSecsTo(const QDateTime&, const QDateTime&)
 *  \brief Returns "'to' - 'from'" for two QDateTime's in seconds.
 */
int MythSecsTo(const QDateTime &from, const QDateTime &to)
{
   return (from.time().secsTo(to.time()) +
           from.date().daysTo(to.date()) * 60 * 60 * 24);
}

/** \fn MythUTCToLocal(const QDateTime&)
 *  \brief Converts a QDateTime in UTC to local time.
 */
QDateTime MythUTCToLocal(const QDateTime &utc)
{
    QDateTime local = QDateTime(QDate(1970, 1, 1));

    int timesecs = MythSecsTo(local, utc);
    QDateTime localdt;
    localdt.setTime_t(timesecs);

    return localdt;
}

/** \fn stringToLongLong(const QString &str)
 *  \brief Converts QString representing long long to a long long.
 *
 *   This is needed to input 64 bit numbers with Qt 3.1.
 */
long long stringToLongLong(const QString &str)
{
    long long retval = 0;
    if (str != QString::null)
    {
        QByteArray tmp = str.toAscii();
        retval = strtoll(tmp.constData(), NULL, 0);
    }
    return retval;
}

/** \fn longLongToString(long long)
 *  \brief Returns QString representation of long long.
 *
 *   This is needed to output 64 bit numbers with Qt 3.1.
 */
QString longLongToString(long long ll)
{
    char str[21];
    snprintf(str, 20, "%lld", ll);
    str[20] = '\0';
    return str;
}

/** \fn getUptime(time_t&)
 *  \brief Returns uptime statistics.
 *  \todo Update Statistics are not supported (by MythTV) on NT or DOS.
 *  \return true if successful, false otherwise.
 */
bool getUptime(time_t &uptime)
{
#ifdef __linux__
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == -1)
    {
        VERBOSE(VB_IMPORTANT, "sysinfo() error");
        return false;
    }
    else
        uptime = sinfo.uptime;

#elif defined(__FreeBSD__) || defined(CONFIG_DARWIN)

    int            mib[2];
    struct timeval bootTime;
    size_t         len;

    // Uptime is calculated. Get this machine's boot time
    // and subtract it from the current machine time
    len    = sizeof(bootTime);
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
    if (sysctl(mib, 2, &bootTime, &len, NULL, 0) == -1)
    {
        VERBOSE(VB_IMPORTANT, "sysctl() error");
        return false;
    }
    else
        uptime = time(NULL) - bootTime.tv_sec;
#elif defined(USING_MINGW)
    uptime = ::GetTickCount() / 1000;
#else
    // Hmmm. Not Linux, not FreeBSD or Darwin. What else is there :-)
    VERBOSE(VB_IMPORTANT, "Unknown platform. How do I get the uptime?");
    return false;
#endif

    return true;
}

/** \fn getDiskSpace(const QString&,long long&,long long&)
 *  \brief Returns free space on disk containing file in KiB,
 *          or -1 if it does not succeed.
 *  \param file_on_disk file on the file system we wish to stat.
 */
long long getDiskSpace(const QString &file_on_disk,
                       long long &total, long long &used)
{
    struct statfs statbuf;
    bzero(&statbuf, sizeof(statbuf));
    long long freespace = -1;
    QByteArray cstr = file_on_disk.toLocal8Bit();

    total = used = -1;

    // there are cases where statfs will return 0 (good), but f_blocks and
    // others are invalid and set to 0 (such as when an automounted directory
    // is not mounted but still visible because --ghost was used),
    // so check to make sure we can have a total size > 0
    if ((statfs(cstr.constData(), &statbuf) == 0) &&
        (statbuf.f_blocks > 0) &&
        (statbuf.f_bsize > 0))
    {
        total      = statbuf.f_blocks;
        total     *= statbuf.f_bsize;
        total      = total >> 10;

        freespace  = statbuf.f_bavail;
        freespace *= statbuf.f_bsize;
        freespace  = freespace >> 10;

        used       = total - freespace;
    }

    return freespace;
}

/** \fn getMemStats(int&,int&,int&,int&)
 *  \brief Returns memory statistics in megabytes.
 *
 *  \todo Memory Statistics are not supported (by MythTV) on NT or DOS.
 *  \return true if it succeeds, false otherwise.
 */
bool getMemStats(int &totalMB, int &freeMB, int &totalVM, int &freeVM)
{
#ifdef __linux__
    size_t MB = (1024*1024);
    struct sysinfo sinfo;
    if (sysinfo(&sinfo) == -1)
    {
        VERBOSE(VB_IMPORTANT, "getMemStats(): Error, sysinfo() call failed.");
        return false;
    }
    else
        totalMB = (int)((sinfo.totalram  * sinfo.mem_unit)/MB),
        freeMB  = (int)((sinfo.freeram   * sinfo.mem_unit)/MB),
        totalVM = (int)((sinfo.totalswap * sinfo.mem_unit)/MB),
        freeVM  = (int)((sinfo.freeswap  * sinfo.mem_unit)/MB);

#elif defined(CONFIG_DARWIN)
    mach_port_t             mp;
    mach_msg_type_number_t  count, pageSize;
    vm_statistics_data_t    s;

    mp = mach_host_self();

    // VM page size
    if (host_page_size(mp, &pageSize) != KERN_SUCCESS)
        pageSize = 4096;   // If we can't look it up, 4K is a good guess

    count = HOST_VM_INFO_COUNT;
    if (host_statistics(mp, HOST_VM_INFO,
                        (host_info_t)&s, &count) != KERN_SUCCESS)
    {
        VERBOSE(VB_IMPORTANT, "getMemStats(): Error, "
                "failed to get virtual memory statistics.");
        return false;
    }

    pageSize >>= 10;  // This gives usages in KB
    totalMB = (s.active_count + s.inactive_count
               + s.wire_count + s.free_count) * pageSize / 1024;
    freeMB  = s.free_count * pageSize / 1024;


    // This is a real hack. I have not found a way to ask the kernel how much
    // swap it is using, and the dynamic_pager daemon doesn't even seem to be
    // able to report what filesystem it is using for the swapfiles. So, we do:
    long long total, used, free;
    free = getDiskSpace("/private/var/vm", total, used);
    totalVM = (int)(total/1024LL), freeVM = (int)(free/1024LL);

#else
    VERBOSE(VB_IMPORTANT, "getMemStats(): Unknown platform. "
            "How do I get the memory stats?");
    return false;
#endif

    return true;
}

/**
 * \brief Eject a disk, unmount a drive, open a tray
 *
 * If the Media Monitor is enabled, we use its fully-featured routine.
 * Otherwise, we guess a drive and use a primitive OS-specific command
 */
void myth_eject()
{
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
        mon->ChooseAndEjectMedia();
    else
    {
        VERBOSE(VB_MEDIA, "CD/DVD Monitor isn't enabled.");
#ifdef __linux__
        VERBOSE(VB_MEDIA, "Trying Linux 'eject -T' command");
        myth_system("eject -T");
#elif defined(CONFIG_DARWIN)
        VERBOSE(VB_MEDIA, "Trying 'disktool -e disk1");
        myth_system("disktool -e disk1");
#endif
    }
}

/**
 * \brief Quess whether a string is UTF-8
 *
 * \note  This does not attempt to \e validate the whole string.
 *        It just checks if it has any UTF-8 sequences in it.
 */

bool hasUtf8(const char *str)
{
    const uchar *c = (uchar *) str;

    while (*c++)
    {
        // ASCII is < 0x80.
        // 0xC2..0xF4 is probably UTF-8.
        // Anything else probably ISO-8859-1 (Latin-1, Unicode)

        if (*c > 0xC1 && *c < 0xF5)
        {
            int bytesToCheck = 2;  // Assume  0xC2-0xDF (2 byte sequence)

            if (*c > 0xDF)         // Maybe   0xE0-0xEF (3 byte sequence)
                ++bytesToCheck;
            if (*c > 0xEF)         // Matches 0xF0-0xF4 (4 byte sequence)
                ++bytesToCheck;

            while (bytesToCheck--)
            {
                ++c;

                if (! *c)                    // String ended in middle
                    return false;            // Not valid UTF-8

                if (*c < 0x80 || *c > 0xBF)  // Bad UTF-8 sequence
                    break;                   // Keep checking in outer loop
            }

            if (!bytesToCheck)  // Have checked all the bytes in the sequence
                return true;    // Hooray! We found valid UTF-8!
        }
    }

    return false;
}

#ifdef USING_MINGW
u_short in_cksum(u_short *addr, int len)
{
    register int nleft = len;
    register u_short *w = addr;
    register u_short answer;
    register int sum = 0;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
    {
        u_short u = 0;

        *(u_char *)(&u) = *(u_char *)w ;
        sum += u;
    }

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);  /* add hi 16 to low 16 */
    sum += (sum >> 16);                  /* add carry */
    answer = ~sum;                       /* truncate to 16 bits */
    return (answer);
}
#endif

/**
 * \brief Can we ping host within timeout seconds?
 */
bool ping(const QString &host, int timeout)
{
#ifdef USING_MINGW
    VERBOSE(VB_SOCKET, QString("Ping: pinging %1 (%2 seconds max)")
                       .arg(host).arg(timeout));
    SOCKET    rawSocket;
    LPHOSTENT lpHost;
    struct    sockaddr_in saDest;

    #define ICMP_ECHOREPLY 0
    #define ICMP_ECHOREQ   8
    struct IPHDR {
        u_char         VIHL;     // Version and IHL
        u_char         TOS;      // Type Of Service
        short          TotLen;   // Total Length
        short          ID;       // Identification
        short          FlagOff;  // Flags and Fragment Offset
        u_char         TTL;      // Time To Live
        u_char         Protocol; // Protocol
        u_short        Checksum; // Checksum
        struct in_addr iaSrc;    // Internet Address - Source
        struct in_addr iaDst;    // Internet Address - Destination
    };
    struct ICMPHDR {
        u_char  Type;            // Type
        u_char  Code;            // Code
        u_short Checksum;        // Checksum
        u_short ID;              // Identification
        u_short Seq;             // Sequence
        char    Data;            // Data
    };

    struct Request {
        ICMPHDR icmpHdr;
        DWORD   dwTime;
        char    cData[32];
    };
    struct Reply {
        IPHDR   ipHdr;
        Request echoRequest;
        char    cFiller[256];
    };

    if (INVALID_SOCKET == (rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)))
    {
        VERBOSE(VB_SOCKET, "Ping: can't create socket");
        return false;
    }

    lpHost = gethostbyname(host.toLocal8Bit().constData());
    if (!lpHost)
    {
        VERBOSE(VB_SOCKET, "Ping: gethostbyname failed");
        closesocket(rawSocket);
        return false;
    }

    saDest.sin_addr.s_addr = *((u_long FAR *) (lpHost->h_addr));
    saDest.sin_family      = AF_INET;
    saDest.sin_port        = 0;

    Request echoReq;
    echoReq.icmpHdr.Type = ICMP_ECHOREQ;
    echoReq.icmpHdr.Code = 0;
    echoReq.icmpHdr.ID   = 123;
    echoReq.icmpHdr.Seq  = 456;
    for (unsigned i = 0; i < sizeof(echoReq.cData); i++)
        echoReq.cData[i] = ' ' + i;
    echoReq.dwTime = GetTickCount();
    echoReq.icmpHdr.Checksum = in_cksum((u_short *)&echoReq, sizeof(Request));

    if (SOCKET_ERROR == sendto(rawSocket, (LPSTR)&echoReq, sizeof(Request),
                               0, (LPSOCKADDR)&saDest, sizeof(SOCKADDR_IN)))
    {
        VERBOSE(VB_SOCKET, "Ping: send failed");
        closesocket(rawSocket);
        return false;
    }

    struct timeval Timeout;
    fd_set readfds;
    readfds.fd_count = 1;
    readfds.fd_array[0] = rawSocket;
    Timeout.tv_sec  = timeout;
    Timeout.tv_usec = 0;

    if (SOCKET_ERROR == select(1, &readfds, NULL, NULL, &Timeout))
    {
        VERBOSE(VB_SOCKET, "Ping: timeout expired or select failed");
        closesocket(rawSocket);
        return false;
    }

    closesocket(rawSocket);
    VERBOSE(VB_SOCKET, "Ping: done");
    return true;
#else
    QString cmd = QString("ping -t %1 -c 1  %2  >/dev/null 2>&1")
                  .arg(timeout).arg(host);

    if (myth_system(cmd))
    {
        // ping command may not like -t argument. Simplify:

        cmd = QString("ping -c 1  %2  >/dev/null 2>&1").arg(host);

        if (myth_system(cmd))
            return false;
    }

    return true;
#endif
}

/**
 * \brief Can we talk to port on host?
 */
bool telnet(const QString &host, int port)
{
    MythSocket *s = new MythSocket();

    if (s->connect(host, port))
    {
        s->close();
        return true;
    }

    return false;
}

/** \fn copy(QFile&,QFile&,uint)
 *  \brief Copies src file to dst file.
 *
 *   If the dst file is open, it must be open for writing.
 *   If the src file is open, if must be open for reading.
 *
 *   The files will be in the same open or close state after
 *   this function runs as they were prior to this function being called.
 *
 *   This function does not care if the files are actual files.
 *   For compatibility with pipes and socket streams the file location
 *   will not be reset to 0 at the end of this function. If the function
 *   is succesful the file pointers will be at the end of the copied
 *   data.
 *
 *  \param dst Destination QFile
 *  \param src Source QFile
 *  \param block_size Optional block size in bytes, must be at least 1024,
 *                    otherwise the default of 16 KB will be used.
 *  \return bytes copied on success, -1 on failure.
 */
long long copy(QFile &dst, QFile &src, uint block_size)
{
    uint buflen = (block_size < 1024) ? (16 * 1024) : block_size;
    char *buf = new char[buflen];
    bool odst = false, osrc = false;

    if (!buf)
        return -1LL;

    if (!dst.isWritable() && !dst.isOpen())
        odst = dst.open(QIODevice::Unbuffered|QIODevice::WriteOnly|QIODevice::Truncate);

    if (!src.isReadable() && !src.isOpen())
        osrc = src.open(QIODevice::Unbuffered|QIODevice::ReadOnly);

    bool ok = dst.isWritable() && src.isReadable();
    long long total_bytes = 0LL;
    while (ok)
    {
        long long rlen, wlen, off = 0;
        rlen = src.read(buf, buflen);
        if (rlen<0)
        {
            VERBOSE(VB_IMPORTANT, "util.cpp:copy: read error");
            ok = false;
            break;
        }
        if (rlen==0)
            break;

        total_bytes += (long long) rlen;

        while ((rlen-off>0) && ok)
        {
            wlen = dst.write(buf + off, rlen - off);
            if (wlen>=0)
                off+= wlen;
            if (wlen<0)
            {
                VERBOSE(VB_IMPORTANT, "util.cpp:copy: write error");
                ok = false;
            }
        }
    }
    delete[] buf;

    if (odst)
        dst.close();

    if (osrc)
        src.close();

    return (ok) ? total_bytes : -1LL;
}

QString createTempFile(QString name_template, bool dir)
{
    int ret = -1;

#ifdef USING_MINGW
    char temppath[MAX_PATH] = ".";
    char tempfilename[MAX_PATH] = "";
    // if GetTempPath fails, use current dir
    GetTempPathA(MAX_PATH, temppath);
    if (GetTempFileNameA(temppath, "mth", 0, tempfilename))
    {
        if (dir)
            ret = mkdir(tempfilename);
        else
            ret = open(tempfilename, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
    }
    QString tmpFileName(tempfilename);
#else
    QByteArray safe_name_template = name_template.toAscii();
    const char *tmp = safe_name_template.constData();
    char *ctemplate = strdup(tmp);

    if (dir)
    {
        ret = (mkdtemp(ctemplate)) ? 0 : -1;
    }
    else
    {
        mode_t cur_umask = umask(S_IRWXO | S_IRWXG);
        ret = mkstemp(ctemplate);
        umask(cur_umask);
    }

    QString tmpFileName(ctemplate);
    free(ctemplate);
#endif

    if (ret == -1)
    {
        VERBOSE(VB_IMPORTANT, QString("createTempFile(%1), Error ")
                .arg(name_template) + ENO);
        return name_template;
    }

    if (!dir && (ret >= 0))
        close(ret);

    return tmpFileName;
}

double MythGetPixelAspectRatio(void)
{
    float pixelAspect = 1.0;
#ifdef USING_X11
    pixelAspect = MythXGetPixelAspectRatio();
#endif // USING_X11
    return pixelAspect;
}

/**
 * In an interactive shell, prompt the user to input a string
 */
QString getResponse(const QString &query, const QString &def)
{
    QByteArray tmp = query.toLocal8Bit();
    cout << tmp.constData();

    tmp = def.toLocal8Bit();
    if (def.size())
        cout << " [" << tmp.constData() << "]  ";
    else
        cout << "  ";

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
    {
        cout << endl << "[console is not interactive, using default '"
             << tmp.constData() << "']" << endl;
        return def;
    }

    char response[80];
    cin.clear();
    cin.getline(response, 80);
    if (!cin.good())
    {
        cout << endl;
        VERBOSE(VB_IMPORTANT, "Read from stdin failed");
        return NULL;
    }

    QString qresponse = response;

    if (qresponse.isEmpty())
        qresponse = def;

    return qresponse;
}

/**
 * In an interactive shell, prompt the user to input a number
 */
int intResponse(const QString &query, int def)
{
    QString str_resp = getResponse(query, QString("%1").arg(def));
    if (str_resp.isEmpty())
        return def;
    bool ok;
    int resp = str_resp.toInt(&ok);
    return (ok ? resp : def);
}
