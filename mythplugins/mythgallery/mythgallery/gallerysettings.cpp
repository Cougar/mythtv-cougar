#include <mythtv/mythcontext.h>

#include "gallerysettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class MythGalleryDir: public LineEditSetting, public GlobalSetting {
public:
    MythGalleryDir():
        GlobalSetting("GalleryDir") {
        setLabel(QObject::tr("Directory that holds images"));
        setValue("/var/lib/pictures");
        setHelpText(QObject::tr("This directory must exist and "
                       "MythGallery needs to have read permission."));
    };
};

class MythGalleryImportDirs: public LineEditSetting, public GlobalSetting {
public:
    MythGalleryImportDirs():
        GlobalSetting("GalleryImportDirs") {
        setLabel(QObject::tr("Paths to import images from"));
        setValue("/mnt/cdrom:/mnt/camera");
        setHelpText(QObject::tr("This is a colon separated list of paths. "
                    "If the path in the list is a directory, its contents will "
                    "be copied. If it is an executable, it will be run."));
    };
};

class SlideshowTransition: public ComboBoxSetting, public GlobalSetting {
public:
    SlideshowTransition() : 
      GlobalSetting("SlideshowTransition") {
        setLabel(QObject::tr("Type of transition"));
        addSelection("none");
        addSelection("fade"); 
        addSelection("wipe");
        setHelpText(QObject::tr("This is the type of transition used "
                    "between pictures in slideshow mode."));
    }
};

class SlideshowBackground: public ComboBoxSetting, public GlobalSetting {
public:
    SlideshowBackground() :
      GlobalSetting("SlideshowBackground") {
        setLabel(QObject::tr("Type of background"));
        // use names from /etc/X11/rgb.txt
        addSelection("theme","");
        addSelection("black");
        addSelection("white");
        setHelpText(QObject::tr("This is the type of background for each "
                    "picture in single view mode."));
    }
};

class SlideshowDelay: public SpinBoxSetting, public GlobalSetting {
public:
    SlideshowDelay():
        SpinBoxSetting(1,600,1) ,
        GlobalSetting("SlideshowDelay") {
        setLabel(QObject::tr("Slideshow Delay"));
        setValue(5);
        setHelpText(QObject::tr("This is the number of seconds to display each "
                    "picture."));
    };
};


GallerySettings::GallerySettings()
{
    VerticalConfigurationGroup* settings = new VerticalConfigurationGroup(false);
    settings->setLabel(QObject::tr("MythGallery Settings"));
    settings->addChild(new MythGalleryDir());
    settings->addChild(new MythGalleryImportDirs());
    //settings->addChild(new SlideshowTransition());
    settings->addChild(new SlideshowBackground());
    settings->addChild(new SlideshowDelay());
    addChild(settings);
}

