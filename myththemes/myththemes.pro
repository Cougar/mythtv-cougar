include ( mythconfig.mak )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files = Iulius Minimalist-wide Titivillus Titivillus-OSD isthmus

INSTALLS += themes

# Input
SOURCES += dummy.c
