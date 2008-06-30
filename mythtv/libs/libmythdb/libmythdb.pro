include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythdb-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += mythexp.h mythdbcon.h mythdb.h mythdbparams.h oldsettings.h
HEADERS += mythverbose.h mythversion.h compat.h mythconfig.h
HEADERS += mythobservable.h mythevent.h httpcomms.h qcodecs.h
HEADERS += mythtimer.h mythdirs.h mythsocket.h lcddevice.h
HEADERS += exitcodes.h msocketdevice.h

SOURCES += mythdbcon.cpp mythdb.cpp oldsettings.cpp mythverbose.cpp
SOURCES += mythobservable.cpp httpcomms.cpp qcodecs.cpp mythdirs.cpp
SOURCES += msocketdevice.cpp mythsocket.cpp lcddevice.cpp

win32:SOURCES += msocketdevice_win.cpp
unix:SOURCES  += msocketdevice_unix.cpp

# Install headers to same location as libmyth to make things easier
inc.path = $${PREFIX}/include/mythtv/
inc.files  = mythverbose.h mythdbcon.h mythdbparams.h mythexp.h mythdb.h
inc.files += compat.h mythversion.h mythconfig.h mythconfig.mak
inc.files += mythobservable.h mythevent.h httpcomms.h qcodecs.h
inc.files += mythtimer.h lcddevice.h mythsocket.h exitcodes.h mythdirs.h
inc.files += msocketdevice.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}

inc3.path  = $${PREFIX}/include/mythtv/libmythdb
inc3.files = $${inc.files}

INSTALLS += inc inc2 inc3

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

QT += sql network

include ( ../libs-targetfix.pro )

