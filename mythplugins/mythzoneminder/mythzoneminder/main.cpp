/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include <iostream>
#include <unistd.h>

// qt
#include <qapplication.h>
#include <qsqldatabase.h>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/mythpluginapi.h>

//zone minder
#include "zmconsole.h"
#include "zmplayer.h"
#include "zmevents.h"
#include "zmliveplayer.h"
//#include "zmutils.h"
#include "zmsettings.h"
#include "zmclient.h"

using namespace std;

void runZMConsole(void);
void runZMLiveView(void);
void runZMEventView(void);

void setupKeys(void)
{
    REG_JUMP("ZoneMinder Console",    "", "", runZMConsole);
    REG_JUMP("ZoneMinder Live View",  "", "", runZMLiveView);
    REG_JUMP("ZoneMinder Events",     "", "", runZMEventView);
}

bool checkConnection(void)
{
    if (!ZMClient::get()->connected())
    {
        if (!ZMClient::setupZMClient())
            return false;
    }

    return true;
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythzoneminder",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    setupKeys();

    return 0;
}

void runZMConsole(void)
{
    if (!checkConnection())
        return;

    gContext->addCurrentLocation("zoneminderconsole");
    ZMConsole console(gContext->GetMainWindow(), "zmconsole",
                      "zoneminder-", "zmconsole");
    console.exec();
    gContext->removeCurrentLocation();
}

void runZMLiveView(void)
{
    if (!checkConnection())
        return;

    gContext->addCurrentLocation("zoneminderliveview");

    ZMLivePlayer player(1, 1, gContext->GetMainWindow(), "zmliveplayer",
                        "zoneminder-", "zmplayer");
    player.exec();

    gContext->removeCurrentLocation();
}

void runZMEventView(void)
{
    if (!checkConnection())
        return;

    gContext->addCurrentLocation("zoneminderevents");

    ZMEvents events(gContext->GetMainWindow(), "zmevents", "zoneminder-", "zmevents");
    events.exec();

   gContext->removeCurrentLocation();
}

void ZoneMinderCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.lower();

    if (sel == "zm_console")
        runZMConsole();
    else if (sel == "zm_live_viewer")
        runZMLiveView();
    else if (sel == "zm_event_viewer")
        runZMEventView();
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu, 
                                              GetMythMainWindow()->GetMainStack(),
                                              "zoneminder menu");

    diag->setCallback(ZoneMinderCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }
}

int mythplugin_run(void)
{
    // setup a connection to the mythzmserver
    if (!ZMClient::setupZMClient())
    {
        return -1;
    }

    runMenu("zonemindermenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    ZMSettings settings;
    settings.exec();

    return 0;
}



