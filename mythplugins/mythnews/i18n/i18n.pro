include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythnews_it.qm mythnews_es.qm mythnews_ca.qm
trans.files += mythnews_nl.qm mythnews_de.qm mythnews_da.qm
trans.files += mythnews_pt.qm mythnews_sv.qm mythnews_fr.qm
trans.files += mythnews_ja.qm mythnews_sl.qm mythnews_et.qm
trans.files += mythnews_fi.qm mythnews_nb.qm mythnews_ru.qm
trans.files += mythnews_cs.qm

INSTALLS += trans

SOURCES += dummy.c
