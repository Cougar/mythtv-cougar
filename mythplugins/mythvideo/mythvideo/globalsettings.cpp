#include "mythtv/mythcontext.h"

#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>
#include "videodlg.h"
// General Settings

class VideoDefaultParentalLevel: public ComboBoxSetting, public GlobalSetting {
public:
    VideoDefaultParentalLevel() :
      GlobalSetting("VideoDefaultParentalLevel") {
        setLabel(QObject::tr("Starting Parental Level"));
        addSelection(QObject::tr("4 - Highest"), "4");
        addSelection(QObject::tr("1 - Lowest"), "1");
        addSelection(QObject::tr("2"), "2"); 
        addSelection(QObject::tr("3"), "3");
        setHelpText(QObject::tr("This is the 'level' that MythVideo starts at. "
                    "Any videos with a level at or below this will be shown in "
                    "the list or while browsing by default. The Parental PIN "
                    "should be set to limit changing of the default level."));
    }
};


class VideoDefaultView: public ComboBoxSetting, public GlobalSetting {
public:
    VideoDefaultView() :
      GlobalSetting("Default MythVideo View") {
        setLabel(QObject::tr("Default View"));
        addSelection(QObject::tr("Gallery"), "1");
        addSelection(QObject::tr("Browser"), "0");
        addSelection(QObject::tr("Listings"), "2"); 
        setHelpText(QObject::tr("The defualt view for MythVideo. "
                    "Other views can be reached via the popup menu available "
                    "via the MENU key."));
    }
};


class VideoAdminPassword: public LineEditSetting, public GlobalSetting {
public:
    VideoAdminPassword():
        GlobalSetting("VideoAdminPassword") {
        setLabel(QObject::tr("Parental Control PIN"));
        setHelpText(QObject::tr("This PIN is used to control the current "
                    "Parental Level. If you want to use this feature, then "
                    "setting the value to all numbers will make your life much "
                    "easier."));
    };
};

class VideoAggressivePC: public CheckBoxSetting, public GlobalSetting {
public:
    VideoAggressivePC():
        GlobalSetting("VideoAggressivePC") {
        setLabel(QObject::tr("Aggressive Parental Control"));
        setValue(false);
        setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Parental "
                    "PIN without first entering the current PIN. You have "
                    "been warned."));
    };
};

class VideoListUnknownFiletypes: public CheckBoxSetting, public GlobalSetting {
public:
    VideoListUnknownFiletypes():
        GlobalSetting("VideoListUnknownFiletypes") {
        setLabel(QObject::tr("Show Unknown File Types"));
        setValue(true);
        setHelpText(QObject::tr("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. "));
    };
};

class VideoTreeNoDB: public CheckBoxSetting, public GlobalSetting {
public:
    VideoTreeNoDB():
        GlobalSetting("VideoTreeNoDB") {
        setLabel(QObject::tr("Video List browses files"));
        setValue(false);
        setHelpText(QObject::tr("If set, this will cause the Video List "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    };
};

class VideoNewBrowsable: public CheckBoxSetting, public GlobalSetting {
public:
    VideoNewBrowsable():
        GlobalSetting("VideoNewBrowsable") {
        setLabel(QObject::tr("Newly scanned files are browsable by default"));
        setValue(true);
        setHelpText(QObject::tr("If set, newly scanned files in the Video Manager "
                                "will be marked as browsable and will appear in "
                                "the 'Browse' menu."));
    };
};


class SearchListingsCommand: public LineEditSetting, public GlobalSetting {
public:
    SearchListingsCommand():
        GlobalSetting("MovieListCommandLine") {
        setLabel(QObject::tr("Command to search for movie listings"));
        setValue(PREFIX "/share/mythtv/mythvideo/scripts/imdb.pl -M tv=no;video=no");
        setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    };
};


class GetPostersCommand: public LineEditSetting, public GlobalSetting {
public:
    GetPostersCommand():
        GlobalSetting("MoviePosterCommandLine") {
        setLabel(QObject::tr("Command to search for movie posters"));
        setValue(PREFIX "/share/mythtv/mythvideo/scripts/imdb.pl -P");
        setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    };
};


class GetDataCommand: public LineEditSetting, public GlobalSetting {
public:
    GetDataCommand():
        GlobalSetting("MovieDataCommandLine") {
        setLabel(QObject::tr("Command to extract data for movies"));
        setValue(PREFIX "/share/mythtv/mythvideo/scripts/imdb.pl -D");
        setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    };
};


class VideoStartupDirectory: public LineEditSetting, public GlobalSetting {
public:
    VideoStartupDirectory():
        GlobalSetting("VideoStartupDir") {
        setLabel(QObject::tr("Directory that holds videos"));
        setValue("/share/Movies/dvd");
        setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo only needs to have read permission "
                    "to the directory."));
    };
};


class VideoArtworkDirectory: public LineEditSetting, public GlobalSetting {
public:
    VideoArtworkDirectory():
        GlobalSetting("VideoArtworkDir") {
        setLabel(QObject::tr("Directory that holds movie posters"));
        char *home = getenv("HOME");
        setValue(QString(home) + "/.mythtv/MythVideo");
        setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    };
};

//Player Settings

class VideoDefaultPlayer: public LineEditSetting, public GlobalSetting {
public:
    VideoDefaultPlayer():
        GlobalSetting("VideoDefaultPlayer") {
        setLabel(QObject::tr("Default Player"));
        setValue("mplayer -fs -zoom -quiet -vo xv %s");
        setHelpText(QObject::tr("This is the command used for any file "
                    "that the extension is not specifically defined. "
                    "You may also enter the name of one of the playback "
                    "plugins such as 'Internal'."));
    };
};

class VideoGalleryRows: public SpinBoxSetting, public GlobalSetting {
public:
    VideoGalleryRows():
        SpinBoxSetting(2, 5, 1), GlobalSetting("VideoGalleryRowsPerPage") {
        setLabel(QObject::tr("Rows to display"));
        setValue(3);
    };
};

class VideoGalleryColumns: public SpinBoxSetting, public GlobalSetting {
public:
    VideoGalleryColumns():
        SpinBoxSetting(2, 5, 1), GlobalSetting("VideoGalleryColsPerPage") {
        setLabel(QObject::tr("Columns to display"));
        setValue(4);
    };
};

class VideoGallerySubtitle: public CheckBoxSetting, public GlobalSetting {
public:
    VideoGallerySubtitle():
        GlobalSetting("VideoGallerySubtitle") {
        setLabel(QObject::tr("Show title below thumbnails"));
        setValue(true);
        setHelpText(QObject::tr("If set, the additional text will make the thumbnails smaller."));
    };
};

class VideoGalleryAspectRatio: public CheckBoxSetting, public GlobalSetting {
public:
    VideoGalleryAspectRatio():
        GlobalSetting("VideoGalleryAspectRatio") {
        setLabel(QObject::tr("Maintain aspect ratio of thumbnails"));
        setValue(true);
        setHelpText(QObject::tr("If set, the scaled thumbnails will maintain their "
                                "original aspect ratio. If not set, they are scaled "
                                "to match the size of the background icon."));
    };
};

VideoGeneralSettings::VideoGeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings"));
    general->addChild(new VideoStartupDirectory());
    general->addChild(new VideoArtworkDirectory());
    general->addChild(new VideoDefaultParentalLevel());
    general->addChild(new VideoAdminPassword());
    general->addChild(new VideoAggressivePC());
    general->addChild(new VideoListUnknownFiletypes());
    general->addChild(new VideoTreeNoDB());
    general->addChild(new VideoNewBrowsable());
    general->addChild(new VideoDefaultView());
    addChild(general);

    VerticalConfigurationGroup* vman = new VerticalConfigurationGroup(false);
    vman->setLabel(QObject::tr("Video Manager"));
    vman->addChild(new SearchListingsCommand());
    vman->addChild(new GetPostersCommand());
    vman->addChild(new GetDataCommand());
    addChild(vman);

    VerticalConfigurationGroup * vgal = new VerticalConfigurationGroup(false);
    vgal->setLabel(QObject::tr("Video Gallery"));
    vgal->addChild(new VideoGalleryColumns());
    vgal->addChild(new VideoGalleryRows());
    vgal->addChild(new VideoGallerySubtitle());
    vgal->addChild(new VideoGalleryAspectRatio());
    addChild(vgal);
}

VideoPlayerSettings::VideoPlayerSettings()
{
    VerticalConfigurationGroup* playersettings = new VerticalConfigurationGroup(false);
    playersettings->setLabel(QObject::tr("Player Settings"));
    playersettings->addChild(new VideoDefaultPlayer());
    addChild(playersettings);
}
