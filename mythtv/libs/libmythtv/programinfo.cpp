#include "programinfo.h"

ProgramInfo::ProgramInfo(void)
{
    spread = -1;
    startCol = -1;

    chanstr = "";

    recordtype = -1;
    conflicting = false;
    recording = true;

    sourceid = -1;
    inputid = -1;
    cardid = -1;
}   
        
ProgramInfo::ProgramInfo(const ProgramInfo &other)
{           
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;
 
    recordtype = other.recordtype;
    conflicting = other.conflicting;
    recording = other.recording;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
}   

int ProgramInfo::CalculateLength(void)
{
    return startts.secsTo(endts);
}
    
ProgramInfo *GetProgramAtDateTime(QString channel, const QString &ltime)
{
    QSqlQuery query;
    QString thequery;
   
    thequery = QString("SELECT channel.chanid,starttime,endtime,title,subtitle,"
                       "description,category,channel.channum "
                       "FROM program,channel WHERE program.chanid = %1 "
                       "AND starttime < %2 AND endtime > %3 AND "
                       "program.chanid = channel.chanid;").arg(channel)
                       .arg(ltime).arg(ltime);

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
        proginfo->spread = -1;
        proginfo->recordtype = -1;

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

ProgramInfo *GetProgramAtDateTime(QString channel, QDateTime &dtime)
{
    QString sqltime = dtime.toString("yyyyMMddhhmm");
    sqltime += "50"; 

    return GetProgramAtDateTime(channel, sqltime);
}

// 0 for no, 1 for weekdaily, 2 for weekly.
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

    if (nextweek && nextweek->title == title)
    {
        delete nextweek;
        return 2;
    }

    if (nextweek)
        delete nextweek;
    return 0;
}

// returns 0 for no recording, 1 for onetime, 2 for timeslot, 3 for every
int ProgramInfo::GetProgramRecordingStatus(void)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", ends.ascii());

    QString thequery;
    QSqlQuery query;

    thequery = QString("SELECT NULL FROM singlerecord WHERE chanid = %1 AND "
                       "starttime = %2 AND endtime = %3 AND title = \"%4\";")
                       .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        recordtype = 1;
        return 1;
    }

    for (int i = 0; i < 8; i++)
    {
        sqlstarttime[i] = '0';
        sqlendtime[i] = '0';
    }

    thequery = QString("SELECT NULL FROM timeslotrecord WHERE chanid = %1 AND "
                       "starttime = %2 AND endtime = %3 AND title = \"%4\";")
                       .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        recordtype = 2;
        return 2;
    }

    thequery = QString("SELECT NULL FROM allrecord WHERE title = \"%1\";")
                       .arg(title);

    query.exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        recordtype = 3;
        return 3;
    }

    recordtype = 0;
    return 0;
}

// newstate uses same values as return of GetProgramRecordingState
void ProgramInfo::ApplyRecordStateChange(int newstate)
{
    QString starts = startts.toString("yyyyMMddhhmm");
    QString ends = endts.toString("yyyyMMddhhmm");

    char sqlstarttime[128];
    sprintf(sqlstarttime, "%s00", starts.ascii());

    char sqlendtime[128];
    sprintf(sqlendtime, "%s00", ends.ascii());

    QString thequery;
    QSqlQuery query;

    if (recordtype == 1)
    {
        thequery = QString("DELETE FROM singlerecord WHERE "
                           "chanid = %1 AND starttime = %2 AND "
                           "endtime = %3;").arg(chanid).arg(sqlstarttime)
                           .arg(sqlendtime);
        query.exec(thequery);
    }
    else if (recordtype == 2)
    {
        char tempstarttime[512], tempendtime[512];
        strcpy(tempstarttime, sqlstarttime);
        strcpy(tempendtime, sqlendtime);

        for (int i = 0; i < 8; i++)
        {
            tempstarttime[i] = '0';
            tempendtime[i] = '0';
        }

        thequery = QString("DELETE FROM timeslotrecord WHERE chanid = %1 "
                           "AND starttime = %2 AND endtime = %3;")
                           .arg(chanid).arg(tempstarttime).arg(tempendtime);

        query.exec(thequery);
    }
    else if (recordtype == 3)
    {
        thequery = QString("DELETE FROM allrecord WHERE title = \"%1\";")
                          .arg(title);

        query.exec(thequery);
    }

    if (newstate == 1)
    {
        thequery = QString("INSERT INTO singlerecord (chanid,starttime,"
                           "endtime,title,subtitle,description) "
                           "VALUES(%1, %2, %3, \"%4\", \"%5\", \"%6\");")
                           .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                           .arg(title).arg(subtitle).arg(description);
        query.exec(thequery);
    }
    else if (newstate == 2)
    {
        for (int i = 0; i < 8; i++)
        {
            sqlstarttime[i] = '0';
            sqlendtime[i] = '0';
        }

        thequery = QString("INSERT INTO timeslotrecord (chanid,starttime,"
                           "endtime,title) VALUES(%1,%2,%3,\"%4\");")
                           .arg(chanid).arg(sqlstarttime).arg(sqlendtime)
                           .arg(title);
        query.exec(thequery);
    }
    else if (newstate == 3)
    {
        thequery = QString("INSERT INTO allrecord (title) VALUES(\"%1\");")
                           .arg(title);
        query.exec(thequery);
    }

    if (newstate != recordtype)
    {
        thequery = "SELECT NULL FROM settings WHERE value = \"RecordChanged\";";
        query.exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            thequery = "UPDATE settings SET data = \"yes\" WHERE "
                       "value = \"RecordChanged\";";
        }
        else
        {
            thequery = "INSERT settings (value,data) "
                       "VALUES(\"RecordChanged\", \"yes\");";
        }
        query.exec(thequery);
    }

    recordtype = newstate;
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

    QString query;
    query = QString("INSERT INTO recorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(title) 
                    .arg(subtitle).arg(description);

    QSqlQuery qquery = db->exec(query);

    query = QString("INSERT INTO oldrecorded (chanid,starttime,endtime,title,"
                    "subtitle,description) "
                    "VALUES(%1,\"%2\",\"%3\",\"%4\",\"%5\",\"%6\");")
                    .arg(chanid).arg(starts).arg(ends).arg(title) 
                    .arg(subtitle).arg(description);

    qquery = db->exec(query);

    if (recordtype == 1)
    {
        query = QString("DELETE FROM singlerecord WHERE chanid = %1 AND "
                        "starttime = %2 AND endtime = %3;").arg(chanid)
                        .arg(starts).arg(ends);

        qquery = db->exec(query);
    }

}

