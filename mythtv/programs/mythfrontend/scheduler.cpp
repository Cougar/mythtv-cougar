#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdatetime.h>

#include <algorithm>
using namespace std;

#include "scheduler.h"
#include "infostructs.h"

Scheduler::Scheduler(QSqlDatabase *ldb)
{
    hasconflicts = false;
    db = ldb;
}

Scheduler::~Scheduler()
{
    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }
}

bool Scheduler::CheckForChanges(void)
{
    QSqlQuery query;
    char thequery[512];

    bool retval = false;

    sprintf(thequery, "SELECT * FROM settings WHERE value = "
                      "\"RecordChanged\";");
    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString value = query.value(1).toString();
        if (value == "yes")
        {
            sprintf(thequery, "UPDATE settings SET data = \"no\" WHERE "
                              "value = \"RecordChanged\";");
            query = db->exec(thequery);

            retval = true;
        }
    }

    return retval;
}

class comp_proginfo 
{
  public:
    bool operator()(ProgramInfo *a, ProgramInfo *b)
    {
        return (a->startts < b->startts);
    }
};

bool Scheduler::FillRecordLists(bool doautoconflicts)
{
    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }

    char thequery[512];
    QSqlQuery query;
    QSqlQuery subquery;

    sprintf(thequery, "SELECT * FROM singlerecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->title = query.value(3).toString();
            proginfo->subtitle = query.value(4).toString();
            proginfo->description = query.value(5).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->channum = query.value(0).toString();
            proginfo->recordtype = 1;

            recordingList.push_back(proginfo);
        }
    }

    sprintf(thequery, "SELECT * FROM timeslotrecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString channum = query.value(0).toString();
            QString starttime = query.value(1).toString();
            QString endtime = query.value(2).toString();
            QString title = query.value(3).toString();

            int hour, min;

            hour = starttime.mid(0, 2).toInt();
            min = starttime.mid(3, 2).toInt();

            QDate curdate = QDate::currentDate();
            char startquery[128], endquery[128];

            sprintf(startquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), hour, min);

            curdate = curdate.addDays(2);
            sprintf(endquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), hour, min);

            sprintf(thequery, "SELECT * FROM program WHERE channum = %s AND "
                              "starttime = %s AND endtime < %s AND "
                              "title = \"%s\";", channum.ascii(), startquery,
                              endquery, title.ascii());
            subquery = db->exec(thequery);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                while (subquery.next())
                {
                    ProgramInfo *proginfo = new ProgramInfo;
                    proginfo->title = subquery.value(3).toString();
                    proginfo->subtitle = subquery.value(4).toString();
                    proginfo->description = subquery.value(5).toString();
                    proginfo->startts = 
                             QDateTime::fromString(subquery.value(1).toString(),
                                                   Qt::ISODate);
                    proginfo->endts = 
                             QDateTime::fromString(subquery.value(2).toString(),
                                                   Qt::ISODate);
                    proginfo->channum = subquery.value(0).toString();
                    proginfo->recordtype = 2;

                    recordingList.push_back(proginfo);
                }
            }
        }
    }

    sprintf(thequery, "SELECT * FROM allrecord;");

    query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString title = query.value(0).toString();

            QTime curtime = QTime::currentTime();
            QDate curdate = QDate::currentDate();
            char startquery[128], endquery[128];

            sprintf(startquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), curtime.hour(), 
                    curtime.minute());

            curdate = curdate.addDays(2);
            sprintf(endquery, "%4d%02d%02d%02d%02d00", curdate.year(),
                    curdate.month(), curdate.day(), curtime.hour(), 
                    curtime.minute());

            sprintf(thequery, "SELECT * FROM program WHERE "
                              "starttime >= %s AND endtime < %s AND "
                              "title = \"%s\";", startquery,
                              endquery, title.ascii());
            subquery = db->exec(thequery);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                while (subquery.next())
                {
                    ProgramInfo *proginfo = new ProgramInfo;
                    proginfo->title = subquery.value(3).toString();
                    proginfo->subtitle = subquery.value(4).toString();
                    proginfo->description = subquery.value(5).toString();
                    proginfo->startts = 
                             QDateTime::fromString(subquery.value(1).toString(),
                                                   Qt::ISODate);
                    proginfo->endts = 
                             QDateTime::fromString(subquery.value(2).toString(),
                                                   Qt::ISODate);
                    proginfo->channum = subquery.value(0).toString();
                    proginfo->recordtype = 3;

                    recordingList.push_back(proginfo);
                }
            }
        }
    }

    if (recordingList.size() > 0)
    {
        recordingList.sort(comp_proginfo());
        MarkConflicts();
        PruneList();
        MarkConflicts();
        MarkConflictsToRemove();
        if (doautoconflicts)
        {
            RemoveConflicts();
            GuessConflicts();
            RemoveConflicts();
        }
        MarkConflicts();
    }

    return hasconflicts;
}

ProgramInfo *Scheduler::GetNextRecording(void)
{
    if (recordingList.size() > 0)
        return recordingList.front();
    return NULL;
}

void Scheduler::RemoveFirstRecording(void)
{
    if (recordingList.size() == 0)
        return;

    ProgramInfo *rec = recordingList.front();

    if (rec->recordtype == 1)
    {
        char startt[128];
        char endt[128];

        QString starts = rec->startts.toString("yyyyMMddhhmm");
        QString ends = rec->endts.toString("yyyyMMddhhmm");
    
        sprintf(startt, "%s00", starts.ascii());
        sprintf(endt, "%s00", ends.ascii());
    
        QSqlQuery query;                                          
        char thequery[512];
        sprintf(thequery, "DELETE FROM singlerecord WHERE "
                          "channum = %d AND starttime = %s AND "
                          "endtime = %s;", rec->channum.toInt(),
                          startt, endt);

        query = db->exec(thequery);
    }

    delete rec;
    recordingList.pop_front();
}

bool Scheduler::Conflict(ProgramInfo *a, ProgramInfo *b)
{
    if ((a->startts <= b->startts && b->startts < a->endts) ||
        (b->endts <= a->endts && a->startts < b->endts))
        return true;
    return false;
}
 
void Scheduler::MarkConflicts(void)
{
    hasconflicts = false;
    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        first->conflicting = false;
    }

    i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        list<ProgramInfo *>::iterator j = i;
        j++;
        for (; j != recordingList.end(); j++)
        {
            ProgramInfo *first = (*i);
            ProgramInfo *second = (*j);

            if (!first->recording || !second->recording)
                continue;
            if (Conflict(first, second))
            {
                first->conflicting = true;
                second->conflicting = true;
                hasconflicts = true;
            }
        }
    }
}

void Scheduler::PruneList(void)
{
    list<ProgramInfo *>::reverse_iterator i = recordingList.rbegin();
    list<ProgramInfo *>::iterator deliter;

    i = recordingList.rbegin();
    for (; i != recordingList.rend(); i++)
    {
        list<ProgramInfo *>::reverse_iterator j = i;
        j++;
        for (; j != recordingList.rend(); j++)
        {
            ProgramInfo *first = (*i);
            ProgramInfo *second = (*j);

            if (first->title == second->title)
            {
                if (first->subtitle == second->subtitle)
                {
                    if (first->description == second->description)
                    {
                        if (second->conflicting && !first->conflicting)
                        {
                            delete second;
                            deliter = j.base();
                            j++;
                            deliter--;
                            recordingList.erase(deliter);
                        }
                        else
                        {
                            delete first;
                            deliter = i.base();
                            deliter--;
                            recordingList.erase(deliter);
                            break;
                        }
                    }
                }
            }
        }
    }    
}

list<ProgramInfo *> *Scheduler::getConflicting(ProgramInfo *pginfo,
                                               bool removenonplaying)
{
    if (!pginfo->conflicting && removenonplaying)
        return NULL;

    list<ProgramInfo *> *retlist = new list<ProgramInfo *>;

    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *second = (*i);

        if (second->title == pginfo->title && 
            second->startts == pginfo->startts)
            continue;

        if (removenonplaying && (!pginfo->recording || !second->recording))
            continue;
        if (Conflict(pginfo, second))
            retlist->push_back(second);
    }

    return retlist;
}

void Scheduler::CheckOverride(ProgramInfo *info,
                              list<ProgramInfo *> *conflictList)
{
    QSqlQuery query;
    char thequery[512];

    QString starts = info->startts.toString("yyyyMMddhhmm");
    starts += "00";
    QString ends = info->endts.toString("yyyyMMddhhmm");
    ends += "00";

    sprintf(thequery, "SELECT * FROM conflictresolutionoverride WHERE "
                      "channum = %d AND starttime = %s AND "
                      "endtime = %s;", info->channum.toInt(),
                      starts.ascii(), ends.ascii());


    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        
        list<ProgramInfo *>::iterator i = conflictList->begin();
        for (; i != conflictList->end(); i++)
        {
            ProgramInfo *del = (*i);

            del->recording = false;
        }
        info->conflicting = false;
    }
}

void Scheduler::MarkSingleConflict(ProgramInfo *info,
                                   list<ProgramInfo *> *conflictList)
{
    QSqlQuery query;
    char thequery[512];

    list<ProgramInfo *>::iterator i;

    QString starts = info->startts.toString("yyyyMMddhhmm");
    starts += "00";
    QString ends = info->endts.toString("yyyyMMddhhmm");
    ends += "00";
  
    sprintf(thequery, "SELECT * FROM conflictresolutionsingle WHERE "
                      "preferchannum = %d AND preferstarttime = %s AND "
                      "preferendtime = %s;", info->channum.toInt(), 
                      starts.ascii(), ends.ascii());
 
    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString badchannum = query.value(3).toString();
            QDateTime badst = QDateTime::fromString(query.value(4).toString(),
                                                    Qt::ISODate);
            QDateTime badend = QDateTime::fromString(query.value(5).toString(),
                                                     Qt::ISODate);

            i = conflictList->begin();
            for (; i != conflictList->end(); i++)
            {
                ProgramInfo *test = (*i);
                if (test->channum == badchannum && test->startts == badst && 
                    test->endts == badend)
                {
                    test->recording = false;
                }
            }
        }
    }

    bool conflictsleft = false;
    i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        if (test->recording == true)
            conflictsleft = true;
    }

    if (!conflictsleft)
    {
        info->conflicting = false;
        return;
    }

    sprintf(thequery, "SELECT * FROM conflictresolutionany WHERE "
                      "prefertitle = \"%s\";", info->title.ascii());

    query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString badtitle = query.value(1).toString();

            i = conflictList->begin();
            for (; i != conflictList->end(); i++)
            {
                ProgramInfo *test = (*i);
                if (test->title == badtitle)
                {
                    test->recording = false;
                }
            }
        }
    }
    
    conflictsleft = false;
    i = conflictList->begin();
    for (; i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        if (test->recording == true)
            conflictsleft = true;
    }

    if (!conflictsleft)
    {
        info->conflicting = false;
    }
}

void Scheduler::MarkConflictsToRemove(void)
{
    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
    
        if (first->conflicting && first->recording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            CheckOverride(first, conflictList); 
            delete conflictList;
        }
    }

    i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
  
        if (first->conflicting && first->recording)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            MarkSingleConflict(first, conflictList);
            delete conflictList;
        }
        else if (!first->recording)
            first->conflicting = false;
    }
}

void Scheduler::RemoveConflicts(void)
{
    list<ProgramInfo *>::iterator del;
    list<ProgramInfo *>::iterator i = recordingList.begin();
    while (i != recordingList.end())
    {
        ProgramInfo *first = (*i);

        del = i;
        i++;

        if (!first->recording)
        {
            delete first;
            recordingList.erase(del);
        }
    }
}

void Scheduler::GuessSingle(ProgramInfo *info, 
                            list<ProgramInfo *> *conflictList)
{
    int type = info->recordtype;
    ProgramInfo *best = info;
    list<ProgramInfo *>::iterator i;
 
    if (conflictList->size() == 0)
    {
        info->conflicting = false;
        return;
    }

    for (i = conflictList->begin(); i != conflictList->end(); i++)
    {
        ProgramInfo *test = (*i);
        if (test->recordtype < type)
        {
            best = test;
            type = test->recordtype;
            break;
        }
        else if (test->recordtype == type)
        {
            if (test->startts < info->startts)
            {
                best = test;
                break;
            }
            if (test->startts.secsTo(test->endts) > 
                info->startts.secsTo(info->endts))
            {
                best = test;
                break;
            }
            if (test->channum.toInt() < info->channum.toInt())
            {
                best = test;
                break;
            }
        }
    }

    if (best == info)
    {
        for (i = conflictList->begin(); i != conflictList->end(); i++)
        {
            ProgramInfo *pginfo = (*i);
            pginfo->recording = false;
        }
        best->conflicting = false;
    }
    else
    {
        info->recording = false;
    } 
}

void Scheduler::GuessConflicts(void)
{
    list<ProgramInfo *>::iterator i = recordingList.begin();
    for (; i != recordingList.end(); i++)
    {
        ProgramInfo *first = (*i);
        if (first->recording && first->conflicting)
        {
            list<ProgramInfo *> *conflictList = getConflicting(first);
            GuessSingle(first, conflictList);
            delete conflictList;
        }
    }
}
