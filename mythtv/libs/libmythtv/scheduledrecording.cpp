#include "scheduledrecording.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "mythcontext.h"
#include "proglist.h"
#include "sr_items.h"
#include "sr_root.h"
#include "sr_dialog.h"
#include "jobqueue.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qregexp.h>

// NOTE: if this changes, you _MUST_ update the RecTypePriority function 
// in recordingtypes.cpp.

ScheduledRecording::ScheduledRecording() 
{
    m_pginfo = NULL;
    type = NULL;
    search = NULL;
    profile = NULL;
    dupin = NULL;
    dupmethod = NULL;
    autoexpire = NULL;
    autotranscode = NULL;
    autocommflag = NULL;
    autouserjob1 = NULL;
    autouserjob2 = NULL;
    autouserjob3 = NULL;
    autouserjob4 = NULL;
    startoffset = NULL;
    endoffset = NULL;
    maxepisodes = NULL;
    maxnewest = NULL;
    m_dialog = NULL;
    recpriority = NULL;
    recgroup = NULL;
    searchType = "";
    searchForWhat = "";
        
    longChannelFormat = gContext->GetSetting("LongChannelFormat", "<num> <name>");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    timeFormat = gContext->GetSetting("TimeFormat", "h:mm AP");
    dateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    shortDateFormat = gContext->GetSetting("ShortDateFormat", "M/d");
    
    addChild(id = new ID());
    
    channel = new SRChannel(*this);
    station = new SRStation(*this);
    title = new SRTitle(*this);
    subtitle = new SRSubtitle(*this);
    description = new SRDescription(*this);
    startTime = new SRStartTime(*this);
    endTime = new SREndTime(*this);
    startDate = new SRStartDate(*this);
    endDate = new SREndDate(*this);
    category = new SRCategory(*this);
    seriesid = new SRSeriesid(*this);
    programid = new SRProgramid(*this);
    search = new SRRecSearchType(*this);
    
    rootGroup = new RootSRGroup(*this);
}


void ScheduledRecording::load(QSqlDatabase *db)
{
    if (getRecordID())
    {
        ConfigurationGroup::load(db);
        
        QString tmpType = type->getValue();
        type->clearSelections();
        if (tmpType.toInt() == kOverrideRecord ||
            tmpType.toInt() == kDontRecord)
            type->addOverrideSelections();
        else
            type->addNormalSelections(!station->getValue().isEmpty());
        
        type->setValue(tmpType);
        type->setUnchanged();
        fetchChannelInfo(db);
    }
}



void ScheduledRecording::loadByProgram(QSqlDatabase* db, ProgramInfo* proginfo) 
{
    m_pginfo = proginfo;
    
    if (proginfo->recordid)
        loadByID(db, proginfo->recordid);
    else
        setDefault(db, true);
        

    
    if (search->intValue() == kNoSearch)
        setProgram(proginfo, db);
}

void ScheduledRecording::loadBySearch(QSqlDatabase *db,
                                      RecSearchType lsearch,
                                      QString forwhat)
{
    int rid = 0;
    QString thequery = QString("SELECT recordid FROM record WHERE "
                               "search = %1 AND description LIKE '%2'")
        .arg(lsearch).arg(forwhat);
    QSqlQuery query = db->exec(thequery);

    if (query.isActive())
    {
        if (query.next())
            rid = query.value(0).toInt();
    }
    else
        MythContext::DBError("loadBySearch", query);

    if (rid)
        loadByID(db, rid);
    else
    {
        setDefault(db, false);
        search->setValue(lsearch);
        searchForWhat = forwhat;
        
        switch (lsearch)
        {
        case kTitleSearch:
            searchType = "(" +  QObject::tr("Title Search") + ")";
            break;
        case kKeywordSearch:
            searchType = "(" +  QObject::tr("Keyword Search") + ")";
            break;
        case kPeopleSearch:
            searchType = "(" +  QObject::tr("People Search") + ")";
            break;
        default:
            searchType = "(" +  QObject::tr("Unknown Search") + ")";
            break;
        }
        QString ltitle = QString("%1 %2").arg(forwhat).arg(searchType);
        title->setValue(ltitle);
        description->setValue(forwhat);
    } 
}

void ScheduledRecording::fetchChannelInfo(QSqlDatabase *db)
{
    
    if (channel->getValue().toInt() > 0)
    {
        QString queryStr(QString("SELECT channum, callsign, name FROM channel "
                                 "WHERE chanid = '%1';").arg(channel->getValue()));
    
        QSqlQuery result = db->exec(queryStr);
        
        if (result.isActive() && result.numRowsAffected() > 0)
        {
            result.next();
            chanstr =  result.value(0).toString();
            chansign =  result.value(1).toString();
            channame =  result.value(2).toString();
        }
    }
    else
    {
        chanstr =  "";
        chansign =  "";
        channame =  "";
    }
}



QString ScheduledRecording::ChannelText(QString format)
{
    format.replace("<num>", chanstr)
          .replace("<sign>", chansign)
          .replace("<name>", channame);

    return format;
}


void ScheduledRecording::ToMap(QMap<QString, QString>& progMap)
{
    QString searchtitle = "";

    if (m_pginfo && search->intValue() != kNoSearch)
    {
        searchtitle = title->getValue();
        m_pginfo->ToMap(NULL, progMap);
    }
    else
    {
        progMap["title"] = title->getValue();
        progMap["subtitle"] = subtitle->getValue();
        progMap["description"] = description->getValue();
        
        progMap["category"] = category->getValue();
        progMap["callsign"] = station->getValue();
        
        progMap["starttime"] = startTime->getValue();
        progMap["startdate"] = startDate->getValue();
        progMap["endtime"] = endTime->getValue();
        progMap["enddate"] = endTime->getValue();
        
        
        if (chanstr.isEmpty())
        {
            progMap["channum"] = QObject::tr("Any");
            progMap["longchannel"] = QObject::tr("Any");
            
        }
        else
        {
            progMap["channum"] = chanstr;
            progMap["longchannel"] = ChannelText(longChannelFormat);
        }
        
        progMap["chanid"] = channel->getValue();
        progMap["channel"] = station->getValue();
        
        QDateTime startts(startDate->dateValue(), startTime->timeValue());
        QDateTime endts(endDate->dateValue(), endTime->timeValue());
        
        QString length;
        int hours, minutes, seconds;
        seconds = startts.secsTo(endts);
        
        minutes = seconds / 60;
        progMap["lenmins"] = QString("%1 %2").arg(minutes).arg(QObject::tr("minutes"));
        hours   = minutes / 60;
        minutes = minutes % 60;
        length.sprintf("%d:%02d", hours, minutes);
        
        progMap["lentime"] = length;
        
        progMap["timedate"] = startts.date().toString(dateFormat) + ", " +
                              startts.time().toString(timeFormat) + " - " +
                              endts.time().toString(timeFormat);

        progMap["shorttimedate"] = startts.date().toString(shortDateFormat) + ", " +
                                   startts.time().toString(timeFormat) + " - " +
                                   endts.time().toString(timeFormat);
    }

    progMap["searchtype"] = searchType;
    progMap["searchforwhat"] = searchForWhat;

        
    if (searchtitle != "")
    {
        progMap["banner"] = searchtitle;

        if (progMap["subtitle"] == "")
            progMap["episode"] = progMap["title"];
        else
            progMap["episode"] = QString("%1 - \"%2\"")
                                        .arg(progMap["title"])
                                        .arg(progMap["subtitle"]);
    }
    else
    {
        progMap["banner"] = progMap["title"];
        progMap["episode"] = progMap["subtitle"];
    }
}


void ScheduledRecording::loadByID(QSqlDatabase* db, int recordID) {
    id->setValue(recordID);
    load(db);
}

RecordingType ScheduledRecording::getRecordingType(void) const {
    return (RecordingType)(type->getValue().toInt());
}

void ScheduledRecording::setRecordingType(RecordingType newType) {
    type->setValue(QString::number(newType));
}

RecSearchType ScheduledRecording::getSearchType(void) const {
    return (RecSearchType)(search->intValue());
}

bool ScheduledRecording::GetAutoExpire(void) const {
    return(autoexpire->getValue().toInt());
}

void ScheduledRecording::SetAutoExpire(bool expire) {
    autoexpire->setValue(expire);
}

int ScheduledRecording::GetAutoRunJobs(void) const {
    int result = 0;

    if (autotranscode->getValue().toInt())
        result |= JOB_TRANSCODE;
    if (autocommflag->getValue().toInt())
        result |= JOB_COMMFLAG;
    if (autouserjob1->getValue().toInt())
        result |= JOB_USERJOB1;
    if (autouserjob2->getValue().toInt())
        result |= JOB_USERJOB2;
    if (autouserjob3->getValue().toInt())
        result |= JOB_USERJOB3;
    if (autouserjob4->getValue().toInt())
        result |= JOB_USERJOB4;

    return result;
}

int ScheduledRecording::GetMaxEpisodes(void) const {
    return(maxepisodes->getValue().toInt());
}

bool ScheduledRecording::GetMaxNewest(void) const {
    return(maxnewest->getValue().toInt());
}

void ScheduledRecording::save(QSqlDatabase* db) 
{
    if (type->isChanged() && getRecordingType() == kNotRecording)
    {
        remove(db);
    }
    else
    {
        ConfigurationGroup::save(db);
    }
    signalChange(db);
}

void ScheduledRecording::remove(QSqlDatabase* db) {
    if (!getRecordID())
        return;

    QString query = QString("DELETE FROM record WHERE recordid = %1")
                            .arg(getRecordID());
    db->exec(query);
    query = QString("UPDATE recordid FROM recorded SET recordid = NULL "
                    "WHERE recordid = %1")
                    .arg(getRecordID());
}

void ScheduledRecording::signalChange(QSqlDatabase* db) {
    MythContext::KickDatabase(db);

    QSqlQuery result = db->exec("SELECT NULL FROM settings WHERE value = \"RecordChanged\";");

    QString query;
    if (result.isActive() && result.numRowsAffected() > 0)
    {
        query = "UPDATE settings SET data = \"yes\" WHERE "
                "value = \"RecordChanged\";";
    }
    else
    {
        query = "INSERT INTO settings (value,data) "
                "VALUES(\"RecordChanged\", \"yes\");";
    }
    db->exec(query);

}

bool ScheduledRecording::hasChanged(QSqlDatabase* db) {
    QString thequery;
    
    bool retval = false;

    thequery = "SELECT data FROM settings WHERE value = \"RecordChanged\";";
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString value = query.value(0).toString();
        if (value == "yes")
        {
            thequery = "UPDATE settings SET data = \"no\" WHERE value = "
                       "\"RecordChanged\";";
            query = db->exec(thequery);

            retval = true;
        }
    }

    return retval;
}

void ScheduledRecording::doneRecording(QSqlDatabase* db, 
                                       const ProgramInfo& proginfo) 
{
    if (getRecordingType() == kFindOneRecord)
        remove(db);

    QString msg;
  
    msg = QString("Finished recording %1 on channel: %2")
                  .arg(proginfo.title)
                  .arg(proginfo.chanid);

    VERBOSE(VB_GENERAL, msg.local8Bit());
    gContext->LogEntry("scheduler", LP_NOTICE, "Finished recording", 
                       msg.utf8());

    addHistory(db, proginfo);
}

void ScheduledRecording::addHistory(QSqlDatabase* db, 
                                    const ProgramInfo& proginfo) 
{
    QSqlQuery result(QString::null, db);

    result.prepare("INSERT INTO oldrecorded (chanid,starttime,"
                   "endtime,title,subtitle,description,category,"
                   "seriesid,programid) "
                   "VALUES(:CHANID,:START,:END,:TITLE,:SUBTITLE,:DESC,"
                   ":CATEGORY,:SERIESID,:PROGRAMID);");
    result.bindValue(":CHANID", proginfo.chanid);
    result.bindValue(":START", proginfo.recstartts.toString(Qt::ISODate));
    result.bindValue(":END", proginfo.recendts.toString(Qt::ISODate));
    result.bindValue(":TITLE", proginfo.title.utf8());
    result.bindValue(":SUBTITLE", proginfo.subtitle.utf8());
    result.bindValue(":DESC", proginfo.description.utf8());
    result.bindValue(":CATEGORY", proginfo.category.utf8());
    result.bindValue(":SERIESID", proginfo.seriesid);
    result.bindValue(":PROGRAMID", proginfo.programid);

    result.exec();

    if (!result.isActive())
    {
        MythContext::DBError("addHistory", result);
    }
    else
    {
        // The addition of an entry to oldrecorded may affect near-future
        // scheduling decisions, so recalculate
        signalChange(db);
    }
}

void ScheduledRecording::forgetHistory(QSqlDatabase* db,
                                       const ProgramInfo& proginfo)
{
    QSqlQuery result(QString::null, db);

    result.prepare("DELETE FROM oldrecorded WHERE title = :TITLE AND "
                   "((subtitle = :SUBTITLE AND description = :DESC) OR "
                   "(programid <> '' AND programid = :PROGRAMID))");
    result.bindValue(":TITLE", proginfo.title.utf8());
    result.bindValue(":SUBTITLE", proginfo.subtitle.utf8());
    result.bindValue(":DESC", proginfo.description.utf8());
    result.bindValue(":PROGRAMID", proginfo.programid);
    
    result.exec();
    if (!result.isActive())
    {
        MythContext::DBError("forgetHistory", result);
    }
    else
    {
        // The removal of an entry from oldrecorded may affect near-future
        // scheduling decisions, so recalculate
        signalChange(db);
    }
}



void ScheduledRecording::runProgList(void)
{
    ProgLister *pl = NULL;

    if (!m_pginfo)
    {
        switch (search->intValue())
        {
        case kTitleSearch:
            pl = new ProgLister(plTitleSearch, description->getValue(),
                                QSqlDatabase::database(),
                                gContext->GetMainWindow(), "proglist");
            break;
        case kKeywordSearch:
            pl = new ProgLister(plKeywordSearch, description->getValue(),
                                QSqlDatabase::database(),
                                gContext->GetMainWindow(), "proglist");
            break;
        case kPeopleSearch:
            pl = new ProgLister(plPeopleSearch, description->getValue(),
                                QSqlDatabase::database(),
                                gContext->GetMainWindow(), "proglist");
            break;
        default:
            pl = new ProgLister(plTitle, title->getValue(),
                                QSqlDatabase::database(),
                                gContext->GetMainWindow(), "proglist");
            break;
        }
    }
    else
        pl = new ProgLister(plTitle, title->getValue(),
                            QSqlDatabase::database(),
                            gContext->GetMainWindow(), "proglist");

    pl->exec();
    delete pl;
    //proglistButton->setFocus();
}

void ScheduledRecording::runShowDetails(void)
{
    if (m_pginfo)
        m_pginfo->showDetails(QSqlDatabase::database());
}

void ScheduledRecording::fillSelections(QSqlDatabase* db, SelectSetting* setting) {
    QSqlQuery result = db->exec("SELECT recordid FROM record");
    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) {
            int id = result.value(0).toInt();

            ScheduledRecording sr;
            sr.loadByID(db, id);

            QString label;
            QString weekly = "";

            switch (sr.getRecordingType()) {
            case kAllRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kFindOneRecord:
                label = QString("%1").arg(sr.title->getValue());
                break;
            case kChannelRecord:
                label = QString("%1 on channel %2")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel());
                break;
            case kWeekslotRecord:
                weekly = QDate(sr.startDate->dateValue()).toString("dddd")+"s ";
            case kTimeslotRecord:
                label = QString("%1 on channel %2 (%3%4 - %5)")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel())
                    .arg(weekly)
                    .arg(sr.startTime->timeValue().toString())
                    .arg(sr.endTime->timeValue().toString());
                break;
            case kSingleRecord:
            case kOverrideRecord:
            case kDontRecord:
                label = QString("%1 on channel %2 (%3 %4 - %5)")
                    .arg(sr.title->getValue())
                    .arg(sr.channel->getSelectionLabel())
                    .arg(sr.startDate->dateValue().toString())
                    .arg(sr.startTime->timeValue().toString())
                    .arg(sr.endTime->timeValue().toString());
                break;
            default:
                label = "You should not see this";
            }

            setting->addSelection(label, QString::number(id));
        }
}

void ScheduledRecordingEditor::load(QSqlDatabase* db) {
    clearSelections();
    ScheduledRecording::fillSelections(db, this);
}


int ScheduledRecordingEditor::exec(QSqlDatabase* db) {
    while (ConfigurationDialog::exec(db) == QDialog::Accepted)
        open(getValue().toInt());

    return QDialog::Rejected;
}

void ScheduledRecordingEditor::open(int id) {
    ScheduledRecording* sr = new ScheduledRecording();

    if (id != 0)
        sr->loadByID(db,id);

    if (sr->exec(db) == QDialog::Accepted)
        sr->save(db);
    delete sr;
}

void ScheduledRecording::setStart(const QDateTime& start) {
    startTime->setValue(start.time());
    startDate->setValue(start.date());
}

void ScheduledRecording::setEnd(const QDateTime& end) {
    endTime->setValue(end.time());
    endDate->setValue(end.date());
}

void ScheduledRecording::setRecPriority(int newrecpriority) {
    recpriority->setValue(newrecpriority);
}

void ScheduledRecording::setRecGroup(const QString& newrecgroup) {
    recgroup->setValue(newrecgroup);
}

QString ScheduledRecording::getProfileName(void) const {
    return profile->getValue();
}



MythDialog* ScheduledRecording::dialogWidget(MythMainWindow *parent, const char *name)
{

    MythDialog* dlg = new RecOptDialog(this, parent, name);
    rootGroup->setDialog(dlg);
    return dlg;
}



void ScheduledRecording::setDefault(QSqlDatabase *db, bool haschannel)
{
    id->setValue(0);
    title->setValue("");
    subtitle->setValue("");
    description->setValue("");
    channel->setValue("");
    station->setValue("");
    QDate date = QDate::currentDate();
    QTime time = QTime::currentTime();
    startDate->setValue(date);
    startTime->setValue(time);
    endDate->setValue(date);
    endTime->setValue(time);
    seriesid->setValue("");
    programid->setValue("");
    category->setValue("");
    search->setValue(kNoSearch);

     
    
    if (!type)   
    {
        cerr << "No type object" << endl;
        return;
    }
    
    type->clearSelections();
    type->addNormalSelections(haschannel);
    type->setValue(kNotRecording);
    
    profile->fillSelections(db);
    profile->setValue(QObject::tr("Default"));
    
    dupin->setValue(kDupsInAll);
    dupmethod->setValue(kDupCheckSubDesc);
    maxepisodes->setValue(0);
    startoffset->setValue(0);
    endoffset->setValue(0);   
    maxnewest->setValue(0);
    recpriority->setValue(0);
    
    autoexpire->setValue(gContext->GetNumSetting("AutoExpireDefault", 0));

    autotranscode->setValue(0);
    autocommflag->setValue(gContext->GetNumSetting("AutoCommercialFlag", 1));

    autouserjob1->setValue(0);
    autouserjob2->setValue(0);
    autouserjob3->setValue(0);
    autouserjob4->setValue(0);

    recgroup->fillSelections(db);    
    recgroup->setValue("Default");
}

void ScheduledRecording::setProgram(ProgramInfo *proginfo, QSqlDatabase* db)
{
    m_pginfo = proginfo;
    if (proginfo)
    {
        title->setValue(proginfo->title);
        subtitle->setValue(proginfo->subtitle);
        description->setValue(proginfo->description);
        channel->setValue(proginfo->chanid);
        station->setValue(proginfo->chansign);
        startDate->setValue(proginfo->startts.date());
        startTime->setValue(proginfo->startts.time());
        endDate->setValue(proginfo->endts.date());
        endTime->setValue(proginfo->endts.time());
        seriesid->setValue(proginfo->seriesid);
        programid->setValue(proginfo->programid);
        category->setValue(proginfo->category);
        
        if (db)
            fetchChannelInfo(db);
    }
}

void ScheduledRecording::makeOverride(void)
{
    if (type->getValue().toInt() == kOverrideRecord ||
        type->getValue().toInt() == kDontRecord)
        return;

    id->setValue(0);
    type->clearSelections();
    type->addOverrideSelections();
    search->setValue(kNoSearch);

    setProgram(m_pginfo);

    profile->setChanged();
    dupin->setChanged();
    dupmethod->setChanged();
    autoexpire->setChanged();
    autotranscode->setChanged();
    autocommflag->setChanged();
    autouserjob1->setChanged();
    autouserjob2->setChanged();
    autouserjob3->setChanged();
    autouserjob4->setChanged();
    maxepisodes->setChanged();
    maxnewest->setChanged();
    startoffset->setChanged();
    endoffset->setChanged();
    recpriority->setChanged();
    recgroup->setChanged();
}




