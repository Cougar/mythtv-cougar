######################################################################
# Automatically generated by qmake (1.02a) Mon Jul 8 22:32:30 2002
######################################################################

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = /usr/local/bin
INSTALLS = target

include ( ../settings.pro )

setting.path = /usr/local/share/mythtv/
setting.files += theme.txt mysql.txt mainmenu.xml tvmenu.xml

INSTALLS += setting

INCLUDEPATH += ../mythepg ../mythdialog ../libNuppelVideo ../libmythtv
LIBS += -L../libmythtv -L../libNuppelVideo -L../libavcodec
LIBS += -lmythtv -lNuppelVideo -lXv -lttf -lmp3lame -lavcodec

TARGETDEPS  = ../libNuppelVideo/libNuppelVideo.a ../libmythtv/libmythtv.a
TARGETDEPS += ../libavcodec/libavcodec.a

# Input
HEADERS += ../mythepg/guidegrid.h ../mythepg/infodialog.h 
HEADERS += ../mythepg/infostructs.h ../mythdialog/dialogbox.h 
HEADERS += scheduler.h playbackbox.h deletebox.h programlistitem.h 
HEADERS += viewscheduled.h themedmenu.h

SOURCES += ../mythepg/guidegrid.cpp ../mythepg/infodialog.cpp 
SOURCES += ../mythepg/infostructs.cpp ../mythdialog/dialogbox.cpp main.cpp 
SOURCES += scheduler.cpp playbackbox.cpp deletebox.cpp 
SOURCES += programlistitem.cpp viewscheduled.cpp themedmenu.cpp
