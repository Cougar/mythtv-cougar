#include "mythcontext.h"
#include "dbsettings.h"

#include "globalsettings.h"
#include "scheduledrecording.h"
#include <qstylefactory.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

class GenericSlider: public SliderSetting, public GlobalSetting {
  public:
    GenericSlider(const QString &name, int min, int max, int step) :
        SliderSetting(min, max, step),
        GlobalSetting(name) { }
};

class GenericSpinBox: public SpinBoxSetting, public GlobalSetting {
  public:
    GenericSpinBox(const QString &name, int min, int max, int step, 
                   bool allow_single_step = false) :
        SpinBoxSetting(min, max, step, allow_single_step),
        GlobalSetting(name) { }
};

class GenericCheckBox: public CheckBoxSetting, public GlobalSetting {
  public:
    GenericCheckBox(const QString &name) :
        GlobalSetting(name) { }
};

class GenericComboBox: public ComboBoxSetting, public GlobalSetting {
  public:
    GenericComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(rw),
        GlobalSetting(name) { }
};

class GenericLineEdit: public LineEditSetting, public GlobalSetting {
  public:
    GenericLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(rw),
        GlobalSetting(name) { }
};

class BackendSlider: public SliderSetting, public BackendSetting {
  public:
    BackendSlider(const QString &name, int min, int max, int step) :
        SliderSetting(min, max, step),
        BackendSetting(name) { }
};

class BackendSpinBox: public SpinBoxSetting, public BackendSetting {
  public:
    BackendSpinBox(const QString &name, int min, int max, int step,
                   bool allow_single_step = false) :
        SpinBoxSetting(min, max, step, allow_single_step),
        BackendSetting(name) { }
};

class BackendCheckBox: public CheckBoxSetting, public BackendSetting {
  public:
    BackendCheckBox(const QString &name) :
        BackendSetting(name) { }
};

class BackendComboBox: public ComboBoxSetting, public BackendSetting {
  public:
    BackendComboBox(const QString &name, bool rw = false) :
        ComboBoxSetting(rw),
        BackendSetting(name) { }
};

class BackendLineEdit: public LineEditSetting, public BackendSetting {
  public:
    BackendLineEdit(const QString &name, bool rw = true) :
        LineEditSetting(rw),
        BackendSetting(name) { }
};

static GenericComboBox *AudioOutputDevice()
{
    GenericComboBox *gc = new GenericComboBox("AudioOutputDevice", true);

    gc->setLabel(QObject::tr("Audio output device"));
    QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);
    dev.setNameFilter("adsp*");
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        dev.setNameFilter("dsp*");
        gc->fillSelectionsFromDir(dev);
        dev.setNameFilter("adsp*");
        gc->fillSelectionsFromDir(dev);
    }

    return gc;
}

static GenericCheckBox *MythControlsVolume()
{
    GenericCheckBox *gc = new GenericCheckBox("MythControlsVolume");
    gc->setLabel(QObject::tr("Use internal volume controls"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("MythTV can control the PCM and master "
                    "mixer volume.  If you prefer to use an external mixer "
                    "program, then disable this option."));
    return gc;
}

static GenericComboBox *MixerDevice()
{
    GenericComboBox *gc = new GenericComboBox("MixerDevice", true);

    gc->setLabel(QObject::tr("Mixer Device"));
    QDir dev("/dev", "mixer*", QDir::Name, QDir::System);
    gc->fillSelectionsFromDir(dev);

    dev.setPath("/dev/sound");
    if (dev.exists())
    {
        gc->fillSelectionsFromDir(dev);
    }

    return gc;
}

static const char* MixerControlControls[] = { "PCM",
                                              "Master" };

static GenericComboBox *MixerControl()
{
    GenericComboBox *gc = new GenericComboBox("MixerControl", true);
    gc->setLabel(QObject::tr("Mixer Controls"));
    for (unsigned int i = 0; i < sizeof(MixerControlControls) / sizeof(char*); 
         ++i) 
    {
        gc->addSelection(QObject::tr(MixerControlControls[i]), 
                         MixerControlControls[i]);
    }

    gc->setHelpText(QObject::tr("Changing the volume adjusts the selected mixer."));
    return gc;
}

static GenericSlider *MixerVolume()
{
    GenericSlider *gs = new GenericSlider("MasterMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("Master Mixer Volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for the Master Mixer.  "
                    "This affects all sound created by the soundcard.  "
                    "Note: Do not set this too low."));
    return gs;
}

static GenericSlider *PCMVolume()
{
    GenericSlider *gs = new GenericSlider("PCMMixerVolume", 0, 100, 1);
    gs->setLabel(QObject::tr("PCM Mixer Volume"));
    gs->setValue(70);
    gs->setHelpText(QObject::tr("Initial volume for PCM output.  Using the "
                    "volume keys in MythTV will adjust this parameter."));
    return gs;
}

static GenericCheckBox *IndividualMuteControl()
{
    GenericCheckBox *gc = new GenericCheckBox("IndividualMuteControl");
    gc->setLabel(QObject::tr("Independent Muting of Left and Right Audio "
                 "Channels"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable muting of just the left or right "
                    "channel.  Useful if your broadcaster puts the "
                    "original language on one channel, and a dubbed "
                    "version of the program on the other one.  This "
                    "modifies the behavior of the Mute key."));
    return gc;
}

static GenericCheckBox *AC3PassThrough()
{
    GenericCheckBox *gc = new GenericCheckBox("AC3PassThru");
    gc->setLabel(QObject::tr("Enable AC3 to SPDIF passthrough"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable sending AC3 audio directly to your "
                    "sound card's SPDIF output, on sources which contain "
                    "AC3 soundtracks (usually digital TV).  Requires that "
                    "the audio output device be set to something suitable."));
    return gc;
}

static GenericCheckBox *Deinterlace()
{
    GenericCheckBox *gc = new GenericCheckBox("Deinterlace");
    gc->setLabel(QObject::tr("Deinterlace playback"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Make the video look normal on a progressive "
                    "display (i.e. monitor)."));
    return gc;
}

static GenericComboBox *DeinterlaceFilter()
{
    GenericComboBox *gc = new GenericComboBox("DeinterlaceFilter", false);
    gc->setLabel(QObject::tr("Algorithm"));
    gc->addSelection(QObject::tr("Linear blend"), "linearblend");
    gc->addSelection(QObject::tr("Kernel (less motion blur)"), "kerneldeint");
    gc->addSelection(QObject::tr("Bob (2x framerate)"), "bobdeint");
    gc->addSelection(QObject::tr("One field"), "onefield");
    gc->setHelpText(QObject::tr("Deinterlace algorithm. "
                                "'Kernel' require SSE. 'Bob' requires "
                                "Xv or XvMC video out."));
    return gc;
}

class DeinterlaceSettings: public HorizontalConfigurationGroup,
                           public TriggeredConfigurationGroup {
public:
    DeinterlaceSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Deinterlace settings"));
        setUseLabel(false);
        Setting *deinterlace = Deinterlace();
        addChild(deinterlace);
        setTrigger(deinterlace);

        Setting *filter = DeinterlaceFilter();
        addTarget("1", filter);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};

static GenericLineEdit *CustomFilters()
{
    GenericLineEdit *ge = new GenericLineEdit("CustomFilters");
    ge->setLabel(QObject::tr("Custom Filters"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("Advanced Filter configuration, format:\n"
                    "[[<filter>=<options>,]...]"));
    return ge;
}

static GenericCheckBox *DecodeExtraAudio()
{
    GenericCheckBox *gc = new GenericCheckBox("DecodeExtraAudio");
    gc->setLabel(QObject::tr("Extra audio buffering"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Enable this setting if MythTV is playing "
                    "\"crackly\" audio and you are using hardware encoding. "
                    "This setting will have no effect "
                    "on MPEG-4 or RTJPEG video. MythTV will keep extra "
                    "audio data in its internal buffers to workaround "
                    "this bug."));
    return gc;
}

static GenericComboBox *PIPLocation()
{
    GenericComboBox *gc = new GenericComboBox("PIPLocation");
    gc->setLabel(QObject::tr("PIP Video Location"));
    gc->addSelection(QObject::tr("Top Left"), "0");
    gc->addSelection(QObject::tr("Bottom Left"), "1");
    gc->addSelection(QObject::tr("Top Right"), "2");
    gc->addSelection(QObject::tr("Bottom Right"), "3");
    gc->setHelpText(QObject::tr("Location of PIP Video window."));
    return gc;
}

static BackendLineEdit *AllRecGroupPassword()
{
    BackendLineEdit *be = new BackendLineEdit("AllRecGroupPassword");
    be->setLabel(QObject::tr("Password required to view all recordings"));
    be->setValue("");
    be->setHelpText(QObject::tr("If given, a password must be entered to "
                    "view the complete list of all recordings."));
    return be;
}

static GenericComboBox *DisplayRecGroup()
{
    GenericComboBox *gc = new GenericComboBox("DisplayRecGroup");
    gc->setLabel(QObject::tr("Default Recording Group to display"));

    gc->addSelection(QObject::tr("All Programs"), QString("All Programs"));
    gc->addSelection(QObject::tr("Default"), QString("Default"));

    QSqlDatabase *db = QSqlDatabase::database();
    QString thequery = QString("SELECT DISTINCT recgroup from recorded");
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next())
            if (query.value(0).toString() != "Default")
            {
                QString recgroup = QString::fromUtf8(query.value(0).toString());
                gc->addSelection(recgroup, recgroup);
            }

    gc->setHelpText(QObject::tr("Default Recording Group to display "
                    "on the View Recordings screen."));
    return gc;
}

static GenericCheckBox *RememberRecGroup()
{
    GenericCheckBox *gc = new GenericCheckBox("RememberRecGroup");
    gc->setLabel(QObject::tr("Save current Recording Group view when changed"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Remember the last selected Recording "
                    "Group instead of displaying the Default group "
                    "whenever you enter the playback screen."));

    return gc;
}

static GenericCheckBox *UseCategoriesAsRecGroups()
{
    GenericCheckBox *gc = new GenericCheckBox("UseCategoriesAsRecGroups");
    gc->setLabel(QObject::tr("Use program categories as display groups"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Add the list of program categories to the "
                    "list of Recording Groups used for display.  Only programs "
                    "in non-password protected groups will be listed."));

    return gc;
}

static GenericCheckBox *UseGroupNameAsAllPrograms()
{
    GenericCheckBox *gc = new GenericCheckBox("DispRecGroupAsAllProg");
    gc->setLabel(QObject::tr("Show group name instead of \"All Programs\""));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use the name of the display group currently "
                    "being shown in place of the term \"All Programs\" in the "
                    "playback screen."));
    return gc;
}

static GenericCheckBox *PBBStartInTitle()
{
    GenericCheckBox *gc = new GenericCheckBox("PlaybackBoxStartInTitle");
    gc->setLabel(QObject::tr("Start in Title section"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the selector highlight will "
                    "start on the Program titles window, otherwise the "
                    "selector will default to the recordings."));
    return gc;
}

static GenericCheckBox *PBBShowGroupSummary()
{
    GenericCheckBox *gc = new GenericCheckBox("ShowGroupInfo");
    gc->setLabel(QObject::tr("Show group summary"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("While selecting a group, show a group "
                    "summary instead of showing info about the first episode "
                    "in that group."));
    return gc;
}

static GenericSpinBox *JumpAmount()
{
    GenericSpinBox *gs = new GenericSpinBox("JumpAmount", 1, 30, 5, true);
    gs->setLabel(QObject::tr("Jump amount (in minutes)"));
    gs->setValue(10);
    gs->setHelpText(QObject::tr("How many minutes to jump forward or backward "
                    "when the jump keys are pressed."));
    return gs;
}

static GenericSpinBox *FastForwardAmount()
{
    GenericSpinBox *gs = new GenericSpinBox("FastForwardAmount", 1, 600, 5, 
                                            true);
    gs->setLabel(QObject::tr("Fast forward amount (in seconds)"));
    gs->setValue(30);
    gs->setHelpText(QObject::tr("How many seconds to skip forward on a fast "
                    "forward."));
    return gs;
}

static GenericSpinBox *RewindAmount()
{
    GenericSpinBox *gs = new GenericSpinBox("RewindAmount", 1, 600, 5, true);
    gs->setLabel(QObject::tr("Rewind amount (in seconds)"));
    gs->setValue(5);
    gs->setHelpText(QObject::tr("How many seconds to skip backward on a "
                    "rewind."));
    return gs;
}

static GenericCheckBox *SmartForward()
{
    GenericCheckBox *gc = new GenericCheckBox("SmartForward");
    gc->setLabel(QObject::tr("Smart Fast Forwarding"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, then immediately after "
                    "rewinding, only skip forward the same amount as "
                    "skipping backwards."));
    return gc;
}

static GenericCheckBox *ExactSeeking()
{
    GenericCheckBox *gc = new GenericCheckBox("ExactSeeking");
    gc->setLabel(QObject::tr("Seek to exact frame"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, seeking is frame exact, but "
                    "slower."));
    return gc;
}

static BackendComboBox *CommercialSkipMethod()
{
    BackendComboBox *bc = new BackendComboBox("CommercialSkipMethod");
    bc->setLabel(QObject::tr("Commercial Skip Method"));
    bc->addSelection(QObject::tr("Blank Frame Detection (default)"), "1");
    bc->addSelection(QObject::tr("Blank Frame + Scene Change Detection"), "3");
    bc->addSelection(QObject::tr("Scene Change Detection"), "2");
    bc->addSelection(QObject::tr("Logo Detection"), "4");
//    bc->addSelection(QObject::tr("All Detection Methods"), "255");
    bc->setHelpText(QObject::tr("This determines the method used by MythTV to "
                    "detect when commercials start and end.  You must have "
                    "'Automatically Flag Commercials' enabled to use "
                    "anything other than 'Blank Frame'." ));
    return bc;
}

static GenericComboBox *AutoCommercialSkip()
{
    GenericComboBox *gc = new GenericComboBox("AutoCommercialSkip");
    gc->setLabel(QObject::tr("Automatically Skip Commercials"));
    gc->addSelection(QObject::tr("Off"), "0");
    gc->addSelection(QObject::tr("Notify, but do not skip"), "2");
    gc->addSelection(QObject::tr("Automatically Skip"), "1");
    gc->setHelpText(QObject::tr("Automatically skip commercial breaks that "
                    "have been flagged during Automatic Commercial Flagging "
                    "or by the mythcommflag program, or just notify that a "
                    "commercial has been detected."));
    return gc;
}

static GenericCheckBox *TryUnflaggedSkip()
{
    GenericCheckBox *gc = new GenericCheckBox("TryUnflaggedSkip");
    gc->setLabel(QObject::tr("Skip Unflagged Commercials"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Try to skip commercial breaks even if they "
                    "have not been flagged.  This does not always work well "
                    "and can disrupt playback if commercial breaks aren't "
                    "detected properly."));
    return gc;
}

static BackendCheckBox *AutoCommercialFlag()
{
    BackendCheckBox *bc = new BackendCheckBox("AutoCommercialFlag");
    bc->setLabel(QObject::tr("Automatically Flag Commercials"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("Automatically flag commercials after a "
                    "recording completes."));
    return bc;
}

static BackendCheckBox *AggressiveCommDetect()
{
    BackendCheckBox *bc = new BackendCheckBox("AggressiveCommDetect");
    bc->setLabel(QObject::tr("Strict Commercial Detection"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("Enable stricter Commercial Detection code.  "
                    "Disable if some commercials are not being detected."));
    return bc;
}

static BackendCheckBox *CommSkipAllBlanks()
{
    BackendCheckBox *bc = new BackendCheckBox("CommSkipAllBlanks");
    bc->setLabel(QObject::tr("Skip blank frames after commercials"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("When using Blank Frame Detection and "
                    "Auto-Flagging, include blank frames following commercial "
                    "breaks as part of the commercial break."));
    return bc;
}

static GenericSpinBox *CommRewindAmount()
{
    GenericSpinBox *gs = new GenericSpinBox("CommRewindAmount", 0, 10, 1);
    gs->setLabel(QObject::tr("Commercial Skip Auto-Rewind Amount"));
    gs->setHelpText(QObject::tr("If set, Myth will automatically rewind "
                    "this many seconds after performing a commercial skip."));
    gs->setValue(0);
    return gs;
}

static GenericSpinBox *CommNotifyAmount()
{
    GenericSpinBox *gs = new GenericSpinBox("CommNotifyAmount", 0, 10, 1);
    gs->setLabel(QObject::tr("Commercial Skip Notify Amount"));
    gs->setHelpText(QObject::tr("If set, MythTV will act like a commercial "
                    "begins this many seconds early.  This can be useful "
                    "when commercial notification is used in place of "
                    "automatic skipping."));
    gs->setValue(0);
    return gs;
}

static BackendSpinBox *AutoExpireDiskThreshold()
{
    BackendSpinBox *bs = new BackendSpinBox("AutoExpireDiskThreshold", 
                                            0, 200, 1);
    bs->setLabel(QObject::tr("Auto Expire Free Disk Space Threshold "
                 "(in Gigabytes)"));
    bs->setHelpText(QObject::tr("Trigger AutoExpire when available disk "
                    "space is below this value.  Disable AutoExpire by "
                    "setting to 0."));
    bs->setValue(0);
    return bs;
};

static BackendSpinBox *AutoExpireFrequency()
{
    BackendSpinBox *bs = new BackendSpinBox("AutoExpireFrequency", 1, 60, 10, 
                                            true);
    bs->setLabel(QObject::tr("Auto Expire Frequency (in minutes)"));
    bs->setHelpText(QObject::tr("How often the AutoExpire process "
                    "checks for free disk space."));
    bs->setValue(10);
    return bs;
}

static BackendComboBox *AutoExpireMethod()
{
    BackendComboBox *bc = new BackendComboBox("AutoExpireMethod");
    bc->setLabel(QObject::tr("Auto Expire Method"));
    bc->addSelection(QObject::tr("Oldest Show First"), "1");
    bc->setHelpText(QObject::tr("Method used to determine which recorded "
                    "shows to AutoExpire first."));
    return bc;
}

static BackendCheckBox *AutoExpireDefault()
{
    BackendCheckBox *bc = new BackendCheckBox("AutoExpireDefault");
    bc->setLabel(QObject::tr("Auto-Expire Default"));
    bc->setValue(true);
    bc->setHelpText(QObject::tr("When enabled, any newly recorded programs "
                    "will be marked as eligible for Auto-Expiration. "
                    "Existing recordings will keep their current value."));
    return bc;
}

static BackendSpinBox *MinRecordDiskThreshold()
{
    BackendSpinBox *bs = new BackendSpinBox("MinRecordDiskThreshold",
                                            0, 1000000, 100);
    bs->setLabel(QObject::tr("New Recording Free Disk Space Threshold "
                 "(in Megabytes)"));
    bs->setHelpText(QObject::tr("MythTV will stop scheduling new recordings on "
                    "a backend when its free disk space falls below this "
                    "value."));
    bs->setValue(300);
    return bs;
}

static BackendCheckBox *RerecordAutoExpired()
{
    BackendCheckBox *bc = new BackendCheckBox("RerecordAutoExpired");
    bc->setLabel(QObject::tr("Re-record programs which have been "
                             "automatically expired"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("If set, programs that are automatically "
                                "deleted (expired) will be allowed to "
                                "re-recorded if they are shown again."));
    return bc;
}

static BackendSpinBox *RecordPreRoll()
{
    BackendSpinBox *bs = new BackendSpinBox("RecordPreRoll", 0, 600, 60, true);
    bs->setLabel(QObject::tr("Time to record before start of show "
                 "(in seconds)"));
    bs->setHelpText(QObject::tr("This global setting is ignored in case of "
                    "conflicts with other scheduled programs."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *RecordOverTime()
{
    BackendSpinBox *bs = new BackendSpinBox("RecordOverTime", 0, 1800, 60, true);
    bs->setLabel(QObject::tr("Time to record past end of show (in seconds)"));
    bs->setValue(0);
    bs->setHelpText(QObject::tr("This global setting is ignored in case of "
                    "conflicts with other scheduled programs."));
    return bs;
}

static GenericCheckBox *PlayBoxOrdering()
{
    GenericCheckBox *gc = new GenericCheckBox("PlayBoxOrdering");
    gc->setLabel(QObject::tr("List Newest Recording First"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, the most recent recording "
                    "will be listed first in the 'Watch Recordings' "
                    "screen, otherwise the oldest recording will be "
                    "listed first."));
    return gc;
}

static GenericCheckBox *StickyKeys()
{
    GenericCheckBox *gc = new GenericCheckBox("StickyKeys");
    gc->setLabel(QObject::tr("Sticky keys"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, fast forward and rewind "
                    "continue after the key is released.  Pressing the key "
                    "again increases the fast forward or rewind speed.  The "
                    "alternate fast forward and rewind keys always behave in "
                    "this way."));
    return gc;
}

static GenericCheckBox *FFRewRepos()
{
    GenericCheckBox *gc = new GenericCheckBox("FFRewRepos");
    gc->setLabel(QObject::tr("Reposition after fast forward/rewind"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When exiting sticky keys fast forward/rewind "
                    "mode, reposition before resuming normal playback. This "
                    "compensates for the reaction time between seeing "
                    "where to resume playback and actually exiting seeking."));
    return gc;
}

static GenericCheckBox *FFRewReverse()
{
    GenericCheckBox *gc = new GenericCheckBox("FFRewReverse");
    gc->setLabel(QObject::tr("Reverse direction in fast forward/rewind"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, pressing the sticky rewind key "
                    "in fast forward mode switches to rewind mode, and "
                    "vice versa.  If disabled, it will decrease the "
                    "current speed or switch to play mode if "
                    "the speed can't be decreased further."));
    return gc; 
}

static GenericSpinBox *OSDDisplayTime()
{
    GenericSpinBox *gs = new GenericSpinBox("OSDDisplayTime", 0, 30, 1);
    gs->setLabel(QObject::tr("Number of seconds for OSD information"));
    gs->setValue(3);
    gs->setHelpText(QObject::tr("How long the program information remains on "
                    "the On Screen Display after a channel change."));
    return gs;
}

static GenericComboBox *MenuTheme()
{
    GenericComboBox *gc = new GenericComboBox("MenuTheme");
    gc->setLabel(QObject::tr("Menu theme"));

    QDir themes(gContext->GetThemesParentDir());
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);
    gc->addSelection(QObject::tr("Default"));
    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return gc;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo xml(theme->absFilePath() + "/mainmenu.xml");

        if (theme->fileName()[0] != '.' && xml.exists())
            gc->addSelection(theme->fileName());
    }

    return gc;
}

static GenericComboBox *OSDTheme()
{
    GenericComboBox *gc = new GenericComboBox("OSDTheme");
    gc->setLabel(QObject::tr("OSD theme"));

    QDir themes(gContext->GetThemesParentDir());
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return gc;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo xml(theme->absFilePath() + "/osd.xml");

        if (theme->fileName()[0] != '.' && xml.exists())
            gc->addSelection(theme->fileName());
    }

    return gc;
}

static GenericComboBox *OSDFont()
{
    GenericComboBox *gc = new GenericComboBox("OSDFont");
    gc->setLabel(QObject::tr("OSD font"));
    QDir ttf(gContext->GetFontsDir(), gContext->GetFontsNameFilter());
    gc->fillSelectionsFromDir(ttf, false);

    return gc;
}

static GenericComboBox *OSDCCFont()
{
    GenericComboBox *gc = new GenericComboBox("OSDCCFont");
    gc->setLabel(QObject::tr("Closed Caption font"));
    QDir ttf(gContext->GetFontsDir(), gContext->GetFontsNameFilter());
    gc->fillSelectionsFromDir(ttf, false);

    return gc;
}

static GenericComboBox *OSDThemeFontSizeType()
{
    GenericComboBox *gc = new GenericComboBox("OSDThemeFontSizeType");
    gc->setLabel(QObject::tr("Font size"));
    gc->addSelection(QObject::tr("default"), "default");
    gc->addSelection(QObject::tr("small"), "small");
    gc->addSelection(QObject::tr("big"), "big");
    gc->setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    return gc;
}

static GenericComboBox *ChannelOrdering()
{
    GenericComboBox *gc = new GenericComboBox("ChannelOrdering");
    gc->setLabel(QObject::tr("Channel ordering"));
    gc->addSelection(QObject::tr("channel number (numeric)"), "channum + 0");
    gc->addSelection(QObject::tr("channel number (alpha)"), "channum");
    gc->addSelection(QObject::tr("database order"), "chanid");
    gc->addSelection(QObject::tr("channel name"), "callsign");
    return gc;
}

static GenericSpinBox *VertScanPercentage()
{
    GenericSpinBox *gs = new GenericSpinBox("VertScanPercentage", -100, 100, 1);
    gs->setLabel(QObject::tr("Vertical over/underscan percentage"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen vertically."));
    return gs;
}

static GenericSpinBox *HorizScanPercentage()
{
    GenericSpinBox *gs = new GenericSpinBox("HorizScanPercentage", -100, 100, 1);
    gs->setLabel(QObject::tr("Horizontal over/underscan percentage"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this if the image does not fill your "
                    "screen horizontally."));
    return gs;
};

static GenericSpinBox *XScanDisplacement()
{
    GenericSpinBox *gs = new GenericSpinBox("XScanDisplacement", -50, 50, 1);
    gs->setLabel(QObject::tr("Scan displacement (X)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this to move the image horizontally."));
    return gs; 
}

static GenericSpinBox *YScanDisplacement()
{
    GenericSpinBox *gs = new GenericSpinBox("YScanDisplacement", -50, 50, 1);
    gs->setLabel(QObject::tr("Scan displacement (Y)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Adjust this to move the image vertically."));
    return gs;
};

static GenericCheckBox *UseVideoTimebase()
{
    GenericCheckBox *gc = new GenericCheckBox("UseVideoTimebase");
    gc->setLabel(QObject::tr("Use video as timebase"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use the video as the timebase and warp "
                    "the audio to keep it in sync. (Experimental)"));
    return gc;
}

static GenericCheckBox *CCBackground()
{
    GenericCheckBox *gc = new GenericCheckBox("CCBackground");
    gc->setLabel(QObject::tr("Black background for Closed Captioning"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, captions will be displayed "
                    "over a black space for maximum contrast. Otherwise, "
                    "captions will use outlined text over the picture."));
    return gc;
}

static GenericCheckBox *DefaultCCMode()
{
    GenericCheckBox *gc = new GenericCheckBox("DefaultCCMode");
    gc->setLabel(QObject::tr("Always display Closed Captioning"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, captions will be displayed "
                    "when playing back recordings or watching "
                    "live TV.  Closed Captioning can be turned on or off "
                    "by pressing \"T\" during playback."));
    return gc;
}

static GenericCheckBox *PersistentBrowseMode()
{
    GenericCheckBox *gc = new GenericCheckBox("PersistentBrowseMode");
    gc->setLabel(QObject::tr("Always use Browse mode when changing channels "
                 "in LiveTV"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, Browse mode will "
                    "automatically be activated whenever you use Channel "
                    "UP/DOWN while watching Live TV."));
    return gc;
}

static GenericCheckBox *AggressiveBuffer()
{
    GenericCheckBox *gc = new GenericCheckBox("AggressiveSoundcardBuffer");
    gc->setLabel(QObject::tr("Aggressive Soundcard Buffering"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will pretend to have "
                   "a smaller soundcard buffer than is really present.  This "
                   "may speed up seeking, but can also cause playback "
                   "problems."));
    return gc;
}

static GenericCheckBox *ClearSavedPosition()
{
    GenericCheckBox *gc = new GenericCheckBox("ClearSavedPosition");
    gc->setLabel(QObject::tr("Clear Saved Position on playback"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Automatically clear saved position on a "
                    "recording when the recording is played back.  If "
                    "disabled, you can mark the beginning with rewind "
                    "then save position."));
    return gc;
}

static GenericCheckBox *AltClearSavedPosition()
{
    GenericCheckBox *gc = new GenericCheckBox("AltClearSavedPosition");
    gc->setLabel(QObject::tr("Alternate Clear Saved Position"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("During playback the Select key "
                    "(Enter or Space) will alternate between \"Position "
                    "Saved\" and \"Position Cleared\". If disabled, the "
                    "Select key will save the current position for each "
                    "keypress."));
    return gc;
}

static GenericCheckBox *UsePicControls()
{
    GenericCheckBox *gc = new GenericCheckBox("UseOutputPictureControls");
    gc->setLabel(QObject::tr("Use Xv picture controls"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, Xv picture controls (brightness, "
                    "contrast, etc.) are used during playback. These are "
                    "independent of the Video4Linux controls used for "
                    "recording. The Xv controls may not work properly on "
                    "some systems."));
    return gc;
}

static GenericCheckBox *CCWarnSetting()
{
    GenericCheckBox *gc = new GenericCheckBox("CCBufferWarnings");
    gc->setLabel(QObject::tr("Enable channel change buffer warnings"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, MythTV will warn you whenever you "
                   "change the channel but are not caught up to live TV."));
    return gc;
}

static GenericLineEdit *UDPNotifyPort()
{
    GenericLineEdit *ge = new GenericLineEdit("UDPNotifyPort");
    ge->setLabel(QObject::tr("UDP Notify Port"));
    ge->setValue("6948");
    ge->setHelpText(QObject::tr("During playback, MythTV will listen for "
                    "connections from the \"mythtvosd\" or \"mythudprelay\" "
                    "programs on this port.  See the README in "
                    "contrib/mythnotify/ for additional information."));
    return ge;
}

static GenericComboBox *PlaybackExitPrompt()
{
    GenericComboBox *gc = new GenericComboBox("PlaybackExitPrompt");
    gc->setLabel(QObject::tr("Action on playback exit"));
    gc->addSelection(QObject::tr("Just exit"), "0");
    gc->addSelection(QObject::tr("Save position and exit"), "2");
    gc->addSelection(QObject::tr("Always prompt"), "1");
    gc->setHelpText(QObject::tr("If set to prompt, a menu will be displayed "
                    "when you exit playback mode.  The options available will "
                    "allow you to save your position, delete the "
                    "recording, or continue watching."));
    return gc;
}

static GenericCheckBox *EndOfRecordingExitPrompt()
{
    GenericCheckBox *gc = new GenericCheckBox("EndOfRecordingExitPrompt");
    gc->setLabel(QObject::tr("Prompt at end of recording"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, a "
                    "menu will be displayed allowing you to delete the "
                    "recording when it has finished playing."));
    return gc;
}

static GenericCheckBox *GeneratePreviewPixmaps()
{
    GenericCheckBox *gc = new GenericCheckBox("GeneratePreviewPixmaps");
    gc->setLabel(QObject::tr("Generate thumbnail preview images of "
                 "recordings"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, a static image of the recording will "
                    "be displayed on the \"Watch a Recording\" menu."));
    return gc;
}

static BackendSpinBox *PreviewPixmapOffset()
{
    BackendSpinBox *bs = new BackendSpinBox("PreviewPixmapOffset", 0, 600, 1);
    bs->setLabel(QObject::tr("Time offset for thumbnail preview images"));
    bs->setHelpText(QObject::tr("MythTV will make a thumbnail image this "
                    "many seconds from the beginning of the recording. "
                    "Useful to skip over advertisements."));
    bs->setValue(64);
    return bs;
}

static GenericCheckBox *PlaybackPreview()
{
    GenericCheckBox *gc = new GenericCheckBox("PlaybackPreview");
    gc->setLabel(QObject::tr("Display live preview of recordings"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, a preview of the recording "
                    "will play in a small window on the \"Watch a "
                    "Recording\" menu."));
    return gc;
}

static GenericCheckBox *PlayBoxTransparency()
{
    GenericCheckBox *gc = new GenericCheckBox("PlayBoxTransparency");
    gc->setLabel(QObject::tr("Use Transparent Boxes"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the Watch Recording and Delete "
                    "Recording screens will use transparency. Disable "
                    "if selecting the recordings is slow due to high "
                    "CPU usage."));
    return gc;
}

static GenericComboBox *PlayBoxShading()
{
    GenericComboBox *gc = new GenericComboBox("PlayBoxShading");
    gc->setLabel(QObject::tr("Popup Background Shading Method"));
    gc->addSelection(QObject::tr("Fill"), "0");
    gc->addSelection(QObject::tr("Image"), "1");
    gc->addSelection(QObject::tr("None"), "2");

    gc->setHelpText(QObject::tr("\"Fill\" is the quickest shading method. "
                    "\"Image\" is somewhat slow, but has a higher visual "
                    "quality. No shading will be the fastest."));
    return gc;
}

static GenericComboBox *AllowQuitShutdown()
{
    GenericComboBox *gc = new GenericComboBox("AllowQuitShutdown");
    gc->setLabel(QObject::tr("System Exit key"));
    gc->addSelection(QObject::tr("ESC"), "4");
    gc->addSelection(QObject::tr("No exit key"), "0");
    gc->addSelection(QObject::tr("Control-ESC"), "1");
    gc->addSelection(QObject::tr("Meta-ESC"), "2");
    gc->addSelection(QObject::tr("Alt-ESC"), "3");
    gc->setHelpText(QObject::tr("MythTV is designed to run continuously. If "
                    "you wish, you may use the ESC key or the ESC key + a "
                    "modifier to exit MythTV. Do not choose a key combination "
                    "that will be intercepted by your window manager."));
    return gc;
}

static GenericCheckBox *NoPromptOnExit()
{
    GenericCheckBox *gc = new GenericCheckBox("NoPromptOnExit");
    gc->setLabel(QObject::tr("Confirm Exit"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("When enabled, MythTV will prompt "
                    "for confirmation when you press the System Exit "
                    "key."));
    return gc;
}

static GenericLineEdit *HaltCommand()
{
    GenericLineEdit *ge = new GenericLineEdit("HaltCommand");
    ge->setLabel(QObject::tr("Halt command"));
    ge->setValue("halt");
    ge->setHelpText(QObject::tr("If you have configured an exit key using the "
                    "System Shutdown option, you will be given the opportunity "
                    "to exit MythTV or halt the system completely. "
                    "Another possibility for this field is \"poweroff\""));
    return ge;
}

static GenericLineEdit *LircKeyPressedApp()
{
    GenericLineEdit *ge = new GenericLineEdit("LircKeyPressedApp");
    ge->setLabel(QObject::tr("Keypress Application"));
    ge->setValue("");
    ge->setHelpText(QObject::tr("External application or script to run when "
                    "a keypress is received by LIRC."));
    return ge;
}

static GenericCheckBox *UseArrowAccels()
{
    GenericCheckBox *gc = new GenericCheckBox("UseArrowAccels");
    gc->setLabel(QObject::tr("Use Arrow Key Accelerators"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, Arrow key accelerators will "
                    "be used, with LEFT performing an exit action and "
                    "RIGHT selecting the current item."));
    return gc;
}

static GenericLineEdit *SetupPinCode()
{
    GenericLineEdit *ge = new GenericLineEdit("SetupPinCode");
    ge->setLabel(QObject::tr("Setup Pin Code"));
    ge->setHelpText(QObject::tr("This PIN is used to control access to the "
                    "setup menus. If you want to use this feature, then "
                    "setting the value to all numbers will make your life "
                    "much easier.  Set it to blank to disable."));
    return ge;
}

static GenericCheckBox *SetupPinCodeRequired()
{
    GenericCheckBox *gc = new GenericCheckBox("SetupPinCodeRequired");
    gc->setLabel(QObject::tr("Require Setup PIN"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If set, you will not be able to return "
                    "to this screen and reset the Setup PIN without first "
                    "entering the current PIN."));
    return gc;
}

static GenericSpinBox *XineramaScreen()
{
    GenericSpinBox *gs = new GenericSpinBox("XineramaScreen", 0, 8, 1);
    gs->setLabel(QObject::tr("Xinerama screen"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("If using Xinerama, run only on the specified "
                    "screen."));
    return gs;
}

static GenericComboBox *AspectOverride()
{
    GenericComboBox *gc = new GenericComboBox("AspectOverride");
    gc->setLabel(QObject::tr("Aspect Override"));
    gc->addSelection(QObject::tr("Off"), "0");
    gc->addSelection(QObject::tr("16/9 Anamorphic"), "1");
    gc->addSelection(QObject::tr("4/3 Normal"), "2");
    gc->addSelection(QObject::tr("16/9 Zoom"), "3");
    gc->addSelection(QObject::tr("4/3 Zoom"), "3");
    gc->setHelpText(QObject::tr("This will override any aspect ratio in the "
                    "recorded stream, the same as pressing the W Key "
                    "during playback."));
    return gc;
}

// Theme settings

static GenericSpinBox *GuiWidth()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiWidth", 0, 1920, 8, true);
    gs->setLabel(QObject::tr("GUI width (px)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The width of the GUI.  Do not make the GUI "
                    "wider than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));
    return gs;
}

static GenericSpinBox *GuiHeight()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiHeight", 0, 1600, 8, true);
    gs->setLabel(QObject::tr("GUI height (px)"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The height of the GUI.  Do not make the GUI "
                    "taller than your actual screen resolution.  Set to 0 to "
                    "automatically scale to fullscreen."));
    return gs;
}

static GenericSpinBox *GuiOffsetX()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiOffsetX", -1600, 1600, 8, true);
    gs->setLabel(QObject::tr("GUI X offset"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The horizontal offset the GUI will be "
                    "displayed at.  May only work if run in a window."));
    return gs;
}

static GenericSpinBox *GuiOffsetY()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiOffsetY", -1600, 1600, 8, true);
    gs->setLabel(QObject::tr("GUI Y offset"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("The vertical offset the GUI will be "
                    "displayed at."));
    return gs;
}

static GenericSpinBox *DisplaySizeWidth()
{
    GenericSpinBox *gs = new GenericSpinBox("DisplaySizeWidth", 0, 10000, 1);
    gs->setLabel(QObject::tr("Display Size - Width"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal size of the monitor or TV, is used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));
    return gs;
}

static GenericSpinBox *DisplaySizeHeight()
{
    GenericSpinBox *gs = new GenericSpinBox("DisplaySizeHeight", 0, 10000, 1);
    gs->setLabel(QObject::tr("Display Size - Height"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical size of the monitor or TV, is used "
                    "to calculate the actual aspect ratio of the display. This "
                    "will override the DisplaySize from the system."));
    return gs;
}

static GenericCheckBox *GuiSizeForTV()
{
    GenericCheckBox *gc = new GenericCheckBox("GuiSizeForTV");
    gc->setLabel(QObject::tr("Use GUI size for TV playback"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, use the above size for TV, "
                    "otherwise use full screen."));
    return gc;
}

#if defined(USING_XRANDR) || defined(CONFIG_DARWIN)
static GenericCheckBox *UseVideoModes()
{
    GenericCheckBox *gc = new GenericCheckBox("UseVideoModes");
    gc->setLabel(QObject::tr("Separate video modes for GUI and TV playback"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Switch X Window video modes for TV. "
                                "Requires \"xrandr\" support."));
    return gc;
}

static GenericSpinBox *GuiVidModeWidth()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiVidModeWidth", 0, 1920, 8, true);
    gs->setLabelAboveWidget(true);
    gs->setLabel(QObject::tr("Width"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal resolution for GUI video mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}

static GenericSpinBox *GuiVidModeHeight()
{
    GenericSpinBox *gs = new GenericSpinBox("GuiVidModeHeight", 0, 1200, 4, true);
    gs->setLabelAboveWidget(true);
    gs->setLabel(QObject::tr("Height"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical resolution for GUI video mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}

static GenericSpinBox *VidModeWidth(int idx)
{
    GenericSpinBox *gs = new GenericSpinBox(QString("VidModeWidth%1").arg(idx),
                                            0, 1920, 8, true);
    gs->setLabel(QObject::tr("X"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal resolution of video mode "
                                "which needs a special output resolution."));
    return gs;
}

static GenericSpinBox *VidModeHeight(int idx)
{
    GenericSpinBox *gs = new GenericSpinBox(QString("VidModeHeight%1").arg(idx),
                                            0, 1200, 4, true);
    gs->setLabel(QObject::tr("Y"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical resolution of video mode "
                                "which needs a special output resolution."));
    return gs;
}

static GenericSpinBox *TVVidModeWidth()
{
    GenericSpinBox *gs = new GenericSpinBox("TVVidModeWidth", 0, 1920, 8, true);
    gs->setLabelAboveWidget(true);
    gs->setLabel(QObject::tr("Width"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal resolution for playback video "
                                "mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}

static GenericSpinBox *TVVidModeWidth(int idx)
{
    GenericSpinBox *gs = new GenericSpinBox(QString("TVVidModeWidth%1").arg(idx), 
                                            0, 1920, 8, true);
    gs->setLabel(QObject::tr("X"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Horizontal resolution for playback video "
                                "mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}


static GenericSpinBox *TVVidModeHeight()
{
    GenericSpinBox *gs = new GenericSpinBox("TVVidModeHeight", 0, 1200, 4, true);
    gs->setLabelAboveWidget(true);
    gs->setLabel(QObject::tr("Height"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical resolution for playback video mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}

static GenericSpinBox *TVVidModeHeight(int idx)
{
    GenericSpinBox *gs = new GenericSpinBox(QString("TVVidModeHeight%1").arg(idx),
                                            0, 1200, 4, true);
    gs->setLabel(QObject::tr("Y"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Vertical resolution for playback video mode. "
                                "This mode must be defined in your X "
                                "configuration file."));
    return gs;
}

static GenericCheckBox *TVVidModeAltAspect()
{
    GenericCheckBox *gc = new GenericCheckBox("TVVidModeAltAspect");
    gc->setLabelAboveWidget(true);
    gc->setLabel(QObject::tr("Alt Aspect"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If X's DisplaySize indicates 16:9, "
                                "fudge vertical size into 4:3, "
                                "and visa-versa"));
    return gc;
}

static GenericCheckBox *TVVidModeAltAspect(int idx)
{
    GenericCheckBox *gc = new GenericCheckBox(QString("TVVidModeAltAspect%1").arg(idx));
    gc->setLabelAboveWidget(true);
    gc->setLabel(QObject::tr("Alt Aspect"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If X's DisplaySize indicates 16:9, "
                                "fudge vertical size into 4:3, "
                                "and visa-versa"));
    return gc;
}

class VideoModeSettings: public VerticalConfigurationGroup,
                         public TriggeredConfigurationGroup {
  public:
    VideoModeSettings():
        VerticalConfigurationGroup(false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video Mode Settings"));
        setUseLabel(false);

        Setting *videomode = UseVideoModes();
        addChild(videomode);
        setTrigger(videomode);

        ConfigurationGroup* settings =
            new VerticalConfigurationGroup(false);
        ConfigurationGroup* defaultsettings =
            new HorizontalConfigurationGroup(false, false);
        ConfigurationGroup *xres =
            new HorizontalConfigurationGroup(true, false);
        ConfigurationGroup *yres =
            new HorizontalConfigurationGroup(true, false);
        ConfigurationGroup *zres =
            new HorizontalConfigurationGroup(true, false);

        xres->setLabel(QObject::tr("GUI Size (px)"));
        xres->addChild(GuiVidModeWidth());
        xres->addChild(GuiVidModeHeight());

        yres->setLabel(QObject::tr("Default Display (px)"));
        yres->addChild(TVVidModeWidth());
        yres->addChild(TVVidModeHeight());

        zres->setLabel(QObject::tr("Physical Size"));
        zres->addChild(TVVidModeAltAspect());

        defaultsettings->addChild(xres);
        defaultsettings->addChild(yres);
        defaultsettings->addChild(zres);

        settings->addChild(defaultsettings);

        for (int idx = 0; idx < 2; ++idx) {

            ConfigurationGroup *xres =
                new HorizontalConfigurationGroup(true, false);

            xres->addChild(VidModeWidth(idx));
            xres->addChild(VidModeHeight(idx));

            xres->addChild(TVVidModeWidth(idx));
            xres->addChild(TVVidModeHeight(idx));

            xres->addChild(TVVidModeAltAspect(idx));

            xres->setLabel(QString("Video Size ---> Display Size  "
                                   "(Override %1)").arg(idx + 1));

            settings->addChild(xres);
        }

        addTarget("1", settings);
        addTarget("0", new VerticalConfigurationGroup(true));
    }
};
#endif

static GenericCheckBox *RunInWindow()
{
    GenericCheckBox *gc = new GenericCheckBox("RunFrontendInWindow");
    gc->setLabel(QObject::tr("Run the frontend in a window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Toggles between windowed and borderless operation."));
    return gc;
}

static GenericCheckBox *RandomTheme()
{
    GenericCheckBox *gc = new GenericCheckBox("RandomTheme");
    gc->setLabel(QObject::tr("Use a random theme"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Use a random theme each time MythTV is "
                    "started."));
    return gc;
}

static GenericComboBox *MythDateFormat()
{
    GenericComboBox *gc = new GenericComboBox("DateFormat");
    gc->setLabel(QObject::tr("Date format"));

    QDate sampdate(2004, 1, 31);

    gc->addSelection(sampdate.toString("ddd MMM d"), "ddd MMM d");
    gc->addSelection(sampdate.toString("ddd MMMM d"), "ddd MMMM d");
    gc->addSelection(sampdate.toString("MMM d"), "MMM d");
    gc->addSelection(sampdate.toString("MM/dd"), "MM/dd");
    gc->addSelection(sampdate.toString("MM.dd"), "MM.dd");
    gc->addSelection(sampdate.toString("ddd d MMM"), "ddd d MMM");
    gc->addSelection(sampdate.toString("dd.MM.yyyy"), "dd.MM.yyyy");
    gc->addSelection(sampdate.toString("yyyy-MM-dd"), "yyyy-MM-dd");
    gc->setHelpText(QObject::tr("Your preferred date format."));
    return gc;
}

static GenericComboBox *MythShortDateFormat()
{
    GenericComboBox *gc = new GenericComboBox("ShortDateFormat");
    gc->setLabel(QObject::tr("Short Date format"));

    QDate sampdate(2004, 1, 31);

    gc->addSelection(sampdate.toString("M/d"), "M/d");
    gc->addSelection(sampdate.toString("d/M"), "d/M");
    gc->addSelection(sampdate.toString("MM/dd"), "MM/dd");
    gc->addSelection(sampdate.toString("dd/MM"), "dd/MM");
    gc->addSelection(sampdate.toString("MM.dd"), "MM.dd");
    gc->addSelection(sampdate.toString("d.M."), "d.M.");
    gc->addSelection(sampdate.toString("dd.MM."), "dd.MM.");
    gc->addSelection(sampdate.toString("MM-dd"), "MM-dd");
    gc->addSelection(sampdate.toString("ddd d"), "ddd d");
    gc->addSelection(sampdate.toString("d ddd"), "d ddd");
    gc->addSelection(sampdate.toString("ddd M/d"), "ddd M/d");
    gc->addSelection(sampdate.toString("M/d ddd"), "M/d ddd");
    gc->setHelpText(QObject::tr("Your preferred short date format."));
    return gc;
}

static GenericComboBox *MythTimeFormat()
{
    GenericComboBox *gc = new GenericComboBox("TimeFormat");
    gc->setLabel(QObject::tr("Time format"));

    QTime samptime(6, 56, 0);

    gc->addSelection(samptime.toString("h:mm AP"), "h:mm AP");
    gc->addSelection(samptime.toString("h:mm ap"), "h:mm ap");
    gc->addSelection(samptime.toString("hh:mm AP"), "hh:mm AP");
    gc->addSelection(samptime.toString("hh:mm ap"), "hh:mm ap");
    gc->addSelection(samptime.toString("h:mm"), "h:mm");
    gc->addSelection(samptime.toString("hh:mm"), "hh:mm");
    gc->setHelpText(QObject::tr("Your preferred time format.  Choose a format "
                    "with \"AP\" in it for an AM/PM display, otherwise "
                    "your time display will be 24-hour or \"military\" "
                    "time."));
    return gc;
}

static GenericComboBox *ThemeFontSizeType()
{
    GenericComboBox *gc = new GenericComboBox("ThemeFontSizeType");
    gc->setLabel(QObject::tr("Font size"));
    gc->addSelection(QObject::tr("default"), "default");
    gc->addSelection(QObject::tr("small"), "small");
    gc->addSelection(QObject::tr("big"), "big");
    gc->setHelpText(QObject::tr("default: TV, small: monitor, big:"));
    return gc;
}

ThemeSelector::ThemeSelector():
    GlobalSetting("Theme") {

    setLabel(QObject::tr("Theme"));

    QDir themes(gContext->GetThemesParentDir());
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();
        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (theme->fileName()[0] == '.' || !preview.exists() || !xml.exists()) {
            //cout << theme->absFilePath() << " doesn't look like a theme\n";
            continue;
        }

        QImage* previewImage = new QImage(preview.absFilePath());
        if (previewImage->width() == 0 || previewImage->height() == 0) {
            cout << QObject::tr("Problem reading theme preview image ")
                 << preview.dirPath() << endl;
            continue;
        }

        addImageSelection(theme->fileName(), previewImage);
    }

    setValue("G.A.N.T.");
}

class StyleSetting: public ComboBoxSetting, public GlobalSetting {
public:
    StyleSetting():
        GlobalSetting("Style") {
        setLabel(QObject::tr("Qt Style"));
        fillSelections();
        setHelpText(QObject::tr("At startup, MythTV will change the Qt "
                    "widget style to this setting.  If \"Desktop Style\" "
                    "is selected, MythTV will use the existing desktop "
                    "setting."));
    };

    void fillSelections(void) {
        clearSelections();
        addSelection(QObject::tr("Desktop Style"), "");
        QStyleFactory factory;
        QStringList list = factory.keys();
        QStringList::iterator iter = list.begin();
        for (; iter != list.end(); iter++ )
            addSelection(*iter);
    };

    void load(QSqlDatabase *db) {
        fillSelections();
        GlobalSetting::load(db);
    };
};

static GenericComboBox *ChannelFormat()
{
    GenericComboBox *gc = new GenericComboBox("ChannelFormat");
    gc->setLabel(QObject::tr("Channel format"));
    gc->addSelection(QObject::tr("number"), "<num>");
    gc->addSelection(QObject::tr("number callsign"), "<num> <sign>");
    gc->addSelection(QObject::tr("number name"), "<num> <name>");
    gc->addSelection(QObject::tr("callsign"), "<sign>");
    gc->addSelection(QObject::tr("name"), "<name>");
    gc->setHelpText(QObject::tr("Your preferred channel format."));
    gc->setValue(1);
    return gc;
}

static GenericComboBox *LongChannelFormat()
{
    GenericComboBox *gc = new GenericComboBox("LongChannelFormat");
    gc->setLabel(QObject::tr("Long Channel format"));
    gc->addSelection(QObject::tr("number"), "<num>");
    gc->addSelection(QObject::tr("number callsign"), "<num> <sign>");
    gc->addSelection(QObject::tr("number name"), "<num> <name>");
    gc->addSelection(QObject::tr("callsign"), "<sign>");
    gc->addSelection(QObject::tr("name"), "<name>");
    gc->setHelpText(QObject::tr("Your preferred long channel format."));
    gc->setValue(2);
    return gc;
}

static BackendSpinBox *ATSCCheckSignalWait()
{
    BackendSpinBox *bs = new BackendSpinBox("ATSCCheckSignalWait", 
                                            1000, 10000, 250);
    bs->setLabel(QObject::tr("Time limit for ATSC signal lock (msec)"));
    bs->setHelpText(QObject::tr("MythTV can check the signal strength "
                    "when you tune into a HDTV or other over-the-air "
                    "digital station. This value is the number of "
                    "milliseconds to allow before MythTV gives up "
                    "trying to get an acceptable signal."));
    bs->setValue(5000);
    return bs;
}

static BackendSpinBox *ATSCCheckSignalThreshold()
{
    BackendSpinBox *bs = new BackendSpinBox("ATSCCheckSignalThreshold",
                                            50, 90, 1);
    bs->setLabel(QObject::tr("ATSC Signal Threshold"));
    bs->setHelpText(QObject::tr("Threshold for a signal to be considered "
                    "acceptable. If you set this too low MythTV may "
                    "crash, and if you set it too high you may not be "
                    "able to tune a channel on which reception would "
                    "be acceptable."));
    bs->setValue(65);
    return bs;
}

static GenericCheckBox *SmartChannelChange()
{
    GenericCheckBox *gc = new GenericCheckBox("SmartChannelChange");
    gc->setLabel(QObject::tr("Change channels immediately without select"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("When a complete channel number is entered "
                    "MythTV will switch to that channel immediately without "
                    "requiring you to hit the select button."));
    return gc;
}

static BackendCheckBox *LastFreeCard()
{
    BackendCheckBox *bc = new BackendCheckBox("LastFreeCard");
    bc->setLabel(QObject::tr("Avoid conflicts between live TV and "
                 "scheduled shows."));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("If enabled, live TV will choose a tuner card "
                    "that is less likely to have scheduled recordings "
                    "rather than the best card available."));
    return bc;
}

static GenericSpinBox *QtFontBig()
{
    GenericSpinBox *gs = new GenericSpinBox("QtFontBig", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Big\" font"));
    gs->setValue(25);
    gs->setHelpText(QObject::tr("Default size is 25."));
    return gs;
}

static GenericSpinBox *QtFontMedium()
{
    GenericSpinBox *gs = new GenericSpinBox("QtFontMedium", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Medium\" font"));
    gs->setValue(16);
    gs->setHelpText(QObject::tr("Default size is 16."));
    return gs;
}

static GenericSpinBox *QtFontSmall()
{
    GenericSpinBox *gs = new GenericSpinBox("QtFontSmall", 1, 48, 1);
    gs->setLabel(QObject::tr("\"Small\" font"));
    gs->setValue(12);
    gs->setHelpText(QObject::tr("Default size is 12."));
    return gs;
}

// EPG settings
static GenericCheckBox *EPGScrollType()
{
    GenericCheckBox *gc = new GenericCheckBox("EPGScrollType");
    gc->setLabel(QObject::tr("Floating Program Guide Selector"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If enabled, the program guide's selector "
                    "will be free to move throughout the guide, otherwise "
                    "it will stay in the center of the guide at all times."));
    return gc;
}

static GenericComboBox *EPGFillType()
{
    GenericComboBox *gc = new GenericComboBox("EPGFillType");
    gc->setLabel(QObject::tr("Guide Shading Method"));
    gc->addSelection(QObject::tr("Alpha - Transparent (CPU Usage - High)"),
                     QString::number((int)UIGuideType::Alpha));
    gc->addSelection(QObject::tr("Blender - Transparent (CPU Usage - Middle)"),
                     QString::number((int)UIGuideType::Dense));
    gc->addSelection(QObject::tr("Eco - Transparent (CPU Usage - Low)"),
                     QString::number((int)UIGuideType::Eco));
    gc->addSelection(QObject::tr("Solid (CPU Usage - Middle)"),
                     QString::number((int)UIGuideType::Solid));
    return gc;
};

static GenericCheckBox *EPGShowCategoryColors()
{
    GenericCheckBox *gc = new GenericCheckBox("EPGShowCategoryColors");
    gc->setLabel(QObject::tr("Display Genre Colors"));
    gc->setHelpText(QObject::tr("Colorize program guide using "
                    "genre colors. (Not available for all grabbers.)"));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *EPGShowCategoryText()
{
    GenericCheckBox *gc = new GenericCheckBox("EPGShowCategoryText");
    gc->setLabel(QObject::tr("Display Genre Text"));
    gc->setHelpText(QObject::tr("(Not available for all grabbers.)"));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *EPGShowChannelIcon()
{
    GenericCheckBox *gc = new GenericCheckBox("EPGShowChannelIcon");
    gc->setLabel(QObject::tr("Display the channel icon"));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *EPGShowFavorites()
{
    GenericCheckBox *gc = new GenericCheckBox("EPGShowFavorites");
    gc->setLabel(QObject::tr("Only display 'favorite' channels"));
    gc->setHelpText(QObject::tr("If enabled, the EPG will initially display "
                    "only the channels marked as favorites. Pressing "
                    "\"4\" will toggle between displaying favorites and all "
                    "channels."));
    gc->setValue(false);
    return gc;
}

static GenericSpinBox *EPGChanDisplay()
{
    GenericSpinBox *gs = new GenericSpinBox("chanPerPage", 3, 12, 1);
    gs->setLabel(QObject::tr("Channels to Display"));
    gs->setValue(5);
    return gs;
}

static GenericSpinBox *EPGTimeDisplay()
{
    GenericSpinBox *gs = new GenericSpinBox("timePerPage", 1, 5, 1);
    gs->setLabel(QObject::tr("Time Blocks (30 mins) to Display"));
    gs->setValue(4);
    return gs;
}

// General RecPriorities settings

static BackendCheckBox *GRSchedMoveHigher()
{
    BackendCheckBox *bc = new BackendCheckBox("SchedMoveHigher");
    bc->setLabel(QObject::tr("Reschedule Higher Priorities"));
    bc->setHelpText(QObject::tr("Move higher priority programs to other "
                    "cards and showings when resolving conflicts.  This "
                    "can be used to record lower priority programs that "
                    "would otherwise not be recorded, but risks missing "
                    "a higher priority program if the schedule changes."));
    bc->setValue(false);
    return bc;
}

static BackendSpinBox *GRSingleRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("SingleRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Single Recordings Priority"));
    bs->setHelpText(QObject::tr("Single Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GRWeekslotRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("WeekslotRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Weekslot Recordings Priority"));
    bs->setHelpText(QObject::tr("Weekslot Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GRTimeslotRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("TimeslotRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Timeslot Recordings Priority"));
    bs->setHelpText(QObject::tr("Timeslot Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GRChannelRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("ChannelRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Channel Recordings Priority"));
    bs->setHelpText(QObject::tr("Channel Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GRAllRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("AllRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("All Recordings Priority"));
    bs->setHelpText(QObject::tr("All Recording types will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GRFindOneRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("FindOneRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Find One Recordings Priority"));
    bs->setHelpText(QObject::tr("Find One Recording types will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static BackendSpinBox *GROverrideRecordRecPriority()
{
    BackendSpinBox *bs = new BackendSpinBox("OverrideRecordRecPriority",
                                            -99, 99, 1);
    bs->setLabel(QObject::tr("Override Recordings Priority"));
    bs->setHelpText(QObject::tr("Override Recordings will receive this "
                    "additional recording priority value."));
    bs->setValue(0);
    return bs;
}

static GenericLineEdit *DefaultTVChannel()
{
    GenericLineEdit *ge = new GenericLineEdit("DefaultTVChannel");
    ge->setLabel(QObject::tr("Guide starts at channel"));
    ge->setValue("3");
    ge->setHelpText(QObject::tr("The program guide starts on this channel if "
                    "it is run from outside of LiveTV mode."));
    return ge;
}

static GenericLineEdit *UnknownTitle()
{
    GenericLineEdit *ge = new GenericLineEdit("UnknownTitle");
    ge->setLabel(QObject::tr("What to call 'unknown' programs"));
    ge->setValue(QObject::tr("Unknown"));
    return ge;
}

static GenericLineEdit *UnknownCategory()
{
    GenericLineEdit *ge = new GenericLineEdit("UnknownCategory");
    ge->setLabel(QObject::tr("What category to give 'unknown' programs"));
    ge->setValue(QObject::tr("Unknown"));
    return ge;
}

static GenericCheckBox *SelectChangesChannel()
{
    GenericCheckBox *gc = new GenericCheckBox("SelectChangesChannel");
    gc->setLabel(QObject::tr("Use select to change the channel in the program "
                 "guide"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If enabled, the Select key will change the "
                    "channel while using the program guide during live TV.  "
                    "If disabled, the select key will bring up the recording "
                    "options screen."));
    return gc;
}

static GenericSpinBox *EPGRecThreshold()
{
    GenericSpinBox *gs = new GenericSpinBox("SelChangeRecThreshold", 1, 600, 1);
    gs->setLabel(QObject::tr("Record Threshold"));
    gs->setValue(16);
    gs->setHelpText(QObject::tr("If the option to use Select to change the channel "
                    "is on, pressing Select on a show that is at least "
                    "this many minutes into the future will schedule a "
                    "recording."));
    return gs;
}

class AudioSettings: public VerticalConfigurationGroup,
                     public TriggeredConfigurationGroup {
public:
     AudioSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Audio"));
         setUseLabel(false);

         addChild(AudioOutputDevice());
         addChild(AC3PassThrough());
         addChild(AggressiveBuffer());

         Setting* volumeControl = MythControlsVolume();
         addChild(volumeControl);
         setTrigger(volumeControl);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(MixerDevice());
         settings->addChild(MixerControl());
         settings->addChild(MixerVolume());
         settings->addChild(PCMVolume());
         settings->addChild(IndividualMuteControl());
         addTarget("1", settings);

         // show nothing if volumeControl is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

static GenericComboBox *MythLanguage()
{
    GenericComboBox *gc = new GenericComboBox("Language");
    gc->setLabel(QObject::tr("Language"));
    gc->addSelection(QString::fromUtf8("English"), "EN");     // English
    gc->addSelection(QString::fromUtf8("Italiano"), "IT");    // Italian
    gc->addSelection(QString::fromUtf8("Català"), "CA");      // Catalan
    gc->addSelection(QString::fromUtf8("Español"), "ES");     // Spanish
    gc->addSelection(QString::fromUtf8("Nederlands"), "NL");  // Dutch
    gc->addSelection(QString::fromUtf8("Français"), "FR");    // French
    gc->addSelection(QString::fromUtf8("Deutsch"), "DE");     // German
    gc->addSelection(QString::fromUtf8("Dansk"), "DK");       // Danish
    gc->addSelection(QString::fromUtf8("Svenska"), "SV");     // Swedish
    gc->addSelection(QString::fromUtf8("Português"), "PT");   // Portuguese
    //gc->addSelection(QString::fromUtf8("日本語"), "JA");    // Japanese
    gc->addSelection(QString::fromUtf8("Nihongo"), "JA");     // Japanese
    gc->addSelection(QString::fromUtf8("Slovenski"), "SI");   // Slovenian
    gc->addSelection(QString::fromUtf8("Suomi"), "FI");   // Finnish
    gc->addSelection(QString::fromUtf8("Hanzi (Traditional)"), "ZH_TW"); // Traditional Chinese
    gc->setHelpText(QObject::tr("Your preferred language.") );
    return gc;
}

static GenericCheckBox *EnableXbox()
{
    GenericCheckBox *gc = new GenericCheckBox("EnableXbox");
    gc->setLabel(QObject::tr("Enable Xbox Hardware"));
    gc->setHelpText(QObject::tr("This enables support for Xbox Specific "
                    "hardware. Requires a frontend restart for changes to "
                    "take effect."));
    gc->setValue(false);
    return gc;
}

static GenericComboBox *XboxBlinkBIN()
{
    GenericComboBox *gc = new GenericComboBox("XboxBlinkBIN");
    gc->setLabel(QObject::tr("Xbox Linux Distribution"));
    gc->addSelection("GentooX","led");
    gc->addSelection(QObject::tr("Other"),"blink");
    gc->setHelpText(QObject::tr("The program used to control the "
                    "LED on the Xbox is dependant on which distribution is "
                    "installed. \"led\" will be used on GentooX, \"blink\" "
                    "on other Xbox distributions."));
    return gc;
}

static GenericComboBox *XboxLEDDefault()
{
    GenericComboBox *gc = new GenericComboBox("XboxLEDDefault");
    gc->setLabel(QObject::tr("Default LED color"));
    gc->addSelection(QObject::tr("Off"), "nnnn");
    gc->addSelection(QObject::tr("Green"),"gggg");
    gc->addSelection(QObject::tr("Orange"),"oooo");
    gc->addSelection(QObject::tr("Red"),"rrrr");
    gc->setHelpText(QObject::tr("Sets the LED color when it is not "
                    "being used for status indication."));
    return gc;
}

static GenericComboBox *XboxLEDRecording()
{
    GenericComboBox *gc = new GenericComboBox("XboxLEDRecording");
    gc->setLabel(QObject::tr("Recording LED mode"));
    gc->addSelection(QObject::tr("Off"), "nnnn");
    gc->addSelection(QObject::tr("Green"),"gggg");
    gc->addSelection(QObject::tr("Orange"),"oooo");
    gc->addSelection(QObject::tr("Red"),"rrrr");
    gc->setHelpText(QObject::tr("Sets the LED color when a backend is "
                    "recording."));
    return gc;
}

static GenericSpinBox *XboxCheckRec()
{
    GenericSpinBox *gs = new GenericSpinBox("XboxCheckRec", 1, 600, 2);
    gs->setLabel(QObject::tr("Recording Check Frequency"));
    gs->setValue(5);
    gs->setHelpText(QObject::tr("This specifies how frequently "
                    "(in seconds) to check if a recording is in "
                    "progress in order to update the Xbox LED."));
    return gs;
}

static GenericCheckBox *EnableMediaMon()
{
    GenericCheckBox *gc = new GenericCheckBox("MonitorDrives");
    gc->setLabel(QObject::tr("Monitor CD/DVD"));
    gc->setHelpText(QObject::tr("This enables support for monitoring "
                    "your CD/DVD drives for new disks and launching "
                    "the proper plugin to handle them."));
    gc->setValue(false);
    return gc;
}

static GenericCheckBox *PVR350OutputEnable()
{
    GenericCheckBox *gc = new GenericCheckBox("PVR350OutputEnable");
    gc->setLabel(QObject::tr("Use the PVR-350's TV out / MPEG decoder"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("MythTV can use the PVR-350's TV out and MPEG "
                    "decoder for high quality playback.  This requires that "
                    "the ivtv-fb kernel module is also loaded and set up "
                    "properly."));
    return gc;
}

static GenericLineEdit *PVR350VideoDev()
{
    GenericLineEdit *ge = new GenericLineEdit("PVR350VideoDev");
    ge->setLabel(QObject::tr("Video device for the PVR-350 MPEG decoder"));
    ge->setValue("/dev/video16");
    return ge;
};

static GenericSpinBox *PVR350EPGAlphaValue()
{
    GenericSpinBox *gs = new GenericSpinBox("PVR350EPGAlphaValue", 0, 255, 1);
    gs->setLabel(QObject::tr("Program Guide Alpha"));
    gs->setValue(164);
    gs->setHelpText(QObject::tr("How much to blend the program guide over the "
                    "live TV image.  Higher numbers mean more guide and less "
                    "TV."));
    return gs;
}

#ifdef USING_XVMC
static GenericCheckBox *UseXVMC()
{
    GenericCheckBox *gc = new GenericCheckBox("UseXVMC");
    gc->setLabel(QObject::tr("Use hardware XvMC MPEG Decoding"));
    gc->setValue(true);
    return gc;
};
#endif

#ifdef USING_XVMC_VLD
static GenericCheckBox *UseXvMcVld()
{
    GenericCheckBox *gc = new GenericCheckBox("UseXvMcVld");
    gc->setLabel(QObject::tr("Use HW XVMC VLD Decoding (Via only)"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("Enables the use of viaXvMC HW Mpeg decoding "
                    "for the Via Unichrome Chipset"));
    return gc;
};
#endif

class HwDecSettings: public  VerticalConfigurationGroup,
                     public TriggeredConfigurationGroup {
public:
     HwDecSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Hardware Decoding Settings"));
         setUseLabel(false);

         Setting* pvr350output = PVR350OutputEnable();
         addChild(pvr350output);
         setTrigger(pvr350output);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(PVR350VideoDev());
         settings->addChild(PVR350EPGAlphaValue());
         addTarget("1", settings);

         addTarget("0", new VerticalConfigurationGroup(true));

#ifdef USING_XVMC
         addChild(UseXVMC());
#endif
#ifdef USING_XVMC_VLD
         addChild(UseXvMcVld());
#endif
    };
};

#if USING_DVB
static BackendSpinBox *DVBMonitorInterval()
{
    BackendSpinBox *bs = new BackendSpinBox("DVBMonitorInterval", 0, 240, 5);
    bs->setLabel(QObject::tr("Sample interval for DVB signal statistics "
                 "(in seconds)"));
    bs->setValue(0);
    bs->setHelpText(QObject::tr("MythTV will monitor the DVB signal "
                    "statistics using the specified interval.  Set to "
                    "\"0\" to disable."));
    return bs;
}

static BackendSpinBox *DVBMonitorRetention()
{
    BackendSpinBox *bs = new BackendSpinBox("DVBMonitorRetention", 1, 30, 1);
    bs->setLabel(QObject::tr("Length of time to retain DVB signal data "
                             "(in days)"));
    bs->setValue(1);
    return bs;
}
#endif

static BackendCheckBox *LogEnabled()
{
    BackendCheckBox *bc = new BackendCheckBox("LogEnabled");
    bc->setLabel(QObject::tr("Log MythTV events to database"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("If enabled, MythTV modules will send event "
                    "details to the database, where they can be viewed with "
                    "MythLog or periodically emailed to the administrator."));
    return bc;
}

static GenericSpinBox *LogMaxCount()
{
    GenericSpinBox *gs = new GenericSpinBox("LogMaxCount", 0, 500, 10);
    gs->setLabel(QObject::tr("Maximum Number of Entries per Module"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("If there are more than this number of entries "
                    "for a module, the oldest log entries will be deleted to "
                    "reduce the count to this number.  Set to 0 to disable."));
    return gs;
}

static GenericCheckBox *LogCleanEnabled()
{
    GenericCheckBox *gc = new GenericCheckBox("LogCleanEnabled");
    gc->setLabel(QObject::tr("Automatic Log Cleaning Enabled"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("This enables the periodic cleanup of the "
                    "events stored in the Myth database (see \"Log MythTV "
                    "events to database\" on the previous page)."));
    return gc;
}

static GenericSpinBox *LogCleanPeriod()
{
    GenericSpinBox *gs = new GenericSpinBox("LogCleanPeriod", 0, 60, 1);
    gs->setLabel(QObject::tr("Log Cleanup Frequency (Days)"));
    gs->setValue(14);
    gs->setHelpText(QObject::tr("The number of days between log cleanup runs."));
    return gs;
}

static GenericSpinBox *LogCleanDays()
{
    GenericSpinBox *gs = new GenericSpinBox("LogCleanDays", 0, 60, 1);
    gs->setLabel(QObject::tr("Number of days to keep acknowledged log "
                 "entries"));
    gs->setValue(14);
    gs->setHelpText(QObject::tr("The number of days before a log entry that has "
                    "been acknowledged will be deleted by the log cleanup "
                    "process."));
    return gs;
}

static GenericSpinBox *LogCleanMax()
{
    GenericSpinBox *gs = new GenericSpinBox("LogCleanMax", 0, 60, 1);
    gs->setLabel(QObject::tr("Number of days to keep unacknowledged log "
                 "entries"));
    gs->setValue(30);
    gs->setHelpText(QObject::tr("The number of days before a log entry that "
                    "has NOT been acknowledged will be deleted by the log "
                    "cleanup process."));
    return gs;
}

static GenericComboBox *LogPrintLevel()
{
    GenericComboBox *gc = new GenericComboBox("LogPrintLevel");
    gc->setLabel(QObject::tr("Log Print Threshold"));
    gc->addSelection(QObject::tr("All Messages"), "8");
    gc->addSelection(QObject::tr("Debug and Higher"), "7");
    gc->addSelection(QObject::tr("Info and Higher"), "6");
    gc->addSelection(QObject::tr("Notice and Higher"), "5");
    gc->addSelection(QObject::tr("Warning and Higher"), "4");
    gc->addSelection(QObject::tr("Error and Higher"), "3");
    gc->addSelection(QObject::tr("Critical and Higher"), "2");
    gc->addSelection(QObject::tr("Alert and Higher"), "1");
    gc->addSelection(QObject::tr("Emergency Only"), "0");
    gc->addSelection(QObject::tr("Disable Printed Output"), "-1");
    gc->setHelpText(QObject::tr("This controls what messages will be printed "
                    "out as well as being logged to the database."));
    return gc;
}

static BackendCheckBox *MythFillEnabled()
{
    BackendCheckBox *bc = new BackendCheckBox("MythFillEnabled");
    bc->setLabel(QObject::tr("Automatically run mythfilldatabase"));
    bc->setValue(false);
    bc->setHelpText(QObject::tr("This enables the automatic execution of "
                    "mythfilldatabase."));
    return bc;
}

static BackendSpinBox *MythFillPeriod()
{
    BackendSpinBox *bs = new BackendSpinBox("MythFillPeriod", 0, 30, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Run Frequency (Days)"));
    bs->setValue(1);
    bs->setHelpText(QObject::tr("The number of days between mythfilldatabase "
                    "runs."));
    return bs;
}

static BackendSpinBox *MythFillMinHour()
{
    BackendSpinBox *bs = new BackendSpinBox("MythFillMinHour", 0, 24, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Execution Start"));
    bs->setValue(2);
    bs->setHelpText(QObject::tr("This setting and the following one define a "
                    "time period when the mythfilldatabase process is "
                    "allowed to run.  For example, setting Start to 11 and "
                    "End to 13 would mean that the process would only "
                    "run between 11 AM and 1 PM."));
    return bs;
}

static BackendSpinBox *MythFillMaxHour()
{
    BackendSpinBox *bs = new BackendSpinBox("MythFillMaxHour", 0, 24, 1);
    bs->setLabel(QObject::tr("mythfilldatabase Execution End"));
    bs->setValue(5);
    bs->setHelpText(QObject::tr("This setting and the preceding one define a "
                    "time period when the mythfilldatabase process is "
                    "allowed to run.  For example, setting Start to 11 and "
                    "End to 13 would mean that the process would only "
                    "run between 11 AM and 1 PM."));
    return bs;
}

static BackendLineEdit *MythFillDatabasePath()
{
    BackendLineEdit *be = new BackendLineEdit("MythFillDatabasePath");
    be->setLabel(QObject::tr("mythfilldatabase Path"));
    be->setValue("mythfilldatabase");
    be->setHelpText(QObject::tr("Path (including executable) of the "
                    "mythfilldatabase program."));
    return be;
}

static BackendLineEdit *MythFillDatabaseArgs()
{
    BackendLineEdit *be = new BackendLineEdit("MythFillDatabaseArgs");
    be->setLabel(QObject::tr("mythfilldatabase Arguments"));
    be->setValue("");
    be->setHelpText(QObject::tr("Any arguments you want passed to the "
                    "mythfilldatabase program."));
    return be;
}

static BackendLineEdit *MythFillDatabaseLog()
{
    BackendLineEdit *be = new BackendLineEdit("MythFillDatabaseLog");
    be->setLabel(QObject::tr("mythfilldatabase Log Path"));
    be->setValue("");
    be->setHelpText(QObject::tr("Path to use for logging output from "
                   "the mythfilldatabase program.  Leave blank "
                   "to disable logging."));
    return be;
}

class MythLogSettings: public VerticalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MythLogSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Myth Database Logging"));
//         setUseLabel(false);

         Setting* logEnabled = LogEnabled();
         addChild(logEnabled);
         setTrigger(logEnabled);
         addChild(LogMaxCount());

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(LogPrintLevel());
         settings->addChild(LogCleanEnabled());
         settings->addChild(LogCleanPeriod());
         settings->addChild(LogCleanDays());
         settings->addChild(LogCleanMax());
         addTarget("1", settings);

         // show nothing if logEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

class MythFillSettings: public VerticalConfigurationGroup,
                        public TriggeredConfigurationGroup {
public:
     MythFillSettings():
         VerticalConfigurationGroup(false),
         TriggeredConfigurationGroup(false) {
         setLabel(QObject::tr("Mythfilldatabase"));
         setUseLabel(false);

         Setting* fillEnabled = MythFillEnabled();
         addChild(fillEnabled);
         setTrigger(fillEnabled);

         ConfigurationGroup* settings = new VerticalConfigurationGroup(false);
         settings->addChild(MythFillDatabasePath());
         settings->addChild(MythFillDatabaseArgs());
         settings->addChild(MythFillDatabaseLog());
         settings->addChild(MythFillPeriod());
         settings->addChild(MythFillMinHour());
         settings->addChild(MythFillMaxHour());
         addTarget("1", settings);

         // show nothing if fillEnabled is off
         addTarget("0", new VerticalConfigurationGroup(true));
     };
};

#ifdef LCD_DEVICE
static GenericCheckBox *LCDShowTime()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowTime");
    gc->setLabel(QObject::tr("LCD Displays Time"));
    gc->setHelpText(QObject::tr("Display current time on idle LCD display. "
                    "Need to restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDShowMenu()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowMenu");
    gc->setLabel(QObject::tr("LCD Displays Menus"));
    gc->setHelpText(QObject::tr("Display selected menu on LCD display. "
                    "Need to restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDShowMusic()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowMusic");
    gc->setLabel(QObject::tr("LCD Displays Music Artist and Title"));
    gc->setHelpText(QObject::tr("Display playing artist and song title in "
                    "MythMusic. Need to restart mythfrontend to (de)activate "
                    "it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDShowChannel()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowChannel");
    gc->setLabel(QObject::tr("LCD Displays Channel Information"));
    gc->setHelpText(QObject::tr("Display tuned channel information. Need to "
                    "restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDShowVolume()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowVolume");
    gc->setLabel(QObject::tr("LCD Displays Volume Information"));
    gc->setHelpText(QObject::tr("Display volume level information. Need to "
                    "restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDShowGeneric()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDShowGeneric");
    gc->setLabel(QObject::tr("LCD Displays Generic Information"));
    gc->setHelpText(QObject::tr("Display generic information. Need to "
                    "restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}

static GenericCheckBox *LCDBacklightOn()
{
    GenericCheckBox *gc = new GenericCheckBox("LCDBacklightOn");
    gc->setLabel(QObject::tr("LCD Backlight Always On"));
    gc->setHelpText(QObject::tr("Turn on the backlight permanently. Need to "
                    "restart mythfrontend to (de)activate it."));
    gc->setValue(true);
    return gc;
}
#endif

#ifdef CONFIG_DARWIN
static GenericCheckBox *MacGammaCorrect()
{
    GenericCheckBox *gc = new GenericCheckBox("MacGammaCorrect");
    gc->setLabel(QObject::tr("Enable gamma correction for video"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, QuickTime will correct the gamma "
                    "of the video to match your monitor.  Turning this off can "
                    "save some CPU cycles."));
    return gc;
}

static GenericCheckBox *MacYuvConversion()
{
    GenericCheckBox *gc = new GenericCheckBox("MacYuvConversion");
    gc->setLabel(QObject::tr("Use Altivec-enhanced color space conversion"));
#ifdef USING_ALTIVEC
    gc->setValue(true);
#else
    gc->setValue(false);
#endif
    gc->setHelpText(QObject::tr("If checked, YUV 4:2:0 will be converted to "
                    "UYVY 4:2:2 in an Altivec-enabled routine.  If unchecked, "
                    "QuickTime will handle the conversion instead."));
    return gc;
}

static GenericCheckBox *MacScaleUp()
{
    GenericCheckBox *gc = new GenericCheckBox("MacScaleUp");
    gc->setLabel(QObject::tr("Scale video as necessary"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be scaled to fit your "
                    "window or screen. If unchecked, video will never be made "
                    "larger than its actual pixel size."));
    return gc;
}

static GenericSpinBox *MacFullSkip()
{
    GenericSpinBox *gs = new GenericSpinBox("MacFullSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip in fullscreen mode"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video displayed in fullscreen or non-windowed "
                    "mode will skip this many frames for each frame drawn. "
                    "Set to 0 to show every frame. Only valid when either "
                    "\"Use GUI size for TV playback\" or \"Run the frontend "
                    "in a window\" is not checked."));
    return gs;
}

static GenericCheckBox *MacMainEnabled()
{
    GenericCheckBox *gc = new GenericCheckBox("MacMainEnabled");
    gc->setLabel(QObject::tr("Video in main window"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in the "
                    "main GUI window. Disable this when you only want video "
                    "on the desktop or in a floating window. Only valid when "
                    "\"Use GUI size for TV playback\" and \"Run the "
                    "frontend in a window\" are checked."));
    return gc;
}

static GenericSpinBox *MacMainSkip()
{
    GenericSpinBox *gs = new GenericSpinBox("MacMainSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video in the main window will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

static GenericSpinBox *MacMainOpacity()
{
    GenericSpinBox *gs = new GenericSpinBox("MacMainOpacity", 0, 100, 5, false);
    gs->setLabel(QObject::tr("Opacity"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("The opacity of the main window. Set to "
                    "100 for completely opaque, set to 0 for completely "
                    "transparent."));
    return gs;
}

class MacMainSettings: public HorizontalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MacMainSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in main window"));
        setUseLabel(false);
        Setting *gc = MacMainEnabled();
        addChild(gc);
        setTrigger(gc);

        VerticalConfigurationGroup *opts = 
            new VerticalConfigurationGroup(false, false);
        opts->addChild(MacMainSkip());
        opts->addChild(MacMainOpacity());
        
        addTarget("1", opts);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

static GenericCheckBox *MacFloatEnabled()
{
    GenericCheckBox *gc = new GenericCheckBox("MacFloatEnabled");
    gc->setLabel(QObject::tr("Video in floating window"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in a "
                    "floating window. Only valid when \"Use GUI size for TV "
                    "playback\" and \"Run the frontend in a window\" are "
                    "checked."));
    return gc;
}

static GenericSpinBox *MacFloatSkip()
{
    GenericSpinBox *gs = new GenericSpinBox("MacFloatSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video in the floating window will skip "
                    "this many frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

static GenericSpinBox *MacFloatOpacity()
{
    GenericSpinBox *gs = new GenericSpinBox("MacFloatOpacity", 0, 100, 5, false);
    gs->setLabel(QObject::tr("Opacity"));
    gs->setValue(100);
    gs->setHelpText(QObject::tr("The opacity of the floating window. Set to "
                    "100 for completely opaque, set to 0 for completely "
                    "transparent."));
    return gs;
}

class MacFloatSettings: public HorizontalConfigurationGroup,
                        public TriggeredConfigurationGroup {
public:
    MacFloatSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in floating window"));
        setUseLabel(false);
        Setting *gc = MacFloatEnabled();
        addChild(gc);
        setTrigger(gc);

        VerticalConfigurationGroup *opts = 
            new VerticalConfigurationGroup(false, false);
        opts->addChild(MacFloatSkip());
        opts->addChild(MacFloatOpacity());
        
        addTarget("1", opts);
        addTarget("0", new VerticalConfigurationGroup(false, false));
    }
};

static GenericCheckBox *MacDockEnabled()
{
    GenericCheckBox *gc = new GenericCheckBox("MacDockEnabled");
    gc->setLabel(QObject::tr("Video in the dock"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If checked, video will be displayed in the "
                    "application's dock icon. Only valid when \"Use GUI size "
                    "for TV playback\" and \"Run the frontend in a window\" "
                    "are checked."));
    return gc;
}

static GenericSpinBox *MacDockSkip()
{
    GenericSpinBox *gs = new GenericSpinBox("MacDockSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(3);
    gs->setHelpText(QObject::tr("Video in the dock icon will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

class MacDockSettings: public HorizontalConfigurationGroup,
                       public TriggeredConfigurationGroup {
public:
    MacDockSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video in the dock"));
        setUseLabel(false);
        Setting *gc = MacDockEnabled();
        addChild(gc);
        setTrigger(gc);

        Setting *skip = MacDockSkip();
        addTarget("1", skip);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};

static GenericCheckBox *MacDesktopEnabled()
{
    GenericCheckBox *gc = new GenericCheckBox("MacDesktopEnabled");
    gc->setLabel(QObject::tr("Video on the desktop"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("If checked, video will be displayed on the "
                    "desktop, behind the Finder icons. Only valid when \"Use "
                    "GUI size for TV playback\" and \"Run the frontend in a "
                    "window\" are checked."));
    return gc;
}

static GenericSpinBox *MacDesktopSkip()
{
    GenericSpinBox *gs = new GenericSpinBox("MacDesktopSkip", 0, 30, 1, true);
    gs->setLabel(QObject::tr("Frames to skip"));
    gs->setValue(0);
    gs->setHelpText(QObject::tr("Video on the desktop will skip this many "
                    "frames for each frame drawn. Set to 0 to show "
                    "every frame."));
    return gs;
}

class MacDesktopSettings: public HorizontalConfigurationGroup,
                          public TriggeredConfigurationGroup {
public:
    MacDesktopSettings():
        HorizontalConfigurationGroup(false, false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("Video on the desktop"));
        setUseLabel(false);
        Setting *gc = MacDesktopEnabled();
        addChild(gc);
        setTrigger(gc);

        Setting *skip = MacDesktopSkip();
        addTarget("1", skip);
        addTarget("0", new HorizontalConfigurationGroup(false, false));
    }
};
#endif

static GenericCheckBox *WatchTVGuide()
{
    GenericCheckBox *gc = new GenericCheckBox("WatchTVGuide");
    gc->setLabel(QObject::tr("Show the program guide when starting Live TV"));
    gc->setHelpText(QObject::tr("This starts the program guide immediately "
             "upon starting to watch Live TV."));
    gc->setValue(false);
    return gc;
}

MainGeneralSettings::MainGeneralSettings()
{
    DatabaseSettings::addDatabaseSettings(this);

    AudioSettings *audio = new AudioSettings();
    addChild(audio);

    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(AllowQuitShutdown());
    general->addChild(NoPromptOnExit());
    general->addChild(HaltCommand());
    general->addChild(LircKeyPressedApp());
    general->addChild(UseArrowAccels());
    general->addChild(SetupPinCodeRequired());
    general->addChild(SetupPinCode());
    general->addChild(EnableMediaMon());
    general->addChild(EnableXbox());
    addChild(general);

    MythLogSettings *mythlog = new MythLogSettings();
    addChild(mythlog);

    MythFillSettings *mythfill = new MythFillSettings();
    addChild(mythfill);
}

PlaybackSettings::PlaybackSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General playback"));
    general->addChild(new DeinterlaceSettings());
    general->addChild(CustomFilters());
    general->addChild(UseVideoTimebase());
    general->addChild(DecodeExtraAudio());
    general->addChild(AspectOverride());
    general->addChild(PIPLocation());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General playback (part 2)"));
    gen2->addChild(PlaybackExitPrompt());
    gen2->addChild(EndOfRecordingExitPrompt());
    gen2->addChild(ClearSavedPosition());
    gen2->addChild(AltClearSavedPosition());
    gen2->addChild(UsePicControls());
    gen2->addChild(CCWarnSetting());
    gen2->addChild(UDPNotifyPort());
    addChild(gen2);

    VerticalConfigurationGroup* pbox = new VerticalConfigurationGroup(false);
    pbox->setLabel(QObject::tr("View Recordings"));
    pbox->addChild(PlayBoxOrdering());
    pbox->addChild(GeneratePreviewPixmaps());
    pbox->addChild(PreviewPixmapOffset());
    pbox->addChild(PlaybackPreview());
    pbox->addChild(PBBStartInTitle());
    pbox->addChild(PBBShowGroupSummary());
    addChild(pbox);

    VerticalConfigurationGroup* pbox2 = new VerticalConfigurationGroup(false);
    pbox2->setLabel(QObject::tr("View Recordings (Recording Groups)"));
    pbox2->addChild(AllRecGroupPassword());
    pbox2->addChild(DisplayRecGroup());
    pbox2->addChild(RememberRecGroup());
    pbox2->addChild(UseCategoriesAsRecGroups());
    pbox2->addChild(UseGroupNameAsAllPrograms());
    addChild(pbox2);

    addChild(new HwDecSettings());

    VerticalConfigurationGroup* seek = new VerticalConfigurationGroup(false);
    seek->setLabel(QObject::tr("Seeking"));
    seek->addChild(FastForwardAmount());
    seek->addChild(RewindAmount());
    seek->addChild(SmartForward());
    seek->addChild(StickyKeys());
    seek->addChild(FFRewRepos());
    seek->addChild(FFRewReverse());
    seek->addChild(ExactSeeking());
    seek->addChild(JumpAmount());
    addChild(seek);

    VerticalConfigurationGroup* comms = new VerticalConfigurationGroup(false);
    comms->setLabel(QObject::tr("Commercial Skip"));
    comms->addChild(AutoCommercialFlag());
    comms->addChild(CommercialSkipMethod());
    comms->addChild(AggressiveCommDetect());
    comms->addChild(CommSkipAllBlanks());
    comms->addChild(CommRewindAmount());
    comms->addChild(CommNotifyAmount());
    comms->addChild(AutoCommercialSkip());
    comms->addChild(TryUnflaggedSkip());
    addChild(comms);

    VerticalConfigurationGroup* oscan = new VerticalConfigurationGroup(false);
    oscan->setLabel(QObject::tr("Overscan"));
    oscan->addChild(VertScanPercentage());
    oscan->addChild(HorizScanPercentage());
    oscan->addChild(XScanDisplacement());
    oscan->addChild(YScanDisplacement());
    addChild(oscan);

    VerticalConfigurationGroup* osd = new VerticalConfigurationGroup(false);
    osd->setLabel(QObject::tr("On-screen display"));
    osd->addChild(OSDTheme());
    osd->addChild(OSDDisplayTime());
    osd->addChild(OSDFont());
    osd->addChild(OSDCCFont());
    osd->addChild(OSDThemeFontSizeType());
    osd->addChild(CCBackground());
    osd->addChild(DefaultCCMode());
    osd->addChild(PersistentBrowseMode());
    addChild(osd);

#ifdef CONFIG_DARWIN
    VerticalConfigurationGroup* mac1 = new VerticalConfigurationGroup(false);
    mac1->setLabel(QObject::tr("Mac OS X video settings") + " 1/2");
    mac1->addChild(MacGammaCorrect());
    mac1->addChild(MacYuvConversion());
    mac1->addChild(MacScaleUp());
    mac1->addChild(MacFullSkip());
    addChild(mac1);

    VerticalConfigurationGroup* mac2 = new VerticalConfigurationGroup(false);
    mac2->setLabel(QObject::tr("Mac OS X video settings") + " 2/2");
    mac2->addChild(new MacMainSettings());
    mac2->addChild(new MacFloatSettings());
    mac2->addChild(new MacDockSettings());
    mac2->addChild(new MacDesktopSettings());
    addChild(mac2);
#endif
}

GeneralSettings::GeneralSettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("General"));
    general->addChild(RecordPreRoll());
    general->addChild(RecordOverTime());
    general->addChild(ChannelOrdering());
    general->addChild(SmartChannelChange());
    general->addChild(LastFreeCard());
    addChild(general);

    VerticalConfigurationGroup* gen2 = new VerticalConfigurationGroup(false);
    gen2->setLabel(QObject::tr("General (page 2)"));
    gen2->addChild(ChannelFormat());
    gen2->addChild(LongChannelFormat());
    gen2->addChild(ATSCCheckSignalWait());
    gen2->addChild(ATSCCheckSignalThreshold());
    addChild(gen2);

    VerticalConfigurationGroup* autoexp = new VerticalConfigurationGroup(false);
    autoexp->setLabel(QObject::tr("Global Auto Expire Settings"));
    autoexp->addChild(AutoExpireDiskThreshold());
    autoexp->addChild(MinRecordDiskThreshold());
    autoexp->addChild(AutoExpireFrequency());
    autoexp->addChild(AutoExpireMethod());
    autoexp->addChild(AutoExpireDefault());
    autoexp->addChild(RerecordAutoExpired());
    addChild(autoexp);

#if USING_DVB
    VerticalConfigurationGroup* dvb = new VerticalConfigurationGroup(false);

    dvb->setLabel(QObject::tr("DVB Global Settings"));
    dvb->addChild(DVBMonitorInterval());
    dvb->addChild(DVBMonitorRetention());
    addChild(dvb);
#endif
}

EPGSettings::EPGSettings()
{
    VerticalConfigurationGroup* epg = new VerticalConfigurationGroup(false);
    epg->setLabel(QObject::tr("Program Guide") + " 1/2");
    epg->addChild(EPGFillType());
    epg->addChild(EPGShowCategoryColors());
    epg->addChild(EPGShowCategoryText());
    epg->addChild(EPGScrollType());
    epg->addChild(EPGShowChannelIcon());
    epg->addChild(EPGShowFavorites());
    epg->addChild(WatchTVGuide());
    epg->addChild(EPGChanDisplay());
    epg->addChild(EPGTimeDisplay());
    addChild(epg);

    VerticalConfigurationGroup* gen = new VerticalConfigurationGroup(false);
    gen->setLabel(QObject::tr("Program Guide") + " 2/2");
    gen->addChild(UnknownTitle());
    gen->addChild(UnknownCategory());
    gen->addChild(DefaultTVChannel());
    gen->addChild(SelectChangesChannel());
    gen->addChild(EPGRecThreshold());
    addChild(gen);
}

GeneralRecPrioritiesSettings::GeneralRecPrioritiesSettings()
{
    VerticalConfigurationGroup* gr = new VerticalConfigurationGroup(false);
    gr->setLabel(QObject::tr("General Recording Priorities Settings"));

    gr->addChild(GRSchedMoveHigher());
    gr->addChild(GRSingleRecordRecPriority());
    gr->addChild(GROverrideRecordRecPriority());
    gr->addChild(GRFindOneRecordRecPriority());
    gr->addChild(GRWeekslotRecordRecPriority());
    gr->addChild(GRTimeslotRecordRecPriority());
    gr->addChild(GRChannelRecordRecPriority());
    gr->addChild(GRAllRecordRecPriority());
    addChild(gr);
}

AppearanceSettings::AppearanceSettings()
{
    VerticalConfigurationGroup* theme = new VerticalConfigurationGroup(false);
    theme->setLabel(QObject::tr("Theme"));

    theme->addChild(new ThemeSelector());
    theme->addChild(new StyleSetting());
    theme->addChild(ThemeFontSizeType());
    theme->addChild(RandomTheme());
    theme->addChild(MenuTheme());
    addChild(theme);

    VerticalConfigurationGroup* screen = new VerticalConfigurationGroup(false);
    screen->setLabel(QObject::tr("Screen settings"));
    screen->addChild(XineramaScreen());
    screen->addChild(GuiWidth());
    screen->addChild(GuiHeight());
//    screen->addChild(DisplaySizeHeight());
//    screen->addChild(DisplaySizeWidth());
    screen->addChild(GuiOffsetX());
    screen->addChild(GuiOffsetY());
    screen->addChild(GuiSizeForTV());
    screen->addChild(RunInWindow());
    addChild(screen);

#if defined(USING_XRANDR) || defined(CONFIG_DARWIN)
    addChild(new VideoModeSettings());
#endif

    VerticalConfigurationGroup* dates = new VerticalConfigurationGroup(false);
    dates->setLabel(QObject::tr("Localization"));
    dates->addChild(MythLanguage());
    dates->addChild(MythDateFormat());
    dates->addChild(MythShortDateFormat());
    dates->addChild(MythTimeFormat());
    addChild(dates);

    VerticalConfigurationGroup* qttheme = new VerticalConfigurationGroup(false);
    qttheme->setLabel(QObject::tr("QT"));
    qttheme->addChild(QtFontSmall());
    qttheme->addChild(QtFontMedium());
    qttheme->addChild(QtFontBig());
    qttheme->addChild(PlayBoxTransparency());
    qttheme->addChild(PlayBoxShading());
    addChild(qttheme );

#ifdef LCD_DEVICE
    VerticalConfigurationGroup* lcdscreen = new VerticalConfigurationGroup(false);
    lcdscreen->setLabel(QObject::tr("LCD device display"));
    lcdscreen->addChild(LCDShowTime());
    lcdscreen->addChild(LCDShowMenu());
    lcdscreen->addChild(LCDShowMusic());
    lcdscreen->addChild(LCDShowChannel());
    lcdscreen->addChild(LCDShowVolume());
    lcdscreen->addChild(LCDShowGeneric());
    lcdscreen->addChild(LCDBacklightOn());
    addChild(lcdscreen);
#endif
}

XboxSettings::XboxSettings()
{
    VerticalConfigurationGroup* xboxset = new VerticalConfigurationGroup(false);

    xboxset->setLabel(QObject::tr("Xbox"));
    xboxset->addChild(XboxBlinkBIN());
    xboxset->addChild(XboxLEDDefault());
    xboxset->addChild(XboxLEDRecording());
    xboxset->addChild(XboxCheckRec());
    addChild(xboxset);
}

