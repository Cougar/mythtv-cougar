#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qaccel.h>
#include <qlistview.h>
#include <qdatetime.h>
#include <qprogressbar.h>
#include <qlayout.h>
#include <qapplication.h>
#include <qtimer.h>
#include <qimage.h>
#include <qpainter.h>
#include <qheader.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qsqldatabase.h>
#include <qmap.h>
#include <qwaitcondition.h>

#include <cmath>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "playbackbox.h"
#include "proglist.h"
#include "tv.h"
#include "oldsettings.h"
#include "NuppelVideoPlayer.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "scheduledrecording.h"
#include "remoteutil.h"
#include "lcddevice.h"
#include "previewgenerator.h"
#include "playgroup.h"
#include "customedit.h"

#define LOC QString("PlaybackBox: ")
#define LOC_ERR QString("PlaybackBox Error: ")

#define REC_CAN_BE_DELETED(rec) \
    ((((rec)->programflags & FL_INUSEPLAYING) == 0) && \
     ((((rec)->programflags & FL_INUSERECORDING) == 0) || \
      ((rec)->recgroup != "LiveTV")))

#define USE_PREV_GEN_THREAD

QWaitCondition pbbIsVisibleCond;

static int comp_programid(ProgramInfo *a, ProgramInfo *b)
{
    if (a->programid == b->programid)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->programid < b->programid ? 1 : -1);
}

static int comp_programid_rev(ProgramInfo *a, ProgramInfo *b)
{
    if (a->programid == b->programid)
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (a->programid > b->programid ? 1 : -1);
}

static int comp_originalAirDate(ProgramInfo *a, ProgramInfo *b)
{
    QDate dt1, dt2;

    if (a->hasAirDate)
        dt1 = a->originalAirDate;
    else
        dt1 = a->startts.date();

    if (b->hasAirDate)
        dt2 = b->originalAirDate;
    else
        dt2 = b->startts.date();

    if (dt1 == dt2)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (dt1 < dt2 ? 1 : -1);
}

static int comp_originalAirDate_rev(ProgramInfo *a, ProgramInfo *b)
{
    QDate dt1, dt2;

    if (a->hasAirDate)
        dt1 = a->originalAirDate;
    else
        dt1 = a->startts.date();

    if (b->hasAirDate)
        dt2 = b->originalAirDate;
    else
        dt2 = b->startts.date();

    if (dt1 == dt2)
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (dt1 > dt2 ? 1 : -1);
}

static int comp_recpriority2(ProgramInfo *a, ProgramInfo *b)
{
    if (a->recpriority2 == b->recpriority2)
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->recpriority2 < b->recpriority2 ? 1 : -1);
}

static PlaybackBox::ViewMask viewMaskToggle(PlaybackBox::ViewMask mask,
        PlaybackBox::ViewMask toggle)
{
    // can only toggle a single bit at a time
    if ((mask & toggle))
        return (PlaybackBox::ViewMask)(mask & ~toggle);
    return (PlaybackBox::ViewMask)(mask | toggle);
}

static QString sortTitle(QString title, PlaybackBox::ViewMask viewmask,
        PlaybackBox::ViewTitleSort titleSort, int recpriority)
{
    if (title == "")
        return title;

    QRegExp prefixes = QObject::tr("^(The |A |An )");
    QString sTitle = title;

    sTitle.remove(prefixes);
    if (viewmask == PlaybackBox::VIEW_TITLES &&
            titleSort == PlaybackBox::TitleSortRecPriority)
    {
        // Also incorporate recpriority (reverse numeric sort). In
        // case different episodes of a recording schedule somehow
        // have different recpriority values (e.g., manual fiddling
        // with database), the title will appear once for each
        // distinct recpriority value among its episodes.
        //
        // Deal with QMap sorting. Positive recpriority values have a
        // '+' prefix (QMap alphabetically sorts before '-'). Positive
        // recpriority values are "inverted" by substracting them from
        // 1000, so that high recpriorities are sorted first (QMap
        // alphabetically). For example:
        //
        //      recpriority =>  sort key
        //          95          +905
        //          90          +910
        //          89          +911
        //           1          +999
        //           0          -000
        //          -5          -005
        //         -10          -010
        //         -99          -099

        QString sortprefix;
        if (recpriority > 0)
            sortprefix.sprintf("+%03u", 1000 - recpriority);
        else
            sortprefix.sprintf("-%03u", -recpriority);

        sTitle = sortprefix + "-" + sTitle;
    }
    return sTitle;
}

static int comp_recordDate(ProgramInfo *a, ProgramInfo *b)
{
    if (a->startts.date() == b->startts.date())
        return (a->recstartts < b->recstartts ? 1 : -1);
    else
        return (a->startts.date() < b->startts.date() ? 1 : -1);
}

static int comp_recordDate_rev(ProgramInfo *a, ProgramInfo *b)
{
    if (a->startts.date() == b->startts.date())
        return (a->recstartts > b->recstartts ? 1 : -1);
    else
        return (a->startts.date() > b->startts.date() ? 1 : -1);
}


ProgramInfo *PlaybackBox::RunPlaybackBox(void * player)
{
    ProgramInfo *nextProgram = NULL;
    qApp->lock();

    PlaybackBox *pbb = new PlaybackBox(PlaybackBox::Play,
            gContext->GetMainWindow(), "tvplayselect", (TV *)player);
    pbb->Show();

    qApp->unlock();
    pbbIsVisibleCond.wait();

    if (pbb->getSelected())
        nextProgram = new ProgramInfo(*pbb->getSelected());
   
    delete pbb;
    
    return nextProgram;
}

PlaybackBox::PlaybackBox(BoxType ltype, MythMainWindow *parent, 
                         const char *name, TV *player)
    : MythDialog(parent, name),
      // Settings
      type(ltype),
      formatShortDate("M/d"),           formatLongDate("ddd MMMM d"),
      formatTime("h:mm AP"),
      titleView(true),                  useCategories(false),
      useRecGroups(false),              watchListAutoExpire(false),
      watchListMaxAge(60),              watchListBlackOut(2),
      groupnameAsAllProg(false),
      arrowAccel(true),                 ignoreKeyPressEvents(false),
      listOrder(1),                     listsize(0),
      // Recording Group settings
      groupDisplayName(tr("All Programs")),
      recGroup("All Programs"),
      recGroupPassword(""),             curGroupPassword(""),
      watchGroup(" " + tr("Watch List")),
      viewMask(VIEW_TITLES),
      // Theme parsing
      theme(new XMLParse()),
      // Non-volatile drawing variables
      drawTransPixmap(NULL),            drawPopupSolid(true),
      drawPopupFgColor(Qt::white),      drawPopupBgColor(Qt::black),
      drawPopupSelColor(Qt::green),
      drawTotalBounds(0, 0, size().width(), size().height()),
      drawListBounds(0, 0, 0, 0),       drawInfoBounds(0, 0, 0, 0),
      drawGroupBounds(0, 0, 0, 0),      drawUsageBounds(0, 0, 0, 0),
      drawVideoBounds(0, 0, 0, 0),      drawCurGroupBounds(0, 0, 0, 0),
      // General popup support
      popup(NULL),                      expectingPopup(false),
      // Recording Group popup support
      recGroupLastItem(0),              recGroupChooserPassword(""),
      recGroupPopup(NULL),              recGroupListBox(NULL),
      recGroupLineEdit(NULL),           recGroupLineEdit1(NULL),
      recGroupOldPassword(NULL),        recGroupNewPassword(NULL),
      recGroupOkButton(NULL),
      // Main Recording List support
      fillListTimer(new QTimer(this)),  connected(false),
      titleIndex(0),                    progIndex(0),
      progsInDB(0),
      // Other state
      curitem(NULL),                    delitem(NULL),
      progCache(NULL),                  playingSomething(false),
      // Selection state variables
      haveGroupInfoSet(false),          inTitle(false),
      leftRight(false),
      // Free disk space tracking
      freeSpaceNeedsUpdate(true),       freeSpaceTimer(new QTimer(this)),
      freeSpaceTotal(0),                freeSpaceUsed(0),
      // Volatile drawing variables
      paintSkipCount(0),                paintSkipUpdate(false),
      // Preview Video Variables
      previewVideoNVP(NULL),            previewVideoRingBuf(NULL),
      previewVideoRefreshTimer(new QTimer(this)),
      previewVideoBrokenRecId(0),       previewVideoState(kStopped),
      previewVideoStartTimerOn(false),  previewVideoEnabled(false),
      previewVideoPlaying(false),       previewVideoThreadRunning(false),
      previewVideoKillState(kDone),
      // Preview Image Variables
      previewPixmapEnabled(false),      previewPixmap(NULL),
      previewSuspend(false),            previewChanid(""),
      // Network Control Variables
      underNetworkControl(false),
      m_player(NULL)
{
    formatShortDate    = gContext->GetSetting("ShortDateFormat", "M/d");
    formatLongDate     = gContext->GetSetting("DateFormat", "ddd MMMM d");
    formatTime         = gContext->GetSetting("TimeFormat", "h:mm AP");
    recGroup           = gContext->GetSetting("DisplayRecGroup","All Programs");
    int pbOrder        = gContext->GetNumSetting("PlayBoxOrdering", 1);
    // Split out sort order modes, wacky order for backward compatibility
    listOrder = (pbOrder >> 1) ^ (allOrder = pbOrder & 1);
    watchListAutoExpire= gContext->GetNumSetting("PlaybackWLAutoExpire", 0);
    watchListMaxAge    = gContext->GetNumSetting("PlaybackWLMaxAge", 60);
    watchListBlackOut  = gContext->GetNumSetting("PlaybackWLBlackOut", 2);
    groupnameAsAllProg = gContext->GetNumSetting("DispRecGroupAsAllProg", 0);
    arrowAccel         = gContext->GetNumSetting("UseArrowAccels", 1);
    inTitle            = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);
    if (!player)
        previewVideoEnabled =gContext->GetNumSetting("PlaybackPreview");
    previewPixmapEnabled=gContext->GetNumSetting("GeneratePreviewPixmaps");
    previewFromBookmark= gContext->GetNumSetting("PreviewFromBookmark");
    drawTransPixmap    = gContext->LoadScalePixmap("trans-backup.png");
    if (!drawTransPixmap)
        drawTransPixmap = new QPixmap();

    bool displayCat  = gContext->GetNumSetting("DisplayRecGroupIsCategory", 0);
    int  initialFilt = gContext->GetNumSetting("QueryInitialFilter", 0);

    viewMask = (ViewMask)gContext->GetNumSetting(
            "DisplayGroupDefaultViewMask", VIEW_TITLES);

    if (gContext->GetNumSetting("PlaybackWatchList", 1))
        viewMask = (ViewMask)(viewMask | VIEW_WATCHLIST);

    progLists[""];
    titleList << "";
    playList.clear();

    if (player)
        m_player = player;

    // recording group stuff
    recGroupType.clear();
    recGroupType[recGroup] = (displayCat) ? "category" : "recgroup";
    if (groupnameAsAllProg)
    {
        groupDisplayName = recGroup;
        if ((recGroup == "All Programs") ||
            (recGroup == "Default") ||
            (recGroup == "LiveTV"))
        {
            groupDisplayName = tr(recGroup);
        }
    }
    recGroupPassword = getRecGroupPassword(recGroup);

    // theme stuff
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "playback");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    UIListType *listtype = NULL;
    if (container)
    {
        listtype = (UIListType *)container->GetType("showing");
        if (listtype)
            listsize = listtype->GetItems();
    }
    else
    {
        QString btn0 = QObject::tr("Failed to get selector object");
        QString btn1 = QObject::tr(
            "Myth could not locate the selector object within your theme.\n"
            "Please make sure that your ui.xml is valid.\n"
            "\n"
            "Myth will now exit.");

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), btn0, btn1);
                                  
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get selector object.");
        exit(FRONTEND_BUGGY_EXIT_NO_SELECTOR);
        return;
    }

    if (theme->GetSet("group_info") && 
        gContext->GetNumSetting("ShowGroupInfo", 0) == 1)
    {
        haveGroupInfoSet = true;
        if (listtype)
            listtype->ShowSelAlways(false);
    }

    // initially fill the list
    connected = FillList();

    // connect up timers...
    connect(previewVideoRefreshTimer, SIGNAL(timeout()),
            this,                     SLOT(timeout()));
    connect(freeSpaceTimer,           SIGNAL(timeout()),
            this,                     SLOT(setUpdateFreeSpace()));
    connect(fillListTimer,            SIGNAL(timeout()),
            this,                     SLOT(listChanged()));

    // preview video & preview pixmap init
    previewVideoRefreshTimer->start(500);
    previewStartts = QDateTime::currentDateTime();

    // misc setup
    updateBackground();
    setNoErase();
    gContext->addListener(this);

    if ((!recGroupPassword.isEmpty()) ||
        ((titleList.count() <= 1) && (progsInDB > 0)) ||
        (initialFilt))
        showRecGroupChooser();

    gContext->addCurrentLocation((type == Delete)? "DeleteBox":"PlaybackBox");
}

PlaybackBox::~PlaybackBox(void)
{
    gContext->removeListener(this);
    gContext->removeCurrentLocation();

    if (!m_player)
        killPlayerSafe();

    if (previewVideoRefreshTimer)
    {
        previewVideoRefreshTimer->disconnect(this);
        previewVideoRefreshTimer->deleteLater();
        previewVideoRefreshTimer = NULL;
    }

    if (fillListTimer)
    {
        fillListTimer->disconnect(this);
        fillListTimer->deleteLater();
        fillListTimer = NULL;
    }

    if (freeSpaceTimer)
    {
        freeSpaceTimer->disconnect(this);
        freeSpaceTimer->deleteLater();
        freeSpaceTimer = NULL;
    }

    if (theme)
    {
        delete theme;
        theme = NULL;
    }

    if (drawTransPixmap)
    {
        delete drawTransPixmap;
        drawTransPixmap = NULL;
    }
    
    if (curitem)
    {
        delete curitem;
        curitem = NULL;
    }

    if (delitem)
    {
        delete delitem;
        delitem = NULL;
    }

    clearProgramCache();

    // disconnect preview generators
    QMutexLocker locker(&previewGeneratorLock);
    QMap<QString, PreviewGenerator*>::iterator it = previewGenerator.begin();
    for (;it != previewGenerator.end(); ++it)
    {
        if (*it)
            (*it)->disconnectSafe();
    }

    // free preview pixmap after preview generators are
    // no longer telling us about any new previews.
    if (previewPixmap)
    {
        delete previewPixmap;
        previewPixmap = NULL;
    }
}

/* blocks until playing has stopped */
void PlaybackBox::killPlayerSafe(void)
{
    QMutexLocker locker(&previewVideoKillLock);

    // Don't process any keys while we are trying to make the nvp stop.
    // Qt's setEnabled(false) doesn't work, because of LIRC events...
    ignoreKeyPressEvents = true;

    while (previewVideoState != kKilled && previewVideoState != kStopped &&
           previewVideoThreadRunning)
    {
        /* ensure that key events don't mess up our previewVideoStates */
        previewVideoState = (previewVideoState == kKilled) ?
            kKilled :  kKilling;

        /* NOTE: need unlock/process/lock here because we need
           to allow updateVideo() to run to handle changes in
           previewVideoStates */
        qApp->unlock();
        qApp->processEvents();
        usleep(500);
        qApp->lock();
    }
    previewVideoState = kStopped;

    ignoreKeyPressEvents = false;
}

void PlaybackBox::LoadWindow(QDomElement &element)
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
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("PlaybackBox::LoadWindow(): Ignoring unknown "
                                "element, %1. ").arg(e.tagName()));
            }
        }
    }
}

void PlaybackBox::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context; 
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        drawListBounds = area;
    if (name.lower() == "program_info_play" && context == 0 && type != Delete)
        drawInfoBounds = area;
    if (name.lower() == "program_info_del" && context == 1 && type == Delete)
        drawInfoBounds = area;
    if (name.lower() == "video")
        drawVideoBounds = area;
    if (name.lower() == "group_info")
        drawGroupBounds = area;        
    if (name.lower() == "usage")
        drawUsageBounds = area;
    if (name.lower() == "cur_group")
        drawCurGroupBounds = area;        
    
}

void PlaybackBox::parsePopup(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty() || name != "confirmdelete")
    {
        if (name.isNull())
            name = "(null)";
        VERBOSE(VB_IMPORTANT,
                QString("PlaybackBox::parsePopup(): Popup name must "
                        "be 'confirmdelete' but was '%1'").arg(name));
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "solidbgcolor")
            {
                QString col = theme->getFirstText(info);
                drawPopupBgColor = QColor(col);
                drawPopupSolid = false;
            }
            else if (info.tagName() == "foreground")
            {
                QString col = theme->getFirstText(info);
                drawPopupFgColor = QColor(col);
            }
            else if (info.tagName() == "highlight")
            {
                QString col = theme->getFirstText(info);
                drawPopupSelColor = QColor(col);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("PlaybackBox::parsePopup(): Unknown child %1")
                        .arg(info.tagName()));
            }
        }
    }
}

void PlaybackBox::exitWin()
{
    if (m_player)
    {
        if (curitem)
            delete curitem;
        curitem = NULL;
        pbbIsVisibleCond.wakeAll();
    }
    else
        killPlayerSafe();

    accept();
}

void PlaybackBox::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container && type != Delete)
        container->Draw(&tmp, 0, 0);
    else
        container->Draw(&tmp, 0, 1);

    tmp.end();
    paintBackgroundPixmap = bground;
 
    setPaletteBackgroundPixmap(paintBackgroundPixmap);
}
  
void PlaybackBox::paintEvent(QPaintEvent *e)
{
    if (e->erased())
        paintSkipUpdate = false;

    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(drawListBounds) && !paintSkipUpdate)
    {
        updateShowTitles(&p);
    }
    
    if (r.intersects(drawInfoBounds) && !paintSkipUpdate)
    {
        updateInfo(&p);
    }
    
    if (r.intersects(drawCurGroupBounds) && !paintSkipUpdate)
    {
        updateCurGroup(&p);
    }
    
    if (r.intersects(drawUsageBounds) && !paintSkipUpdate)
    {
        updateUsage(&p);
    }
    
    if (r.intersects(drawVideoBounds))
    {
        updateVideo(&p);
    }

    paintSkipCount--;
    if (paintSkipCount < 0)
    {
        paintSkipUpdate = true;
        paintSkipCount = 0;
    }
}

void PlaybackBox::grayOut(QPainter *tmp)
{
    (void)tmp;
/*
    int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);
    if (transparentFlag == 0)
        tmp->fillRect(QRect(QPoint(0, 0), size()), 
                      QBrush(QColor(10, 10, 10), Dense4Pattern));
    else if (transparentFlag == 1)
    {
        int ww, hh;

        if (d->IsWideMode())
        {
            ww = 1280;
            hh = 720;
        }
        else
        {
            ww = 800;
            hh = 600;
        }
        tmp->drawPixmap(0, 0, *drawTransPixmap, 0, 0, (int)(ww*wmult), 
                        (int)(hh*hmult));
    }
*/
}
void PlaybackBox::updateCurGroup(QPainter *p)
{
    QRect pr = drawCurGroupBounds;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    if (recGroup != "Default")
        updateGroupInfo(p, pr, pix, "cur_group");
    else
    {
        LayerSet *container = theme->GetSet("cur_group");
        if (container)
        {
            QPainter tmp(&pix);
            
            container->ClearAllText();
            container->Draw(&tmp, 6, 1);

            tmp.end();
            p->drawPixmap(pr.topLeft(), pix);
        }
    
    }
}


void PlaybackBox::updateGroupInfo(QPainter *p, QRect& pr,
                                  QPixmap& pix, QString cont_name)
{
    LayerSet *container = theme->GetSet(cont_name);
    if ( container)
    {    
        container->ClearAllText();
        QPainter tmp(&pix);
        QMap<QString, QString> infoMap;
        int countInGroup; // = progLists[""].count();
        
       
        if (titleList[titleIndex] == "")
        {
           countInGroup = progLists[""].count(); 
           infoMap["title"] = groupDisplayName;
           infoMap["group"] = groupDisplayName;
           infoMap["show"] = QObject::tr("All Programs");
        }
        else
        {                  
            countInGroup = progLists[titleList[titleIndex]].count();
            infoMap["group"] = groupDisplayName;
            infoMap["show"] = titleList[titleIndex];
            infoMap["title"] = QString("%1 - %2").arg(groupDisplayName)
                                                 .arg(titleList[titleIndex]);
        }
        
        if (countInGroup > 1)
            infoMap["description"] = QString(tr("There are %1 recordings in "
                                                "this display group"))
                                                .arg(countInGroup);
        else if (countInGroup == 1)
            infoMap["description"] = QString(tr("There is one recording in "
                                                 "this display group"));
        else
            infoMap["description"] = QString(tr("There are no recordings in "
                                                "this display group"));
        
        infoMap["rec_count"] = QString("%1").arg(countInGroup);
                                                
        container->SetText(infoMap);

        if (type != Delete)
            container->Draw(&tmp, 6, 0);
        else
            container->Draw(&tmp, 6, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
    else
    {
        if (cont_name == "group_info")
            updateProgramInfo(p, pr, pix);
    }
}


void PlaybackBox::updateProgramInfo(QPainter *p, QRect& pr, QPixmap& pix)
{
    QMap<QString, QString> infoMap;
    QPainter tmp(&pix);
        
    if (previewVideoPlaying)
        previewVideoState = kChanging;

    LayerSet *container = NULL;
    if (type != Delete)
        container = theme->GetSet("program_info_play");
    else
        container = theme->GetSet("program_info_del");

    if (container)
    {
        if (curitem)
            curitem->ToMap(infoMap);
        
        if ((previewVideoEnabled == 0) &&
            (previewPixmapEnabled == 0))
            container->UseAlternateArea(true);

        container->ClearAllText();
        container->SetText(infoMap);

        int flags = 0;
        if (curitem)
            flags = curitem->programflags;

        QMap <QString, int>::iterator it;
        QMap <QString, int> iconMap;

        iconMap["commflagged"] = FL_COMMFLAG;
        iconMap["cutlist"]     = FL_CUTLIST;
        iconMap["autoexpire"]  = FL_AUTOEXP;
        iconMap["processing"]  = FL_EDITING;
        iconMap["bookmark"]    = FL_BOOKMARK;
        iconMap["inuse"]       = (FL_INUSERECORDING | FL_INUSEPLAYING);
        iconMap["transcoded"]  = FL_TRANSCODED;
        iconMap["stereo"]      = FL_STEREO;
        iconMap["cc"]          = FL_CC;
        iconMap["hdtv"]        = FL_HDTV;
        iconMap["watched"]     = FL_WATCHED;
        iconMap["preserved"]   = FL_PRESERVED;

        UIImageType *itype;
        for (it = iconMap.begin(); it != iconMap.end(); ++it)
        {
            itype = (UIImageType *)container->GetType(it.key());
            if (itype)
            {
                if (flags & it.data())
                    itype->show();
                else
                    itype->hide();
            }
        }

        container->Draw(&tmp, 6, (type == Delete) ? 1 : 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

    previewVideoStartTimer.start();
    previewVideoStartTimerOn = true;
}

void PlaybackBox::updateInfo(QPainter *p)
{
    QRect pr = drawInfoBounds;
    bool updateGroup = (inTitle && haveGroupInfoSet);
    if (updateGroup)
        pr = drawGroupBounds;
        
    QPixmap pix(pr.size());
    
    pix.fill(this, pr.topLeft());
    
    if (titleList.count() > 1 && curitem && !updateGroup)
        updateProgramInfo(p, pr, pix);
    else if (updateGroup)
        updateGroupInfo(p, pr, pix);
    else
    {
        QPainter tmp(&pix);
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (type != Delete && norec)
            norec->Draw(&tmp, 8, 0);
        else if (norec)
            norec->Draw(&tmp, 8, 1);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
}

void PlaybackBox::updateVideo(QPainter *p)
{
    // If we're displaying group info don't update the video.
    if (inTitle && haveGroupInfoSet)
        return;

    /* show a still frame if the user doesn't want a video preview or nvp 
     * hasn't started playing the video preview yet */
    if (curitem && !playingSomething &&
        (!previewVideoEnabled             ||
         !previewVideoPlaying             ||
         (previewVideoState == kStarting) || 
         (previewVideoState == kChanging)))
    {
        QPixmap temp = getPixmap(curitem);
        if (temp.width() > 0)
            p->drawPixmap(drawVideoBounds.x(), drawVideoBounds.y(), temp);
    }

    /* keep calling killPlayer() to handle nvp cleanup */
    /* until killPlayer() is done */
    if (previewVideoKillState != kDone && !killPlayer())
        return;

    /* if we aren't supposed to have a preview playing then always go */
    /* to the stopping previewVideoState */
    if (!previewVideoEnabled &&
        (previewVideoState != kKilling) && (previewVideoState != kKilled))
    {
        previewVideoState = kStopping;
    }

    /* if we have no nvp and aren't playing yet */
    /* if we have an item we should start playing */
    if (!previewVideoNVP   && previewVideoEnabled  &&
        curitem            && !previewVideoPlaying &&
        (previewVideoState != kKilling) &&
        (previewVideoState != kKilled)  &&
        (previewVideoState != kStarting))
    {
        ProgramInfo *rec = curitem;

        if (fileExists(rec) == false)
        {
            VERBOSE(VB_IMPORTANT, QString("Error: File '%1' missing.")
                    .arg(rec->pathname));

            previewVideoState = kStopping;

            ProgramInfo *tmpItem = findMatchingProg(rec);
            if (tmpItem)
                tmpItem->availableStatus = asFileNotFound;

            return;
        }
        previewVideoState = kStarting;
    }

    if (previewVideoState == kChanging)
    {
        if (previewVideoNVP)
        {
            killPlayer(); /* start killing the player */
            return;
        }

        previewVideoState = kStarting;
    }

    if ((previewVideoState == kStarting) && 
        (!previewVideoStartTimerOn ||
         (previewVideoStartTimer.elapsed() > 500)))
    {
        previewVideoStartTimerOn = false;

        if (!previewVideoNVP)
            startPlayer(curitem);

        if (previewVideoNVP)
        {
            if (previewVideoNVP->IsPlaying())
                previewVideoState = kPlaying;
        }
        else
        {
            // already dead, so clean up
            killPlayer();
            return;
        }
    }

    if ((previewVideoState == kStopping) || (previewVideoState == kKilling))
    {
        if (previewVideoNVP)
        {
            killPlayer(); /* start killing the player and exit */
            return;
        }

        if (previewVideoState == kKilling)
            previewVideoState = kKilled;
        else
            previewVideoState = kStopped;
    }

    /* if we are playing and nvp is running, then grab a new video frame */
    if ((previewVideoState == kPlaying) && previewVideoNVP->IsPlaying() &&
        !playingSomething)
    {
        QSize size = drawVideoBounds.size();
        float saspect = ((float)size.width()) / ((float)size.height());
        float vaspect = previewVideoNVP->GetVideoAspect();
        size.setHeight((int) ceil(size.height() * (saspect / vaspect)));
        size.setHeight(((size.height() + 7) / 8) * 8);
        size.setWidth( ((size.width()  + 7) / 8) * 8);
        const QImage &img = previewVideoNVP->GetARGBFrame(size);
        p->drawImage(drawVideoBounds.x(), drawVideoBounds.y(), img);
    }

    /* have we timed out waiting for nvp to start? */
    if ((previewVideoState == kPlaying) && !previewVideoNVP->IsPlaying())
    {
        if (previewVideoPlayingTimer.elapsed() > 2000)
        {
            previewVideoState = kStarting;
            killPlayer();
            return;
        }
    }
}

void PlaybackBox::updateUsage(QPainter *p)
{
    LayerSet *container = NULL;
    container = theme->GetSet("usage");
    if (container)
    {
        int ccontext = container->GetContext();
        
        if (ccontext != -1)
        {
            if (ccontext == 1 && type != Delete)
                return;
            if (ccontext == 0 && type == Delete)
                return;
        }

        vector<FileSystemInfo> fsInfos;
        if (freeSpaceNeedsUpdate && connected)
        {
            freeSpaceNeedsUpdate = false;
            freeSpaceTotal = 0;
            freeSpaceUsed = 0;

            fsInfos = RemoteGetFreeSpace();
            for (unsigned int i = 0; i < fsInfos.size(); i++)
            {
                freeSpaceTotal += (int) (fsInfos[i].totalSpaceKB >> 10);
                freeSpaceUsed += (int) (fsInfos[i].usedSpaceKB >> 10);
            }
            freeSpaceTimer->start(15000);
        }    

        QString usestr;

        double perc = 0.0;
        if (freeSpaceTotal > 0)
            perc = (100.0 * freeSpaceUsed) / (double) freeSpaceTotal;

        usestr.sprintf("%d", (int)perc);
        usestr = usestr + tr("% used");

        QString size;
        size.sprintf("%0.2f", (freeSpaceTotal - freeSpaceUsed) / 1024.0);
        QString rep = tr(", %1 GB free").arg(size);
        usestr = usestr + rep;

        QRect pr = drawUsageBounds;
        QPixmap pix(pr.size());
        pix.fill(this, pr.topLeft());
        QPainter tmp(&pix);

        UITextType *ttype = (UITextType *)container->GetType("freereport");
        if (ttype)
            ttype->SetText(usestr);

        UIStatusBarType *sbtype = 
                               (UIStatusBarType *)container->GetType("usedbar");
        if (sbtype)
        {
            sbtype->SetUsed(freeSpaceUsed);
            sbtype->SetTotal(freeSpaceTotal);
        }

        if (type != Delete)
        {
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
        }
        else
        {
            container->Draw(&tmp, 5, 1);
            container->Draw(&tmp, 6, 1);
        }

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }
}

void PlaybackBox::updateShowTitles(QPainter *p)
{
    QString tempTitle;
    QString tempSubTitle;
    QString tempDate;
    QString tempTime;
    QString tempSize;

    QString match;
    QRect pr = drawListBounds;
    QPixmap pix(pr.size());

    LayerSet *container = NULL;
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LCD *lcddev = LCD::Get();
    QString tstring, lcdTitle;
    QPtrList<LCDMenuItem> lcdItems;
    lcdItems.setAutoDelete(true);

    container = theme->GetSet("selector");

    ProgramInfo *tempInfo;

    ProgramList *plist;
    if (titleList.count() > 1)
        plist = &progLists[titleList[titleIndex]];
    else
        plist = &progLists[""];

    int progCount = plist->count();

    if (curitem)
    {
        delete curitem;
        curitem = NULL;
    }

    if (container && titleList.count() >= 1)
    {
        UIListType *ltype = (UIListType *)container->GetType("toptitles");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(inTitle);

            int h = titleIndex - ltype->GetItems() +
                ltype->GetItems() * titleList.count();
            h = h % titleList.count();

            for (int cnt = 0; cnt < ltype->GetItems(); cnt++)
            {
                if (titleList[h] == "")
                    tstring = groupDisplayName;
                else
                    tstring = titleList[h];

                tstring.remove(QRegExp("^ "));
                ltype->SetItemText(cnt, tstring);
                if (lcddev && inTitle)
                    lcdItems.append(new LCDMenuItem(0, NOTCHECKABLE, tstring));

                h++;
                h = h % titleList.count();
             }
        }
        else if (ltype)
        {
            ltype->ResetList();
        }

        UITextType *typeText = (UITextType *)container->GetType("current");
        if (typeText && titleList.count() > 1)
        {
            if (titleList[titleIndex] == "")
                tstring = groupDisplayName;
            else
                tstring = titleList[titleIndex];

            tstring.remove(QRegExp("^ "));
            typeText->SetText(tstring);
            if (lcddev) 
            {
                if (inTitle)
                {
                    lcdItems.append(new LCDMenuItem(1, NOTCHECKABLE, tstring));
                    lcdTitle = "Recordings";
                }
                else
                    lcdTitle = " <<" + tstring;
            }
        }
        else if (typeText)
        {
            typeText->SetText("");
        }

        ltype = (UIListType *)container->GetType("bottomtitles");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(inTitle);

            int h = titleIndex + 1;
            h = h % titleList.count();

            for (int cnt = 0; cnt < ltype->GetItems(); cnt++)
            {
                if (titleList[h] == "")
                    tstring = groupDisplayName;
                else
                    tstring = titleList[h];

                tstring.remove(QRegExp("^ "));
                ltype->SetItemText(cnt, tstring);
                if (lcddev && inTitle)
                    lcdItems.append(new LCDMenuItem(0, NOTCHECKABLE, tstring));

                h++;
                h = h % titleList.count();
            }
        }
        else if (ltype)
        {
            ltype->ResetList();
        }

        ltype = (UIListType *)container->GetType("showing");
        if (ltype && titleList.count() > 1)
        {
            ltype->ResetList();
            ltype->SetActive(!inTitle);

            int skip;
            if (progCount <= listsize || progIndex <= listsize / 2)
                skip = 0;
            else if (progIndex >= progCount - listsize + listsize / 2)
                skip = progCount - listsize;
            else
                skip = progIndex - listsize / 2;

            ltype->SetUpArrow(skip > 0);
            ltype->SetDownArrow(skip + listsize < progCount);

            int cnt;
            for (cnt = 0; cnt < listsize; cnt++)
            {
                if (cnt + skip >= progCount)
                    break;

                tempInfo = plist->at(skip+cnt);

                if (titleList[titleIndex] != tempInfo->title)
                {
                    tempSubTitle = tempInfo->title;
                    if (tempInfo->subtitle.stripWhiteSpace().length() > 0)
                        tempSubTitle = tempSubTitle + " - \"" + 
                                       tempInfo->subtitle + "\"";
                }
                else
                {
                    tempSubTitle = tempInfo->subtitle;
                    if (tempSubTitle.stripWhiteSpace().length() == 0)
                        tempSubTitle = tempInfo->title;
                }

                tempDate = (tempInfo->recstartts).toString(formatShortDate);
                tempTime = (tempInfo->recstartts).toString(formatTime);

                long long size = tempInfo->filesize;
                tempSize.sprintf("%0.2f GB", size / 1024.0 / 1024.0 / 1024.0);

                if (skip + cnt == progIndex)
                {
                    curitem = new ProgramInfo(*tempInfo);
                    ltype->SetItemCurrent(cnt);
                }

                if (lcddev && !inTitle) 
                {
                    QString lcdSubTitle = tempSubTitle;
                    lcdItems.append(new LCDMenuItem(skip + cnt == progIndex,
                                    NOTCHECKABLE,
                                    lcdSubTitle.replace('"', "'") +
                                    " " + tempDate));                       
                }

                ltype->SetItemText(cnt, 1, tempSubTitle);
                ltype->SetItemText(cnt, 2, tempDate);
                ltype->SetItemText(cnt, 3, tempTime);
                ltype->SetItemText(cnt, 4, tempSize);
                if (tempInfo->recstatus == rsRecording)
                    ltype->EnableForcedFont(cnt, "recording");

                if (playList.grep(tempInfo->MakeUniqueKey()).count())
                    ltype->EnableForcedFont(cnt, "tagged");

                if (tempInfo->availableStatus != asAvailable || 
                        (m_player && m_player->IsSameProgram(tempInfo)) && tempInfo->recstatus != rsRecording)
                    ltype->EnableForcedFont(cnt, "inactive");
            }
        } 
        else if (ltype)
        {
            ltype->ResetList();
        }
    } 

    if (lcddev && !lcdItems.isEmpty())
        lcddev->switchToMenu(&lcdItems, lcdTitle);

    // DRAW LAYERS
    if (container && type != Delete)
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
    else
    {
        container->Draw(&tmp, 0, 1);
        container->Draw(&tmp, 1, 1);
        container->Draw(&tmp, 2, 1);
        container->Draw(&tmp, 3, 1);
        container->Draw(&tmp, 4, 1);
        container->Draw(&tmp, 5, 1);
        container->Draw(&tmp, 6, 1);
        container->Draw(&tmp, 7, 1);
        container->Draw(&tmp, 8, 1);
    }

    leftRight = false;

    if (titleList.count() <= 1)
    {
        LayerSet *norec = theme->GetSet("norecordings_list");
        UITextType *ttype = (UITextType *)norec->GetType("msg");
        if (ttype)
        {
            if (progCache && !progCache->empty())
                ttype->SetText(tr(
                    "There are no recordings in your current view"));
            else
                ttype->SetText(tr(
                    "There are no recordings available"));
        }

        if (type != Delete && norec)
            norec->Draw(&tmp, 8, 0);
        else if (norec)
            norec->Draw(&tmp, 8, 1);
    }
  
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);

    if (titleList.count() <= 1)
        update(drawInfoBounds);
}

void PlaybackBox::cursorLeft()
{
    if (!inTitle)
    {
        if (haveGroupInfoSet)
            killPlayerSafe();
        
        inTitle = true;
        paintSkipUpdate = false;
        
        update(drawTotalBounds);
        leftRight = true;
    }
    else if (arrowAccel)
        exitWin();
        
}

void PlaybackBox::cursorRight()
{
    if (inTitle)
    {
        leftRight = true;
        inTitle = false;
        paintSkipUpdate = false;
        update(drawTotalBounds);
    }
    else if (arrowAccel)
        showActionsSelected();
    else if (curitem && curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
}

void PlaybackBox::cursorDown(bool page, bool newview)
{
    if (inTitle == true || newview)
    {
        titleIndex += (page ? 5 : 1);
        titleIndex = titleIndex % (int)titleList.count();

        progIndex = 0;

        if (newview)
            inTitle = false;

        paintSkipUpdate = false;
        update(drawTotalBounds);
    }
    else 
    {
        int progCount = progLists[titleList[titleIndex]].count();
        if (progIndex < progCount - 1) 
        {
            progIndex += (page ? listsize : 1);
            if (progIndex > progCount - 1)
                progIndex = progCount - 1;

            paintSkipUpdate = false;
            update(drawListBounds);
            update(drawInfoBounds);
        }
    }
}

void PlaybackBox::cursorUp(bool page, bool newview)
{
    if (inTitle == true || newview)
    {
        titleIndex -= (page ? 5 : 1);
        titleIndex += 5 * titleList.count();
        titleIndex = titleIndex % titleList.count();

        progIndex = 0;

        if (newview)
            inTitle = false;

        paintSkipUpdate = false;
        update(drawTotalBounds);
    }
    else 
    {
        if (progIndex > 0) 
        {
            progIndex -= (page ? listsize : 1);
            if (progIndex < 0)
                progIndex = 0;

            paintSkipUpdate = false;
            update(drawListBounds);
            update(drawInfoBounds);
        }
    }
}

void PlaybackBox::listChanged(void)
{
    if (playingSomething)
        return;

    connected = FillList();      
    paintSkipUpdate = false;
    update(drawTotalBounds);
    if (type == Delete)
        UpdateProgressBar();
}

bool PlaybackBox::FillList(bool useCachedData)
{
    ProgramInfo *p;

    // Save some information so we can find our place again.
    QString oldtitle = titleList[titleIndex];
    QString oldchanid;
    QString oldprogramid;
    QDate oldoriginalAirDate;
    QDateTime oldstartts;
    int oldrecpriority = 0;
    int oldrecordid = 0;

    p = progLists[oldtitle].at(progIndex);
    if (p)
    {
        oldchanid = p->chanid;
        oldstartts = p->recstartts;
        oldprogramid = p->programid;
        oldrecordid = p->recordid;
        oldoriginalAirDate = p->originalAirDate;
        oldrecpriority = p->recpriority;
    }

    QMap<QString, AvailableStatusType> asCache;
    QString asKey;
    for (unsigned int i = 0; i < progLists[""].count(); i++)
    {
        p = progLists[""].at(i);
        asKey = p->MakeUniqueKey();
        asCache[asKey] = p->availableStatus;
    }

    progsInDB = 0;
    titleList.clear();
    progLists.clear();
    // Clear autoDelete for the "all" list since it will share the
    // objects with the title lists.
    progLists[""].setAutoDelete(false);

    fillRecGroupPasswordCache();

    ViewTitleSort titleSort = (ViewTitleSort)gContext->GetNumSetting(
            "DisplayGroupTitleSort", TitleSortAlphabetical);

    QMap<QString, QString> sortedList;
    QMap<int, QString> searchRule;
    QMap<int, int> recidEpisodes;
    QString sTitle = "";

    bool LiveTVInAllPrograms = gContext->GetNumSetting("LiveTVInAllPrograms",0);

    if (!useCachedData || !progCache || progCache->empty())
    {
        clearProgramCache();

        progCache = RemoteGetRecordedList(allOrder == 0 || type == Delete);
    }

    if (progCache)
    {
        if ((viewMask & VIEW_SEARCHES))
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT recordid,title FROM record "
                          "WHERE search > 0 AND search != :MANUAL;");
            query.bindValue(":MANUAL", kManualSearch);

            if (query.exec() && query.isActive())
            {
                while (query.next())
                {
                    QString tmpTitle = query.value(1).toString();
                    tmpTitle.remove(QRegExp(" \\(.*\\)$"));
                    searchRule[query.value(0).toInt()] = tmpTitle;
                }
            }
        }

        sortedList[""] = "";
        vector<ProgramInfo *>::iterator i = progCache->begin();
        for ( ; i != progCache->end(); i++)
        {
            progsInDB++;
            p = *i;
            if ((((p->recgroup == recGroup) ||
                  ((recGroup == "All Programs") &&
                   (p->recgroup != "LiveTV" || LiveTVInAllPrograms))) &&
                 (recGroupPassword == curGroupPassword)) ||
                ((recGroupType[recGroup] == "category") &&
                 (p->category == recGroup ) &&
                 ( !recGroupPwCache.contains(p->recgroup))))
            {
                if (viewMask != VIEW_NONE)
                    progLists[""].prepend(p);

                asKey = p->MakeUniqueKey();
                if (asCache.contains(asKey))
                    p->availableStatus = asCache[asKey];
                else
                    p->availableStatus = asAvailable;

                if ((viewMask & VIEW_TITLES)) // Show titles
                {
                    sTitle = sortTitle(p->title, viewMask, titleSort,
                            p->recpriority);
                    sTitle = sTitle.lower();

                    if (!sortedList.contains(sTitle))
                        sortedList[sTitle] = p->title;
                    progLists[sortedList[sTitle]].prepend(p);
                    progLists[sortedList[sTitle]].setAutoDelete(false);
                } 

                if ((viewMask & VIEW_RECGROUPS) &&
                    p->recgroup != "") // Show recording groups
                {
                    sortedList[p->recgroup.lower()] = p->recgroup;
                    progLists[p->recgroup].prepend(p);
                    progLists[p->recgroup].setAutoDelete(false);
                }

                if ((viewMask & VIEW_CATEGORIES) &&
                    p->category != "") // Show categories
                {
                    sortedList[p->category.lower()] = p->category;
                    progLists[p->category].prepend(p);
                    progLists[p->category].setAutoDelete(false);
                }

                if ((viewMask & VIEW_SEARCHES) &&
                    searchRule[p->recordid] != "" &&
                    p->title != searchRule[p->recordid]) // Show search rules
                {
                    QString tmpTitle = QString("(%1)")
                                               .arg(searchRule[p->recordid]);
                    sortedList[tmpTitle] = tmpTitle;
                    progLists[tmpTitle].prepend(p);
                    progLists[tmpTitle].setAutoDelete(false);
                }

                if ((viewMask & VIEW_WATCHLIST))
                {
                    if (watchListAutoExpire && !p->GetAutoExpireFromRecorded())
                    {
                        p->recpriority2 = wlExpireOff;
                        VERBOSE(VB_FILE, QString("Auto-expire off:  %1")
                                                 .arg(p->title));
                    }
                    else if (p->programflags & FL_WATCHED)
                    {
                        p->recpriority2 = wlWatched;
                        VERBOSE(VB_FILE, QString("Marked as 'watched':  %1")
                                                 .arg(p->title));
                    }
                    else
                    {
                        if (p->recordid > 0)
                            recidEpisodes[p->recordid] += 1;
                        if (recidEpisodes[p->recordid] == 1 ||
                            p->recordid == 0 )
                        {
                            sortedList[watchGroup] = watchGroup;
                            progLists[watchGroup].prepend(p);
                            progLists[watchGroup].setAutoDelete(false);
                        }
                        else
                        {
                            p->recpriority2 = wlEarlier;
                            VERBOSE(VB_FILE, QString("Not the earliest:  %1")
                                                     .arg(p->title));
                        }
                    }
                }
            }
        }
    }

    if (sortedList.count() == 0)
    {
        progLists[""];
        titleList << "";
        progIndex = 0;
        titleIndex = 0;
        playList.clear();

        return 0;
    }

    QString episodeSort = gContext->GetSetting("PlayBoxEpisodeSort", "Date");

    if (episodeSort == "OrigAirDate")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = progLists.begin(); Iprog != progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
            {
                if (listOrder == 0 || type == Delete)
                    Iprog.data().Sort(comp_originalAirDate_rev);
                else
                    Iprog.data().Sort(comp_originalAirDate);
            }
        }
    }
    else if (episodeSort == "Id")
    {
        QMap<QString, ProgramList>::Iterator Iprog;
        for (Iprog = progLists.begin(); Iprog != progLists.end(); ++Iprog)
        {
            if (!Iprog.key().isEmpty())
            {
                if (listOrder == 0 || type == Delete)
                    Iprog.data().Sort(comp_programid_rev);
                else
                    Iprog.data().Sort(comp_programid);
            }
        }
    }
    else if (episodeSort == "Date")
    {
        QMap<QString, ProgramList>::iterator it;
        for (it = progLists.begin(); it != progLists.end(); ++it)
        {
            if (!it.key().isEmpty())
            {
                if (!listOrder || type == Delete)
                    (*it).Sort(comp_recordDate_rev);
                else
                    (*it).Sort(comp_recordDate);
            }
        }
    }

    if (progLists[watchGroup].count() > 1)
    {
        QDateTime now = QDateTime::currentDateTime();
        int maxAge = watchListMaxAge;

        QMap<int, int> recType;
        QMap<int, int> maxEpisodes;
        QMap<int, int> spanHours;
        QMap<int, int> delHours;
        QMap<int, int> nextHours;

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recordid, type, maxepisodes, "
                      "next_record, last_record, last_delete FROM record;");

        if (query.exec() && query.isActive())
        {
            while (query.next())
            {
                int recid = query.value(0).toInt();
                recType[recid] = query.value(1).toInt();
                maxEpisodes[recid] = query.value(2).toInt();

                QDateTime next_record = query.value(3).toDateTime();
                QDateTime last_record = query.value(4).toDateTime();
                QDateTime last_delete = query.value(5).toDateTime();

                // Time between the last and next recordings
                spanHours[recid] = 1000;
                if (last_record.isValid() && next_record.isValid())
                    spanHours[recid] =
                        last_record.secsTo(next_record) / 3600 + 1;

                // Time since the last episode was deleted
                delHours[recid] = 1000;
                if (last_delete.isValid())
                    delHours[recid] = last_delete.secsTo(now) / 3600 + 1;

                // Time until the next recording if any
                if (next_record.isValid())
                    nextHours[recid] = now.secsTo(next_record) / 3600 + 1;
            }
        }

        ProgramInfo *p;
        p = progLists[watchGroup].first(); 
        while (p)
        {
            int recid = p->recordid;

            // Set the intervals beyond range if there is no record entry
            if (spanHours[recid] == 0)
            {
                spanHours[recid] = 1000;
                delHours[recid] = 1000;
            }

            // add point equal to maxAge for each additional episode
            if (p->recordid == 0 || maxEpisodes[recid] > 0)
                p->recpriority2 = 0;
            else
                p->recpriority2 = (recidEpisodes[p->recordid] - 1) * maxAge;

            // add a point every 2hr leading up to the next recording
            if (nextHours[recid] > 0 && nextHours[recid] < maxAge * 2)
                p->recpriority2 += (maxAge * 2 - nextHours[recid]) / 2;

            int hrs = p->startts.secsTo(now) / 3600;

            // add points for a new recording that decrease each hour
            if (hrs < 24)
                p->recpriority2 += 24 - hrs;

            // Single
            if (recType[recid] == kSingleRecord || 
                recType[recid] == kFindOneRecord)
            {
                // add points for a new Singles that decrease over 2 days
                if (hrs < 48)
                    p->recpriority2 += maxAge * (48 - hrs) / 48;
            }
            // Daily 
            else if (spanHours[recid] < 50 ||
                     recType[recid] == kTimeslotRecord ||
                     recType[recid] == kFindDailyRecord)
            {
                if (delHours[recid] < watchListBlackOut * 4)
                {
                    p->recpriority2 = wlDeleted;
                    VERBOSE(VB_FILE, QString("Recently deleted daily:  %1")
                                             .arg(p->title));
                    progLists[watchGroup].remove();
                    p = progLists[watchGroup].current();
                    continue;
                }
                else
                {
                    VERBOSE(VB_FILE, QString("Daily interval:  %1")
                                             .arg(p->title));

                    if (maxEpisodes[recid] > 0)
                        p->recpriority2 += maxAge / 2 + p->startts.daysTo(now);
                    else
                        p->recpriority2 += maxAge / 3 + 
                                          (p->startts.secsTo(now) / 1800);
                }
            }
            // Weekly
            else if (nextHours[recid] ||
                     recType[recid] == kWeekslotRecord || 
                     recType[recid] == kFindWeeklyRecord)
                     
            {
                if (delHours[recid] < (watchListBlackOut * 24) - 4)
                {
                    p->recpriority2 = wlDeleted;
                    VERBOSE(VB_FILE, QString("Recently deleted weekly:  %1")
                                             .arg(p->title));
                    progLists[watchGroup].remove();
                    p = progLists[watchGroup].current();
                    continue;
                }
                else
                {
                    VERBOSE(VB_FILE, QString("Weekly interval: %1")
                                             .arg(p->title));

                    if (maxEpisodes[recid] > 0)
                        p->recpriority2 += maxAge / 2 + p->startts.daysTo(now);
                    else
                        p->recpriority2 += 
                            maxAge / 3 + (p->startts.daysTo(now) * maxAge / 4);
                }
            }
            // Not recurring
            else
            {
                if (delHours[recid] < (watchListBlackOut * 48) - 4)
                {
                    p->recpriority2 = wlDeleted;
                    progLists[watchGroup].remove();
                    p = progLists[watchGroup].current();
                    continue;
                }
                else
                {
                    if (p->startts.daysTo(now) < maxAge)
                        p->recpriority2 += p->startts.daysTo(now);
                    else
                        p->recpriority2 += maxAge;
                }
            }

            VERBOSE(VB_FILE, QString(" %1  %2  %3")
                    .arg(p->startts.toString(formatShortDate))
                    .arg(p->recpriority2).arg(p->title));

            p = progLists[watchGroup].next();
        }
        progLists[watchGroup].Sort(comp_recpriority2);
    }

    // Try to find our old place in the title list.  Scan the new
    // titles backwards until we find where we were or go past.  This
    // is somewhat inefficient, but it works.

    QStringList sTitleList = sortedList.keys();
    titleList = sortedList.values();

    QString oldsTitle = sortTitle(oldtitle, viewMask, titleSort,
            oldrecpriority);
    oldsTitle = oldsTitle.lower();
    titleIndex = titleList.count() - 1;
    for (titleIndex = titleList.count() - 1; titleIndex >= 0; titleIndex--)
    {
        sTitle = sTitleList[titleIndex];
        sTitle = sTitle.lower();
        
        if (oldsTitle > sTitle)
        {
            if (titleIndex + 1 < (int)titleList.count())
                titleIndex++;
            break;
        }

        if (oldsTitle == sTitle)
            break;
    }

    // Now do pretty much the same thing for the individual shows on
    // the specific program list if needed.
    if (oldtitle != titleList[titleIndex] || oldchanid.isNull())
        progIndex = 0;
    else
    {
        ProgramList *l = &progLists[oldtitle];
        progIndex = l->count() - 1;

        for (int i = progIndex; i >= 0; i--)
        {
            p = l->at(i);

            if (oldtitle != watchGroup)
            {
                if (titleIndex == 0)
                {
                    if (allOrder == 0 || type == Delete)
                    {
                        if (oldstartts > p->recstartts)
                            break;
                    }
                    else
                    {
                        if (oldstartts < p->recstartts)
                            break;  
                    }
                }
                else
                {
                    if (!listOrder || type == Delete)
                    {
                        if (episodeSort == "OrigAirDate")
                        {
                            if (oldoriginalAirDate > p->originalAirDate)
                                break;
                        }
                        else if (episodeSort == "Id")
                        {
                            if (oldprogramid > p->programid)
                                break;
                        }
                        else 
                        {
                            if (oldstartts > p->recstartts)
                                break;
                        }
                    }
                    else
                    {
                        if (episodeSort == "OrigAirDate")
                        {
                            if (oldoriginalAirDate < p->originalAirDate)
                                break;
                        }
                        else if (episodeSort == "Id")
                        {
                            if (oldprogramid < p->programid)
                                break;
                        }
                        else
                        {
                            if (oldstartts < p->recstartts)
                                break;
                        }
                    }
                }
            }
            progIndex = i;

            if (oldchanid == p->chanid &&
                oldstartts == p->recstartts)
                break;
        }
    }

    return (progCache != NULL);
}

static void *SpawnPreviewVideoThread(void *param)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)param;
    nvp->StartPlaying();
    return NULL;
}

bool PlaybackBox::killPlayer(void)
{
    QMutexLocker locker(&previewVideoUnsafeKillLock);

    previewVideoPlaying = false;

    /* if we don't have nvp to deal with then we are done */
    if (!previewVideoNVP)
    {
        previewVideoKillState = kDone;
        return true;
    }

    if (previewVideoKillState == kDone)
    {
        previewVideoKillState = kNvpToPlay;
        previewVideoKillTimeout.start();
    }
 
    if (previewVideoKillState == kNvpToPlay)
    {
        if (previewVideoNVP->IsPlaying() ||
            (previewVideoKillTimeout.elapsed() > 2000))
        {
            previewVideoKillState = kNvpToStop;

            previewVideoRingBuf->Pause();
            previewVideoNVP->StopPlaying();
        }
        else /* return false status since we aren't done yet */
            return false;
    }

    if (previewVideoKillState == kNvpToStop)
    {
        if (!previewVideoNVP->IsPlaying() ||
            (previewVideoKillTimeout.elapsed() > 2000))
        {
            pthread_join(previewVideoThread, NULL);
            previewVideoThreadRunning = true;
            delete previewVideoNVP;
            delete previewVideoRingBuf;

            previewVideoNVP = NULL;
            previewVideoRingBuf = NULL;
            previewVideoKillState = kDone;
        }
        else /* return false status since we aren't done yet */
            return false;
    }

    return true;
}        

void PlaybackBox::startPlayer(ProgramInfo *rec)
{
    previewVideoPlaying = true;

    if (rec != NULL)
    {
        // Don't keep trying to open broken files when just sitting on entry
        if (previewVideoBrokenRecId &&
            previewVideoBrokenRecId == rec->recordid)
        {
            return;
        }

        if (previewVideoRingBuf || previewVideoNVP)
        {
            VERBOSE(VB_IMPORTANT,
                    "PlaybackBox::startPlayer(): Error, last preview window "
                    "didn't clean up. Not starting a new preview.");
            return;
        }

        previewVideoRingBuf = new RingBuffer(rec->pathname, false, false, 1);
        if (!previewVideoRingBuf->IsOpen())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not open file for preview video.");
            delete previewVideoRingBuf;
            previewVideoRingBuf = NULL;
            previewVideoBrokenRecId = rec->recordid;
            return;
        }
        previewVideoBrokenRecId = 0;

        previewVideoNVP = new NuppelVideoPlayer("preview player");
        previewVideoNVP->SetRingBuffer(previewVideoRingBuf);
        previewVideoNVP->SetAsPIP();
        QString filters = "";
        previewVideoNVP->SetVideoFilters(filters);

        previewVideoThreadRunning = true;
        pthread_create(&previewVideoThread, NULL,
                       SpawnPreviewVideoThread, previewVideoNVP);

        previewVideoPlayingTimer.start();

        previewVideoState = kStarting;

        int previewRate = 30;
        if (gContext->GetNumSetting("PlaybackPreviewLowCPU", 0))
        {
            previewRate = 12;
        }
        
        previewVideoRefreshTimer->start(1000 / previewRate); 
    }
}

void PlaybackBox::playSelectedPlaylist(bool random)
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    ProgramInfo *tmpItem;
    QStringList::Iterator it = playList.begin();
    QStringList randomList = playList;
    bool playNext = true;
    int i = 0;
   
    while (randomList.count() && playNext)
    {
        if (random)
            i = (int)(1.0 * randomList.count() * rand() / (RAND_MAX + 1.0));

        it = randomList.at(i);
        tmpItem = findMatchingProg(*it);
        if (tmpItem)
        {
            ProgramInfo *rec = new ProgramInfo(*tmpItem);
            if (rec->availableStatus == asAvailable)
                playNext = play(rec, true);
            delete rec;
        }
        randomList.remove(it);
    }
}

void PlaybackBox::playSelected()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    if (m_player && m_player->IsSameProgram(curitem))
        exitWin();

    if (curitem && curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else
        play(curitem);

    if (m_player)
    {
        pbbIsVisibleCond.wakeAll();
        accept();
    }
}

void PlaybackBox::stopSelected()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    stop(curitem);
}

void PlaybackBox::deleteSelected()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    if ((curitem->availableStatus != asPendingDelete) &&
        (REC_CAN_BE_DELETED(curitem)))
        remove(curitem);
    else
        showAvailablePopup(curitem);
}

void PlaybackBox::upcoming()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
        return;
    }

    ProgLister *pl = new ProgLister(plTitle, curitem->title, "",
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void PlaybackBox::customEdit()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
        return;
    }
    ProgramInfo *pi = curitem;

    if (!pi)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                    "customedit", pi);
    ce->exec();
    delete ce;
}

void PlaybackBox::details()
{
    previewVideoState = kStopping;

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
        showAvailablePopup(curitem);
    else
        curitem->showDetails();
}

void PlaybackBox::selected()
{
    previewVideoState = kStopping;

    if (inTitle && haveGroupInfoSet)
    {
        cursorRight();
        return;
    }    

    if (!curitem)
        return;

    switch (type) 
    {
        case Play: playSelected(); break;
        case Delete: deleteSelected(); break;
    }
}

void PlaybackBox::showMenu()
{
    killPlayerSafe();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "menu popup");

    QLabel *label = popup->addLabel(tr("Recording List Menu"),
                                  MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *topButton = popup->addButton(tr("Change Group Filter"), this,
                     SLOT(showRecGroupChooser()));

    popup->addButton(tr("Change Group View"), this, 
                     SLOT(showViewChanger()));

    if (recGroupType[recGroup] == "recgroup")
        popup->addButton(tr("Change Group Password"), this,
                         SLOT(showRecGroupPasswordChanger()));

    if (playList.count())
    {
        popup->addButton(tr("Playlist options"), this,
                         SLOT(showPlaylistPopup()));
    }
    else if (!m_player)
    {
        if (inTitle)
        {
            popup->addButton(tr("Add this Group to Playlist"), this,
                             SLOT(togglePlayListTitle()));
        }
        else if (curitem && curitem->availableStatus == asAvailable)
        {
            popup->addButton(tr("Add this recording to Playlist"), this,
                             SLOT(togglePlayListItem()));
        }
    }

    popup->ShowPopup(this, SLOT(doCancel()));

    topButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showActionsSelected()
{
    if (!curitem)
        return;

    if (inTitle && haveGroupInfoSet)
        return;

    if ((curitem->availableStatus != asAvailable) &&
        (curitem->availableStatus != asFileNotFound))
        showAvailablePopup(curitem);
    else
        showActions(curitem);
}

bool PlaybackBox::play(ProgramInfo *rec, bool inPlaylist)
{
    bool playCompleted = false;

    if (!rec)
        return false;

    if (m_player)
        return true;
            
    if (fileExists(rec) == false)
    {
        QString msg = 
            QString("PlaybackBox::play(): Error, %1 file not found")
            .arg(rec->pathname);
        VERBOSE(VB_IMPORTANT, msg);

        ProgramInfo *tmpItem = findMatchingProg(rec);
        if (tmpItem)
        {
            tmpItem->availableStatus = asFileNotFound;
            showAvailablePopup(tmpItem);
        }

        return false;
    }

    ProgramInfo *tvrec = new ProgramInfo(*rec);

    setEnabled(false);
    previewVideoState = kKilling; // stop preview playback and don't restart it
    playingSomething = true;

    playCompleted = TV::StartTV(tvrec, false, inPlaylist, underNetworkControl);

    playingSomething = false;
    setEnabled(true);


    previewVideoState = kStarting; // restart playback preview

    delete tvrec;

    connected = FillList();

    return playCompleted;
}

void PlaybackBox::stop(ProgramInfo *rec)
{
    RemoteStopRecording(rec);
}

bool PlaybackBox::doRemove(ProgramInfo *rec, bool forgetHistory,
                           bool forceMetadataDelete)
{
    previewVideoBrokenRecId = rec->recordid;
    killPlayerSafe();

    if (playList.grep(rec->MakeUniqueKey()).count())
        togglePlayListItem(rec);

    if (!forceMetadataDelete)
        rec->UpdateLastDelete(true);

    return RemoteDeleteRecording(rec, forgetHistory, forceMetadataDelete);
}

void PlaybackBox::remove(ProgramInfo *toDel)
{
    previewVideoState = kStopping;

    if (delitem)
        delete delitem;

    delitem = new ProgramInfo(*toDel);
    showDeletePopup(delitem, DeleteRecording);
}

void PlaybackBox::showActions(ProgramInfo *toExp)
{
    killPlayer();

    if (delitem)
        delete delitem;

    delitem = new ProgramInfo(*toExp);

    if (fileExists(delitem) == false)
    {
        QString msg =
            QString("PlaybackBox::showActions(): Error, %1 file not found")
            .arg(delitem->pathname);
        VERBOSE(VB_IMPORTANT, msg);

        ProgramInfo *tmpItem = findMatchingProg(delitem);
        if (tmpItem)
        {
            tmpItem->availableStatus = asFileNotFound;
            showFileNotFoundActionPopup(delitem);
        }
    }
    else if (delitem->availableStatus != asAvailable)
        showAvailablePopup(delitem);
    else
        showActionPopup(delitem);
}

void PlaybackBox::showDeletePopup(ProgramInfo *program, deletePopupType types)
{
    freeSpaceNeedsUpdate = true;

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "delete popup");
    QString message1 = "";
    QString message2 = "";
    switch (types)
    {
        case EndOfRecording:
             message1 = tr("You have finished watching:"); break;
        case DeleteRecording:
             message1 = tr("Are you sure you want to delete:"); break;
        case ForceDeleteRecording:
             message1 = tr("ERROR: Recorded file does not exist.");
             message2 = tr("Are you sure you want to delete:");
             break;
        case StopRecording:
             message1 = tr("Are you sure you want to stop:"); break;
    }
    
    initPopup(popup, program, message1, message2);

    QString tmpmessage;
    const char *tmpslot = NULL;

    if ((types == EndOfRecording || types == DeleteRecording) &&
        (program->IsSameProgram(*program)) &&
        (program->recgroup != "LiveTV"))
    {
        if (types == EndOfRecording)
            tmpmessage = tr("Delete it, but allow it to re-record"); 
        else
            tmpmessage = tr("Yes, and allow re-record"); 
        tmpslot = SLOT(doDeleteForgetHistory());
        popup->addButton(tmpmessage, this, tmpslot);
    }

    switch (types)
    {
        case EndOfRecording:
             tmpmessage = tr("Delete it"); 
             tmpslot = SLOT(doDelete());
             break;
        case DeleteRecording:
             tmpmessage = tr("Yes, delete it"); 
             tmpslot = SLOT(doDelete());
             break;
        case ForceDeleteRecording:
             tmpmessage = tr("Yes, delete it"); 
             tmpslot = SLOT(doForceDelete());
             break;
        case StopRecording:
             tmpmessage = tr("Yes, stop recording it"); 
             tmpslot = SLOT(doStop());
             break;
    }
    
    QButton *yesButton = popup->addButton(tmpmessage, this, tmpslot);

    switch (types)
    {
        case EndOfRecording:
             tmpmessage = tr("Save it so I can watch it again"); 
             tmpslot = SLOT(noDelete());
             break;
        case DeleteRecording:
        case ForceDeleteRecording:
             tmpmessage = tr("No, keep it, I changed my mind"); 
             tmpslot = SLOT(noDelete());
             break;
        case StopRecording:
             tmpmessage = tr("No, continue recording it"); 
             tmpslot = SLOT(noStop());
             break;
    }
    QButton *noButton = popup->addButton(tmpmessage, this, tmpslot);

    if (types == EndOfRecording || types == DeleteRecording ||
        types == ForceDeleteRecording)
        noButton->setFocus();
    else
    {
        if (program->GetAutoExpireFromRecorded())
            yesButton->setFocus();
        else
            noButton->setFocus();
    }
 
    popup->ShowPopup(this, SLOT(doCancel())); 

    expectingPopup = true;
}

void PlaybackBox::showAvailablePopup(ProgramInfo *rec)
{
    if (!rec)
        return;

    QString msg = rec->title + "\n";
    if (rec->subtitle != "")
        msg += rec->subtitle + "\n";
    msg += "\n";

    switch (rec->availableStatus)
    {
        case asAvailable:
                 if (rec->programflags & (FL_INUSERECORDING | FL_INUSEPLAYING))
                 {
                     QString byWho;
                     rec->IsInUse(byWho);

                     MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                   QObject::tr("Recording Available"), msg +
                                   QObject::tr("This recording is currently in "
                                               "use by:") + "\n" + byWho);
                 }
                 else
                 {
                     MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                   QObject::tr("Recording Available"), msg +
                                   QObject::tr("This recording is currently "
                                               "Available"));
                 }
                 break;
        case asPendingDelete:
                 MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                               QObject::tr("Recording Unavailable"), msg +
                               QObject::tr("This recording is currently being "
                                           "deleted and is unavailable"));
                 break;
        case asFileNotFound:
                 MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                               QObject::tr("Recording Unavailable"), msg +
                               QObject::tr("The file for this recording can "
                                           "not be found"));
                 break;
    }
}

void PlaybackBox::showPlaylistPopup()
{
    if (expectingPopup)
       cancelPopup();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "playlist popup");

    QLabel *tlabel = NULL;
    if (playList.count() > 1)
    {
        tlabel = popup->addLabel(tr("There are %1 items in the playlist.")
                                       .arg(playList.count()));
    } else {
        tlabel = popup->addLabel(tr("There is %1 item in the playlist.")
                                       .arg(playList.count()));
    }
    tlabel->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *playButton = popup->addButton(tr("Play"), this, SLOT(doPlayList()));
    
    popup->addButton(tr("Shuffle Play"), this, SLOT(doPlayListRandom()));
    popup->addButton(tr("Clear Playlist"), this, SLOT(doClearPlaylist()));
 
    if (inTitle)
    {
        if ((viewMask & VIEW_TITLES))
            popup->addButton(tr("Toggle playlist for this Category/Title"),
                             this, SLOT(togglePlayListTitle()));
        else 
            popup->addButton(tr("Toggle playlist for this Recording Group"),
                             this, SLOT(togglePlayListTitle()));
    }
    else
    {
        popup->addButton(tr("Toggle playlist for this recording"), this,
                         SLOT(togglePlayListItem()));
    }

    QLabel *label = popup->addLabel(
                        tr("These actions affect all items in the playlist"));
    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    popup->addButton(tr("Change Recording Group"), this,
                     SLOT(doPlaylistChangeRecGroup()));
    popup->addButton(tr("Change Playback Group"), this,
                     SLOT(doPlaylistChangePlayGroup()));
    popup->addButton(tr("Job Options"), this,
                     SLOT(showPlaylistJobPopup()));
    popup->addButton(tr("Delete"), this, SLOT(doPlaylistDelete()));

    playButton->setFocus();

    popup->ShowPopup(this, SLOT(doCancel()));
  
    expectingPopup = true;
}

void PlaybackBox::showPlaylistJobPopup()
{
    if (expectingPopup)
       cancelPopup();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "playlist popup");

    QLabel *tlabel = NULL;
    if (playList.count() > 1)
    {
        tlabel = popup->addLabel(tr("There are %1 items in the playlist.")
                                       .arg(playList.count()));
    } else {
        tlabel = popup->addLabel(tr("There is %1 item in the playlist.")
                                       .arg(playList.count()));
    }
    tlabel->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QButton *jobButton;
    QString jobTitle = "";
    QString command = "";
    QStringList::Iterator it;
    ProgramInfo *tmpItem;
    bool isTranscoding = true;
    bool isFlagging = true;
    bool isRunningUserJob1 = true;
    bool isRunningUserJob2 = true;
    bool isRunningUserJob3 = true;
    bool isRunningUserJob4 = true;

    for(it = playList.begin(); it != playList.end(); ++it)
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem) {
            if (!JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE,
                                       tmpItem->chanid, tmpItem->recstartts))
                isTranscoding = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG,
                                       tmpItem->chanid, tmpItem->recstartts))
                isFlagging = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob1 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob2 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob3 = false;
            if (!JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4,
                                       tmpItem->chanid, tmpItem->recstartts))
                isRunningUserJob4 = false;
            if (!isTranscoding && !isFlagging && !isRunningUserJob1 &&
                !isRunningUserJob2 && !isRunningUserJob3 && !isRunningUserJob4)
                break;
        }
    }
    if (!isTranscoding)
        jobButton = popup->addButton(tr("Begin Transcoding"), this,
                         SLOT(doPlaylistBeginTranscoding()));
    else
        jobButton = popup->addButton(tr("Stop Transcoding"), this,
                         SLOT(stopPlaylistTranscoding()));
    if (!isFlagging)
        popup->addButton(tr("Begin Commercial Flagging"), this,
                         SLOT(doPlaylistBeginFlagging()));
    else
        popup->addButton(tr("Stop Commercial Flagging"), this,
                         SLOT(stopPlaylistFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc1");

        if (!isRunningUserJob1)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob1()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob1()));
    }
    
    command = gContext->GetSetting("UserJob2", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc2");

        if (!isRunningUserJob2)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob2()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob2()));
    }
    
    command = gContext->GetSetting("UserJob3", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc3");

        if (!isRunningUserJob3)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob3()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob3()));
    }
    
    command = gContext->GetSetting("UserJob4", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc4");

        if (!isRunningUserJob4)
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doPlaylistBeginUserJob4()));
        else
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(stopPlaylistUserJob4()));
    }
    
    popup->ShowPopup(this, SLOT(doCancel()));
    jobButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showPlayFromPopup()
{
    if (expectingPopup)
        cancelPopup();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "playfrom popup");

    initPopup(popup, delitem, "", "");

    QButton *playButton = popup->addButton(tr("Play from bookmark"), this,
            SLOT(doPlay()));
    popup->addButton(tr("Play from beginning"), this, SLOT(doPlayFromBeg()));
     
    popup->ShowPopup(this, SLOT(doCancel()));
    playButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showStoragePopup()
{
    if (expectingPopup)
        cancelPopup();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "storage popup");

    initPopup(popup, delitem, "", "");

    QButton *storageButton;

    MythPushButton *toggleButton;
    
    storageButton = popup->addButton(tr("Change Recording Group"), this,
                                     SLOT(showRecGroupChanger()));

    popup->addButton(tr("Change Playback Group"), this,
                     SLOT(showPlayGroupChanger()));

    if (delitem)
    {
        toggleButton = new MythPushButton(
            tr("Disable Auto Expire"), tr("Enable Auto Expire"),
            popup, delitem->GetAutoExpireFromRecorded());
        connect(toggleButton, SIGNAL(toggled(bool)), this,
                SLOT(toggleAutoExpire(bool)));
        popup->addWidget(toggleButton, false);

        toggleButton = new MythPushButton(
            tr("Do not preserve this episode"), tr("Preserve this episode"),
            popup, delitem->GetPreserveEpisodeFromRecorded());
        connect(toggleButton, SIGNAL(toggled(bool)), this,
                SLOT(togglePreserveEpisode(bool)));
        popup->addWidget(toggleButton, false);
    }

    popup->ShowPopup(this, SLOT(doCancel()));
    storageButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showRecordingPopup()
{
    if (expectingPopup)
        cancelPopup();

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "recording popup");

    initPopup(popup, delitem, "", "");

    QButton *editButton = popup->addButton(tr("Edit Recording Schedule"), this,
                     SLOT(doEditScheduled()));

    popup->addButton(tr("Show Program Details"), this,
                     SLOT(showProgramDetails()));

    popup->addButton(tr("Change Recording Title"), this,
                     SLOT(showRecTitleChanger()));
    
    popup->ShowPopup(this, SLOT(doCancel()));
    editButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showJobPopup()
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "job popup");

    initPopup(popup, delitem, "", "");

    QButton *jobButton;
    QString jobTitle = "";
    QString command = "";

    if (JobQueue::IsJobQueuedOrRunning(JOB_TRANSCODE, curitem->chanid,
                                                  curitem->recstartts))
        jobButton = popup->addButton(tr("Stop Transcoding"), this,
                         SLOT(doBeginTranscoding()));
    else
        jobButton = popup->addButton(tr("Begin Transcoding"), this,
                         SLOT(showTranscodingProfiles()));

    if (JobQueue::IsJobQueuedOrRunning(JOB_COMMFLAG, curitem->chanid,
                                                  curitem->recstartts))
        popup->addButton(tr("Stop Commercial Flagging"), this,
                         SLOT(doBeginFlagging()));
    else
        popup->addButton(tr("Begin Commercial Flagging"), this,
                         SLOT(doBeginFlagging()));

    command = gContext->GetSetting("UserJob1", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc1", tr("User Job") + " #1");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB1, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob1()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob1()));
    }

    command = gContext->GetSetting("UserJob2", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc2", tr("User Job") + " #2");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB2, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob2()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob2()));
    }

    command = gContext->GetSetting("UserJob3", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc3", tr("User Job") + " #3");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB3, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob3()));
        else
            popup->addButton(tr("Begin") + " " + jobTitle, this,
                             SLOT(doBeginUserJob3()));
    }

    command = gContext->GetSetting("UserJob4", "");
    if (command != "") {
        jobTitle = gContext->GetSetting("UserJobDesc4", tr("User Job") + " #4");

        if (JobQueue::IsJobQueuedOrRunning(JOB_USERJOB4, curitem->chanid,
                                   curitem->recstartts))
            popup->addButton(tr("Stop") + " " + jobTitle, this,
                             SLOT(doBeginUserJob4()));
        else
            popup->addButton(tr("Begin") + " "  + jobTitle, this,
                             SLOT(doBeginUserJob4()));
    }

    popup->ShowPopup(this, SLOT(doCancel()));
    jobButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::showTranscodingProfiles()
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "transcode popup");

    initPopup(popup, delitem, "", "");

    QButton *defaultButton;

    defaultButton = popup->addButton(tr("Default"), this,
                                 SLOT(doBeginTranscoding()));
    popup->addButton(tr("Autodetect"), this,
                     SLOT(changeProfileAndTranscodeAuto()));
    popup->addButton(tr("High Quality"), this,
                     SLOT(changeProfileAndTranscodeHigh()));
    popup->addButton(tr("Medium Quality"), this,
                     SLOT(changeProfileAndTranscodeMedium()));
    popup->addButton(tr("Low Quality"), this,
                     SLOT(changeProfileAndTranscodeLow()));

    popup->ShowPopup(this, SLOT(doCancel()));
    defaultButton->setFocus();
    
    expectingPopup = true;
}

void PlaybackBox::changeProfileAndTranscode(QString profile)
{
    curitem->ApplyTranscoderProfileChange(profile);
    doBeginTranscoding();
}

void PlaybackBox::showActionPopup(ProgramInfo *program)
{
    if (!curitem || !program)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "action popup");

    initPopup(popup, program, "", "");

    QButton *playButton = new QButton();

    if (!(m_player && m_player->IsSameProgram(curitem)))
    {
        if (curitem->programflags & FL_BOOKMARK)
            popup->addButton(tr("Play from..."), this,
                                        SLOT(showPlayFromPopup()));
        else
            popup->addButton(tr("Play"), this, SLOT(doPlay()));
    }

    if (!m_player)
    {
        if (playList.grep(curitem->MakeUniqueKey()).count())
            popup->addButton(tr("Remove from Playlist"), this,
                            SLOT(togglePlayListItem()));
        else
            popup->addButton(tr("Add to Playlist"), this,
                            SLOT(togglePlayListItem()));
    }

    if (program->recstatus == rsRecording)
        popup->addButton(tr("Stop Recording"), this, SLOT(askStop()));

    if (curitem->programflags & FL_WATCHED)
        popup->addButton(tr("Mark as Unwatched"), this,
                                    SLOT(setUnwatched()));
    else
        popup->addButton(tr("Mark as Watched"), this,
                                    SLOT(setWatched()));

    popup->addButton(tr("Storage Options"), this, SLOT(showStoragePopup()));
    popup->addButton(tr("Recording Options"), this, SLOT(showRecordingPopup()));
    popup->addButton(tr("Job Options"), this, SLOT(showJobPopup()));

    if (!(m_player && m_player->IsSameProgram(curitem)))
        popup->addButton(tr("Delete"), this, SLOT(askDelete()));

    popup->ShowPopup(this, SLOT(doCancel()));

    playButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::showFileNotFoundActionPopup(ProgramInfo *program)
{
    if (!curitem || !program)
        return;

    popup = new MythPopupBox(gContext->GetMainWindow(), drawPopupSolid,
                             drawPopupFgColor, drawPopupBgColor,
                             drawPopupSelColor, "action popup");

    QString msg = QObject::tr("Recording Unavailable") + "\n";
    msg += QObject::tr("The file for this recording can "
                       "not be found") + "\n";

    initPopup(popup, program, "", msg);

    QButton *detailsButton;
    detailsButton = popup->addButton(tr("Show Program Details"), this,
                                     SLOT(showProgramDetails()));

    popup->addButton(tr("Delete"), this, SLOT(askDelete()));

    popup->ShowPopup(this, SLOT(doCancel()));

    detailsButton->setFocus();

    expectingPopup = true;
}

void PlaybackBox::initPopup(MythPopupBox *popup, ProgramInfo *program,
                            QString message, QString message2)
{
    killPlayerSafe();

    QDateTime recstartts = program->recstartts;
    QDateTime recendts = program->recendts;

    QString timedate = recstartts.date().toString(formatLongDate) + QString(", ") +
                       recstartts.time().toString(formatTime) + QString(" - ") +
                       recendts.time().toString(formatTime);

    QString descrip = program->description;
    descrip = cutDownString(descrip, &defaultMediumFont, (int)(width() / 2));
    QString titl = program->title;
    titl = cutDownString(titl, &defaultBigFont, (int)(width() / 2));

    if (message.stripWhiteSpace().length() > 0)
        popup->addLabel(message);

    popup->addLabel(program->title, MythPopupBox::Large);

    if ((program->subtitle).stripWhiteSpace().length() > 0)
        popup->addLabel("\"" + program->subtitle + "\"\n" + timedate);
    else
        popup->addLabel(timedate);

    if (message2.stripWhiteSpace().length() > 0)
        popup->addLabel(message2);
}

void PlaybackBox::cancelPopup(void)
{
    popup->hide();
    expectingPopup = false;

    delete popup;
    popup = NULL;

    paintSkipUpdate = false;
    paintSkipCount = 2;

    setActiveWindow();
}

void PlaybackBox::doClearPlaylist(void)
{
    if (expectingPopup)
        cancelPopup();

    playList.clear();
}

void PlaybackBox::doPlay(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (curitem)
        delete curitem;

    curitem = new ProgramInfo(*delitem);

    playSelected();
}

void PlaybackBox::doPlayFromBeg(void)
{
    delitem->setIgnoreBookmark(true);
    doPlay();
}

void PlaybackBox::doPlayList(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    playSelectedPlaylist(false);
}


void PlaybackBox::doPlayListRandom(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    playSelectedPlaylist(true);
}

void PlaybackBox::askStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, StopRecording);
}

void PlaybackBox::noStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);
}

void PlaybackBox::doStop(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    stop(delitem);

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);
}

void PlaybackBox::showProgramDetails()
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (!curitem)
        return;

    curitem->showDetails();
} 

void PlaybackBox::doEditScheduled()
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
    }
    else
    {
        ScheduledRecording record;
        record.loadByProgram(curitem);
        record.exec();

        connected = FillList();
    }
}    

void PlaybackBox::doJobQueueJob(int jobType, int jobFlags)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    if (!curitem)
        return;

    ProgramInfo *tmpItem = findMatchingProg(curitem);

    if (JobQueue::IsJobQueuedOrRunning(jobType, curitem->chanid,
                               curitem->recstartts))
    {
        JobQueue::ChangeJobCmds(jobType, curitem->chanid,
                                curitem->recstartts, JOB_STOP);
        if ((jobType & JOB_COMMFLAG) && (tmpItem))
        {
            tmpItem->programflags &= ~FL_EDITING;
            tmpItem->programflags &= ~FL_COMMFLAG;
        }
    } else {
        QString jobHost = "";
        if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = curitem->hostname;

        JobQueue::QueueJob(jobType, curitem->chanid, curitem->recstartts,
                           "", "", jobHost, jobFlags);
    }
}

void PlaybackBox::doBeginFlagging()
{
    doJobQueueJob(JOB_COMMFLAG);
    update(drawListBounds);
}

void PlaybackBox::doPlaylistJobQueueJob(int jobType, int jobFlags)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    if (expectingPopup)
        cancelPopup();

    for (it = playList.begin(); it != playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem && 
            (!JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                    tmpItem->recstartts))) {
            QString jobHost = "";
            if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
                jobHost = tmpItem->hostname;

            JobQueue::QueueJob(jobType, tmpItem->chanid,
                               tmpItem->recstartts, "", "", jobHost, jobFlags);
        }
    }
}

void PlaybackBox::stopPlaylistJobQueueJob(int jobType)
{
    ProgramInfo *tmpItem;
    QStringList::Iterator it;
    int jobID;

    if (expectingPopup)
        cancelPopup();

    for (it = playList.begin(); it != playList.end(); ++it)
    {
        jobID = 0;
        tmpItem = findMatchingProg(*it);
        if (tmpItem &&
            (JobQueue::IsJobQueuedOrRunning(jobType, tmpItem->chanid,
                                tmpItem->recstartts)))
        {
            JobQueue::ChangeJobCmds(jobType, tmpItem->chanid,
                                     tmpItem->recstartts, JOB_STOP);
            if ((jobType & JOB_COMMFLAG) && (tmpItem))
            {
                tmpItem->programflags &= ~FL_EDITING;
                tmpItem->programflags &= ~FL_COMMFLAG;
            }
        }
    }
}

void PlaybackBox::askDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    showDeletePopup(delitem, DeleteRecording);
}

void PlaybackBox::noDelete(void)
{
    previewSuspend = false;
    if (!expectingPopup)
        return;

    cancelPopup();

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);
}

void PlaybackBox::doPlaylistDelete(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    ProgramInfo *tmpItem;
    QStringList::Iterator it;

    for (it = playList.begin(); it != playList.end(); ++it )
    {
        tmpItem = findMatchingProg(*it);
        if (tmpItem && (REC_CAN_BE_DELETED(tmpItem)))
            RemoteDeleteRecording(tmpItem, false, false);
    }

    connected = FillList();
    playList.clear();
}

void PlaybackBox::doDelete(void)
{
    if (!expectingPopup)
    {
        previewSuspend = false;
        return;
    }

    cancelPopup();

    if ((delitem->availableStatus == asPendingDelete) ||
        (!REC_CAN_BE_DELETED(delitem)))
    {
        showAvailablePopup(delitem);
        previewSuspend = false;
        return;
    }

    bool result = doRemove(delitem, false, false);

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);

    if (result)
    {
        ProgramInfo *tmpItem = findMatchingProg(delitem);
        if (tmpItem)
            tmpItem->availableStatus = asPendingDelete;
        previewSuspend = false;
    }
    else
        showDeletePopup(delitem, ForceDeleteRecording);
}

void PlaybackBox::doForceDelete(void)
{
    if (!expectingPopup)
    {
        previewSuspend = false;
        return;
    }

    cancelPopup();

    if ((delitem->availableStatus == asPendingDelete) ||
        (!REC_CAN_BE_DELETED(delitem)))
    {
        showAvailablePopup(delitem);
        previewSuspend = false;
        return;
    }

    doRemove(delitem, true, true);

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);
}

void PlaybackBox::doDeleteForgetHistory(void)
{
    if (!expectingPopup)
    {
        previewSuspend = false;
        return;
    }

    cancelPopup();

    if ((delitem->availableStatus == asPendingDelete) ||
        (!REC_CAN_BE_DELETED(delitem)))
    {
        showAvailablePopup(delitem);
        previewSuspend = false;
        return;
    }

    bool result = doRemove(delitem, true, false);

    previewVideoState = kChanging;

    previewVideoRefreshTimer->start(500);

    if (result)
    {
        ProgramInfo *tmpItem = findMatchingProg(delitem);
        if (tmpItem)
            tmpItem->availableStatus = asPendingDelete;
        previewSuspend = false;
    }
    else
        showDeletePopup(delitem, ForceDeleteRecording);
}

ProgramInfo *PlaybackBox::findMatchingProg(ProgramInfo *pginfo)
{
    ProgramInfo *p;
    ProgramList *l = &progLists[""];

    for (p = l->first(); p; p = l->next())
    {
        if (p->recstartts == pginfo->recstartts &&
            p->chanid == pginfo->chanid)
            return p;
    }

    return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(QString key)
{
    QStringList keyParts;

    if ((key == "") || (key.find('_') < 0))
        return NULL;

    keyParts = QStringList::split("_", key);

    // ProgramInfo::MakeUniqueKey() has 2 parts separated by '_' characters
    if (keyParts.count() == 2)
        return findMatchingProg(keyParts[0], keyParts[1]);
    else
        return NULL;
}

ProgramInfo *PlaybackBox::findMatchingProg(QString chanid, QString recstartts)
{
    ProgramInfo *p;
    ProgramList *l = &progLists[""];

    for (p = l->first(); p; p = l->next())
    {
        if (p->recstartts.toString(Qt::ISODate) == recstartts &&
            p->chanid == chanid)
            return p;
    }

    return NULL;
}

void PlaybackBox::setUnwatched(void)
{
    if (!expectingPopup && delitem)
        return;

    cancelPopup();

    delitem->SetWatchedFlag(0);

    ProgramInfo *tmpItem = findMatchingProg(delitem);
    if (tmpItem)
        tmpItem->programflags &= ~FL_WATCHED;

    delete delitem;
    delitem = NULL;

    previewVideoState = kChanging;

    connected = FillList();      
    update(drawListBounds);
}

void PlaybackBox::setWatched(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    delitem->SetWatchedFlag(1);

    ProgramInfo *tmpItem = findMatchingProg(delitem);
    if (tmpItem)
        tmpItem->programflags |= FL_WATCHED;

    delete delitem;
    delitem = NULL;

    previewVideoState = kChanging;

    connected = FillList();      
    update(drawListBounds);
}

void PlaybackBox::toggleAutoExpire(bool turnOn)
{
    if (!expectingPopup)
        return;

    ProgramInfo *tmpItem = findMatchingProg(delitem);

    if (tmpItem)
    {
        tmpItem->SetAutoExpire(turnOn);

        if (turnOn)
            tmpItem->programflags |= FL_AUTOEXP;
        else
            tmpItem->programflags &= ~FL_AUTOEXP;

        update(drawListBounds);
    }
}

void PlaybackBox::togglePreserveEpisode(bool turnOn)
{
    if (!expectingPopup)
        return;

    ProgramInfo *tmpItem = findMatchingProg(delitem);

    if (tmpItem)
    {
        if (tmpItem->availableStatus != asAvailable)
            showAvailablePopup(tmpItem);
        else
            tmpItem->SetPreserveEpisode(turnOn);
    }
}

void PlaybackBox::doCancel(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    previewVideoState = kChanging;
}

void PlaybackBox::toggleView(ViewMask itemMask, bool setOn)
{
    if (setOn)
        viewMask = (ViewMask)(viewMask | itemMask);
    else
        viewMask = (ViewMask)(viewMask & ~itemMask);

    connected = FillList(true);
    paintSkipUpdate = false;
    update(drawTotalBounds);
}

void PlaybackBox::promptEndOfRecording(ProgramInfo *rec)
{
    if (!rec)
        return;

    previewVideoState = kStopping;

    if (!rec)
        return;

    if (delitem)
        delete delitem;
        
    delitem = new ProgramInfo(*rec);
    showDeletePopup(delitem, EndOfRecording);
}

void PlaybackBox::UpdateProgressBar(void)
{
    update(drawUsageBounds);
}

void PlaybackBox::togglePlayListTitle(void)
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    QString currentTitle = titleList[titleIndex];
    ProgramInfo *p;

    for( unsigned int i = 0; i < progLists[currentTitle].count(); i++)
    {
        p = progLists[currentTitle].at(i);
        if (p && (p->availableStatus == asAvailable))
            togglePlayListItem(p);
    }

    if (inTitle)
        cursorRight();
}

void PlaybackBox::togglePlayListItem(void)
{
    if (expectingPopup)
        cancelPopup();

    if (!curitem)
        return;

    if (curitem->availableStatus != asAvailable)
    {
        showAvailablePopup(curitem);
        return;
    }

    togglePlayListItem(curitem);

    if (!inTitle)
        cursorDown();

    paintSkipUpdate = false;
    update(drawListBounds);
}

void PlaybackBox::togglePlayListItem(ProgramInfo *pginfo)
{
    if (!pginfo)
        return;

    if (pginfo->availableStatus != asAvailable)
    {
        showAvailablePopup(pginfo);
        return;
    }

    QString key = pginfo->MakeUniqueKey();

    if (playList.grep(key).count())
    {
        QStringList tmpList;
        QStringList::Iterator it;

        tmpList = playList;
        playList.clear();

        for (it = tmpList.begin(); it != tmpList.end(); ++it )
        {
            if (*it != key)
                playList << *it;
        }
    }
    else
    {
        playList << key;
    }

    paintSkipUpdate = false;
    update(drawListBounds);
}

void PlaybackBox::timeout(void)
{
    if (titleList.count() <= 1)
        return;

    if (previewVideoEnabled)
        update(drawVideoBounds);
}

void PlaybackBox::processNetworkControlCommands(void)
{
    int commands = 0;
    QString command;

    ncLock.lock();
    commands = networkControlCommands.size();
    ncLock.unlock();

    while (commands)
    {
        ncLock.lock();
        command = networkControlCommands.front();
        networkControlCommands.pop_front();
        ncLock.unlock();

        processNetworkControlCommand(command);

        ncLock.lock();
        commands = networkControlCommands.size();
        ncLock.unlock();
    }
}

void PlaybackBox::processNetworkControlCommand(QString command)
{
    QStringList tokens = QStringList::split(" ", command);

    if (tokens.size() >= 4 && (tokens[1] == "PLAY" || tokens[1] == "RESUME"))
    {
        if (tokens.size() == 5 && tokens[2] == "PROGRAM")
        {
            VERBOSE(VB_IMPORTANT,
                    QString("NetworkControl: Trying to %1 program '%2' @ '%3'")
                            .arg(tokens[1]).arg(tokens[3]).arg(tokens[4]));

            if (playingSomething)
            {
                VERBOSE(VB_IMPORTANT,
                        "NetworkControl: ERROR: Already playing");

                MythEvent me("NETWORK_CONTROL RESPONSE ERROR: Unable to play, "
                             "player is already playing another recording.");
                gContext->dispatch(me);
                return;
            }

            ProgramInfo *tmpItem =
                ProgramInfo::GetProgramFromRecorded(tokens[3], tokens[4]);

            if (tmpItem)
            {
                if (curitem)
                    delete curitem;

                curitem = tmpItem;
                curitem->pathname = curitem->GetPlaybackURL();

                MythEvent me("NETWORK_CONTROL RESPONSE OK");
                gContext->dispatch(me);

                if (tokens[1] == "PLAY")
                    curitem->setIgnoreBookmark(true);

                underNetworkControl = true;
                playSelected();
                underNetworkControl = false;
            }
            else
            {
                QString message = QString("NETWORK_CONTROL RESPONSE "
                                          "ERROR: Could not find recording for "
                                          "chanid %1 @ %2")
                                          .arg(tokens[3]).arg(tokens[4]);
                MythEvent me(message);
                gContext->dispatch(me);
            }
        }
    }
}

void PlaybackBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    
    if (ignoreKeyPressEvents)
        return;

    // This should be an impossible keypress we've simulated
    if ((e->key() == Qt::Key_LaunchMedia) &&
        (e->state() == (Qt::MouseButtonMask | Qt::KeyButtonMask)))
    {
        e->accept();
        ncLock.lock();
        int commands = networkControlCommands.size();
        ncLock.unlock();
        if (commands)
            processNetworkControlCommands();
        return;
    }

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            exitWin();
        else if (action == "1" || action == "HELP")
            showIconHelp();
        else if (action == "MENU")
            showMenu();
        else if (action == "NEXTFAV")
        {
            if (inTitle)
                togglePlayListTitle();
            else
                togglePlayListItem();
        }
        else if (action == "TOGGLEFAV")
        {
            playList.clear();
            connected = FillList(true);      
            paintSkipUpdate = false;
            update(drawTotalBounds);
        }
        else if (action == "TOGGLERECORD")
        {
            viewMask = viewMaskToggle(viewMask, VIEW_TITLES);
            connected = FillList(true);
            paintSkipUpdate = false;
            update(drawTotalBounds);
        }
        else if (action == "CHANGERECGROUP")
            showRecGroupChooser();
        else if (action == "CHANGEGROUPVIEW")
            showViewChanger();
        else if (titleList.count() > 1)
        {
            if (action == "DELETE")
                deleteSelected();
            else if (action == "PLAYBACK")
                playSelected();
            else if (action == "INFO")
                showActionsSelected();
            else if (action == "DETAILS")
                details();
            else if (action == "CUSTOMEDIT")
                customEdit();
            else if (action == "UPCOMING")
                upcoming();
            else if (action == "SELECT")
                selected();
            else if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "LEFT")
                cursorLeft();
            else if (action == "RIGHT")
                cursorRight();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "PREVVIEW")
                cursorUp(false, true);
            else if (action == "NEXTVIEW")
                cursorDown(false, true);
            else
                handled = false;
        }
        else
        { 
            if (action == "LEFT" && arrowAccel) 
                exitWin(); 
            else 
                handled = false; 
        } 
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void PlaybackBox::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();
 
        if (message == "RECORDING_LIST_CHANGE" && !fillListTimer->isActive())
            fillListTimer->start(1000, true);
        else if (message.left(15) == "NETWORK_CONTROL")
        {
            QStringList tokens = QStringList::split(" ", message);
            if ((tokens[1] != "ANSWER") && (tokens[1] != "RESPONSE"))
            {
                ncLock.lock();
                networkControlCommands.push_back(message);
                ncLock.unlock();

                // This should be an impossible keypress we're simulating
                QKeyEvent *event;
                int buttons = 
                        (Qt::MouseButtonMask | Qt::KeyButtonMask);
                
                event = new QKeyEvent(QEvent::KeyPress, Qt::Key_LaunchMedia,
                                      0, buttons);
                QApplication::postEvent((QObject*)(gContext->GetMainWindow()),
                                        event);

                event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_LaunchMedia,
                                      0, buttons);
                QApplication::postEvent((QObject*)(gContext->GetMainWindow()),
                                        event);
            }
        }
    }
}

bool PlaybackBox::fileExists(ProgramInfo *pginfo)
{
    if (pginfo->pathname.left(7) == "myth://")
    {
        bool ret = RemoteCheckFile(pginfo);
        return ret;
    }

    QFile checkFile(pginfo->pathname);

    return checkFile.exists();
}

QDateTime PlaybackBox::getPreviewLastModified(ProgramInfo *pginfo)
{
    QDateTime datetime;
    QString filename = pginfo->pathname + ".png";

    if (filename.left(7) != "myth://")
    {
        QFileInfo retfinfo(filename);
        if (retfinfo.exists())
            datetime = retfinfo.lastModified();
    }
    else
    {
        QString filetime = RemoteGetPreviewLastModified(pginfo);
        if (!filetime.isEmpty() && filetime.upper() != "BAD")
            datetime = QDateTime::fromString(filetime, Qt::TextDate);
    }

    return datetime;
}

/** \fn PlaybackBox::SetPreviewGenerator(const QString&, PreviewGenerator*)
 *  \brief Sets the PreviewGenerator for a specific file.
 *  \return true iff call succeeded.
 */
bool PlaybackBox::SetPreviewGenerator(const QString &xfn, PreviewGenerator *g)
{
    QString fn = xfn.mid(max(xfn.findRev('/'),0));
    if (!g)
    {
        if (previewGeneratorLock.tryLock())
        {
            previewGenerator.erase(fn);
            previewGeneratorLock.unlock();
            return true;
        }
        return false;
    }

    QMutexLocker locker(&previewGeneratorLock);
    g->AttachSignals(this);
    previewGenerator[fn] = g;
    g->Start();

    return true;
}

/** \fn PlaybackBox::IsGeneratingPreview(const QString&) const
 *  \brief Returns true if we have already started a
 *         PreviewGenerator to create this file.
 */
bool PlaybackBox::IsGeneratingPreview(const QString &xfn) const
{
    QMap<QString, PreviewGenerator*>::const_iterator it;
    QMutexLocker locker(&previewGeneratorLock);

    QString fn = xfn.mid(max(xfn.findRev('/'),0));
    if ((it = previewGenerator.find(fn)) == previewGenerator.end())
        return false;
    return *it;
}

/** \fn PlaybackBox::IncPreviewGeneratorAttempts(const QString&)
 *  \brief Increments and returns number of times we have
 *         started a PreviewGenerator to create this file.
 */
uint PlaybackBox::IncPreviewGeneratorAttempts(const QString &xfn)
{
    QMutexLocker locker(&previewGeneratorLock);
    QString fn = xfn.mid(max(xfn.findRev('/'),0));
    return previewGeneratorAttempts[fn]++;
}

void PlaybackBox::previewThreadDone(const QString &fn, bool &success)
{
    success = SetPreviewGenerator(fn, NULL);
}

/** \fn PlaybackBox::previewReady(const ProgramInfo*)
 *  \brief Callback used by PreviewGenerator to tell us a preview
 *         we requested has been returned from the backend.
 *  \param pginfo ProgramInfo describing the preview.
 */
void PlaybackBox::previewReady(const ProgramInfo *pginfo)
{
    // lock QApplication so that we don't remove pixmap
    // from under a running paint event.
    qApp->lock();

    // If we are still displaying this preview update it.
    if (pginfo->recstartts  == previewStartts &&
        pginfo->chanid      == previewChanid  &&
        previewLastModified == previewFilets  &&
        titleList.count() > 1)
    {
        if (previewPixmap)
        {
            delete previewPixmap;
            previewPixmap = NULL;
        }

        // ask for repaint
        update(drawVideoBounds);
    }
    qApp->unlock();
}

bool check_lastmod(LastCheckedMap &elapsedtime, const QString &filename)
{
    LastCheckedMap::iterator it = elapsedtime.find(filename);

    if (it != elapsedtime.end() && ((*it).elapsed() < 250))
        return false;

    elapsedtime[filename].restart();
    return true;
}

QPixmap PlaybackBox::getPixmap(ProgramInfo *pginfo)
{
    QPixmap retpixmap;

    if (!previewPixmapEnabled || !pginfo)
        return retpixmap;
        
    if ((asPendingDelete == pginfo->availableStatus) || previewSuspend)
    {
        if (previewPixmap)
            retpixmap = *previewPixmap;

        return retpixmap;
    }

    QString filename = pginfo->pathname + ".png";
    bool check_date = check_lastmod(previewLastModifyCheck, filename);

    if (check_date)
        previewLastModified = getPreviewLastModified(pginfo);

    if (previewFromBookmark &&
        check_date &&
        (!previewLastModified.isValid() ||
         (previewLastModified <  pginfo->lastmodified &&
          previewLastModified >= pginfo->recendts)) &&
        !pginfo->IsEditing() && 
        !JobQueue::IsJobRunning(JOB_COMMFLAG, pginfo) &&
        !IsGeneratingPreview(filename))
    {
        VERBOSE(VB_PLAYBACK, QString("Starting preview generator ") +
                QString("%1 && (%2 || ((%3<%4)->%5 && (%6>=%7)->%8)) && ")
                .arg(previewFromBookmark)
                .arg(!previewLastModified.isValid())
                .arg(previewLastModified.toString(Qt::ISODate))
                .arg(pginfo->lastmodified.toString(Qt::ISODate))
                .arg(previewLastModified <  pginfo->lastmodified)
                .arg(previewLastModified.toString(Qt::ISODate))
                .arg(pginfo->recendts.toString(Qt::ISODate))
                .arg(previewLastModified >= pginfo->recendts) +
                QString("%1 && %2 && %3")
                .arg(!pginfo->IsEditing())
                .arg(!JobQueue::IsJobRunning(JOB_COMMFLAG, pginfo))
                .arg(!IsGeneratingPreview(filename)));

#ifdef USE_PREV_GEN_THREAD
        uint attempts = IncPreviewGeneratorAttempts(filename);
        if (attempts < 5)
        {
            SetPreviewGenerator(filename, new PreviewGenerator(pginfo, false));
        }
        else if (attempts == 5)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Attempted to generate preview for '%1'")
                    .arg(filename) + " 5 times, giving up.");
            return retpixmap;
        }
#else
        PreviewGenerator pg(pginfo, false);
        pg.Run();
#endif
    }

    // Check and see if we've already tried this one.
    if (previewPixmap &&
        pginfo->recstartts == previewStartts &&
        pginfo->chanid == previewChanid &&
        previewLastModified == previewFilets)
    {
        return *previewPixmap;
    }

    paintSkipUpdate = false; // repaint background next time around

    if (previewPixmap)
    {
        delete previewPixmap;
        previewPixmap = NULL;
    }

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    previewPixmap = gContext->LoadScalePixmap(filename, true /*fromcache*/);
    if (previewPixmap)
    {
        previewStartts = pginfo->recstartts;
        previewChanid = pginfo->chanid;
        previewFilets = previewLastModified;
        retpixmap = *previewPixmap;
        return retpixmap;
    }

    //if this is a remote frontend, then we need to refresh the pixmap
    //if another frontend has already regenerated the stale pixmap on the disk
    bool refreshPixmap      = (previewLastModified >= previewFilets);
    bool generating_preview = IsGeneratingPreview(filename);

    QImage *image = NULL;
    if (!generating_preview)
        image = gContext->CacheRemotePixmap(filename, refreshPixmap);

    // If the image is not available remotely either, we need to generate it.
    if (!image && !generating_preview)
    {
#ifdef USE_PREV_GEN_THREAD
        uint attempts = IncPreviewGeneratorAttempts(filename);
        if (attempts < 5)
        {
            VERBOSE(VB_PLAYBACK, "Starting preview generator");
            SetPreviewGenerator(filename, new PreviewGenerator(pginfo, false));
        }
        else if (attempts == 5)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Attempted to generate preview for '%1'")
                    .arg(filename) + " 5 times, giving up.");
            return retpixmap;
        }
#else
        PreviewGenerator pg(pginfo, false);
        pg.Run();
        image = gContext->CacheRemotePixmap(filename, true);
#endif
    }

    if (image)
    {
        previewPixmap = new QPixmap();

        if (wmult != 1.0f || hmult != 1.0f)
        {
            QImage tmp2 = image->smoothScale((int)(image->width() * wmult),
                                             (int)(image->height() * hmult));
            previewPixmap->convertFromImage(tmp2);
        }
        else
        {
            previewPixmap->convertFromImage(*image);
        }
    }
 
    if (!previewPixmap)
    {
        previewPixmap = new QPixmap((int)(160 * wmult), (int)(120 * hmult));
        previewPixmap->fill(black);
    }

    retpixmap = *previewPixmap;
    previewStartts = pginfo->recstartts;
    previewChanid = pginfo->chanid;
    previewFilets = previewLastModified;

    return retpixmap;
}

void PlaybackBox::showIconHelp(void)
{
    int curRow = 0;
    LayerSet *container = NULL;
    if (type != Delete)
        container = theme->GetSet("program_info_play");
    else
        container = theme->GetSet("program_info_del");

    if (!container)
        return;

    MythPopupBox *iconhelp = new MythPopupBox(
        gContext->GetMainWindow(), true, drawPopupFgColor,
        drawPopupBgColor, drawPopupSelColor, "icon help");

    QGridLayout *grid = new QGridLayout(6, 2, (int)(10 * wmult));

    QLabel *label;
    UIImageType *itype;
    bool displayme = false;

    label = iconhelp->addLabel(tr("Status Icons"), MythPopupBox::Large,
                               false);

    label->setAlignment(Qt::AlignCenter | Qt::WordBreak);

    QMap <QString, QString>::iterator it;
    QMap <QString, QString> iconMap;
    iconMap["commflagged"] = tr("Commercials are flagged");
    iconMap["cutlist"]     = tr("An editing cutlist is present");
    iconMap["autoexpire"]  = tr("The program is able to auto-expire");
    iconMap["processing"]  = tr("Commercials are being flagged");
    iconMap["bookmark"]    = tr("A bookmark is set");
    iconMap["inuse"]       = tr("Recording is in use");
    iconMap["transcoded"]  = tr("Recording has been transcoded");
    iconMap["stereo"]      = tr("Recording is in Stereo");
    iconMap["cc"]          = tr("Recording is Closed Captioned");
    iconMap["hdtv"]        = tr("Recording is in High Definition");
    iconMap["preserved"]   = tr("Recording is preserved");

    for (it = iconMap.begin(); it != iconMap.end(); ++it)
    {
        itype = (UIImageType *)container->GetType(it.key());
        if (itype)
        {
            label = new QLabel(it.data(), iconhelp);
            label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
            label->setBackgroundOrigin(ParentOrigin);
            label->setPaletteForegroundColor(drawPopupFgColor);
            grid->addWidget(label, curRow, 1, Qt::AlignLeft);
             
            label = new QLabel(iconhelp, "nopopsize");

            itype->ResetFilename();
            itype->LoadImage();
            label->setPixmap(itype->GetImage());
            displayme = true;

            label->setMaximumWidth(width() / 2);
            label->setBackgroundOrigin(ParentOrigin);
            label->setPaletteForegroundColor(drawPopupFgColor);
            grid->addWidget(label, curRow, 0, Qt::AlignCenter);

            curRow++;
        }
    }

    if (!displayme)
    {
        delete iconhelp;
        return;
    }

    killPlayerSafe();

    iconhelp->addLayout(grid);

    QButton *button = iconhelp->addButton(tr("Ok"));
    button->setFocus();

    iconhelp->ExecPopup();

    delete iconhelp;

    previewVideoState = kChanging;

    paintSkipUpdate = false;
    paintSkipCount = 2;

    setActiveWindow();
}

void PlaybackBox::initRecGroupPopup(QString title, QString name)
{
    if (recGroupPopup)
        closeRecGroupPopup();

    recGroupPopup = new MythPopupBox(gContext->GetMainWindow(), true,
                                     drawPopupFgColor, drawPopupBgColor,
                                     drawPopupSelColor, name);

    QLabel *label = recGroupPopup->addLabel(title, MythPopupBox::Large, false);
    label->setAlignment(Qt::AlignCenter);

    killPlayerSafe();

}

void PlaybackBox::closeRecGroupPopup(bool refreshList)
{
    if (!recGroupPopup)
        return;

    recGroupPopup->hide();
    delete recGroupPopup;

    recGroupPopup = NULL;
    recGroupLineEdit = NULL;
    recGroupListBox = NULL;
    recGroupOldPassword = NULL;
    recGroupNewPassword = NULL;
    recGroupOkButton = NULL;

    if (refreshList)
        connected = FillList(true);

    paintSkipUpdate = false;
    paintSkipCount = 2;

    previewVideoState = kChanging;

    setActiveWindow();

    if (delitem)
    {
        delete delitem;
        delitem = NULL;
    }
}

void PlaybackBox::showViewChanger(void)
{
    ViewMask savedMask = viewMask;

    if (!expectingPopup)
        return;

    cancelPopup();

    initRecGroupPopup(tr("Group View"), "showViewChanger");

    MythCheckBox *checkBox;

    checkBox = new MythCheckBox(tr("Show Titles"), recGroupPopup);
    checkBox->setChecked(viewMask & VIEW_TITLES);
    connect(checkBox, SIGNAL(toggled(bool)), this, SLOT(toggleTitleView(bool)));
    recGroupPopup->addWidget(checkBox, false);
    checkBox->setFocus();

    checkBox = new MythCheckBox(tr("Show Categories"), recGroupPopup);
    checkBox->setChecked(viewMask & VIEW_CATEGORIES);
    connect(checkBox, SIGNAL(toggled(bool)), this,
            SLOT(toggleCategoryView(bool)));
    recGroupPopup->addWidget(checkBox, false);

    checkBox = new MythCheckBox(tr("Show Recording Groups"), recGroupPopup);
    checkBox->setChecked(viewMask & VIEW_RECGROUPS);
    connect(checkBox, SIGNAL(toggled(bool)), this,
            SLOT(toggleRecGroupView(bool)));
    recGroupPopup->addWidget(checkBox, false);

    checkBox = new MythCheckBox(tr("Show Watch List"), recGroupPopup);
    checkBox->setChecked(viewMask & VIEW_WATCHLIST);
    connect(checkBox, SIGNAL(toggled(bool)), this,
            SLOT(toggleWatchListView(bool)));
    recGroupPopup->addWidget(checkBox, false);

    checkBox = new MythCheckBox(tr("Show Searches"), recGroupPopup);
    checkBox->setChecked(viewMask & VIEW_SEARCHES);
    connect(checkBox, SIGNAL(toggled(bool)), this,
            SLOT(toggleSearchView(bool)));
    recGroupPopup->addWidget(checkBox, false);

    MythPushButton *okbutton = new MythPushButton(recGroupPopup);
    okbutton->setText(tr("Save Current View"));
    recGroupPopup->addWidget(okbutton);
    connect(okbutton, SIGNAL(clicked()), recGroupPopup, SLOT(accept()));

    MythPushButton *exitbutton = new MythPushButton(recGroupPopup);
    exitbutton->setText(tr("Cancel"));
    recGroupPopup->addWidget(exitbutton);
    connect(exitbutton, SIGNAL(clicked()), recGroupPopup, SLOT(reject()));

    int result = recGroupPopup->ExecPopup();

    if (result == MythDialog::Accepted)
    {
        if (viewMask == VIEW_NONE)
            viewMask = VIEW_TITLES;
        connected = FillList(true);
        gContext->SaveSetting("DisplayGroupDefaultViewMask", (int)viewMask);
        gContext->SaveSetting("PlaybackWatchList",
            (bool)(viewMask & VIEW_WATCHLIST));
    }
    else
    {
        viewMask = savedMask;
        connected = FillList(true);
        paintSkipUpdate = false;
        update(drawTotalBounds);
    }

    closeRecGroupPopup(result == MythDialog::Accepted);
}

void PlaybackBox::showRecGroupChooser(void)
{
    // This is contrary to other code because the option to display this
    // box when starting the Playbackbox means expectingPopup will not be
    // set, nor is there a popup to be canceled as is normally the case
    if (expectingPopup)
       cancelPopup();

    initRecGroupPopup(tr("Select Group Filter"), "showRecGroupChooser");

    QStringList groups;
    
    MSqlQuery query(MSqlQuery::InitCon());
    QString itemStr;
    QString dispGroup;
    int items;
    int totalItems = 0;
    bool liveTVInAll = gContext->GetNumSetting("LiveTVInAllPrograms",0);

    recGroupType.clear();

    recGroupListBox = new MythListBox(recGroupPopup);

    // Find each recording group, and the number of recordings in each
    query.prepare(
        "SELECT recgroup, COUNT(title) "
        "FROM recorded "
        "WHERE deletepending = 0 "
        "GROUP BY recgroup");
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            dispGroup = QString::fromUtf8(query.value(0).toString());
            items     = query.value(1).toInt();
            itemStr   = (items == 1) ? tr("item") : tr("items");
            dispGroup = (dispGroup == "Default") ? tr("Default") : dispGroup;
            dispGroup = (dispGroup == "LiveTV")  ? tr("LiveTV")  : dispGroup;

            groups += QString("%1 [%2 %3]").arg(dispGroup)
                              .arg(items).arg(itemStr);

            recGroupType[query.value(0).toString()] = "recgroup";

            if (dispGroup != "LiveTV" || liveTVInAll)
                totalItems += items;
        }
    }

    // Create and add the "All Programs" entry
    itemStr = (totalItems == 1) ? tr("item") : tr("items");
    recGroupListBox->insertItem(QString("%1 [%2 %3]").arg(tr("All Programs"))
                                        .arg(totalItems).arg(itemStr));
    recGroupType["All Programs"] = "recgroup";

    // Add the group entries
    recGroupListBox->insertItem(QString("------- %1 -------")
                                        .arg(tr("Groups")));
    groups.sort();
    recGroupListBox->insertStringList(groups);
    groups.clear();

    // Find each category, and the number of recordings in each
    query.prepare(
        "SELECT DISTINCT category, COUNT(title) "
        "FROM recorded "
        "GROUP BY category");
    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            dispGroup = QString::fromUtf8(query.value(0).toString());
            items     = query.value(1).toInt();
            itemStr   = (items == 1) ? tr("item") : tr("items");

            if (!recGroupType.contains(dispGroup))
            {
                groups += QString("%1 [%2 %3]").arg(dispGroup)
                                  .arg(items).arg(itemStr);

                recGroupType[dispGroup] = "category";
            }
        }
    }

    // Add the category entries
    recGroupListBox->insertItem(QString("------- %1 -------")
                                .arg(tr("Categories")));
    groups.sort();
    recGroupListBox->insertStringList(groups);

    // Now set up the widget
    recGroupPopup->addWidget(recGroupListBox);
    recGroupListBox->setFocus();

    // figure out what our recGroup is called in the recGroupPopup
    if (recGroup == "All Programs")
        dispGroup = tr("All Programs");
    else if (recGroup == "Default")
        dispGroup = tr("Default");
    else if (recGroup == "LiveTV")
        dispGroup = tr("LiveTV");
    else
        dispGroup = recGroup;

    // select the recGroup in the dialog
    int index = recGroupListBox->index(recGroupListBox->findItem(dispGroup));
    // HACK make the selection show up by selecting a different item first.
    recGroupListBox->setCurrentItem((index + 1) % 2);
    recGroupListBox->setCurrentItem(index);

    recGroupLastItem = recGroupListBox->currentItem();

    connect(recGroupListBox, SIGNAL(accepted(int)), recGroupPopup,
            SLOT(accept()));
    connect(recGroupListBox, SIGNAL(currentChanged(QListBoxItem *)), this,
            SLOT(recGroupChooserListBoxChanged()));

    int result = recGroupPopup->ExecPopup();
    
    if (result == MythDialog::Accepted)
        setGroupFilter();

    closeRecGroupPopup(result == MythDialog::Accepted);

    if (result == MythDialog::Accepted)
    {
        progIndex = 0;
        titleIndex = 0;
    }
}

void PlaybackBox::recGroupChooserListBoxChanged(void)
{
    if (!recGroupListBox)
        return;

    QString item = recGroupListBox->currentText().section(" [", 0, 0);

    if (item.left(5) == "-----")
    {
        int thisItem = recGroupListBox->currentItem();
        if ((recGroupLastItem > thisItem) &&
            (thisItem > 0))
            recGroupListBox->setCurrentItem(thisItem - 1);
        else if ((thisItem > recGroupLastItem) &&
                 ((unsigned int)thisItem < (recGroupListBox->count() - 1)))
            recGroupListBox->setCurrentItem(thisItem + 1);
    }
    recGroupLastItem = recGroupListBox->currentItem();
}

void PlaybackBox::setGroupFilter(void)
{
    recGroup = recGroupListBox->currentText().section(" [", 0, 0);

    if (groupnameAsAllProg)
        groupDisplayName = recGroup;

    if (recGroup == tr("Default"))
        recGroup = "Default";
    else if (recGroup == tr("All Programs"))
        recGroup = "All Programs";
    else if (recGroup == tr("LiveTV"))
        recGroup = "LiveTV";

    recGroupPassword = getRecGroupPassword(recGroup);

    if (recGroupPassword != "" )
    {

        bool ok = false;
        QString text = tr("Password:");

        MythPasswordDialog *pwd = new MythPasswordDialog(text, &ok,
                                                     recGroupPassword,
                                                     gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if (!ok)
            return;

        curGroupPassword = recGroupPassword;
    } else
        curGroupPassword = "";

    if (gContext->GetNumSetting("RememberRecGroup",1))
        gContext->SaveSetting("DisplayRecGroup", recGroup);

    if (recGroupType[recGroup] == "recgroup")
        gContext->SaveSetting("DisplayRecGroupIsCategory", 0);
    else
        gContext->SaveSetting("DisplayRecGroupIsCategory", 1);
}

QString PlaybackBox::getRecGroupPassword(QString group)
{
    QString result = QString("");

    if (group == "All Programs")
    {
        result = gContext->GetSetting("AllRecGroupPassword");
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT password FROM recgrouppassword "
                                   "WHERE recgroup = :GROUP ;");
        query.bindValue(":GROUP", group.utf8());

        if (query.exec() && query.isActive() && query.size() > 0)
            if (query.next())
                result = query.value(0).toString();
    }

    if (result == QString::null)
        result = QString("");

    return(result);
}

void PlaybackBox::fillRecGroupPasswordCache(void)
{
    recGroupPwCache.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recgroup, password "
                  "FROM recgrouppassword "
                  "WHERE password is not null "
                  "AND password <> '' ;");

    if (query.exec() && query.isActive() && query.size() > 0)
        while (query.next())
        {
            QString recgroup = QString::fromUtf8(query.value(0).toString());
            recGroupPwCache[recgroup] =
                query.value(1).toString();
        }
}

void PlaybackBox::doPlaylistChangeRecGroup(void)
{
    // If delitem is not NULL, then the Recording Group changer will operate
    // on just that recording, otherwise it operates on the items in theplaylist
    if (delitem)
    {
        delete delitem;
        delitem = NULL;
    }

    showRecGroupChanger();
}

void PlaybackBox::showRecGroupChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    initRecGroupPopup(tr("Recording Group"), "showRecGroupChanger");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT recgroup, COUNT(title) FROM recorded "
        "WHERE deletepending = 0 GROUP BY recgroup");

    QStringList groups;
    QString itemStr;
    QString dispGroup;

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            if (query.value(1).toInt() == 1)
                itemStr = tr("item");
            else
                itemStr = tr("items");

            dispGroup = QString::fromUtf8(query.value(0).toString());

            if (dispGroup == "Default")
                dispGroup = tr("Default");
            else if (dispGroup == "LiveTV")
                dispGroup = tr("LiveTV");

            groups += QString("%1 [%2 %3]").arg(dispGroup)
                              .arg(query.value(1).toInt()).arg(itemStr);
        }
    }
    groups.sort();

    recGroupListBox = new MythListBox(recGroupPopup);
    recGroupListBox->insertStringList(groups);
    recGroupPopup->addWidget(recGroupListBox);

    recGroupLineEdit = new MythLineEdit(recGroupPopup);
    recGroupLineEdit->setText("");
    recGroupLineEdit->selectAll();
    recGroupPopup->addWidget(recGroupLineEdit);

    if (delitem && (delitem->recgroup != "Default"))
    {
        recGroupLineEdit->setText(delitem->recgroup);
        recGroupListBox->setCurrentItem(
            recGroupListBox->index(recGroupListBox->findItem(delitem->recgroup)));
    }
    else
    {
        QString dispGroup = recGroup;

        if (recGroup == "Default")
            dispGroup = tr("Default");
        else if (recGroup == "LiveTV")
            dispGroup = tr("LiveTV");

        recGroupLineEdit->setText(dispGroup);
        recGroupListBox->setCurrentItem(recGroupListBox->index(
                                        recGroupListBox->findItem(dispGroup)));
    }

    recGroupOkButton = new MythPushButton(recGroupPopup);
    recGroupOkButton->setText(tr("OK"));
    recGroupPopup->addWidget(recGroupOkButton);

    recGroupListBox->setFocus();

    connect(recGroupListBox, SIGNAL(accepted(int)), recGroupPopup,
            SLOT(accept()));
    connect(recGroupListBox, SIGNAL(currentChanged(QListBoxItem *)), this,
            SLOT(recGroupChangerListBoxChanged()));
    connect(recGroupOkButton, SIGNAL(clicked()), recGroupPopup, SLOT(accept()));

    int result = recGroupPopup->ExecPopup();

    if (result == MythDialog::Accepted)
        setRecGroup();

    closeRecGroupPopup(result == MythDialog::Accepted);
}

void PlaybackBox::doPlaylistChangePlayGroup(void)
{
    // If delitem is not NULL, then the Playback Group changer will operate
    // on just that recording, otherwise it operates on the items in theplaylist
    if (delitem)
    {
        delete delitem;
        delitem = NULL;
    }

    showPlayGroupChanger();
}

void PlaybackBox::showPlayGroupChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    initRecGroupPopup(tr("Playback Group"), "showPlayGroupChanger");

    recGroupListBox = new MythListBox(recGroupPopup);
    recGroupListBox->insertItem(tr("Default"));
    recGroupListBox->insertStringList(PlayGroup::GetNames());
    recGroupPopup->addWidget(recGroupListBox);

    if (delitem && (delitem->playgroup != "Default"))
    {
        recGroupListBox->setCurrentItem(
            recGroupListBox->index(recGroupListBox->findItem(delitem->playgroup)));
    }
    else
    {
        QString dispGroup = tr("Default");
        recGroupListBox->setCurrentItem(recGroupListBox->index(
                                        recGroupListBox->findItem(dispGroup)));
    }

    recGroupListBox->setFocus();
    connect(recGroupListBox, SIGNAL(accepted(int)), recGroupPopup,
            SLOT(accept()));

    int result = recGroupPopup->ExecPopup();

    if (result == MythDialog::Accepted)
        setPlayGroup();

    closeRecGroupPopup(result == MythDialog::Accepted);
}

void PlaybackBox::showRecTitleChanger()
{
    if (!expectingPopup)
        return;

    cancelPopup();

    initRecGroupPopup(tr("Recording Title"), "showRecTitleChanger");

    recGroupPopup->addLabel(tr("Title"));
    recGroupLineEdit = new MythLineEdit(recGroupPopup);
    recGroupLineEdit->setText(delitem->title);
    recGroupLineEdit->selectAll();
    recGroupPopup->addWidget(recGroupLineEdit);

    recGroupPopup->addLabel(tr("Subtitle"));
    recGroupLineEdit1 = new MythLineEdit(recGroupPopup);
    recGroupLineEdit1->setText(delitem->subtitle);
    recGroupLineEdit1->selectAll();
    recGroupPopup->addWidget(recGroupLineEdit1);

    recGroupLineEdit->setFocus();

    recGroupOkButton = new MythPushButton(recGroupPopup);
    recGroupOkButton->setText(tr("OK"));
    recGroupPopup->addWidget(recGroupOkButton);

    connect(recGroupOkButton, SIGNAL(clicked()), recGroupPopup, SLOT(accept()));

    int result = recGroupPopup->ExecPopup();

    if (result == MythDialog::Accepted)
        setRecTitle();

    closeRecGroupPopup(result == MythDialog::Accepted);

    delete delitem;
    delitem = NULL;
}

void PlaybackBox::setRecGroup(void)
{
    QString newRecGroup = recGroupLineEdit->text();
    ProgramInfo *tmpItem;

    if (newRecGroup != "" )
    {
        if (newRecGroup == tr("Default"))
            newRecGroup = "Default";
        else if (newRecGroup == tr("LiveTV"))
            newRecGroup = "LiveTV";

        if (delitem)
        {
            if ((delitem->recgroup == "LiveTV") &&
                (newRecGroup != "LiveTV"))
                delitem->SetAutoExpire(
                    gContext->GetNumSetting("AutoExpireDefault", 0));
            else if ((delitem->recgroup != "LiveTV") &&
                     (newRecGroup == "LiveTV"))
                delitem->SetAutoExpire(kLiveTVAutoExpire);


            tmpItem = findMatchingProg(delitem);
            if (tmpItem)
                tmpItem->ApplyRecordRecGroupChange(newRecGroup);
        } else if (playList.count() > 0) {
            QStringList::Iterator it;

            for (it = playList.begin(); it != playList.end(); ++it )
            {
                tmpItem = findMatchingProg(*it);
                if (tmpItem)
                {
                    if ((tmpItem->recgroup == "LiveTV") &&
                        (newRecGroup != "LiveTV"))
                        tmpItem->SetAutoExpire(
                            gContext->GetNumSetting("AutoExpireDefault", 0));
                    else if ((tmpItem->recgroup != "LiveTV") &&
                             (newRecGroup == "LiveTV"))
                        tmpItem->SetAutoExpire(kLiveTVAutoExpire);

                    tmpItem->ApplyRecordRecGroupChange(newRecGroup);
                }
            }
            playList.clear();
        }
    }
}

void PlaybackBox::setPlayGroup(void)
{
    QString newPlayGroup = recGroupListBox->currentText();
    ProgramInfo *tmpItem;

    if (newPlayGroup == tr("Default"))
        newPlayGroup = "Default";

    if (delitem)
    {
        tmpItem = findMatchingProg(delitem);
        if (tmpItem)
            tmpItem->ApplyRecordPlayGroupChange(newPlayGroup);
    } 
    else if (playList.count() > 0) 
    {
        QStringList::Iterator it;

        for (it = playList.begin(); it != playList.end(); ++it )
        {
            tmpItem = findMatchingProg(*it);
            if (tmpItem)
                tmpItem->ApplyRecordPlayGroupChange(newPlayGroup);
        }
        playList.clear();
    }
}

void PlaybackBox::setRecTitle()
{
    QString newRecTitle = recGroupLineEdit->text();
    QString newRecSubtitle = recGroupLineEdit1->text();
    ProgramInfo *tmpItem;

    if (newRecTitle == "")
        return;

    tmpItem = findMatchingProg(delitem);
    if (tmpItem)
        tmpItem->ApplyRecordRecTitleChange(newRecTitle, newRecSubtitle);

    inTitle = gContext->GetNumSetting("PlaybackBoxStartInTitle", 0);
    titleIndex = 0;
    progIndex = 0;

    connected = FillList(true);
    paintSkipUpdate = false;
    update(drawTotalBounds);
}

void PlaybackBox::recGroupChangerListBoxChanged(void)
{
    if (!recGroupPopup || !recGroupListBox || !recGroupLineEdit)
        return;

    recGroupLineEdit->setText(
        recGroupListBox->currentText().section('[', 0, 0).simplifyWhiteSpace());
}

void PlaybackBox::showRecGroupPasswordChanger(void)
{
    if (!expectingPopup)
        return;

    cancelPopup();

    initRecGroupPopup(tr("Group Password"),
                      "showRecGroupPasswordChanger");

    QGridLayout *grid = new QGridLayout(3, 2, (int)(10 * wmult));

    QLabel *label = new QLabel(tr("Recording Group:"), recGroupPopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(drawPopupFgColor);
    grid->addWidget(label, 0, 0, Qt::AlignLeft);

    if ((recGroup == "Default") || (recGroup == "All Programs") || 
        (recGroup == "LiveTV"))
        label = new QLabel(tr(recGroup), recGroupPopup);
    else
        label = new QLabel(recGroup, recGroupPopup);

    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(drawPopupFgColor);
    grid->addWidget(label, 0, 1, Qt::AlignLeft);

    label = new QLabel(tr("Old Password:"), recGroupPopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(drawPopupFgColor);
    grid->addWidget(label, 1, 0, Qt::AlignLeft);

    recGroupOldPassword = new MythLineEdit(recGroupPopup);
    recGroupOldPassword->setText("");
    recGroupOldPassword->selectAll();
    grid->addWidget(recGroupOldPassword, 1, 1, Qt::AlignLeft);

    label = new QLabel(tr("New Password:"), recGroupPopup);
    label->setAlignment(Qt::WordBreak | Qt::AlignLeft);
    label->setBackgroundOrigin(ParentOrigin);
    label->setPaletteForegroundColor(drawPopupFgColor);
    grid->addWidget(label, 2, 0, Qt::AlignLeft);

    recGroupNewPassword = new MythLineEdit(recGroupPopup);
    recGroupNewPassword->setText("");
    recGroupNewPassword->selectAll();
    grid->addWidget(recGroupNewPassword, 2, 1, Qt::AlignLeft);

    recGroupPopup->addLayout(grid);

    recGroupOkButton = new MythPushButton(recGroupPopup);
    recGroupOkButton->setText(tr("OK"));
    recGroupPopup->addWidget(recGroupOkButton);

    recGroupChooserPassword = getRecGroupPassword(recGroup);

    recGroupOldPassword->setEchoMode(QLineEdit::Password);
    recGroupNewPassword->setEchoMode(QLineEdit::Password);

    if (recGroupPassword == "" )
        recGroupOkButton->setEnabled(true);
    else
        recGroupOkButton->setEnabled(false);

    recGroupOldPassword->setFocus();

    connect(recGroupOldPassword, SIGNAL(textChanged(const QString &)), this,
            SLOT(recGroupOldPasswordChanged(const QString &)));
    connect(recGroupOkButton, SIGNAL(clicked()), recGroupPopup, SLOT(accept()));

    if (recGroupPopup->ExecPopup() == MythDialog::Accepted)
        setRecGroupPassword();

    closeRecGroupPopup(false);
}

void PlaybackBox::setRecGroupPassword(void)
{
    QString newPassword = recGroupNewPassword->text();

    if (recGroupOldPassword->text() != recGroupPassword)
        return;

    if (recGroup == "All Programs")
    {
        gContext->SaveSetting("AllRecGroupPassword", newPassword);
    }
    else
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("DELETE FROM recgrouppassword "
                           "WHERE recgroup = :RECGROUP ;");
        query.bindValue(":RECGROUP", recGroup.utf8());

        query.exec();

        if (newPassword != "")
        {
            query.prepare("INSERT INTO recgrouppassword "
                          "(recgroup, password) VALUES "
                          "( :RECGROUP , :PASSWD )");
            query.bindValue(":RECGROUP", recGroup.utf8());
            query.bindValue(":PASSWD", newPassword);

            query.exec();
        }
    }
}

void PlaybackBox::recGroupOldPasswordChanged(const QString &newText)
{
    if (newText == recGroupChooserPassword)
        recGroupOkButton->setEnabled(true);
    else
        recGroupOkButton->setEnabled(false);
}

void PlaybackBox::clearProgramCache(void)
{
    if (!progCache)
        return;

    vector<ProgramInfo *>::iterator i = progCache->begin();
    for ( ; i != progCache->end(); i++)
        delete *i;
    delete progCache;
    progCache = NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
