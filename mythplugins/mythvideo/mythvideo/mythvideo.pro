######################################################################
# Automatically generated by qmake (1.02a) Wed Jul 24 19:17:01 2002
######################################################################

include ( ../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythvideo
target.path = $${PREFIX}/bin
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = video-ui.xml
installfiles.path = $${PREFIX}/share/mythtv/
installfiles.files = videomenu.xml video_settings.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files = mythvideo_it.qm

INSTALLS += installfiles trans uifiles installimages

LIBS += -L$${PREFIX}/lib -lmyth-$$LIBVERSION

# Input

HEADERS += metadata.h videomanager.h inetcomms.h videobrowser.h globalsettings.h

SOURCES += main.cpp metadata.cpp videomanager.cpp inetcomms.cpp videobrowser.cpp globalsettings.cpp
TRANSLATIONS = mythvideo_it.ts
