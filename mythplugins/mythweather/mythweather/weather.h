#ifndef WEATHER_H_
#define WEATHER_H_

// QT headers
#include <QList>

// MythTV headers
#include <mythscreentype.h>
#include <mythuitext.h>
#include <mythmainwindow.h>

// MythWeather headers
#include "weatherUtils.h"

class SourceManager;
class WeatherScreen;

typedef QList<WeatherScreen*> ScreenList;

class Weather : public MythScreenType
{
    Q_OBJECT

  public:
    Weather(MythScreenStack *parent, const QString &name, SourceManager *srcMan);
    ~Weather();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    bool UpdateData();

  public slots:
    void setupScreens();

  private slots:
    void update_timeout() {}
    void nextpage_timeout();
    void weatherTimeout() {}
    void cursorLeft();
    void cursorRight();
    void holdPage();
    void setupPage();
    void screenReady(WeatherScreen *ws);

  private:
    WeatherScreen *nextScreen();
    WeatherScreen *prevScreen();
    void clearScreens();
    void showScreen(WeatherScreen *ws);
    void hideScreen(void);

    MythScreenStack *m_weatherStack;
    int m_timeoutCounter;
    int m_wantAnimated;
    bool m_stopProcessing;

    units_t m_units;
    bool m_firstRun;
    int m_updateInterval;
    int m_nextpageInterval;
    int m_nextpageIntArrow;
    QTimer *m_nextpage_Timer;

    bool m_gotDataHook;

    bool m_firstSetup;

    bool m_createdSrcMan;
    SourceManager *m_srcMan;
    ScreenList m_screens; //screens in correct display order
    int        m_cur_screen;

    ScreenListMap m_allScreens; //screens parsed from xml
    WeatherScreen *m_currScreen;
    bool m_paused;

    MythUIText *m_pauseText;
    MythUIText *m_headerText;
    MythUIText *m_updatedText;
};

#endif
