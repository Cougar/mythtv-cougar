#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qregexp.h>
#include <qstring.h>
#include <qdatetime.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include <sys/stat.h>
#ifdef linux
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "autoexpire.h"

#include "programinfo.h"
#include "libmyth/mythcontext.h"

AutoExpire::AutoExpire(bool runthread, bool master, QSqlDatabase *ldb)
{
    db = ldb;
    isMaster = master;

    threadrunning = runthread;

    if (runthread)
    {
        pthread_t expthread;
        pthread_create(&expthread, NULL, ExpirerThread, this);
        gContext->addListener(this);
    }
}

AutoExpire::~AutoExpire()
{
    if (threadrunning)
        gContext->removeListener(this);
}

void AutoExpire::RunExpirer(void)
{
    QString recordfileprefix = gContext->GetSetting("RecordFilePrefix");

    // wait a little for main server to come up and things to settle down
    sleep(10);

    while (1)
    {
        if (isMaster)
            ExpireEpisodesOverMax();

        struct statfs statbuf;
        int freespace = -1;
        if (statfs(recordfileprefix.ascii(), &statbuf) == 0) {
            freespace = statbuf.f_bavail / (1024*1024*1024/statbuf.f_bsize);
        }

        int minFree = gContext->GetNumSetting("AutoExpireDiskThreshold", 0);

        if ((minFree) && (freespace != -1) && (freespace < minFree))
        {
            QString msg = QString("Running AutoExpire: Want %1 Gigs free but "
                                  "only have %2.")
                                  .arg(minFree).arg(freespace);
            VERBOSE(VB_GENERAL, msg);

            FillExpireList();

            if ((freespace < minFree) &&
                (expireList.size() > 0))
            {
                // delete the "first" item on our list (really off the end)
                ProgramInfo *pginfo = expireList.back();

                QString msg = QString("AutoExpiring: %1 %2 %3 MBytes")
                                      .arg(pginfo->title)
                                      .arg(pginfo->startts.toString())
                                      .arg((int)(pginfo->filesize/1024/1024));
                VERBOSE(VB_GENERAL, msg.local8Bit());
                gContext->LogEntry("autoexpire", LP_NOTICE, "Expired Program", msg);

                QString message =
                           QString("AUTO_EXPIRE %1 %2")
                                   .arg(pginfo->chanid)
                                   .arg(pginfo->startts.toString(Qt::ISODate));
                MythEvent me(message);
                gContext->dispatch(me);

                delete pginfo;
                expireList.erase(expireList.end() - 1);

                if (statfs(recordfileprefix.ascii(), &statbuf) == 0)
                {
                    freespace =
                        statbuf.f_bavail / (1024*1024*1024/statbuf.f_bsize);
                }
            }

            if (freespace < minFree)
            {
                msg = QString("WARNING: Not enough space freed, only %1 "
                              "Gigs free but need %2.")
                              .arg(freespace).arg(minFree);
            }
            else
            {
                msg = QString("AutoExpire successful, %1 Gigs now free.")
                              .arg(freespace);
            }

            VERBOSE(VB_GENERAL, msg);
        }
        else if ((minFree) && (freespace == -1))
        {
            QString msg = QString("WARNING: AutoExpire Failed.   Want %1 Gigs "
                                  "free but unable to calculate actual free.")
                                  .arg(minFree);
            VERBOSE(VB_GENERAL, msg);
        }

        sleep(gContext->GetNumSetting("AutoExpireFrequency", 10) * 60);
    }
} 

void *AutoExpire::ExpirerThread(void *param)
{
    AutoExpire *expirer = (AutoExpire *)param;
    expirer->RunExpirer();
 
    return NULL;
}

void AutoExpire::ExpireEpisodesOverMax(void)
{
    QMap<QString, int> maxEpisodes;
    QMap<QString, int>::Iterator maxIter;

    QString fileprefix = gContext->GetFilePrefix();
    QString querystr = "SELECT recordid, maxepisodes, title "
                       "FROM record WHERE maxepisodes > 0 "
                       "ORDER BY recordid ASC, maxepisodes DESC";

    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        VERBOSE(VB_GENERAL, QString("Found %1 record profiles using max episode expiration")
                                    .arg(query.numRowsAffected()));
        while (query.next()) {
            VERBOSE(VB_GENERAL, QString(" - %1").arg(query.value(2).toString()));
            maxEpisodes[query.value(0).toString()] = query.value(1).toInt();
        }
    }

    for(maxIter = maxEpisodes.begin(); maxIter != maxEpisodes.end(); maxIter++)
    {
        querystr = QString( "SELECT chanid, starttime, title FROM recorded "
                            "WHERE recordid = %1 AND preserve = 0 "
                            "ORDER BY starttime DESC;")
                            .arg(maxIter.key());

        query = db->exec(querystr);

        VERBOSE(VB_GENERAL, QString("Found %1 episodes in recording profile %2 using max expiration")
                                    .arg(query.numRowsAffected())
                                    .arg(maxIter.key()));
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            int found = 0;
            while (query.next()) {
                found++;

                if (found > maxIter.data())
                {
                    QString msg = QString("Expiring \"%1\" from %2, "
                                          "too many episodes.")
                                          .arg(query.value(2).toString())
                                          .arg(query.value(1).toString());
                    VERBOSE(VB_GENERAL, msg);
                    gContext->LogEntry("autoexpire", LP_NOTICE, "Expired program", msg);

                    QString message = QString("AUTO_EXPIRE %1 %2")
                                              .arg(query.value(0).toString())
                                              .arg(query.value(1).toDateTime()
                                                   .toString(Qt::ISODate));

                    MythEvent me(message);
                    gContext->dispatchNow(me);
                }
            }
        }
    }
}

void AutoExpire::FillExpireList(void)
{
    int expMethod = gContext->GetNumSetting("AutoExpireMethod", 0);

    ClearExpireList();

    switch(expMethod)
    {
        case 1: FillOldestFirst(); break;

        // default falls through so list is empty so no AutoExpire
    }
}

void AutoExpire::PrintExpireList(void)
{
    cout << "MythTV AutoExpire List (programs listed last will expire first)\n";

    vector<ProgramInfo *>::iterator i = expireList.begin();
    for(; i != expireList.end(); i++)
    {
        ProgramInfo *first = (*i);
        QString title = first->title;

        if (first->subtitle != "")
            title += ": \"" + first->subtitle + "\"";

        cout << title.local8Bit().leftJustify(40, ' ', true) << " "
             << first->startts.toString().local8Bit().leftJustify(24, ' ', true)
             << "  " << first->filesize / 1024 / 1024 << " MBytes"
             << endl;
    }
}


void AutoExpire::ClearExpireList(void)
{
    while (expireList.size() > 0)
    {
        ProgramInfo *pginfo = expireList.back();
        delete pginfo;
        expireList.erase(expireList.end() - 1);
    }
}

void AutoExpire::FillOldestFirst(void)
{
    QString fileprefix = gContext->GetFilePrefix();
    QString querystr = QString(
               "SELECT recorded.chanid,starttime,endtime,title,subtitle, "
               "description,hostname,channum,name,callsign,seriesid,programid "
               "FROM recorded "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "WHERE recorded.hostname = '%1' "
               "AND autoexpire > 0 "
               "ORDER BY autoexpire ASC, starttime DESC")
               .arg(gContext->GetHostName());

    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next()) {
            ProgramInfo *proginfo = new ProgramInfo;

            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = QString::fromUtf8(query.value(5).toString());
            proginfo->hostname = query.value(6).toString();

            if (proginfo->hostname.isEmpty())
                proginfo->hostname = gContext->GetHostName();

            if (!query.value(7).toString().isEmpty())
            {
                proginfo->chanstr = query.value(7).toString();
                proginfo->channame = query.value(8).toString();
                proginfo->chansign = query.value(9).toString();
            }
            else
            {
                proginfo->chanstr = "#" + proginfo->chanid;
                proginfo->channame = "#" + proginfo->chanid;
                proginfo->chansign = "#" + proginfo->chanid;
            }

            proginfo->seriesid = query.value(10).toString();
            proginfo->programid = query.value(11).toString();

            proginfo->pathname = proginfo->GetRecordFilename(fileprefix);

            struct stat st;
            long long size = 0;
            if (stat(proginfo->pathname.ascii(), &st) == 0)
                size = st.st_size;

            proginfo->filesize = size;

            expireList.push_back(proginfo);
        }
}

