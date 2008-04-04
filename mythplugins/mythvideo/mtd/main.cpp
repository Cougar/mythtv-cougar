/*
    main.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Starting point for the myth transcoding daemon

*/

#include <QApplication>
#include <QSqlDatabase>
#include <QFile>
#include <Q3TextStream>

#include <unistd.h>

#include <mythtv/exitcodes.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/langsettings.h>
#include <mythtv/compat.h>

#include "../mythvideo/dbcheck.h"
#include "mtd.h"

#define MTD_EXIT_DAEMONIZING_ERROR                FRONTEND_EXIT_START-1

int main(int argc, char **argv)
{
    QApplication a(argc, argv, false);
    bool daemon_mode = false;
    int  special_port = -1;    
    //
    //  Check command line arguments
    //
    
    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        
        if (!strcmp(a.argv()[argpos],"-d") ||
            !strcmp(a.argv()[argpos],"--daemon"))
        {
            daemon_mode = true;
        }
        else if (!strcmp(a.argv()[argpos],"-n") ||
            !strcmp(a.argv()[argpos],"--nodaemon"))
        {
            daemon_mode = false;
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
            !strcmp(a.argv()[argpos],"--port"))
        {
            if (a.argc() > argpos)
            {
                QString port_number = a.argv()[argpos+1];
                ++argpos;
                special_port = port_number.toInt();
                if(special_port < 1 ||
                   special_port > 65534)
                {
                    VERBOSE(VB_IMPORTANT, "mtd: Bad port number");
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
            } 
            else 
            {
                VERBOSE(VB_IMPORTANT, "mtd: Missing argument to -p/--port option");
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-v"))
        {
            if (++argpos >= argc) {
                cerr << "Error: -v requires parameters (try -v help)" <<
                        std::endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            int err;
            if ((err = parse_verbose_arg(a.argv()[argpos])) !=
                GENERIC_EXIT_OK)
            {
                return err;
            }
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-p or --port number  A port number to listen on (default is 2442) " << endl <<
                    "-d or --daemon       Runs mtd as a daemon " << endl <<
                    "-n or --nodaemon     Does not run mtd as a daemon (default)" << endl;
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }
    
    //
    //  Switch to daemon mode?
    //
    
    if(daemon_mode)
    {
        if(daemon(0, 1) < 0)
        {
            cerr << "mtd: Failed to run as a daemon. Bailing out." << endl ;
            return MTD_EXIT_DAEMONIZING_ERROR;
        }
        cout << endl;
    }

    //
    //  Get the Myth context and db hooks
    //    
    
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cerr << "Could not initialize myth context. Exiting." << endl;
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "mtd: Couldn't open database. I go away now. " << endl;
        return FRONTEND_EXIT_DB_ERROR;
    }

    gContext->ActivateSettingsCache(false);
    UpgradeVideoDatabaseSchema();
    gContext->ActivateSettingsCache(true);
 
    //
    //  Figure out port to listen on
    //

    int assigned_port = gContext->GetNumSetting("MTDPort", 2442);
    if(special_port > 0)
    {
        assigned_port = special_port;
    }



    //
    //  Where to log
    //
   
    bool log_stdout = false;
    if(gContext->GetNumSetting("MTDLogFlag", 0))
    {
        log_stdout = true;
    } 
    if(!daemon_mode)
    {
        log_stdout = true;
    }

    //
    //  Nice ourself
    //
   
    LanguageSettings::load("mythdvd");
    
    new MTD(assigned_port, log_stdout);
    
    a.exec();
                                
    delete gContext;
    return FRONTEND_EXIT_OK;
}

