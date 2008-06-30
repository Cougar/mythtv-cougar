include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gametree.h
HEADERS += rom_metadata.h romedit.h

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gametree.cpp unzip.c
SOURCES += gamesettings.cpp dbcheck.cpp rom_metadata.cpp romedit.cpp

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

LIBS += -lz

#The following line was inserted by qt3to4
QT += xml sql opengl qt3support 

include ( ../../libs-targetfix.pro )
