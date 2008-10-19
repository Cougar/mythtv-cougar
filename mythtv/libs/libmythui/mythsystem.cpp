#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <QApplication>

#include "mythsystem.h"

#include "compat.h"

# ifdef linux
#   include <sys/vfs.h>
#   include <sys/statvfs.h>
#   include <sys/sysinfo.h>
# else
#   ifdef __FreeBSD__
#     include <sys/mount.h>
#   endif
#   ifdef CONFIG_CYGWIN
#     include <sys/statfs.h>
#   endif
#   ifndef _WIN32
#     include <sys/sysctl.h>
#   endif
# endif

#include "mythverbose.h"
#include "mythmainwindow.h"
#include "exitcodes.h"

#ifdef USE_LIRC
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenuevent.h"
#endif

/** \fn myth_system(const QString&, int)
 *  \brief Runs a system command inside the /bin/sh shell.
 *
 *  Note: Returns GENERIC_EXIT_NOT_OK if it can not execute the command.
 *  \return Exit value from command as an unsigned int in range [0,255].
 */
uint myth_system(const QString &command, int flags)
{
    (void)flags; /* Kill warning */

    bool ready_to_lock = HasMythMainWindow();

    (void)ready_to_lock; /* Kill warning */
#ifdef USE_LIRC
    bool lirc_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_LIRC);
    LircEventLock lirc_lock(lirc_lock_flag && ready_to_lock);
#endif

#ifdef USE_JOYSTICK_MENU
    bool joy_lock_flag = !(flags & MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);
    JoystickMenuEventLock joystick_lock(joy_lock_flag && ready_to_lock);
#endif

#ifdef BSD
    // Darwin waitpid() frequently fails EINTR (interrupted system call) -
    // I think because the parent is being toggled between kernel sleep/wake.
    // This seems to work around whatever is causing this 
    flags |= MYTH_SYSTEM_DONT_BLOCK_PARENT;
#endif

    QString LOC_ERR = QString("myth_system('%1'): Error: ").arg(command);

#ifndef USING_MINGW
    pid_t child = fork();

    if (child < 0)
    {
        /* Fork failed */
        VERBOSE(VB_IMPORTANT, (LOC_ERR + "fork() failed because %1")
                .arg(strerror(errno)));
        return GENERIC_EXIT_NOT_OK;
    }
    else if (child == 0)
    {
        /* Child */
        /* Close all open file descriptors except stdout/stderr */
        for (int i = sysconf(_SC_OPEN_MAX) - 1; i > 2; i--)
            close(i);

        /* Try to attach stdin to /dev/null */
        int fd = open("/dev/null", O_RDONLY);
        if (fd > 0)
        {
            // Note: dup2() will close old stdin descriptor.
            if (dup2(fd, 0) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Can not redirect /dev/null to standard input,"
                        "\n\t\t\tfailed to duplicate file descriptor." + ENO);
            }
            close(fd);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Can not redirect /dev/null "
                    "to standard input, failed to open." + ENO);
        }

        /* Run command */
        execl("/bin/sh", "sh", "-c", command.toUtf8().constData(), (char *)0);
        if (errno)
        {
            VERBOSE(VB_IMPORTANT, (LOC_ERR + "execl() failed because %1")
                    .arg(strerror(errno)));
        }

        /* Failed to exec */
        _exit(MYTHSYSTEM__EXIT__EXECL_ERROR); // this exit is ok
    }
    else
    {
        /* Parent */
        int status;

        if (flags & MYTH_SYSTEM_DONT_BLOCK_PARENT)
        {
            int res = 0;

            while (res == 0)
            {
                res = waitpid(child, &status, WNOHANG);
                if (res == -1)
                {
                    VERBOSE(VB_IMPORTANT,
                            (LOC_ERR + "waitpid() failed because %1")
                            .arg(strerror(errno)));
                    return GENERIC_EXIT_NOT_OK;
                }

                qApp->processEvents();


                if (res > 0)
                    return WEXITSTATUS(status);

                usleep(100000);
            }
        }
        else
        {
            if (waitpid(child, &status, 0) < 0)
            {
                VERBOSE(VB_IMPORTANT, (LOC_ERR + "waitpid() failed because %1")
                        .arg(strerror(errno)));
                return GENERIC_EXIT_NOT_OK;
            }
            return WEXITSTATUS(status);
        }
    }

#else

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    QString cmd = QString("cmd.exe /c %1").arg(command);
    if (!::CreateProcessA(NULL, cmd.toUtf8().data(), NULL, NULL,
                          FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        VERBOSE(VB_IMPORTANT, (LOC_ERR + "CreateProcess() failed because %1")
                .arg(::GetLastError()));
        return MYTHSYSTEM__EXIT__EXECL_ERROR;
    }
    else
    {
        if (::WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            VERBOSE(VB_IMPORTANT,
                    (LOC_ERR + "WaitForSingleObject() failed because %1")
                    .arg(::GetLastError()));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return GENERIC_EXIT_OK;
    }
#endif
    return GENERIC_EXIT_NOT_OK;
}

