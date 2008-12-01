include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path   = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythweather_es.qm mythweather_ca.qm mythweather_nl.qm
trans.files += mythweather_da.qm mythweather_pt.qm mythweather_sv.qm
trans.files += mythweather_de.qm mythweather_ja.qm mythweather_it.qm
trans.files += mythweather_fr.qm mythweather_sl.qm mythweather_fi.qm
trans.files += mythweather_ru.qm mythweather_cs.qm

INSTALLS += trans

SOURCES += dummy.c
