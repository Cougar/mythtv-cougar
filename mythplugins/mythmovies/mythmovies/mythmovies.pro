include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmovies
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = movie_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = movies-ui.xml

INSTALLS += uifiles installfiles

# Input
HEADERS += moviesui.h helperobjects.h moviessettings.h

SOURCES += main.cpp moviesui.cpp moviessettings.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
