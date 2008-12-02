#include <unistd.h>

// QT headers
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QApplication>

// MythTV headers
#include <mythcontext.h>
#include <mythdbcon.h>
#include <compat.h>
#include <mythdirs.h>

// MythWeather headers
#include "weatherScreen.h"
#include "weatherSource.h"

QStringList WeatherSource::ProbeTypes(QString workingDirectory,
                                      QString program)
{
    QStringList arguments("-t");
    const QString loc_err =
        QString("WeatherSource::ProbeTypes(%1 %2), Error: ")
        .arg(program).arg(arguments.join(" "));

    QProcess proc;
    proc.setWorkingDirectory(workingDirectory);
    proc.start(program, arguments);

    QStringList types;
    if (!proc.waitForStarted())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Cannot run script");
        return types;
    }

    proc.waitForFinished();

    if (QProcess::NormalExit != proc.exitStatus())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Script Crashed");
        return types;
    }

    if (proc.exitCode())
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Script Returned error %1")
                .arg(proc.exitCode()));
        VERBOSE(VB_IMPORTANT, proc.readAllStandardError());
        return types;
    }

    proc.setReadChannel(QProcess::StandardOutput);
    while (proc.canReadLine())
    {
        QByteArray tmp = proc.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            types += QString(tmp);
    }

    if (types.empty())
        VERBOSE(VB_IMPORTANT, loc_err + "Invalid output from -t option");

    return types;
}

bool WeatherSource::ProbeTimeouts(QString  workingDirectory,
                                  QString  program,
                                  uint    &updateTimeout,
                                  uint    &scriptTimeout)
{
    QStringList arguments("-T");
    const QString loc_err =
        QString("WeatherSource::ProbeTimeouts(%1 %2), Error: ")
        .arg(program).arg(arguments.join(" "));

    updateTimeout = DEFAULT_UPDATE_TIMEOUT;
    scriptTimeout = DEFAULT_SCRIPT_TIMEOUT;

    QProcess proc;
    proc.setWorkingDirectory(workingDirectory);
    proc.start(program, arguments);

    if (!proc.waitForStarted())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Cannot run script");
        return false;
    }

    proc.waitForFinished();

    if (QProcess::NormalExit != proc.exitStatus())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Script Crashed");
        return false;
    }

    if (proc.exitCode())
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Script Returned error %1")
                .arg(proc.exitCode()));
        VERBOSE(VB_IMPORTANT, proc.readAllStandardError());
        return false;
    }

    proc.setReadChannel(QProcess::StandardOutput);
    QStringList lines;
    while (proc.canReadLine())
    {
        QByteArray tmp = proc.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            lines += QString(tmp);
    }

    if (lines.empty())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Invalid Script Output! No Lines");
        return false;
    }

    QStringList temp = lines[0].split(',');
    if (temp.size() != 2)
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    bool isOK[2];
    uint ut = temp[0].toUInt(&isOK[0]);
    uint st = temp[1].toUInt(&isOK[1]);
    if (!isOK[0] || !isOK[1])
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    updateTimeout = ut * 1000;
    scriptTimeout = st * 1000;

    return true;
}

bool WeatherSource::ProbeInfo(QString     workingDirectory,
                              QString     program,
                              ScriptInfo &info)
{
    QStringList arguments("-v");

    const QString loc_err =
        QString("WeatherSource::ProbeInfo(%1 %2), Error: ")
        .arg(program).arg(arguments.join(" "));

    QProcess proc;
    proc.setWorkingDirectory(workingDirectory);
    proc.setReadChannel(QProcess::StandardOutput);
    proc.start(program, arguments);

    if (!proc.waitForStarted())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Cannot run script");
        return false;
    }

    proc.waitForFinished();

    if (QProcess::NormalExit != proc.exitStatus())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Script Crashed");
        return false;
    }

    if (proc.exitCode())
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Script Returned error %1")
                .arg(proc.exitCode()));
        VERBOSE(VB_IMPORTANT, proc.readAllStandardError());
        return false;
    }

    proc.setReadChannel(QProcess::StandardOutput);
    QStringList lines;
    while (proc.canReadLine())
    {
        QByteArray tmp = proc.readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            lines += QString(tmp);
    }

    if (lines.empty())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Invalid Script Output! No Lines");
        return false;
    }

    QStringList temp = lines[0].split(',');
    if (temp.size() != 4)
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Invalid Script Output! '%1'").arg(lines[0]));
        return false;
    }

    info.name    = temp[0];
    info.version = temp[1];
    info.author  = temp[2];
    info.email   = temp[3];

    return true;
}

/* Basic logic of this behemouth...
 * run script with -v flag, this returns among other things, the version number
 * Search the database using the name (also returned from -v).
 * if it exists, compare versions from -v and db
 * if the same, populate the info struct from db, and we're done
 * if they differ, get the rest of the needed information from the script and
 * update the database, note, it does not overwrite the existing timeout values.
 * if the script is not in the database, we probe it for types and default
 * timeout values, and add it to the database
 */
ScriptInfo *WeatherSource::ProbeScript(const QFileInfo &fi)
{
    QStringList temp;

    if (!fi.isReadable() || !fi.isExecutable())
        return NULL;

    QString workingDirectory = fi.absolutePath();
    QString program          = fi.absoluteFilePath();

    ScriptInfo info;
    info.fileInfo = fi;
    if (!WeatherSource::ProbeInfo(workingDirectory, program, info))
        return NULL;

    MSqlQuery db(MSqlQuery::InitCon());
    QString query =
            "SELECT sourceid, source_name, update_timeout, retrieve_timeout, "
            "path, author, version, email, types FROM weathersourcesettings "
            "WHERE hostname = :HOST AND source_name = :NAME;";
    db.prepare(query);
    db.bindValue(":HOST", gContext->GetHostName());
    db.bindValue(":NAME", info.name);

    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, "Invalid response from database");
        return NULL;
    }

    // the script exists in the db
    if (db.next())
    {
        info.id            = db.value(0).toInt();
        info.updateTimeout = db.value(2).toUInt() * 1000;
        info.scriptTimeout = db.value(3).toUInt() * 1000;

        // compare versions, if equal... be happy
        QString dbver = db.value(6).toString();
        if (dbver == info.version)
        {
            info.types = db.value(8).toString().split(",");
        }
        else
        {
            // versions differ, change db to match script output
            VERBOSE(VB_GENERAL, "New version of " + info.name + " found");
            query = "UPDATE weathersourcesettings SET source_name = :NAME, "
                "path = :PATH, author = :AUTHOR, version = :VERSION, "
                "email = :EMAIL, types = :TYPES WHERE sourceid = :ID";
            db.prepare(query);
            // these info values were populated when getting the version number
            // we leave the timeout values in
            db.bindValue(":NAME", info.name);
            db.bindValue(":PATH", info.fileInfo.absoluteFilePath());
            db.bindValue(":AUTHOR", info.author);
            db.bindValue(":VERSION", info.version);

            // run the script to get supported data types
            info.types = WeatherSource::ProbeTypes(
                workingDirectory, program);

            db.bindValue(":TYPES", info.types.join(","));
            db.bindValue(":ID", info.id);
            db.bindValue(":EMAIL", info.email);
            if (!db.exec())
            {
                VERBOSE(VB_IMPORTANT, db.lastError().text());
                return NULL;
            }
        }
    }
    else
    {
        // Script is not in db, probe it and insert it into db
        query = "INSERT INTO weathersourcesettings "
                "(hostname, source_name, update_timeout, retrieve_timeout, "
                "path, author, version, email, types) "
                "VALUES (:HOST, :NAME, :UPDATETO, :RETTO, :PATH, :AUTHOR, "
                ":VERSION, :EMAIL, :TYPES);";

        if (!WeatherSource::ProbeTimeouts(workingDirectory,
                                          program,
                                          info.updateTimeout,
                                          info.scriptTimeout))
        {
            return NULL;
        }
        db.prepare(query);
        db.bindValue(":NAME", info.name);
        db.bindValue(":HOST", gContext->GetHostName());
        db.bindValue(":UPDATETO", QString::number(info.updateTimeout / 1000));
        db.bindValue(":RETTO", QString::number(info.scriptTimeout / 1000));
        db.bindValue(":PATH", info.fileInfo.absoluteFilePath());
        db.bindValue(":AUTHOR", info.author);
        db.bindValue(":VERSION", info.version);
        db.bindValue(":EMAIL", info.email);
        info.types = ProbeTypes(workingDirectory, program);
        db.bindValue(":TYPES", info.types.join(","));
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            return NULL;
        }
        query = "SELECT sourceid FROM weathersourcesettings "
                "WHERE source_name = :NAME AND hostname = :HOST;";
        // a little annoying, but look at what we just inserted to get the id
        // number, not sure if we really need it, but better safe than sorry.
        db.prepare(query);
        db.bindValue(":HOST", gContext->GetHostName());
        db.bindValue(":NAME", info.name);
        if (!db.exec())
        {
            VERBOSE(VB_IMPORTANT, db.lastError().text());
            return NULL;
        }
        db.next();
        info.id = db.value(0).toInt();
    }

    return new ScriptInfo(info);
}

/*
 * Watch out, we store the parameter as a member variable, don't go deleting it,
 * that wouldn't be good.
 */
WeatherSource::WeatherSource(ScriptInfo *info)
    : m_ready(info ? true : false),    m_inuse(info ? true : false),
      m_info(info),
      m_proc(NULL),
      m_locale(""),
      m_units(SI_UNITS),               m_scriptTimer(new QTimer(this)),
      m_updateTimer(new QTimer(this)), m_connectCnt(0)
{
    if (info)
        m_proc = new QProcess();

    QDir dir(GetConfDir());
    if (!dir.exists("MythWeather"))
        dir.mkdir("MythWeather");
    dir.cd("MythWeather");
    if (!dir.exists(info->name))
        dir.mkdir(info->name);
    dir.cd(info->name);
    m_dir = dir.absolutePath();

    connect( m_scriptTimer, SIGNAL(timeout()),
            this, SLOT(scriptTimeout()));

    connect( m_updateTimer, SIGNAL(timeout()),
            this, SLOT(updateTimeout()));

    if (m_proc)
    {
        m_proc->setWorkingDirectory(
             QDir(GetShareDir() + "mythweather/scripts/").absolutePath());
        connect(this, SIGNAL(killProcess()), m_proc, SLOT(kill()));
    }
}

WeatherSource::WeatherSource(const QString &filename)
    : m_ready(false),                  m_inuse(false),
      m_info(NULL),                    m_proc(NULL),
      m_dir(""),                       m_locale(""),
      m_units(SI_UNITS),
      m_scriptTimer(new QTimer(this)), m_updateTimer(new QTimer(this)),
      m_connectCnt(0)
{
    connect( m_scriptTimer, SIGNAL(timeout()),
            this, SLOT(scriptTimeout()));

    connect( m_updateTimer, SIGNAL(timeout()),
            this, SLOT(updateTimeout()));


    const QFileInfo fi(filename);
    m_info = ProbeScript(fi);

    if (m_info)
    {
        m_proc = new QProcess();
        // program = filename;
        m_proc->setWorkingDirectory(
            QDir(GetShareDir() + "mythweather/scripts/").absolutePath());
        connect(this, SIGNAL(killProcess()),
                m_proc, SLOT(kill()));
        m_ready = true;
    }
    else
        VERBOSE(VB_IMPORTANT, "Error probing script");
}

WeatherSource::~WeatherSource()
{
    delete m_proc;
    delete m_scriptTimer;
    delete m_updateTimer;
}

void WeatherSource::connectScreen(WeatherScreen *ws)
{
    connect(this, SIGNAL(newData(QString, units_t, DataMap)),
            ws, SLOT(newData(QString, units_t, DataMap)));
    ++m_connectCnt;

    if (m_data.size() > 0)
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::disconnectScreen(WeatherScreen *ws)
{
    disconnect(this, 0, ws, 0);
    --m_connectCnt;
}

QStringList WeatherSource::getLocationList(const QString &str)
{
    QString program = m_info->fileInfo.absoluteFilePath();
    QStringList args;
    args.push_back("-l");
    args.push_back(str);

    const QString loc_err =
        QString("WeatherSource::getLocationList(%1 %2), Error: ")
        .arg(program).arg(args.join(" "));

    if (isRunning())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Script already running");
        return QStringList();
    }

    m_proc->setWorkingDirectory(m_info->fileInfo.absolutePath());
    m_proc_debug = QString("%1 %2").arg(program).arg(args.join(" "));
    m_proc->start(program, args);

    if (!m_proc->waitForStarted())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Cannot run script");
        return QStringList();
    }

    m_proc->waitForFinished();

    if (QProcess::NormalExit != m_proc->exitStatus())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Script Crashed");
        return QStringList();
    }

    if (m_proc->exitCode())
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                QString("Script Returned error %1")
                .arg(m_proc->exitCode()));
        VERBOSE(VB_IMPORTANT, m_proc->readAllStandardError());
        return QStringList();
    }

    QStringList locs;
    m_proc->setReadChannel(QProcess::StandardOutput);
    while (m_proc->canReadLine())
    {
        QByteArray tmp = m_proc->readLine();

        while (tmp.endsWith('\n') || tmp.endsWith('\r'))
            tmp.chop(1);

        if (!tmp.isEmpty())
            locs.push_back(QString(tmp));
    }

    return locs;
}

void WeatherSource::startUpdate()
{
    m_buffer.clear();

    MSqlQuery db(MSqlQuery::InitCon());
    VERBOSE(VB_GENERAL, "Starting update of " + m_info->name);
    db.prepare("SELECT updated FROM weathersourcesettings "
               "WHERE sourceid = :ID AND "
               "TIMESTAMPADD(SECOND,update_timeout,updated) > NOW()");
    db.bindValue(":ID", getId());
    db.exec();
    if (db.size() > 0)
    {
        VERBOSE(VB_IMPORTANT, QString("%1 recently updated, skipping.")
                                    .arg(m_info->name));

        QString cachefile = QString("%1/cache_%2").arg(m_dir).arg(m_locale);
        QFile cache(cachefile);
        if (cache.exists() && cache.open( QIODevice::ReadOnly ))
        {
            m_buffer = cache.readAll();
            cache.close();

            processData();

            if (m_connectCnt)
            {
                emit newData(m_locale, m_units, m_data);
            }
            return;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, QString("No cachefile for %1, forcing "
                                          "update.").arg(m_info->name));
        }
    }

    m_data.clear();
    QString program = "nice";
    QStringList args;
    args.push_back(m_info->fileInfo.absoluteFilePath());
    args.push_back("-u");
    args.push_back(m_units == SI_UNITS ? "SI" : "ENG");

    if (!m_dir.isEmpty())
    {
        args.push_back("-d");
        args.push_back(m_dir);
    }
    args.push_back(m_locale);

    m_proc->setReadChannel(QProcess::StandardOutput);
    connect(m_proc, SIGNAL(readyRead()),
            this, SLOT(read()));
    connect(m_proc, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(processExit()));

    m_proc->setWorkingDirectory(m_info->fileInfo.absolutePath());
    m_proc_debug = QString("%1 %2").arg(program).arg(args.join(" "));
    m_proc->start(program, args);
    if (!m_proc->waitForStarted(1000))
    {
        VERBOSE(VB_IMPORTANT, "Error running script");
    }
    else
        m_scriptTimer->start(m_info->scriptTimeout);
}

bool WeatherSource::isRunning(void) const
{
    return QProcess::NotRunning != m_proc->state();
}

void WeatherSource::updateTimeout()
{
    if (!isRunning())
    {
        startUpdate();
        startUpdateTimer();
    }
}

void WeatherSource::read(void)
{
    m_buffer += m_proc->readAll();
}

void WeatherSource::processExit()
{
    VERBOSE(VB_GENERAL, QString("'%1' has exited").arg(m_proc_debug));
    m_proc->disconnect(); // disconnects all signals

    m_scriptTimer->stop();
    if (m_proc->exitStatus())
    {
        VERBOSE(VB_IMPORTANT, "script exit status " + m_proc->exitStatus());
        return;
    }

    QByteArray tempStr = m_proc->readAll();
    if (!tempStr.isEmpty())
        m_buffer += tempStr;

    QString cachefile = QString("%1/cache_%2").arg(m_dir).arg(m_locale);
    QFile cache(cachefile);
    if (cache.open( QIODevice::WriteOnly ))
    {
        cache.write(m_buffer);
        cache.close();
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to save data to cachefile: %1")
                                    .arg(cachefile));
    }

    processData();

    MSqlQuery db(MSqlQuery::InitCon());

    db.prepare("UPDATE weathersourcesettings "
               "SET updated = NOW() WHERE sourceid = :ID;");

    db.bindValue(":ID", getId());
    if (!db.exec())
    {
        VERBOSE(VB_IMPORTANT, db.lastError().text());
        return;
    }

    if (m_connectCnt)
    {
        emit newData(m_locale, m_units, m_data);
    }
}

void WeatherSource::processData()
{
    QStringList data = QString(m_buffer).split('\n', QString::SkipEmptyParts);

    for (int i = 0; i < data.size(); ++i)
    {
        QStringList temp = data[i].split("::", QString::SkipEmptyParts);
        if (temp.size() > 2)
            VERBOSE(VB_IMPORTANT, "Error parsing script file, ignoring");
        if (temp.size() < 2)
        {
            VERBOSE(VB_IMPORTANT,
                    "Unrecoverable error parsing script output " + temp.size());
            VERBOSE(VB_IMPORTANT, QString("data[%1]: '%2'")
                    .arg(i).arg(data[i]));
            return; // we don't emit signal
        }

        if (!m_data[temp[0]].isEmpty())
        {
            m_data[temp[0]].append("\n" + temp[1]);
        }
        else
            m_data[temp[0]] = temp[1];
    }
}

void WeatherSource::scriptTimeout()
{
    if (isRunning())
    {
        VERBOSE(VB_IMPORTANT,
                "Script timeout exceeded, summarily executing it");
        emit killProcess();
    }
}
