#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qmap.h>
#include <unistd.h>
#include <qdir.h>
#include <qtextcodec.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "proglist.h"
#include "progfind.h"
#include "manualbox.h"
#include "manualschedule.h"
#include "playbackbox.h"
#include "viewscheduled.h"
#include "programrecpriority.h"
#include "channelrecpriority.h"
#include "globalsettings.h"
#include "profilegroup.h"

#include "themedmenu.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "guidegrid.h"
#include "mythplugin.h"
#include "remoteutil.h"
#include "xbox.h"
#include "dbcheck.h"
#include "mythmediamonitor.h"
#include "statusbox.h"

#define QUIT     1
#define HALT     2

ThemedMenu *menu;
QTranslator *translator;
XBox *xbox = NULL;

void startGuide(void)
{
    QString startchannel = gContext->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    RunProgramGuide(startchannel);
}

void startFinder(void)
{
    RunProgramFind();
}

void startSearchTitle(void)
{
  ProgLister searchTitle(plTitleSearch, "",
                         QSqlDatabase::database(),
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchTitle.exec();
    qApp->lock();
}

void startSearchKeyword(void)
{
  ProgLister searchKeyword(plKeywordSearch, "",
                        QSqlDatabase::database(),
                        gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchKeyword.exec();
    qApp->lock();
}

void startSearchPeople(void)
{
  ProgLister searchPeople(plPeopleSearch, "",
                         QSqlDatabase::database(),
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchPeople.exec();
    qApp->lock();
}

void startSearchChannel(void)
{
    ProgLister searchChannel(plChannel, "",
                             QSqlDatabase::database(),
                             gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchChannel.exec();
    qApp->lock();
}

void startSearchCategory(void)
{
  ProgLister searchCategory(plCategory, "",
                            QSqlDatabase::database(),
                            gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchCategory.exec();
    qApp->lock();
}

void startSearchMovie(void)
{
    ProgLister searchMovie(plMovies, "",
                           QSqlDatabase::database(),
                           gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchMovie.exec();
    qApp->lock();
}

void startSearchNew(void)
{
    ProgLister searchNew(plNewListings, "",
                         QSqlDatabase::database(),
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchNew.exec();
    qApp->lock();
}

void startSearchTime(void)
{
  ProgLister searchTime(plTime, "",
                         QSqlDatabase::database(),
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchTime.exec();
    qApp->lock();
}

void startManaged(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ViewScheduled vsb(db, gContext->GetMainWindow(), "view scheduled");

    qApp->unlock();
    vsb.exec();
    qApp->lock();
}

void startProgramRecPriorities(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ProgramRecPriority rsb(db, gContext->GetMainWindow(), "recpri scheduled");

    qApp->unlock();
    rsb.exec();
    qApp->lock();
}

void startChannelRecPriorities(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ChannelRecPriority rch(db, gContext->GetMainWindow(), "recpri channels");

    qApp->unlock();
    rch.exec();
    qApp->lock();
}

void startPlayback(void)
{
    PlaybackBox pbb(PlaybackBox::Play, gContext->GetMainWindow(), 
                    "tvplayselect");

    qApp->unlock();
    pbb.exec();
    qApp->lock();
}

void startDelete(void)
{
    PlaybackBox delbox(PlaybackBox::Delete, gContext->GetMainWindow(), 
                       "tvplayselect");
   
    qApp->unlock();
    delbox.exec();
    qApp->lock();
}

void startManual(void)
{
    ManualBox manbox(gContext->GetMainWindow(), "manual box");

    qApp->unlock();
    manbox.exec();
    qApp->lock();
}

void startManualSchedule(void)
{
    ManualSchedule mansched(gContext->GetMainWindow(), "manual schedule");

    qApp->unlock();
    mansched.exec();
    qApp->lock();
}

void startTV(void)
{
    TV *tv = new TV();

    QTime timer;
    timer.start();

    tv->Init();

    bool tryTV = false;
    bool tryRecorder = false;
    bool quitAll = false;

    bool showDialogs = true;
    if (!tv->LiveTV(showDialogs))
    {
        tv->StopLiveTV();
        quitAll = true;
    }

    while (!quitAll)
    {
        timer.restart();
        while (tryRecorder)
        { //try to play recording, if filenotfound retry until timeout

            if (tv->PlayFromRecorder(tv->GetLastRecorderNum()) == 1)
                tryRecorder = false;
            else if (timer.elapsed() > 2000)
            {
                tryRecorder = false;
                tryTV = true;
            }
            else
            {
                qApp->unlock();
                qApp->processEvents();
                usleep(100000);
                qApp->lock();
             }
        }

        timer.restart();
        while (tryTV)
        {//try to start livetv
            bool showDialogs = false;
            if (tv->LiveTV(showDialogs) == 1) //1 == livetv ok
                tryTV = false;
            else if (timer.restart() > 2000)
            {
                tryTV = false;
                quitAll = true;
            }
            else
            {
                qApp->unlock();
                qApp->processEvents();
                usleep(100000);
                qApp->lock();
            }
        }

        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(100);
            qApp->lock();
        }

        if (tv->WantsToQuit() && !tv->IsSwitchingCards())
            quitAll = true;
        else
        {
            if (tv->IsSwitchingCards())
            {
                tryRecorder = false;
                tryTV = true;
            }
            else
            {
                tryRecorder = true;
                tryTV = false;
            }
        }
    }

    delete tv;
}

void showStatus(void)
{
    StatusBox statusbox(gContext->GetMainWindow(), "status box");
    qApp->unlock();
    statusbox.exec();
    qApp->lock();
}

void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel == "tv_watch_live")
        startTV();
    else if (sel == "tv_watch_recording")
        startPlayback();
    else if (sel == "tv_schedule")
        startGuide();
    else if (sel == "tv_delete")
        startDelete();
    else if (sel == "tv_manual")
        startManual();
    else if (sel == "tv_manualschedule")
        startManualSchedule();
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "tv_set_recpriorities")
        startProgramRecPriorities();
    else if (sel == "tv_progfind")
        startFinder();
    else if (sel == "tv_search_title")
        startSearchTitle();
    else if (sel == "tv_search_keyword")
        startSearchKeyword();
    else if (sel == "tv_search_people")
        startSearchPeople();
    else if (sel == "tv_search_channel")
        startSearchChannel();
    else if (sel == "tv_search_category")
        startSearchCategory();
    else if (sel == "tv_search_movie")
        startSearchMovie();
    else if (sel == "tv_search_new")
        startSearchNew();
    else if (sel == "tv_search_time")
        startSearchTime();
    else if (sel == "settings appearance") 
    {
        AppearanceSettings settings;
        settings.exec(QSqlDatabase::database());

        qApp->removeTranslator(translator);
        translator->load(PREFIX + QString("/share/mythtv/i18n/mythfrontend_") +
                         QString(gContext->GetSetting("Language").lower()) +
                         QString(".qm"), ".");
        qApp->installTranslator(translator);

        gContext->LoadQtConfig();
        gContext->GetMainWindow()->Init();
        gContext->UpdateImageCache();
        menu->ReloadTheme();
    } 
    else if (sel == "settings recording") 
    {
        ProfileGroupEditor editor(QSqlDatabase::database());
        editor.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings general") 
    {
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings maingeneral") 
    {
        MainGeneralSettings mainsettings;
        mainsettings.exec(QSqlDatabase::database());
        menu->ReloadExitKey();
        QStringList strlist = QString("REFRESH_BACKEND");
        gContext->SendReceiveStringList(strlist);
    } 
    else if (sel == "settings playback") 
    {
        PlaybackSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings epg") 
    {
        EPGSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings generalrecpriorities") 
    {
        GeneralRecPrioritiesSettings settings;
        settings.exec(QSqlDatabase::database());
        ScheduledRecording::signalChange(QSqlDatabase::database());
    } 
    else if (sel == "settings channelrecpriorities") 
    {
        startChannelRecPriorities();
    }
    else if (xbox && sel == "settings xboxsettings")
    {
        XboxSettings settings;
        settings.exec(QSqlDatabase::database());

        xbox->GetSettings();
    }
    else if (sel == "tv_status")
        showStatus();
}

int handleExit(void)
{
    if (gContext->GetNumSetting("NoPromptOnExit") == 1)
        return QUIT;

    // first of all find out, if a backend runs on this host...
    bool backendOnLocalhost = false;

    QStringList strlist = "";
    strlist << "QUERY_IS_ACTIVE_BACKEND";
    strlist << gContext->GetHostName();

    gContext->SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "FALSE")
        backendOnLocalhost = false;
    else //if (QString(strlist[0]) == "TRUE")
        backendOnLocalhost = true;

    QString title = QObject::tr("Do you really want to exit MythTV?");

    DialogBox diag(gContext->GetMainWindow(), title);
    diag.AddButton(QObject::tr("No"));
    diag.AddButton(QObject::tr("Yes, Exit now"));
    if (!backendOnLocalhost)
        diag.AddButton(QObject::tr("Yes, Exit and Shutdown"));

    int result = diag.exec();
    switch (result)
    {
        case 2: return QUIT;
        case 3: return HALT;
        default: return 0;
    }

    return 0;
}

void haltnow()
{
    QString halt_cmd = gContext->GetSetting("HaltCommand", 
                                            "sudo /sbin/halt -p");
    if (!halt_cmd.isEmpty())
        system(halt_cmd.ascii());
}

int RunMenu(QString themedir)
{
    menu = new ThemedMenu(themedir.ascii(), "mainmenu.xml", 
                          gContext->GetMainWindow(), "mainmenu");
    menu->setCallback(TVMenuCallback, gContext);
   
    int exitstatus = 0;
 
    if (menu->foundTheme())
    {
        do {
            menu->exec();
        } while (!(exitstatus = handleExit()));
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    return exitstatus;
}   

// If any settings are missing from the database, this will write
// the default values
void WriteDefaults(QSqlDatabase* db) 
{
    PlaybackSettings ps;
    ps.load(db);
    ps.save(db);
    GeneralSettings gs;
    gs.load(db);
    gs.save(db);
    EPGSettings es;
    es.load(db);
    es.save(db);
    AppearanceSettings as;
    as.load(db);
    as.save(db);
    MainGeneralSettings mgs;
    mgs.load(db);
    mgs.save(db);
    GeneralRecPrioritiesSettings grs;
    grs.load(db);
    grs.save(db);
}

QString RandTheme(QString &themename, QSqlDatabase *db)
{
    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);

    QFileInfoListIterator it( *fil);
    QFileInfo *theme;
    QStringList themelist;

    srand(time(NULL));

    for ( ; it.current() !=0; ++it)
    {
        theme = it.current();
        if (theme->fileName() == "." || theme->fileName() =="..")
            continue;

        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (!preview.exists() || !xml.exists())
            continue;

        // We don't want the same one as last time.
        if (theme->fileName() != themename)
            themelist.append(theme->fileName());
    }

    if (themelist.size()) 
        themename = themelist[rand() % themelist.size()];

    ThemeSelector Theme;
    Theme.setValue(themename);
    Theme.save(db);

    return themename;
}

int internal_play_media(const char *mrl, const char* plot, const char* title, 
                        const char* director, int lenMins, const char* year) 
{
    int res = -1; 
    
   
    TV *tv = new TV();

    tv->Init();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->recstartts = QDateTime::currentDateTime().addSecs((0 - (lenMins + 1)) * 60 );
    pginfo->recendts = QDateTime::currentDateTime().addSecs(-60);
    pginfo->recstartts.setDate(QDate::fromString(year));
    pginfo->lenMins = lenMins;
    pginfo->isVideo = true;
    pginfo->pathname = mrl;
    pginfo->description = plot;
    
    
    if (strlen(director))
        pginfo->subtitle = QString( "%1: %2" ).arg(QObject::tr("Directed By")).arg(director);
    
    pginfo->title = title;

    tv->Playback(pginfo);
    
    if (tv->Playback(pginfo))
    {
        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(10000);
            qApp->lock();
        }
    }
    

    res = 0;
    
    sleep(1);
    delete tv;
    delete pginfo;
    
    return res;
}

void InitJumpPoints(void)
{
    REG_JUMP("Program Guide", "", "", startGuide);
    REG_JUMP("Program Finder", "", "", startFinder);
    //REG_JUMP("Search Listings", "", "", startSearch);
    REG_JUMP("Manage Recordings / Fix Conflicts", "", "", startManaged);
    REG_JUMP("Program Recording Priorities", "", "", startProgramRecPriorities);
    REG_JUMP("Channel Recording Priorities", "", "", startChannelRecPriorities);
    REG_JUMP("TV Recording Playback", "", "", startPlayback);
    REG_JUMP("TV Recording Deletion", "", "", startDelete);
    REG_JUMP("Live TV", "", "", startTV);
    REG_JUMP("Manual Record Scheduling", "", "", startManual);

    TV::InitKeys();
}

int internal_media_init() 
{
    REG_MEDIAPLAYER("Internal", "MythTV's native media player.", 
                    internal_play_media);
    return 0;
}

static void *run_priv_thread(void *data)
{
    (void)data;
    while (true) 
    {
        if (gContext->hasPrivRequest()) 
        {
            MythPrivRequest req = gContext->popPrivRequest();
            switch (req.getType()) 
            {
            case MythPrivRequest::MythRealtime:
                {
                    pthread_t *target_thread = (pthread_t *)(req.getData());
                    // Raise the given thread to realtime priority
                    struct sched_param sp = {1};
                    int status = pthread_setschedparam(
                        *target_thread, SCHED_FIFO, &sp);
                    if (status) {
                        perror("pthread_setschedparam");
                        VERBOSE(VB_GENERAL, "Running as SUID root would allow "
                                "some threads to run with realtime priority, "
                                "improving video smoothness.");
                    }
                }
                break;
            case MythPrivRequest::MythExit:
                pthread_exit(NULL);
                break;
            case MythPrivRequest::PrivEnd:
                break;
            }
        }
        sleep(1);
    }
    return NULL; // will never happen
}
   
int main(int argc, char **argv)
{
    QString lcd_host;
    int lcd_port;

    QApplication a(argc, argv);
    translator = new QTranslator(0);

    QString logfile = "";
    QString verboseString = QString(" important general");

    QString pluginname = "";

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    if (binname != "mythfrontend")
        pluginname = binname;

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc() > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return -1;
                }
                else
                {
                    ++argpos;
                }
            }
        } else if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[argpos+1];
                if (temp.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -v/--verbose option\n";
                    return -1;
                } else
                {
                    QStringList verboseOpts;
                    verboseOpts = QStringList::split(',',a.argv()[argpos+1]);
                    ++argpos;
                    for (QStringList::Iterator it = verboseOpts.begin(); 
                         it != verboseOpts.end(); ++it )
                    {
                        if (!strcmp(*it,"none"))
                        {
                            print_verbose_messages = VB_NONE;
                            verboseString = "";
                        }
                        else if (!strcmp(*it,"all"))
                        {
                            print_verbose_messages = VB_ALL;
                            verboseString = "all";
                        }
                        else if (!strcmp(*it,"quiet"))
                        {
                            print_verbose_messages = VB_IMPORTANT;
                            verboseString = "important";
                        }
                        else if (!strcmp(*it,"record"))
                        {
                            print_verbose_messages |= VB_RECORD;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"playback"))
                        {
                            print_verbose_messages |= VB_PLAYBACK;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"channel"))
                        {
                            print_verbose_messages |= VB_CHANNEL;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"osd"))
                        {
                            print_verbose_messages |= VB_OSD;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"file"))
                        {
                            print_verbose_messages |= VB_FILE;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"schedule"))
                        {
                            print_verbose_messages |= VB_SCHEDULE;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"network"))
                        {
                            print_verbose_messages |= VB_NETWORK;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"commflag"))
                        {
                            print_verbose_messages |= VB_COMMFLAG;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"audio"))
                        {
                            print_verbose_messages |= VB_AUDIO;
                            verboseString += " " + *it;
                        }
                        else if (!strcmp(*it,"libav"))
                        {
                            print_verbose_messages |= VB_LIBAV;
                            verboseString += " " + *it;
                        }
                        else
                        {
                            cerr << "Unknown argument for -v/--verbose: "
                                 << *it << endl;;
                        }
                    }
                }
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return -1;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--version"))
        {
            cout << MYTH_BINARY_VERSION << endl;
            exit(0);
        }
        else if ((argpos + 1 == a.argc()) &&
                    !QString(a.argv()[argpos]).startsWith("-"))
        {
            pluginname = a.argv()[argpos];
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: " << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-v or --verbose debug-level    Prints more information" << endl <<
                    "                               Accepts any combination (separated by comma)" << endl << 
                    "                               of all,none,quiet,record,playback,channel," << endl <<
                    "                               osd,file,schedule,network,commflag,audio,libav" << endl <<
                    "--version                      Version information" << endl <<
                    "<plugin>                       Initialize and run this plugin" << endl;
            return -1;
        }
    }

    int logfd = -1;

    if (logfile != "")
    {
        logfd = open(logfile.ascii(), O_WRONLY|O_CREAT|O_APPEND, 0664);
 
        if (logfd < 0)
        {
            perror("open(logfile)");
            return -1;
        }
    }

    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    QString fileprefix = QDir::homeDirPath() + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    
    // Create priveleged thread, then drop privs
    pthread_t priv_thread;

    int status = pthread_create(&priv_thread, NULL, run_priv_thread, NULL);
    if (status) 
    {
        perror("pthread_create");
        priv_thread = 0;
    }
    setuid(getuid());


    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        int pause;
        if ((pause = gContext->GetNumSetting("WOLsqlReconnectWaitTime", 0)) > 0)
        {
            int acttry = 1;
            int retries = gContext->GetNumSetting("WOLsqlConnectRetry", 5);

            QString WOLsqlCommand = gContext->GetSetting("WOLsqlCommand", 
                                                   "echo \'WOLsqlServerCommand "
                                                   "not set\nPlease do so in "
                                                   "your mysql.txt!\'");

            if (!WOLsqlCommand.isEmpty())
            {
                while (acttry <= retries && !gContext->OpenDatabase(db))
                {
                    printf("Trying to wakeup SQLserver (Try %d of %d)\n",
                           acttry, retries);
                    system(WOLsqlCommand);
                    sleep(pause);
                    ++acttry;
                }
            }

            if (WOLsqlCommand.isEmpty() || (acttry > retries))
            {
                printf("Sorry, couldn't open db\nExiting.\n");
                return -1;
            }
        }
        else
            return -1;
    }

    UpgradeTVDatabaseSchema();

    VERBOSE(VB_ALL, QString("%1 version: %2 www.mythtv.org")
                            .arg(binname).arg(MYTH_BINARY_VERSION));

    VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

    lcd_host = gContext->GetSetting("LCDHost", "localhost");
    lcd_port = gContext->GetNumSetting("LCDPort", 13666);
    if (lcd_host.length() > 0 && lcd_port > 1024 && gContext->GetLCDDevice())
    {
        gContext->GetLCDDevice()->connectToHost(lcd_host, lcd_port);
        gContext->GetLCDDevice()->setupLEDs(RemoteGetRecordingMask);
    }

    translator->load(PREFIX + QString("/share/mythtv/i18n/mythfrontend_") + 
                     QString(gContext->GetSetting("Language").lower()) + 
                     QString(".qm"), ".");
    a.installTranslator(translator);

    WriteDefaults(db);

    QString themename = gContext->GetSetting("Theme", "blue");
    bool randomtheme = gContext->GetNumSetting("RandomTheme", 0);

    if (randomtheme)
        themename = RandTheme(themename, db);

    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(21);
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    InitJumpPoints();

    internal_media_init();

    MythPluginManager *pmanager = new MythPluginManager();
    gContext->SetPluginManager(pmanager);

    gContext->UpdateImageCache();

    if (gContext->GetNumSetting("EnableXbox") == 1)
    {
        xbox = new XBox();
        xbox->GetSettings();
    }
    else
        xbox = NULL;

    // this direct connect is only necessary, if the user wants to use the
    // auto shutdown/wakeup feature
    if (gContext->GetNumSetting("idleTimeoutSecs",0) > 0)
        gContext->ConnectToMasterServer();

    if (pluginname != "")
    {
        if (pmanager->run_plugin(pluginname))
            return 0;
        else
        {
            pluginname = "myth" + pluginname;
            if (pmanager->run_plugin(pluginname))
                return 0;
        }
    }

    qApp->lock();
    qApp->unlock();

#ifndef _WIN32
    MediaMonitor *mon = NULL;
    mon = MediaMonitor::getMediaMonitor();
    if (mon)
    {
        VERBOSE(VB_ALL, QString("Starting media monitor."));
        mon->startMonitoring();
    }
#endif

    int exitstatus = RunMenu(themedir);

    if (exitstatus == HALT)
        haltnow();

#ifndef _WIN32
    if (mon)
    {
        mon->stopMonitoring();
        delete mon;
    }
#endif

    if (priv_thread != 0)
    {
        void *value;
        gContext->addPrivRequest(MythPrivRequest::MythExit, NULL);
        pthread_join(priv_thread, &value);
    }

    delete mainWindow;
    delete gContext;
    return exitstatus;
}
