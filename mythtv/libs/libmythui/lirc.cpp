#include <QApplication>
#include <QEvent>
#include <QKeySequence>

#include <cstdio>
#include <cerrno>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "mythverbose.h"
#include "mythdb.h"
#include "mythsystem.h"

#include "lirc.h"
#include "lircevent.h"

LircThread::LircThread(QObject *main_window)
    : QThread(),
      m_lircConfig(NULL), m_mainWindow(main_window),
      m_bStop(false),     m_fd(-1),
      m_externalApp("")
{
}

/**
 *  \brief Initialise the class variables, read the lirc config and user
 *         settings
 */
int LircThread::Init(const QString &config_file, const QString &program,
                        bool ignoreExtApp)
{
    /* Connect the unix socket */
    m_fd = lirc_init((char *)qPrintable(program), 1);
    if (m_fd == -1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("lirc_init failed for %1, see preceding messages")
                .arg(program));
        return -1;
    }

    /* parse the config file */
    if (lirc_readconfig((char *)qPrintable(config_file), &m_lircConfig, NULL))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to read lirc config %1 for %2")
                .arg(config_file).arg(program));
        lirc_deinit();
        return -1;
    }

    if (!ignoreExtApp)
        m_externalApp = GetMythDB()->GetSetting("LircKeyPressedApp", "");

    VERBOSE(VB_GENERAL,
            QString("lirc init success using configuration file: %1")
            .arg(config_file));

    return 0;
}

LircThread::~LircThread()
{
    lirc_deinit();
    if (m_lircConfig)
        lirc_freeconfig(m_lircConfig);
}

/**
 *  \brief Main thread loop
 */
void LircThread::run(void)
{
    char *code = 0;
    char *ir = 0;
    int ret;
    fd_set readfds;
    struct timeval timeout;

    /* Process all events read */
    while (!m_bStop)
    {
        FD_ZERO(&readfds);
        FD_SET(m_fd, &readfds);

        // the maximum time select() should wait
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        ret = select(m_fd + 1, &readfds, NULL, NULL, &timeout);

        if (ret == 0)
            continue;

        if (ret == -1)
        {
            perror("LircThread - select");
            return;
        }

        if (ret == 1)
        {
            ret = lirc_nextcode(&ir);

            if (ret == -1)
            {
                if (errno != 0)
                    VERBOSE(VB_IMPORTANT,
                                QString("LircThread: lirc_nextcode failed"
                                        "last error was: %1").arg(errno));
                return;
            }

            if (!ir)
                continue;

            while ((ret = lirc_code2char(m_lircConfig, ir, &code)) == 0 &&
                code != NULL)
            {
                QKeySequence a(code);

                int keycode = 0;

                // Send a dummy keycode if we couldn't convert the key sequence.
                // This is done so the main code can output a warning for bad
                // mappings.
                if (!a.count())
                    QApplication::postEvent(m_mainWindow,
                                            new LircKeycodeEvent(code, keycode,
                                                                 true));

                for (unsigned int i = 0; i < a.count(); i++)
                {
                    keycode = a[i];

                    QApplication::postEvent(m_mainWindow,
                                            new LircKeycodeEvent(code, keycode,
                                                                 true));
                    QApplication::postEvent(m_mainWindow,
                                            new LircKeycodeEvent(code, keycode,
                                                                 false));

                    SpawnApp();
                }
            }

            free(ir);
            if (ret == -1)
                break;
        }
    }
}

/**
 *  \brief Spawn a user defined application which might do something like
 *         illuminating an led to give positive feedback that a key was received
 */
void LircThread::SpawnApp(void)
{
    if (m_externalApp.isEmpty())
        return;

    QString command = m_externalApp + " &";

    int status = myth_system(command);

    if (status > 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("External key pressed command exited with status %1")
                .arg(status));
    }
}
