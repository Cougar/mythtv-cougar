#include <qapplication.h>
#include <qpainter.h>
#include <qfont.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qaccel.h>
#include <math.h>
#include <qcursor.h>
#include <qapplication.h>

#include <iostream>
using namespace std;

#include "guidegrid.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "settings.h"

extern Settings *globalsettings;

GuideGrid::GuideGrid(const QString &channel, QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    screenheight = QApplication::desktop()->height();
    screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    wmult = screenwidth / 800.0;
    hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setCursor(QCursor(Qt::BlankCursor));

    usetheme = globalsettings->GetNumSetting("ThemeQt");

    if (usetheme)
    {
        bgcolor = QColor(globalsettings->GetSetting("BackgroundColor"));
        fgcolor = QColor(globalsettings->GetSetting("ForegroundColor"));
    }
    else
    {
        bgcolor = QColor("white");
        fgcolor = QColor("black");
    }

    setPalette(QPalette(bgcolor));
    m_font = new QFont("Arial", (int)(11 * hmult), QFont::Bold);
    m_largerFont = new QFont("Arial", (int)(13 * hmult), QFont::Bold);

    m_originalStartTime = QDateTime::currentDateTime();

    int secsoffset = -(m_originalStartTime.time().minute() % 30) * 60;
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_currentStartChannel = 0;
    m_startChanStr = channel;

    m_currentRow = 0;
    m_currentCol = 0;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        m_timeInfos[x] = NULL;
        for (int y = 0; y < DISPLAY_CHANS; y++)
            m_programInfos[y][x] = NULL;
    }

    QTime clock = QTime::currentTime();
    clock.start();
    fillTimeInfos();
    int filltime = clock.elapsed();
    clock.restart();
    fillChannelInfos();
    int fillchannels = clock.elapsed();
    clock.restart();
    fillProgramInfos();
    int fillprogs = clock.elapsed();

    cout << filltime << " " << fillchannels << " " << fillprogs << endl;

    QAccel *accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_A), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_D), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_S), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_W), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_C), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(enter()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(enter()));

    accel->connectItem(accel->insertItem(Key_I), this, SLOT(displayInfo()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(displayInfo()));

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    selectState = false;
    showInfo = false;

    showFullScreen();
    setActiveWindow();
    raise();
    setFocus();
}

GuideGrid::~GuideGrid()
{
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];

        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
        }
    }

    m_channelInfos.clear();
}

QString GuideGrid::getLastChannel(void)
{
    unsigned int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= m_channelInfos.size())
        chanNum -= m_channelInfos.size();

    if (selectState)
        return m_channelInfos[chanNum].chanstr;
    return 0;
}

void GuideGrid::fillChannelInfos()
{
    m_channelInfos.clear();

    QString thequery;
    QSqlQuery query;
    
    thequery = "SELECT channum,callsign,icon,chanid FROM channel "
               "ORDER BY channum + 0;";
    query.exec(thequery);
    
    bool set = false;
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ChannelInfo val;
            val.callsign = query.value(1).toString();
            if (val.callsign == QString::null)
                val.callsign = "";
            val.iconpath = query.value(2).toString();
            if (val.iconpath == QString::null)
                val.iconpath = "";
            val.chanstr = query.value(0).toString();
            if (val.chanstr == QString::null)
                val.chanstr = "";
            val.chanid = query.value(3).toInt();
            val.icon = NULL;
        
            if (val.chanstr == m_startChanStr && !set)
            {
                m_currentStartChannel = m_channelInfos.size();
                set = true;
            }
		
            m_channelInfos.push_back(val);
        }
    }
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

    char temp[512];
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        TimeInfo *timeinfo = new TimeInfo;

        int year = t.date().year();
        int month = t.date().month();
        int day = t.date().day();

        int hour = t.time().hour();
        int mins = t.time().minute();

        int sqlmins;        

        mins = 5 * (mins / 5);
        sqlmins = mins + 2;

        bool am = true;
        if (hour >= 12)
            am = false;

        if (hour == 0)
            sprintf(temp, "12:%02d AM", mins);
        else
        {
            if (hour > 12)
                sprintf(temp, "%02d:%02d ", hour - 12, mins);
            else 
                sprintf(temp, "%02d:%02d ", hour, mins);
            strcat(temp, (am ? "AM" : "PM"));
        }
        timeinfo->year = year;
        timeinfo->month = month;
        timeinfo->day = day;
        timeinfo->hour = hour;
        timeinfo->min = mins;

        timeinfo->usertime = temp;

        sprintf(temp, "%4d%02d%02d%02d%02d50", year, month, day, hour, sqlmins);
        timeinfo->sqltime = temp;

        m_timeInfos[x] = timeinfo;

        t = t.addSecs(5 * 60);
    }
}

ProgramInfo *GuideGrid::getProgramInfo(unsigned int row, unsigned int col)
{
    unsigned int chanNum = row + m_currentStartChannel;
    if (chanNum >= m_channelInfos.size())
        chanNum -= m_channelInfos.size();

    if (m_channelInfos[chanNum].chanstr == "" || !m_timeInfos[col])
        return NULL;

    QString chanstr = QString("%1").arg(m_channelInfos[chanNum].chanid);

    ProgramInfo *pginfo = GetProgramAtDateTime(chanstr,
                                             m_timeInfos[col]->sqltime.ascii());
    if (pginfo)
    {
        pginfo->startCol = col;
        return pginfo;
    }
  
    ProgramInfo *proginfo = new ProgramInfo;
    proginfo->title = "Unknown"; 
    TimeInfo *tinfo = m_timeInfos[col];
    QDateTime ts;
    ts.setDate(QDate(tinfo->year, tinfo->month, tinfo->day));
    ts.setTime(QTime(tinfo->hour, tinfo->min));
    proginfo->startts = ts;
    if (col < DISPLAY_TIMES - 1)
    { 
        tinfo = m_timeInfos[col + 1];
        ts.setDate(QDate(tinfo->year, tinfo->month, tinfo->day));
        ts.setTime(QTime(tinfo->hour, tinfo->min));
    }
    proginfo->endts = ts;
    proginfo->spread = 1;
    proginfo->startCol = col;
    return proginfo;
}

void GuideGrid::fillProgramInfos(void)
{
    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
            m_programInfos[y][x] = NULL;
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            ProgramInfo *pginfo = getProgramInfo(y, x);
            if (!pginfo)
                pginfo = new ProgramInfo;
            m_programInfos[y][x] = pginfo;
        }
    }
}

void GuideGrid::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (showInfo)
    {
    }
    else
    {
        if (r.intersects(dateRect()))
            paintDate(&p);
        if (r.intersects(channelRect()))
            paintChannels(&p);
        if (r.intersects(timeRect()))
            paintTimes(&p);
        if (r.intersects(programRect()))
            paintPrograms(&p);
    }
}

void GuideGrid::paintDate(QPainter *p)
{
    QRect dr = dateRect();
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_largerFont);

    QString date = m_currentStartTime.toString("ddd");
    QFontMetrics lfm(*m_largerFont);
    int datewidth = lfm.width(date);
    int dateheight = lfm.height();

    tmp.drawText((dr.width() - datewidth) / 2,
                 (dr.height() - dateheight) / 2 + dateheight / 3, date);

    date = m_currentStartTime.toString("MMM d");
    datewidth = lfm.width(date);

    tmp.drawText((dr.width() - datewidth) / 2,
                 (dr.height() - dateheight) / 2 + dateheight * 4 / 3, date);

    tmp.drawLine(0, dr.bottom(), dr.right() + 1, dr.bottom());
    tmp.drawLine(dr.right(), 0, dr.right(), dr.bottom() + 1);

    tmp.end();

    p->drawPixmap(dr.topLeft(), pix);
}

void GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_font);

    int ydifference = (int)(ceil(cr.height() * 1.0 / DISPLAY_CHANS));

    for (unsigned int y = 1; y < DISPLAY_CHANS + 1; y++)
    {
        tmp.drawLine(0, ydifference * y, cr.right(), ydifference * y);
    }
    tmp.drawLine(cr.right(), 0, cr.right(), cr.bottom());

    for (unsigned int y = 0; y < DISPLAY_CHANS; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
  
        if (m_channelInfos[chanNumber].chanstr == "")
            break;

        ChannelInfo *chinfo = &(m_channelInfos[chanNumber]);
        if (chinfo->iconpath != "none" && chinfo->iconpath != "")
        {
            if (!chinfo->icon)
                chinfo->LoadIcon();
            if (chinfo->icon)
            {
                int yoffset = (int)(6 * hmult);
                tmp.drawPixmap((cr.width() - chinfo->icon->width()) / 2, 
                               ydifference * y + yoffset, *(chinfo->icon));
            }
        }
        tmp.setFont(*m_largerFont);
        QFontMetrics lfm(*m_largerFont);
        int width = lfm.width(chinfo->chanstr);
        int bheight = lfm.height();
        
        int yoffset = (int)(36 * hmult);
        tmp.drawText((cr.width() - width) / 2, 
                     ydifference * y + yoffset + bheight, chinfo->chanstr);

        tmp.setFont(*m_font);
        QFontMetrics fm(*m_font);
        width = fm.width(chinfo->callsign);
        int height = fm.height();
        tmp.drawText((cr.width() - width) / 2, 
                     ydifference * y + yoffset + bheight + height, 
                     chinfo->callsign);
    }

    tmp.end();
    
    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect tr = timeRect();
    QPixmap pix(tr.size());
    pix.fill(this, tr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_largerFont);

    int xdifference = (int)(tr.width() / DISPLAY_TIMES); 
    int xslop = (tr.width() % DISPLAY_TIMES) / (DISPLAY_TIMES / 6);

    for (int x = 1; x < DISPLAY_TIMES + 1; x++)
    {
        if (m_timeInfos[x]->min % 30 == 0)
        {
            tmp.drawLine(x * xdifference + (x / 6) * xslop, 0, 
                         x * xdifference + (x / 6) * xslop, tr.bottom());
        }
    }

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x]->min % 30 == 0)
        {
            TimeInfo *tinfo = m_timeInfos[x];

            QFontMetrics fm(*m_largerFont);
            int width = fm.width(tinfo->usertime);
            int height = fm.height();

            int xpos = (x * xdifference + (x / 6) * xslop) + 
                       ((xdifference * 6 - width) / 2);
            tmp.drawText(xpos, (tr.bottom() - height) / 2 + height, 
                         tinfo->usertime);
        }
    }

    tmp.drawLine(0, tr.bottom(), tr.right(), tr.bottom());

    tmp.end();

    p->drawPixmap(tr.topLeft(), pix);
}

QBrush GuideGrid::getBGColor(const QString &category)
{
    QBrush br = QBrush(bgcolor);
   
    if (!usetheme)
        return br;

    QString cat = "Cat_" + category;

    QString color = globalsettings->GetSetting(cat);
    if (color != "")
    {
        br = QBrush(color);
    }

    return br;
}

void GuideGrid::paintPrograms(QPainter *p)
{
    QRect pr = programRect();
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());

    QPainter tmp(&pix);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

    tmp.setFont(*m_largerFont);

    int ydifference = (int)ceil(pr.height() * 1.0 / DISPLAY_CHANS);
    int xdifference = (int)(pr.width() / DISPLAY_TIMES);
    int xslop = (pr.width() % DISPLAY_TIMES) / (DISPLAY_TIMES / 6);
 
    for (int y = 1; y < DISPLAY_CHANS + 1; y++)
    {
        int ypos = ydifference * y;
        tmp.drawLine(0, ypos, pr.right(), ypos);
    }

    tmp.drawLine(0, 0, 0, 0);
    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        unsigned int chanNum = y + m_currentStartChannel;
        if (chanNum >= m_channelInfos.size())
            chanNum -= m_channelInfos.size();

        if (m_channelInfos[chanNum].chanstr == "")
            break;

        QDateTime lastprog;
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];

            if (pginfo->recordtype == -1)
                pginfo->GetProgramRecordingStatus();

            int spread = 1;
            if (pginfo->startts != lastprog)
            {
                QFontMetrics fm(*m_largerFont);
                int height = fm.height();

                if (pginfo->spread != -1)
                {
                    spread = pginfo->spread;
                }
                else
                {
                    for (int z = x + 1; z < DISPLAY_TIMES; z++)
                    {
                        if (m_programInfos[y][z]->startts == pginfo->startts)
                            spread++;
                    }
                    pginfo->spread = spread;
                    pginfo->startCol = x;

                    for (int z = x + 1; z < x + spread; z++)
                    {
                        m_programInfos[y][z]->spread = spread;
                        m_programInfos[y][z]->startCol = x;
                        m_programInfos[y][z]->recordtype = pginfo->recordtype;
                    }
                }

                QBrush br = getBGColor(pginfo->category);

                int startx = (int)(x * xdifference + 1 * wmult + 
                             (x / 6) * xslop);
                int endx = (int)((x + spread) * xdifference - 2 * wmult +
                            ((x + spread) / 6) * xslop);
                tmp.fillRect(startx, (int)(ydifference * y + 1 * hmult),
                             endx, (int)(ydifference - 2 * hmult), br);

                int maxwidth = (int)(spread * xdifference - (10 * wmult) +
                               (spread / 6) * xslop);

                QString info = pginfo->title;
                if (pginfo->category != "" && usetheme)
                    info += " (" + pginfo->category + ")";
                
                startx = (int)(x * xdifference + (x / 6) * xslop + 7 * wmult);
                tmp.drawText(startx,
                             (int)(height / 8 + y * ydifference + 1 * hmult), 
                             maxwidth, ydifference,
                             AlignLeft | WordBreak,
                             info);

                tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

                startx = (int)((x + spread) * xdifference + 
                               ((x + spread) / 6) * xslop);
                tmp.drawLine(startx, ydifference * y, startx, 
                             ydifference * (y + 1));

                if (pginfo->recordtype > 0)
                {
                    tmp.setPen(QPen(red, (int)(2 * wmult)));
                    QString text;

                    if (pginfo->recordtype == 1)
                        text = "R";
                    else if (pginfo->recordtype == 2)
                        text = "T";
                    else if (pginfo->recordtype == 3) 
                        text = "A";

                    int width = fm.width(text);

                    startx = (int)((x + spread) * xdifference + 
                                  ((x + spread) / 6 * xslop) - width * 1.5);
                    tmp.drawText(startx, (y + 1) * ydifference - height,
                                 (int)(width * 1.5), height, AlignLeft, text);
             
                    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                }

                if (m_currentRow == (int)y)
                {
                    if ((m_currentCol >= x) && (m_currentCol < (x + spread)))
                    {
                        tmp.setPen(QPen(red, (int)(3.75 * wmult)));

                        int ystart = 1;
                        int rectheight = (int)(ydifference - 3 * hmult);
                        if (y != 0)
                        {
                            ystart = (int)(ydifference * y + 2 * hmult);
                            rectheight = (int)(ydifference - 4 * hmult);
                        }

                        int xstart = 1;
                        if (x != 0)
                            xstart = (int)(x * xdifference + 2 * wmult +
                                          (x / 6) * xslop);
                        int xend = (int)(xdifference * spread - 4 * wmult +
                                         ceil(spread / 6.0) * xslop);
                        if (x == 0)
                            xend += (int)(1 * wmult);
			
                        tmp.drawRect(xstart, ystart, xend, rectheight);
                        tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                    }
                }
            }
            lastprog = pginfo->startts;
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

QRect GuideGrid::fullRect() const
{
    QRect r(0, 0, (int)(800 * wmult), (int)(600 * hmult));
    return r;
}

QRect GuideGrid::dateRect() const
{
    QRect r(0, 0, (int)(74 * wmult), (int)(49 * hmult));
    return r;
}

QRect GuideGrid::channelRect() const
{
    QRect r(0, (int)(49 * hmult), (int)(74 * wmult), (int)((600 - 49) * hmult));
    return r;
}

QRect GuideGrid::timeRect() const
{
    QRect r((int)(74 * wmult), 0, (int)((800 - 74) * wmult), (int)(49 * hmult));
    return r;
}

QRect GuideGrid::programRect() const
{
    QRect r((int)(74 * wmult), (int)(49 * hmult), (int)((800 - 74) * wmult), 
            (int)((600 - 49) * hmult));
    return r;
}

void GuideGrid::cursorLeft()
{
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        emit scrollLeft();
        emit scrollLeft();
        emit scrollLeft();
        emit scrollLeft();
        emit scrollLeft();
        emit scrollLeft();
    }
    else
        update(programRect());
}

void GuideGrid::cursorRight()
{
    int spread = m_programInfos[m_currentRow][m_currentCol]->spread;
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > DISPLAY_TIMES - 1)
    {
        m_currentCol = DISPLAY_TIMES - 1;
        emit scrollRight();
        emit scrollRight();
        emit scrollRight();
        emit scrollRight();
        emit scrollRight();
        emit scrollRight();
    }
    else
        update(programRect());
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > DISPLAY_CHANS - 1)
    {
        m_currentRow = DISPLAY_CHANS - 1;
        emit scrollDown();
    }
    else
        update(programRect());
}

void GuideGrid::cursorUp()
{
    m_currentRow--;

    if (m_currentRow < 0)
    {
        m_currentRow = 0;
        emit scrollUp();
    }
    else
        update(programRect());
}

void GuideGrid::scrollLeft()
{
    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-5 * 60);

    bool updatedate = false;
    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        ProgramInfo *pginfo = m_programInfos[y][DISPLAY_TIMES - 1];
        if (pginfo)
            delete pginfo;
        m_programInfos[y][DISPLAY_TIMES - 1] = NULL;
    }

    for (int x = DISPLAY_TIMES - 1; x > 0; x--)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            m_programInfos[y][x] = m_programInfos[y][x - 1];
            m_programInfos[y][x]->spread = -1;
            m_programInfos[y][x]->startCol = x;
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        ProgramInfo *pginfo = getProgramInfo(y, 0);
        m_programInfos[y][0] = pginfo;
    }

    update(programRect().unite(timeRect()));
    if (updatedate) 
        update(dateRect());
}

void GuideGrid::scrollRight()
{
    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(5 * 60);

    bool updatedate = false;
    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        ProgramInfo *pginfo = m_programInfos[y][0];
        if (pginfo)
            delete pginfo;
        m_programInfos[y][0] = NULL;
    }

    for (int x = 0; x < DISPLAY_TIMES - 1; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            m_programInfos[y][x] = m_programInfos[y][x + 1];
            m_programInfos[y][x]->spread = -1;
            m_programInfos[y][x]->startCol = x;
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        ProgramInfo *pginfo = getProgramInfo(y, DISPLAY_TIMES - 1);
        m_programInfos[y][DISPLAY_TIMES - 1] = pginfo;
    }

    update(programRect().unite(timeRect()));
    if (updatedate)
        update(dateRect());
}

void GuideGrid::scrollDown()
{
    m_currentStartChannel++;
    if (m_currentStartChannel >= m_channelInfos.size())
        m_currentStartChannel -= m_channelInfos.size();

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = m_programInfos[0][x];
        if (pginfo)
            delete pginfo;
        m_programInfos[0][x] = NULL;
    }

    for (int y = 0; y < DISPLAY_CHANS - 1; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y + 1][x];
        }
    } 

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(DISPLAY_CHANS - 1, x);
        m_programInfos[DISPLAY_CHANS - 1][x] = pginfo;
    }

    update(programRect().unite(channelRect()));
}

void GuideGrid::scrollUp()
{
    if (m_currentStartChannel == 0)
        m_currentStartChannel = m_channelInfos.size() - 1;
    else
        m_currentStartChannel--;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = m_programInfos[DISPLAY_CHANS - 1][x];
        if (pginfo)
            delete pginfo;
        m_programInfos[DISPLAY_CHANS - 1][x] = NULL;
    }

    for (int y = DISPLAY_CHANS - 1; y > 0; y--)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y - 1][x];
        }
    } 

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(0, x);
        m_programInfos[0][x] = pginfo;
    }

    update(programRect().unite(channelRect()));
}

void GuideGrid::enter()
{
    unsetCursor();
    selectState = 1;
    emit killTheApp();
}

void GuideGrid::escape()
{
    unsetCursor();
    emit killTheApp();
}

void GuideGrid::displayInfo()
{
    showInfo = 1;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (pginfo)
    {
        InfoDialog diag(pginfo, this, "Program Info");
        diag.setCaption("BLAH!!!");
        diag.exec();
    }

    showInfo = 0;

    pginfo->GetProgramRecordingStatus();

    m_programInfos[m_currentRow][m_currentCol] = pginfo;

    setActiveWindow();
    setFocus();

    update(programRect());
}
