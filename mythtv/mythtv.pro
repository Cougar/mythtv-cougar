######################################################################
# Automatically generated by qmake (1.02a) Tue Jul 16 20:59:39 2002
######################################################################

TEMPLATE = subdirs
CONFIG += ordered

!exists(config.mak) {
    error(Please run the configure script first)
}
include ( config.mak )
include ( settings.pro )

# Directories
SUBDIRS += libs filters programs themes i18n bindings

# clean up config on distclean, this must be the last sub-directory
SUBDIRS += config
