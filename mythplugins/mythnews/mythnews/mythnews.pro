include ( ../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythnews
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = news-ui.xml
installfiles.path = $${PREFIX}/share/mythtv/mythnews
installfiles.files = news-sites.xml
installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installfiles installimages uifiles

# Input
HEADERS += mythnews.h mythnewsconfig.h newsengine.h
SOURCES += main.cpp mythnews.cpp mythnewsconfig.cpp newsengine.cpp
