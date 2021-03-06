#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// qt
#include <QApplication>

// myth
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "settings.h"
#include "langsettings.h"
#include "mythdbcon.h"
#include "exitcodes.h"
#include "compat.h"
#include "mythuihelper.h"
#include "lcddevice.h"
#include "myththemebase.h"

#include "libmythtv/tv.h"

// mythwelcome
#include "welcomedialog.h"
#include "welcomesettings.h"


QString logfile = "";

static bool log_rotate(bool report_error);
static void log_rotate_handler(int);


void initKeys(void)
{
    REG_KEY("Welcome", "STARTXTERM", "Open an Xterm window", "F12");
    REG_KEY("Welcome", "SHOWSETTINGS", "Show Mythshutdown settings", "F11");
    REG_KEY("Welcome", "STARTSETUP", "Start Mythtv-Setup", "");
}

int main(int argc, char **argv)
{
    bool bShowSettings = false;

    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not initialize myth context. "
                        "Exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not open the database. "
                        "Exiting.");
        return -1;
    }

    // Check command line arguments
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return FRONTEND_EXIT_INVALID_CMDLINE;

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--setup"))
        {
            bShowSettings = true;
        }
        else if (!strcmp(a.argv()[argpos], "-l") ||
            !strcmp(a.argv()[argpos], "--logfile"))
        {
            if (a.argc()-1 > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
                    "-s or --setup                  Run setup for the mythshutdown program" << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl;
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (logfile != "")
    {
        if (!log_rotate(true))
            cerr << "cannot open logfile; using stdout/stderr" << endl;
        else
            signal(SIGHUP, &log_rotate_handler);
    }

    LCD::SetupLCD();

    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime();

    LanguageSettings::load("mythfrontend");

    GetMythUI()->LoadQtConfig();

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    MythThemeBase *themeBase = new MythThemeBase();

    initKeys();

    if (bShowSettings)
    {
        MythShutdownSettings settings;
        settings.exec();
    }
    else
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        WelcomeDialog *welcome = new WelcomeDialog(mainStack, "mythwelcome");

        if (welcome->Create())
            mainStack->AddScreen(welcome, false);
        else
            return -1;

        do
        {
            qApp->processEvents();
            usleep(5000);
        } while (mainStack->TotalScreens() > 0);
    }

    DestroyMythMainWindow();

    delete themeBase;
    delete gContext;

    return 0;
}


static bool log_rotate(bool report_error)
{
    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND, 0664);

    if (new_logfd < 0)
    {
        /* If we can't open the new logfile, send data to /dev/null */
        if (report_error)
        {
            cerr << "cannot open logfile " << logfile.toAscii().constData() << endl;
            return false;
        }

        new_logfd = open("/dev/null", O_WRONLY);

        if (new_logfd < 0)
        {
            /* There's not much we can do, so punt. */
            return false;
        }
    }

    while (dup2(new_logfd, 1) < 0 && errno == EINTR);
    while (dup2(new_logfd, 2) < 0 && errno == EINTR);
    while (close(new_logfd) < 0   && errno == EINTR);

    return true;
}


static void log_rotate_handler(int)
{
    log_rotate(false);
}
