include ( ../../settings.pro )

INCLUDEPATH += ../../libs/libmythui ../../libs/libmyth
DEPENDPATH += ../../libs/libmythui ../../libs/libmyth

LIBS += -L../../libs/libmythui -lmythui-$$LIBVERSION

macx {
    # OS X complains about indirectly linked libraries
    LIBS += -L../../libs/libmyth -lmyth-$$LIBVERSION
    
    # Duplication of source with libmyth (e.g. oldsettings.cpp)
    # means that the linker complains, so we have to ignore duplicates 
    QMAKE_LFLAGS += -multiply_defined suppress
}


TEMPLATE = app
TARGET = mythuitest
CONFIG += thread opengl

# Input
HEADERS = test1.h

SOURCES = main.cpp test1.cpp

