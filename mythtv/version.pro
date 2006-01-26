############################################################
# Optional qmake instructions which automatically generate #
# an extra source file containing the Subversion revision. #
#       If the directory has no .svn directories,          #
#        "exported" is reported as the revision.           #
############################################################

SVNTREEDIR = $$system(pwd)

SOURCES += version.cpp

version.target = version.cpp 
version.commands = echo 'const char *myth_source_version =' \
'"'`(svnversion $${SVNTREEDIR} 2>/dev/null) || echo Unknown`'";' > .vers.new ; \
diff .vers.new version.cpp > .vers.diff 2>&1 ; \
if test -s .vers.diff ; then mv -f .vers.new version.cpp ; fi ; \
rm -f .vers.new .vers.diff
version.depends = FORCE 

QMAKE_EXTRA_UNIX_TARGETS += version
