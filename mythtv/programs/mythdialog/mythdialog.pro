######################################################################
# Automatically generated by qmake (1.02a) Tue Jul 2 15:55:32 2002
######################################################################

TEMPLATE = app
CONFIG += thread
target.path = /usr/local/bin
INSTALLS = target

include (../settings.pro)

INCLUDEPATH += ../libmythtv

# Input
HEADERS += dialogbox.h ../libmythtv/settings.h
SOURCES += dialogbox.cpp main.cpp ../libmythtv/settings.cpp

