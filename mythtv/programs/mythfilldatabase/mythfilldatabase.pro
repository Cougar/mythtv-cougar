######################################################################
# Automatically generated by qmake (1.03a) Sat Aug 31 10:10:35 2002
######################################################################

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

INCLUDEPATH += ../../libs/
LIBS += -lmyth-$$LIBVERSION -lXinerama -L../../libs/libmyth -L/usr/local/lib

DEPENDPATH += ../../libs/libmyth

# Input
SOURCES += filldata.cpp
