include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += debug thread dll 
target.path = $${PREFIX}/lib
INSTALLS = target
INCLUDEPATH += ../../libs/libmyth
LIBS += -L../libmyth -lmyth-$$LIBVERSION

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h
HEADERS += mythpainter_ogl.h mythpainter_qt.h
HEADERS += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
HEADERS += mythuitext.h mythuianimatedimage.h mythlistbutton.h
HEADERS += oldsettings.h remotefile.h util.h themedmenu.h
HEADERS += dialogbox.h myththemeddialog.h mythxmlparser.h mythcontainer.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp
SOURCES += mythpainter_ogl.cpp mythpainter_qt.cpp
SOURCES += mythscreenstack.cpp mythscreentype.cpp 
SOURCES += mythuitype.cpp mythuiimage.cpp mythuitext.cpp mythuianimatedimage.cpp
SOURCES += mythlistbutton.cpp
SOURCES += oldsettings.cpp remotefile.cpp themedmenu.cpp 
SOURCES += util.cpp
SOURCES += dialogbox.cpp myththemeddialog.cpp mythxmlparser.cpp mythcontainer.cpp

inc.path = $${PREFIX}/include/mythui/

inc.files  = mythmainwindow.h mythpainter.h mythimage.h
inc.files += mythpainter_ogl.h mythpainter_qt.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
inc.files += mythuitext.h mythuianimatedimage.h mythlistbutton.h
inc.files += oldsettings.h remotefile.h util.h themedmenu.h
inc.files += dialogbox.h mythfontproperties.h myththemeddialog.h

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11 {
    LIBS += -L/usr/X11R6/lib -lXinerama
}

macx {
    LIBS += -framework OpenGL
    # Duplication of source with libmyth (e.g. oldsettings.cpp)
    # means that the linker complains, so we have to ignore duplicates 
    QMAKE_LFLAGS_SHLIB += -multiply_defined suppress
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xCC000000
}

