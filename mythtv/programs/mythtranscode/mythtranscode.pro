include ( ../../config.mak )
include ( ../../settings.pro)
include ( ../programs-libs.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp transcode.cpp mpeg2fix.cpp helper.c
SOURCES += replex/element.c replex/mpg_common.c replex/multiplex.c \
           replex/pes.c     replex/ringbuffer.c replex/ts.c
HEADERS += mpeg2fix.h transcodedefs.h
HEADERS += replex/element.h replex/mpg_common.h replex/multiplex.h \
           replex/pes.h     replex/ringbuffer.h replex/ts.h

INCLUDEPATH += replex
INCLUDEPATH += ../../libs/libavcodec ../../libs/libavformat \
               ../../libs/libavutil  ../../libs/ ../../libs/libmythmpeg2

#The following line was inserted by qt3to4
QT += network xml  sql opengl qt3support
