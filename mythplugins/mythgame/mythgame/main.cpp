#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "gamehandler.h"
#include "databasebox.h"
#include "rominfo.h"

#include <mythtv/themedmenu.h>
#include <mythtv/settings.h>

Settings *globalsettings;
char theprefix[] = "/usr/local";

void startDatabaseTree(QSqlDatabase *db, QString &paths,
                       QValueList<RomInfo> *romlist)
{
    DatabaseBox dbbox(db, paths, romlist);
    dbbox.Show();

    dbbox.exec();
}

struct GameCBData
{
    QString paths;
    QSqlDatabase *db;
    QValueList<RomInfo> *romlist;
};

void GameCallback(void *data, QString &selection)
{
    GameCBData *gdata = (GameCBData *)data;

    QString sel = selection.lower();

    if (sel == "game_play")
        startDatabaseTree(gdata->db, gdata->paths, gdata->romlist);
}

bool runMenu(QString themedir, QSqlDatabase *db,
             QString paths, QValueList<RomInfo> &romlist)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), theprefix, 
                                      "gamemenu.xml");

    GameCBData data;
    data.paths = paths;
    data.db = db;
    data.romlist = &romlist;

    diag->setCallback(GameCallback, &data);
    diag->setKillable();
        
    if (diag->foundTheme())
    {
        diag->Show();
        diag->exec();
    }     
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }       
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    globalsettings = new Settings();

    globalsettings->LoadSettingsFiles("mythgame-settings.txt", theprefix);
    globalsettings->LoadSettingsFiles("theme.txt", theprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", theprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Could not connect to database\n");
        return -1;
    }
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));
    if (!db->open())
    {
        printf("could not open db\n");
        return -1;
    }

    //for each game handler process it's games
    //TODO: move this to a settings option
    GameHandler::processAllGames();
    QString paths = globalsettings->GetSetting("TreeLevels");
    QValueList<RomInfo> Romlist;

    QString themename = globalsettings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    runMenu(themedir, db, paths, Romlist);

    db->close();

    delete globalsettings;

    return 0;
}
