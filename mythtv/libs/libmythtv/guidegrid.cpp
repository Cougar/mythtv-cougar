#include "guidegrid.h"

#include <qpainter.h>
#include <qfont.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qaccel.h>

GuideGrid::GuideGrid(int channel, QWidget *parent, const char *name)
         : QWidget(parent, name)
{
    setPalette(QPalette(QColor(250, 250, 250)));
    m_font = new QFont("Arial", 11, QFont::Bold);
    m_largerFont = new QFont("Arial", 13, QFont::Bold);

    m_currentStartTime = QDateTime::currentDateTime();
    m_currentStartChannel = channel;

    m_currentRow = 0;
    m_currentCol = 0;

    for (int y = 0; y < 10; y++)
    {
        m_channelInfos[y] = NULL;
        m_timeInfos[y] = NULL;
        for (int x = 0; x < 10; x++)
            m_programInfos[x][y] = NULL;
    }

    fillTimeInfos();
    fillChannelInfos();
    fillProgramInfos();

    QAccel *accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));
}

ChannelInfo *GuideGrid::getChannelInfo(int channum)
{
    char thequery[512];
    QSqlQuery query;

    sprintf(thequery, "SELECT channum,callsign,icon FROM channel WHERE "
                      "channum = %d;", channum);
    query.exec(thequery);

    ChannelInfo *retval = NULL;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        retval = new ChannelInfo;
        retval->callsign = query.value(1).toString();
        retval->iconpath = query.value(2).toString();
        retval->chanstr = query.value(0).toString();
        retval->channum = channum;
        retval->icon = NULL;
    }

    return retval;   
}

void GuideGrid::fillChannelInfos()
{
    for (int x = 0; x < 10; x++)
    {
        if (m_channelInfos[x])
            delete m_channelInfos[x];
        m_channelInfos[x] = NULL;
    }

    int channum = m_currentStartChannel;
    for (int y = 0; y < 6; y++)
    {
        bool done = false;
        ChannelInfo *chinfo;
 
        while (!done)
        {
            if ((chinfo = getChannelInfo(channum)) != NULL)
            { 
                done = true;
                break;
            }
            channum++;
            if (channum > CHANNUM_MAX)
                channum = 0;
        }

        if (!done)
            break;

        m_currentEndChannel = chinfo->channum;
        m_channelInfos[y] = chinfo;
        channum++;
        if (channum > CHANNUM_MAX)
            channum = 0;
    }

    m_currentStartChannel = m_channelInfos[0]->channum;
}

void GuideGrid::fillTimeInfos()
{
    for (int x = 0; x < 10; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
        m_timeInfos[x] = NULL;
    }

    QDateTime t = m_currentStartTime;

    char temp[512];
    for (int x = 0; x < 5; x++)
    {
        TimeInfo *timeinfo = new TimeInfo;

        int year = t.date().year();
        int month = t.date().month();
        int day = t.date().day();

        QTime midnight(0, 0, 0);

        int secs = midnight.secsTo(t.time());

        int hour = secs / 60 / 60;
        int mins = secs / 60 - hour * 60;

        if (mins > 30)
            mins = 30;
        else
            mins = 0;

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
        timeinfo->usertime = temp;

        sprintf(temp, "%4d%02d%02d%02d%02d50", year, month, day, hour, mins);
        timeinfo->sqltime = temp;

        m_timeInfos[x] = timeinfo;

        t = t.addSecs(1800);
    }
}

ProgramInfo *GuideGrid::getProgramInfo(unsigned int row, unsigned int col)
{
    char thequery[512];
    QSqlQuery query;

    if (!m_channelInfos[row] || !m_timeInfos[col])
        return NULL;

    sprintf(thequery, "SELECT title,subtitle,description,category,starttime,"
                      "endtime,channum FROM program WHERE channum = %d AND "
                      "starttime < %s and endtime > %s;", 
                      m_channelInfos[row]->channum, 
                      m_timeInfos[col]->sqltime.ascii(),
                      m_timeInfos[col]->sqltime.ascii());

    query.exec(thequery);
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ProgramInfo *proginfo = new ProgramInfo;
        proginfo->title = query.value(0).toString();
        proginfo->subtitle = query.value(1).toString();
        proginfo->description = query.value(2).toString();
        proginfo->category = query.value(3).toString();
        proginfo->starttime = query.value(4).toString();
        proginfo->endtime = query.value(5).toString();
        proginfo->channum = query.value(6).toString();
        proginfo->spread = -1;
        proginfo->startCol = col;
 
        return proginfo;
    }
    return NULL;
}

void GuideGrid::fillProgramInfos(void)
{
    for (int y = 0; y < 10; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
            m_programInfos[y][x] = NULL;
        }
    }

    for (int y = 0; y < 6; y++)
    {
        for (int x = 0; x < 6; x++)
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

    if (r.intersects(programRect()))
        paintPrograms(&p);  
    if (r.intersects(channelRect()))
        paintChannels(&p);
    if (r.intersects(timeRect()))
        paintTimes(&p);
}

void GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(black);
    tmp.setPen(QPen(black, 2));
    tmp.setFont(*m_font);

    for (unsigned int i = 0; i < 6; i++)
    {
        tmp.drawLine(0, i * 92 + 48, 74, i * 92 + 48);
    }
    tmp.drawLine(74, 0, 74, 600);

    for (unsigned int i = 0; i < 6; i++)
    {
        if (!m_channelInfos[i])
            break;

        ChannelInfo *chinfo = m_channelInfos[i];
        if (chinfo->iconpath != "none")
        {
            if (!chinfo->icon)
                chinfo->LoadIcon();
            tmp.drawPixmap((75 - chinfo->icon->width()) / 2, i * 92 + 55,
                           *(chinfo->icon));
        }
        tmp.setFont(*m_largerFont);
        QFontMetrics lfm(*m_largerFont);
        int width = lfm.width(chinfo->chanstr);
        int bheight = lfm.height();
            
        tmp.drawText((75 - width) / 2, i * 92 + 55 + 30 + bheight, 
                     chinfo->chanstr);

        tmp.setFont(*m_font);
        QFontMetrics fm(*m_font);
        width = fm.width(chinfo->callsign);
        int height = fm.height();
        tmp.drawText((75 - width) / 2, i * 92 + 55 + 30 + bheight + height,
                     chinfo->callsign);
    }

    tmp.end();
    
    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect cr = timeRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(black);
    tmp.setPen(QPen(black, 2));
    tmp.setFont(*m_font);

    for (int i = 0; i < 5; i++)
    {
        tmp.drawLine(i * 145, 0, i * 145, 48);
    }

    for (int i = 0; i < 5; i++)
    {
        TimeInfo *tinfo = m_timeInfos[i];

        QFontMetrics fm(*m_font);
        int width = fm.width(tinfo->usertime);
        int height = fm.height();

        tmp.drawText((145 - width) / 2 + i * 145, (48 - height) / 2 + height, 
                     tinfo->usertime);
    }

    tmp.drawLine(0, 48, 800, 48);

    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintPrograms(QPainter *p)
{
    QRect cr = programRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setPen(QPen(black, 2));

    tmp.setFont(*m_largerFont);

    for (int i = 1; i < 6; i++)
    {
        tmp.drawLine(0, i * 92 - 1, 800, i * 92 - 1);
    }

    for (unsigned int y = 0; y < 6; y++)
    {
        if (!m_channelInfos[y])
            break;

        QString lastprog;
        for (int x = 0; x < 5; x++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];

            int spread = 1;
            if (pginfo->starttime != lastprog)
            {
                QFontMetrics fm(*m_largerFont);
                int height = fm.height();

                if (pginfo->spread != -1)
                {
                    spread = pginfo->spread;
                }
                else
                {
                    for (int z = x + 1; z < 5; z++)
                    {
                        if (m_programInfos[y][z]->starttime == 
                            pginfo->starttime)
                            spread++;
                    }
                    pginfo->spread = spread;
                    pginfo->startCol = x;

                    for (int z = x + 1; z < x + spread; z++)
                    {
                        m_programInfos[y][z]->spread = spread;
                        m_programInfos[y][z]->startCol = x;
                    }
                }
                
                int maxwidth = spread * 145 - 15;

                tmp.drawText(10 + x * 145, 
                             height / 2 + 92 * y, maxwidth, 92,
                             AlignLeft | WordBreak,
                             pginfo->title);

                tmp.setPen(QPen(black, 2));
                if (x != 4)
                    tmp.drawLine((x + spread) * 145, 92 * y - 1, 
                                 (x + spread) * 145, 92 * (y + 1) - 1);
            }
            lastprog = pginfo->starttime;
        }
    }

    if (m_currentRow != -1 && m_currentCol != -1)
    {
        tmp.setPen(QPen(red, 2));

        int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;
        int spread = m_programInfos[m_currentRow][m_currentCol]->spread;

        tmp.drawRect(startCol * 145 + 2, m_currentRow * 92 + 1,
                     142 + 145 * (spread - 1), 92 - 3);
    }
    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

QRect GuideGrid::channelRect() const
{
    QRect r(0, 0, 75, 600);
    return r;
}

QRect GuideGrid::timeRect() const
{
    QRect r(74, 0, 800, 49);
    return r;
}

QRect GuideGrid::programRect() const
{
    QRect r(74, 49, 800, 600);
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
    }
    else
        update(programRect());
}

void GuideGrid::cursorRight()
{
    int spread = m_programInfos[m_currentRow][m_currentCol]->spread;
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > 4)
    {
        m_currentCol = 4;
        emit scrollRight();
    }
    else
        update(programRect());
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > 5)
    {
        m_currentRow = 5;
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

    t = m_currentStartTime.addSecs(-1800);

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    update(programRect().unite(timeRect()));
}

void GuideGrid::scrollRight()
{
    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(1800);
    
    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    update(programRect().unite(timeRect()));
}

void GuideGrid::scrollDown()
{
    m_currentStartChannel++;

    fillChannelInfos();
    fillProgramInfos();

    update(programRect().unite(channelRect()));
}

void GuideGrid::scrollUp()
{
    bool done = false;
 
    while (!done)
    {
        m_currentStartChannel--;
 
        if (m_currentStartChannel < 2)
            m_currentStartChannel = CHANNUM_MAX;

        ChannelInfo *chinfo = getChannelInfo(m_currentStartChannel);
        if (chinfo)
        {
            done = true;
            delete chinfo;
        }
    }

    fillChannelInfos();
    fillProgramInfos();

    update(programRect().unite(channelRect()));
}

