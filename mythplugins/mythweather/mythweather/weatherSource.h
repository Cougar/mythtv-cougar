#ifndef __WEATHER_SOURCE_H__
#define __WEATHER_SOURCE_H__

#include <QStringList>
#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>

// MythWeather headers
#include "weatherUtils.h"

class WeatherScreen;

/*
 * Instance indpendent information about a script
 */
class ScriptInfo
{
  public:
    QString name;
    QString version;
    QString author;
    QString email;
    QStringList types;
    QFileInfo fileInfo;
    unsigned int scriptTimeout;
    unsigned int updateTimeout;
    int id;
};

class WeatherSource : public QObject
{
    Q_OBJECT

  public:
    static ScriptInfo *ProbeScript(const QFileInfo &fi);
    static QStringList ProbeTypes(QString    workingDirectory,
                                  QString    program);
    static bool ProbeTimeouts(QString        workingDirectory,
                              QString        program,
                              uint          &updateTimeout,
                              uint          &scriptTimeout);
    static bool ProbeInfo(QString            workingDirectory,
                          QString            program,
                          struct ScriptInfo &scriptInfo);

    WeatherSource(ScriptInfo *info);
    WeatherSource(const QString &filename);
    ~WeatherSource();

    bool isReady() { return m_ready; }
    QString getVersion() { return m_info->version; }
    QString getName() { return m_info->name; }
    QString getAuthor() { return m_info->author; }
    QString getEmail() { return m_info->email; }
    units_t getUnits() { return m_units; }
    void setUnits(units_t units) { m_units = units; }
    QStringList getLocationList(const QString &str);
    void setLocale(const QString &locale) { m_locale = locale; }
    QString getLocale() { return m_locale; }

    void startUpdate();
    bool isRunning(void) const;

    int getScriptTimeout() { return m_info->scriptTimeout; }
    void setScriptTimeout(int timeout) { m_info->scriptTimeout = timeout; }

    int getUpdateTimeout() { return m_info->updateTimeout; }
    void setUpdateTimeout(int timeout) { m_info->updateTimeout = timeout; }

    void startUpdateTimer() { m_updateTimer->start(m_info->updateTimeout); }
    void stopUpdateTimer() { m_updateTimer->stop(); }

    bool inUse() { return m_inuse; }
    void setInUse(bool inuse) { m_inuse = inuse; }

    int getId() { return m_info->id; }

    void connectScreen(WeatherScreen *ws);
    void disconnectScreen(WeatherScreen *ws);

  signals:
    void newData(QString, units_t,  DataMap);
    void killProcess();

  private slots:
    void read(void);
    void processExit();
    void scriptTimeout();
    void updateTimeout();

  private:
    void processData();

    bool m_ready;
    bool m_inuse;
    ScriptInfo *m_info;
    QProcess *m_proc;
    QString m_proc_debug;
    QString m_dir;
    QString m_locale;
    QByteArray m_buffer;
    units_t m_units;
    QTimer *m_scriptTimer;
    QTimer *m_updateTimer;
    int m_connectCnt;
    DataMap m_data;
};

#endif
