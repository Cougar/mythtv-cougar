######################################################################
# Automatically generated by qmake (1.03a) Fri Aug 23 15:01:33 2002
######################################################################

include ( ../../settings.pro )

TEMPLATE = lib
CONFIG -= moc
CONFIG += plugin thread
target.path = $${PREFIX}/lib/mythtv/filters
INSTALLS = target

INCLUDEPATH += ../../libs/libNuppelVideo

QMAKE_CFLAGS_RELEASE += -Wno-missing-prototypes -O2 -fomit-frame-pointer
QMAKE_CFLAGS_DEBUG += -Wno-missing-prototypes

# Input
HEADERS += cpudetect.h cputable.h mangle.h postprocess.h postprocess_template.c
SOURCES += cpudetect.c postprocess.c filter_pp.c
