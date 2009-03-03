
// C headers
#include <unistd.h>
#include <cstdlib>

// QT headers
#include <QApplication>

// MythTV headers
#include <mythdbcon.h>
#include <mythcontext.h>

// MythWeather headers
#include "weatherScreen.h"
#include "sourceManager.h"
#include "weatherSetup.h"
#include "weather.h"

Weather::Weather(MythScreenStack *parent, const QString &name, SourceManager *srcMan)
    : MythScreenType(parent, name),
      m_cur_screen(-1)
{
    m_weatherStack = new MythScreenStack(GetMythMainWindow(), "weather stack");

    m_paused = false;

    m_firstRun = true;
    m_firstSetup = true;

    if (!srcMan)
    {
        m_srcMan = new SourceManager();
        m_srcMan->startTimers();
        m_srcMan->doUpdate();
        m_createdSrcMan = true;
    }
    else
    {
        m_srcMan = srcMan;
        m_createdSrcMan = false;
    }

    m_pauseText = m_headerText = m_updatedText = NULL;

    m_nextpageInterval = gContext->GetNumSetting("weatherTimeout", 10);

    m_nextpage_Timer = new QTimer(this);
    connect(m_nextpage_Timer, SIGNAL(timeout()), SLOT(nextpage_timeout()) );
    m_allScreens = loadScreens();
}

Weather::~Weather()
{
    if (m_createdSrcMan)
        delete m_srcMan;

    clearScreens();

    if (m_weatherStack)
        GetMythMainWindow()->PopScreenStack();
}

bool Weather::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("weather-ui.xml", "weatherbase", this);

    if (!foundtheme)
    {
        VERBOSE(VB_IMPORTANT, "Missing required window - weatherbase.");
        return false;
    }

    m_pauseText = dynamic_cast<MythUIText *> (GetChild("pause_text"));
    m_headerText = dynamic_cast<MythUIText *> (GetChild("header"));
    m_updatedText = dynamic_cast<MythUIText *> (GetChild("update_text"));

    if (!m_pauseText || !m_headerText || !m_updatedText)
    {
        VERBOSE(VB_IMPORTANT, "Window weatherbase is missing required elements.");
        return false;
    }

    if (m_pauseText)
    {
        m_pauseText->SetText(tr("Paused"));
        m_pauseText->Hide();
    }

    return true;
}

void Weather::clearScreens()
{
    m_currScreen = NULL;

    while (m_weatherStack->TotalScreens() > 0)
    {
        m_weatherStack->PopScreen(false,false);
    }

    m_cur_screen = -1;
    while (!m_screens.empty())
    {
        WeatherScreen *screen = m_screens.back();
        m_screens.pop_back();
        if (screen)
            delete screen;
    }
}

void Weather::setupScreens()
{
    // Delete any existing screens
    clearScreens();

    m_paused = false;
    m_pauseText->Hide();

    // Refresh sources
    m_srcMan->clearSources();
    m_srcMan->findScriptsDB();
    m_srcMan->setupSources();

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT screen_id, container, units, draworder FROM weatherscreens "
            " WHERE hostname = :HOST ORDER BY draworder;";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    if (!db.size())
    {
        if (m_firstSetup)
        {
            // If no screens exist, run the setup
            MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

            ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup",
                                                  m_srcMan);

            connect(ssetup, SIGNAL(Exiting()), this, SLOT(setupScreens()));

            if (ssetup->Create())
            {
                mainStack->AddScreen(ssetup);
            }

            m_firstSetup = false;
        }
        else
        {
            Close();
        }
    }
    else
    {
        while (db.next())
        {
            int id = db.value(0).toInt();
            QString container = db.value(1).toString();
            units_t units = db.value(2).toUInt();
            uint draworder = db.value(3).toUInt();

            ScreenListInfo *screenListInfo = m_allScreens[container];

            WeatherScreen *ws = WeatherScreen::loadScreen(m_weatherStack, screenListInfo, id);
            if (!ws->Create())
                continue;

            ws->setUnits(units);
            ws->setInUse(true);
            m_screens.insert(draworder, ws);
            connect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
                    SLOT(screenReady(WeatherScreen *)));
            m_srcMan->connectScreen(id, ws);
        }

        m_srcMan->startTimers();
        m_srcMan->doUpdate();
    }
}

void Weather::screenReady(WeatherScreen *ws)
{
    WeatherScreen *nxt = nextScreen();

    if (m_firstRun && ws == nxt)
    {
        m_firstRun = false;
        showScreen(nxt);
        m_nextpage_Timer->start((int)(1000 * m_nextpageInterval));
    }
    disconnect(ws, SIGNAL(screenReady(WeatherScreen *)), this,
               SLOT(screenReady(WeatherScreen *)));
}

WeatherScreen *Weather::nextScreen(void)
{
    if (m_screens.empty())
        return NULL;

    m_cur_screen = (m_cur_screen + 1) % m_screens.size();
    return m_screens[m_cur_screen];
}

WeatherScreen *Weather::prevScreen(void)
{
    if (m_screens.empty())
        return NULL;

    m_cur_screen = (m_cur_screen < 0) ? 0 : m_cur_screen;
    m_cur_screen = (m_cur_screen + m_screens.size() - 1) % m_screens.size();
    return m_screens[m_cur_screen];
}

bool Weather::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Weather", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "PAUSE")
            holdPage();
        else if (action == "MENU")
            setupPage();
        else if (action == "UPDATE")
        {
            m_srcMan->doUpdate();
        }
        else if (action == "ESCAPE")
        {
            m_nextpage_Timer->stop();
            hideScreen();
            Close();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void Weather::showScreen(WeatherScreen *ws)
{
    if (!ws)
        return;

    m_currScreen = ws;
    m_weatherStack->AddScreen(m_currScreen, false);
    m_headerText->SetText(m_currScreen->objectName());
    m_updatedText->SetText(m_currScreen->getValue("updatetime"));
}

void Weather::hideScreen()
{
    if (!m_currScreen)
        return;

    m_weatherStack->PopScreen(false,false);
}

void Weather::holdPage()
{
    if (!m_nextpage_Timer->isActive())
        m_nextpage_Timer->start(1000 * m_nextpageInterval);
    else
        m_nextpage_Timer->stop();

    m_paused = !m_paused;

    if (m_pauseText)
    {
        if (m_paused)
            m_pauseText->Show();
        else
            m_pauseText->Hide();
    }
}

void Weather::setupPage()
{
    m_srcMan->stopTimers();
    m_nextpage_Timer->stop();
    m_srcMan->clearSources();
    m_srcMan->findScripts();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ScreenSetup *ssetup = new ScreenSetup(mainStack, "weatherscreensetup",
                                            m_srcMan);

    connect(ssetup, SIGNAL(Exiting()), this, SLOT(setupScreens()));

    if (ssetup->Create())
    {
        clearScreens();
        mainStack->AddScreen(ssetup);
    }

    m_firstRun = true;
}

void Weather::cursorRight()
{
    WeatherScreen *ws = nextScreen();
    if (ws && ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        if (!m_paused)
            m_nextpage_Timer->start((int)(1000 * m_nextpageInterval));
    }
}

void Weather::cursorLeft()
{
    WeatherScreen *ws = prevScreen();
    if (ws && ws->canShowScreen())
    {
        hideScreen();
        showScreen(ws);
        if (!m_paused)
            m_nextpage_Timer->start((int)(1000 * m_nextpageInterval));
    }
}

void Weather::nextpage_timeout()
{
    WeatherScreen *nxt = nextScreen();

    if (nxt->canShowScreen())
    {
        hideScreen();
        showScreen(nxt);
    }
    else
        VERBOSE(VB_GENERAL, "Next screen not ready");

    m_nextpage_Timer->start((int)(1000 * m_nextpageInterval));
}
