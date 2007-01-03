#include <mythtv/mythcontext.h>

#include "globalsettings.h"
#include "globals.h"

namespace
{
// General Settings
HostComboBox *VideoDefaultParentalLevel()
{
    HostComboBox *gc = new HostComboBox("VideoDefaultParentalLevel");
    gc->setLabel(QObject::tr("Starting Parental Level"));
    gc->addSelection(QObject::tr("4 - Highest"), "4");
    gc->addSelection(QObject::tr("1 - Lowest"), "1");
    gc->addSelection("2");
    gc->addSelection("3");
    gc->setHelpText(QObject::tr("This is the 'level' that MythVideo starts at. "
                    "Any videos with a level at or below this will be shown in "
                    "the list or while browsing by default. The Parental PIN "
                    "should be set to limit changing of the default level."));
    return gc;
}

HostComboBox *VideoDefaultView()
{
    HostComboBox *gc = new HostComboBox("Default MythVideo View");
    gc->setLabel(QObject::tr("Default View"));
    gc->addSelection(QObject::tr("Gallery"), "1");
    gc->addSelection(QObject::tr("Browser"), "0");
    gc->addSelection(QObject::tr("Listings"), "2");
    gc->setHelpText(QObject::tr("The default view for MythVideo. "
                    "Other views can be reached via the popup menu available "
                    "via the MENU key."));
    return gc;
}

HostLineEdit *VideoAdminPassword()
{
    HostLineEdit *gc = new HostLineEdit("VideoAdminPassword");
    gc->setLabel(QObject::tr("Parental Control PIN"));
    gc->setHelpText(QObject::tr("This PIN is used to control the current "
                    "Parental Level. If you want to use this feature, then "
                    "setting the value to all numbers will make your life much "
                    "easier."));
    return gc;
}

HostCheckBox *VideoAggressivePC()
{
    HostCheckBox *gc = new HostCheckBox("VideoAggressivePC");
    gc->setLabel(QObject::tr("Aggressive Parental Control"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Parental "
                    "PIN without first entering the current PIN. You have "
                    "been warned."));
    return gc;
}

HostCheckBox *VideoListUnknownFiletypes()
{
    HostCheckBox *gc = new HostCheckBox("VideoListUnknownFiletypes");
    gc->setLabel(QObject::tr("Show Unknown File Types"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, all files below the Myth Video "
                    "directory will be displayed unless their "
                    "extension is explicitly set to be ignored. "));
    return gc;
}

HostCheckBox *VideoBrowserNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoBrowserNoDB");
    gc->setLabel(QObject::tr("Video Browser browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video Browser "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

HostCheckBox *VideoGalleryNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoGalleryNoDB");
    gc->setLabel(QObject::tr("Video Gallery browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video Gallery "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

HostCheckBox *VideoTreeNoDB()
{
    HostCheckBox *gc = new HostCheckBox("VideoTreeNoDB");
    gc->setLabel(QObject::tr("Video List browses files"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, this will cause the Video List "
                    "screen to show all relevant files below "
                    "the MythVideo starting directory whether "
                    "they have been scanned or not."));
    return gc;
}

HostCheckBox *VideoTreeNoMetaData()
{
    HostCheckBox *gc = new HostCheckBox("VideoTreeLoadMetaData");
    gc->setLabel(QObject::tr("Video List Loads Video Meta Data"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set along with Browse Files, this "
                    "will cause the Video List to load any known video meta"
                    "data from the database. Turning this off can greatly "
                    " speed up how long it takes to load the Video List tree"));
    return gc;
}

HostCheckBox *VideoNewBrowsable()
{
    HostCheckBox *gc = new HostCheckBox("VideoNewBrowsable");
    gc->setLabel(QObject::tr("Newly scanned files are browsable by default"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, newly scanned files in the Video "
                    "Manager will be marked as browsable and will appear in "
                    "the 'Browse' menu."));
    return gc;
}

HostCheckBox *VideoSortIgnoresCase()
{
    HostCheckBox *hcb = new HostCheckBox("mythvideo.sort_ignores_case");
    hcb->setLabel(QObject::tr("Sorting ignores case"));
    hcb->setValue(true);
    hcb->setHelpText(QObject::tr("If set, case is ignored when sorting "
                                 "entries in a view."));
    return hcb;
}

HostCheckBox *VideoDBFolderView()
{
    HostCheckBox *hcb = new HostCheckBox("mythvideo.db_folder_view");
    hcb->setLabel(QObject::tr("Show folders for database views"));
    hcb->setValue(true);
    hcb->setHelpText(QObject::tr("If set, sub folders of your video "
                                 "directory will be shown in supported "
                                 "views."));
    return hcb;
}

HostSpinBox *VideoImageCacheSize()
{
    HostSpinBox *hsb = new HostSpinBox("mythvideo.ImageCacheSize", 10,
                                       1000, 10);
    hsb->setValue(50);
    hsb->setLabel(QObject::tr("Image cache size"));
    hsb->setHelpText(QObject::tr("This setting determines the number "
                                 "of images MythVideo will cache for "
                                 "views."));
    return hsb;
}

HostLineEdit *SearchListingsCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieListCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie listings"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl " +
                 "-M tv=no;video=no");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetPostersCommand()
{
    HostLineEdit *gc = new HostLineEdit("MoviePosterCommandLine");
    gc->setLabel(QObject::tr("Command to search for movie posters"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -P");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *GetDataCommand()
{
    HostLineEdit *gc = new HostLineEdit("MovieDataCommandLine");
    gc->setLabel(QObject::tr("Command to extract data for movies"));
    gc->setValue(gContext->GetShareDir() + "mythvideo/scripts/imdb.pl -D");
    gc->setHelpText(QObject::tr("This command must be "
                    "executable by the user running MythVideo."));
    return gc;
}

HostLineEdit *VideoStartupDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoStartupDir");
    gc->setLabel(QObject::tr("Directory that holds videos"));
    gc->setValue(DEFAULT_VIDEOSTARTUP_DIR);
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo only needs to have read permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *VideoArtworkDirectory()
{
    HostLineEdit *gc = new HostLineEdit("VideoArtworkDir");
    gc->setLabel(QObject::tr("Directory that holds movie posters"));
    gc->setValue(MythContext::GetConfDir() + "/MythVideo");
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythVideo needs to have read/write permission "
                    "to the directory."));
    return gc;
}

//Player Settings

HostLineEdit *VideoDefaultPlayer()
{
    HostLineEdit *gc = new HostLineEdit("VideoDefaultPlayer");
    gc->setLabel(QObject::tr("Default Video Player"));
    gc->setValue("mplayer -fs -zoom -quiet -vo xv %s");
    gc->setHelpText(QObject::tr("This is the command used for any file "
                    "that the extension is not specifically defined. "
                    "You may also enter the name of one of the playback "
                    "plugins such as 'Internal'."));
    return gc;
}

HostSpinBox *VideoGalleryRows()
{
    HostSpinBox *gc = new HostSpinBox("VideoGalleryRowsPerPage", 2, 5, 1);
    gc->setLabel(QObject::tr("Rows to display"));
    gc->setValue(3);
    return gc;
}

HostSpinBox *VideoGalleryColumns()
{
    HostSpinBox *gc = new HostSpinBox("VideoGalleryColsPerPage", 2, 6, 1);
    gc->setLabel(QObject::tr("Columns to display"));
    gc->setValue(4);
    return gc;
}

HostCheckBox *VideoGallerySubtitle()
{
    HostCheckBox *gc = new HostCheckBox("VideoGallerySubtitle");
    gc->setLabel(QObject::tr("Show title below thumbnails"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the additional text will make the "
                    "thumbnails smaller."));
    return gc;
}

HostCheckBox *VideoGalleryAspectRatio()
{
    HostCheckBox *gc = new HostCheckBox("VideoGalleryAspectRatio");
    gc->setLabel(QObject::tr("Maintain aspect ratio of thumbnails"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, the scaled thumbnails will maintain "
                    "their original aspect ratio. If not set, they are scaled "
                    "to match the size of the background icon."));
    return gc;
}

///////////////////////////////////////////////////////////
//// DVD Settings
///////////////////////////////////////////////////////////

// General Settings

HostLineEdit *SetVCDDevice()
{
    HostLineEdit *gc = new HostLineEdit("VCDDeviceLocation");
    gc->setLabel(QObject::tr("Location of VCD device"));
    gc->setValue("/dev/cdrom");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    return gc;
}

HostLineEdit *SetDVDDevice()
{
    HostLineEdit *gc = new HostLineEdit("DVDDeviceLocation");
    gc->setLabel(QObject::tr("Location of DVD device"));
    gc->setValue("/dev/dvd");
    gc->setHelpText(QObject::tr("This device must exist, and the user "
                    "running MythDVD needs to have read permission "
                    "on the device."));
    return gc;
}

HostComboBox *SetOnInsertDVD()
{
    HostComboBox *gc = new HostComboBox("DVDOnInsertDVD");
    gc->setLabel(QObject::tr("On DVD insertion"));
    gc->addSelection(QObject::tr("Display mythdvd menu"),"1");
    gc->addSelection(QObject::tr("Do nothing"),"0");
    gc->addSelection(QObject::tr("Play DVD"),"2");
    gc->addSelection(QObject::tr("Rip DVD"),"3");
    gc->setHelpText(QObject::tr("Media Monitoring should be turned on to "
                   "allow this feature (Setup -> General -> CD/DVD Monitor"));
    return gc;
}

HostSlider *DVDBookmarkDays()
{
    HostSlider *gs = new HostSlider("DVDBookmarkDays",5, 50, 5);
    gs->setLabel(QObject::tr("Remove DVD Bookmarks Older then (days)"));
    gs->setValue(10);
    gs->setHelpText((QObject::tr("Delete DVD Bookmarks that are older then the "
                                 "Number of days specified")));
    return gs;
}

HostCheckBox *EnableDVDBookmark()
{
    HostCheckBox *gc = new HostCheckBox("EnableDVDBookmark");
    gc->setLabel(QObject::tr("Enable DVD Bookmark Support"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable DVD Bookmark Support"));
    return gc;
}

HostCheckBox *DVDBookmarkPrompt()
{
    HostCheckBox *gc = new HostCheckBox("DVDBookmarkPrompt");
    gc->setLabel(QObject::tr("DVD Bookmark Prompt"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Display a prompt to choose whether "
                "to play the DVD from the beginning or from the bookmark"));
    return gc;
}

class DVDBookmarkSettings : public TriggeredConfigurationGroup
{
    public:
        DVDBookmarkSettings():
            TriggeredConfigurationGroup(false, false, true, true)
        {
            Setting *dvdbookmarkSettings = EnableDVDBookmark();
            addChild(dvdbookmarkSettings);
            setTrigger(dvdbookmarkSettings);

            ConfigurationGroup *settings =
                    new VerticalConfigurationGroup(false);
            settings->addChild(DVDBookmarkPrompt());
            settings->addChild(DVDBookmarkDays());
            addTarget("1", settings);
            addTarget("0", new VerticalConfigurationGroup(true));
        }
};

// Player Settings

HostLineEdit *PlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("mythdvd.DVDPlayerCommand");
    gc->setLabel(QObject::tr("DVD Player Command"));
//    gc->setValue("mplayer dvd:// -dvd-device %d -fs -zoom -vo xv");
    gc->setValue("Internal");
    gc->setHelpText(QObject::tr("This can be any command to launch a DVD "
                    " player (e.g. MPlayer, ogle, etc.). If present, %d will "
                    "be substituted for the DVD device (e.g. /dev/dvd)."));
    return gc;
}

HostLineEdit *VCDPlayerCommand()
{
    HostLineEdit *gc = new HostLineEdit("VCDPlayerCommand");
    gc->setLabel(QObject::tr("VCD Player Command"));
    gc->setValue("mplayer vcd:// -cdrom-device %d -fs -zoom -vo xv");
    gc->setHelpText(QObject::tr("This can be any command to launch a VCD "
                    "player (e.g. MPlayer, xine, etc.). If present, %d will "
                    "be substituted for the VCD device (e.g. /dev/cdrom)."));
    return gc;
}

// Ripper Settings

HostLineEdit *SetRipDirectory()
{
    HostLineEdit *gc = new HostLineEdit("DVDRipLocation");
    gc->setLabel(QObject::tr("Directory to hold temporary files"));
#ifdef Q_WS_MACX
    gc->setValue(QDir::homeDirPath() + "/Library/Application Support");
#else
    gc->setValue("/var/lib/mythdvd/temp");
#endif
    gc->setHelpText(QObject::tr("This directory must exist, and the user "
                    "running MythDVD needs to have write permission "
                    "to the directory."));
    return gc;
}

HostLineEdit *TitlePlayCommand()
{
    HostLineEdit *gc = new HostLineEdit("TitlePlayCommand");
    gc->setLabel(QObject::tr("Title Playing Command"));
    gc->setValue("mplayer dvd://%t -dvd-device %d -fs -zoom -vo xv -aid %a "
                 "-channels %c");
    gc->setHelpText(QObject::tr("This is a command used to preview a given "
                    "title on a DVD. If present %t will be set "
                    "to the title, %d for device, %a for audio "
                    "track, %c for audio channels."));
    return gc;
}

HostLineEdit *SubTitleCommand()
{
    HostLineEdit *gc = new HostLineEdit("SubTitleCommand");
    gc->setLabel(QObject::tr("Subtitle arguments:"));
    gc->setValue("-sid %s");
    gc->setHelpText(QObject::tr("If you choose any subtitles for ripping, this "
                    "command is added to the end of the Title Play "
                    "Command to allow previewing of subtitles. If  "
                    "present %s will be set to the subtitle track. "));
    return gc;
}

HostLineEdit *TranscodeCommand()
{
    HostLineEdit *gc = new HostLineEdit("TranscodeCommand");
    gc->setLabel(QObject::tr("Base transcode command"));
    gc->setValue("transcode");
    gc->setHelpText(QObject::tr("This is the base (without arguments) command "
                    "to run transcode on your system."));
    return gc;
}

HostSpinBox *MTDPortNumber()
{
    HostSpinBox *gc = new HostSpinBox("MTDPort", 1024, 65535, 1);
    gc->setLabel(QObject::tr("MTD port number"));
    gc->setValue(2442);
    gc->setHelpText(QObject::tr("The port number that should be used for "
                    "communicating with the MTD (Myth Transcoding "
                    "Daemon)"));
    return gc;
}

HostCheckBox *MTDLogFlag()
{
    HostCheckBox *gc = new HostCheckBox("MTDLogFlag");
    gc->setLabel(QObject::tr("MTD logs to terminal window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will log to the window it is started from. "
                    "Otherwise, it will write to a file called  "
                    "mtd.log in the top level ripping directory."));
    return gc;
}

HostCheckBox *MTDac3Flag()
{
    HostCheckBox *gc = new HostCheckBox("MTDac3Flag");
    gc->setLabel(QObject::tr("Transcode AC3 Audio"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, the MTD (Myth Transcoding Daemon) "
                    "will, by default, preserve AC3 (Dolby "
                    "Digital) audio in transcoded files. "));
    return gc;
}

HostCheckBox *MTDxvidFlag()
{
    HostCheckBox *gc = new HostCheckBox("MTDxvidFlag");
    gc->setLabel(QObject::tr("Use xvid rather than divx"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, mythdvd will use the (open, free) "
                    "xvid codec rather than divx whenever "
                    "possible."));
    return gc;
}

HostSpinBox *MTDNiceLevel()
{
    HostSpinBox *gc = new HostSpinBox("MTDNiceLevel", 0, 20, 1);
    gc->setLabel(QObject::tr("Nice level for MTD"));
    gc->setValue(20);
    gc->setHelpText(QObject::tr("This determines the priority of the Myth "
                    "Transcoding Daemon. Higher numbers mean "
                    "lower priority (more CPU to other tasks)."));
    return gc;
}

HostSpinBox *MTDConcurrentTranscodes()
{
    HostSpinBox *gc = new HostSpinBox("MTDConcurrentTranscodes", 1, 99, 1);
    gc->setLabel(QObject::tr("Simultaneous Transcode Jobs"));
    gc->setValue(1);
    gc->setHelpText(QObject::tr("This determines the number of simultaneous "
                    "transcode jobs. If set at 1 (the default), "
                    "there will only be one active job at a time."));
    return gc;
}

HostSpinBox *MTDRipSize()
{
    HostSpinBox *gc = new HostSpinBox("MTDRipSize", 0, 4096, 1);
    gc->setLabel(QObject::tr("Ripped video segments"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("If set to something other than 0, ripped "
                    "video titles will be broken up into files "
                    "of this size (in MB). Applies to both perfect "
                    "quality recordings and intermediate files "
                    "used for transcoding."));
    return gc;
}
} // namespace

VideoGeneralSettings::VideoGeneralSettings()
{
    const int pages = 5;

    VerticalConfigurationGroup *general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General Settings (%1/%2)")
                      .arg(1).arg(pages));
    general->addChild(VideoStartupDirectory());
    general->addChild(VideoArtworkDirectory());
    general->addChild(VideoDefaultParentalLevel());
    general->addChild(VideoAdminPassword());
    general->addChild(VideoAggressivePC());
    general->addChild(VideoDefaultView());
    addChild(general);

    VerticalConfigurationGroup *general2 =
            new VerticalConfigurationGroup(false);
    general2->setLabel(QObject::tr("General Settings (%1/%2)")
                       .arg(2).arg(pages));
    general2->addChild(VideoListUnknownFiletypes());
    general2->addChild(VideoBrowserNoDB());
    general2->addChild(VideoGalleryNoDB());
    general2->addChild(VideoTreeNoDB());
    general2->addChild(VideoTreeNoMetaData());
    general2->addChild(VideoNewBrowsable());
    general2->addChild(VideoSortIgnoresCase());
    general2->addChild(VideoDBFolderView());
    general2->addChild(VideoImageCacheSize());
    addChild(general2);

    VerticalConfigurationGroup *general3 =
            new VerticalConfigurationGroup(false);
    general3->setLabel(QObject::tr("General Settings (%1/%2)")
                       .arg(3).arg(pages));
    general3->addChild(SetDVDDevice());
    general3->addChild(SetVCDDevice());
    general3->addChild(SetOnInsertDVD());
    general3->addChild(new DVDBookmarkSettings());
    addChild(general3);

    VerticalConfigurationGroup *general4 =
            new VerticalConfigurationGroup(false);
    general4->setLabel(QObject::tr("General Settings (%1/%2)")
                       .arg(4).arg(pages));
    VerticalConfigurationGroup *vman =
            new VerticalConfigurationGroup(true, false);
    vman->setLabel(QObject::tr("Video Manager"));
    vman->addChild(SearchListingsCommand());
    vman->addChild(GetPostersCommand());
    vman->addChild(GetDataCommand());
    general4->addChild(vman);
    addChild(general4);

    VerticalConfigurationGroup *general5 =
            new VerticalConfigurationGroup(false);
    general5->setLabel(QObject::tr("General Settings (%1/%2)")
                       .arg(5).arg(pages));
    VerticalConfigurationGroup *vgal =
            new VerticalConfigurationGroup(true, false);
    vgal->setLabel(QObject::tr("Video Gallery"));
    vgal->addChild(VideoGalleryColumns());
    vgal->addChild(VideoGalleryRows());
    vgal->addChild(VideoGallerySubtitle());
    vgal->addChild(VideoGalleryAspectRatio());
    general5->addChild(vgal);
    addChild(general5);
}

VideoPlayerSettings::VideoPlayerSettings()
{
    VerticalConfigurationGroup *videoplayersettings =
            new VerticalConfigurationGroup(false);
    videoplayersettings->setLabel(QObject::tr("Player Settings"));
    videoplayersettings->addChild(VideoDefaultPlayer());
    videoplayersettings->addChild(PlayerCommand());
    videoplayersettings->addChild(VCDPlayerCommand());
    addChild(videoplayersettings);
}

DVDRipperSettings::DVDRipperSettings()
{
    VerticalConfigurationGroup *rippersettings =
            new VerticalConfigurationGroup(false);
    rippersettings->setLabel(QObject::tr("DVD Ripper Settings"));
    rippersettings->addChild(SetRipDirectory());
    rippersettings->addChild(TitlePlayCommand());
    rippersettings->addChild(SubTitleCommand());
    rippersettings->addChild(TranscodeCommand());
    addChild(rippersettings);

    VerticalConfigurationGroup *mtdsettings =
            new VerticalConfigurationGroup(false);
    mtdsettings->setLabel(QObject::tr("MTD Settings"));
    mtdsettings->addChild(MTDPortNumber());
    mtdsettings->addChild(MTDNiceLevel());
    mtdsettings->addChild(MTDConcurrentTranscodes());
    mtdsettings->addChild(MTDRipSize());
    mtdsettings->addChild(MTDLogFlag());
    mtdsettings->addChild(MTDac3Flag());
    mtdsettings->addChild(MTDxvidFlag());
    addChild(mtdsettings);
}
