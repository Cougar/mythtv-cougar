include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythcontrols
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = controls-ui.xml
installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = controls-ui.xml

INSTALLS += uifiles

CFLAGS += -I$${PREFIX}/include

# Input
HEADERS += action.h actionid.h mythcontrols.h keybindings.h keygrabber.h
SOURCES += action.cpp actionset.cpp keybindings.cpp mythcontrols.cpp
SOURCES += keygrabber.cpp main.cpp 

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
