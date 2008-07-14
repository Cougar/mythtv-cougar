#include <iostream>


#include <qapplication.h>
#include <qimage.h>
#include <qdir.h>

#include "iconview.h"
#include "gallerysettings.h"
#include "dbcheck.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/mythmedia.h>
#include <mythtv/mythpluginapi.h>

static void run(MythMediaDevice *dev)
{
    QString startdir = gContext->GetSetting("GalleryDir");

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    IconView *iconview = new IconView(mainStack, "mythgallery",
                                      startdir, dev);

    if (iconview->Create())
        mainStack->AddScreen(iconview);
}

void runGallery(void)
{
    run(NULL);
}

void handleMedia(MythMediaDevice *dev)
{
    if (dev && dev->isUsable())
        run(dev);
}

void setupKeys(void)
{
    REG_JUMP("MythGallery", "Image viewer / slideshow", "", runGallery);

    REG_KEY("Gallery", "PLAY", "Start/Stop Slideshow", "P");
    REG_KEY("Gallery", "HOME", "Go to the first image in thumbnail view",
            "Home");
    REG_KEY("Gallery", "END", "Go to the last image in thumbnail view", "End");
    REG_KEY("Gallery", "MENU", "Toggle activating menu in thumbnail view", "M");
    REG_KEY("Gallery", "SLIDESHOW", "Start Slideshow in thumbnail view", "S");
    REG_KEY("Gallery", "RANDOMSHOW", "Start Random Slideshow in thumbnail view", "R");

    REG_KEY("Gallery", "ROTRIGHT", "Rotate image right 90 degrees", "],3");
    REG_KEY("Gallery", "ROTLEFT", "Rotate image left 90 degrees", "[,1");
    REG_KEY("Gallery", "ZOOMOUT", "Zoom image out", "7");
    REG_KEY("Gallery", "ZOOMIN", "Zoom image in", "9");
    REG_KEY("Gallery", "SCROLLUP", "Scroll image up", "2");
    REG_KEY("Gallery", "SCROLLLEFT", "Scroll image left", "4");
    REG_KEY("Gallery", "SCROLLRIGHT", "Scroll image right", "6");
    REG_KEY("Gallery", "SCROLLDOWN", "Scroll image down", "8");
    REG_KEY("Gallery", "RECENTER", "Recenter image", "5");
    REG_KEY("Gallery", "FULLSIZE", "Full-size (un-zoom) image", "0");
    REG_KEY("Gallery", "UPLEFT", "Go to the upper-left corner of the image",
            "PgUp");
    REG_KEY("Gallery", "LOWRIGHT", "Go to the lower-right corner of the image",
            "PgDown");
    REG_KEY("Gallery", "INFO", "Toggle Showing Information about Image", "I");
    REG_KEY("Gallery", "DELETE", "Delete marked images or current image if none are marked", "D");
    REG_KEY("Gallery", "MARK", "Mark image", "T");
    REG_KEY("Gallery", "FULLSCREEN", "Toggle scale to fullscreen/scale to fit", "W");
    REG_MEDIA_HANDLER("MythGallery Media Handler 1/2", "", "", handleMedia,
                      MEDIATYPE_DATA | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER("MythGallery Media Handler 2/2", "", "", handleMedia,
                      MEDIATYPE_MGALLERY, "gif,jpg,png");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgallery", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    UpgradeGalleryDatabaseSchema();
    gContext->ActivateSettingsCache(true);

    GallerySettings settings;
    settings.Load();
    settings.Save();

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    runGallery();
    return 0;
}

int mythplugin_config(void)
{
    GallerySettings settings;
    settings.exec();

    return 0;
}

