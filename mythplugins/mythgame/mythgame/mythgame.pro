include ( ../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythgame
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = game_settings.xml
uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = game-ui.xml

INSTALLS += uifiles installfiles

# Input
HEADERS += gamehandler.h rominfo.h unzip.h gamesettings.h gametree.h
HEADERS += mamehandler.h mamerominfo.h mamesettingsdlg.h
HEADERS += neshandler.h nesrominfo.h nessettingsdlg.h
HEADERS += sneshandler.h snesrominfo.h snessettingsdlg.h
HEADERS += pchandler.h pcrominfo.h pcsettingsdlg.h 

SOURCES += main.cpp gamehandler.cpp rominfo.cpp gametree.cpp unzip.c
SOURCES += gamesettings.cpp
SOURCES += mamehandler.cpp mamerominfo.cpp mamesettingsdlg.cpp
SOURCES += neshandler.cpp nesrominfo.cpp nessettingsdlg.cpp 
SOURCES += sneshandler.cpp snesrominfo.cpp snessettingsdlg.cpp 
SOURCES += pchandler.cpp pcrominfo.cpp pcsettingsdlg.cpp

