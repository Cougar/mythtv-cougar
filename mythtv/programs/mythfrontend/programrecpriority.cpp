#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qheader.h>

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

#include "programrecpriority.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "proglist.h"
#include "tv.h"

#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

// overloaded version of ProgramInfo with additional recording priority
// values so we can keep everything together and don't
// have to hit the db mulitiple times
ProgramRecPriorityInfo::ProgramRecPriorityInfo(void) : ProgramInfo()
{
    channelRecPriority = 0;
    recTypeRecPriority = 0;
    recType = kNotRecording;
}

ProgramRecPriorityInfo::ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other) 
                      : ProgramInfo::ProgramInfo(other)
{
    channelRecPriority = other.channelRecPriority;
    recTypeRecPriority = other.recTypeRecPriority;
    recType = other.recType;
}

ProgramRecPriorityInfo& ProgramRecPriorityInfo::operator=(const ProgramInfo &other)
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
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;

    recstatus = other.recstatus;
    recordid = other.recordid;
    rectype = other.rectype;
    dupin = other.dupin;
    dupmethod = other.dupmethod;
    recgroup = other.recgroup;
    chancommfree = other.chancommfree;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;

    seriesid = other.seriesid;
    programid = other.programid;

    return(*this);
}

ProgramRecPriority::ProgramRecPriority(QSqlDatabase *ldb, MythMainWindow *parent, 
                             const char *name)
            : MythDialog(parent, name)
{
    db = ldb;

    curitem = NULL;
    bgTransBackup = NULL;
    pageDowner = false;

    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    longChannelFormat = gContext->GetSetting("LongChannelFormat", "<num> <name>");

    listCount = 0;
    dataCount = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "recpriorityprograms"))
    {
        DialogBox diag(gContext->GetMainWindow(), tr("The theme you are using "
                       "does not contain a 'recpriorityprograms' element.  "
                       "Please contact the theme creator and ask if they could "
                       "please update it.<br><br>The next screen will be empty."
                       "  Escape out of it to return to the menu."));
        diag.AddButton(tr("OK"));
        diag.exec();

        return;
    }

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("recprioritylist");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythFrontEnd: ProgramRecPriority - Failed to get selector object.\n";
        exit(27);
    }

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    MythContext::KickDatabase(db);
    FillList();
    sortType = (SortType)gContext->GetNumSetting("ProgramRecPrioritySorting", 
                                                 (int)byTitle);
    reverseSort = gContext->GetNumSetting("ProgramRecPriorityReverse", 0);

    SortList(); 
    inList = inData = 0;
    setNoErase();

    gContext->addListener(this);
}

ProgramRecPriority::~ProgramRecPriority()
{
    gContext->removeListener(this);
    delete theme;
    if (bgTransBackup)
        delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void ProgramRecPriority::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "RIGHT")
                changeRecPriority(1);
            else if (action == "LEFT")
                changeRecPriority(-1);
            else if ((action == "PAUSE") || (action == "PLAYBACK"))
                deactivate();
            else if (action == "ESCAPE")
            {
                saveRecPriority();
                gContext->SaveSetting("ProgramRecPrioritySorting",
                                      (int)sortType);
                gContext->SaveSetting("ProgramRecPriorityReverse",
                                      (int)reverseSort);
                done(MythDialog::Accepted);
            }
            else if (action == "1")
            {
                if (sortType != byTitle)
                {
                    sortType = byTitle;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "2")
            {
                if (sortType != byRecPriority)
                {
                    sortType = byRecPriority;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "4")
            {
                if (sortType != byRecType)
                {
                    sortType = byRecType;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "PREVVIEW" || action == "NEXTVIEW")
            {
                reverseSort = false;
                if (sortType == byTitle)
                    sortType = byRecPriority;
                else if (sortType == byRecPriority)
                    sortType = byRecType;
                else
                    sortType = byTitle;
                SortList();
                update(fullRect);
            }
            else if (action == "SELECT" || action == "MENU" ||
                     action == "INFO")
            {
                saveRecPriority();
                edit();
            }
            else if (action == "UPCOMING")
            {
                saveRecPriority();
                upcoming();
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void ProgramRecPriority::LoadWindow(QDomElement &element)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(28);
            }
        }
    }
}

void ProgramRecPriority::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "program_info")
        infoRect = area;
}

void ProgramRecPriority::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void ProgramRecPriority::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);
 
    if (r.intersects(listRect))
    {
        updateList(&p);
    }
    if (r.intersects(infoRect))
    {
        updateInfo(&p);
    }
}

void ProgramRecPriority::cursorDown(bool page)
{
    if (page == false)
    {
        if (inList > (int)((int)(listsize / 2) - 1)
            && ((int)(inData + listsize) <= (int)(dataCount - 1))
            && pageDowner == true)
        {
            inData++;
            inList = (int)(listsize / 2);
        }
        else
        {
            inList++;

            if (inList >= listCount)
                inList = listCount - 1;
        }
    }
    else if (page == true && pageDowner == true)
    {
        if (inList >= (int)(listsize / 2) || inData != 0)
        {
            inData = inData + listsize;
        }
        else if (inList < (int)(listsize / 2) && inData == 0)
        {
            inData = (int)(listsize / 2) + inList;
            inList = (int)(listsize / 2);
        }
    }
    else if (page == true && pageDowner == false)
    {
        inList = listsize - 1;
    }

    if ((int)(inData + inList) >= (int)(dataCount))
    {
        inData = dataCount - listsize;
        inList = listsize - 1;
    }
    else if ((int)(inData + listsize) >= (int)dataCount)
    {
        inData = dataCount - listsize;
    }

    if (inData < 0)
        inData = 0;

    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void ProgramRecPriority::cursorUp(bool page)
{
    if (page == false)
    {
        if (inList < ((int)(listsize / 2) + 1) && inData > 0)
        {
            inList = (int)(listsize / 2);
            inData--;
            if (inData < 0)
            {
                inData = 0;
                inList--;
            }
         }
         else
         {
             inList--;
         }
     }
     else if (page == true && inData > 0)
     {
         inData = inData - listsize;
         if (inData < 0)
         {
             inList = inList + inData;
             inData = 0;
             if (inList < 0)
                 inList = 0;
         }

         if (inList > (int)(listsize / 2))
         {
             inList = (int)(listsize / 2);
             inData = inData + (int)(listsize / 2) - 1;
         }
     }
     else if (page == true)
     {
         inData = 0;
         inList = 0;
     }

     if (inList > -1)
     {
         update(fullRect);
     }
     else
         inList = 0;
}

void ProgramRecPriority::edit(void)
{
    if (!curitem)
        return;

    ProgramRecPriorityInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if (rec)
    {
        int recid = 0;
        ScheduledRecording record;
        record.loadByID(db, rec->recordid);
        if (record.getSearchType() == kNoSearch)
            record.loadByProgram(db, rec);
        record.exec(db);
        recid = record.getRecordID();

        // We need to refetch the recording priority values since the Advanced
        // Recording Options page could've been used to change them 
        QString thequery;

        if (!recid)
            recid = rec->getRecordID(db);

        thequery = QString(
                   "SELECT recpriority, type, inactive FROM record WHERE recordid = %1;")
                           .arg(recid);
        QSqlQuery query = db->exec(thequery);

        if (query.isActive())
            if (query.numRowsAffected() > 0)
            {
                query.next();
                int recPriority = query.value(0).toInt();
                int rectype = query.value(1).toInt();
                int inactive = query.value(2).toInt();

                int cnt;
                QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                ProgramRecPriorityInfo *progInfo;

                // iterate through programData till we hit the line where
                // the cursor currently is
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                progInfo = &(it.data());
           
                int rtRecPriors[10];
                rtRecPriors[0] = gContext->GetNumSetting("SingleRecordRecPrior", 0);
                rtRecPriors[1] = gContext->GetNumSetting("TimeslotRecordRecPrior", 0);
                rtRecPriors[2] = gContext->GetNumSetting("ChannelRecordRecPrior", 0);
                rtRecPriors[3] = gContext->GetNumSetting("AllRecordRecPrior", 0);
                rtRecPriors[4] = gContext->GetNumSetting("WeekslotRecordRecPrior", 0);
                rtRecPriors[5] = gContext->GetNumSetting("FindOneRecordRecPrior", 0);
                rtRecPriors[6] = gContext->GetNumSetting("OverrideRecordRecPrior", 0);
                rtRecPriors[7] = gContext->GetNumSetting("OverrideRecordRecPrior", 0);
                rtRecPriors[8] = gContext->GetNumSetting("FindOneRecordRecPrior", 0);
                rtRecPriors[9] = gContext->GetNumSetting("FindOneRecordRecPrior", 0);

                // set the recording priorities of that program 
                progInfo->recpriority = recPriority;
                progInfo->recType = (RecordingType)rectype;
                progInfo->recTypeRecPriority = rtRecPriors[progInfo->recType-1];
                // also set the origRecPriorityData with new recording 
                // priority so we don't save to db again when we exit
                QString key = progInfo->MakeUniqueKey(); 
                origRecPriorityData[key] = progInfo->recpriority;

                // also set the active/inactive state
                progInfo->recstatus = inactive ? rsInactive : rsWillRecord;

                SortList();
            }
            else
            {
                // empty query means this recordid no longer exists
                // in record so it was deleted
                // remove it from programData
                int cnt;
                QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                programData.remove(it);
                SortList();
                delete curitem;
                curitem = NULL;
                dataCount--;

                if (cnt >= dataCount)
                    cnt = dataCount - 1;
                if (dataCount <= listsize || cnt <= listsize / 2)
                    inData = 0;
                else if (cnt >= dataCount - listsize + listsize / 2)
                    inData = dataCount - listsize;
                else
                    inData = cnt - listsize / 2;
                inList = cnt - inData;
            }
        else
            MythContext::DBError("Get new recording priority query", query);

        update(fullRect);
    }
}

void ProgramRecPriority::deactivate(void)
{
    if (!curitem)
        return;

    ProgramRecPriorityInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if (rec)
    {
        QString thequery;

        thequery = QString("SELECT inactive FROM record WHERE recordid = %1")
                           .arg(rec->recordid);

        QSqlQuery query = db->exec(thequery);

        int inactive = 0;
        if (query.isActive())
            if (query.numRowsAffected() > 0)
            {
                query.next();
                inactive = query.value(0).toInt();
                if (inactive)
                    inactive = 0;
                else
                    inactive = 1;

                QString theupdatequery;
                theupdatequery = QString("UPDATE record SET inactive = %1 WHERE recordid = %2")
                                         .arg(inactive).arg(rec->recordid);

                QSqlQuery uquery = db->exec(theupdatequery);

                if (uquery.isActive())
                {
                    ScheduledRecording::signalChange(0);
                    int cnt;
                    QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                    ProgramRecPriorityInfo *progInfo;

                    // iterate through programData till we hit the line where
                    // the cursor currently is
                    for (cnt = 0, it = programData.begin(); cnt < inList+inData;
                         cnt++, ++it);
                    progInfo = &(it.data());
                    progInfo->recstatus = inactive ? rsInactive : rsWillRecord;
                } else
                    MythContext::DBError("Update recording schedule inactive query", uquery);
            }

        QPainter p(this);
        updateInfo(&p);
        update(fullRect);
    }
}

void ProgramRecPriority::upcoming(void)
{
    if (!curitem)
        return;

    ScheduledRecording record;

    record.loadByID(db, curitem->recordid);
    record.runProgList();
}

void ProgramRecPriority::changeRecPriority(int howMuch) 
{
    int tempRecPriority, cnt;
    QPainter p(this);
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;
    ProgramRecPriorityInfo *progInfo;
 
    // iterate through programData till we hit the line where
    // the cursor currently is
    for (cnt = 0, it = programData.begin(); cnt < inList+inData; cnt++, ++it);
    progInfo = &(it.data());

    // inc/dec recording priority
    tempRecPriority = progInfo->recpriority + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100) 
    {
        progInfo->recpriority = tempRecPriority;

        // order may change if sorting by recording priority, so resort
        if (sortType == byRecPriority)
            SortList();
        updateList(&p);
        updateInfo(&p);
    }
}

void ProgramRecPriority::saveRecPriority(void) 
{
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;

    for (it = programData.begin(); it != programData.end(); ++it) 
    {
        ProgramRecPriorityInfo *progInfo = &(it.data());
        QString key = progInfo->MakeUniqueKey(); 

        // if this program's recording priority changed from when we entered
        // save new value out to db
        if (progInfo->recpriority != origRecPriorityData[key])
            progInfo->ApplyRecordRecPriorityChange(db, progInfo->recpriority);
    }
}

void ProgramRecPriority::FillList(void)
{
    int cnt = 999, rtRecPriors[10];
    vector<ProgramInfo *> recordinglist;

    programData.clear();

    RemoteGetAllScheduledRecordings(recordinglist);

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        programData[QString::number(cnt)] = *(*pgiter);

        // save recording priority value in map so we don't have to 
        // save all program's recording priority values when we exit
        QString key = (*pgiter)->MakeUniqueKey();
        origRecPriorityData[key] = (*pgiter)->recpriority;

        delete (*pgiter);
        cnt--;
        dataCount++;
    }

//    cerr << "RemoteGetAllScheduledRecordings() returned " << programData.size();
//    cerr << " programs" << endl;

    // get all the recording type recording priority values
    rtRecPriors[0] = gContext->GetNumSetting("SingleRecordRecPriority", 0);
    rtRecPriors[1] = gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
    rtRecPriors[2] = gContext->GetNumSetting("ChannelRecordRecPriority", 0);
    rtRecPriors[3] = gContext->GetNumSetting("AllRecordRecPriority", 0);
    rtRecPriors[4] = gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
    rtRecPriors[5] = gContext->GetNumSetting("FindOneRecordRecPriority", 0);
    rtRecPriors[6] = gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[7] = gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[8] = gContext->GetNumSetting("FindOneRecordRecPriority", 0);
    rtRecPriors[9] = gContext->GetNumSetting("FindOneRecordRecPriority", 0);
    
    // get channel recording priorities and recording types associated with each
    // program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)
    QString query = QString("SELECT recordid, record.title, record.chanid, "
                            "record.starttime, record.startdate, "
                            "record.type, channel.recpriority,  "
                            "record.inactive "
                            "FROM record "
                            "LEFT JOIN channel ON "
                            "(record.chanid = channel.chanid);");

    QSqlQuery result = db->exec(query);
   
    int matches = 0;

    if (result.isActive() && result.numRowsAffected() > 0)
    {
        while (result.next()) 
        {
            int recordid = result.value(0).toInt();
            QString title = QString::fromUtf8(result.value(1).toString());
            QString chanid = result.value(2).toString();
            QString tempTime = result.value(3).toString();
            QString tempDate = result.value(4).toString();
            RecordingType recType = (RecordingType)result.value(5).toInt();
            int channelRecPriority = result.value(6).toInt();
            int recTypeRecPriority = rtRecPriors[recType-1];
            int inactive = result.value(7).toInt();

            if (recType == kAllRecord || recType == kFindOneRecord ||
                recType == kFindDailyRecord || recType == kFindWeeklyRecord)
                channelRecPriority = 0;

            // find matching program in programData and set
            // channelRecPriority, recTypeRecPriority and recType
            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                ProgramRecPriorityInfo *progInfo = &(it.data());

                if (progInfo->recordid == recordid)
                {
                    progInfo->sortTitle = progInfo->title;
                    progInfo->sortTitle.remove(QRegExp(tr("^(The |A |An )")));

                    progInfo->channelRecPriority = channelRecPriority;
                    progInfo->recTypeRecPriority = recTypeRecPriority;
                    progInfo->recType = recType;
                    progInfo->recstatus = inactive ? rsInactive : rsWillRecord;
                    matches++;
                    break;
                }
            }
        }
    }
    else
        MythContext::DBError("Get program recording priorities query", query);

    recMatch.clear();
    ProgramList schedList;
    schedList.FromScheduler();
    QDateTime now = QDateTime::currentDateTime();

    ProgramInfo *s;
    for (s = schedList.first(); s; s = schedList.next())
    {
        if (s->recendts > now)
        {
            listMatch[s->recordid]++;
            if (s->recstatus == rsWillRecord || s->recstatus == rsRecording)
                recMatch[s->recordid]++;
        }
    }
}

typedef struct RecPriorityInfo 
{
    ProgramRecPriorityInfo *prog;
    int cnt;
};

class titleSort 
{
    public:
        titleSort(bool reverseSort = false) {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            if (m_reverse)
                return (a.prog->sortTitle < b.prog->sortTitle);
            else
                return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programRecPrioritySort 
{
    public:
        programRecPrioritySort(bool reverseSort = false)
                               {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int finalA = a.prog->recpriority + 
                         a.prog->channelRecPriority +
                         a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + 
                         b.prog->channelRecPriority +
                         b.prog->recTypeRecPriority;

            if (finalA == finalB)
            {
                int typeA = RecTypePriority(a.prog->recType);
                int typeB = RecTypePriority(b.prog->recType);
                if (typeA == typeB)
                    if (m_reverse)
                        return (a.prog->sortTitle < b.prog->sortTitle);
                    else
                        return (a.prog->sortTitle > b.prog->sortTitle);

                if (m_reverse)
                    return (typeA < typeB);
                else
                    return (typeA > typeB);
            }
            if (m_reverse)
                return (finalA > finalB);
            else
                return (finalA < finalB);
        }

    private:
        bool m_reverse;
};

class programRecTypeSort 
{
    public:
        programRecTypeSort(bool reverseSort = false)
                               {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA == typeB)
                if (m_reverse)
                    return (a.prog->sortTitle < b.prog->sortTitle);
                else
                    return (a.prog->sortTitle > b.prog->sortTitle);

            if (m_reverse)
                return (typeA < typeB);
            else
                return (typeA > typeB);
        }

    private:
        bool m_reverse;
};

void ProgramRecPriority::SortList() 
{
    int i, j;
    bool cursorChanged = false;
    vector<RecPriorityInfo> sortedList;
    QMap<QString, ProgramRecPriorityInfo>::Iterator pit;
    vector<RecPriorityInfo>::iterator sit;
    ProgramRecPriorityInfo *progInfo;
    RecPriorityInfo *recPriorityInfo;
    QMap<QString, ProgramRecPriorityInfo> pdCopy;

    // copy programData into sortedList and make a copy
    // of programData in pdCopy
    for (i = 0, pit = programData.begin(); pit != programData.end(); ++pit, i++)
    {
        progInfo = &(pit.data());
        RecPriorityInfo tmp = {progInfo, i};
        sortedList.push_back(tmp);
        pdCopy[pit.key()] = pit.data();
    }

    // sort sortedList
    switch(sortType) 
    {
        case byTitle :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(),
                          titleSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), titleSort());
                 break;
        case byRecPriority :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecPrioritySort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecPrioritySort());
                 break;
        case byRecType :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecTypeSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecTypeSort());
                 break;
    }

    programData.clear();

    // rebuild programData in sortedList order from pdCopy
    for (i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit)
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in pdCopy 
        for (j = 0,pit = pdCopy.begin(); j != recPriorityInfo->cnt; j++, ++pit);

        progInfo = &(pit.data());

        // put back into programData
        programData[QString::number(999-i)] = pit.data();

        // if recPriorityInfo[i] is the program where the cursor
        // was pre-sort then we need to update to cursor
        // to the ith position
        if (!cursorChanged && recPriorityInfo->cnt == inList+inData) 
        {
            inList = dataCount - i - 1;
            if (inList > (int)((int)(listsize / 2) - 1)) 
            {
                inList = (int)(listsize / 2);
                inData = dataCount - i - 1 - inList;
            }
            else
                inData = 0;

            if (dataCount > listsize && inData > dataCount - listsize) 
            {
                inList += inData - (dataCount - listsize);
                inData = dataCount - listsize;
            }
            cursorChanged = true;
        }
    }
}

void ProgramRecPriority::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("recprioritylist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        ProgramRecPriorityInfo *progInfo = &(it.data());

                        int progRecPriority = progInfo->recpriority;
                        int finalRecPriority = progRecPriority + 
                                        progInfo->channelRecPriority +
                                        progInfo->recTypeRecPriority;
        
                        QString tempSubTitle = progInfo->title;
                        if ((progInfo->rectype == kSingleRecord ||
                             progInfo->rectype == kOverrideRecord ||
                             progInfo->rectype == kDontRecord) &&
                            (progInfo->subtitle).stripWhiteSpace().length() > 0)
                            tempSubTitle = tempSubTitle + " - \"" + 
                                           progInfo->subtitle + "\"";

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ProgramRecPriorityInfo(*progInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, progInfo->RecTypeChar());
                        ltype->SetItemText(cnt, 2, tempSubTitle);

                        if (progRecPriority < 0)
                            ltype->SetItemText(cnt, 3, "-");
                        else
                            ltype->SetItemText(cnt, 3, "+");
                        ltype->SetItemText(cnt, 4, 
                                QString::number(abs(progRecPriority)));

                        if (finalRecPriority < 0)
                            ltype->SetItemText(cnt, 5, "-");
                        else
                            ltype->SetItemText(cnt, 5, "+");

                        ltype->SetItemText(cnt, 6, 
                                QString::number(abs(finalRecPriority)));

                        if (progInfo->recType == kDontRecord ||
                            progInfo->recstatus == rsInactive)
                            ltype->EnableForcedFont(cnt, "inactive");
                        else if (recMatch[progInfo->recordid] > 0)
                            ltype->EnableForcedFont(cnt, "recording");
                        // else if (listMatch[progInfo->recordid] < 1)
                        //    ltype->EnableForcedFont(cnt, "dormant");

                        cnt++;
                        listCount++;
                    }
                    pastSkip--;
                }
                else
                    pageDowner = true;
            }
        }

        ltype->SetDownArrow(pageDowner);
        if (inData > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
    }

    if (programData.count() <= 0)
        container = theme->GetSet("norecordings_list");

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ProgramRecPriority::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (programData.count() > 0 && curitem)
    {  
        int progRecPriority, chanrecpriority, rectyperecpriority, finalRecPriority;
        RecordingType rectype; 

        progRecPriority = curitem->recpriority;
        chanrecpriority = curitem->channelRecPriority;
        rectyperecpriority = curitem->recTypeRecPriority;
        finalRecPriority = progRecPriority + chanrecpriority + rectyperecpriority;

        rectype = curitem->recType;

        QString subtitle = "";
        if (curitem->subtitle != "(null)" &&
            (curitem->rectype == kSingleRecord ||
             curitem->rectype == kOverrideRecord ||
             curitem->rectype == kDontRecord))
        {
            subtitle = curitem->subtitle;
        }

        QString matchInfo;
        if (curitem->recstatus == rsInactive)
            matchInfo = curitem->RecStatusText();
        else
            matchInfo = QString(tr("Recording %1 of %2"))
                                   .arg(recMatch[curitem->recordid])
                                   .arg(listMatch[curitem->recordid]);

        subtitle = QString("(%1) %2").arg(matchInfo).arg(subtitle);

        LayerSet *container = NULL;
        container = theme->GetSet("program_info");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->title);
 
            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(subtitle);

            type = (UITextType *)container->GetType("type");
            if (type) {
                QString text;
                switch (rectype)
                {
                    case kSingleRecord:
                        text = tr("Recording just this showing");
                        break;
                    case kOverrideRecord:
                        text = tr("Recording with override options");
                        break;
                    case kWeekslotRecord:
                        text = tr("Recording every week");
                        break;
                    case kTimeslotRecord:
                        text = tr("Recording in this timeslot");
                        break;
                    case kChannelRecord:
                        text = tr("Recording on this channel");
                        break;
                    case kAllRecord:
                        text = tr("Recording all showings");
                        break;
                    case kFindOneRecord:
                        text = tr("Recording one showing");
                        break;
                    case kFindDailyRecord:
                        text = tr("Recording a showing daily");
                        break;
                    case kFindWeeklyRecord:
                        text = tr("Recording a showing weekly");
                        break;
                    case kDontRecord:
                        text = tr("Not allowed to record this showing");
                        break;
                    case kNotRecording:
                        text = tr("Not recording this showing");
                        break;
                    default:
                        text = tr("Error!");
                        break;
                }
                type->SetText(text);
            }

            type = (UITextType *)container->GetType("typerecpriority");
            if (type) {
                type->SetText(QString::number(abs(rectyperecpriority)));
            }
            type = (UITextType *)container->GetType("typesign");
            if (type) {
                if (rectyperecpriority >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("channel");
            if (type) {
                if (rectype != kAllRecord && rectype != kFindOneRecord &&
                    rectype != kFindDailyRecord && rectype != kFindWeeklyRecord)
                    type->SetText(curitem->ChannelText(channelFormat));
                else
                    type->SetText(tr("Any"));
            }

            type = (UITextType *)container->GetType("longchannel");
            if (type) {
                if (rectype != kAllRecord && rectype != kFindOneRecord &&
                    rectype != kFindDailyRecord && rectype != kFindWeeklyRecord)
                    type->SetText(curitem->ChannelText(longChannelFormat));
                else
                    type->SetText(tr("Any"));
            }

            type = (UITextType *)container->GetType("channelrecpriority");
            if (type) {
                type->SetText(QString::number(abs(chanrecpriority)));
            }

            type = (UITextType *)container->GetType("channelsign");
            if (type) {
                if (chanrecpriority >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("recpriority");
            if (type) {
                if (curitem->recpriority >= 0)
                    type->SetText("+"+QString::number(curitem->recpriority));
                else
                    type->SetText(QString::number(curitem->recpriority));
            }

            type = (UITextType *)container->GetType("recpriorityB");
            if (type) {
                type->SetText(QString::number(abs(progRecPriority)));
            }

            type = (UITextType *)container->GetType("recprioritysign");
            if (type) {
                if (finalRecPriority >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("finalrecpriority");
            if (type) {
                if (finalRecPriority >= 0)
                    type->SetText("+"+QString::number(finalRecPriority));
                else
                    type->SetText(QString::number(finalRecPriority));
            }
        }
       
        if (container)
        {
            container->Draw(&tmp, 4, 0);
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
            container->Draw(&tmp, 7, 0);
            container->Draw(&tmp, 8, 0);
        }
    }
    else
    {
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (norec)
        {
            norec->Draw(&tmp, 4, 0);
            norec->Draw(&tmp, 5, 0);
            norec->Draw(&tmp, 6, 0);
            norec->Draw(&tmp, 7, 0);
            norec->Draw(&tmp, 8, 0);
        }

    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}
