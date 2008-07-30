include ( ../../config.mak )
include (../../settings.pro)
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += CommDetectorFactory.h CommDetectorBase.h
HEADERS += ClassicLogoDetector.h
HEADERS += ClassicSceneChangeDetector.h
HEADERS += ClassicCommDetector.h
HEADERS += Histogram.h
HEADERS += quickselect.h
HEADERS += CommDetector2.h
HEADERS += pgm.h
HEADERS += EdgeDetector.h CannyEdgeDetector.h
HEADERS += PGMConverter.h BorderDetector.h
HEADERS += FrameAnalyzer.h
HEADERS += TemplateFinder.h TemplateMatcher.h
HEADERS += HistogramAnalyzer.h
HEADERS += BlankFrameDetector.h
HEADERS += SceneChangeDetector.h

HEADERS += LogoDetectorBase.h SceneChangeDetectorBase.h
HEADERS += SlotRelayer.h CustomEventRelayer.h

SOURCES += CommDetectorFactory.cpp CommDetectorBase.cpp
SOURCES += ClassicLogoDetector.cpp
SOURCES += ClassicSceneChangeDetector.cpp
SOURCES += ClassicCommDetector.cpp
SOURCES += Histogram.cpp
SOURCES += quickselect.c
SOURCES += CommDetector2.cpp
SOURCES += pgm.cpp
SOURCES += EdgeDetector.cpp CannyEdgeDetector.cpp
SOURCES += PGMConverter.cpp BorderDetector.cpp
SOURCES += FrameAnalyzer.cpp
SOURCES += TemplateFinder.cpp TemplateMatcher.cpp
SOURCES += HistogramAnalyzer.cpp
SOURCES += BlankFrameDetector.cpp
SOURCES += SceneChangeDetector.cpp

SOURCES += main.cpp

#The following line was inserted by qt3to4
QT += xml sql

