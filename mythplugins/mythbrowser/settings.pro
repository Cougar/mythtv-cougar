#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.16

INCLUDEPATH += $${PREFIX}/include

LIBS *= -L$${PREFIX}/lib

DEFINES += _GNU_SOURCE
DEFINES += PREFIX=\"$${PREFIX}\"
release {
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
}

# EXTRA_LIBS = 
