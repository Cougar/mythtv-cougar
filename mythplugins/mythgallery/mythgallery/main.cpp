#include <iostream>


#include <qapplication.h>
#include <qsqldatabase.h>
#include <qimage.h>

#include "iconview.h"
#include "gallerysettings.h"
#include "dbcheck.h"
#include "qtiffio.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runGallery(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgallery_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QString startdir = gContext->GetSetting("GalleryDir");
    IconView icv(QSqlDatabase::database(), startdir,
                  gContext->GetMainWindow(), "icon view");
    icv.exec();

    qApp->removeTranslator(&translator);
}

void setupKeys(void)
{
    REG_JUMP("MythGallery", "Image viewer / slideshow", "", runGallery);

    REG_KEY("Gallery", "PLAY", "Start/Stop Slideshow", "P");
    REG_KEY("Gallery", "HOME", "Go to the first image in thumbnail view", 
            "Home");
    REG_KEY("Gallery", "END", "Go to the last image in thumbnail view", "End");
    REG_KEY("Gallery", "MENU", "Toggle activating menu in thumbnail view", "M");

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
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgallery", libversion, 
                                    MYTH_BINARY_VERSION))
        return -1;

    qInitTiffIO();
    

    UpgradeGalleryDatabaseSchema();

    GallerySettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());    

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
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgallery_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    GallerySettings settings;
    settings.exec(QSqlDatabase::database());

    qApp->removeTranslator(&translator);
    return 0;
}

