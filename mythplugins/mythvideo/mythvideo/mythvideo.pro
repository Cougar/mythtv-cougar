######################################################################
# Automatically generated by qmake (1.02a) Wed Jul 24 19:17:01 2002
######################################################################

include ( ../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythvideo
target.path = $${PREFIX}/bin
INSTALLS += target

installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = mythexplorer-settings.txt videomenu.xml

INSTALLS += installfiles

LIBS += 
LIBS += -L$${PREFIX}/lib -lmyth-$$LIBVERSION

# Input

HEADERS += metadata.h databasebox.h treecheckitem.h 
SOURCES += main.cpp metadata.cpp databasebox.cpp treecheckitem.cpp 
