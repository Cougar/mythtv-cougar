#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "gamesettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>


const QString GetGameExtensions(const QString GameType)
{
    QString result = "";

    for (int i = 0; i < MAX_GAME_TYPES; i++)
    {
        if (GameTypeList[i].idStr == GameType) {
            result = GameTypeList[i].extensions;
            break;
        }
    }
    return result;
}

// General Settings
static HostLineEdit *GameAllTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameAllTreeLevels");
    gc->setLabel(QObject::tr("Game display order"));
    gc->setValue("system gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
}

static HostLineEdit *GameFavTreeLevels()
{
    HostLineEdit *gc = new HostLineEdit("GameFavTreeLevels");
    gc->setLabel(QObject::tr("Favourite display order"));
    gc->setValue("gamename");
    gc->setHelpText(QObject::tr("Order in which to sort the games "
                    "marked as favourites "
                    "- this is for all systems. Available choices: "
                    "system, year, genre and gamename"));
    return gc;
}

static HostCheckBox *GameDeepScan()
{   
    HostCheckBox *gc = new HostCheckBox("GameDeepScan");
    gc->setLabel(QObject::tr("Indepth Game Scan"));
    gc->setHelpText(QObject::tr("Enabling this causes a game scan to gather crc values and attempt to find out more"
                    " detailed information about the game: NOTE this can greatly increase the time a gamescan takes"
                    " based on the amount of games scanned."));
    return gc;
}   

static HostCheckBox *GameRemovalPrompt()
{
    HostCheckBox *gc = new HostCheckBox("GameRemovalPrompt");
    gc->setLabel(QObject::tr("Prompt for removal of deleted rom(s)"));
    gc->setHelpText(QObject::tr("This enables a prompt for removing"
                                " deleted roms from the database during a "
                                " gamescan"));
    
    return gc;
}

static HostCheckBox *GameShowFileNames()
{  
    HostCheckBox *gc = new HostCheckBox("GameShowFileNames");
    gc->setLabel(QObject::tr("Display Files Names in Game Tree"));
    gc->setHelpText(QObject::tr("Enabling this causes the filenames to be displayed in the game tree rather than the trimmed/looked up gamename"));
    return gc;
}   

static HostCheckBox *GameTreeView()
{
    HostCheckBox *gc = new HostCheckBox("GameTreeView");
    gc->setLabel(QObject::tr("Hash filenames in display"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("Enable hashing of names in the display tree. This can make navigating long lists a little faster"));
    return gc;
}

MythGameGeneralSettings::MythGameGeneralSettings()
{
    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("MythGame Settings -- General"));
    general->addChild(GameAllTreeLevels());
    general->addChild(GameFavTreeLevels());
    general->addChild(GameDeepScan());
    general->addChild(GameRemovalPrompt());
    general->addChild(GameShowFileNames());
    general->addChild(GameTreeView());
    addChild(general);
}

// Player Settings
QString MGSetting::whereClause(MSqlBindings &bindings) {
    QString playerId(":PLAYERID");
    QString query("gameplayerid = " + playerId);

    bindings.insert(playerId, parent.getGamePlayerID());

    return query;
}

QString MGSetting::setClause(MSqlBindings &bindings) {
    QString playerID(":SETPLAYERID");
    QString colTag(":SET" + getColumn().upper());

    QString query("gameplayerid = " + playerID + ", " +
                  getColumn() + " = " + colTag);

    bindings.insert(playerID, parent.getGamePlayerID());
    bindings.insert(colTag, getValue());

    return query;
}

class AllowMultipleRoms: virtual public MGSetting, virtual public CheckBoxSetting {
public:
    AllowMultipleRoms(const MythGamePlayerSettings& parent):
        MGSetting(parent, "spandisks") {
        setLabel(QObject::tr("Allow games to span multiple roms/disks"));
        setHelpText(QObject::tr("This setting means that we will look for items like game.1.rom, game.2.rom and consider them a single game."));
    };
};

class Command: virtual public MGSetting, virtual public LineEditSetting {
public:
    Command(const MythGamePlayerSettings& parent):
        MGSetting(parent, "commandline") {
        setLabel(QObject::tr("Command"));
        setHelpText(QObject::tr("Binary and optional parameters. Multiple commands seperated with \';\' . Use \%s for the rom name. \%d1, \%d2, \%d3 and \%d4 represent disks in a multidisk/game. %s auto appended if not specified"));
    };
};

class GameType: public ComboBoxSetting, public MGSetting {
public:
    GameType(const MythGamePlayerSettings& parent):
        MGSetting(parent, "gametype") {
        setLabel(QObject::tr("Type"));
        for (int i = 0; i < MAX_GAME_TYPES; i++)
        {
            addSelection(GameTypeList[i].nameStr, GameTypeList[i].idStr);
        }
        setValue(0);
        setHelpText(QObject::tr("Type of Game/Emulator. Mostly for informational purposes and has little effect on the function of your system."));
    };
};

class RomPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    RomPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "rompath") {
        setLabel(QObject::tr("Rom Path"));
        setHelpText(QObject::tr("Location of the ROM files for this emulator"));
    };
};

class WorkingDirPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    WorkingDirPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "workingpath") {
        setLabel(QObject::tr("Working Directory"));
        setHelpText(QObject::tr("Directory to change to before launching emulator. Blank is usually fine"));
    };
};

class Extensions: virtual public MGSetting, virtual public LineEditSetting {
public:
    Extensions(const MythGamePlayerSettings& parent):
        MGSetting(parent, "extensions") {
        setLabel(QObject::tr("File Extensions"));
        setHelpText(QObject::tr("A Comma seperated list of all file extensions for this emulator. Blank means any file under ROM PATH is considered to be used with this emulator"));
    };
};


class ScreenPath: virtual public MGSetting, virtual public LineEditSetting {
public:
    ScreenPath(const MythGamePlayerSettings& parent):
        MGSetting(parent, "screenshots") {
        setLabel(QObject::tr("ScreenShots"));
        setHelpText(QObject::tr("Path to any screenshots for this player"));
    };
};

MythGamePlayerSettings::MythGamePlayerSettings()
{
    // must be first
    addChild(id = new ID());
    
    ConfigurationGroup *group = new VerticalConfigurationGroup(false, false);
    group->setLabel(QObject::tr("Game Player Setup"));
    group->addChild(name = new Name(*this));
    group->addChild(new GameType(*this));
    group->addChild(new Command(*this));
    group->addChild(new RomPath(*this));
    group->addChild(new ScreenPath(*this));
    group->addChild(new WorkingDirPath(*this));
    group->addChild(new Extensions(*this));
    group->addChild(new AllowMultipleRoms(*this));
    addChild(group);
};

void MythGamePlayerSettings::fillSelections(SelectSetting* setting)
{
    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT playername, gameplayerid, gametype FROM gameplayers WHERE playername <> '' ORDER BY playername;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next())
        {
            setting->addSelection(result.value(0).toString() + " (" + result.value(2).toString() + ")",
                                  result.value(1).toString());
        }
    }
}

void MythGamePlayerSettings::loadByID(int sourceid)
{
    id->setValue(sourceid);
    load();
}

int MythGamePlayerEditor::exec() {
    while (ConfigurationDialog::exec() == QDialog::Accepted)
        edit();

    return QDialog::Rejected;
}

void MythGamePlayerEditor::load() {
    clearSelections();
    addSelection(QObject::tr("(New Game Player)"), "0");
    MythGamePlayerSettings::fillSelections(this);
}

MythDialog* MythGamePlayerEditor::dialogWidget(MythMainWindow* parent,
                                            const char* widgetName)
{
    dialog = ConfigurationDialog::dialogWidget(parent, widgetName);
    connect(dialog, SIGNAL(menuButtonPressed()), this, SLOT(menu()));
    connect(dialog, SIGNAL(editButtonPressed()), this, SLOT(edit()));
    connect(dialog, SIGNAL(deleteButtonPressed()), this, SLOT(del()));
    return dialog;
}

void MythGamePlayerEditor::menu(void)
{
    if (getValue().toInt() == 0)
    {   
        MythGamePlayerSettings gp;
        gp.exec();
    }
    else
    {   
        int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(),
                                                 "",
                                                 tr("Game Player Menu"),
                                                 tr("Edit.."),                                                                   tr("Delete.."), 1);

        if (val == 0)
            edit();
        else if (val == 1)
            del();
    }
}

void MythGamePlayerEditor::edit(void)                                                  {   
    MythGamePlayerSettings gp;
    if (getValue().toInt() != 0)
        gp.loadByID(getValue().toInt());

   gp.exec();
}

void MythGamePlayerEditor::del(void)
{
    int val = MythPopupBox::show2ButtonPopup(gContext->GetMainWindow(), "",
                                          tr("Are you sure you want to delete "
                                             "this item?"),
                                             tr("Yes, delete It"),
                                             tr("No, don't"), 2);

    if (val == 0)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("DELETE FROM gameplayers WHERE gameplayerid= :SOURCEID ;");
        query.bindValue(":SOURCEID", getValue());

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Deleting MythGamePlayerSettings:", query);

        load(); 
    }
}

