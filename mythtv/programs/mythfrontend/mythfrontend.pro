######################################################################
# Automatically generated by qmake (1.02a) Mon Jul 8 22:32:30 2002
######################################################################

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += theme.txt mysql.txt mainmenu.xml tvmenu.xml tv_settings.xml
setting.extra = -ldconfig

INSTALLS += setting

INCLUDEPATH += ../../libs/libNuppelVideo ../../libs/libmythtv ../../libs

LIBS += -L../../libs/libmythtv -L../../libs/libNuppelVideo 
LIBS += -L../../libs/libavcodec -L../../libs/libmyth

LIBS += -lmythtv -lNuppelVideo -lavcodec -lmyth-$$LIBVERSION -lXv -lXinerama
LIBS += -lmp3lame 

TARGETDEPS  = ../../libs/libNuppelVideo/libNuppelVideo.a 
TARGETDEPS += ../../libs/libmythtv/libmythtv.a
TARGETDEPS += ../../libs/libavcodec/libavcodec.a

# Input
HEADERS += playbackbox.h programlistitem.h viewscheduled.h themesetup.h
HEADERS += globalsettings.h setrectime.h

SOURCES += main.cpp playbackbox.cpp programlistitem.cpp viewscheduled.cpp
SOURCES += themesetup.cpp globalsettings.cpp setrectime.cpp
