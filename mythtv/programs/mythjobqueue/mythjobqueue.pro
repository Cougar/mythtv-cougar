include ( ../../config.mak )
include ( ../../settings.pro)
include ( ../programs-libs.pro)

QT += sql
CONFIG -= opengl

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp
