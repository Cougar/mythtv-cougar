
// QT headers
#include <qdir.h>
#include <qfileinfo.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qapplication.h>
//Added by qt3to4:
#include <Q3PtrList>

// MythTV headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/libmythui/mythprogressdialog.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"
#include "sourceManager.h"

#define LOC QString("SourceManager: ")
#define LOC_ERR QString("SourceManager Error: ")

SourceManager::SourceManager()
{
    findScriptsDB();
    setupSources();
}

bool SourceManager::findScriptsDB()
{
    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT DISTINCT wss.sourceid, source_name, update_timeout, "
            "retrieve_timeout, path, author, version, email, types "
            "FROM weathersourcesettings wss "
            "LEFT JOIN weatherdatalayout wdl "
            "ON wss.sourceid = wdl.weathersourcesettings_sourceid "
            "WHERE hostname = :HOST;";

    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return false;
    }

    while (db.next())
    {
        QFileInfo *fi = new QFileInfo(db.value(4).toString());

        if (!fi->isExecutable())
        {
            // scripts will be deleted from db in the more robust (i.e. slower)
            // findScripts() -- run when entering setup
            delete fi;
            continue;
        }
        ScriptInfo *si = new ScriptInfo;
        si->id = db.value(0).toInt();
        si->name = db.value(1).toString();
        si->updateTimeout = db.value(2).toUInt() * 1000;
        si->scriptTimeout = db.value(3).toUInt() * 1000;
        si->file = fi;
        si->author = db.value(5).toString();
        si->version = db.value(6).toString();
        si->email = db.value(7).toString();
        si->types = QStringList::split(",", db.value(8).toString());
        m_scripts.append(si);
    }

    return true;
}

bool SourceManager::findScripts()
{
    QString path =  gContext->GetShareDir() + "mythweather/scripts/";
    QDir dir(path);
    dir.setFilter(QDir::Executable | QDir::Files | QDir::Dirs);

    if (!dir.exists())
    {
        VERBOSE(VB_IMPORTANT, "MythWeather: Scripts directory not found");
        return false;
    }
    QString busymessage = tr("Searching for scripts");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new MythUIBusyDialog(busymessage, popupStack,
                                                       "mythweatherbusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);

    qApp->processEvents();

    recurseDirs(dir);

    // run through and see if any scripts have been deleted
    MSqlQuery db(MSqlQuery::InitCon());

    db.prepare("SELECT sourceid, path FROM weathersourcesettings "
               "WHERE hostname = :HOST;");
    db.bindValue(":HOST", gContext->GetHostName());
    db.exec();
    QStringList toRemove;
    while (db.next())
    {
        QFileInfo fi(db.value(1).toString());
        if (!fi.isExecutable())
        {
            toRemove << db.value(0).toString();
            VERBOSE(VB_GENERAL,  fi.absFilePath() + " No longer exists");
        }
    }

    db.prepare("DELETE FROM weathersourcesettings WHERE sourceid = :ID;");
    for (int i = 0; i < toRemove.count(); ++i)
    {
        db.bindValue(":ID", toRemove[i]);
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT,  db.lastError().text());
        }
    }

    if (busyPopup)
    {
        busyPopup->Close();
        busyPopup = NULL;
    }

    return m_scripts.count() > 0;
}

void SourceManager::clearSources()
{
    while (!m_scripts.isEmpty())
        delete m_scripts.takeFirst();
    m_scripts.clear();

    while (!m_sources.isEmpty())
        delete m_sources.takeFirst();
    m_sources.clear();
}

void SourceManager::setupSources()
{
    MSqlQuery db(MSqlQuery::InitCon());

    QString query = "SELECT DISTINCT location,weathersourcesettings_sourceid,weatherscreens.units,"
        "weatherscreens.screen_id FROM weatherdatalayout,weatherscreens "
        "WHERE weatherscreens.screen_id = weatherscreens_screen_id AND weatherscreens.hostname = :HOST;";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    m_sourcemap.clear();

    while (db.next())
    {
        QString loc = db.value(0).toString();
        uint sourceid = db.value(1).toUInt();
        units_t units = db.value(2).toUInt();
        uint screen = db.value(3).toUInt();
        const WeatherSource *src = needSourceFor(sourceid, loc, units);
        m_sourcemap.insert((long)screen, src);
    }
}

ScriptInfo *SourceManager::getSourceByName(const QString &name)
{
    ScriptInfo *src = 0;
    for (int x = 0; x < m_scripts.size(); x++)
    {
        src = m_scripts.at(x);
        if (src->name == name)
        {
            return src;
          //  src = new ScriptInfo;
          //  src->name = m_scripts[i]->name;
          //  src->version = m_scripts[i]->version;
          //  src->author = m_scripts[i]->author;
          //  src->email = m_scripts[i]->email;
          //  src->types = m_scripts[i]->types;
          //  src->file = new QFileInfo(*m_scripts[i]->file);
          //  src->scriptTimeout = m_scripts[i]->scriptTimeout;
          //  src->updateTimeout = m_scripts[i]->updateTimeout;
          //  src->id = m_scripts[i]->id;
        }
    }

    if (!src)
    {
        VERBOSE(VB_IMPORTANT, "No Source found for " + name);
    }

    return NULL;
}

QStringList SourceManager::getLocationList(ScriptInfo *si, const QString &str)
{
    if (!m_scripts.contains(si))
        return QStringList();
    WeatherSource *ws = new WeatherSource(si);
    return ws->getLocationList(str);
}

WeatherSource *SourceManager::needSourceFor(int id, const QString &loc,
                                            units_t units)
{
    // matching source exists?
    WeatherSource *src;
    for (int x = 0; x < m_sources.size(); x++)
    {
        src = m_sources.at(x);
        if (src->getId() == id && src->getLocale() == loc &&
            src->getUnits() == units)
        {
            return src;
        }
    }

    // no matching source, make one
    ScriptInfo *si;
    for (int x = 0; x < m_scripts.size(); x++)
    {
        si = m_scripts.at(x);
        if (si->id == id)
        {
            WeatherSource *ws = new WeatherSource(si);
            ws->setLocale(loc);
            ws->setUnits(units);
            m_sources.append(ws);
            return ws;
        }
    }

    VERBOSE(VB_IMPORTANT, LOC + QString("NeedSourceFor: Unable to find source "
            "for %1, %2, %3").arg(id).arg(loc).arg(units));
    return NULL;
}

void SourceManager::startTimers()
{
    WeatherSource *src;
    for (int x = 0; x < m_sources.size(); x++)
    {
        src = m_sources.at(x);
        src->startUpdateTimer();
    }
}

void SourceManager::stopTimers()
{
    WeatherSource *src;
    for (int x = 0; x < m_sources.size(); x++)
    {
        src = m_sources.at(x);
        src->stopUpdateTimer();
    }
}

void SourceManager::doUpdate()
{
    WeatherSource *src;
    for (int x = 0; x < m_sources.size(); x++)
    {
        src = m_sources.at(x);
        if (src->isRunning())
        {
            VERBOSE(VB_GENERAL, tr("Script %1 is still running when trying to do update, "
                    "Make sure it isn't hanging, make sure timeout values are sane... "
                    "Not running this time around").arg(src->getName()));
        }
        else if (src->inUse())
            src->startUpdate();
    }
}

bool SourceManager::findPossibleSources(QStringList types,
                                        QList<ScriptInfo *> &sources)
{
    ScriptInfo *si;
    bool handled;
    for (int x = 0; x < m_scripts.size(); x++)
    {
        si = m_scripts.at(x);
        QStringList stypes = si->types;
        handled = true;
        int i;
        for (i = 0; i < types.count() && handled; ++i)
        {
            handled = stypes.contains(types[i]);
        }
        if (handled)
            sources.append(si);
    }

    if (sources.count())
        return true;

    return false;
}

bool SourceManager::connectScreen(uint id, WeatherScreen *screen)
{
    if (!screen)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Can not connect nonexistent screen "<<screen);

        return false;
    }

    WeatherSource *ws = m_sourcemap[id];
    if (!ws)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Can not connect nonexistent source "<<id);

        return false;
    }
    ws->connectScreen(screen);
    return true;
}

bool SourceManager::disconnectScreen(WeatherScreen *screen)
{
    if (!screen)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Can not disconnect nonexistent screen "<<screen);

        return false;
    }

    WeatherSource *ws = m_sourcemap[screen->getId()];
    if (!ws)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Can not disconnect nonexistent source "<<screen->getId());

        return false;
    }
    ws->disconnectScreen(screen);
    return true;
}

// Recurses dir for script files
void SourceManager::recurseDirs( QDir dir )
{
    if (!dir.exists())
        return;

    dir.setFilter(QDir::Executable | QDir::Files | QDir::Dirs);
    QFileInfoList files = dir.entryInfoList();
    QFileInfo file;

    for (int x = 0; x < files.size(); x++)
    {
        qApp->processEvents();
        file = files.at(x);
        if (file.isDir())
        {
            if (file.fileName() == QString("..")) continue;
            if (file.fileName() == QString("."))  continue;
            QDir recurseTo(file.filePath());
            recurseDirs(recurseTo);
        }

        if (file.isExecutable() && !(file.isDir()))
        {
            ScriptInfo *info = WeatherSource::probeScript(file);
            if (info)
            {
                m_scripts.append(info);
                VERBOSE(VB_GENERAL, "found script " + file.absFilePath());
            }
        }
    }

    return;
}
