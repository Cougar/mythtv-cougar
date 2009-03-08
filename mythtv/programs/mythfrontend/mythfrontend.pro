include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql opengl

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += MFEXML_scpd.xml
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += playbackbox.h viewscheduled.h globalsettings.h
HEADERS += manualschedule.h programrecpriority.h channelrecpriority.h
HEADERS += statusbox.h networkcontrol.h custompriority.h
HEADERS += mediarenderer.h mythfexml.h playbackboxlistitem.h
HEADERS += mythappearance.h exitprompt.h proglist.h 
HEADERS += action.h mythcontrols.h keybindings.h keygrabber.h
HEADERS += mythosdmenueditor.h progfind.h

SOURCES += main.cpp playbackbox.cpp viewscheduled.cpp
SOURCES += globalsettings.cpp manualschedule.cpp programrecpriority.cpp
SOURCES += channelrecpriority.cpp statusbox.cpp networkcontrol.cpp
SOURCES += mediarenderer.cpp mythfexml.cpp playbackboxlistitem.cpp
SOURCES += custompriority.cpp mythappearance.cpp exitprompt.cpp proglist.cpp
SOURCES += action.cpp actionset.cpp  mythcontrols.cpp keybindings.cpp
SOURCES += keygrabber.cpp mythosdmenueditor.cpp progfind.cpp 

macx {
    mac_bundle {
        CONFIG -= console  # Force behaviour of producing .app bundle
        RC_FILE += mythfrontend.icns
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythfrontend.app
    }

    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}

using_x11:DEFINES += USING_X11
using_xv:DEFINES += USING_XV
using_ivtv:DEFINES += USING_IVTV
using_xvmc:DEFINES += USING_XVMC
using_xvmc_vld:DEFINES += USING_XVMC_VLD
using_xrandr:DEFINES += USING_XRANDR
using_opengl_vsync:DEFINES += USING_OPENGL_VSYNC
using_opengl_video:DEFINES += USING_OPENGL_VIDEO
using_vdpau:DEFINES += USING_VDPAU

using_alsa:DEFINES += USING_ALSA
using_arts:DEFINES += USING_ARTS
using_jack:DEFINES += USING_JACK
using_oss: DEFINES += USING_OSS
macx:      DEFINES += USING_COREAUDIO
