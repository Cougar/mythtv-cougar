#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "gamehandler.h"
#include "rominfo.h"
#include "gamesettings.h"
#include "gametree.h"
#include "dbcheck.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>

struct GameData
{
    QSqlDatabase *db;
};

void GameCallback(void *data, QString &selection)
{
    GameData *ddata = (GameData *)data;
    QString sel = selection.lower();

    (void)ddata;

    if (sel == "game_settings")
    {
        MythGameSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "search_for_games")
    {
        GameHandler::processAllGames();
    }
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();
    QSqlDatabase *db = QSqlDatabase::database();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu,
                                      gContext->GetMainWindow(), "game menu");

    GameData data;
    data.db = db;

    diag->setCallback(GameCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runGames(void);

void setupKeys(void)
{
    REG_JUMP("MythGame", "Game frontend", "", runGames);

    REG_KEY("Game", "TOGGLEFAV", "Toggle the current game as a favorite", 
            "?,/");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgame", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;


    UpgradeGameDatabaseSchema();

    MythGameSettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());

    setupKeys();

    return 0;
}

void runGames(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgame_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QSqlDatabase *db = QSqlDatabase::database();

    //look for new systems that haven't been added to the database
    //yet and tell them to scan their games

    //build a list of all the systems in the database
    QStringList systems;
    QString thequery = "SELECT DISTINCT system FROM gamemetadata;";
    QSqlQuery query = db->exec(thequery);
    while (query.next())
    {
        QString name = query.value(0).toString();
        systems.append(name);
    }

    //run through the list of registered systems, and if they're not
    //in the database, tell them to scan for games
    for (uint i = 0; i < GameHandler::count(); ++i)
    {
        GameHandler* handler = GameHandler::getHandler(i);
        bool found = systems.find(handler->Systemname()) != systems.end();
        if (!found)
        {
            handler->processGames();
        }
    }

    QString paths = gContext->GetSetting("GameTreeLevels");

    GameTree gametree(gContext->GetMainWindow(), db, "gametree", "game-",
                      paths);
    gametree.exec();

    qApp->removeTranslator(&translator);
}

int mythplugin_run(void)
{
    runGames();
    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgame_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    runMenu("game_settings.xml");

    qApp->removeTranslator(&translator);
    return 0;
}

