#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <qwidget.h>
#include <qlabel.h>
#include <qdialog.h>
#include <qstring.h>
#include <qtimer.h>
#include <qpixmap.h>
#include <qdatetime.h>
#include <qptrlist.h>
#include <vector>

using namespace std;

class QFont;
class ProgramInfo;
class TimeInfo;
class ChannelInfo;
class MythContext;
class Settings;

namespace libmyth 
{

#define MAX_DISPLAY_CHANS 8
#define DISPLAY_TIMES 30

class GuideGrid : public QDialog
{
    Q_OBJECT
  public:
    GuideGrid(MythContext *context, const QString &channel, 
              QWidget *parent = 0, const char *name = 0);
   ~GuideGrid();

    QString getLastChannel(void);

  signals:
    void killTheApp();

  protected slots:
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();

    void scrollLeft();
    void scrollRight();
    void scrollDown();
    void scrollUp();

    void dayLeft();
    void dayRight();
    void pageLeft();
    void pageRight();
    void pageDown();
    void pageUp();

    void enter();
    void escape();

    void displayInfo();

  protected:
    void paintEvent(QPaintEvent *);

  private slots:
    void timeout();

  private:
    void paintDate(QPainter *p);
    void paintChannels(QPainter *p);
    void paintTimes(QPainter *p);
    void paintPrograms(QPainter *p);
    void paintTitle(QPainter *p);

    QRect fullRect() const;
    QRect dateRect() const;
    QRect channelRect() const;
    QRect timeRect() const;
    QRect programRect() const;
    QRect titleRect() const;
    QRect infoRect() const;

    void fillChannelInfos();

    void fillTimeInfos();

    void fillProgramInfos(void);
    void fillProgramRowInfos(unsigned int row);

    QBrush getBGColor(const QString &category);
    
    void setStartChannel(int newStartChannel);

    void updateTopInfo();
    void createProgramLabel(int, int);
    void setupColorScheme();

    QString getDateLabel(ProgramInfo *pginfo);

    QLabel *titlefield;
    QLabel *channelimage;
    QLabel *recordingfield;
    QLabel *date;
    QLabel *subtitlefield;
    QLabel *descriptionfield;
    QLabel *currentTime;
    QLabel *currentChan;

    QFont *m_timeFont;
    QFont *m_chanFont;
    QFont *m_chanCallsignFont;
    QFont *m_progFont;
    QFont *m_titleFont;

    vector<ChannelInfo> m_channelInfos;
    TimeInfo *m_timeInfos[DISPLAY_TIMES];
    QPtrList<ProgramInfo> *m_programs[MAX_DISPLAY_CHANS];
    ProgramInfo *m_programInfos[MAX_DISPLAY_CHANS][DISPLAY_TIMES];

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    QDateTime m_currentEndTime;
    unsigned int m_currentStartChannel;
    QString m_startChanStr;
    
    int m_currentRow;
    int m_currentCol;

    bool selectState;
    bool showInfo;
    bool showIcon;

    int screenwidth, screenheight;
    float wmult, hmult;

    bool showtitle;
    bool usetheme;
    QColor fgcolor;
    QColor bgcolor;

    int startChannel;
    int programGuideType;
    int DISPLAY_CHANS;

    QDateTime firstTime;
    QDateTime lastTime;

    QColor curTimeChan_bgColor;
    QColor curTimeChan_fgColor;
    QColor date_bgColor;
    QColor date_dsColor;
    QColor date_fgColor;
    QColor chan_bgColor;
    QColor chan_dsColor;
    QColor chan_fgColor;
    QColor time_bgColor;
    QColor time_dsColor;
    QColor time_fgColor;
    QColor prog_bgColor;
    QColor prog_fgColor;
    QColor progLine_Color;
    QColor progArrow_Color;
    QColor curProg_bgColor;
    QColor curRecProg_bgColor;
    QColor curProg_dsColor;
    QColor curProg_fgColor;
    QColor misChanIcon_bgColor;
    QColor misChanIcon_fgColor;
    int progArrow_Type;

    MythContext *m_context;
    Settings *m_settings;
};

}
#endif
