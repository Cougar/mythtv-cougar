include (../config.mak)
include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libavutil libavcodec libavformat libmythsamplerate
SUBDIRS += libmythsoundtouch libmythmpeg2 libmythdvdnav
SUBDIRS += libmythfreesurround libmythdb

using_mheg:SUBDIRS += libmythfreemheg
using_live:SUBDIRS += libmythlivemedia
using_hdhomerun:SUBDIRS += libmythhdhomerun

# Libraries with dependencies
SUBDIRS += libmythui libmyth libmythtv
libmythui.depends = libmythdb
libmyth.depends = libmythdb libmythui

LIBMYTHTVDEPS = $$SUBDIRS
LIBMYTHTVDEPS -= libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS

SUBDIRS += libmythupnp
libmythupnp.depends = libmythtv
