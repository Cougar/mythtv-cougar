include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += debug thread dll 
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../libmyth
INCLUDEPATH += ../.. ../

DEPENDPATH += ../libmyth .

LIBS += -L../libmyth -lmyth-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h myththemebase.h
HEADERS += mythpainter_qt.h
HEADERS += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
HEADERS += mythuitext.h mythuistatetype.h mythgesture.h xmlparsebase.h
HEADERS += mythuibutton.h mythlistbutton.h myththemedmenu.h mythdialogbox.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp myththemebase.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp
SOURCES += mythscreenstack.cpp mythscreentype.cpp mythgesture.cpp
SOURCES += mythuitype.cpp mythuiimage.cpp mythuitext.cpp
SOURCES += mythuistatetype.cpp mythlistbutton.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp

inc.path = $${PREFIX}/include/mythtv/libmythui/

inc.files  = mythmainwindow.h mythpainter.h mythimage.h myththemebase.h
inc.files += mythpainter_qt.h mythuistatetype.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
inc.files += mythuitext.h mythuibutton.h mythlistbutton.h xmlparsebase.h
inc.files += myththemedmenu.h mythdialogbox.h mythfontproperties.h

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11 {
    DEFINES += USE_OPENGL_PAINTER
    SOURCES += mythpainter_ogl.cpp
    HEADERS += mythpainter_ogl.h
    inc.files += mythpainter_ogl.h
}

using_x11 {
    LIBS += $$EXTRA_LIBS
}

macx {
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/Carbon.framework/Frameworks
    LIBS           += -framework Carbon -framework OpenGL

    QMAKE_LFLAGS_SHLIB += -seg1addr 0xCC000000
}

using_joystick_menu {
    DEFINES += USE_JOYSTICK_MENU
}

using_lirc {
    DEFINES += USE_LIRC
}

cygwin:DEFINES += _WIN32
