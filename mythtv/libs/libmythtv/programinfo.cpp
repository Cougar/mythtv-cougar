#include "programinfo.h"
#include "scheduledrecording.h"
#include "util.h"
#include <iostream>
#include <qsocket.h>

using namespace std;

ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;

    chanstr = "";
    chansign = "";
    channame = "";

    pathname = "";
    filesize = 0;

    conflicting = false;
    recording = true;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
    record = NULL;
}   
        
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    pathname = other.pathname;
    filesize = other.filesize;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;
 
    conflicting = other.conflicting;
    recording = other.recording;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    record = NULL;
}

ProgramInfo::~ProgramInfo() {
    if (record != NULL)
        delete record;
}

void ProgramInfo::ToStringList(QStringList &list)
{
    if (title == "")
        title = " ";
    if (subtitle == "")
        subtitle = " ";
    if (description == "")
        description = " ";
    if (category == "")
        category = " ";
    if (pathname == "")
        pathname = " ";
    if (chanid == "")
        chanid = " ";
    if (chanstr == "")
        chanstr = " ";
    if (pathname == "")
        pathname = " ";

    list << title;
    list << subtitle;
    list << description;
    list << category;
    list << chanid;
    list << chanstr;
    list << chansign;
    list << channame;
    list << pathname;
    encodeLongLong(list, filesize);
    list << startts.toString();
    list << endts.toString();
    list << QString::number(conflicting);
    list << QString::number(recording);
}

void ProgramInfo::FromStringList(QStringList &list, int offset)
{
    if (offset + NUMPROGRAMLINES > (int)list.size())
    {
        cerr << "offset is: " << offset << " but size is " << list.size() 
             << endl;
        return;
    }

    title = list[offset];
    subtitle = list[offset + 1];
    description = list[offset + 2];
    category = list[offset + 3];
    chanid = list[offset + 4];
    chanstr = list[offset + 5];
    chansign = list[offset + 6];
    channame = list[offset + 7];
    pathname = list[offset + 8];
    filesize = decodeLongLong(list, offset + 9);
    startts = QDateTime::fromString(list[offset + 11]);
    endts = QDateTime::fromString(list[offset + 12]);
    conflicting = list[offset + 13].toInt();
    recording = list[offset + 14].toInt();

    if (title == " ")
        title = "";
    if (subtitle == " ")
        subtitle = "";
    if (description == " ")
        description = "";
    if (category == " ")
        category = "";
    if (pathname == " ")
        pathname = "";
    if (chanid == " ")
        chanid = "";
    if (chanstr == " ")
        chanstr = "";
    if (pathname == " ")
        pathname = "";
    if (channame == " ")
        channame = "";
}

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}

void ProgramInfo::GetProgramRangeDateTime(QPtrList<ProgramInfo> *proglist, QString channel, 
                                          const QString &ltime, const QString &rtime)
{
    QSqlQuery query;
    QString thequery;

    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel "
                       "WHERE program.chanid = %1 AND endtime >= %1 AND "
                       "starttime <= %3 AND program.chanid = channel.chanid "
                       "ORDER BY starttime;")
                       .arg(channel).arg(ltime).arg(rtime);
    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;
            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = 
                                   QString::fromUtf8(query.value(5).toString());
            proginfo->category = QString::fromUtf8(query.value(6).toString());
            proginfo->chanstr = query.value(7).toString();
            proginfo->chansign = query.value(8).toString();
            proginfo->channame = query.value(9).toString();
            proginfo->spread = -1;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";
            if (proginfo->category == QString::null)
                proginfo->category = "";

            proglist->append(proginfo);
        }
    }
}    

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, const QString &ltime)
{
    QSqlQuery query;
    QString thequery;
   
    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum,channel.callsign, "
                       "channel.name FROM program,channel WHERE "
                       "program.chanid = %1 AND starttime < %2 AND "
                       "endtime > %3 AND program.chanid = channel.chanid;")
                       .arg(channel).arg(ltime).arg(ltime);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->chanid = query.value(0).toString();
        proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                  Qt::ISODate);
        proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                Qt::ISODate);
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());
        proginfo->category = QString::fromUtf8(query.value(6).toString());
        proginfo->chanstr = query.value(7).toString();
        proginfo->chansign = query.value(8).toString();
        proginfo->channame = query.value(9).toString();
        proginfo->spread = -1;

        if (proginfo->title == QString::null)
            proginfo->title = "";
        if (proginfo->subtitle == QString::null)
            proginfo->subtitle = "";
        if (proginfo->description == QString::null)
            proginfo->description = "";
        if (proginfo->category == QString::null)
            proginfo->category = "";

        return proginfo;
    }

    return NULL;
}

ProgramInfo *ProgramInfo::GetProgramAtDateTime(QString channel, QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "50"; 

    return GetProgramAtDateTime(channel, sqltime);
}

// -1 for no data, 0 for no, 1 for weekdaily, 2 for weekly.
int ProgramInfo::IsProgramRecurring(void)
{
    QDateTime dtime = startts;

    int weekday = dtime.date().dayOfWeek();
    if (weekday < 6)
    {
        // week day    
        int daysadd = 1;
        if (weekday == 5)
            daysadd = 3;

        QDateTime checktime = dtime.addDays(daysadd);

        ProgramInfo *nextday = GetProgramAtDateTime(chanid, checktime);

        if (NULL == nextday)
            return -1;

        if (nextday && nextday->title == title)
        {
            delete nextday;
            return 1;
        }
        if (nextday)
            delete nextday;
    }

    QDateTime checktime = dtime.addDays(7);
    ProgramInfo *nextweek = GetProgramAtDateTime(chanid, checktime);

    if (NULL == nextweek)
        return -1;

    if (nextweek && nextweek->title == title)
    {
        delete nextweek;
        return 2;
    }

    if (nextweek)
        delete nextweek;
    return 0;
}

ScheduledRecording::RecordingType ProgramInfo::GetProgramRecordingStatus()
{
    if (record == NULL) {
        record = new ScheduledRecording();
        record->loadByProgram(QSqlDatabase::database(), *this);
    }

    return record->getRecordingType();
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(ScheduledRecording::RecordingType newstate)
{
    GetProgramRecordingStatus();
    record->setRecordingType(newstate);
    record->save(QSqlDatabase::database());
}

void ProgramInfo::ApplyRecordTimeChange(const QDateTime &newstartts, 
                                        const QDateTime &newendts)
{
    GetProgramRecordingStatus();
    if (record->getRecordingType() != ScheduledRecording::NotRecording) {
        record->setStart(newstartts);
        record->setEnd(newendts);
    }
}


bool ProgramInfo::IsSameProgram(const ProgramInfo& other) const
{
    if (title == other.title &&
        subtitle.length() > 2 &&
        description.length() > 2 &&
        subtitle == other.subtitle &&
        description == other.description)
        return true;
    else
        return false;
}
 
bool ProgramInfo::IsSameTimeslot(const ProgramInfo& other) const
{
    if (chanid == other.chanid &&
        startts == other.startts &&
        endts == other.endts &&
        sourceid == other.sourceid &&
        cardid == other.cardid &&
	inputid == other.inputid)
        return true;
    else
        return false;
}

QString ProgramInfo::GetRecordFilename(const QString &prefix)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString retval = QString("%1/%2_%3_%4.nuv").arg(prefix).arg(chanid)
                             .arg(starts).arg(ends);
    
    return retval;
}               

void ProgramInfo::WriteRecordedToDB(QSqlDatabase *db)
{
    if (!db)
        return;

    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    starts += "00";
    ends += "00";

    QString sqltitle = title;
    QString sqlsubtitle = subtitle;
    QString sqldescription = description;

    sqltitle.replace(QRegExp("\""), QString("\\\""));
    sqlsubtitle.replace(QRegExp("\""), QString("\\\""));
    sqldescription.replace(QRegExp("\""), QString("\\\""));

    QString query;
    query = QString("INSERT INTO recorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle) 
                    .arg(sqlsubtitle).arg(sqldescription);

    QSqlQuery qquery = db->exec(query);
    if (!qquery.isActive())
    {
        cerr << "DB Error: recorded program insertion failed, SQL query "
             << "was:" << endl;
        cerr << query << endl;
        cerr << "Driver error was:" << endl;
        cerr << qquery.lastError().driverText() << endl;
        cerr << "Database error was:" << endl;
        cerr << qquery.lastError().databaseText() << endl;
    }

    query = QString("INSERT INTO oldrecorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(sqltitle) 
                    .arg(sqlsubtitle).arg(sqldescription);

    qquery = db->exec(query);
    if (!qquery.isActive())
    {
        cerr << "DB Error: recorded program insertion failed, SQL query "
             << "was:" << endl;
        cerr << query << endl;
        cerr << "Driver error was:" << endl;
        cerr << qquery.lastError().driverText() << endl;
        cerr << "Database error was:" << endl;
        cerr << qquery.lastError().databaseText() << endl;
    }

    GetProgramRecordingStatus();
    record->doneRecording(QSqlDatabase::database(), *this);
}
