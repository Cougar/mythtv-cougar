######################################################################
# Automatically generated by qmake (1.03a) Wed Sep 25 13:43:31 2002
######################################################################

TEMPLATE = app
CONFIG -= moc
CONFIG += thread

INCLUDEPATH += ../libs/
LIBS +=  -L../libs/libmyth -L../libs/libmythtv

include ( ../settings.pro )

LIBS += -lmyth-$$LIBVERSION -lmythtv $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmyth

# Input
SOURCES += main.cpp backendsettings.cpp

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu
