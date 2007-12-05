#ifndef _SOURCEMANAGER_H_
#define _SOURCEMANAGER_H_

#include <qobject.h>
#include <qintdict.h>
#include <qstringlist.h>

#include "defs.h"

#include "weatherSource.h"

class WeatherScreen;
struct ScriptInfo;

class SourceManager : public QObject
{
    Q_OBJECT

  public:
    SourceManager();
    WeatherSource *needSourceFor(int id, const QString &loc, units_t units);
    QStringList getLocationList(ScriptInfo *si, const QString &str);
    void startTimers();
    void stopTimers();
    void doUpdate();
    bool findPossibleSources(QStringList types, QPtrList<ScriptInfo> &sources);
    void clearSources();
    bool findScripts();
    bool findScriptsDB();
    void setupSources();
    bool connectScreen(uint id, WeatherScreen *screen);
    bool disconnectScreen(WeatherScreen *screen);
    ScriptInfo *getSourceByName(const QString &name);

  private slots:
    void timeout(void) {}

  private:
    QPtrList<ScriptInfo> m_scripts; //all scripts
    QPtrList<WeatherSource> m_sources; //in-use scripts
    QIntDict<WeatherSource> m_sourcemap;
    units_t m_units;
    void recurseDirs(QDir dir);
};

#endif
