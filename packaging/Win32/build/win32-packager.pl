#!c:/perl/bin/perl.exe -w
##############################################################################
### =file
### win32-packager.pl
###
### =location
### http://svn.mythtv.org/svn/trunk/packaging/Win32/build/win32-packager.pl
###
### =description
### Tool for automating frontend builds on MS Windows XP (and compatible)
### originally based loosely on osx-packager.pl, but now is its own beast.
###
### =examples
### win32-packager.pl -h
###      => Print usage
### win32-packager.pl   
###      => based on latest "tested" SVN trunk (ie a known-good win32 build)
### win32-packager.pl -r head
###      => based on trunk head
### win32-packager.pl -b
###      => based on release-021-fixes branch
### win32-packager.pl -b -t 
###      => include some known patches which are not accepted and needed for Win32 
### win32-packager.pl -b -t -k
###      =>Same but package and create setup.exe at the end
###
### =revision
### $Id$
###
### =author
### David Bussenschutt
##############################################################################

use strict;
use LWP::UserAgent;
use IO::File;
use Data::Dumper; 
use File::Copy qw(cp);
use Getopt::Std;
use Digest::MD5;


$| = 1; # autoflush stdout;

# this script was last tested to work with this version, on other versions YMMV.
#my $SVNRELEASE = '16789'; # This is the last version that was Qt 3 based.
                           # Qt 4 merges began immediately after.
#my $SVNRELEASE = '18442'; # Recent 0-21-fixes
my $SVNRELEASE = '19821'; # Recent trunk
#my $SVNRELEASE = 'HEAD'; # If you are game, go forth and test the latest!


# We allow SourceForge to tell us which server to download from,
# rather than assuming specific server/s
my $sourceforge = 'downloads.sourceforge.net';     # auto-redirect to a
                                                   # mirror of SF's choosing,
                                                   # hopefully close to you
# alternatively you can choose your own mirror:
#my $sourceforge = 'optusnet.dl.sourceforge.net';  # Australia 
#my $sourceforge = 'internap.dl.sourceforge.net';  # USA,California
#my $sourceforge = 'easynews.dl.sourceforge.net';  # USA,Arizona,Phoenix,
#my $sourceforge = 'jaist.dl.sourceforge.net';     # Japan
#my $sourceforge = 'mesh.dl.sourceforge.net';      # Germany
#my $sourceforge = 'transact.dl.sourceforge.net';  # Germany

# Set this to the empty string for no-proxy:
my $proxy = '';
#my $proxy = 'http://enter.your.proxy.here:8080';
# Subversion proxy settings are configured in %APPDATA%\Subversion\servers

my $NOISY   = 1;            # Set to 0 for less output to the screen
my $version = '0.22';       # Main mythtv version - used to name dlls
my $package = 0;            # Create a Win32 Distribution package? 1 for yes
my $update  = 0;            # Revert instead of checkout.
                            #  Careful, will destruct local copy!
my $compile_type = "debug"; # compile options: debug, profile or release
my $tickets = 0;            # Apply specific win32 tickets -
                            #  usually those not merged into SVN
my $dbconf = 0;             # Configure MySQL as part of the build process.
                            #  Required only for testing
my $makeclean = 0;          # Flag to make clean
my $svnlocation = "trunk";  # defaults to trunk unless -b specified
my $qtver = 3;              # default to 3 until we can test otherwise below,
                            #  do not change this here.
my $continuous = 0 ;        # by default the app pauses to notify you what 
                            #  it's about to do, -y overrides for batch usage.

##############################################################################
# get command line options
my $opt_string = 'vhu:kp:r:c:tldby';
my %opt = ();
getopts( "$opt_string", \%opt );
usage() if $opt{h};

$update     = 1       if defined $opt{u};
$package    = 1       if defined $opt{k};
$NOISY      = 1       if defined $opt{v};
$tickets    = 1       if defined $opt{t};
$proxy      = $opt{p} if defined $opt{p};
$SVNRELEASE = $opt{r} if defined $opt{r};
$dbconf     = 1       if defined $opt{d};
$makeclean  = 1       if defined $opt{l};
$continuous  = 1       if defined $opt{y};


if (defined $opt{c}) {
    $compile_type = $opt{c} if ($opt{c} eq "release") ;
    $compile_type = $opt{c} if ($opt{c} eq "profile") ;
}

if (defined $opt{b}) {
    my @num = split /\./, $version;
    $svnlocation = "branches/release-$num[0]-$num[1]-fixes";
    print "NOT using SVN 'trunk', using '$svnlocation'\n";
    
} else {
    $svnlocation = "trunk";
}

# force QT4 if we are on trunk above the point where QT4 patches were merged
if ( $svnlocation eq 'trunk' &&
     ($SVNRELEASE =~ m/^HEAD$/i || $SVNRELEASE > 16789) )
{   $qtver = 4   }

# 0.22-fixes doesn't exist yet.
if ($svnlocation eq "branches/release-0-22-fixes") {
    $version = '0.21';
    $svnlocation = "branches/release-0-21-fixes";
    $qtver = 3;
}

# Try to use parallel make
my $numCPU = $ENV{'NUMBER_OF_PROCESSORS'} or 1;
my $parallelMake = 'make';
if ( $numCPU gt 1 ) {
    # Pre-queue one extra job to keep the pipeline full:
    $parallelMake = 'make -j '. ($numCPU + 1);
}


print "Config:\n\tQT version: $qtver\n\tDLLs will be labeled as: $version\n";
if ( $numCPU gt 1 ) {
    print "\tBuilding with ", $numCPU, " processors\n";
}
print "\tSVN location is: $svnlocation\n\tSVN revision is: $SVNRELEASE\n\n";
print "Press [enter] to continue, or [ctrl]-c to exit now....\n";
getc() unless $continuous;

# this will be used to test if we the same
# basic build type/layout as previously.
my $is_same = "$compile_type-$svnlocation-$qtver.same";
$is_same =~ s#/#-#g; # don't put dir slashes in a filename! 

# this list defines the components to build , to build everything, leave as-is
my @components = ( 'mythtv', 'myththemes', 'mythplugins', 'packaging' );


# TODO - we should try to autodetect these paths, rather than assuming
#        the defaults - perhaps from environment variables like this:
#  die "must have env variable SOURCES pointing to your sources folder"
#      unless $ENV{SOURCES};
#  my $sources = $ENV{SOURCES};
# TODO - although theoretically possible to change these paths,
#        it has NOT been tested much,
#        and will with HIGH PROBABILITY fail somewhere. 
#      - Only $mingw is tested and most likely is safe to change.

# Perl compatible paths. DOS style, but forward slashes, and must end in slash:
# TIP: using paths with spaces in them is NOT supported, and will break.
#      patches welcome.
my $msys    = 'C:/MSys/1.0/';
my $sources = 'C:/MSys/1.0/sources/';
my $mingw   = 'C:/MinGW/';
my $mythtv  = 'C:/mythtv/';       # this is where the entire SVN checkout lives
                                  # so c:/mythtv/mythtv/ is the main codebase.
my $build   = 'C:/mythtv/build/'; # where 'make install' installs into

# Where is the users home? 
# Script later creates $home\.mythtv\mysql.txt
my $doshome = '';
if ( ! exists $ENV{'HOMEPATH'} || $ENV{'HOMEPATH'} eq '\\' ) {   
  $doshome = $ENV{'USERPROFILE'};  
} else {
  $doshome = $ENV{HOMEDRIVE}.$ENV{HOMEPATH};
}
my $home = $doshome; 
$home =~ s#\\#/#g;
$home =~ s/ /\\ /g;
$home .= '/'; # all paths should end in a slash


# DOS executable CMD.exe versions of the paths (for when we shell to DOS mode):
my $dosmsys    = perl2dos($msys);
my $dossources = perl2dos($sources);
my $dosmingw   = perl2dos($mingw); 
my $dosmythtv  = perl2dos($mythtv);

# Unix/MSys equiv. versions of the paths (for when we shell to MSYS/UNIX mode):
my $unixmsys  = '/';       # MSys root is always mounted here,
                           # irrespective of where DOS says it really is.
my $unixmingw = '/mingw/'; # MinGW is always mounted here under unix,
                           # if you setup mingw right in msys,
                           # so we will usually just say /mingw in the code,
                           # not '.$unixmingw.' or similar (see /etc/fstab)
my $unixsources = perl2unix($sources);
my $unixmythtv  = perl2unix($mythtv);
my $unixhome    = perl2unix($home);
my $unixbuild   = perl2unix($build);

# The installer for MinGW:
my $MinGWinstaller = 'MinGW-5.1.4.exe';
my $installMinGW   = $dossources.$MinGWinstaller;


#NOTE: IT'S IMPORTANT that the PATHS use the correct SLASH-ing method for
#the type of action:
#      for [exec] actions, use standard DOS paths, with single BACK-SLASHES
#      '\' (unless in double quotes, then double the backslashes)
#      for [shell] actions, use standard UNIX paths, with single
#      FORWARD-SLASHES '/'
#
#NOTE: when referring to variables in paths, try to keep them out of double
#quotes, or the slashing can get confused:
#      [exec]   actions should always refer to  $dosXXX path variables
#      [shell]  actions should always refer to $unixXXX path  variables
#      [dir],[file],[mkdirs],[archive] actions should always refer to
#      default perl compatible paths
# NOTE:  The architecture of this script is based on cause-and-event.  
#        There are a number of "causes" (or expectations) that can trigger
#        an event/action.
#        There are a number of different actions that can be taken.
#
# eg: [ dir  => "c:/MinGW", exec => $dossources.'MinGW-5.1.4.exe' ],
#
# means: expect there to be a dir called "c:/MinGW", and if there isn't
# execute the file MinGW-5.1.4.exe. (clearly there needs to be a file
# MinGW-5.1.4.exe on disk for that to work, so there is an earlier
# declaration to 'fetch' it)


#build expectations (causes) :
#  missing a file (given an expected path)                           [file]
#  missing folder                                                    [dir]
#  missing source archive (version of 'file' to fetch from the web)  [archive]
#  apply a perl pattern match and if it DOESNT match execute action  [grep]
#  - this 'cause' actually needs two parameters in an array
#    [ pattern, file].  If the file is absent, the pattern
#    is assumed to not match (and emits a warning).
#  test the file/s are totally the same (by size and mtime)          [filesame]
#  - if first file is non-existant then that's permitted,
#    it causes the action to trigger.
#  test the first file is newer(mtime) than the second one           [newer]
#  - if given 2 existing files, not necessarily same size/content,
#    and the first one isn't newer, execute the action!. 
#    If the first file is ABSENT, run the action too.
#  execute the action only if a file or directory exists             [exists]
#  stop the run, useful for script debugging                         [stop]
#  pause the run, await a enter                                      [pause]
#  always execute the action  (try to minimise the use of this!)     [always]

#build actions (events) are:
#  fetch a file from the web (to a location)                         [fetch]
#  execute a DOS/Win32 exe/command and wait to complete              [exec]
#  execute a MSYS/Unix script/command in bash and wait to complete   [shell]
#  - this 'effect' actually accepts many parameters in an array
#    [ cmd1, cmd2, etc ]
#  extract a .tar .tar.gz or .tar.bz2 or ,zip file ( to a location)  [extract]
#  - (note that .gz and .bz2 are thought equivalent)
#  write a small patch/config/script file directly to disk           [write]
#  make directory tree upto the path specified                       [mkdirs]
#  copy a new version of a file, set mtime to the original           [copy] 

#TODO:
#  copy a set of files (path/filespec,  destination)               not-yet-impl
#   =>  use exec => 'copy /Y xxx.* yyy'
#  apply a diff                                                    not-yet-impl
#   =>   use shell => 'patch -p0 < blah.patch'
#  search-replace text in a file                                   not-yet-impl
#   =>   use grep => ['pattern',subject],
#                     exec => shell 'patch < etc to replace it'


# NOTES on specific actions: 
# 'extract' now requires all paths to be perl compatible (like all other
# commands) If not supplied, it extracts into the folder the .tar.gz is in.
# 'exec' actually runs all your commands inside a single cmd.exe
# command-line. To link commands use '&&'
# 'shell' actually runs all your commands inside a bash shell with -c "(
# cmd;cmd;cmd )" so be careful about quoting.


#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
# DEFINE OUR EXPECTATIONS and the related ACTIONS:
#  - THIS IS THE GUTS OF THE APPLICATION!
#  - A SET OF DECLARATIONS THAT SHOULD RESULT IN A WORKING WIN32 INSTALATION
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------

my $expect;

push @{$expect},

[ dir => [$sources] , 
  mkdirs  => [$sources], 
  comment => 'We download all the files from the web, and save them here:'],


[ archive => $sources.$MinGWinstaller, 
  'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/'.$MinGWinstaller,
  comment => 'Get mingw and addons first, or we cant do [shell] requests!' ],
[ archive => $sources.'mingw-utils-0.3.tar.gz', 
  'fetch' => 'http://'.$sourceforge.
              '/sourceforge/mingw/mingw-utils-0.3.tar.gz' ],


[ dir     => $mingw, 
  exec    => $installMinGW,
  comment => 'install MinGW (be sure to install g++, g77 and ming '.
             'make too. Leave the default directory at c:\mingw ) '.
             '- it will require user interaction, '.
             'but once completed, will return control to us....' ],

# interactive, supposed to install g++ and ming make too,
# but people forget to select them? 

[ file    => $mingw."bin/gcc.exe", 
  exec    => $installMinGW,
  comment => 'unable to find a gcc.exe where expected, '.
             'rerunning MinGW installer!' ], 

[ archive => $sources.'MSYS-1.0.10.exe', 
  'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw/MSYS-1.0.10.exe',
  comment => 'Get the MSYS and addons:' ] ,
[ archive => $sources.'bash-3.1-MSYS-1.0.11-1.tar.bz2', 
  'fetch' => 'http://'.$sourceforge.
             '/sourceforge/mingw/bash-3.1-MSYS-1.0.11-1.tar.bz2' ] ,
[ archive => $sources.'zlib-1.2.3-MSYS-1.0.11.tar.bz2', 
  'fetch' => 'http://easynews.dl.sourceforge.net'.
             '/sourceforge/mingw/zlib-1.2.3-MSYS-1.0.11.tar.bz2' ] ,
[ archive => $sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2', 
  'fetch' => 'http://'.$sourceforge.'/sourceforge/mingw'.
             '/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2' ] ,

# install MSYS, it supplies the 'tar' executable, among others:
[ file    => $msys.'bin/tar.exe', 
  exec    => $dossources.'MSYS-1.0.10.exe',
  comment => 'Install MSYS, it supplies the tar executable, among others. You '.
             'should follow prompts, AND do post-install in DOS box.' ] , 

#  don't use the [shell] or [copy] actions here,
# as neither are avail til bash is installed!
[ file    => $msys.'bin/sh2.exe', 
  exec    => 'copy /Y '.$dosmsys.'bin\sh.exe '.$dosmsys.'bin\sh2.exe',
  comment => 'make a copy of the sh.exe so that we can '.
             'utilise it when we extract later stuff' ],

# prior to this point you can't use the 'extract' 'copy' or 'shell' features!

# if you did a default-install of MingW, then you need to try again, as we
# really need g++ and mingw32-make, and g77 is needed for fftw
[ file    => $mingw.'bin/mingw32-make.exe',  
  exec    => $installMinGW,
  comment => 'Seriously?  You must have done a default install of MinGW.  '.
             'go try again! You MUST choose the custom installed and select '.
             'the mingw make, g++ and g77 optional packages.' ],
[ file    => $mingw.'bin/g++.exe', 
  exec    => $installMinGW,
  comment => 'Seriously?  You must have done a default install of MinGW.  '.
             'go try again! You MUST choose the custom installed and select '.
             'the mingw make, g++ and g77 optional packages.' ],
[ file    => $mingw.'bin/g77.exe', 
  exec    => $installMinGW,
   comment => 'Seriously?  You must have done a default install of MinGW.  '.
              'go try again! You MUST choose the custom installed and select '.
              'the mingw make, g++ and g77 optional packages.' ],

#[ file => 'C:/MinGW/bin/mingw32-make.exe',  extract =>
#$sources.'mingw32-make-3.81-2.tar',"C:/MinGW" ], - optionally we could get
#mingw32-make from here

# now that we have the 'extract' feature, we can finish ...
[ file    => $mingw.'/bin/reimp.exe',  
  extract => [$sources.'mingw-utils-0.3.tar', $mingw],
  comment => 'Now we can finish all the mingw and msys addons:' ],
[ file    => $msys.'bin/bash.exe',  
  extract => [$sources.'bash-3.1-MSYS-1.0.11-1.tar', $msys] ],
[ dir     => $sources.'coreutils-5.97',  
  extract => [$sources.'coreutils-5.97-MSYS-1.0.11-snapshot.tar'] ],
[ file    => $msys.'bin/pr.exe', 
  shell   => ["cd ".$unixsources."coreutils-5.97","cp -r * / "] ],

[ dir     => $msys."lib" ,  mkdirs => $msys.'lib' ],
[ dir     => $msys."include" ,  mkdirs => $msys.'include' ],

#get gdb
[ archive => $sources.'gdb-6.7.50.20071127-mingw.tar.bz2', 
  'fetch' => 'http://'.$sourceforge.
             '/sourceforge/mingw/gdb-6.7.50.20071127-mingw.tar.bz2',
  comment => 'Get gdb for possible debugging later' ],
[ file    => $msys.'bin/gdb.exe',  
  extract => [$sources.'gdb-6.7.50.20071127-mingw.tar.bz2', $msys] ],


# (alternate would be from the gnuwin32 project,
#  which is actually from same source)
#  run it into a 'unzip' folder, because it doesn't extract to a folder:
[ dir     => $sources."unzip" ,  
  mkdirs  => $sources.'unzip',
  comment => 'unzip.exe - Get a precompiled '.
             'native Win32 version from InfoZip' ],
[ archive => $sources.'unzip/unz552xN.exe',  
  'fetch' => 'ftp://tug.ctan.org/tex-archive'.
             '/tools/zip/info-zip/WIN32/unz552xN.exe'],
[ file    => $sources.'unzip/unzip.exe', 
  exec    => 'chdir '.$dossources.'unzip && '.
             $dossources.'unzip/unz552xN.exe' ],
# we could probably put the unzip.exe into the path...


# we now use SVN 1.5.x
[ archive => $sources.'svn-win32-1.5.1.zip',  
  'fetch' => 'http://subversion.tigris.org/files/'.
             'documents/15/43251/svn-win32-1.5.1.zip',
  comment => 'Subversion comes as a zip file, so it '.
             'cant be done earlier than the unzip tool!'],
[ dir     => $sources.'svn-win32-1.5.1', 
  extract => $sources.'svn-win32-1.5.1.zip' ],


[ file    => $msys.'bin/svn151_.exe', 
  shell   => [ "cp -R $unixsources/svn-win32-1.5.1/* ".$unixmsys,
               "cp $unixsources/svn-win32-1.5.1/bin/svn.exe ".
                   $unixmsys."bin/svn151.exe",
               "cp $unixsources/svn-win32-1.5.1/bin/svn.exe ".
                   $unixmsys."bin/svn151_.exe",
               "cp $unixsources/svn-win32-1.5.1/bin/intl3_svn.dll ".
                   $unixmsys."bin"
             ],
  comment => 'put the svn.exe executable into the path, '.
             'so we can use it easily later!' ],

# :
[ dir     => $sources."zlib" ,  
  mkdirs  => $sources.'zlib',
  comment => 'the zlib download is a bit messed-up, and needs some TLC '.
             'to put everything in the right place' ],
[ dir     => $sources."zlib/usr",  
  extract => [$sources.'zlib-1.2.3-MSYS-1.0.11.tar', $sources."zlib"] ],
# install to /usr:
[ file    => $msys.'lib/libz.a',      
  exec    => ["copy /Y ".$dossources.'zlib\usr\lib\* '.$dosmsys."lib"] ], 
[ file    => $msys.'bin/msys-z.dll',  
  exec    => ["copy /Y ".$dossources.'zlib\usr\bin\* '.$dosmsys."bin"] ],
[ file    => $msys.'include/zlib.h',  
  exec    => ["copy /Y ".$dossources.'zlib\usr\include\* '.
              $dosmsys."include"] ],
# taglib also requires zlib in /mingw , so we'll put it there too, why not! 
[ file    => $mingw.'lib/libz.a',     
  exec    => ["copy /Y ".$dossources.'zlib\usr\lib\* '.$dosmingw."lib"] ],
[ file    => $mingw.'bin/msys-z.dll', 
  exec    => ["copy /Y ".$dossources.'zlib\usr\bin\* '. $dosmingw."bin"] ],
[ file    => $mingw.'include/zlib.h', 
  exec    => ["copy /Y ".$dossources.'zlib\usr\include\* '.
              $dosmingw."include"] ],

# fetch mysql
# primary server site is:
# http://dev.mysql.com/get/Downloads/MySQL-5.0/
#     mysql-essential-5.1.30-win32.msi/from/http://mysql.mirrors.ilisys.com.au/
# alternate: http://mysql.mirrors.ilisys.com.au/Downloads/MySQL-5.1/
#     mysql-essential-5.1.30-win32.msi
[ archive => $sources.'mysql-essential-5.1.30-win32.msi', 
  'fetch' => 'http://mysql.mirrors.ilisys.com.au/Downloads/'.
             'MySQL-5.1/mysql-essential-5.1.30-win32.msi',
  comment => 'fetch mysql binaries - this is a big download(23MB) '.
             'so it might take a while' ],
[ file    => "c:/Program Files/MySQL/MySQL Server 5.1/bin/libmySQL.dll",
  exec    => $dossources.'mysql-essential-5.1.30-win32.msi INSTALLLEVEL=2',
  comment => 'Install mysql - be sure to choose to do a "COMPLETE" install. '.
             'You should also choose NOT to "configure the server now" ' ],

# after mysql install 
[ filesame => [$mingw.'bin/libmySQL.dll',
               'c:/Program Files/MySQL/MySQL Server 5.1/bin/libmySQL.dll'],  
  copy     => [''=>'',
  comment  => 'post-mysql-install'] ],
[ filesame => [$mingw.'lib/libmySQL.dll',
               'c:/Program Files/MySQL/MySQL Server 5.1/bin/libmySQL.dll'],  
  copy     => [''=>'',
  comment  => 'post-mysql-install'] ],
[ filesame => [$mingw.'lib/libmysql.lib',
               'c:/Program Files/MySQL/MySQL Server 5.1/lib/opt/libmysql.lib'],
  copy     => [''=>''] ],
[ file     => $mingw.'include/mysql.h'  ,   
  exec     => 'copy /Y "c:\Program Files\MySQL\MySQL Server 5.1\include\*" '.
              $dosmingw."include" ],
  
  
# make sure that /mingw is mounted in MSYS properly before trying
# to use the /mingw folder.  this is supposed to happen as part
# of the MSYS post-install, but doesnt always work.
[ file    => $msys.'etc/fstab', 
  write   => [$msys.'etc/fstab',
"$mingw /mingw
" ],
  comment => 'correct a MinGW bug that prevents the /etc/fstab from existing'],

#
# TIP: we use a special file (with two extra _'s )
#      as a marker to say this acton is already done! 
[ file    => $mingw.'lib/libmysql.lib__',
  shell   => ["cd /mingw/lib","reimp -d libmysql.lib",
           "dlltool -k --input-def libmysql.def --dllname libmysql.dll".
           " --output-lib libmysql.a",
           "touch ".$unixmingw.'lib/libmysql.lib__'],
  comment => ' rebuild libmysql.a' ],

# grep    => [pattern,file] , actions/etc
[ file    => $mingw.'include/mysql___h.patch', 
  write   => [$mingw.'include/mysql___h.patch',
'--- mysql.h_orig	Fri Jan  4 19:35:33 2008
+++ mysql.h	Tue Jan  8 14:48:36 2008
@@ -41,11 +41,9 @@

 #ifndef _global_h				/* If not standard header */
 #include <sys/types.h>
-#ifdef __LCC__
 #include <winsock.h>				/* For windows */
-#endif
 typedef char my_bool;
-#if (defined(_WIN32) || defined(_WIN64)) && !defined(__WIN__)
+#if (defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__)) && !defined(__WIN__)
 #define __WIN__
 #endif
 #if !defined(__WIN__)
 
 ' ],comment => 'write the patch for the the mysql.h file'],
# apply it!?
[ grep    => ['\|\| defined\(__MINGW32__\)',$mingw.'include/mysql.h'], 
  shell   => ["cd /mingw/include","patch -p0 < mysql___h.patch"],
  comment => 'Apply mysql.h patch file, if not already applied....' ],


# apply sspi.h patch
[ file    => $mingw.'include/sspi_h.patch', 
  write   => [$mingw.'include/sspi_h.patch',
'*** sspi.h      Sun Jan 25 17:55:57 2009
--- sspi.h.new  Sun Jan 25 17:55:51 2009
***************
*** 8,13 ****
--- 8,15 ----
  extern "C" {
  #endif
  
+ #include <subauth.h>
+ 
  #define SECPKG_CRED_INBOUND 1
  #define SECPKG_CRED_OUTBOUND 2
  #define SECPKG_CRED_BOTH (SECPKG_CRED_OUTBOUND|SECPKG_CRED_INBOUND)

' ],comment => 'write the patch for the the sspi.h file'],
# apply it!?
[ grep    => ['subauth.h',$mingw.'include/sspi.h'], 
  shell   => ["cd /mingw/include","patch -p1 < sspi_h.patch"],
  comment => 'Apply sspi.h patch file, if not already applied....' ],

#[ pause => 'check  patch.... press [enter] to continue !'],

## apply stdlib.h patch
#[ file    => $mingw.'include/stdlib_h.patch', 
#  write   => [$mingw.'include/stdlib_h.patch',
#'--- include/stdlib.h.org	Thu Dec  4 11:11:40 2008
#+++ include/stdlib.h	Thu Dec  4 11:12:46 2008
#@@ -314,7 +314,7 @@
# #else
# static
# #endif /* Not __cplusplus */
#-inline double __cdecl __MINGW_NOTHROW strtod (const char* __restrict__ __nptr, char** __restrict__ __endptr)
#+__inline__ double __cdecl __MINGW_NOTHROW strtod (const char* __restrict__ __nptr, char** __restrict__ __endptr)
# { return __strtod(__nptr, __endptr); }
# float __cdecl __MINGW_NOTHROW strtof (const char * __restrict__, char ** __restrict__);
# long double __cdecl __MINGW_NOTHROW strtold (const char * __restrict__, char ** __restrict__);
#' ],comment => 'write the patch for the the stdlib.h file'],
## apply it!?
#[ grep    => ['__inline__ double __cdecl __MINGW_NOTHROW strtod ',$mingw.'include/stdlib.h'], 
#  shell   => ["cd /mingw/include","patch -p1 < stdlib_h.patch"],
#  comment => 'Apply sspi.h patch file, if not already applied....' ],
#
#[ pause => 'check  patch.... press [enter] to continue !'],

# fetch it
[ dir     => $sources.'pthread', 
  mkdirs  => $sources.'pthread' ],
[ archive => $sources.'pthread/libpthread.a',   
  'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/'.
             'dll-latest/lib/libpthreadGC2.a',
  comment => 'libpthread is precompiled, we just download it to the right place'
],
[ archive => $sources.'pthread/pthreadGC2.dll', 
  'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/'.
             'dll-latest/lib/pthreadGC2.dll' ],
[ filesame => [$sources.'pthread/pthread.dll',
               $sources."pthread/pthreadGC2.dll"],
  copy    => [''=>''] ],
[ archive => $sources.'pthread/pthread.h',  
  'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/'.
             'dll-latest/include/pthread.h' ],
[ archive => $sources.'pthread/sched.h',    
  'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/'.
             'dll-latest/include/sched.h' ],
[ archive => $sources.'pthread/semaphore.h',
  'fetch' => 'ftp://sources.redhat.com/pub/pthreads-win32/'.
             'dll-latest/include/semaphore.h' ],
# install it:
[ filesame => [$mingw.'lib/libpthread.a',   $sources."pthread/libpthread.a"],  
  copy     => [''=>'',
  comment  => 'install pthread'] ],
[ filesame => [$mingw.'bin/pthreadGC2.dll', $sources."pthread/pthreadGC2.dll"],
  copy     => [''=>''] ],
[ filesame => [$mingw.'bin/pthread.dll',    $sources."pthread/pthread.dll"],   
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/pthread.h',  $sources."pthread/pthread.h"],     
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/sched.h',    $sources."pthread/sched.h"],       
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/semaphore.h',$sources."pthread/semaphore.h"],   
  copy     => [''=>''] ],


# apply sched.h patch
[ always    => $mingw.'include/sched_h.patch', 
  write   => [$mingw.'include/sched_h.patch',
"--- include/sched.h.org	Thu Dec  4 12:00:16 2008
+++ include/sched.h	Wed Dec  3 13:42:54 2008
@@ -124,8 +124,16 @@
 typedef int pid_t;
 #endif
 
-/* Thread scheduling policies */
+/* pid_t again! */
+#if defined(__MINGW32__) || defined(_UWIN)
+/* Define to `int' if <sys/types.h> does not define. */
+/* GCC 4.x reportedly defines pid_t. */
+#ifndef _PID_T_
+#define pid_t int
+#endif
+#endif
 
+/* Thread scheduling policies */
 enum {
   SCHED_OTHER = 0,
   SCHED_FIFO,
" ],comment => 'write the patch for the the sched.h file'],
# apply it!?
[ grep    => ['GCC 4.x reportedly defines pid_t',$mingw.'include/sched.h'], 
  shell   => ["cd /mingw/include","patch -p1 < sched_h.patch"],
  comment => 'Apply sched.h patch file, if not already applied....' ],
# apply it to the sources we downloaded too, so that we don't try to copy the 
#  original file over the new one again, next time!
[ grep    => ['GCC 4.x reportedly defines pid_t',$sources."pthread/sched.h"], 
  shell   => ["cd ".$unixsources."pthread",
              "patch -p1 < ".$unixmingw.'include/sched_h.patch'],
  comment => 'Apply sched.h patch file (twice!) , if not already applied....' ],


#[ pause => 'check  patch.... press [enter] to continue !'],


#   ( save bandwidth compare to the above full SDK where they came from:
[ archive  => $sources.'DX9SDK_dsound_Include_subset.zip', 
  'fetch'  => 'http://davidbuzz.googlepages.com/'.
              'DX9SDK_dsound_Include_subset.zip',
  comment  => 'We download just the required Include files for DX9' ], 
[ dir      => $sources.'DX9SDK_dsound_Include_subset', 
  extract  => $sources.'DX9SDK_dsound_Include_subset.zip' ],
[ filesame => [$mingw.'include/dsound.h',$sources.
               "DX9SDK_dsound_Include_subset/dsound.h"], 
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/dinput.h',$sources.
               "DX9SDK_dsound_Include_subset/dinput.h"], 
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/ddraw.h', $sources.
               "DX9SDK_dsound_Include_subset/ddraw.h"],  
  copy     => [''=>''] ],
[ filesame => [$mingw.'include/dsetup.h',$sources.
               "DX9SDK_dsound_Include_subset/dsetup.h"], 
  copy     => [''=>''] ],
;

# if packaging is selected, get innosetup
if ($package == 1) {
  push @{$expect},
 [ archive => $sources.'isetup-5.2.3.exe',
     fetch => 'http://files.jrsoftware.org/ispack/ispack-5.2.3.exe',
   comment => 'fetch inno setup setup' ],
 [ file    => "C:/Program Files/Inno Setup 5/iscc.exe",
   exec    => $dossources.'isetup-5.2.3.exe', #/silent is broken! 
   comment => 'Install innosetup - install ISTool, ',
              'ISSP, AND encryption support.' ],
 # Get advanced uninstall
 [ archive => $sources.'UninsHs.rar',
     fetch => 'http://www.uninshs.com/down/UninsHs.rar',
   comment => 'fetch uninstall for innosetup' ],
 [ archive => $sources.'unrar-3.4.3-bin.zip',
     fetch => 'http://downloads.sourceforge.net/gnuwin32/unrar-3.4.3-bin.zip',
   comment => 'fetching unrar'],
 [ file    => $sources.'bin/unrar.exe',
  shell    => ["cd ".$sources, "unzip/unzip.exe unrar-3.4.3-bin.zip"]],
 [ file    => 'C:/Program Files/Inno Setup 5/UninsHs.exe',
  shell    => ['cd /c/program\ files/inno\ setup\ 5',
              $sources.'bin/unrar.exe e '.$sources.'UninsHs.rar'],
  comment  => 'Install innosetup' ],
 [ archive => $sources.'istool-5.2.1.exe',
     fetch => 'http://downloads.sourceforge.net/sourceforge'.
              '/istool/istool-5.2.1.exe',
   comment => 'fetching istool' ],
 [ file    => "c:/Program Files/ISTool/isxdl.dll",
   exec    => $dossources.'istool-5.2.1.exe /silent',
   comment => 'Install istool'],
 [ archive => $sources.'logo_mysql_sun.gif',
     fetch => 'http://www.mysql.com/common/logos/logo_mysql_sun.gif',
   comment => 'Download MySQL logo for an install page in the package' ],
 [ exists  => $mythtv.'build/package_flag',
    shell  => ["rm ".$unixmythtv."build/package_flag"],
   comment => '' ],
;
}


#----------------------------------------
# now we do each of the source library dependencies in turn:
# download,extract,build/install
# TODO - ( and just pray that they all work?)  These should really be more
# detailed, and actually check that we got it installed properly.
# Most of these look for a Makefile as a sign that the ./configure was
# successful (not necessarily true, but it's a start) but this requires that
# the .tar.gz didn't come with a Makefile in it.

# Most of these look for a Makefile as a sign that the
# ./configure was successful (not necessarily true, but it's a start)
# but this requires that the .tar.gz didn't come with a Makefile in it.
push @{$expect},
[ archive => $sources.'freetype-2.3.5.tar.gz',  
  fetch   => 'http://download.savannah.nongnu.org'.
             'releases/freetype/freetype-2.3.5.tar.gz'],
[ dir     => $sources.'freetype-2.3.5', 
  extract => $sources.'freetype-2.3.5.tar' ],
# caution... freetype comes with a Makefile in the .tar.gz, so work around it!
[ file    => $sources.'freetype-2.3.5/Makefile_', 
  shell   => ["cd $unixsources/freetype-2.3.5",
              "./configure --prefix=/mingw",
              "touch $unixsources/freetype-2.3.5/Makefile_"],
  comment => 'building freetype' ],
              
# here's an example of specifying the make and make install steps separately, 
# for apps that can't be relied on to have the make step work!
[ file    => $sources.'freetype-2.3.5/objs/.libs/libfreetype.a', 
  shell   => ["cd $unixsources/freetype-2.3.5",
              "make"],
  comment => 'checking freetype' ],
[ file    => $mingw.'lib/libfreetype.a', 
  shell   => ["cd $unixsources/freetype-2.3.5",
              "make install"],
  comment => 'installing freetype' ],
[ file    => $mingw.'bin/libfreetype-6.dll', 
  shell   => ["cp $unixsources/freetype-2.3.5/objs/.libs/libfreetype-6.dll ".
              "$mingw/bin/"] ],

#eg: http://transact.dl.sourceforge.net/sourceforge/lame/lame-398-2.tar.gz
[ archive => $sources.'lame-398-2.tar.gz',  
  fetch   => 'http://'.$sourceforge.'/sourceforge/lame/lame-398-2.tar.gz'],
[ dir     => $sources.'lame-398-2', 
  extract => $sources.'lame-398-2.tar' ],
[ file    => $mingw.'lib/libmp3lame.a', 
  shell   => ["cd $unixsources/lame-398-2",
              "./configure --prefix=/mingw",
              "make",
              "make install"],
  comment => 'building and installing: mingw lame' ],
[ file    => $msys.'lib/libmp3lame.a', 
  shell   => ["cd $unixsources/lame-398-2",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: msys lame' ],

# confirmed latest version as at 26-12-2008:
[ archive => $sources.'libmad-0.15.1b.tar.gz',  
  fetch   => 'http://'.$sourceforge.'/sourceforge/mad/libmad-0.15.1b.tar.gz'],
[ dir     => $sources.'libmad-0.15.1b', 
  extract => $sources.'libmad-0.15.1b.tar' ],
[ file    => $msys.'lib/libmad.a', 
  shell   => ["cd $unixsources/libmad-0.15.1b",
              "./configure --prefix=/usr",
              "make",
             "make install"],
  comment => 'building and installing: msys libmad' ],

[ archive => $sources.'libmad-0.15.1b.tar.gz',  
  fetch   => 'http://'.$sourceforge.'/sourceforge/mad/libmad-0.15.1b.tar.gz'],
[ dir     => $sources.'libmad-0.15.1b', 
  extract => $sources.'libmad-0.15.1b.tar' ],
[ file    => $mingw.'lib/libmad.a', 
  shell   => ["cd $unixsources/libmad-0.15.1b",
              "./configure --prefix=/mingw",
              "make",
              "make install"],
  comment => 'building and installing: mingw libmad' ],
              
# taglib 1.5 sources changed it's build system under win32 to use 'cmake', 
# which we don't have, however pre-compiled mingw 1.5 binaries are available:
[ archive => $sources.'taglib-1.5-mingw-bin.zip',  
  fetch   => 'http://ftp.musicbrainz.org/pub/musicbrainz'.
             '/users/luks/taglib/taglib-1.5-mingw-bin.zip'],
[ dir     => $sources.'taglib-1.5-mingw-bin', 
  extract => $sources.'taglib-1.5-mingw-bin.zip' ],
[ file    => $mingw.'lib/libtag.dll.a', 
  shell   => ['cd '.$sources.'taglib-1.5-mingw-bin',
              "cp -vr * $unixmingw"],
  comment => 'installing: mingw taglib' ],
[ file    => $msys.'lib/libtag.dll.a', 
  shell   => ['cd '.$sources.'taglib-1.5-mingw-bin',
              "cp -vr * $unixmsys"],
  comment => 'installing: msys taglib' ],
              
# NOTE: --disable-fast-perl fixes makefiles that otherwise have bits like below:
# INSTALL = ../C:/msys/1.0/bin/install -c -p
# INSTALL = ../../C:/msys/1.0/bin/install -c -p

# confirmed latest version as at 26-12-2008:
[ archive => $sources.'libao-0.8.8.tar.gz',  
  fetch   => 'http://downloads.xiph.org/releases/ao/libao-0.8.8.tar.gz'],
[ dir     => $sources.'libao-0.8.8', 
  extract => $sources.'libao-0.8.8.tar' ],
[ file    => $msys.'bin/libao-2.dll',  
  # test completion of LAST step (ie make install), not the first one.
  shell   => ["cd $unixsources/libao-0.8.8",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: libao' ],

# confirmed latest version as at 26-12-2008:
# definitely need mingw version of ogg for plugins,
# and for mingw vorbis to build!
[ archive => $sources.'libogg-1.1.3.tar.gz',  
  fetch   => 'http://downloads.xiph.org/releases/ogg/libogg-1.1.3.tar.gz'],
[ dir     => $sources.'libogg-1.1.3', 
  extract => $sources.'libogg-1.1.3.tar' ],
[ file    => $msys.'bin/libogg-0.dll', 
  shell   => ["cd $unixsources/libogg-1.1.3",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: msys libogg' ],

#  need msys version of ogg for msys version of vorbis!
[ archive => $sources.'libogg-1.1.3.tar.gz',  
  fetch   => 'http://downloads.xiph.org/releases/ogg/libogg-1.1.3.tar.gz'],
[ dir     => $sources.'libogg-1.1.3', 
  extract => $sources.'libogg-1.1.3.tar' ],
[ file    => $mingw.'bin/libogg-0.dll', 
  shell   => ["cd $unixsources/libogg-1.1.3",
              "./configure --prefix=/mingw",
              "make",
              "make install"],
  comment => 'building and installing: mingw libogg' ],

# confirmed latest version as at 26-12-2008:
[ archive => $sources.'libvorbis-1.2.0.tar.gz',  
  fetch   => 'http://downloads.xiph.org/releases/'.
             'vorbis/libvorbis-1.2.0.tar.gz'],
[ dir     => $sources.'libvorbis-1.2.0', 
  extract => $sources.'libvorbis-1.2.0.tar' ],
[ file    => $msys.'lib/libvorbis.a', 
  shell   => ["cd $unixsources/libvorbis-1.2.0",
              "./configure --prefix=/usr --disable-shared",
              "make",
              "make install"],
  comment => 'building and installing: msys libvorbis' ],

# definitely need mingw version for mythtv plugins to build
[ archive => $sources.'libvorbis-1.2.0.tar.gz',  
  fetch   => 'http://downloads.xiph.org/releases/'.
             'vorbis/libvorbis-1.2.0.tar.gz'],
[ dir     => $sources.'libvorbis-1.2.0', 
  extract => $sources.'libvorbis-1.2.0.tar' ],
[ file    => $mingw.'lib/libvorbis.a', 
  shell   => ["cd $unixsources/libvorbis-1.2.0",
              "./configure --prefix=/mingw --disable-shared",
              "make",
              "make install"],
  comment => 'building and installing: mingw libvorbis' ],

# confirmed latest source version as at 26-12-2008 
#( flac-win binary packages are unsuitable in this case)
[ archive => $sources.'flac-1.2.1.tar.gz',  
  fetch   => 'http://'.$sourceforge.'/sourceforge/flac/flac-1.2.1.tar.gz'],
[ dir     => $sources.'flac-1.2.1', 
  extract => $sources.'flac-1.2.1.tar' ],
[ grep    => ['\#define SIZE_T_MAX UINT_MAX',$mingw.'include/limits.h'], 
  shell   => "echo \\#define SIZE_T_MAX UINT_MAX >> $mingw/include/limits.h" ], 
[ file    => $mingw.'lib/libFLAC.a', 
  shell   => ["cd $unixsources/flac-1.2.1",
              "./configure --prefix=/mingw",
              "make",
              "make install"],
  comment => 'building and installing: mingw flac/FLAC' ],
[ file    => $msys.'lib/libFLAC.a', 
  shell   => ["cd $unixsources/flac-1.2.1",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: msys flac/FLAC' ],
 
 
[ archive => $sources.'libcdaudio-0.99.12p2.tar.gz',
  fetch   => 'http://'.$sourceforge.
             '/sourceforge/libcdaudio/libcdaudio-0.99.12p2.tar.gz'],
[ dir     => $sources.'libcdaudio-0.99.12p2', 
  extract => $sources.'libcdaudio-0.99.12p2.tar' ],

[ archive => $sources.'libcdaudio-0.99.12p2-WIN32-support.patch',  
  fetch   => 'http://sourceforge.net/tracker/download.php?'.
             'group_id=27134&atid=389444&file_id=286748&aid=2035008'],
  
[ grep => ['ifdef __WIN32', $sources.'libcdaudio-0.99.12p2/src/cddb.c'], 
  shell => ["cd ".$sources.'libcdaudio-0.99.12p2',
            "patch -p1 < ".$unixsources
            .'libcdaudio-0.99.12p2-WIN32-support.patch'] ],
 
[ file    => $msys.'bin/libcdaudio-config', 
  shell   => ["cd ".$unixsources."libcdaudio-0.99.12p2",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing:  msys libcdaudio' ],  

# mingw version is needed, or mythmusic won't build
[ file    => $mingw.'bin/libcdaudio-config', 
  shell   => ["cd ".$unixsources."libcdaudio-0.99.12p2",
              "./configure --prefix=/mingw",
              "make",
              "make install"],
  comment => 'building and installing:  mingw libcdaudio' ],  
  
#[ pause => 'flac, taglib, lame all done.... press [enter] to continue !'],

# skip doing pthreads from source, we use binaries that were fetched earlier:
#[ archive => $sources.'pthreads-w32-2-8-0-release.tar.gz',  
#  fetch => 'ftp://sourceware.org/pub/pthreads-win32/'.
#           'pthreads-w32-2-8-0-release.tar.gz'],
#[ dir => $sources.'pthreads-w32-2-8-0-release', 
#  extract => $sources.'pthreads-w32-2-8-0-release.tar' ],

# confirmed latest source version as at 26-12-2008:
[ archive => $sources.'SDL-1.2.13.tar.gz',  
  fetch   => 'http://www.libsdl.org/release/SDL-1.2.13.tar.gz'],
[ dir     => $sources.'SDL-1.2.13', 
  extract => $sources.'SDL-1.2.13.tar.gz' ],
[ file    => $sources.'SDL-1.2.13/Makefile', 
  shell   => ["cd $unixsources/SDL-1.2.13",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: SDL' ],

#  as at 26-12-2008 3.8.2 is latest stable. 
# NOTE: 3.9.0 and 4.0.0 are in beta and alpha respectively.
[ archive => $sources.'tiff-3.8.2.tar.gz',  
  fetch   => 'ftp://ftp.remotesensing.org/pub/libtiff/tiff-3.8.2.tar.gz'],
[ dir     => $sources.'tiff-3.8.2', 
  extract => $sources.'tiff-3.8.2.tar' ],
[ file    => $sources.'tiff-3.8.2/Makefile', 
  shell   => ["cd $unixsources/tiff-3.8.2",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: TIFF' ],

# confirmed latest source version as at 26-12-2008
[ archive => $sources.'libexif-0.6.17.tar.gz',  
  fetch   => 'http://'.$sourceforge.
             '/sourceforge/libexif/libexif-0.6.17.tar.gz'],
[ dir     => $sources.'libexif-0.6.17', 
  extract => $sources.'libexif-0.6.17.tar' ],
[ file    => $sources.'libexif-0.6.17/Makefile', 
  shell   => ["cd $unixsources/libexif-0.6.17",
              "./configure --prefix=/usr",
              "make",
              "make install"],
  comment => 'building and installing: libexif' ],

# confirmed latest source version as at 26-12-2008
[ archive => $sources.'libvisual-0.4.0.tar.gz',  
  fetch   => 'http://'.$sourceforge.
             '/sourceforge/libvisual/libvisual-0.4.0.tar.gz'],
[ dir     => $sources.'libvisual-0.4.0', 
  extract => $sources.'libvisual-0.4.0.tar' ],
[ file    => $sources.'libvisual-0.4.0/Makefile', 
  shell  => ["cd $unixsources/libvisual-0.4.0",
             "./configure --prefix=/usr",
             "make",
             "make install"],
  comment => 'building and installing: libvisual' ],


[ archive => $sources.'fftw-3.2.tar.gz',  
  fetch   => 'http://www.fftw.org/fftw-3.2.tar.gz'],
[ dir     => $sources.'fftw-3.2', 
  extract => $sources.'fftw-3.2.tar' ],
[ file    => $msys.'lib/libfftw3.a', 
  shell  => ["cd $unixsources/fftw-3.2",
             "./configure --prefix=/usr",
             "make",
             "make install"],
  comment => 'building and installing: msys fftw' ],
[ file    => $mingw.'lib/libfftw3.a', 
  shell  => ["cd $unixsources/fftw-3.2",
             "./configure --prefix=/mingw",
             "make",
             "make install"],
  comment => 'building and installing: mingw fftw' ],

# typical template:
#[ archive => $sources.'xxx.tar.gz',  fetch => ''],
#[ dir => $sources.'xxx', extract => $sources.'xxx.tar' ],
#[ file => $msys.'lib/something-xx.a', 
#  shell => ["cd $unixsources/xxx",
#            "./configure --prefix=/usr",
#            "make",
#            "make install"] ],

#----------------------------------------
# Building QT3 is complicated!
#

;

if ( $qtver == 3  ) {
push @{$expect}, 

#[ pause => 'press [enter] to build QT3 next!'],
[ archive => $sources.'qt-3.3.x-p8.tar.bz2',  
  fetch => 'http://'.$sourceforge.'/sourceforge/qtwin/qt-3.3.x-p8.tar.bz2'],
[ dir => $msys.'qt-3.3.x-p8',
  extract => [$sources.'qt-3.3.x-p8.tar', $msys] ],


# equivalent patch:
[ archive => $sources.'qt.patch' ,
  'fetch' => 'http://tanas.ca/qt.patch',
  comment => ' patch the QT sources'],
[ filesame => [$msys.'qt-3.3.x-p8/qt.patch',$sources."qt.patch"],
  copy => [''=>''] ],
[ grep => ["\-lws2_32", $msys.'qt-3.3.x-p8/mkspecs/win32-g++/qmake.conf'],
  shell => ["cd ".$unixmsys."qt-3.3.x-p8/","patch -p1 < qt.patch"],
  comment => 'patch the qt3 qmake.conf' ],


[ file => $msys.'bin/sh_.exe',
  shell => ["mv ".$unixmsys."bin/sh.exe ".$unixmsys."bin/sh_.exe"],
  comment => 'rename msys sh.exe out of the way before building QT! ' ] ,

# write a batch script for the QT environment under DOS:
[ file => $msys.'qt-3.3.x-p8/qt3_env.bat',
  write => [$msys.'qt-3.3.x-p8/qt3_env.bat',
'rem a batch script for the QT environment under DOS:
set QTDIR='.$dosmsys.'qt-3.3.x-p8
set MINGW='.$dosmingw.'
set PATH=%QTDIR%\bin;%MINGW%\bin;%PATH%
set QMAKESPEC=win32-g++
cd %QTDIR%
'
],comment=>'write a batch script for the QT3 environment under DOS'],


[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll', 
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && '.
          'configure.bat -thread -plugin-sql-mysql -opengl -no-sql-sqlite',
  comment => 'Execute qt3_env.bat  && the configure command to actually '.
             'build QT now!  - ie configures qt and '.
             'also makes it, hopefully! WARNING SLOW (MAY TAKE HOURS!)' ],
[ exists => $mingw.'bin/sh.exe',
   shell => ["mv ".$unixmingw."bin/sh.exe ".$unixmingw."bin/sh_.exe"],
 comment => 'rename mingw sh.exe out of the way before building QT! ' ] ,

[ filesame => [$msys.'qt-3.3.x-p8/bin/libqt-mt3.dll',$msys.'qt-3.3.x-p8/lib/libqt-mt3.dll'], 
  copy => [''=>''], 
  comment => 'is there a libqt-mt3.dll in the "lib" folder of QT? if so, copy it to the the "bin" folder for uic.exe to use!' ],

# did the configure finish?  - run mingw32-make to get it to finish
# properly.
# HINT: the very last file built in a successful QT build env is the C:\msys\1.0\qt-3.3.x-p8\examples\xml\tagreader-with-features\tagreader-with-features.exe
#[ file => $msys.'qt-3.3.x-p8/examples/xml/tagreader-with-features/tagreader-with-features.exe', exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make',comment => 'we try to finish the build of QT with mingw32-make, incase it was just a build dependancy issue? WARNING SLOW (MAY TAKE HOURS!)' ],

# TODO - do we have an action we can take to build just this one file/dll if it fails?  
# For now, we will just test if it built, and try to run 'make' again if it didn't!
[ file => $msys.'qt-3.3.x-p8/plugins/sqldrivers/libqsqlmysql.dll',
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make -j',
  comment => 'lib\libqsqlmysql.dll - we are just validating some basics of '.
             'the QT install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file => $msys.'qt-3.3.x-p8/bin/qmake.exe',
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make',
  comment => 'bin\qmake.exe - here we are just validating some basics of '.
             'the QT install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file => $msys.'qt-3.3.x-p8/bin/moc.exe',
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make', 
  comment => 'bin\moc.exe - here we are just validating some basics of '.
             'the QT install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file => $msys.'qt-3.3.x-p8/bin/uic.exe',
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make', 
  comment => 'bin\uic.exe - here we are just validating some basics of '.
             'the QT install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file => $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll',
  exec => $dosmsys.'qt-3.3.x-p8\qt3_env.bat && mingw32-make',
  comment => 'bin\libqt-mt3.dll - here we are just validating some basics of '.
             'the QT install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],

# a manual method for "installing" QT would be to put all the 'bin' files
# into /mingw/bin and similarly for the 'lib' and 'include' folders to their
# respective mingw folders, but we don't.  we run it from the build location!


#  (back to sh.exe ) now that we are done !
[ file => $msys.'bin/sh.exe',
 shell => ["mv ".$unixmsys."bin/sh_.exe ".$unixmsys."bin/sh.exe"],
 comment => 'rename msys sh_.exe back again!' ] ,
[ exists => $mingw.'bin/sh_.exe',
   shell => ["mv ".$unixmingw."bin/sh_.exe ".$unixmingw."bin/sh.exe"],
 comment => 'rename mingw sh_.exe back again!' ] ,

#Copy libqt-mt3.dll to libqt-mt.dll  , if there is any date/size change!
[ filesame => [$msys.'qt-3.3.x-p8/lib/libqt-mt.dll',
               $msys.'qt-3.3.x-p8/lib/libqt-mt3.dll'],
  copy => [''=>''],
  comment => 'Copy libqt-mt3.dll to libqt-mt.dll' ] ,
;
}

# 
#----------------------------------------
# building QT4 is complicated too
#----------------------------------------
if ( $qtver == 4  ) {
push @{$expect}, 
#[ pause => 'press [enter] to extract and patch QT4 next!'],

[ archive => $sources.'qt-win-opensource-src-4.4.3.zip',  
  fetch => 'http://ftp.icm.edu.pl/packages/qt/source/'.
           'qt-win-opensource-src-4.4.3.zip'],
[ dir => $msys.'qt-win-opensource-src-4.4.3', 
  extract => [$sources.'qt-win-opensource-src-4.4.3.zip', $msys] ],

[ always => [],
  write => [$sources.'qt-4.4.3.patch1',
"diff -u qt-win-opensource-src-4.4.3/qmake/option.cpp.orig qt-win-opensource-src-4.4.3/qmake/option.cpp
--- qt-win-opensource-src-4.4.3/qmake/option.cpp.orig	Wed Feb 20 04:35:22 2008
+++ qt-win-opensource-src-4.4.3/qmake/option.cpp	Sun May 18 12:12:51 2008
@@ -619,7 +613,10 @@
     Q_ASSERT(!((flags & Option::FixPathToLocalSeparators) && (flags & Option::FixPathToTargetSeparators)));
     if(flags & Option::FixPathToLocalSeparators) {
 #if defined(Q_OS_WIN32)
-        string = string.replace('/', '\\\\');
+        if(Option::shellPath.isEmpty())  // i.e. not running under MinGW
+            string = string.replace('/', '\\\\');
+        else
+            string = string.replace('\\\\', '/');
 #else
         string = string.replace('\\\\', '/');
 #endif
"], comment => 'Create patch1 for QT4'],

[ grep  => ['Option::shellPath.isEmpty',
            $msys.'qt-win-opensource-src-4.4.3/qmake/option.cpp'], 
  shell => ['cd '.$unixmsys, 'patch -p0 < '.$sources.'qt-4.4.3.patch1'] ],


[ always => [],
  write => [$sources.'qt-4.4.3.patch2',
"diff -ru qt-win-opensource-src-4.4.3/mkspecs/win32-g++/qmake.conf.orig qt-win-opensource-src-4.4.3/mkspecs/win32-g++/qmake.conf
--- qt-win-opensource-src-4.4.3/mkspecs/win32-g++/qmake.conf.orig	Wed Feb 20 04:34:58 2008
+++ qt-win-opensource-src-4.4.3/mkspecs/win32-g++/qmake.conf	Sun May 18 21:10:57 2008
@@ -75,12 +75,15 @@
     MINGW_IN_SHELL      = 1
 	QMAKE_DIR_SEP		= /
 	QMAKE_COPY		= cp
-	QMAKE_COPY_DIR		= xcopy /s /q /y /i
+	QMAKE_COPY_DIR		= cp -r
 	QMAKE_MOVE		= mv
 	QMAKE_DEL_FILE		= rm
-	QMAKE_MKDIR		= mkdir
+	QMAKE_MKDIR		= mkdir -p
 	QMAKE_DEL_DIR		= rmdir
     QMAKE_CHK_DIR_EXISTS = test -d
+    QMAKE_MOC		= \$\$[QT_INSTALL_BINS]/moc
+    QMAKE_UIC		= \$\$[QT_INSTALL_BINS]/uic
+    QMAKE_IDC		= \$\$[QT_INSTALL_BINS]/idc
 } else {
 	QMAKE_COPY		= copy /y
 	QMAKE_COPY_DIR		= xcopy /s /q /y /i
@@ -89,11 +92,10 @@
 	QMAKE_MKDIR		= mkdir
 	QMAKE_DEL_DIR		= rmdir
     QMAKE_CHK_DIR_EXISTS	= if not exist
+    QMAKE_MOC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}moc.exe
+    QMAKE_UIC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}uic.exe
+    QMAKE_IDC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}idc.exe
 }
-
-QMAKE_MOC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}moc.exe
-QMAKE_UIC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}uic.exe
-QMAKE_IDC		= \$\$[QT_INSTALL_BINS]\$\${DIR_SEPARATOR}idc.exe
 
 QMAKE_IDL		= midl
 QMAKE_LIB		= ar -ru
"], comment => 'Create patch2 for QT4'],

[ grep  => ['QMAKE_COPY_DIR.*?= cp -r',
            $msys.'qt-win-opensource-src-4.4.3/mkspecs/win32-g++/qmake.conf'], 
  shell => ['cd '.$unixmsys, 'patch -p0 < '.$sources.'qt-4.4.3.patch2'] ],


[ always => [],
  write => [$sources.'qt-4.4.3.patch3', 
"diff -ur qt-win-opensource-src-4.4.3.orig/src/corelib/kernel/qobjectdefs.h qt-win-opensource-src-4.4.3/src/corelib/kernel/qobjectdefs.h
--- qt-win-opensource-src-4.4.3.orig/src/corelib/kernel/qobjectdefs.h Mon Jun 30 20:25:04 2008
+++ qt-win-opensource-src-4.4.3/src/corelib/kernel/qobjectdefs.h Mon Jun 30 16:25:50 2008
@@ -149,7 +149,7 @@
 #define Q_OBJECT \
 public: \\
     Q_OBJECT_CHECK \\
-    static const QMetaObject staticMetaObject; \\
+    static QMetaObject staticMetaObject; \\
     virtual const QMetaObject *metaObject() const; \\
     virtual void *qt_metacast(const char *); \\
     QT_TR_FUNCTIONS \\
@@ -160,7 +160,7 @@
 /* tmake ignore Q_GADGET */
 #define Q_GADGET \\
 public: \\
-    static const QMetaObject staticMetaObject; \\
+    static QMetaObject staticMetaObject; \\
 private:
 #else // Q_MOC_RUN
 #define slots slots
"], comment => 'Create patch3 for QT4'],

[ grep  => ['static QMetaObject staticMetaObject',
            $msys.'qt-win-opensource-src-4.4.3/src/corelib/kernel/qobjectdefs.h'], 
  shell => ['cd '.$unixmsys, 'patch -p0 < '.$sources.'qt-4.4.3.patch3'] ],


[ always => [],
  write => [$sources.'qt-4.4.3.patch4', 
"diff -ur qt-win-opensource-src-4.4.3.orig/src/tools/moc/generator.cpp qt-win-opensource-src-4.4.3/src/tools/moc/generator.cpp
--- qt-win-opensource-src-4.4.3.orig/src/tools/moc/generator.cpp Mon Jun 30 20:25:24 2008
+++ qt-win-opensource-src-4.4.3/src/tools/moc/generator.cpp Mon Jun 30 16:16:08 2008
@@ -319,7 +319,7 @@
     if (isQt)
         fprintf(out, \"const QMetaObject QObject::staticQtMetaObject = {\\n\");
     else
-        fprintf(out, \"const QMetaObject \%s::staticMetaObject = {\\n\", cdef->qualified.constData());
+        fprintf(out, \"QMetaObject \%s::staticMetaObject = {\\n\", cdef->qualified.constData());

     if (isQObject)
         fprintf(out, \"    { 0, \");
"], comment => 'Create patch4 for QT4'],

[ grep  => ['fprintf\(out, \"QMetaObject \%s::staticMetaObject',
            $msys.'qt-win-opensource-src-4.4.3/src/tools/moc/generator.cpp'], 
  shell => ['cd '.$unixmsys, 'patch -p0 < '.$sources.'qt-4.4.3.patch4'] ],



[ always => [],
  write => [$sources.'qatomic_windows.h', 
'--- src/corelib/arch/qatomic_windows.h.org	Thu Dec  4 11:35:46 2008
+++ src/corelib/arch/qatomic_windows.h	Wed Dec  3 22:47:04 2008
@@ -383,6 +383,9 @@
 
 #else
 
+#ifndef __INTERLOCKED_DECLARED
+#define __INTERLOCKED_DECLARED
+
 extern "C" {
     __declspec(dllimport) long __stdcall InterlockedCompareExchange(long *, long, long);
     __declspec(dllimport) long __stdcall InterlockedIncrement(long *);
@@ -390,6 +393,8 @@
     __declspec(dllimport) long __stdcall InterlockedExchange(long *, long);
     __declspec(dllimport) long __stdcall InterlockedExchangeAdd(long *, long);
 }
+
+#endif
 
 inline bool QBasicAtomicInt::ref()
 {
'], comment => 'Create patch3 for QT4'],
[ grep  => ['#define __INTERLOCKED_DECLARED',
            $msys.'qt-win-opensource-src-4.4.3/src/corelib/arch/qatomic_windows.h'], 
  shell => ['cd '.$msys."qt-win-opensource-src-4.4.3", 'patch -p0 < '.$sources.'qatomic_windows.h'] ],





#[ pause => 'press [enter] to build QT4 !'],

# Trolltech USED TO recommend NOT having sh.exe in the path when building QT
# ( at least under QT4 )
#[ file   => $msys.'bin/sh_.exe',
# shell   => ["mv ".$unixmsys."bin/sh.exe ".$unixmsys."bin/sh_.exe"],
# comment => 'rename msys sh.exe out of the way before building QT! ' ] ,
#[ exists => $mingw.'bin/sh.exe',
#   shell => ["mv ".$unixmingw."bin/sh.exe ".$unixmingw."bin/sh_.exe"],
# comment => 'rename mingw sh.exe out of the way before building QT! ' ] ,

# Write a batch script for the QT environment under DOS:
[ always => [], write => [$msys.'qt-win-opensource-src-4.4.3/qt4_env.bat',
'rem a batch script for building the QT environment under DOS:
set QTDIR='.$dosmsys.'qt-win-opensource-src-4.4.3
set MINGW='.$dosmingw.'
set PATH=%QTDIR%\bin;%MINGW%\bin;%PATH%
set QMAKESPEC=win32-g++
cd %QTDIR%
goto SMALL

rem This would do a full build:
'.$dosmsys.'bin\yes | configure -plugin-sql-mysql -no-sql-sqlite -debug-and-release -fast -no-sql-odbc -no-qdbus
mingw32-make -j '.($numCPU + 1).'
goto END

:SMALL
rem This cuts out the examples and demos:
'.$dosmsys.'bin\yes | configure -plugin-sql-mysql -no-sql-sqlite -debug-and-release -fast -no-sql-odbc -no-qdbus
bin\qmake projects.pro
mingw32-make -j '.($numCPU + 1).' sub-plugins-make_default-ordered
goto END

:NODEBUGSMALL
rem This cuts out the examples and demos, and only builds release libs
'.$dosmsys.'bin\yes | configure -plugin-sql-mysql -no-sql-sqlite -release -fast -no-sql-odbc -no-qdbus
bin\qmake projects.pro
mingw32-make -j '.($numCPU + 1).' sub-plugins-make_default-ordered
rem
rem Since we omit debug libs, this pretends release is debug:
cd lib
copy QtCore4.dll     QtCored4.dll
copy QtXml4.dll      QtXmld4.dll
copy QtSql4.dll      QtSqld4.dll
copy QtGui4.dll      QtGuid4.dll
copy QtNetwork4.dll  QtNetworkd4.dll
copy QtOpenGL4.dll   QtOpenGLd4.dll
copy Qt3Support4.dll Qt3Supportd4.dll
copy libqtmain.a     libqtmaind.a

:END
',
],comment=>'write a batch script for the QT4 environment under DOS'],

# test if the core .dll is built, and build QT if it isn't! 
[ file    => $msys.'qt-win-opensource-src-4.4.3/lib/QtCore4.dll',
  exec    => $dosmsys.'qt-win-opensource-src-4.4.3\qt4_env.bat',
  comment => 'Execute qt4_env.bat to actually build QT now!  - '.
             'ie configures qt and also makes it, hopefully! '.
             'WARNING SLOW (MAY TAKE HOURS!)' ],

# For now, we will just test if it built, and abort if it didn't!
[ file    => $msys.
             'qt-win-opensource-src-4.4.3/plugins/sqldrivers/qsqlmysql4.dll', 
  exec    => '', 
  comment => 'lib\libqsqlmysql4.dll - here we are just validating some basics '.
             'of the QT4 install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file    => $msys.'qt-win-opensource-src-4.4.3/bin/qmake.exe', 
  exec    => '', 
  comment => 'bin\qmake.exe - here we are just validating some basics of '.
             'the QT4 install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file    => $msys.'qt-win-opensource-src-4.4.3/bin/moc.exe', 
  exec    => '', 
  comment => 'bin\moc.exe - here we are just validating some basics of '.
             'the QT4 install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],
[ file    => $msys.'qt-win-opensource-src-4.4.3/bin/uic.exe', 
  exec    => '', 
  comment => 'bin\uic.exe - here we are just validating some basics of '.
             'the QT4 install, and if any of these components are missing, '.
             'the build must have failed (is the sql driver built properly?) '],

#  move it (back to sh.exe ) now that we are done !
#[ file    => $msys.'bin/sh.exe',
#  shell   => ["mv ".$unixmsys."bin/sh_.exe ".$unixmsys."bin/sh.exe"],
#  comment => 'rename msys sh_.exe back again!' ] ,
#[ exists  => $mingw.'bin/sh_.exe',
#  shell   => ["mv ".$unixmingw."bin/sh_.exe ".$unixmingw."bin/sh.exe"],
#s  comment => 'rename mingw sh_.exe back again!' ] ,

;
} # end of QT4 install


push @{$expect},
#----------------------------------------
# is the build type we are doing basically the same as any previous one?
# if not, we must cleanup our build and SVN area BEFORE
# we try to do any SVN activities! 
#----------------------------------------
# $compile_type, $svnlocation, and $qtver changing
# are all good reasons to cleanup the build area:
# later on, we'll also see if the $SVNRELEASE has changed,
# and trigger on that to....
[ file  => $mythtv.$is_same,
  shell   => ['source '.$unixmythtv.'make_clean.sh',
              'nocheck' ],
  comment => 'cleaning environment - step 1a' ],

[ file  => $mythtv."delete_to_do_make_clean.txt",
  shell   => ['source '.$unixmythtv.'make_clean.sh',
              'nocheck' ],
  comment => 'cleaning environment - step 1b' ],

#----------------------------------------
# get mythtv sources, if we don't already have them
# download all the files from the web, and save them here:
#----------------------------------------
[ dir     => $mythtv.'mythtv',
  mkdirs  => $mythtv.'mythtv',
  comment => 'make myth dir'],


# if we dont have the sources at all, get them all from SVN!
# (do a checkout, but only if we don't already have the .pro file
#  as a sign of an earlier checkout)
;

foreach my $comp( @components ) {
  push @{$expect}, [ dir => $mythtv.$comp, mkdirs => $mythtv.$comp ];
  push @{$expect},
  [ dir => "$mythtv$comp/.svn",
    exec => ["$dosmsys\\bin\\svn checkout ".
             "http://svn.mythtv.org/svn/$svnlocation/$comp $dosmythtv$comp",
             'nocheck'],
    comment => "Get all the mythtv sources from SVN!:$comp" ];

}


push @{$expect}, 
# now lets write some build scripts to help with mythtv itself

# Qt3
[ always => [], 
  write => [$mythtv.'qt3_env.sh',
'export QTDIR='.$unixmsys.'qt-3.3.x-p8
export QMAKESPEC=$QTDIR/mkspecs/win32-g++
export LD_LIBRARY_PATH=$QTDIR/lib:/usr/lib:/mingw/lib:/lib
export PATH=$QTDIR/bin:/usr/local/bin:$PATH
' ],
  comment => 'write a QT3 script that we can source later when inside msys '.
             'to setup the environment variables'],


# Qt4
[ always => [], 
  write => [$mythtv.'qt4_env.sh',
'export QTDIR='.$unixmsys.'qt-win-opensource-src-4.4.3
export QMAKESPEC=$QTDIR/mkspecs/win32-g++
export LD_LIBRARY_PATH=$QTDIR/lib:/usr/lib:/mingw/lib:/lib
export PATH=$QTDIR/bin:/usr/local/bin:$PATH
' ],
  comment => 'write a QT4 script that we can source later when inside msys '.
             'to setup the environment variables'],


[ always => [], 
  write => [$mythtv.'make_clean.sh',
'source '.$unixmythtv.'qt'.$qtver.'_env.sh
rm -f /usr/bin/myth*.exe
rm -f /usr/bin/libmyth*.dll
rm -f /usr/lib/libmyth*.*
rm -f /usr/lib/liblibmyth*.*
rm -fr /usr/include/mythtv
rm -fr /usr/lib/mythtv
rm -fr /usr/share/mythtv
rm -fr /lib/mythtv
rm -f /usr/bin/mtd.exe
rm -f /usr/bin/ignyte.exe
cd '.$unixmythtv.'mythtv
make clean
rm Makefile
cd '.$unixmythtv.'mythplugins
make clean
rm Makefile
cd '.$unixmythtv.'myththemes
make clean
rm Makefile
cd '.$unixmythtv.'
find . -type f -name \*.dll | grep -v build | grep -v setup | xargs -n1 rm
find . -type f -name \*.exe | grep -v build | grep -v setup | xargs -n1 rm
find . -type f -name \*.a | xargs -n1 rm
find . -type f -name \*.o | xargs -n1 rm
#find . -type f -name Makefile\.* | grep -v svn | xargs -n1 rm
find . -type f -name moc_\*.cpp |  grep -v svn | xargs -n1 rm
#rm -rf '.$unixmythtv.'mythtv/libs/libmythdb
rm -f '.$mythtv.'delete_to_do_make_clean.txt
'], 
  comment => 'write a script to clean up myth environment'],


# chmod the shell scripts, everytime
[ always => [] ,
  shell => ["cd $mythtv","chmod 775 *.sh"] ],

#----------------------------------------
# now we prep for the build of mythtv! 
#----------------------------------------
;
if ($makeclean) {
  push @{$expect},
  [ always  => [],
    shell   => ['source '.$unixmythtv.'make_clean.sh'],
    comment => 'cleaning environment'],
  ;
}

foreach my $comp( @components ) {

# switch to the correct SVN branch before we do anything,
# if we are on the wrong one...
  push @{$expect},
  #[ file  => $mythtv.$is_same,
  [ always => '',
    exec => [ "$dosmsys\\bin\\svn cleanup $dosmythtv$comp",
             'nocheck'],
    comment => " SVN cleanup:$comp" ],
    
  #[ file  => $mythtv.$is_same,
  [ always => '',
    exec => [ "$dosmsys\\bin\\svn -r $SVNRELEASE  switch ".
             "http://svn.mythtv.org/svn/$svnlocation/$comp $dosmythtv$comp",
             'nocheck'],
    comment => " SVN SWITCH BRANCH!:$comp" ],
    
  # if we don't have the needed indicator of the branch we are now on,
  # save that info...
  [ file  => $mythtv.$is_same,
    shell => ['rm -f '.$unixmythtv.'*.same',
              'touch '.$unixmythtv.$is_same,
             ],
    comment => 'cleaning environment' ], 

  #[ pause => 'press [enter] to continue !'],

# ... then SVN update every time, before patches

  [ always  => [],
    exec    => [$dosmsys."bin\\svn.exe -r $SVNRELEASE update $dosmythtv$comp"],
    comment => "Getting SVN updates for:$comp on $svnlocation" ];
}

push @{$expect}, 

# always get svn num
[ always   => [],
  exec     => ['cd '.$dosmythtv.'mythtv && '.
               $dosmsys.'bin\svn.exe info > '.$dosmythtv.'mythtv\svn_info.new'],
 comment   => 'fetching the SVN number to a text file, if we can'],
[ filesame => [$mythtv.'mythtv/svn_info.txt',$mythtv.'mythtv/svn_info.new'],
  shell    => ['touch -r '.$unixmythtv.'mythtv/svn_info.txt '.
               $unixmythtv.'mythtv/svn_info.new', 'nocheck'],
  comment  => 'match the datetime of these files, '.
              'so that the contents only can be compared next' ],

# is svn num (ie file contents) changed since last run, if so, do a 'make
# clean' (overkill, I know, but safer)!
[ filesame => [$mythtv.'mythtv/svn_info.txt',$mythtv.'mythtv/svn_info.new'], 
  shell => ['source '.$unixmythtv.'make_clean.sh',
            'touch '.$unixmythtv.'mythtv/last_build.txt',
            'cat '.$unixmythtv.'mythtv/svn_info.new >'.$unixmythtv.'mythtv/svn_info.txt',
            'touch -r '.$unixmythtv.'mythtv/svn_info.txt '.$unixmythtv.'mythtv/svn_info.new',
            'sleep 5'], 
  comment => 'if the SVN number is changed, then remember that, AND arrange for a full re-make of mythtv. (overkill, I know, but safer)' ], 

# open up the permissions:
[ always   => [],
  shell    => ['cd '.$unixmythtv.'mythtv',
#TODO - reenable               'chmod -R 777 .'
#               'cd '.$unixmythtv.'mythplugins',
#               'chmod -R 777 .'
#               'cd '.$unixmythtv.'mythtvthemes',
#               'chmod -R 777 .'
               ],
  comment   => 'loosening permissions on source code:'],



#  [ pause => 'press [enter] to continue !'],
  
# apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN:
#----------------------------------------
# expired patches
#----------------------------------------

# 15586 and earlier need this patch:
#[ archive => $sources.'backend.patch.gz' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4392/backend.patch.gz', comment => 'backend.patch.gz - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/backend.patch.gz',$sources."backend.patch.gz"], copy => [''=>'',comment => '4392: - backend connections being accepted patch '] ],
#[ grep => ['unsigned\* Indexes = new unsigned\[n\]\;',$mythtv.'mythtv/libs/libmyth/mythsocket.cpp'], shell => ["cd ".$unixmythtv."mythtv/","gunzip -f backend.patch.gz","patch -p0 < backend.patch"] ],

# these next 3 patches are needed for 15528 (and earlier)
#[ archive => $sources.'importicons_windows_2.diff' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/3334/importicons_windows_2.diff', comment => 'importicons_windows_2.diff - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/importicons_windows_2.diff',$sources."importicons_windows_2.diff"], copy => [''=>'',comment => '3334 fixes error with mkdir() unknown.'] ],
#
#[ grep => ['\#include <qdir\.h>',$mythtv.'mythtv/libs/libmythtv/importicons.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < importicons_windows_2.diff"] ],
#[ archive => $sources.'mingw.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4516/mingw.patch', comment => 'mingw.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/mingw.patch',$sources."mingw.patch"], copy => [''=>'',comment => '4516 fixes build'] ],
#[ grep => ['LIBS \+= -lmyth-\$\$LIBVERSION',$mythtv.'mythtv/libs/libmythui/libmythui.pro'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < mingw.patch"] ]
#
# (fixed in 15547)
#[ archive => $sources.'util_win32.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4497/util_win32.patch', comment => 'util_win32.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/util_win32.patch',$sources."util_win32.patch"], copy => [''=>'',comment => '4497 fixes build'] ],
#[ grep => ['\#include "compat.h"',$mythtv.'mythtv/libs/libmyth/util.h'], shell => ["cd ".$unixmythtv."mythtv/libs/libmyth/","patch -p0 < ".$unixmythtv."mythtv/util_win32.patch"] ],

# post 15528, pre 15568  needs this: equivalent to: svn merge -r 15541:15540 .
#[ archive => $sources.'15541_undo.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/XXXX/15541_undo.patch', comment => 'util_win32.patch - apply any outstanding win32 patches - this section will be hard to keep up with HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/15541_undo.patch',$sources."15541_undo.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['\#include \"compat.h\"',$mythtv.'mythtv/libs/libmythui/mythpainter.cpp'], shell => ["cd ".$unixmythtv."mythtv/libs/libmyth/","patch -p2 < ".$unixmythtv."mythtv/15541_undo.patch"] , comment => 'currently need this patch too, equivalemnt of: svn merge -r 15541:15540 .'],

# Ticket 4984
#[ archive => $sources.'4984_mythcontext.patch6' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4984/mythcontext.patch6', comment => 'Applying Ticket 4984'],
#[ filesame => [$mythtv.'mythtv/4984_mythcontext.patch6',$sources."4984_mythcontext.patch6"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['MYTHLIBDIR',$mythtv.'mythtv/libs/libmyth/mythcontext.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < ".$unixmythtv."mythtv/4984_mythcontext.patch6"] , comment => ' 4984'],


# Ticket 4699
#[ archive => $sources.'4699_win32_fs.patch', 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4699/win32_fs.patch', comment => 'win32_fs.patch'],
#[ filesame => [$mythtv.'mythtv/4699_win32_fs.patch',$sources."4699_win32_fs.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['setCaption',$mythtv.'mythtv/libs/libmythui/mythmainwindow.cpp'], shell => ["cd ".$unixmythtv."mythtv","patch -p0 < ".$unixmythtv."mythtv/4699_win32_fs.patch"] , comment => ' 4699'],

#  4718
#[ archive => $sources.'4718_undo.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4718/dvd_playback.patch', comment => 'Ticket 4718 dvdplayer.patch'],
#[ filesame => [$mythtv.'mythtv/4718_undo.patch',$sources."4718_undo.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['filename.right(filename.length() -  4)',$mythtv.'mythtv/libs/libmythtv/RingBuffer.cpp'], shell => ["cd ".$unixmythtv."mythtv/","patch -p0 < ".$unixmythtv."mythtv/4718_undo.patch"] , comment => ' .'],
#[ archive => $sources.'4718_playback.patch' , 'fetch' => 'http://svn.mythtv.org/trac/raw-attachment/ticket/4718/dvd_playback_plugin.patch', comment => 'dvdplayer.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/4718_playback.patch',$sources."4718_playback.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  => ['gc->setValue\("D:',$mythtv.'mythplugins/mythvideo/mythvideo/globalsettings.cpp'], shell => ["cd ".$unixmythtv."mythplugins/mythvideo","patch -p0 < ".$unixmythtv."mythtv/4718_undo.patch"] , comment => ' .'],
#[ grep  => ['\"dvd:\/\"',$mythtv.'mythplugins/mythvideo/mythvideo/main.cpp'], shell => ["cd ".$unixmythtv."mythplugins/mythvideo","patch -p1 < ".$unixmythtv."mythtv/4718_undo.patch"] , comment => ' 4718'],

# Ticket 15831 
#[ archive => $sources.'15831_win32_fs.patch', 'fetch' => 'http://svn.mythtv.org/trac/changeset/15831?format=diff&new=15831', comment => 'win32_fs.patch - apply any outstanding win32 patches - this section will be hard to keep upwith HEAD/SVN'],
#[ filesame => [$mythtv.'mythtv/15831_win32_fs.patch',$sources."15831_win32_fs.patch"], copy => [''=>'',comment => 'XXXX'] ],
#[ grep  =>  ['\+', $mythtv.'mythtv/15831_win32_fs.patch'], shell => ["cd ".$unixmythtv."mythtv","dos2unix 15831_win32_fs.patch"], comment => ' .'],
#[ grep  => ['LOCALAPPDATA',$mythtv.'mythtv/libs/libmyth/mythcontext.cpp'], shell => ["cd ".$unixmythtv."mythtv","patch -p0 < ".$unixmythtv."mythtv/15831_win32_fs.patch"] , comment => ' 15831'],

; 
#
if ($tickets == 1) {
 push @{$expect}, 

# Ticket 4702
[ archive => $sources.'4702_mingw.patch', 
  'fetch' => 'http://svn.mythtv.org/trac/raw-attachment'.
             '/ticket/4702/mingw.patch', 
  comment => 'mingw.patch - apply any outstanding win32 patches - '.
             'this section will be hard to keep upwith HEAD/SVN'],
[ filesame => [$mythtv.'mythtv/4702_mingw.patch',$sources."4702_mingw.patch"], 
  copy     => [''=>'',comment => 'XXXX'] ],
[ grep  => ['\$\$\{PREFIX\}\/bin',
            $mythtv.'mythtv/libs/libmythui/libmythui.pro'], 
  shell => ["cd ".$unixmythtv."mythtv",
            "patch -p0 < ".$unixmythtv."mythtv/4702_mingw.patch"] , 
  comment => ' 4702'],


#  [ pause => 'press [enter] to continue !'],
;

} # End if for $ticket


#----------------------------------------
# now we actually build mythtv! 
#----------------------------------------
push @{$expect}, 

[ file    => $mythtv.'mythtv/config/config.pro',
  shell   => ['touch '.$unixmythtv.'mythtv/config/config.pro'], 
  comment => 'create an empty config.pro or the mythtv build will fail'],
;
# do a make clean before going any further?
# Yes, if the SVN revision has changed (it deleted the file for us),
#  or the user deleted the file manually, to request it. 
# ...to do a full clean-up of a prevous build,
# a bit more than a 'make clean' can be required:
foreach my $comp( @components ) {
  push @{$expect}, 
  [ file    => $mythtv.'delete_to_do_make_clean.txt',
    exec    => [$dosmsys."bin\\svn.exe -R revert $dosmythtv$comp", "nocheck"],
    comment => "reverting any local MODS from SVN - $comp on $svnlocation" ],
    
# this is now done as part of make_clean.sh, which happens EARLIER
# in the process, prior to any possible SVN branch changes! 
#  [ file    => $mythtv.'delete_to_do_make_clean.txt',
#    shell   =>['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
#              'cd '.$unixmythtv.$comp,
#              'make clean',
#              'rm Makefile',
#              'nocheck']
#              ];
}
push @{$expect}, 

[ file    => $mythtv.'delete_to_do_make_clean.txt',
  shell   => ['touch '.$unixmythtv.'delete_to_do_make_clean.txt'],
  comment => 'do a "make clean" first? not strictly necessary in all cases, '.
             'and the build will be MUCH faster without it, but it is safer '.
             'with it... ( we do a make clean if the SVN revision changes) '], 


#broken Makefile?, delete it
[ grep    => ['Makefile|MAKEFILE',$mythtv.'mythtv/Makefile'],
  shell   => ['rm '.$unixmythtv.'mythtv/Makefile','nocheck'],
  comment => 'broken Makefile, delete it' ],

# configure
[ file   => $mythtv.'mythtv/Makefile',
  shell  => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'mythtv',
            './configure --prefix='.$unixbuild.' --runprefix=..'.
            ' --disable-dbox2 --disable-hdhomerun'.
            ' --disable-iptv --disable-joystick-menu --disable-xvmc-vld'.
            ' --disable-xvmc --enable-directx --disable-lirc'.
            ' --cpu=k8 --compile-type='.$compile_type],
  comment => 'do we already have a Makefile for mythtv?' ],

# make

# fix a bug in Makefile and make COPY_DIR cp -fr instead of cp -f
# TODO Check if this is necessary. I suspect it was meant to fix #4949?
#
[ file => $mythtv.'mythtv/fix_makefile.sh', 
  write => [$mythtv.'mythtv/fix_makefile.sh', 
'cd '.$unixmythtv.'mythtv
cat Makefile | sed  "s/\(^COPY_DIR\W*\=\W*cp\)\(\W\-f\)/\1 -fr/" > Makefile_new
cp Makefile_new Makefile
'],
  comment => 'write a script to fix Makefile'],

[ newer => [$mythtv."mythtv/libs/libmyth/libmyth-$version.dll",
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv."mythtv/libs/libmyth/libmyth-$version.dll",
            'source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'mythtv', $parallelMake],
  comment => 'libs/libmyth/libmyth-$version.dll - '.
             'redo make unless all these files exist, '.
             'and are newer than the last_build.txt identifier' ],
[ newer => [$mythtv."mythtv/libs/libmythtv/libmythtv-$version.dll",
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv."mythtv/libs/libmythtv/libmythtv-$version.dll",
            'source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'mythtv', $parallelMake],
  comment => 'libs/libmythtv/libmythtv-$version.dll - '.
             'redo make unless all these files exist, '.
             'and are newer than the last_build.txt identifier' ],
[ newer => [$mythtv.'mythtv/programs/mythfrontend/mythfrontend.exe',
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv.'mythtv/programs/mythfrontend/mythfrontend.exe',
            'source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'mythtv', $parallelMake],
  comment => 'programs/mythfrontend/mythfrontend.exe - '.
             'redo make unless all these files exist, '.
             'and are newer than the last_build.txt identifier' ],
[ newer => [$mythtv.'mythtv/programs/mythbackend/mythbackend.exe',
            $mythtv.'mythtv/last_build.txt'],
  shell => ['rm '.$unixmythtv.'mythtv/programs/mythbackend/mythbackend.exe',
            'source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'mythtv', $parallelMake],
  comment => 'programs/mythbackend/mythbackend.exe - '.
             'redo make unless all these files exist, '.
             'and are newer than the last_build.txt identifier' ],


# Archive old build before we create a new one with make install:
[ exists  => $mythtv.'build_old',
  shell   => ['rm -fr '.$unixmythtv.'build_old'],
  comment => 'Deleting old build backup'],
[ exists  => $build,
  shell   => ['mv '.$unixbuild.' '.$unixmythtv.'build_old'],
  comment => 'Renaming build to build_old for backup....'],

# re-install to /c/mythtv/build if we have a newer mythtv build
# ready:
[ newer   => [$build.'bin/mythfrontend.exe',
              $mythtv.'mythtv/programs/mythfrontend/mythfrontend.exe'],
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
              'cd '.$unixmythtv.'mythtv',
              'make install'],
  comment => 'was the last configure successful? then install mythtv ' ],


# setup_build tidies up the build area and copies extra libs in there
[ always => [], 
  write => [$mythtv.'setup_build.sh',
'#!/bin/bash
source '.$unixmythtv.'qt'.$qtver.'_env.sh
cd '.$unixmythtv.'
echo copying main QT dlls to build folder...
# mythtv probably needs the qt3 dlls at runtime:
cp '.$unixmsys.'qt-3.3.x-p8/lib/*.dll '.$unixmythtv.'build/bin
# mythtv needs the qt4 dlls at runtime:
cp '.$unixmsys.'qt-win-opensource-src-4.4.3/lib/*.dll '.$unixmythtv.'build/bin
# qt mysql connection dll has to exist in a subfolder called sqldrivers:
echo Creating build-folder Directories...
# Assumptions
# <installprefix> = ./mythtv
# themes go into <installprefix>/share/mythtv/themes
# fonts go into <installprefix>/share/mythtv
# libraries go into installlibdir/mythtv
# plugins go into installlibdir/mythtv/plugins
# filters go into installlibdir/mythtv/filters
# translations go into <installprefix>/share/mythtv/i18n
mkdir '.$unixmythtv.'/build/bin/sqldrivers
echo Copying QT plugin required dlls....
cp '.$unixmsys.'qt-3.3.x-p8/plugins/sqldrivers/libqsqlmysql.dll '.$unixmythtv.'build/bin/sqldrivers 
cp '.$unixmsys.'qt-win-opensource-src-4.4.3/plugins/sqldrivers/qsqlmysql4.dll '.$unixmythtv.'build/bin/sqldrivers 
echo Copying ming and msys dlls to build folder.....
# pthread dlls and mingwm10.dll are copied from here:
cp /mingw/bin/*.dll '.$unixmythtv.'build/bin
# msys-1.0.dll dll is copied from here:
cp /bin/msys-1.0.dll '.$unixmythtv.'build/bin
echo copying lib files...
mv '.$unixmythtv.'build/lib/*.dll '.$unixmythtv.'build/bin/
mv '.$unixmythtv.'build/bin/*.a '.$unixmythtv.'build/lib/
mv '.$unixmythtv.'build/lib/mythtv/filters/*.dll '.$unixmythtv.'build/bin/

# because the install process failes to copy the .mak file (needed by the plugins), we copy it manually.
cp '.$unixmythtv.'mythtv/libs/libmyth/mythconfig.mak '.$unixmythtv.'build//include/mythtv/

touch '.$unixmythtv.'/build/package_flag
cp '.$unixmythtv.'gdb_*.bat '.$unixmythtv.'build/bin
cp '.$unixmythtv.'packaging/Win32/debug/*.cmd '.$unixmythtv.'build/bin
'],
  comment => 'write a script to install mythtv to build folder'],


# Create file to install mythplugins
[ always => [], 
  write => [$mythtv.'setup_plugins.sh',
'#!/bin/bash
source '.$unixmythtv.'qt'.$qtver.'_env.sh
cd '.$unixmythtv.'
echo Copying exe files....
find /usr/bin -name \\myth*.exe  | grep -v build | xargs -n1 -i__ cp __ ./build/
echo Copying dll files....
find /usr/bin -name \\libmyth*.dll  | grep -v build | xargs -n1 -i__ cp __ ./build/  
find /usr/lib -name \\libmyth*.dll  | grep -v build | xargs -n1 -i__ cp __ ./build/  
echo Copying share files...
cp -ur /share/mythtv ./build/share/
echo Copying lib files...
cp -ur /lib/mythtv/* ./build/lib/mythtv
cp -ur /usr/lib/mythtv/* ./build/lib/mythtv
'],
  comment => 'write a script to install plugins to build folder'],


# chmod the shell scripts, everytime
[ always  => [],
  shell   => ["cd $mythtv","chmod 755 *.sh"] ],

# Change - don't run this until mythplugins are complete -
#          otherwise dll/exe are copied twice
# Run setup_build.sh which creates the build area and copies executables
[ always  => [], 
  shell   => [$unixmythtv.'setup_build.sh' ], 
  comment => 'Copy mythtv into ./build folder' ],
;

if ($dbconf) {
# --------------------------------
# DB Preperation for developers - need a similar process in the installation
# --------------------------------
# TODO allow a configuration for local SQL server, as well as frontend only -
# with remote SQL will move all testing to vbs script


push @{$expect},
[ dir => $home.'.mythtv', 
  mkdirs => $home.'.mythtv' ] ,
;

 
#execute and capture output: C:\Program Files\MySQL\MySQL Server 5.1\bin\
#                            mysqlshow.exe -u mythtv --password=mythtv
# example response:
# mysqlshow.exe: Can't connect to MySQL server on 'localhost' (10061)
# if this is doing an anonymous connection, so the BEST we should expect is
# an "access denied" message if the server is running properly.
push @{$expect},
[ always => [],
  exec   => [ 'sc query mysql > '.$mythtv.'testmysqlsrv.bat']],
[ grep   => ['SERVICE_NAME',$mythtv.'testmysqlsrv.bat'],
  exec   => ['sc start mysql','nocheck']],
[ grep   => ['does not exist',$mythtv.'testmysqlsrv.bat'],
  exec   => ['C:\Program Files\MySQL\MySQL Server 5.1\bin\MySQLd-nt.exe'.
             ' --standalone  -console','nocheck']],

 
[ always => [], 
  write  => [ $mythtv.'testmysql.bat',
'@echo off
echo testing connection to a local mysql server...
sleep 5
del '.$dosmythtv.'_mysqlshow_err.txt
"C:\Program Files\MySQL\MySQL Server 5.1\bin\mysqlshow.exe" -u mythtv --password=mythtv 2> '.$dosmythtv.'_mysqlshow_err.txt  > '.$dosmythtv.'_mysqlshow_out.txt 
type '.$dosmythtv.'_mysqlshow_out.txt >> '.$dosmythtv.'_mysqlshow_err.txt 
del '.$dosmythtv.'_mysqlshow_out.txt
sleep 1
']],

# try to connect as mythtv/mythtv first (the best case scenario)
[ file    => $mythtv.'skipping_db_tests.txt', 
  exec    => [$mythtv.'testmysql.bat','nocheck'], 
  comment => 'First check - is the local mysql server running, accepting '.
             'connections, and all that? (follow the bouncing ball on '.
             'the install, a standard install is OK, remember the root '.
             'password that you set, start it as a service!)' ],

# if the connection was good, or the permissions were wrong, but the server 
# was there, there's no need to reconfigure the server!
[ grep    => ['(\+--------------------\+|Access denied for user)',
              $mythtv.'_mysqlshow_err.txt'], 
  exec    => ['C:\Program Files\MySQL\MySQL Server 5.1\bin\MySQLd-nt.exe '.
              '--standalone  -console', 'nocheck'], 
  comment => 'See if we couldnt connect to a local mysql server. '.
             'Please re-configure the MySQL server to start as a service.'],

# try again to connect as mythtv/mythtv first (the best case scenario) - the
# connection info should have changed!
[ file    => $mythtv.'skipping_db_tests.txt', 
  exec    => [$mythtv.'testmysql.bat','nocheck'], 
  comment => 'Second check - that the local mysql server '.
             'is at least now now running?' ],


#set/reset mythtv/mythtv password! 
[ always => [], 
  write => [ $mythtv.'resetmythtv.bat',
"\@echo off
echo stopping mysql service:
net stop MySQL
".$dosmsys.'bin\sleep 2'."
echo writing script to reset mythtv/mythtv passwords:
echo USE mysql; >resetmythtv.sql
echo. >>resetmythtv.sql
echo INSERT IGNORE INTO user VALUES ('localhost','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo REPLACE INTO user VALUES ('localhost','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo INSERT IGNORE INTO user VALUES ('\%\%','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo REPLACE INTO user VALUES ('\%\%','mythtv', PASSWORD('mythtv'),'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0,0); >>resetmythtv.sql
echo trying to reset mythtv/mythtv passwords:
\"C:\\Program Files\\MySQL\\MySQL Server 5.1\\bin\\mysqld-nt.exe\" --no-defaults --bind-address=127.0.0.1 --bootstrap --console --skip-grant-tables --skip-innodb --standalone <resetmythtv.sql
del resetmythtv.sql
echo trying to re-start mysql
rem net stop MySQL
net start MySQL
rem so that the server has time to start before we query it again
".$dosmsys.'bin\sleep 5'."
echo.
echo Password for user 'mythtv' was reset to 'mythtv'
echo.
"],
  comment => 'writing a script to create the mysql user (mythtv/mythtv) '.
             'without needing to ask you for the root password ...'],

# reset passwords, this give the mythtv user FULL access to the entire mysql
# instance!
# TODO give specific access to just the the mythconverg DB, as needed.
[ grep    => ['(\+--------------------\+)',$mythtv.'_mysqlshow_err.txt'], 
  exec    => [$dosmythtv.'resetmythtv.bat','nocheck'], 
  comment => 'Resetting the mythtv/mythtv permissions to mysql - '.
             'if the user already has successful login access '.
             'to the mysql server, theres no need to run this' ],


# try again to connect as mythtv/mythtv first (the best case scenario) - the
# connection info should have changed!
[ file    => $mythtv.'skipping_db_tests.txt', 
  exec    => [$mythtv.'testmysql.bat','nocheck'], 
  comment => 'Third check - that the local mysql server '.
             'is fully accepting connections?' ],

# create DB:
# this has the 'nocheck' flag because the creation of the DB doesn't
# instantly reflect in the .txt file we are looking at:
[ grep    => ['mythconverg',$mythtv.'_mysqlshow_err.txt'], 
  exec    => [ 'echo create database mythconverg;'.
               ' | "C:\Program Files\MySQL\MySQL Server 5.1\bin\mysql.exe" '.
               ' -u mythtv --password=mythtv','nocheck'], 
  comment => 'does the mythconverg database exist? (and can this user see it?)'
],

# Make mysql.txt file required for testing
[ file  => $home.'.mythtv\mysql.txt', 
  write => [$home.'.mythtv\mysql.txt', 
'DBHostName=127.0.0.1
DBHostPing=no
DBUserName=mythtv
DBPassword=mythtv
DBName=mythconverg
DBType=QMYSQL3
LocalHostName='.$ENV{COMPUTERNAME}
],
  comment => 'create a mysql.txt file at: %HOMEPATH%\.mythtv\mysql.txt' ],

;
} # end if($dbconf)


if ( grep m/mythplugins/, @components ) {
#----------------------------------------
#  build the mythplugins now:
#----------------------------------------
# 
push @{$expect},
#
# hack location of //include/mythtv/mythconfig.mak
# so that configure is successful
[ always  => [], 
  shell   => ['mkdir /include/mythtv',
             'cp '.$unixbuild.'include/mythtv/mythconfig.mak'.
               ' /include/mythtv/mythconfig.mak'], 
  comment => 'link mythconfig.mak'],
## config:
[ file    => $mythtv.'mythplugins/Makefile', 
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
             'cd '.$unixmythtv.'mythplugins',
             './configure --prefix='.$unixbuild.
             ' --disable-mythgallery --disable-mythmusic'.
             ' --disable-mytharchive --disable-mythbrowser --disable-mythflix'.
             ' --disable-mythgame --disable-mythnews --disable-mythphone'.
             ' --disable-mythcontrols'.
             ' --disable-mythzoneminder --disable-mythweb --enable-aac'.
             ' --enable-libvisual --enable-fftw --compile-type='.$compile_type,
             #'touch '.$unixmythtv.'mythplugins/cleanup/cleanup.pro'
             ], 
  comment => 'do we already have a Makefile for myth plugins?' ],
  
#[ pause => 'how does the mythplugins Makefile look? '.
#           '(did we get any errors on screen?)'],


[ grep    => ['win32:DEPENDS', $mythtv.'mythplugins/settings.pro'], 
  shell   => ['echo \'win32:DEPENDS += ./\' >> '.
              $mythtv.'mythplugins/settings.pro','nocheck'], 
  comment => 'fix settings.pro' ],

#hack mythconfig.mak to remove /usr
[ grep    => ['LIBDIR=/lib', $mythtv.'mythplugins/mythconfig.mak'], 
  shell   => ['cd '.$unixmythtv.'mythplugins',
              'sed -e \'s|/usr||\' mythconfig.mak > mythconfig2.mak',
              'cp mythconfig2.mak mythconfig.mak', 'nocheck'], 
  comment => 'hack mythconfig.mak'],

# make
[ newer   => [$mythtv.'mythplugins/mythmovies/mythmovies/libmythmovies.dll',
              $mythtv.'mythtv/last_build.txt'], 
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
              'cd '.$unixmythtv.'mythplugins', $parallelMake], 
  comment => 'PLUGINS! redo make if we need to '.
             '(see the  last_build.txt identifier)' ],

# make cleanup/cleanup.pro as install fails without it
#[ file    => $mythtv.'mythplugins/cleanup/cleanup.pro', 
#  shell   => ['touch '.$unixmythtv.'mythplugins/cleanup/cleanup.pro',
#              'nocheck'], 
#  comment => 'make cleanup.pro'],

# make install
[ newer   => [$mythtv.'build/bin/mythtv/plugins/libmythmovies.dll',
              $mythtv.'mythplugins/mythmovies/mythmovies/libmythmovies.dll'],
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
              'cd '.$unixmythtv.'mythplugins','make install'],
  comment => 'PLUGINS! make install' ],

# Copy to build area
#[ newer => [$mythtv.'build/lib/mythtv/plugins/libmythmovies.dll',
#            $msys.'lib/mythtv/plugins/libmythmovies.dll'],
#  shell => [$unixmythtv.'setup_plugins.sh'],
#  comment => 'Copy mythplugins to ./build folder' ],

# Qt3 can't cope with wildcards in the install targets:
[ file   => $build.'/share/mythtv/videomenu.xml',
  shell  => ['cd '.$unixmythtv.'mythplugins',
             'cp mythcontrols/mythcontrols/controls-ui.xml'.
             '   mythcontrols/images/*.png '.
             '   mythvideo/theme/default/*.xml'.
             '   mythvideo/theme/default/images/*.png'.
             '   mythweather/mythweather/weather-ui.xml'.
             '   mythweather/mythweather/images/*.png'.
             ' '.$unixbuild.'/share/mythtv/themes/default',
             'cp mythmovies/mythmovies/theme-wide/*.xml'.
             '   mythvideo/theme/default-wide/*.xml'.
             '   mythvideo/theme/default-wide/images/*.png'.
             '   mythweather/mythweather/theme-wide/weather-ui.xml'.
             '   mythweather/mythweather/theme-wide/images/*.png'.
             ' '.$unixbuild.'/share/mythtv/themes/default-wide',
             'cp mythweather/mythweather/weather_settings.xml'.
             '   mythvideo/theme/menus/*.xml'.
             ' '.$unixbuild.'/share/mythtv',
             'mkdir -p '.$unixbuild.'/share/mythtv/mythvideo/scripts',
             'cp mythvideo/mythvideo/scripts/*'.
             ' '.$unixbuild.'/share/mythtv/mythvideo/scripts',
             'mkdir -p '.$unixbuild.'/share/mythtv/mythweather/scripts',
             'cp mythweather/mythweather/weather-screens.xml'.
             ' '.$unixbuild.'/share/mythtv/mythweather',
             'cp -pr mythweather/mythweather/scripts/* '.
             ' '.$unixbuild.'/share/mythtv/mythweather/scripts']
]
;
}


if ( grep m/myththemes/, @components ) {
# -------------------------------
# Make themes
# -------------------------------
push @{$expect},
## config:
[ file    => $mythtv.'myththemes/Makefile',
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'myththemes','./configure --prefix='.$unixbuild],
  comment => 'do we already have a Makefile for myththemes?' ],

## fix myththemes.pro
[ grep    => ['^win32:QMAKE_INSTALL_DIR', $mythtv.'myththemes/myththemes.pro'], 
  shell   => ['echo \'win32:QMAKE_INSTALL_DIR = sh ./cpsvndir\' >> '.
              $mythtv.'myththemes/myththemes.pro','nocheck'], 
  comment => 'fix myththemes.pro' ],
[ grep    => ['win32:DEPENDS', $mythtv.'myththemes/myththemes.pro'], 
  shell   => ['echo \'win32:DEPENDS += ./\' >> '.
              $mythtv.'myththemes/myththemes.pro','nocheck'], 
  comment => 'fix myththemes.pro' ],

#hack mythconfig.mak to remove /usr
[ grep    => ['LIBDIR=/lib', $mythtv.'myththemes/mythconfig.mak'], 
  shell   => ['cd '.$unixmythtv.'myththemes',
             'sed -e \'s|/usr||\' mythconfig.mak > mythconfig2.mak',
             'cp mythconfig2.mak mythconfig.mak', 'nocheck'], 
  comment => 'hack mythconfig.mak'],

## make
[ dir     => [$mythtv.'/build/share/mythtv/themes/Retro'], 
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
              'cd '.$unixmythtv.'myththemes','make', 'make install'], 
  comment => 'THEMES! redo make if we need to '.
             '(see the  last_build.txt identifier)' ],
  

# Get any extra Themes
#

[ archive => $sources.'MePo-wide-0.45.tar.gz',  
  fetch   => 'http://home.comcast.net/~zdzisekg/'.
             'download/MePo-wide-0.45.tar.gz'],
[ dir     => $sources.'MePo-wide', 
  extract => $sources.'MePo-wide-0.45.tar.gz' ],
[ file    => $mythtv.'build/share/mythtv/themes/MePo-wide/ui.xml', 
  shell   => ['cp -fr '.$unixsources.'MePo-wide '.
                      $unixmythtv.'build/share/mythtv/themes','nocheck'], 
  comment => 'install MePo-wide'],

# Move ttf fonts to font directory
[ always  => [],
  shell   => ['source '.$unixmythtv.'qt'.$qtver.'_env.sh',
            'cd '.$unixmythtv.'myththemes',
            'find '.$unixmythtv.'build/share/mythtv/themes/ -name "*.ttf"'.
            ' | xargs -n1 -i__ cp __ '.$unixmythtv.'build/share/mythtv'],
  comment => 'move ttf files'],
  
  
# install fixup for redundant themes folders/folders/folders:
[ always => '',
  shell => [ "cd $unixbuild/share/mythtv/themes",
             'for f in *; do mv $f/$f/* ./$f; done' ],
  comment => 'relocate badly installed THEMES! ' ],

;
}


# -------------------------------
# Prepare Readme.txt for distribution file - temporary for now
# -------------------------------


push @{$expect},
[ file => $mythtv.'build/readme.txt',
 write => [$mythtv.'build/readme.txt',
'README for Win32 Installation of MythTV version: '.$version.
' svn '.$SVNRELEASE.'
=============================================================
The current installation very basic:
 - All exe and dlls are %PROGRAMFILES%\mythtv\bin
 - share/mythtv is copied to %PROGRAMFILES%\mythtv\share\mythtv
 - lib/mythtv is copied to %PROGRAMFILES%\mythtv\lib\mythtv
 - mysql.txt and user configuration files are set to %APPDATA%\mythtv

If you have MYSQL installed locally, mysql.txt will be configured to use it.
If you want to run the frontend with a remote MYSQL server,
edit mysql.txt in %APPDATA%\mythtv

If you don\'t have MYSQL installed locally, only the frontend will be installed
and mysql.txt needs to be configured to the backend IP address.

If you install the components in other directories other than the default,
some environment variables will need to be set:
MYTHCONFDIR = defaulting to %APPDATA%\mythtv
MYTHLIBDIR  = defaulting to %PROGRAMFILES%\mythv\lib
MYTHTVDIR   = defaulting to %APPDATA%\mythtv
','nocheck'],comment => ''],

[ file => $mythtv.'build/readme.txt_', 
  shell => ['unix2dos '.$unixmythtv.'build/readme.txt', 'nocheck'],
  comment => '' ],
;

if ($package == 1) {
    push @{$expect},
    # Create directories
    [ dir     => [$mythtv.'setup'] , 
      mkdirs => [$mythtv.'setup'],
      comment => 'Create Packaging directory'],
    [ dir     => [$mythtv.'build/isfiles'] , 
      mkdirs => [$mythtv.'build/isfiles'],
      comment => 'Create Packaging directory'],
    # Move required files from inno setup to setup directory
    [ file    => $mythtv."build/isfiles/UninsHs.exe",
      exec    => 'copy /Y "C:\Program Files\Inno Setup 5\UninsHs.exe" '.
                 $dosmythtv.'build\isfiles\UninsHs.exe',
      comment => 'Copy UninsHs to setup directory' ],
    [ file    => $mythtv."build/isfiles/isxdl.dll",
      exec    => 'copy /Y "C:\Program Files\ISTool\isxdl.dll" '.
                 $dosmythtv.'build\isfiles\isxdl.dll',
      comment => 'Copy isxdl.dll to setup directory' ],
    [ file    => $mythtv."build/isfiles/WizModernSmallImage-IS.bmp",
      exec    => 'copy /Y "C:\Program Files\Inno Setup 5\'WizModernSmallImage-IS.bmp" '.
                 $dosmythtv.'build\isfiles\WizModernSmallImage-IS.bmp',
      comment => 'Copy WizModernSmallImage-IS.bmp to setup directory' ],
    # Copy required files from sources or packaging to setup directory:
    [ filesame => [$mythtv.'build/isfiles/mythtvsetup.iss',
                   $mythtv.'packaging/win32/build/mythtvsetup.iss'],
      copy     => [''=>'', 
      comment  => 'mythtvsetup.iss'] ],
    [ filesame => [$mythtv.'build/isfiles/mysql.gif',
                   $sources.'logo_mysql_sun.gif'],
      copy     => [''=>'', 
      comment  => 'mysql.gif'] ],
    # Create on-the-fly  files required
    [ file     => $mythtv.'build/isfiles/configuremysql.vbs',
      write    => [$mythtv.'build/isfiles/configuremysql.vbs',
'WScript.Echo "Currently Unimplemented"
' ], 
      comment   => 'Write a VB script to configure MySQL' ],
    [ always   => [],
      write    => [$mythtv.'build/isfiles/versioninfo.iss', '
#define MyAppName      "MythTv"
#define MyAppVerName   "MythTv '.$version.'(svn_'.$SVNRELEASE .')"
#define MyAppPublisher "Mythtv"
#define MyAppURL       "http://www.mythtv.org"
#define MyAppExeName   "Win32MythTvInstall.exe"
' ], 
      comment  => 'write the version information for the setup'],
    [ file     => $mythtv.'genfiles.sh', 
      write    => [$mythtv.'genfiles.sh','
cd '.$unixmythtv.'build
find . -type f -printf "Source: '.$mythtv.'build/%h/%f; Destdir: {app}/%h\n" | sed "s/\.\///" | grep -v ".svn" | grep -v "isfiles" | grep -v "include" > '.$unixmythtv.'/build/isfiles/files.iss
',], 
      comment  => 'write script to generate setup files'], 
    [ newer    => [$mythtv.'build/isfiles/files.iss',
                   $mythtv.'mythtv/last_build.txt'],
      shell    => [$unixmythtv.'genfiles.sh'] ],
# Run setup
#    [ newer   => [$mythtv.'setup/MythTvSetup.exe',
#                   $mythtv.'mythtv/last_build.txt'],
#      exec    => ['"c:\Program Files\Inno Setup 5\Compil32.exe" /cc "'.
#                  $dosmythtv.'build\isfiles\mythtvsetup.iss"' ]],
    [ newer    => [$mythtv.'setup/MythTvSetup.exe',
                    $mythtv.'mythtv/last_build.txt'],
      exec     => ['cd '.$dosmythtv.'build\isfiles && '.
                   '"c:\Program Files\Inno Setup 5\iscc.exe" "'.
                   $dosmythtv.'build\isfiles\mythtvsetup.iss"' ]],

    ;
}


#------------------------------------------------------------------------------

; # END OF CAUSE->ACTION DEFINITIONS

#------------------------------------------------------------------------------

sub _end {
    comment("This version of the Win32 Build script ".
            "last was last tested on: $SVNRELEASE");

    print << 'END';    
#
# SCRIPT TODO/NOTES:  - further notes on this scripts direction....
END
}

#------------------------------------------------------------------------------

# this is the mainloop that iterates over the above definitions and
# determines what to do:
# cause:
foreach my $dep ( @{$expect} ) { 
    my @dep = @{$dep};

    #print Dumper(\@dep);

    my $causetype = $dep[0];
    my $cause =  $dep[1];
    my $effecttype = $dep[2];
    my $effectparams = $dep[3] || '';
    die "too many parameters in cause->event declaration (".join('|',@dep).")"
        if defined $dep[4] && $dep[4] ne 'comment'; 
    # four pieces: cause => [blah] , effect => [blah]

    my $comment = $dep[5] || '';

    if ( $comment && $NOISY ) {
        comment($comment);
    }

    my @cause;
    if (ref($cause) eq "ARRAY" ) {
        @cause = @{$cause};
    } else { 
        push @cause, $cause ;
    }

    # six pieces: cause => [blah] , effect => [blah] , comment => ''
    die "too many parameters in cause->event declaration (@dep)"
        if defined $dep[6];

    my @effectparams = ();
    if (ref($effectparams) eq "ARRAY" ) {
        @effectparams = @{$effectparams};
    } else { 
        push @effectparams, $effectparams ;
    }
    # if a 'nocheck' parameter is passed through, dont pass it through to
    # the 'effect()', use it to NOT check if the file/dir exists at the end.
    my @nocheckeffectparams = grep { ! /nocheck/i } @effectparams; 
    my $nocheck = 0;
    if ( $#nocheckeffectparams != $#effectparams ) { $nocheck = 1; } 

    if ( $causetype eq 'archive' ) {
        die "archive only supports type fetch ($cause)($effecttype)"
            unless $effecttype eq 'fetch';
        if ( -f $cause[0] ) {print "file exists: $cause[0]\n"; next;}
        # 2nd and 3rd params get squashed into
        # a single array on passing to effect();
        effect($effecttype,$cause[0],@nocheckeffectparams);
        if ( ! -f $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to ".
                "locate expected file ($cause[0]) that was to be ".
                "fetched from $nocheckeffectparams[0]\n";
        }

    }   elsif ( $causetype eq 'dir' ) {
        if ( -d $cause[0] ) {
            print "directory exists: $causetype,$cause[0]\n"; next;
        }
        effect($effecttype,@nocheckeffectparams);
        if ( ! -d $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to ".
                "locate expected directory ($cause[0]).\n";
        }

    } elsif ( $causetype eq 'file' ) {
        if ( -f $cause[0] ) {print "file already exists: $cause[0]\n"; next;}
        effect($effecttype,@nocheckeffectparams);
        if ( ! -f $cause[0] && $nocheck == 0) {
            die "EFFECT FAILED ($causetype -> $effecttype): unable to ".
                "locate expected file ($cause[0]).\n";
        }
    } elsif ( $causetype eq 'filesame' ) {
        # NOTE - we check file mtime, byte size, AND MD5 of contents
        #      as without the MD5, the script can break in some circumstances.
        my ( $size,$mtime,$md5)=fileinfo($cause[0]);
        my ( $size2,$mtime2,$md5_2)=fileinfo($cause[1]);
        if ( $mtime != $mtime2 || $size != $size2 || $md5 ne $md5_2 ) {
          if ( ! $nocheckeffectparams[0] ) {
            die "sorry but you can not leave the arguments list empty for ".
                "anything except the 'copy' action (and only when used with ".
                "the 'filesame' cause)" unless $effecttype eq 'copy';
            print "no parameters defined, so applying effect($effecttype) as ".
                  "( 2nd src parameter => 1st src parameter)!\n";
            effect($effecttype,$cause[1],$cause[0]); 
          } else {
            effect($effecttype,@nocheckeffectparams);
            # do something else if the files are not 100% identical?
            if ( $nocheck == 0 ) {
              # now verify the effect was successful
              # at matching the file contents!:
              undef $size; undef $size2;
              undef $mtime; undef $mtime2;
              undef $md5; undef $md5_2;
              ( $size,$mtime,$md5)=fileinfo($cause[0]);
              ( $size2,$mtime2,$md5_2)=fileinfo($cause[1]);
              if ( $mtime != $mtime2 || $size != $size2 || $md5 ne $md5_2 ) {
                die("effect failed, files NOT identical size,mtime, ".
                    "and MD5: ($cause[0] => $cause[1]).\n");
              } else {
                print "files ($cause[0] => $cause[1]) now ".
                      "identical - successful! \n";
              }
            }
          }  
        }else {
           print "effect not required files already up-to-date/identical: ".
                 "($cause[0] => $cause[1]).\n";
        }
        undef $size; undef $size2;
        undef $mtime; undef $mtime2;
        undef $md5; undef $md5_2;
        
    } elsif ( $causetype eq 'newer' ) {
        my $mtime = 0;
        if ( -f $cause[0] ) {
          $mtime = (stat($cause[0]))[9]
                   || warn("$cause[0] could not be stated");
        }
        if (! ( -f $cause[1]) ) {
            die "cause: $causetype requires it's SECOND filename to exist ".
                "for comparison: $cause[1].\n";
        }
        my $mtime2  = (stat($cause[1]))[9];
        if ( $mtime < $mtime2 ) {
          effect($effecttype,@nocheckeffectparams);
          if ( $nocheck == 0 ) {
            # confirm it worked, mtimes should have changed now: 
            my $mtime3 = 0;
            if ( -f $cause[0] ) {
              $mtime3   = (stat($cause[0]))[9];
            }
            my $mtime4  = (stat($cause[1]))[9];
            if ( $mtime3 < $mtime4  ) {
                die "EFFECT FAILED ($causetype -> $effecttype): mtime of file".
                    " ($cause[0]) should be greater than file ($cause[1]).\n".
                    "[$mtime3]  [$mtime4]\n";
            }
          }
        } else {
           print "file ($cause[0]) has same or newer mtime than ".
                 "($cause[1]) already, no action taken\n";
        } 
        undef $mtime;
        undef $mtime2;
        
    } elsif ( $causetype eq 'grep' ) {
        print "grep-ing for pattern($cause[0]) in file($cause[1]):\n"
            if $NOISY >0;
        if ( ! _grep($cause[0],$cause[1]) ) {
# grep actually needs two parameters on the source side of the action
            effect($effecttype,@nocheckeffectparams);   
        } else {
            print "grep - already matched source file($cause[1]), ".
                  "with pattern ($cause[0]) - no action reqd\n";
        }
        if ( (! _grep($cause[0],$cause[1])) && $nocheck == 0 ) { 
           die "EFFECT FAILED ($causetype -> $effecttype): unable to locate regex pattern ($cause[0]) in file ($cause[1])\n";
        }

    } elsif ( $causetype eq 'exists' ) {
        print "testing if '$cause[0]' exists...\n" if $NOISY >0;
        if ( -e $cause[0] ) {
            effect($effecttype,@nocheckeffectparams);
        }
    } elsif ( $causetype eq 'always' ) {
        effect($effecttype,@nocheckeffectparams);
    } elsif ( $causetype eq 'stop' ){
        die "Stop found \n";
    } elsif ( $causetype eq 'pause' ){
        comment("PAUSED! : ".$cause);
        my $temp = getc() unless $continuous;
    } else {
        die " unknown causetype $causetype \n";
    }
}
print "\nwin32-packager all done\n";
_end();

#------------------------------------------------------------------------------
# each cause has an effect, this is where we do them:
sub effect {
    my ( $effecttype, @effectparams ) = @_;

        if ( $effecttype eq 'fetch') {
            # passing two parameters that came in via the array
            _fetch(@effectparams);

        } elsif ( $effecttype eq 'extract') {
            my $tarfile = $effectparams[0];
            my $destdir = $effectparams[1] || '';
            if ($destdir eq '') {
                $destdir = $tarfile;
                # strip off everything after the final forward slash
                $destdir =~ s#[^/]*$##;
            }
        my $t = findtar($tarfile);
        print "found equivalent: ($t) -> ($tarfile)\n" if $t ne $tarfile;
        print "extracttar($t,$destdir);\n";
        extracttar($t,$destdir);

        } elsif ($effecttype eq 'exec') { # execute a DOS command
            my $cmd = shift @effectparams;
            #print `$cmd`;
            print "exec:$cmd\n";
            open F, $cmd." |"  || die "err: $!";
            while (<F>) {
                print;
            }   

        } elsif ($effecttype eq 'shell') {
            shell(@effectparams);
            
        } elsif ($effecttype eq 'copy') {
            die "Can not copy non-existant file ($effectparams[0])\n"
                unless -f $effectparams[0];
            print "copying file ($effectparams[0] => $effectparams[1]) \n";
            cp($effectparams[0],$effectparams[1]);
            # make destn mtime the same as the original for ease of comparison:
            shell("touch --reference='".perl2unix($effectparams[0])."' '"
                                       .perl2unix($effectparams[1])."'");

        } elsif ($effecttype eq 'mkdirs') {
            mkdirs(shift @effectparams);

        } elsif ($effecttype eq 'write') {
            # just dump the requested content from the array to the file.
            my $filename = shift @effectparams;
            my $fh = new IO::File ("> $filename")
                || die "error opening $filename for writing: $!\n";
            $fh->binmode();
            $fh->print(join('',@effectparams));
            $fh->close();

        } else {
            die " unknown effecttype $effecttype from cause 'dir'\n";
        }
        return; # which ever one we actioned,
                # we don't want to action anything else 
}

#------------------------------------------------------------------------------
# get info from a file for comparisons
sub fileinfo {
    # filename passed in should be perl compatible path
    # using single FORWARD slashes
    my $filename = shift;

    my ( $size,$mtime,$md5)=(0,0,0);
    
    if ( -f $filename ) {
        $size = (stat($filename))[7];
        $mtime  = (stat($filename))[9];
        my $md5obj = Digest::MD5->new();
        my $fileh = IO::File->new($filename);
        binmode($fileh);
        $md5obj->addfile($fileh);
        $md5 = $md5obj->digest();
    } else {
        warn(" invalid file name provided for testing: ($filename)\n");
        $size=rand(99);
        $mtime=rand(999);
        $md5=rand(999);
        }

    #print "compared: $size,$mtime,$md5\n";
    return ($size,$mtime,$md5);
}

#------------------------------------------------------------------------------
# kinda like a directory search for blah.tar* but faster/easier.
#  only finds .tar.gz, .tar.bz2, .zip
sub findtar {
    my $t = shift;
    return "$t.gz" if -f "$t.gz";
    return "$t.bz2" if -f "$t.bz2";

    if ( -f "$t.zip" || $t =~ m/\.zip$/ ) {
        die "no unzip.exe found ! - yet" unless -f $sources."unzip/unzip.exe";
        # TODO - a bit of a special test, should be fixed better.
        return "$t.zip" if  -f "$t.zip";
        return $t if -f $t;
    }
    return $t if -f $t;
    die "findtar failed to match a file from:($t)\n";
}

#------------------------------------------------------------------------------
# given a ($t) .tar.gz, .tar.bz2, .zip extract it to the directory ( $d)
# changes current directory to $d too
sub extracttar {
    my ( $t, $d) = @_;

    # both $t and $d at this point should be perl-compatible-forward-slashed
    die "extracttar expected forward-slashes only in pathnames ($t,$d)"
        if $t =~ m#\\# || $d =~ m#\\#;

    unless ( $t =~ m/zip/ ) {
        # the unzip tool need the full DOS path,
        # the msys commands need that stripped off.
        $t =~ s#^$msys#/#i;
    }

    print "extracting to: $d\n";

    # unzipping happens in DOS as it's a dos utility:
    if ( $t =~ /\.zip$/ ) {
        #$d =~ s#/#\\#g;  # the chdir command MUST have paths with backslashes,
                          # not forward slashes. 
        $d = perl2dos($d);
        $t = perl2dos($t);
        my $cmd = 'chdir '.$d.' && '.$dossources.'unzip\unzip.exe -o '.$t;
        print "extracttar:$cmd\n";
        open F, "$cmd |"  || die "err: $!";
        while (<F>) {
            print;
        }
        return; 
    }

    $d = perl2unix($d);
    $t = perl2unix($t);
    # untarring/gzipping/bunzipping happens in unix/msys mode:
    die "unable to locate sh2.exe" unless -f $dosmsys.'bin/sh2.exe';
    my $cmd = $dosmsys.
              'bin\sh2.exe -c "( export PATH=/bin:/mingw/bin:$PATH ; ';
    $cmd .= "cd $d;";
    if ( $t =~ /\.gz$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -zxvpf $t";
    } elsif ( $t =~ /\.bz2$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -jxvpf $t";
    } elsif ( $t =~ /\.tar$/ ) {
        $cmd .= $unixmsys."bin/tar.exe -xvpf $t";
    } else {
        die  "extract tar failed on ($t,$d)\n";
    }
    $cmd .= ')"'; # end-off the brackets around the shell commands.

    # execute the cmd, and capture the output!  
    # this is a glorified version of "print `$cmd`;"
    # except it doesn't buffer the output, if $|=1; is set.
    # $t should be a msys compatible path ie /sources/etc
    print "extracttar:$cmd\n";
    open F, "$cmd |"  || die "err: $!";
    while (<F>) {
        print;
    }   
}

#------------------------------------------------------------------------------
# get the $url (typically a .tar.gz or similar) , and save it to $file
sub _fetch {
    my ( $file,$url ) = @_;

    #$file =~ s#/#\\\\#g;
    print "already exists: $file \n" if -f $file;
    return undef if -f $file;

    print "fetching $url to $file (please wait)...\n";
    my $ua = LWP::UserAgent->new;
    $ua->proxy(['http', 'ftp'], $proxy);

    my $req = HTTP::Request->new(GET => $url);
    my $res = $ua->request($req);

    if ($res->is_success) {
        my $f = new IO::File "> $file" || die "_fetch: $!\n";
        $f->binmode();
        $f->print($res->content);
        $f->close();
    }
    if ( ! -s $file ) {
      die ('ERR: Unable to automatically fetch file!\nPerhaps manually '.
           'downloading from the URL to the filename (both listed above) '.
           'might work, or you might want to choose your own SF mirror '.
           '(edit this script for instructions), or perhaps this version'.
           ' of the file is no longer available.');
    }
}

#------------------------------------------------------------------------------
# execute a sequence of commands in a bash shell.
# we explicitly add the /bin and /mingw/bin to the path
# because at this point they aren't likely to be there
# (cause we are in the process of installing them)
sub shell {
    my @cmds = @_;
    my $cmd = $dosmsys.'bin\bash.exe -c "( export PATH=/bin:/mingw/bin:$PATH;'.
              join(';',@cmds).') 2>&1 "';
    print "shell:$cmd\n";
    # execute the cmd, and capture the output!  this is a glorified version
    # of "print `$cmd`;" except it doesn't buffer the output if $|=1; is set.
    open F, "$cmd |"  || die "err: $!";
    while (<F>) {
        if (! $NOISY )  {
          # skip known spurious messages from going to the screen unnecessarily
          next if /redeclared without dllimport attribute after being referenced with dllimpo/;
          next if /declared as dllimport: attribute ignored/;
          next if /warning: overriding commands for target `\.\'/;
          next if /warning: ignoring old commands for target `\.\'/;
          next if /Nothing to be done for `all\'/;
          next if /^cd .* \&\& \\/;
          next if /^make -f Makefile/;
        }
        print;
    }
}

#------------------------------------------------------------------------------
# recursively make folders, requires perl-compatible folder separators
# (ie forward slashes)
# 
sub mkdirs {
    my $path = shift;
    die "backslash in foldername not allowed in mkdirs function:($path)\n"
        if $path =~ m#\\#;
    $path = perl2dos($path);
    print "mkdir $path\n";
    # reduce path to just windows,
    # incase we have a rogue unix mkdir command elsewhere!
    print `set PATH=C:\\WINDOWS\\system32;c:\\WINDOWS;C:\\WINDOWS\\System32\\Wbem && mkdir $path`;

}

#------------------------------------------------------------------------------
# unix compatible  versions of the perl paths (for when we [shell] to unix/bash mode):
sub perl2unix  {
    my $p = shift;
    print "perl2unix: $p\n";
    $p =~ s#$msys#/#i;  # remove superfluous msys folders if they are there

    #change c:/ into /c  (or a D:)   so c:/msys becomes /c/msys etc.
    $p =~ s#^([CD]):#/$1#ig;
    $p =~ s#//#/#ig; # reduce any double forward slashes to single ones.
    return $p;
}

#------------------------------------------------------------------------------
# DOS executable CMD.exe versions of the paths (for when we [exec] to DOS mode):
sub perl2dos {
    my $p = shift;
    $p =~ s#/#\\#g; # single forward to single backward
    return $p;
}

#------------------------------------------------------------------------------
# find a pattern in a file and return if it was found or not.
# Absent file assumes pattern was not found.
sub _grep {
    my ($pattern,$file) = @_;
    #$pattern = qw($pattern);
    
    my $fh = IO::File->new("< $file");
    unless ( $fh) {
        print "WARNING: Unable to read file ($file) when searching for ".
              "pattern:($pattern), assumed to NOT match pattern\n";
        return 0;
    }
    my $found = 0;
    while ( my $contents = <$fh> ) {
        if ( $contents =~ m/$pattern/ ) { $found= 1; }
    }
    $fh->close();
    return $found;
}

#------------------------------------------------------------------------------
# where is this script? 
sub scriptpath {
  return "" if ($0 eq "");
  my @path = split /\\/, $0;
  
  pop @path;
  return join '\\', @path;
}

#------------------------------------------------------------------------------
# display stuff to the user in a manner that unclutters it from
# the other compilation messages that will be scrolling past! 
sub comment {
    my $comment = shift;
    print "\nCOMMENTS:";
    print "-"x30;
    print "\n";
    print "COMMENTS:$comment\nCOMMENTS:";
    print "-"x30;
    print "\n";
    print "\n";
}

#------------------------------------------------------------------------------
# how? what the?   oh! like that!  I get it now, I think.
sub usage {
    print << 'END_USAGE';

-h             This message
-v             Verbose output
-u all|mythtv|mythplugins|myththemes
               Revert instead of checkout !!
-k             Package win32 distribution
-p proxy:port  Your proxy
-r XXXX|head   Your prefered revision (to change revision)
-b             Checkout release-0-xx-fixes instead of trunk
-c debug|release|profile
               Compile type for mythtv/mythplugins [default debug]
-t             Apply post $SVNRELEASE patches [default off]
-l             Make clean
-d             Configure Database [default off]
END_USAGE
    exit;
}

#------------------------------------------------------------------------------
