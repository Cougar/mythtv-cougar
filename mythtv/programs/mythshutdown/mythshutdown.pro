include ( ../../config.mak )
include (../../settings.pro)
include (../programs-libs.pro)

QT += sql

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += 
SOURCES += main.cpp
