#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qregexp.h>
#include <qstring.h>
#include <qdatetime.h>

#include <iostream>
#include <algorithm>
using namespace std;

#ifdef linux
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "scheduler.h"

#include "libmythtv/programinfo.h"
#include "libmythtv/scheduledrecording.h"
#include "encoderlink.h"
#include "libmyth/mythcontext.h"
#include "mainserver.h"
#include "remoteutil.h"

Scheduler::Scheduler(bool runthread, QMap<int, EncoderLink *> *tvList, 
                     QSqlDatabase *ldb)
{
    hasconflicts = false;
    db = ldb;
    m_tvList = tvList;

    m_mainServer = NULL;

    m_isShuttingDown = false;

    verifyCards();

    threadrunning = runthread;

    reclist_lock = new QMutex(true);
    schedlist_lock = new QMutex(true);

    if (runthread)
    {
        pthread_t scthread;
        pthread_create(&scthread, NULL, SchedulerThread, this);
    }
}

Scheduler::~Scheduler()
{
    while (reclist.size() > 0)
    {
        ProgramInfo *pginfo = reclist.back();
        delete pginfo;
        reclist.pop_back();
    }
}

void Scheduler::SetMainServer(MainServer *ms)
{
    m_mainServer = ms;
}

void Scheduler::verifyCards(void)
{
    QString thequery;

    thequery = "SELECT NULL FROM capturecard;";

    QSqlQuery query = db->exec(thequery);

    int numcards = -1;
    if (query.isActive())
        numcards = query.numRowsAffected();

    if (numcards <= 0)
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        cerr << "Perhaps you should read the installation instructions?\n";
        exit(5);
    }

    thequery = "SELECT sourceid,name FROM videosource ORDER BY sourceid;";

    query = db->exec(thequery);

    int numsources = -1;
    if (query.isActive())
    {
        numsources = query.numRowsAffected();

        int source = 0;

        while (query.next())
        {
            source = query.value(0).toInt();

            thequery = QString("SELECT cardinputid FROM cardinput WHERE "
                               "sourceid = %1 ORDER BY cardinputid;")
                              .arg(source);
            QSqlQuery subquery = db->exec(thequery);
            
            if (!subquery.isActive() || subquery.numRowsAffected() <= 0)
                cerr << query.value(1).toString() << " is defined, but isn't "
                     << "attached to a cardinput.\n";
        }
    }

    if (numsources <= 0)
    {
        cerr << "ERROR: No channel sources defined in the database\n";
        exit(0);
    }
}

bool Scheduler::CheckForChanges(void)
{
    return ScheduledRecording::hasChanged(db);
}

static inline bool Recording(ProgramInfo *p)
{
    return (p->recstatus == rsRecording || p->recstatus == rsWillRecord);
}

static bool comp_overlap(ProgramInfo *a, ProgramInfo *b)
{
    if (a->startts != b->startts)
        return a->startts < b->startts;
    if (a->endts != b->endts)
        return a->endts < b->endts;

    // Note: the PruneOverlaps logic depends on the following
    if (a->chanid != b->chanid)
        return a->chanid < b->chanid;
    if (a->inputid != b->inputid)
        return a->inputid < b->inputid;
    return a->recordid < b->recordid;
}

static bool comp_recstart(ProgramInfo *a, ProgramInfo *b)
{
    if (a->recstartts != b->recstartts)
        return a->recstartts < b->recstartts;
    if (a->recendts != b->recendts)
        return a->recendts < b->recendts;

    // Note: the PruneRedundants logic depends on the following
    if (a->recordid != b->recordid)
        return a->recordid < b->recordid;
    if (a->chansign != b->chansign)
        return a->chansign < b->chansign;
    return a->recstatus < b->recstatus;
}

static QDateTime schedTime;
static bool schedCardsFirst;

static bool comp_common(ProgramInfo *a, ProgramInfo *b)
{
    if (a->recpriority != b->recpriority)
        return a->recpriority > b->recpriority;

    int apast = (a->recstartts < schedTime.addSecs(-30) && !a->reactivate);
    int bpast = (b->recstartts < schedTime.addSecs(-30) && !b->reactivate);

    if (apast != bpast)
        return apast < bpast;

    int apri = RecTypePriority(a->rectype);
    int bpri = RecTypePriority(b->rectype);

    if (apri != bpri)
        return apri < bpri;

    if (a->recstartts != b->recstartts)
    {
        if (apast)
            return a->recstartts > b->recstartts;
        else
            return a->recstartts < b->recstartts;
    }

    if (a->inputid != b->inputid)
        return a->inputid < b->inputid;

    return a->recordid < b->recordid;
}

static bool comp_priority(ProgramInfo *a, ProgramInfo *b)
{
    int arec = (a->recstatus != rsRecording);
    int brec = (b->recstatus != rsRecording);

    if (arec != brec)
        return arec < brec;

    return comp_common(a, b);
}

static bool comp_retry(ProgramInfo *a, ProgramInfo *b)
{
    if (a->conflictpriority != b->conflictpriority)
        return a->conflictpriority < b->conflictpriority;

    if (a->numconflicts != b->numconflicts)
        return a->numconflicts < b->numconflicts;

    return comp_common(a, b);
}

void Scheduler::FillEncoderFreeSpaceCache()
{
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = enciter.data();
        enc->cacheFreeSpace();
    }
}

bool Scheduler::FillRecordLists(void)
{
    QMutexLocker lockit(reclist_lock);

    schedCardsFirst = (bool)gContext->GetNumSetting("SchedCardsFirst");
    schedMoveHigher = (bool)gContext->GetNumSetting("SchedMoveHigher");
    schedTime = QDateTime::currentDateTime();

    VERBOSE(VB_SCHEDULE, "PruneOldRecords...");
    PruneOldRecords();
    VERBOSE(VB_SCHEDULE, "AddNewRecords...");
    AddNewRecords();

    VERBOSE(VB_SCHEDULE, "Sort by time...");
    reclist.sort(comp_overlap);
    VERBOSE(VB_SCHEDULE, "PruneOverlaps...");
    PruneOverlaps();

    VERBOSE(VB_SCHEDULE, "Sort by priority...");
    reclist.sort(comp_priority);
    VERBOSE(VB_SCHEDULE, "BuildListMaps...");
    BuildListMaps();

    VERBOSE(VB_SCHEDULE, "SchedNewRecords...");
    SchedNewRecords();

    if (schedMoveHigher)
    {
        VERBOSE(VB_SCHEDULE, "Sort retrylist...");
        retrylist.sort(comp_retry);
        VERBOSE(VB_SCHEDULE, "MoveHigherRecords...");
        MoveHigherRecords();
        retrylist.clear();
    }

    VERBOSE(VB_SCHEDULE, "ClearListMaps...");
    ClearListMaps();

    VERBOSE(VB_SCHEDULE, "Sort by time...");
    reclist.sort(comp_recstart);
    VERBOSE(VB_SCHEDULE, "PruneRedundants...");
    PruneRedundants();

    return hasconflicts;
}

void Scheduler::FillRecordListFromMaster(void)
{
    ProgramList schedList(false);
    schedList.FromScheduler();

    QMutexLocker lockit(reclist_lock);

    ProgramInfo *p;
    for (p = schedList.first(); p; p = schedList.next())
        reclist.push_back(p);
}

void Scheduler::PrintList(bool onlyFutureRecordings)
{
    if ((print_verbose_messages & VB_SCHEDULE) == 0)
        return;

    QDateTime now = QDateTime::currentDateTime();

    cout << "--- print list start ---\n";
    cout << "Title - Subtitle                    Chan ChID Day Start  End   "
        "S C I  T N Pri" << endl;

    RecIter i = reclist.begin();
    for ( ; i != reclist.end(); i++)
    {
        ProgramInfo *first = (*i);

        if (onlyFutureRecordings &&
            (first->recendts < now ||
             (first->recstartts < now && !Recording(first))))
            continue;

        PrintRec(first);
    }

    cout << "---  print list end  ---\n";
}

void Scheduler::PrintRec(ProgramInfo *p, const char *prefix)
{
    if ((print_verbose_messages & VB_SCHEDULE) == 0)
        return;

    QString episode;

    if (prefix)
        cout << prefix;

    if (p->subtitle > " ")
        episode = QString("%1 - \"%2\"").arg(p->title.local8Bit())
            .arg(p->subtitle.local8Bit());
    else
        episode = p->title.local8Bit();

    cout << episode.leftJustify(35, ' ', true) << " "
         << p->chanstr.rightJustify(4, ' ') << " " << p->chanid 
         << p->recstartts.toString("  dd hh:mm-").local8Bit()
         << p->recendts.toString("hh:mm  ").local8Bit()
         << p->sourceid << " " << p->cardid << " "
         << p->inputid << "  " << p->RecTypeChar() << " "
         << p->RecStatusChar() << " "
         << QString::number(p->recpriority).rightJustify(3, ' ')
         << endl;
}

void Scheduler::UpdateRecStatus(ProgramInfo *pginfo)
{
    QMutexLocker lockit(reclist_lock);

    RecIter dreciter = reclist.begin();
    for (; dreciter != reclist.end(); ++dreciter)
    {
        ProgramInfo *p = *dreciter;
        if (p->IsSameProgramTimeslot(*pginfo))
        {
            p->recstatus = pginfo->recstatus;
            return;
        }
    }
}

bool Scheduler::ReactivateRecording(ProgramInfo *pginfo)
{
    QDateTime now = QDateTime::currentDateTime();

    if (pginfo->recstartts >= now || pginfo->recendts <= now)
        return false;

    QMutexLocker lockit(reclist_lock);

    RecIter dreciter = reclist.begin();
    for (; dreciter != reclist.end(); ++dreciter)
    {
        ProgramInfo *p = *dreciter;
        if (p->recordid == pginfo->recordid &&
            p->IsSameProgramTimeslot(*pginfo))
        {
            if (p->recstatus != rsRecorded &&
                p->recstatus != rsRecording &&
                p->recstatus != rsWillRecord)
            {
                p->reactivate = 1;
                ScheduledRecording::signalChange(db);
            }
            return true;
        }
    }

    return false;
}

void Scheduler::PruneOldRecords(void)
{
    QDateTime deltime = schedTime.addDays(-1);

    RecIter dreciter = reclist.begin();
    while (dreciter != reclist.end())
    {
        ProgramInfo *p = *dreciter;
        if (p->recendts < deltime ||
            (p->recstartts > schedTime && p->recstatus >= rsWillRecord && 
             p->recstatus != rsCancelled && p->recstatus != rsLowDiskSpace && 
             p->recstatus != rsTunerBusy))
        {
            delete p;
            dreciter = reclist.erase(dreciter);
        }
        else
        {
            if (p->recstatus == rsRecording && p->recendts < schedTime)
                p->recstatus = rsRecorded;
            dreciter++;
        }
    }
}

void Scheduler::PruneOverlaps(void)
{
    ProgramInfo *lastp = NULL;

    RecIter dreciter = reclist.begin();
    while (dreciter != reclist.end())
    {
        ProgramInfo *p = *dreciter;
        if (lastp == NULL || lastp->recordid == p->recordid ||
            !lastp->IsSameTimeslot(*p))
        {
            lastp = p;
            dreciter++;
        }
        else
        {
            int lpri = RecTypePriority(lastp->rectype);
            int cpri = RecTypePriority(p->rectype);
            if (lpri > cpri)
                *lastp = *p;
            delete p;
            dreciter = reclist.erase(dreciter);
        }
    }
}

void Scheduler::BuildListMaps(void)
{
    RecIter i = reclist.begin();
    for ( ; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording || 
            p->recstatus == rsWillRecord ||
            p->recstatus == rsUnknown)
        {
            cardlistmap[p->cardid].push_back(p);
            titlelistmap[p->title].push_back(p);
        }
    }
}

void Scheduler::ClearListMaps(void)
{
    cardlistmap.clear();
    titlelistmap.clear();
}

bool Scheduler::FindNextConflict(RecList &cardlist, ProgramInfo *p, RecIter &j)
{
    for ( ; j != cardlist.end(); j++)
    {
        ProgramInfo *q = *j;

        if (p == q)
            continue;
        if (!Recording(q))
            continue;
        if (p->cardid != 0 && p->cardid != q->cardid)
            continue;
        if (p->recendts <= q->recstartts || p->recstartts >= q->recendts)
            continue;
        if (p->inputid == q->inputid && p->shareable)
            continue;

        p->numconflicts++;
        if (q->recpriority > p->conflictpriority)
            p->conflictpriority = q->recpriority;
        return true;
    }

    return false;
}

void Scheduler::MarkOtherShowings(RecList &titlelist, ProgramInfo *p)
{
    RecIter i = titlelist.begin();
    for ( ; i != titlelist.end(); i++)
    {
        ProgramInfo *q = *i;
        if (q == p)
            continue;
        if (q->recstatus != rsUnknown && 
            q->recstatus != rsWillRecord &&
            q->recstatus != rsEarlierShowing &&
            q->recstatus != rsLaterShowing)
            continue;
        if (q->IsSameTimeslot(*p))
            q->recstatus = rsLaterShowing;
        else if (q->rectype != kSingleRecord && 
                 q->rectype != kOverrideRecord && 
                 q->IsSameProgram(*p))
        {
            if (q->recstartts < p->recstartts)
                q->recstatus = rsLaterShowing;
            else
                q->recstatus = rsEarlierShowing;
        }
    }
}

void Scheduler::BackupRecStatus(void)
{
    RecIter i = reclist.begin();
    for ( ; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        p->savedrecstatus = p->recstatus;
    }
}

void Scheduler::RestoreRecStatus(void)
{
    RecIter i = reclist.begin();
    for ( ; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        p->recstatus = p->savedrecstatus;
    }
}

bool Scheduler::TryAnotherShowing(RecList &titlelist, ProgramInfo *p)
{
    if (p->recstatus == rsRecording)
        return false;

    RecIter j = titlelist.begin();
    for ( ; j != titlelist.end(); j++)
    {
        ProgramInfo *q = *j;
        if (q == p)
            break;
    }

    p->recstatus = rsLaterShowing;

    for (j++; j != titlelist.end(); j++)
    {
        ProgramInfo *q = *j;
        if (q->recstatus != rsEarlierShowing &&
            q->recstatus != rsLaterShowing)
            continue;
        if (!p->IsSameProgram(*q))
            continue;
        if ((p->rectype == kSingleRecord || 
             p->rectype == kOverrideRecord) && 
            !p->IsSameTimeslot(*q))
            continue;
        if (q->recstartts < schedTime && p->recstartts >= schedTime)
            continue;

        RecList &cardlist = cardlistmap[q->cardid];
        RecIter k = cardlist.begin();
        if (FindNextConflict(cardlist, q, k))
            continue;

        q->recstatus = rsWillRecord;
        MarkOtherShowings(titlelist, q);
        PrintRec(p, "     -");
        PrintRec(q, "     +");
        return true;
    }

    p->recstatus = rsWillRecord;
    return false;
}

void Scheduler::SchedNewRecords(void)
{
    VERBOSE(VB_SCHEDULE, "Scheduling:");

    RecIter i = reclist.begin();
    for ( ; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording ||
            p->recstatus == rsWillRecord)
        {
            RecList &titlelist = titlelistmap[p->title];
            MarkOtherShowings(titlelist, p);
        }
        if (p->recstatus != rsUnknown)
            continue;

        RecList &cardlist = cardlistmap[p->cardid];
        RecIter k = cardlist.begin();
        if (!FindNextConflict(cardlist, p, k))
        {
            RecList &titlelist = titlelistmap[p->title];
            p->recstatus = rsWillRecord;
            MarkOtherShowings(titlelist, p);
            PrintRec(p, "  +");
        }
        else if (schedMoveHigher)
        {
            for (k++; !FindNextConflict(cardlist, p, k); k++)
                ;
            retrylist.push_back(p);
        }
    }
}

void Scheduler::MoveHigherRecords(void)
{
    VERBOSE(VB_SCHEDULE, "Retrying:");

    RecIter i = retrylist.begin();
    for ( ; i != retrylist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus != rsUnknown)
            continue;

        PrintRec(p, "  ?");

        BackupRecStatus();
        p->recstatus = rsWillRecord;
        RecList &titlelist = titlelistmap[p->title];
        MarkOtherShowings(titlelist, p);

        RecList &cardlist = cardlistmap[p->cardid];
        RecIter k = cardlist.begin();
        for ( ; FindNextConflict(cardlist, p, k); k++)
        {
            RecList &ktitlelist = titlelistmap[(*k)->title];
            if (!TryAnotherShowing(ktitlelist, *k))
            {
                RestoreRecStatus();
                break;
            }
        }

        if (p->recstatus == rsWillRecord)
            PrintRec(p, "  +");
    }
}

void Scheduler::PruneRedundants(void)
{
    ProgramInfo *lastp = NULL;
    hasconflicts = false;

    RecIter i = reclist.begin();
    while (i != reclist.end())
    {
        ProgramInfo *p = *i;

        // Check for rsConflict
        if (p->recstatus == rsUnknown)
        {
            p->recstatus = rsConflict;
            hasconflicts = true;
        }
        if (p->recstatus > rsWillRecord)
        {
            p->cardid = 0;
            p->inputid = 0;
        }

        // Check for redundant against last non-deleted
        if (lastp == NULL || lastp->recordid != p->recordid ||
            !lastp->IsSameTimeslot(*p))
        {
            lastp = p;
            i++;
        }
        else
        {
            delete p;
            i = reclist.erase(i);
        }
    }
}

void Scheduler::getConflicting(ProgramInfo *pginfo, QStringList &strlist)
{
    QMutexLocker lockit(reclist_lock);

    RecList *curList = getConflicting(pginfo);

    strlist << QString::number(curList->size());

    RecIter i = curList->begin();
    for ( ; i != curList->end(); i++)
        (*i)->ToStringList(strlist);

    delete curList;
}
 
RecList *Scheduler::getConflicting(ProgramInfo *pginfo)
{
    QMutexLocker lockit(reclist_lock);

    RecList *retlist = new RecList;

    RecIter i = reclist.begin();
    for (; FindNextConflict(reclist, pginfo, i); i++)
    {
        ProgramInfo *p = *i;
        retlist->push_back(p);
    }

    return retlist;
}

void Scheduler::getAllPending(RecList *retList)
{
    QMutexLocker lockit(reclist_lock);

    while (retList->size() > 0)
    {
        ProgramInfo *pginfo = retList->back();
        delete pginfo;
        retList->pop_back();
    }

    QDateTime now = QDateTime::currentDateTime();

    RecIter i = reclist.begin();
    for (; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording && p->recendts < now)
            p->recstatus = rsRecorded;
        retList->push_back(new ProgramInfo(*p));
    }
}

void Scheduler::getAllPending(QStringList &strList)
{
    QMutexLocker lockit(reclist_lock);

    strList << QString::number(hasconflicts);
    strList << QString::number(reclist.size());

    QDateTime now = QDateTime::currentDateTime();

    RecIter i = reclist.begin();
    for (; i != reclist.end(); i++)
    {
        ProgramInfo *p = *i;
        if (p->recstatus == rsRecording && p->recendts < now)
            p->recstatus = rsRecorded;
        p->ToStringList(strList);
    }
}

RecList *Scheduler::getAllScheduled(void)
{
    while (schedlist.size() > 0)
    {
        ProgramInfo *pginfo = schedlist.back();
        delete pginfo;
        schedlist.pop_back();
    }

    findAllScheduledPrograms(schedlist);

    return &schedlist;
}

void Scheduler::getAllScheduled(QStringList &strList)
{
    QMutexLocker lockit(schedlist_lock);

    getAllScheduled();

    strList << QString::number(schedlist.size());

    RecIter i = schedlist.begin();
    for (; i != schedlist.end(); i++)
        (*i)->ToStringList(strList);
}

void Scheduler::RunScheduler(void)
{
    int prerollseconds = 0;

    int secsleft;
    EncoderLink *nexttv = NULL;

    ProgramInfo *nextRecording = NULL;
    QDateTime nextrectime;

    QDateTime curtime;
    QDateTime lastupdate = QDateTime::currentDateTime().addDays(-1);

    QString recordfileprefix = gContext->GetFilePrefix();

    RecIter startIter = reclist.begin();

    bool blockShutdown = gContext->GetNumSetting("blockSDWUwithoutClient", 1);
    QDateTime idleSince = QDateTime();
    int idleTimeoutSecs = 0;
    int idleWaitForRecordingTime = 0;
    bool firstRun = true;

    struct timeval fillstart, fillend;

    // wait for slaves to connect
    sleep(2);

    while (1)
    {
        curtime = QDateTime::currentDateTime();
        bool statuschanged = false;

        if ((startIter == reclist.end() ||
             curtime.secsTo((*startIter)->recstartts) > 30) &&
            (CheckForChanges() ||
             (lastupdate.date().day() != curtime.date().day())))
        {
            VERBOSE(VB_GENERAL, "Found changes in the todo list.");
            FillEncoderFreeSpaceCache();
            gettimeofday(&fillstart, NULL);
            FillRecordLists();
            gettimeofday(&fillend, NULL);
            PrintList();
            VERBOSE(VB_GENERAL, QString("Scheduled %1 items in %2 seconds.")
                    .arg(reclist.size())
                    .arg(((fillend.tv_sec  - fillstart.tv_sec ) * 1000000 +
                         (fillend.tv_usec - fillstart.tv_usec)) / 1000000.0));

            lastupdate = curtime;
            startIter = reclist.begin();
            statuschanged = true;

            // Determine if the user wants us to start recording early
            // and by how many seconds
            prerollseconds = gContext->GetNumSetting("RecordPreRoll");

            idleTimeoutSecs = gContext->GetNumSetting("idleTimeoutSecs", 0);
            idleWaitForRecordingTime =
                       gContext->GetNumSetting("idleWaitForRecordingTime", 15);

            if (firstRun)
            {
                //the parameter given to the startup_cmd. "user" means a user
                // started the BE, 'auto' means it was started automatically
                QString startupParam = "user";
                
                // check once on startup, if a recording starts within the
                // idleWaitForRecordingTime. If no, block the shutdown,
                // because system seems to be waken up by the user and not by a
                // wakeup call
                if (blockShutdown)
                {
                    // have we been started automatically?
                    if (startIter != reclist.end() &&
                        curtime.secsTo((*startIter)->startts) - prerollseconds
                        < idleWaitForRecordingTime * 60)
                    {
                        VERBOSE(VB_ALL,
                                "Recording starts soon, AUTO-Startup assumed");
                        blockShutdown = false;
                        startupParam = "auto";
                    }
                    else
                        VERBOSE(VB_ALL, "Seem to be woken up by USER");
                }
                QString startupCommand = gContext->GetSetting("startupCommand",
                                                              "");
                if (!startupCommand.isEmpty())
                {
                    startupCommand.replace("$status", startupParam);
                    system(startupCommand.ascii());
                }
                firstRun = false;
            }
        }

        for ( ; startIter != reclist.end(); startIter++)
            if ((*startIter)->recstatus == rsWillRecord)
                break;

        RecIter recIter = startIter;
        for ( ; recIter != reclist.end(); recIter++)
        {
            QString msg;

            nextRecording = *recIter;

            if (nextRecording->recstatus != rsWillRecord)
                continue;

            nextrectime = nextRecording->recstartts;
            secsleft = curtime.secsTo(nextrectime);

            if (secsleft - prerollseconds > 35)
                break;

            if (m_tvList->find(nextRecording->cardid) == m_tvList->end())
            {
                msg = QString("invalid cardid (%1) for %2")
                    .arg(nextRecording->cardid)
                    .arg(nextRecording->title);
                VERBOSE(VB_GENERAL, msg);

                QMutexLocker lockit(reclist_lock);
                nextRecording->recstatus = rsTunerBusy;
                statuschanged = true;
                continue;
            }

            nexttv = (*m_tvList)[nextRecording->cardid];
            // cerr << "nexttv = " << nextRecording->cardid;
            // cerr << " title: " << nextRecording->title << endl;

            if (nexttv->isLowOnFreeSpace())
            {
                FillEncoderFreeSpaceCache();
                if (nexttv->isLowOnFreeSpace())
                {
                    msg = QString("SUPPRESSED recording '%1' on channel"
                                  " %2 on cardid %3, sourceid %4.  Only"
                                  " %5 Megs of disk space available.")
                        .arg(nextRecording->title.utf8())
                        .arg(nextRecording->chanid)
                        .arg(nextRecording->cardid)
                        .arg(nextRecording->sourceid)
                        .arg(nexttv->getFreeSpace());
                    VERBOSE(VB_GENERAL, msg);

                    QMutexLocker lockit(reclist_lock);
                    nextRecording->recstatus = rsLowDiskSpace;
                    statuschanged = true;
                    continue;
                }
            }

            if (nexttv->isTunerLocked())
            {
                msg = QString("SUPPRESSED recording \"%1\" on channel: "
                              "%2 on cardid: %3, sourceid %4. Tuner "
                              "is locked by an external application.")
                    .arg(nextRecording->title.utf8())
                    .arg(nextRecording->chanid)
                    .arg(nextRecording->cardid)
                    .arg(nextRecording->sourceid);
                VERBOSE(VB_GENERAL, msg);

                QMutexLocker lockit(reclist_lock);
                nextRecording->recstatus = rsTunerBusy;
                statuschanged = true;
                continue;
            }

            if (!nexttv->IsBusyRecording())
            {
                // Will use pre-roll settings only if no other
                // program is currently being recorded
                secsleft -= prerollseconds;
            }

            //VERBOSE(VB_GENERAL, secsleft << " seconds until " << nextRecording->title);

            if (secsleft > 30)
                continue;

            if (secsleft > 2)
            {
                QString id = nextRecording->schedulerid;
                if (!recPendingList.contains(id))
                    recPendingList[id] = false;
                if (recPendingList[id] == false)
                {
                    nexttv->RecordPending(nextRecording, secsleft);
                    recPendingList[id] = true;
                }
            }

            if (secsleft > -2)
                continue;

            nextRecording->recstartts = 
                QDateTime::currentDateTime().addSecs(30);
            nextRecording->recstartts.setTime(QTime(
                nextRecording->recstartts.time().hour(),
                nextRecording->recstartts.time().minute()));

            QMutexLocker lockit(reclist_lock);

            int retval = nexttv->StartRecording(nextRecording);
            if (retval > 0)
            {
                msg = "Started recording";
                nextRecording->recstatus = rsRecording;
            }
            else
            {
                msg = "Canceled recording"; 
                if (retval < 0)
                    nextRecording->recstatus = rsTunerBusy;
                else
                    nextRecording->recstatus = rsCancelled;
            }
            statuschanged = true;

            msg += QString(" \"%1\" on channel: %2 on cardid: %3, "
                           "sourceid %4").arg(nextRecording->title.utf8())
                .arg(nextRecording->chanid)
                .arg(nextRecording->cardid)
                .arg(nextRecording->sourceid);
            VERBOSE(VB_GENERAL, msg);
        }

        if (statuschanged)
        {
            MythEvent me("SCHEDULE_CHANGE");
            gContext->dispatch(me);
        }

        // if idletimeout is 0, the user disabled the auto-shutdown feature
        if ((idleTimeoutSecs > 0) && (m_mainServer != NULL)) 
        {
            // we release the block when a client connects
            if (blockShutdown)
                blockShutdown &= !m_mainServer->isClientConnected();
            else
            {
                // find out, if we are currently recording (or LiveTV)
                bool recording = false;
                QMap<int, EncoderLink *>::Iterator it;
                for (it = m_tvList->begin(); (it != m_tvList->end()) && 
                     !recording; ++it)
                {
                    if (it.data()->IsBusy())
                        recording = true;
                }
                
                if (!(m_mainServer->isClientConnected()) && !recording)
                {
                    if (!idleSince.isValid())
                    {
                        if (startIter != reclist.end())
                        {
                            if (curtime.secsTo((*startIter)->startts) - 
                                prerollseconds > idleWaitForRecordingTime * 60)
                            {
                                idleSince = curtime;
                            }
                        }
                        else
                            idleSince = curtime;
                    } 
                    else 
                    {
                        // is the machine already ideling the timeout time?
                        if (idleSince.addSecs(idleTimeoutSecs) < curtime)
                        {
                            if (!m_isShuttingDown &&
                                CheckShutdownServer(prerollseconds, idleSince,
                                                    blockShutdown))
                            {
                                ShutdownServer(prerollseconds);
                            }

                        }
                        else
                        {
                            int itime = idleSince.secsTo(curtime);
                            QString msg;
                            if (itime == 1)
                            {
                                msg = QString("I\'m idle now... shutdown will "
                                              "occur in %1 seconds.")
                                             .arg(idleTimeoutSecs);
                                VERBOSE(VB_ALL, msg);
                            }
                            else if (itime % 10 == 0)
                            {
                                msg = QString("%1 secs left to system "
                                              "shutdown!")
                                             .arg(idleTimeoutSecs - itime);
                                VERBOSE(VB_ALL, msg);
                            }
                        }
                    }
                }
                else
                    // not idle, make the time invalid
                    idleSince = QDateTime();
            }
        }

        sleep(1);
    }
} 

//returns true, if the shutdown is not blocked
bool Scheduler::CheckShutdownServer(int prerollseconds, QDateTime &idleSince, 
                                    bool &blockShutdown)
{
    (void)prerollseconds;
    bool retval = false;
    QString preSDWUCheckCommand = gContext->GetSetting("preSDWUCheckCommand", 
                                                       "");

    int state = 0;
    if (!preSDWUCheckCommand.isEmpty())
    {
        state = system(preSDWUCheckCommand.ascii());
                      
        if (WIFEXITED(state) && state != -1)
        {
            retval = false;
            switch(WEXITSTATUS(state))
            {
                case 0:
                    retval = true;
                    break;
                case 1:
                    // just reset idle'ing on retval == 1
                    idleSince = QDateTime();
                    break;
                case 2:
                    // reset shutdown status on retval = 2
                    // (needs a clientconnection again,
                    // before shutdown is executed)
                    blockShutdown
                             = gContext->GetNumSetting("blockSDWUwithoutClient",
                                                       1);
                    idleSince = QDateTime();
                    break;
                // case 3:
                //    //disable shutdown routine generally
                //    m_noAutoShutdown = true;
                //    break;
                default:
                    break;
            }
        }
    }
    return retval;
}

void Scheduler::ShutdownServer(int prerollseconds)
{    
    m_isShuttingDown = true;
  
    RecIter recIter = reclist.begin();
    for ( ; recIter != reclist.end(); recIter++)
        if ((*recIter)->recstatus == rsWillRecord)
            break;

    // set the wakeuptime if needed
    if (recIter != reclist.end())
    {
        ProgramInfo *nextRecording = (*recIter);
        QDateTime restarttime = nextRecording->startts.addSecs((-1) * 
                                                               prerollseconds);

        int add = gContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1) * add);

        QString wakeup_timeformat = gContext->GetSetting("WakeupTimeFormat",
                                                         "hh:mm yyyy-MM-dd");
        QString setwakeup_cmd = gContext->GetSetting("SetWakeuptimeCommand",
                                                     "echo \'Wakeuptime would "
                                                     "be $time if command "
                                                     "set.\'");

        if (wakeup_timeformat == "time_t")
        {
            QString time_ts;
            setwakeup_cmd.replace("$time", 
                                  time_ts.setNum(restarttime.toTime_t()));
        }
        else
            setwakeup_cmd.replace("$time", 
                                  restarttime.toString(wakeup_timeformat));

        // now run the command to set the wakeup time
        if (!setwakeup_cmd.isEmpty())
            system(setwakeup_cmd.ascii());
    }

    QString halt_cmd = gContext->GetSetting("ServerHaltCommand",
                                            "sudo /sbin/halt -p");

    if (!halt_cmd.isEmpty())
    {
        // now we shut the slave backends down...
        m_mainServer->ShutSlaveBackendsDown(halt_cmd);

        // and now shutdown myself
        system(halt_cmd.ascii());
    }
}

void *Scheduler::SchedulerThread(void *param)
{
    Scheduler *sched = (Scheduler *)param;
    sched->RunScheduler();
 
    return NULL;
}

void Scheduler::BuildNewRecordsQueries(QStringList &from, QStringList &where)
{
    QString query;
    QSqlQuery result;
    QString qphrase;

    from << "";
    where << QString("record.search = %1 AND "
                     "program.title = record.title").arg(kNoSearch);

    query = QString("SELECT recordid,search,subtitle,description "
                    "FROM record WHERE search <> %1").arg(kNoSearch);

    result = db->exec(query);
    if (!result.isActive())
    {
        MythContext::DBError("BuildNewRecordsQueries", result);
        return;
    }

    while (result.next())
    {
    qphrase = result.value(3).toString();
    qphrase.replace("\'", "\\\'");

        switch (result.value(1).toInt())
        {
        case kPowerSearch:
            from << result.value(2).toString();
            where << QString("record.recordid = %1 AND %2")
                .arg(result.value(0).toString())
                .arg(result.value(2).toString())
                .arg(qphrase);
            break;
        case kTitleSearch:
            from << "";
            where << QString("record.recordid = %1 AND "
                             "program.title LIKE '\%%2\%'")
                .arg(result.value(0).toString())
                .arg(qphrase);
            break;
        case kKeywordSearch:
            from << "";
            where << QString("record.recordid = %1 AND "
                             "(program.title LIKE '\%%2\%' OR "
                             " program.subtitle LIKE '\%%3\%' OR "
                             " program.description LIKE '\%%4\%')")
                .arg(result.value(0).toString())
                .arg(qphrase).arg(qphrase).arg(qphrase);
            break;
        case kPeopleSearch:
            from << ", people, credits";
            where << QString("record.recordid = %1 AND "
                             "people.name LIKE '\%%2\%' AND "
                             "credits.person = people.person AND "
                             "program.chanid = credits.chanid AND "
                             "program.starttime = credits.starttime")
                .arg(result.value(0).toString())
                .arg(qphrase);
            break;
        default:
            cerr << "Unknown RecSearchType (" << result.value(1).toInt()
                 << ") for recordid " << result.value(0).toString() << endl;
            break;
        }
    }
}

void Scheduler::AddNewRecords(void) {
    QMap<RecordingType, int> recTypeRecPriorityMap;
    RecList tmpList;
    QMap<int, bool> allowmap;

    QMap<int, bool> cardMap;
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *enc = enciter.data();
        if (enc->isConnected())
            cardMap[enc->getCardId()] = true;
    }

    unsigned clause;
    QStringList fromclauses, whereclauses;

    BuildNewRecordsQueries(fromclauses, whereclauses);

    if (print_verbose_messages & VB_SCHEDULE)
    {
        for (clause = 0; clause < fromclauses.count(); clause++)
            cout << "Query " << clause << ": " << fromclauses[clause] 
                 << "/" << whereclauses[clause] << endl;
    }

    for (clause = 0; clause < fromclauses.count(); clause++)
    {

    QString query = QString(
"SELECT DISTINCT channel.chanid, channel.sourceid, "
"program.starttime, program.endtime, "
"program.title, program.subtitle, program.description, "
"channel.channum, channel.callsign, channel.name, "
"oldrecorded.endtime IS NOT NULL AS oldrecduplicate, program.category, "
"record.recpriority + channel.recpriority + "
"cardinput.preference, "
"record.dupin, "
"recorded.endtime IS NOT NULL AND recorded.endtime < NOW() AS recduplicate, "
"record.type, record.recordid, 0, "
"program.starttime - INTERVAL record.startoffset minute AS recstartts, "
"program.endtime + INTERVAL record.endoffset minute AS recendts, "
"program.previouslyshown, record.recgroup, record.dupmethod, "
"channel.commfree, capturecard.cardid, "
"cardinput.cardinputid, UPPER(cardinput.shareable) = 'Y' AS shareable, "
"program.seriesid, program.programid, "
"program.stars, program.originalairdate "

"FROM record, program ") + fromclauses[clause] + QString(

" INNER JOIN channel ON (channel.chanid = program.chanid) "
" INNER JOIN cardinput ON (channel.sourceid = cardinput.sourceid) "
" INNER JOIN capturecard ON (capturecard.cardid = cardinput.cardid) "
" LEFT JOIN oldrecorded ON "
"  ( "
"    record.dupmethod > 1 AND "
"    program.title = oldrecorded.title "
"     AND "
"     ( "
"      (program.programid <> '' AND program.programid NOT LIKE 'SH%0000' "
"       AND program.programid = oldrecorded.programid) "
"      OR "
"      ( "
"       (program.programid = '' OR oldrecorded.programid = '') "
"       AND "
"       (((record.dupmethod & 0x02) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = oldrecorded.subtitle)) "
"       AND "
"       (((record.dupmethod & 0x04) = 0) OR (program.description <> '' "
"          AND program.description = oldrecorded.description)) "
"      ) "
"     ) "
"  ) "
" LEFT JOIN recorded ON "
"  ( "
"    record.dupmethod > 1 AND "
"    program.title = recorded.title "
"     AND "
"     ( "
"      (program.programid <> '' AND program.programid NOT LIKE 'SH%0000' "
"       AND program.programid = recorded.programid) "
"      OR "
"      ( "
"       (program.programid = '' OR recorded.programid = '') "
"       AND "
"       (((record.dupmethod & 0x02) = 0) OR (program.subtitle <> '' "
"          AND program.subtitle = recorded.subtitle)) "
"       AND "
"       (((record.dupmethod & 0x04) = 0) OR (program.description <> '' "
"          AND program.description = recorded.description)) "
"      ) "
"     ) "
"  ) "

"WHERE ") + whereclauses[clause] + QString(" AND "

"((record.type = %1 " // allrecord
"OR record.type = %2) " // findonerecord
" OR "
" ((record.station = channel.callsign) " // channel matches
"  AND "
"  ((record.type = %3) " // channelrecord
"   OR"
"   ((TIME_TO_SEC(record.starttime) = TIME_TO_SEC(program.starttime)) " // timeslot matches
"    AND "
"    ((record.type = %4) " // timeslotrecord
"     OR"
"     ((DAYOFWEEK(record.startdate) = DAYOFWEEK(program.starttime) "
"      AND "
"      ((record.type = %5) " // weekslotrecord
"       OR"
"       ((TO_DAYS(record.startdate) = TO_DAYS(program.starttime)) " // date matches
"        AND "
"        (TIME_TO_SEC(record.endtime) = TIME_TO_SEC(program.endtime)) "
"        AND "
"        (TO_DAYS(record.enddate) = TO_DAYS(program.endtime)) "
"        )"
"       )"
"      )"
"     )"
"    )"
"   )"
"  )"
" )"
") ")
        .arg(kAllRecord)
        .arg(kFindOneRecord)
        .arg(kChannelRecord)
        .arg(kTimeslotRecord)
        .arg(kWeekslotRecord);

    VERBOSE(VB_SCHEDULE, QString(" |-- Start DB Query %1...").arg(clause));
    QSqlQuery result = db->exec(query);

    if (!result.isActive())
    {
        MythContext::DBError("AddNewRecords", result);
        return;
    }

    VERBOSE(VB_SCHEDULE, QString(" |-- Processing %1 results...")
                          .arg(result.size()));
    while (result.next())
    {
        // Don't bother if card isn't on-line of end time has already passed
        int cardid = result.value(24).toInt();
        if (threadrunning && !cardMap.contains(cardid))
            continue;
        QDateTime recendts = result.value(19).toDateTime();
        if (recendts < schedTime)
            continue;

        ProgramInfo *p = new ProgramInfo;
        p->recstatus = rsUnknown;
        p->chanid = result.value(0).toString();
        p->sourceid = result.value(1).toInt();
        p->startts = result.value(2).toDateTime();
        p->endts = result.value(3).toDateTime();
        p->title = QString::fromUtf8(result.value(4).toString());
        p->subtitle = QString::fromUtf8(result.value(5).toString());
        p->description = QString::fromUtf8(result.value(6).toString());
        p->chanstr = result.value(7).toString();
        p->chansign = result.value(8).toString();
        p->channame = QString::fromUtf8(result.value(9).toString());
        p->category = QString::fromUtf8(result.value(11).toString());
        p->recpriority = result.value(12).toInt();
        p->dupin = RecordingDupInType(result.value(13).toInt());
        p->dupmethod = RecordingDupMethodType(result.value(22).toInt());
        p->rectype = RecordingType(result.value(15).toInt());
        p->recordid = result.value(16).toInt();

        p->recstartts = result.value(18).toDateTime();
        p->recendts = recendts;
        p->chancommfree = result.value(23).toInt();
        p->cardid = cardid;
        p->inputid = result.value(25).toInt();
        p->shareable = result.value(26).toInt();
        p->seriesid = result.value(27).toString();
        p->programid = result.value(28).toString();
        p->stars =  result.value(29).toDouble();
        
        p->repeat = result.value(20).toInt();
        
        if(result.value(30).isNull())
            p->originalAirDate = p->startts.date();
        else
        {
            p->originalAirDate = QDate::fromString(result.value(30).toString(), Qt::ISODate);
            
            if(p->originalAirDate < p->startts.date())
                p->repeat = true;
        }

        if (!recTypeRecPriorityMap.contains(p->rectype))
            recTypeRecPriorityMap[p->rectype] = 
                p->GetRecordingTypeRecPriority(p->rectype);
        p->recpriority += recTypeRecPriorityMap[p->rectype];

        if (p->recstartts >= p->recendts)
        {
            // pre/post-roll are invalid so ignore
            p->recstartts = p->startts;
            p->recendts = p->endts;
        }

        
        p->recgroup = result.value(21).toString();

        p->schedulerid = 
            p->startts.toString() + "_" + p->chanid;

        // would save many queries to create and populate a
        // ScheduledRecording and put it in the proginfo at the
        // same time, since it will be loaded later anyway with
        // multiple queries

        // Chedk if already in reclist and don't bother if so
        RecIter rec = reclist.begin();
        for ( ; rec != reclist.end(); rec++)
        {
            ProgramInfo *r = *rec;
            if (p->IsSameTimeslot(*r))
            {
                if (r->reactivate > 0)
                {
                    r->reactivate = 2;
                    p->reactivate = -1;
                }
                else
                {
                    delete p;
                    p = NULL;
                }
                break;
            }
        }
        if (p == NULL)
            continue;

        // Check for rsTooManyRecordings
        if (!allowmap.contains(p->recordid))
            allowmap[p->recordid] = p->AllowRecordingNewEpisodes(db);
        if (!p->reactivate && !allowmap[p->recordid])
            p->recstatus = rsTooManyRecordings;

        // Check for rsCurrentRecording and rsPreviousRecording
        if (p->rectype == kDontRecord)
            p->recstatus = rsDontRecord;
        else if (p->rectype != kSingleRecord &&
                 p->rectype != kOverrideRecord &&
                 !p->reactivate && (p->dupmethod == kDupCheckNewEpi))
        {
            if(p->repeat || result.value(10).toInt() || result.value(14).toInt())
                p->recstatus = rsRepeat;
        }
        else if (p->rectype != kSingleRecord &&
                 p->rectype != kOverrideRecord &&
                 !p->reactivate &&
                 !(p->dupmethod & kDupCheckNone))
        {
            if (p->dupin & kDupsInOldRecorded &&
                result.value(10).toInt())
                p->recstatus = rsPreviousRecording;
            if (p->dupin & kDupsInRecorded &&
                result.value(14).toInt())
                p->recstatus = rsCurrentRecording;
        }

        tmpList.push_back(p);
    }

    }

    VERBOSE(VB_SCHEDULE, " +-- Cleanup...");
    // Delete existing programs that were reactivated
    RecIter rec = reclist.begin();
    while (rec != reclist.end())
    {
        ProgramInfo *r = *rec;
        if (r->reactivate < 2)
            rec++;
        else
        {
            delete r;
            rec = reclist.erase(rec);
        }
    }

    RecIter tmp = tmpList.begin();
    for ( ; tmp != tmpList.end(); tmp++)
        reclist.push_back(*tmp);
}

void Scheduler::findAllScheduledPrograms(list<ProgramInfo *> &proglist)
{
    QString temptime, tempdate;
    QString query = QString("SELECT record.chanid, record.starttime, "
"record.startdate, record.endtime, record.enddate, record.title, "
"record.subtitle, record.description, record.recpriority, record.type, "
"channel.name, record.recordid, record.recgroup, record.dupin, "
"record.dupmethod, channel.commfree, channel.channum, record.station, "
"record.seriesid, record.programid "
"FROM record "
"LEFT JOIN channel ON channel.callsign = record.station "
"GROUP BY recordid "
"ORDER BY title ASC;");

    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) 
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = result.value(0).toString();
            proginfo->rectype = RecordingType(result.value(9).toInt());
            proginfo->recordid = result.value(11).toInt();

            if (proginfo->rectype == kSingleRecord || 
                proginfo->rectype == kOverrideRecord ||
                proginfo->rectype == kTimeslotRecord ||
                proginfo->rectype == kWeekslotRecord) 
            {
                proginfo->startts = QDateTime(result.value(2).toDate(),
                                              result.value(1).toTime());
                proginfo->endts = QDateTime(result.value(4).toDate(),
                                            result.value(3).toTime());
            }
            else 
            {
                // put currentDateTime() in time fields to prevent
                // Invalid date/time warnings later
                proginfo->startts = QDateTime::currentDateTime();
                proginfo->startts.setTime(QTime(0,0));
                proginfo->endts = QDateTime::currentDateTime();
                proginfo->endts.setTime(QTime(0,0));
            }

            proginfo->title = QString::fromUtf8(result.value(5).toString());
            proginfo->subtitle =
                    QString::fromUtf8(result.value(6).toString());
            proginfo->description =
                QString::fromUtf8(result.value(7).toString());

            proginfo->recpriority = result.value(8).toInt();
            proginfo->channame = QString::fromUtf8(result.value(10).toString());
            if (proginfo->channame.isNull())
                proginfo->channame = "";
            proginfo->recgroup = result.value(12).toString();
            proginfo->dupin = RecordingDupInType(result.value(13).toInt());
            proginfo->dupmethod =
                RecordingDupMethodType(result.value(14).toInt());
            proginfo->chancommfree = result.value(15).toInt();
            proginfo->chanstr = result.value(16).toString();
            if (proginfo->chanstr.isNull())
                proginfo->chanstr = "";
            proginfo->chansign = result.value(17).toString();
            proginfo->seriesid = result.value(18).toString();
            proginfo->programid = result.value(19).toString();
            
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;

            proglist.push_back(proginfo);
        }
}

