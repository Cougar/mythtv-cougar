!include ( mythconfig.mak ) {
    error("Please run ./configure")
}

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

!macx:QMAKE_COPY_DIR = sh ./cpsvndir

themes.path = $${PREFIX}/share/mythtv/themes/
themes.files  = ProjectGrayhem ProjectGrayhem-wide
themes.files += blootube-osd blootube-wide blootubelite-wide
themes.files += neon-wide

INSTALLS += themes

# Input
SOURCES += dummy.c
