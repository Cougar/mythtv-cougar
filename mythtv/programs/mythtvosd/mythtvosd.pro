include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythtvosd
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 

# Input
SOURCES += main.cpp
