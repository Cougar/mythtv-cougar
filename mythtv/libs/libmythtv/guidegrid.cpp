#include <math.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
using namespace std;

#include <QApplication>
#include <QPainter>
#include <QFont>
#include <QKeyEvent>
#include <QEvent>
#include <QPixmap>
#include <QPaintEvent>
#include <QCursor>
#include <QImage>
#include <QLayout>
#include <QLabel>
#include <QDateTime>
#include <QRect>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "guidegrid.h"
#include "infostructs.h"
#include "programinfo.h"
#include "scheduledrecording.h"
#include "oldsettings.h"
#include "tv_play.h"
#include "tv_rec.h"
//#include "progfind.h"
#include "proglist_qt.h"
#include "customedit.h"
#include "util.h"
#include "remoteutil.h"
#include "channelutil.h"
#include "cardutil.h"

QWaitCondition epgIsVisibleCond;

#define LOC      QString("GuideGrid: ")
#define LOC_ERR  QString("GuideGrid, Error: ")
#define LOC_WARN QString("GuideGrid, Warning: ")

JumpToChannel::JumpToChannel(
    JumpToChannelListener *parent, const QString &start_entry,
    int start_chan_idx, int cur_chan_idx, uint rows_disp) :
    listener(parent),
    entry(start_entry),
    previous_start_channel_index(start_chan_idx),
    previous_current_channel_index(cur_chan_idx),
    rows_displayed(rows_disp),
    timer(new QTimer(this))
{
    if (parent && timer)
    {
        connect(timer, SIGNAL(timeout()), SLOT(deleteLater()));
        timer->setSingleShot(true);
    }
    Update();
}


void JumpToChannel::deleteLater(void)
{
    if (listener)
    {
        listener->SetJumpToChannel(NULL);
        listener = NULL;
    }

    if (timer)
    {
        timer->stop();
        timer = NULL;
    }

    QObject::deleteLater();
}


static bool has_action(QString action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

bool JumpToChannel::ProcessEntry(
    const QStringList &actions, const QKeyEvent *e)
{
    if (!listener)
        return false;

    if (has_action("ESCAPE", actions))
    {
        listener->GoTo(previous_start_channel_index,
                       previous_current_channel_index);
        deleteLater();
        return true;
    }

    if (has_action("DELETE", actions))
    {
        if (entry.length())
            entry = entry.left(entry.length()-1);
        Update();
        return true;
    }

    if (has_action("SELECT", actions))
    {
        if (Update())
            deleteLater();
        return true;
    }

    QString txt = e->text();
    bool isUInt;
    txt.toUInt(&isUInt);
    if (isUInt)
    {
        entry += txt;
        Update();
        return true;
    }

    if (entry.length() && (txt=="_" || txt=="-" || txt=="#" || txt=="."))
    {
        entry += txt;
        Update();
        return true;
    }

    return false;
}

bool JumpToChannel::Update(void)
{
    if (!timer || !listener)
        return false;

    timer->stop();

    // find the closest channel ...
    int i = listener->FindChannel(0, entry, false);
    if (i >= 0)
    {
        // setup the timeout timer for jump mode
        timer->start(kJumpToChannelTimeout);

        // rows_displayed to center
        int start = i - rows_displayed/2;
        int cur   = rows_displayed/2;
        listener->GoTo(start, cur);
        return true;
    }
    else
    { // prefix must be invalid.. reset entry..
        deleteLater();
        return false;
    }
}

DBChanList GuideGrid::Run(
    uint           chanid,
    const QString &channum,
    bool           thread,
    TV            *player,
    bool           allowsecondaryepg)
{
    DBChanList channel_changed;

    //if (thread)
    //    qApp->lock();

    gContext->addCurrentLocation("GuideGrid");

    GuideGrid *gg = new GuideGrid(gContext->GetMainWindow(),
                                  chanid, channum,
                                  player, allowsecondaryepg, "guidegrid");

    gg->Show();

    if (thread)
    {
        //qApp->unlock();
        QMutex glock;
        glock.lock();
        epgIsVisibleCond.wait(&glock);
        glock.unlock();
    }
    else
        gg->exec();

    if (gg->selectState)
    {
        DBChanList sel = gg->GetSelection();
        if (sel.size() &&
            (std::find(sel.begin(), sel.end(), chanid) == sel.end()))
        {
            channel_changed = sel;
        }
    }

    //if (thread)
    //    qApp->lock();

    delete gg;

    gContext->removeCurrentLocation();

    //if (thread)
    //    qApp->unlock();

    return channel_changed;
}

GuideGrid::GuideGrid(MythMainWindow *parent,
                     uint chanid, QString channum,
                     TV *player, bool allowsecondaryepg,
                     const char *name) :
    MythDialog(parent, name),
    m_player(player),
    using_null_video(false),
    previewVideoRefreshTimer(new QTimer(this)),
    jumpToChannelLock(QMutex::Recursive),
    jumpToChannel(NULL),
    jumpToChannelEnabled(true),
    jumpToChannelHasRect(false)
{
    connect(previewVideoRefreshTimer, SIGNAL(timeout()),
            this,                     SLOT(refreshVideo()));

    desiredDisplayChans = DISPLAY_CHANS = 6;
    DISPLAY_TIMES = 30;
    int maxchannel = 0;
    m_currentStartChannel = 0;

    m_context = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    dateRect = QRect(0, 0, 0, 0);
    jumpToChannelRect = QRect(0, 0, 0, 0);
    channelRect = QRect(0, 0, 0, 0);
    timeRect = QRect(0, 0, 0, 0);
    programRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    curInfoRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);

    jumpToChannelEnabled =
        gContext->GetNumSetting("EPGEnableJumpToChannel", 1);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (m_player && m_player->IsRunning() && allowsecondaryepg)
        theme->LoadTheme(xmldata, "programguide-video");
    else 
        theme->LoadTheme(xmldata, "programguide");

    LoadWindow(xmldata);

    if (m_player && m_player->IsRunning())
    {
        if (!allowsecondaryepg)
            videoRect = QRect(0, 0, 1, 1);
        else
            EmbedTVWindow();
    }

    showFavorites = gContext->GetNumSetting("EPGShowFavorites", 0);
    gridfilltype = gContext->GetNumSetting("EPGFillType", UIGuideType::Alpha);
    if (gridfilltype < (int)UIGuideType::Alpha)
    { // update old settings to new fill types
        if (gridfilltype == 5)
            gridfilltype = UIGuideType::Dense;
        else
            gridfilltype = UIGuideType::Alpha;
            
        gContext->SaveSetting("EPGFillType", gridfilltype);
    }

    scrolltype = gContext->GetNumSetting("EPGScrollType", 1);
    sortReverse = gContext->GetNumSetting("EPGSortReverse", 0);

    selectChangesChannel = gContext->GetNumSetting("SelectChangesChannel", 0);
    selectRecThreshold = gContext->GetNumSetting("SelChangeRecThreshold", 16);

    LayerSet *container = NULL;
    container = theme->GetSet("guide");
    if (container)
    {
        UIGuideType *type = (UIGuideType *)container->GetType("guidegrid");
        if (type) 
        {
            type->SetFillType(gridfilltype);
            type->SetShowCategoryColors(
                   gContext->GetNumSetting("EPGShowCategoryColors", 1));
            type->SetShowCategoryText(
                   gContext->GetNumSetting("EPGShowCategoryText", 1));
        }
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QTime new_time = QTime::currentTime();
    QString curTime = new_time.toString(timeformat);

    container = theme->GetSet("current_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("time");
        if (type)
            type->SetText(curTime);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }
    
    container = theme->GetSet("program_info");
    if (container)
    {
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    channelOrdering = gContext->GetSetting("ChannelOrdering", "channum");
    dateformat = gContext->GetSetting("ShortDateFormat", "ddd d");
    unknownTitle = gContext->GetSetting("UnknownTitle", "Unknown");
    unknownCategory = gContext->GetSetting("UnknownCategory", "Unknown");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    channelFormat.replace(" ", "\n");

    UIBarType *type = NULL;
    container = theme->GetSet("chanbar");

    int dNum = gContext->GetNumSetting("chanPerPage", 8);

    if (m_player && m_player->IsRunning() && allowsecondaryepg)
        dNum = dNum * 2 / 3 + 1;

    desiredDisplayChans = DISPLAY_CHANS = dNum;
    if (container)
    {
        type = (UIBarType *)container->GetType("chans");
        if (type)
            type->SetSize(dNum);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    container = theme->GetSet("timebar");
    dNum = gContext->GetNumSetting("timePerPage", 5);
    if (dNum > 5)
        dNum = 5;
    DISPLAY_TIMES = 6 * dNum;

    if (container)
    {
        type = (UIBarType *)container->GetType("times");
        if (type)
            type->SetSize(dNum);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }
    m_originalStartTime = QDateTime::currentDateTime();

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    startChanID  = chanid;
    startChanNum = channum;

    m_currentRow = (int)(desiredDisplayChans / 2);
    m_currentCol = 0;

    for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
        m_programs[y] = NULL;

    for (int x = 0; x < MAX_DISPLAY_TIMES; x++)
    {
        m_timeInfos[x] = NULL;
        for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
            m_programInfos[y][x] = NULL;
    }

    //MythTimer clock = QTime::currentTime();
    //clock.start();
    fillTimeInfos();
    //int filltime = clock.elapsed();
    //clock.restart();
    fillChannelInfos();
    maxchannel = max((int)GetChannelCount() - 1, 0);
    setStartChannel((int)(m_currentStartChannel) - 
                    (int)(desiredDisplayChans / 2));
    DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

    //int fillchannels = clock.elapsed();
    //clock.restart();
    m_recList.FromScheduler();
    fillProgramInfos();
    //int fillprogs = clock.elapsed();

    timeCheck = NULL;
    timeCheck = new QTimer(this);
    connect(timeCheck, SIGNAL(timeout()), SLOT(timeCheckTimeout()) );
    timeCheck->start(200);

    selectState = false;

    updateBackground();

    setNoErase();
    gContext->addListener(this);
    
    keyDown = false;
    setFocusPolicy(Qt::ClickFocus); //get keyRelease events after refocus
}

GuideGrid::~GuideGrid()
{
    gContext->removeListener(this);
    for (int x = 0; x < MAX_DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
        {
            delete m_timeInfos[x];
            m_timeInfos[x] = NULL;
        }
    }

    for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
    {
        if (m_programs[y])
        {
            delete m_programs[y];
            m_programs[y] = NULL;
        }
    }

    m_channelInfos.clear();

    if (theme)
    {
        delete theme;
        theme = NULL;
    }

    if (timeCheck)
    {
        timeCheck->disconnect(this);
        timeCheck = NULL;
    }


    if (previewVideoRefreshTimer)
    {
        previewVideoRefreshTimer->disconnect(this);
        previewVideoRefreshTimer = NULL;
    }

    gContext->SaveSetting("EPGSortReverse", sortReverse ? "1" : "0");
}

void GuideGrid::keyPressEvent(QKeyEvent *e)
{
    // keyDown limits keyrepeats to prevent continued scrolling
    // after key is released. Note: Qt's keycompress should handle
    // this but will fail with fast key strokes and keyboard repeat
    // enabled. Keys will not be marked as autorepeat and flood buffer.
    // setFocusPolicy(QWidget::ClickFocus) in constructor is important 
    // or keyRelease events will not be received after a refocus.
    
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    if (actions.size() > 0 && keyDown && actions[0] != "ESCAPE")
        return; //check for Escape in case something goes wrong 
                //with KeyRelease events, shouldn't happen.

    if (e->key() != Qt::Key_Control && e->key() != Qt::Key_Shift &&
        e->key() != Qt::Key_Meta && e->key() != Qt::Key_Alt)
        keyDown = true;    

    if (e->modifiers() == Qt::ControlModifier)
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "LEFT")
                pageLeft();
            else if (action == "RIGHT")
                pageRight();
            else if (action == "UP")
                pageUp();
            else if (action == "DOWN")
                pageDown();
            else
                handled = false;
        }
        handled = true;
    }
    else
    {
        // We want to handle jump to channel before everything else
        // The reason is because the number keys could be mapped to
        // other things. If this is the case, then the jump to channel
        // will not work correctly.
        {
            QMutexLocker locker(&jumpToChannelLock);

            if (!jumpToChannel || jumpToChannelEnabled)
            {
                bool isNum;
                e->text().toInt(&isNum);
                if (isNum && !jumpToChannel)
                {
                    jumpToChannel = new JumpToChannel(
                        this, e->text(),
                        m_currentStartChannel, m_currentRow, DISPLAY_CHANS);
                    handled = true;
                }
            }

            if (jumpToChannel && !handled)
                handled = jumpToChannel->ProcessEntry(actions, e);
        }
	      
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "LEFT")
                cursorLeft();
            else if (action == "RIGHT")
                cursorRight();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "UP")
                cursorUp();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "PAGELEFT")
                pageLeft();
            else if (action == "PAGERIGHT")
                pageRight();
            else if (action == "DAYLEFT")
                dayLeft();
            else if (action == "DAYRIGHT")
                dayRight();
            else if (action == "NEXTFAV" || action == "4")
                toggleGuideListing();
            else if (action == "FINDER" || action == "6")
                showProgFinder();
            else if (action == "MENU")
                enter();                    
            else if (action == "ESCAPE")
                escape();
            else if (action == "SELECT")
            {                         
                if (m_player && selectChangesChannel)
                {
                    // See if this show is far enough into the future that it's probable
                    // that the user wanted to schedule it to record instead of changing the channel.
                    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
                    if (pginfo && (pginfo->title != unknownTitle) &&
                        ((pginfo->SecsTillStart() / 60) >= selectRecThreshold))
                    {
                        editRecording();
                    }
                    else
                    {
                        enter();
                    }
                }
                else
                    editRecording();
            }
            else if (action == "INFO")
                editScheduled();
            else if (action == "CUSTOMEDIT")
                customEdit();
            else if (action == "DELETE")
                remove();
            else if (action == "UPCOMING")
                upcoming();
            else if (action == "DETAILS")
                details();
            else if (action == "TOGGLERECORD")
                quickRecord();
            else if (action == "TOGGLEFAV")
                toggleChannelFavorite();
            else if (action == "CHANUPDATE")
                channelUpdate();
            else if (action == "VOLUMEUP")
                volumeUpdate(true);
            else if (action == "VOLUMEDOWN")
                volumeUpdate(false);
            else if (action == "MUTE")
                toggleMute();
            else if (action == "TOGGLEEPGORDER")
            {
                sortReverse = !sortReverse;
                generateListings();
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void GuideGrid::keyReleaseEvent(QKeyEvent *e)
{
    // note: KeyRelease events may not reflect the released 
    // key if delayed (key==0 instead).
    (void)e;
    keyDown = false;
}

void GuideGrid::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();

    QPalette p = palette();
    p.setBrush(backgroundRole(), QBrush(bground));
    setPalette(p);
}

void GuideGrid::LoadWindow(QDomElement &element)
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
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        "Unknown element: " + e.tagName());
                continue;
            }
        }
    }
}

void GuideGrid::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.toLower() == "guide")
        programRect = area;
    if (name.toLower() == "program_info")
        infoRect = area;
    if (name.toLower() == "chanbar")
        channelRect = area;
    if (name.toLower() == "timebar")
        timeRect = area;
    if (name.toLower() == "date_info")
        dateRect = area;
    if (name.toLower() == "jumptochannel") {
        jumpToChannelRect = area;
        jumpToChannelHasRect = true;
    }
    if (name.toLower() == "current_info")
        curInfoRect = area;
    if (name.toLower() == "current_video")
        videoRect = area;
}

PixmapChannel *GuideGrid::GetChannelInfo(uint chan_idx, int sel)
{
    sel = (sel >= 0) ? sel : m_channelInfoIdx[chan_idx];

    if (chan_idx >= GetChannelCount())
        return NULL;

    if (sel >= (int) m_channelInfos[chan_idx].size())
        return NULL;

    return &(m_channelInfos[chan_idx][sel]);
}

const PixmapChannel *GuideGrid::GetChannelInfo(uint chan_idx, int sel) const
{
    return ((GuideGrid*)this)->GetChannelInfo(chan_idx, sel);
}

uint GuideGrid::GetChannelCount(void) const
{
    return m_channelInfos.size();
}

int GuideGrid::GetStartChannelOffset(int row) const
{
    uint cnt = GetChannelCount();
    if (!cnt)
        return -1;

    row = (row < 0) ? m_currentRow : row;
    return (row + m_currentStartChannel) % cnt;
}

ProgramList GuideGrid::GetProgramList(uint chanid) const
{
    ProgramList proglist;
    MSqlBindings bindings;
    QString querystr =
        "WHERE program.chanid     = :CHANID  AND "
        "      program.endtime   >= :STARTTS AND "
        "      program.starttime <= :ENDTS   AND "
        "      program.manualid   = 0 ";
    bindings[":STARTTS"] = m_currentStartTime.toString("yyyy-MM-ddThh:mm:00");
    bindings[":ENDTS"]   = m_currentEndTime.toString("yyyy-MM-ddThh:mm:00");
    bindings[":CHANID"]  = chanid;

    ProgramList dummy;
    proglist.FromProgram(querystr, bindings, dummy);

    return proglist;
}

uint GuideGrid::GetAlternateChannelIndex(
    uint chan_idx, bool with_same_channum) const
{
    PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);

    uint si = m_channelInfoIdx[chan_idx];
    const PixmapChannel *chinfo = GetChannelInfo(chan_idx, si);

    for (uint i = 0; (i < m_channelInfos[chan_idx].size()); i++)
    {
        if (i == si)
            continue;

        const PixmapChannel *ciinfo = GetChannelInfo(chan_idx, i);
        bool same_channum = ciinfo->channum == chinfo->channum;

        if (with_same_channum != same_channum)
            continue;

        if (!ciinfo || !m_player->IsTunable(ctx, ciinfo->chanid, true))
            continue;

        if (with_same_channum ||
            (GetProgramList(chinfo->chanid) ==
             GetProgramList(ciinfo->chanid)))
        {
            si = i;
            break;
        }
    }

    m_player->ReturnPlayerLock(ctx);

    return si;
}


#define MKKEY(IDX,SEL) ((((uint64_t)IDX) << 32) | SEL)
DBChanList GuideGrid::GetSelection(void) const
{
    DBChanList selected;

    int idx = GetStartChannelOffset();
    if (idx < 0)
        return selected;

    uint si  = m_channelInfoIdx[idx];

    vector<uint64_t> sel;
    sel.push_back( MKKEY(idx, si) );

    const PixmapChannel *ch = GetChannelInfo(sel[0]>>32, sel[0]&0xffff);
    if (!ch)
        return selected;

    selected.push_back(*ch);
    if (m_channelInfos[idx].size() <= 1)
        return selected;

    ProgramList proglist = GetProgramList(selected[0].chanid);

    if (proglist.count() == 0)
        return selected;

    for (uint i = 0; i < m_channelInfos[idx].size(); i++)
    {
        const PixmapChannel *ci = GetChannelInfo(idx, i);
        if (ci && (i != si) &&
            (ci->callsign == ch->callsign) && (ci->channum  == ch->channum))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 0; i < m_channelInfos[idx].size(); i++)
    {
        const PixmapChannel *ci = GetChannelInfo(idx, i);
        if (ci && (i != si) &&
            (ci->callsign == ch->callsign) && (ci->channum  != ch->channum))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 0; i < m_channelInfos[idx].size(); i++)
    {
        const PixmapChannel *ci = GetChannelInfo(idx, i);
        if ((i != si) && (ci->callsign != ch->callsign))
        {
            sel.push_back( MKKEY(idx, i) );
        }
    }

    for (uint i = 1; i < sel.size(); i++)
    {
        const PixmapChannel *ci = GetChannelInfo(sel[i]>>32, sel[i]&0xffff);
        if (!ci)
            continue;

        ProgramList ch_proglist = GetProgramList(ch->chanid);
        if (proglist == ch_proglist)
            selected.push_back(*ci);
    }

    return selected;
}
#undef MKKEY

void GuideGrid::timeCheckTimeout(void)
{
    timeCheck->start((int)(60 * 1000));
    QTime new_time = QTime::currentTime();
    QString curTime = new_time.toString(timeformat);

    LayerSet *container = NULL;
    container = theme->GetSet("current_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("time");
        if (type)
            type->SetText(curTime);
    }

    fillProgramInfos();
    repaint(programRect);
    repaint(curInfoRect);
}

void GuideGrid::fillChannelInfos(bool gotostartchannel)
{
    m_channelInfos.clear();
    m_channelInfoIdx.clear();
    m_currentStartChannel = 0;

    DBChanList channels = ChannelUtil::GetChannels(0, true);
    ChannelUtil::SortChannels(channels, channelOrdering, false);

    if (showFavorites)
    {
        DBChanList tmp;
        for (uint i = 0; i < channels.size(); i++)
        {
            if (channels[i].favorite)
                tmp.push_back(channels[i]);
        }

        if (!tmp.empty())
            channels = tmp;
    }

    typedef vector<uint> uint_list_t;
    QMap<QString,uint_list_t> channum_to_index_map;
    QMap<QString,uint_list_t> callsign_to_index_map;

    for (uint i = 0; i < channels.size(); i++)
    {
        uint chan=i;
        if (sortReverse)
        {
            chan=channels.size()-i-1;
        }

        bool ndup = channum_to_index_map[channels[chan].channum].size();
        bool cdup = callsign_to_index_map[channels[chan].callsign].size();

        if (ndup && cdup)
            continue;

        PixmapChannel val(channels[chan]);

        channum_to_index_map[val.channum].push_back(GetChannelCount());
        callsign_to_index_map[val.callsign].push_back(GetChannelCount());

        // add the new channel to the list
        pix_chan_list_t tmp;
        tmp.push_back(val);
        m_channelInfos.push_back(tmp);
    }
 
    // handle duplicates
    for (uint i = 0; i < channels.size(); i++)
    {
        const uint_list_t &ndups = channum_to_index_map[channels[i].channum];
        for (uint j = 0; j < ndups.size(); j++)
        {
            if (channels[i].chanid != m_channelInfos[ndups[j]][0].chanid)
                m_channelInfos[ndups[j]].push_back(channels[i]);
        }

        const uint_list_t &cdups = callsign_to_index_map[channels[i].callsign];
        for (uint j = 0; j < cdups.size(); j++)
        {
            if (channels[i].chanid != m_channelInfos[cdups[j]][0].chanid)
                m_channelInfos[cdups[j]].push_back(channels[i]);
        }
    }

    if (gotostartchannel)
    {
        int ch = FindChannel(startChanID, startChanNum);
        m_currentStartChannel = (uint) max(0, ch);
    }

    if (m_channelInfos.empty())
    {
        VERBOSE(VB_IMPORTANT, "GuideGrid: "
                "\n\t\t\tYou don't have any channels defined in the database."
                "\n\t\t\tGuide grid will have nothing to show you.");
    }
}

int GuideGrid::FindChannel(uint chanid, const QString &channum,
                           bool exact) const
{
    static QMutex chanSepRegExpLock;
    static QRegExp chanSepRegExp(ChannelUtil::kATSCSeparators);

    // first check chanid
    uint i = (chanid) ? 0 : GetChannelCount();
    for (; i < GetChannelCount(); i++)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); j++)
        {
            if (m_channelInfos[i][j].chanid == chanid)
                return i;
        }
    }

    // then check channum, first only
    i = (channum.isEmpty()) ? GetChannelCount() : 0;
    for (; i < GetChannelCount(); i++)
    {
        if (m_channelInfos[i][0].chanid == chanid)
            return i;
    }

    // then check all channum
    i = (channum.isEmpty()) ? GetChannelCount() : 0;
    for (; i < GetChannelCount(); i++)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); j++)
        {
            if (m_channelInfos[i][j].chanid == chanid)
                return i;
        }
    }

    if (exact || channum.isEmpty())
        return -1;

    // then check partial channum, first only
    for (i = 0; i < GetChannelCount(); i++)
    {
        if (m_channelInfos[i][0].channum.left(channum.length()) == channum)
            return i;
    }

    // then check all partial channum
    for (i = 0; i < GetChannelCount(); i++)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); j++)
        {
            if (m_channelInfos[i][j].channum.left(channum.length()) == channum)
                return i;
        }
    }

    // then check all channum with "_" for subchannels
    QMutexLocker locker(&chanSepRegExpLock);
    QString tmpchannum = channum;
    if (tmpchannum.contains(chanSepRegExp))
    {
        tmpchannum.replace(chanSepRegExp, "_");
    }
    else if (channum.length() >= 2)
    {
        tmpchannum = channum.left(channum.length() - 1) + "_" +
            channum.right(1);
    }
    else
    {
        return -1;
    }

    for (i = 0; i < GetChannelCount(); i++)
    {
        for (uint j = 0; j < m_channelInfos[i].size(); j++)
        {
            QString tmp = m_channelInfos[i][j].channum;
            tmp.replace(chanSepRegExp, "_");
            if (tmp == tmpchannum)
                return i;
        }
    }

    return -1;
}

void GuideGrid::fillTimeInfos()
{
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
        m_timeInfos[x] = NULL;
    }

    QDateTime t = m_currentStartTime;
    int cnt = 0;
 
    LayerSet *container = NULL;
    UIBarType *type = NULL;
    container = theme->GetSet("timebar");
    if (container)
        type = (UIBarType *)container->GetType("times");
   
    firstTime = m_currentStartTime;
    lastTime = firstTime.addSecs(DISPLAY_TIMES * 60 * 4);

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        int mins = t.time().minute();
        mins = 5 * (mins / 5);
        if (mins % 30 == 0)
        {
            TimeInfo *timeinfo = new TimeInfo;

            int hour = t.time().hour();
            timeinfo->hour = hour;
            timeinfo->min = mins;

            timeinfo->usertime = QTime(hour, mins).toString(timeformat);

            m_timeInfos[x] = timeinfo;
            if (type)
                type->SetText(cnt, timeinfo->usertime);
            cnt++;
        }

        t = t.addSecs(5 * 60);
    }
    m_currentEndTime = t;
}

void GuideGrid::fillProgramInfos(void)
{
    LayerSet *container = NULL;
    UIGuideType *type = NULL;
    container = theme->GetSet("guide");
    if (container)
    {
        type = (UIGuideType *)container->GetType("guidegrid");
        if (type)
        {
            type->SetWindow(this);
            type->SetScreenLocation(programRect.topLeft());
            type->SetNumRows(DISPLAY_CHANS);
            type->ResetData();
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        fillProgramRowInfos(y);
    }
}

void GuideGrid::fillProgramRowInfos(unsigned int row)
{
    LayerSet *container = NULL;
    container = theme->GetSet("guide");
    UIGuideType *type = NULL;
    if (container)
        type = (UIGuideType *)container->GetType("guidegrid");

    if (type)
        type->ResetRow(row);
 
    ProgramList *proglist;

    if (m_programs[row])
        delete m_programs[row];
    m_programs[row] = NULL;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        m_programInfos[row][x] = NULL;
    }

    if (m_channelInfos.size() == 0)
        return;

    int chanNum = row + m_currentStartChannel;
    if (chanNum >= (int) m_channelInfos.size())
        chanNum -= (int) m_channelInfos.size();
    if (chanNum >= (int) m_channelInfos.size())
        return;

    if (chanNum < 0)
        chanNum = 0;

    m_programs[row] = proglist = new ProgramList();

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid = :CHANID "
                       "  AND program.endtime >= :STARTTS "
                       "  AND program.starttime <= :ENDTS "
                       "  AND program.manualid = 0 ";
    bindings[":CHANID"]  = GetChannelInfo(chanNum)->chanid;
    bindings[":STARTTS"] = m_currentStartTime.toString("yyyy-MM-ddThh:mm:00");
    bindings[":ENDTS"] = m_currentEndTime.toString("yyyy-MM-ddThh:mm:00");

    proglist->FromProgram(querystr, bindings, m_recList);

    QDateTime ts = m_currentStartTime;

    if (type)
    {
        QDateTime tnow = QDateTime::currentDateTime();
        int progPast = 0;
        if (tnow > m_currentEndTime)
            progPast = 100;
        else if (tnow < m_currentStartTime)
            progPast = 0;
        else
        {
            int played = m_currentStartTime.secsTo(tnow);
            int length = m_currentStartTime.secsTo(m_currentEndTime);
            if (length)
                progPast = played * 100 / length;
        }

        type->SetProgPast(progPast);
    }

    ProgramList::iterator program;
    program = proglist->begin();
    vector<ProgramInfo*> unknownlist;
    bool unknown = false;
    ProgramInfo *proginfo = NULL;
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (program != proglist->end() && (ts >= (*program)->endts))
        {
            ++program;
        }

        if ((program == proglist->end()) || (ts < (*program)->startts))
        {
            if (unknown)
            {
                proginfo->spread++;
                proginfo->endts = proginfo->endts.addSecs(5 * 60);
            }
            else
            {
                proginfo = new ProgramInfo;
                unknownlist.push_back(proginfo);
                proginfo->title = unknownTitle;
                proginfo->category = unknownCategory;
                proginfo->startCol = x;
                proginfo->spread = 1;
                proginfo->startts = ts;
                proginfo->endts = proginfo->startts.addSecs(5 * 60);
                unknown = true;
            }
        }
        else
        {
            if (proginfo == *program)
            {
                proginfo->spread++;
            }
            else
            {
                proginfo = *program;
                proginfo->startCol = x;
                proginfo->spread = 1;
                unknown = false;
            }
        }
        m_programInfos[row][x] = proginfo;
        ts = ts.addSecs(5 * 60);
    }

    vector<ProgramInfo*>::iterator it = unknownlist.begin();
    for (; it != unknownlist.end(); ++it)
        proglist->append(*it);

    int ydifference = programRect.height() / DISPLAY_CHANS;
    int xdifference = programRect.width() / DISPLAY_TIMES;

    int arrow = 0;
    int cnt = 0;
    int spread = 1;
    QDateTime lastprog; 
    QRect tempRect;
    bool isCurrent = false;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = m_programInfos[row][x];
        if (!pginfo)
            continue;

        spread = 1;
        if (pginfo->startts != lastprog)
        {
            arrow = 0;
            if (pginfo->startts < firstTime.addSecs(-300))
                arrow = arrow + 1;
            if (pginfo->endts > lastTime.addSecs(2100))
                arrow = arrow + 2;

            if (pginfo->spread != -1)
            {
                spread = pginfo->spread;
            }
            else
            {
                for (int z = x + 1; z < DISPLAY_TIMES; z++)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test && test->startts == pginfo->startts)
                        spread++;
                }
                pginfo->spread = spread;
                pginfo->startCol = x;

                for (int z = x + 1; z < x + spread; z++)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test)
                    {
                        test->spread = spread;
                        test->startCol = x;
                    }
                }
            }

            tempRect = QRect((int)(x * xdifference), (int)(row * ydifference),
                            (int)(xdifference * pginfo->spread), ydifference);

            if (m_currentRow == (int)row && (m_currentCol >= x) &&
                (m_currentCol < (x + spread)))
                isCurrent = true;
            else 
                isCurrent = false;

            if (type)
            {
                int recFlag;
                switch (pginfo->rectype) 
                {
                case kSingleRecord:
                    recFlag = 1;
                    break;
                case kTimeslotRecord:
                    recFlag = 2;
                    break;
                case kChannelRecord:
                    recFlag = 3;
                    break;
                case kAllRecord:
                    recFlag = 4;
                    break;
                case kWeekslotRecord:
                    recFlag = 5;
                    break;
                case kFindOneRecord:
                case kFindDailyRecord:
                case kFindWeeklyRecord:
                    recFlag = 6;
                    break;
                case kOverrideRecord:
                case kDontRecord:
                    recFlag = 7;
                    break;
                case kNotRecording:
                default:
                    recFlag = 0;
                    break;
                }

                int recStat;
                if (pginfo->recstatus == rsConflict ||
                    pginfo->recstatus == rsOffLine)
                    recStat = 2;
                if (pginfo->recstatus <= rsWillRecord)
                    recStat = 1;
                else
                    recStat = 0;

                type->SetProgramInfo(row, cnt, tempRect, pginfo->title,
                                     pginfo->category, arrow, recFlag, 
                                     recStat, isCurrent);

                cnt++;
            }
        }

        lastprog = pginfo->startts;
    } 
}

void GuideGrid::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message == "SCHEDULE_CHANGE")
        {
            m_recList.FromScheduler();
            fillProgramInfos();
            update(fullRect);
        }
    }
}

void GuideGrid::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(channelRect) && paintChannels(&p))
    {
        fillProgramInfos();
        update(programRect|curInfoRect|r);
        return;
    }

    if (r.intersects(infoRect))
        paintInfo(&p);
    if (r.intersects(dateRect) &&
        (jumpToChannelHasRect || (!jumpToChannelHasRect && !jumpToChannel)))
        paintDate(&p);
    if (r.intersects(timeRect))
        paintTimes(&p);
    if (r.intersects(programRect))
        paintPrograms(&p);
    if (r.intersects(curInfoRect))
        paintCurrentInfo(&p);

    // if jumpToChannel has its own rect, use that;
    // otherwise use the date's rect
    if ((jumpToChannelHasRect && r.intersects(jumpToChannelRect)) ||
        (!jumpToChannelHasRect && r.intersects(dateRect)))
        paintJumpToChannel(&p);

    if (r.intersects(videoRect))
        paintVideo(&p);
}

void GuideGrid::paintVideo(QPainter *p)
{
    if (!m_player)
        return;

    if (!using_null_video)
    {
        m_player->DrawUnusedRects(false);
        return;
    }

    PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
    ctx->LockDeleteNVP(__FILE__, __LINE__);
    ctx->nvp->ExposeEvent();
    if (ctx->nvp->UsingNullVideo())
        ctx->DrawARGBFrame(p);
    ctx->UnlockDeleteNVP(__FILE__, __LINE__);
    m_player->ReturnPlayerLock(ctx);
}

void GuideGrid::paintDate(QPainter *p)
{
    QRect dr = dateRect;
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("date_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("date");
        if (type)
            type->SetText(m_currentStartTime.toString(dateformat));
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(dr.topLeft(), pix);
}

void GuideGrid::paintJumpToChannel(QPainter *p)
{
    QString txt = "";
    {
        QMutexLocker locker(&jumpToChannelLock);
        if (jumpToChannel)
            txt = jumpToChannel->GetEntry();
    }

    if (txt.isEmpty())
        return;

    QRect jtcr;
    LayerSet *container = NULL;

    if (jumpToChannelHasRect) {
        jtcr = jumpToChannelRect;
        container = theme->GetSet("jumptochannel");
    } else {
        jtcr = dateRect;
        container = theme->GetSet("date_info");
    }

    QPixmap pix(jtcr.size());
    pix.fill(this, jtcr.topLeft());
    QPainter tmp(&pix);

    if (container)
    {
        UITextType *type = (UITextType *)container->GetType(
            (jumpToChannelHasRect) ? "channel" : "date");

        if (type)
            type->SetText(txt);
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(jtcr.topLeft(), pix);
}

void GuideGrid::paintCurrentInfo(QPainter *p)
{
    QRect dr = curInfoRect;
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("current_info");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(dr.topLeft(), pix);
}

bool GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect;
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());
    QPainter tmp(&pix);
    bool channelsChanged = false;

    LayerSet *container = NULL;
    LayerSet *infocontainer = NULL;
    UIBarType *type = NULL;
    UIImageType *itype = NULL;
    container = theme->GetSet("chanbar");
    infocontainer = theme->GetSet("program_info");
    if (container)
        type = (UIBarType *)container->GetType("chans");
    if (infocontainer)
        itype = (UIImageType *)infocontainer->GetType("icon");

    if (type && type->GetNums() != DISPLAY_CHANS)
        type->SetSize(DISPLAY_CHANS);

    PixmapChannel *chinfo = GetChannelInfo(m_currentStartChannel);

    if (m_player)
        m_player->ClearTunableCache();

    bool showChannelIcon = gContext->GetNumSetting("EPGShowChannelIcon", 0);

    for (unsigned int y = 0; (y < (unsigned int)DISPLAY_CHANS) && chinfo; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
        if (chanNumber >= m_channelInfos.size())
            break;  

        chinfo = GetChannelInfo(chanNumber);

        bool unavailable = false, try_alt = false;

        if (m_player)
        {
            const PlayerContext *ctx = m_player->GetPlayerReadLock(
                -1, __FILE__, __LINE__);
            if (ctx && chinfo)
                try_alt = !m_player->IsTunable(ctx, chinfo->chanid, true);
            m_player->ReturnPlayerLock(ctx);
        }

        if (try_alt)
        {
            unavailable = true;

            // Try alternates with same channum if applicable
            uint alt = GetAlternateChannelIndex(chanNumber, true);
            if (alt != m_channelInfoIdx[chanNumber])
            {
                unavailable = false;
                m_channelInfoIdx[chanNumber] = alt;
                chinfo = GetChannelInfo(chanNumber);
                channelsChanged = true;
            }

            // Try alternates with different channum if applicable
            if (unavailable && GetProgramList(chinfo->chanid).count())
            {
                alt = GetAlternateChannelIndex(chanNumber, false);
                unavailable = (alt == m_channelInfoIdx[chanNumber]);
            }
        }

        if ((y == (unsigned int)2 && scrolltype != 1) || 
            ((signed int)y == m_currentRow && scrolltype == 1))
        {
            if (showChannelIcon && !chinfo->icon.isEmpty())
            {
                int iconsize = 0;
                if (itype)
                    iconsize = itype->GetSize(true).width();
                else if (type)
                    iconsize = type->GetSize();
                if (!chinfo->iconLoaded)
                    chinfo->LoadChannelIcon(iconsize);
                if (chinfo->iconLoaded)
                {
                    if (itype)
                        itype->SetImage(chinfo->iconPixmap);
                }
                else
                    chinfo->icon = QString::null;
            }
        }

        QString tmpChannelFormat = channelFormat;
        if (chinfo->favorite > 0)
        {
            tmpChannelFormat.insert(
                tmpChannelFormat.indexOf('<'), "<MARK:fav>");
        }

        if (unavailable)
        {
            tmpChannelFormat.insert(
                tmpChannelFormat.indexOf('<'), "<MARK:unavail>");
        }

        if (type)
        {
            if (showChannelIcon && !chinfo->icon.isEmpty())
            {
                int iconsize = 0;
                if (itype)
                    iconsize = itype->GetSize(true).width();
                else if (type)
                    iconsize = type->GetSize();
                if (!chinfo->iconLoaded)
                    chinfo->LoadChannelIcon(iconsize);
                if (chinfo->iconLoaded)
                    type->SetIcon(y, chinfo->iconPixmap);
                else
                {
                    chinfo->icon = QString::null;
                    type->ResetImage(y);
                }
            }
            else
            {
                type->ResetImage(y);
            }

            type->SetText(y, chinfo->GetFormatted(tmpChannelFormat));
        }
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }

    tmp.end();
    p->drawPixmap(cr.topLeft(), pix);

    return channelsChanged;
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect tr = timeRect;
    QPixmap pix(tr.size());
    pix.fill(this, tr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = theme->GetSet("timebar");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(tr.topLeft(), pix);
}

void GuideGrid::paintPrograms(QPainter *p)
{
    QRect pr = programRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = theme->GetSet("guide");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void GuideGrid::paintInfo(QPainter *p)
{
    if (m_currentRow < 0 || m_currentCol < 0)
        return;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    if (!pginfo)
        return;

    QMap<QString, QString> infoMap;
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    PixmapChannel *chinfo = GetChannelInfo(chanNum);

    bool showChannelIcon = gContext->GetNumSetting("EPGShowChannelIcon", 0);

    pginfo->ToMap(infoMap);

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        container->ClearAllText();
        container->SetText(infoMap);

        UIImageType *itype = (UIImageType *)container->GetType("icon");
        if (itype && showChannelIcon)
        {
            int iconsize = 0;
            iconsize = itype->GetSize().width();
            if (!chinfo->iconLoaded)
                chinfo->LoadChannelIcon(iconsize);
            if (chinfo->iconLoaded)
                itype->SetImage(chinfo->icon);
            else
                itype->SetImage(QPixmap());
        }

        if (!showChannelIcon || !itype || chinfo->icon.isEmpty())
        {
            UITextType *type = (UITextType *)container->GetType("misicon");
            if (type)
                type->SetText(chinfo->callsign);
        }

        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void GuideGrid::toggleGuideListing()
{
    showFavorites = (!showFavorites);
    generateListings();
}

void GuideGrid::generateListings()
{
    m_currentStartChannel = 0;
    m_currentRow = 0;

    int maxchannel = 0;
    DISPLAY_CHANS = desiredDisplayChans;
    fillChannelInfos();
    maxchannel = max((int)GetChannelCount() - 1, 0);
    DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

    m_recList.FromScheduler();
    fillProgramInfos();
    update(fullRect);
}

void GuideGrid::toggleChannelFavorite()
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Get current channel id, and make sure it exists...
    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    PixmapChannel *ch = GetChannelInfo(chanNum);
    uint favid  = ch->favorite;
    uint chanid = ch->chanid;

    if (favid > 0) 
    {
        query.prepare("DELETE FROM favorites WHERE favid = :FAVID ;");
        query.bindValue(":FAVID", favid);
        query.exec(); 
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        query.prepare("INSERT INTO favorites (chanid) VALUES (:FAVID);");
        query.bindValue(":FAVID", chanid);
        query.exec(); 
    }

    if (showFavorites)
        generateListings();
    else
    {
        int maxchannel = 0;
        DISPLAY_CHANS = desiredDisplayChans;
        fillChannelInfos(false);
        maxchannel = max((int)GetChannelCount() - 1, 0);
        DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

        repaint(channelRect);
    }
}

void GuideGrid::cursorLeft()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];
    
    if (!test)
    {
        scrollLeft();
        return;
    }

    int startCol = test->startCol;
    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        scrollLeft();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        repaint(programRect);
        repaint(infoRect);
        repaint(timeRect);
    }
}

void GuideGrid::cursorRight()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];

    if (!test)
    {
        scrollRight();
        return;
    }

    int spread = test->spread;
    int startCol = test->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > DISPLAY_TIMES - 1)
    {
        m_currentCol = DISPLAY_TIMES - 1;
        scrollRight();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        repaint(programRect);
        repaint(infoRect);
        repaint(timeRect); 
    }
}

void GuideGrid::cursorDown()
{
    if (scrolltype == 1)
    {
        m_currentRow++;

        if (m_currentRow > DISPLAY_CHANS - 1)
        {
            m_currentRow = DISPLAY_CHANS - 1;
            scrollDown();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            repaint(channelRect);
            repaint(programRect);
            repaint(infoRect);
        }
    }
    else
        scrollDown();
}

void GuideGrid::cursorUp()
{
    if (scrolltype == 1)
    {
        m_currentRow--;

        if (m_currentRow < 0)
        {
            m_currentRow = 0;
            scrollUp();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            repaint(channelRect);
            repaint(programRect);
            repaint(infoRect);
        }
    }
    else
        scrollUp();
}

void GuideGrid::scrollLeft()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-30 * 60);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(programRect);
    repaint(infoRect);
    repaint(dateRect);
    repaint(jumpToChannelRect);
    repaint(timeRect);
}

void GuideGrid::scrollRight()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;
    t = m_currentStartTime.addSecs(30 * 60);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(programRect);
    repaint(infoRect);
    repaint(dateRect);
    repaint(jumpToChannelRect);
    repaint(timeRect);
}

void GuideGrid::setStartChannel(int newStartChannel)
{
    if (newStartChannel < 0)
        m_currentStartChannel = newStartChannel + GetChannelCount();
    else if (newStartChannel >= (int) GetChannelCount())
        m_currentStartChannel = newStartChannel - GetChannelCount();
    else
        m_currentStartChannel = newStartChannel;
}

void GuideGrid::scrollDown()
{
    setStartChannel(m_currentStartChannel + 1);

    fillProgramInfos();

    repaint(channelRect);
    repaint(programRect);
    repaint(infoRect);
}

void GuideGrid::scrollUp()
{
    setStartChannel((int)(m_currentStartChannel) - 1);

    fillProgramInfos();

    repaint(channelRect);
    repaint(programRect);
    repaint(infoRect);
}

void GuideGrid::dayLeft()
{
    m_currentStartTime = m_currentStartTime.addSecs(-24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::dayRight()
{
    m_currentStartTime = m_currentStartTime.addSecs(24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::pageLeft()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-5 * 60 * DISPLAY_TIMES);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::pageRight()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(5 * 60 * DISPLAY_TIMES);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::pageDown()
{
    setStartChannel(m_currentStartChannel + DISPLAY_CHANS);

    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::pageUp()
{
    setStartChannel((int)(m_currentStartChannel) - DISPLAY_CHANS);

    fillProgramInfos();

    repaint(fullRect);
}
 
void GuideGrid::showProgFinder()
{
//    RunProgramFind(false, true);

    activateWindow();
    setFocus();

    EmbedTVWindow();
}

void GuideGrid::enter()
{
    if (timeCheck)
        timeCheck->stop();

    unsetCursor();
    selectState = 1;
    accept();
    epgIsVisibleCond.wakeAll();
}

void GuideGrid::escape()
{
    if (timeCheck)
        timeCheck->stop();

    unsetCursor();
    accept();
    epgIsVisibleCond.wakeAll();
}

void GuideGrid::quickRecord()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    pginfo->ToggleRecord();

    m_recList.FromScheduler();
    fillProgramInfos();
    repaint(programRect);
    repaint(infoRect);
}

void GuideGrid::editRecording()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    Qt::FocusPolicy storeFocus = focusPolicy();
    setFocusPolicy(Qt::NoFocus);

    ProgramInfo *temppginfo = new ProgramInfo(*pginfo);
    temppginfo->EditRecording();
    delete temppginfo;

    setFocusPolicy(storeFocus);

    activateWindow();
    setFocus();

    m_recList.FromScheduler();
    fillProgramInfos();
    repaint(fullRect);
}

void GuideGrid::editScheduled()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    Qt::FocusPolicy storeFocus = focusPolicy();
    setFocusPolicy(Qt::NoFocus);

    ProgramInfo *temppginfo = new ProgramInfo(*pginfo);
    temppginfo->EditScheduled();
    delete temppginfo;

    setFocusPolicy(storeFocus);

    activateWindow();
    setFocus();

    m_recList.FromScheduler();
    fillProgramInfos();

    repaint(fullRect);
}

void GuideGrid::customEdit()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                    "customedit", pginfo);
    ce->exec();
    delete ce;
}

void GuideGrid::remove()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo || pginfo->recordid <= 0)
        return;

    ScheduledRecording *record = new ScheduledRecording();
    int recid = pginfo->recordid;
    record->loadByID(recid);

    QString message =
        tr("Delete '%1' %2 rule?").arg(record->getRecordTitle())
                                  .arg(pginfo->RecTypeText());

    bool ok = MythPopupBox::showOkCancelPopup(gContext->GetMainWindow(), "",
                                              message, false);

    if (ok)
    {
        record->remove();
        ScheduledRecording::signalChange(recid);
    }
    record->deleteLater();
}

void GuideGrid::upcoming()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    ProgListerQt *pl = new ProgListerQt(plTitle, pginfo->title, "",
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void GuideGrid::details()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    pginfo->showDetails();
}

void GuideGrid::channelUpdate(void)
{
    if (!m_player)
        return;

    DBChanList sel = GetSelection();

    if (sel.size())
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ChangeChannel(ctx, sel);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::volumeUpdate(bool up)
{
    if (m_player)
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ChangeVolume(ctx, up);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::toggleMute(void)
{
    if (m_player)
    {
        PlayerContext *ctx = m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        m_player->ToggleMute(ctx);
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::GoTo(int start, int cur_row)
{
    setStartChannel(start);
    m_currentRow = cur_row % DISPLAY_CHANS;
    fillProgramInfos();
    repaint(fullRect);
}

void GuideGrid::SetJumpToChannel(JumpToChannel *ptr)
{
    QMutexLocker locker(&jumpToChannelLock);
    jumpToChannel = ptr;
}

void GuideGrid::EmbedTVWindow(void)
{
    previewVideoRefreshTimer->stop();
    if (m_player && m_player->IsRunning() && 
        videoRect.height() > 1 && videoRect.width() > 1)
    {
        PlayerContext *ctx =
            m_player->GetPlayerReadLock(-1, __FILE__, __LINE__);
        using_null_video =
            !m_player->StartEmbedding(ctx, this->winId(), videoRect);
        if (!using_null_video)
        {
            QRegion r1 = QRegion(fullRect);
            QRegion r2 = QRegion(videoRect);
            setMask(r1.xored(r2));
            m_player->DrawUnusedRects(false, ctx);
        }
        else
        {
            previewVideoRefreshTimer->start(66);
        }
        m_player->ReturnPlayerLock(ctx);
    }
}

void GuideGrid::refreshVideo(void)
{
    if (m_player && m_player->IsRunning() && using_null_video)
        update(videoRect);
}
