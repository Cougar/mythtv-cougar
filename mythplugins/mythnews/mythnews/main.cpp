/* ============================================================
 * File  : main.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2003-08-30
 * Description :
 *
 * Copyright 2003 by Renchi Raju

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

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "mythnews.h"
#include "mythnewsconfig.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

using namespace std;

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythnews",
                                    libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    return 0;
}

int mythplugin_run(void)
{

//     QTranslator translator(0);
//     translator.load(PREFIX + QString("/share/mythtv/i18n/mythwnews_") +
//                     QString(gContext->GetSetting("Language").lower()) +
//                     QString(".qm"), ".");
//     qApp->installTranslator(&translator);

    MythNews news(QSqlDatabase::database(),
                  gContext->GetMainWindow(), "news");
    news.exec();

//     qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    MythNewsConfig config(QSqlDatabase::database(),
                          gContext->GetMainWindow(), "news");
    config.exec();
    return 0;
}



