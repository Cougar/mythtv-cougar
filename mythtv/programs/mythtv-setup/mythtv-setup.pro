include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythtv-setup
target.path = $${PREFIX}/bin

INSTALLS = target

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += backendsettings.h
SOURCES += backendsettings.cpp checksetup.cpp main.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythtv-setup
    }
}

using_x11:DEFINES += USING_X11

#The following line was inserted by qt3to4
QT += network xml  sql opengl qt3support
