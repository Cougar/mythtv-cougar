include ( ../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files += mythphone_de.qm

INSTALLS += trans

SOURCES += dummy.c
