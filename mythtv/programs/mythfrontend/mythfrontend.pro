INCLUDEPATH += ../../libs/libmythtv ../../libs ../../libs/libmyth
DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += theme.txt mysql.txt
setting.files += info_menu.xml mainmenu.xml media_settings.xml tv_schedule.xml
setting.files += util_menu.xml info_settings.xml main_settings.xml
setting.files += recpriorities_settings.xml tv_search.xml tv_lists.xml
setting.files += library.xml manage_recordings.xml optical_menu.xml tvmenu.xml
setting.files += tv_settings.xml
setting.extra = -ldconfig

INSTALLS += setting

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

# Input
HEADERS += manualbox.h playbackbox.h viewscheduled.h globalsettings.h
HEADERS += manualschedule.h programrecpriority.h channelrecpriority.h
HEADERS += statusbox.h

SOURCES += main.cpp manualbox.cpp playbackbox.cpp viewscheduled.cpp
SOURCES += globalsettings.cpp manualschedule.cpp programrecpriority.cpp 
SOURCES += channelrecpriority.cpp statusbox.cpp
