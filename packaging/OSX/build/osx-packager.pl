#!/usr/bin/perl

### = file
### osx-packager.pl
###
### = revision
### $Id$
###
### = location
### http://svn.mythtv.org/svn/trunk/packaging/OSX/build/osx-packager.pl
###
### = description
### Tool for automating frontend builds on Mac OS X.
### Run "osx-packager.pl -man" for full documentation.

use strict;
use Getopt::Long qw(:config auto_abbrev);
use Pod::Usage ();
use Cwd ();

### Configuration settings (stuff that might change more often)

# We try to auto-locate the Subversion client binaries.
# If they are not in your path, we build them from source
#
our $svn = `which svn`; chomp $svn;

# This script used to always delete the installed include and lib dirs.
# That probably ensures a safe build, but when rebuilding adds minutes to
# the total build time, and prevents us skipping some parts of a full build
#
our $cleanLibs = 1;

# By default, only the frontend is built (i.e. no backend or transcoding)
#
our $backend = 0;
our $jobtools = 0;

# Parallel makes?
#
#$ENV{'DISTCC_HOSTS'}   = "localhost 192.168.0.6";
#$ENV{'DISTCC_VERBOSE'} = '1';

# Start with a generic address and let sourceforge 
# figure out which mirror is closest to us.
#
our $sourceforge = 'http://downloads.sourceforge.net';

# At the moment, there is mythtv plus these two
our @components = ( 'myththemes', 'mythplugins' );

# The OS X programs that we are likely to be interested in.
our @targetsJT = ( 'MythCommFlag',  'MythJobQueue');
our @targetsBE = ( 'MythBackend',   'MythFillDatabase',
                   'MythTranscode', 'MythTV-Setup');

# Patches for MythTV source
our %patches = (); 

our %depend_order = (
  'mythtv'
  =>  [
        'freetype',
        'lame',
        'mysqlclient',
        'qt',
      ],
  'mythplugins'
  =>  [
        'exif',
        'dvdcss',
# MythMusic needs these:
        'libmad',
        'taglib',
        'libogg',
        'vorbis',
        'flac',
      ],
);

our %depend = (

  'svndeps' =>
  {
    'url'
    => 'http://subversion.tigris.org/downloads/subversion-deps-1.4.3.tar.bz2',
    'skip'
    => 'yes'   # Don't actually untarr/configure/make.
  },

  'svn' =>
  {
    'url'
    => 'http://subversion.tigris.org/downloads/subversion-1.4.3.tar.bz2',
    'pre-conf'
    => 'cd .. ; tar -xjf subversion-deps-1.4.3.tar.bz2',
    'conf'
    =>  [
           '--disable-keychain',  # Workaround a 10.3 build problem
           "MAKEFLAGS=\$parallel_make_flags"   # For builds of deps
        ],
    # For some reason, this dies when the neon sub-make ends
    #'parallel-make'
    #=> 'yes'
  },

  'freetype' =>
  {
    'url' =>  "$sourceforge/sourceforge/freetype/freetype-2.1.10.tar.gz",
  },

  'lame' =>
  {
    'url'
    =>  "$sourceforge/sourceforge/lame/lame-3.96.1.tar.gz",
    'conf'
    =>  [
          '--disable-frontend',
        ],
  },

  'libmad' =>
  {
    'url' => "$sourceforge/sourceforge/mad/libmad-0.15.0b.tar.gz"
  },

  'taglib' =>
  {
    'url' => 'http://developer.kde.org/~wheeler/files/src/taglib-1.4.tar.gz',
  },

  'libogg' =>
  {
    'url' => 'http://downloads.xiph.org/releases/ogg/libogg-1.1.2.tar.gz'
  },

  'vorbis' =>
  {
    'url' => 'http://downloads.xiph.org/releases/vorbis/libvorbis-1.1.1.tar.gz'
  },

  'flac' =>
  {
    'url' => "$sourceforge/sourceforge/flac/flac-1.1.4.tar.gz",
    # Workaround Intel problem - Missing _FLAC__lpc_restore_signal_asm_ia32
    'conf' => [ '--disable-asm-optimizations' ]
  },

  'dvdcss' =>
  {
    'url'
    =>  'http://download.videolan.org/pub/videolan/libdvdcss/1.2.9/libdvdcss-1.2.9.tar.bz2'
  },

  'mysqlclient' =>
  {
    'url' 
    => 'http://mysql.mirrors.pair.com/Downloads/MySQL-5.0/mysql-5.0.75.tar.gz',
    'conf'
    =>  [
          '--without-debug',
          '--without-docs',
          '--without-man',
          '--without-bench',
          '--without-server',
          '--without-geometry',
          '--without-extra-tools',
        ],
  },
  
  'qt'
  =>
  {
    'url'
    =>  'http://ftp.iasi.roedu.net/mirrors/ftp.trolltech.com/qt/source/qt-mac-opensource-src-4.3.1.tar.gz',
    'url'
    =>  'ftp://ftp.trolltech.com/qt/source/qt-mac-opensource-src-4.3.4.tar.gz',
    'url'
    =>  'http://wftp.tu-chemnitz.de/pub/Qt/qt/source/qt-mac-opensource-src-4.3.4.tar.gz',
    #'url'
    #=>  'http://ftp3.ie.freebsd.org/pub/trolltech/pub/qt/source/qt-mac-opensource-src-4.4.0.tar.bz2',
    'conf-cmd'
    =>  'echo yes | MAKEFLAGS=$parallel_make_flags ./configure',
    'conf'
    =>  [
          '-prefix', '"$PREFIX"',
          '-release',
          '-fast',
          #'-no-exceptions',
          '-no-accessibility',
          '-no-stl',
          # When MythTV all ported:  '-no-qt3support',
          '-I"$PREFIX/include/mysql"',
          '-L"$PREFIX/lib/mysql"',
          '-qt-sql-mysql',
          '-no-sql-sqlite',
          '-no-sql-odbc',
          '-system-zlib',
          '-qt-libjpeg',
          '-no-libtiff',
          '-no-libmng',
          '-nomake examples -nomake demos',
          '-no-nis',
          '-no-cups',
          '-no-qdbus',
          '-no-framework',
       ],
    'make'
    =>  [
                                               #'sub-moc',
                                               #'sub-rcc',
                                               #'sub-uic',
                                               #'sub-corelib',
                                               #'sub-xml',
                                               #'sub-gui',
                                               #'sub-sql',
                                               #'sub-network',
                                               #'sub-svg',
                                               #'sub-script',
                                               #'sub-opengl',
          # When MythTV is all ported, change this to opengl?
          'sub-qt3support-install_subtargets-ordered',
                                               #'sub-uic3',
                                               #'sub-plugins',
          # For lupdate, et c.:
          #'sub-tools-install_subtargets',     #'sub-tools',
          'install_qmake',
          'install_mkspecs',
        ],
    # Using configure -release saves a lot of space and time,
    # but by default, debug builds of mythtv try to link against
    # debug libraries of Qt. This works around that:
    'post-conf' => 'cd $PREFIX/lib ; '.
                   'ln -sf libQt3Support.dylib libQt3Support_debug.dylib ; '.
                   'ln -sf libQtSql.dylib      libQtSql_debug.dylib      ; '.
                   'ln -sf libQtXml.dylib      libQtXml_debug.dylib      ; '.
                   'ln -sf libQtOpenGL.dylib   libQtOpenGL_debug.dylib   ; '.
                   'ln -sf libQtGui.dylib      libQtGui_debug.dylib      ; '.
                   'ln -sf libQtNetwork.dylib  libQtNetwork_debug.dylib  ; '.
                   'ln -sf libQtCore.dylib     libQtCore_debug.dylib     ; '.
                   '',
    'parallel-make' => 'yes'
  },
  
  'exif' =>
  {
    'url'  => "$sourceforge/sourceforge/libexif/libexif-0.6.17.tar.bz2",
    'conf' => [ '--disable-docs' ]
  }
);


=head1 NAME

osx-packager.pl - build OS X binary packages for MythTV

=head1 SYNOPSIS

 osx-packager.pl [options]
 
 Options:
   -help            print the usage message
   -man             print full documentation
   -verbose         print informative messages during the process
   -version <str>   custom version suffix (defaults to "svnYYYYMMDD")
   -noversion       don't use any version (for building release versions)
   -distclean       throw away all intermediate files and exit
   -thirdclean      do a clean rebuild of third party packages
   -thirdskip       don't rebuild the third party packages
   -mythtvskip      don't rebuild/install mythtv
   -pluginskip      don't rebuild/install mythplugins
   -themeskip       don't install the extra themes from myththemes
   -clean           do a clean rebuild of MythTV
   -svnbranch <str> build a specified Subversion branch,   instead of HEAD
   -svnrev <str>    build a specified Subversion revision, instead of HEAD
   -svntag <str>    build a specified release, instead of Subversion HEAD
   -nohead          don't update to HEAD revision of MythTV before building
   -usehdimage      perform build inside of a case-sensitive disk image
   -leavehdimage    leave disk image mounted on exit
   -enable-backend  build the backend server as well as the frontend
   -enable-jobtools build commflag/jobqueue  as well as the frontend
   -profile         build with compile-type=profile
   -debug           build with compile-type=debug
   -plugins <str>   comma-separated list of plugins to include
                      Available plugins:
   mythbrowser mythcontrols mythdvd mythflix mythgallery mythgame
   mythmusic mythnews mythphone mythvideo mythweather

=head1 DESCRIPTION

This script builds a MythTV frontend and all necessary dependencies, along
with plugins as specified, as a standalone binary package for Mac OS X. 

It was designed for building daily CVS (now Subversion) snapshots,
but can also be used to create release builds with the '-svntag' option.

All intermediate files go into an '.osx-packager' directory in the current
working directory. The finished application is named 'MythFrontend.app' and
placed in the current working directory.

=head1 EXAMPLES

Building two snapshots, one with plugins and one without:

  osx-packager.pl -clean -plugins mythvideo,mythweather
  mv MythFrontend.app MythFrontend-plugins.app
  osx-packager.pl -nohead
  mv MythFrontend.app MythFrontend-noplugins.app

Building a "fixes" branch:

  osx-packager.pl -distclean
  osx-packager.pl -svnbranch release-0-22-fixes

Note that this script will not build old branches.
Please try the branched version instead. e.g.
http://svn.mythtv.org/svn/branches/release-0-21-fixes/mythtv/contrib/OSX/osx-packager.pl

=head1 CREDITS

Written by Jeremiah Morris (jm@whpress.com)

Special thanks to Nigel Pearson, Jan Ornstedt, Angel Li, and Andre Pang
for help, code, and advice.

Small modifications made by Bas Hulsken (bhulsken@hotmail.com) to allow building current svn, and allow lirc (if installed properly on before running script). The modifications are crappy, and should probably be revised by someone who can actually code in perl. However it works for the moment, and I wanted to share with other mac frontend experimenters!

=cut

# Parse options
our (%OPT);
Getopt::Long::GetOptions(\%OPT,
                         'help|?',
                         'man',
                         'verbose',
                         'version=s',
                         'noversion',
                         'distclean',
                         'thirdclean',
                         'thirdskip',
                         'mythtvskip',
                         'pluginskip',
                         'themeskip',
                         'clean',
                         'svnbranch=s',
                         'svnrev=s',
                         'svntag=s',
                         'nocvs', # This is obsolete, but should stay a while
                         'nohead',
                         'usehdimage',
                         'leavehdimage',
                         'enable-backend',
                         'enable-jobtools',
                         'profile',
                         'debug',
                         'plugins=s',
                        ) or Pod::Usage::pod2usage(2);
Pod::Usage::pod2usage(1) if $OPT{'help'};
Pod::Usage::pod2usage('-verbose' => 2) if $OPT{'man'};

if ( $OPT{'enable-backend'} )
{   $backend = 1  }

if ( $OPT{'clean'} )
{   $cleanLibs = 1  }

if ( $OPT{'enable-jobtools'} )
{   $jobtools = 1  }

# Get version string sorted out
if ($OPT{'svntag'} && !$OPT{'version'})
{
  $OPT{'version'} = $OPT{'svntag'};
  $OPT{'version'} =~ s/-r *//;
  $OPT{'version'} =~ s/--revision *//;
}
$OPT{'version'} = '' if $OPT{'noversion'};
unless (defined $OPT{'version'})
{
  my @lt = gmtime(time);
  $OPT{'version'} = sprintf('svn%04d%02d%02d',
                            $lt[5] + 1900,
                            $lt[4] + 1,
                            $lt[3]);
}

# Fold nocvs to nohead
$OPT{'nohead'} = 1 if $OPT{'nocvs'};

# Build our temp directories
our $SCRIPTDIR = Cwd::abs_path(Cwd::getcwd());
if ($SCRIPTDIR =~ /\s/)
{
  &Complain(<<END);
Working directory contains spaces

Error: Your current working path:

   $SCRIPTDIR

contains one or more spaces. This will break the compilation process,
so the script cannot continue. Please re-run this script from a different
directory (such as /tmp).

The application produced will run from any directory, the no-spaces
rule is only for the build process itself.

END
  die;
}

if ( $OPT{'svnbranch'} )
{
    &Complain(<<END);
Note that this script can not build old branches.
Please try the branched version instead. e.g.
http://svn.mythtv.org/svn/branches/release-0-21-fixes/mythtv/contrib/OSX/osx-packager.pl
END
    die;
}

our $WORKDIR = "$SCRIPTDIR/.osx-packager";
mkdir $WORKDIR;

# Do we need to force a case-sensitive disk image?
if (0 &&       # No. MythTV source doesn't require it at the moment.
    !$OPT{usehdimage} && !CaseSensitiveFilesystem())
{
  Verbose("Forcing -usehdimage due to case-insensitive filesystem");
  $OPT{usehdimage} = 1;
}

if ($OPT{usehdimage})
{  MountHDImage()  }

our $PREFIX = "$WORKDIR/build";
mkdir $PREFIX;

our $SRCDIR = "$WORKDIR/src";
mkdir $SRCDIR;

our $SVNDIR = "$SRCDIR/myth-svn";

# configure mythplugins, and mythtv, etc
our %conf = (
  'mythplugins'
  =>  [
        '--prefix=' . $PREFIX,
        '--enable-opengl',
        '--disable-mythbrowser',
        '--disable-transcode',
        '--enable-mythgallery',
        '--enable-exif',
        '--enable-new-exif',
        '--disable-mythgame',
        '--disable-mythmusic',
        '--disable-mythzoneminder',
      ],
  'myththemes'
  =>  [
        '--prefix=' . $PREFIX,
      ],
  'mythtv'
  =>  [
        '--prefix=' . $PREFIX,
        '--runprefix=../Resources',
        # To "cross compile" something for a lesser Mac:
        #'--tune=G3',
        #'--disable-altivec',
      ],
);

# configure mythplugins, and mythtv, etc
our %makecleanopt = (
  'mythplugins'
  =>  [
        'distclean',
      ],
);

# Source code version.pro needs to call subversion binary
#
use File::Basename;
our $svnpath = dirname $svn;

# Clean the environment
$ENV{'PATH'} = "$PREFIX/bin:/bin:/usr/bin:/usr/sbin:$svnpath";
$ENV{'PKG_CONFIG_PATH'} = "$PREFIX/lib/pkgconfig:";
delete $ENV{'CC'};
delete $ENV{'CXX'};
delete $ENV{'CPP'};
delete $ENV{'CXXCPP'};
$ENV{'CFLAGS'} = $ENV{'CXXFLAGS'} = $ENV{'CPPFLAGS'} = "-I$PREFIX/include";
$ENV{'LDFLAGS'} = "-F/System/Library/Frameworks -L/usr/lib -L$PREFIX/lib";
$ENV{'PREFIX'} = $PREFIX;

# set up Qt environment
$ENV{'QTDIR'} = $PREFIX;

# If environment is setup to use distcc, take advantage of it
our $standard_make = '/usr/bin/make';
our $parallel_make = $standard_make;
our $parallel_make_flags = '';

if ( $ENV{'DISTCC_HOSTS'} )
{
  my @hosts = split m/\s+/, $ENV{'DISTCC_HOSTS'};
  my $numhosts = $#hosts + 1;
  &Verbose("Using ", $numhosts * 2, " DistCC jobs on $numhosts build hosts:",
           join ', ', @hosts);
  $parallel_make_flags = '-j' . $numhosts * 2;
}

# Ditto for multi-cpu setups:
my $cmd = "/usr/bin/hostinfo | grep 'processors\$'";
&Verbose($cmd);
my $cpus = `$cmd`; chomp $cpus;
$cpus =~ s/.*, (\d+) processors$/$1/;
if ( $cpus gt 1 )
{
  &Verbose("Using", $cpus+1, "jobs on $cpus parallel CPUs");
  ++$cpus;
  $parallel_make_flags = "-j$cpus";
}

$parallel_make .= " $parallel_make_flags";

### Distclean?
if ($OPT{'distclean'})
{
  &Syscall([ '/bin/rm', '-f',       '$PREFIX/bin/myth*'    ]);
  &Syscall([ '/bin/rm', '-f', '-r', '$PREFIX/lib/libmyth*' ]);
  &Syscall([ '/bin/rm', '-f', '-r', '$PREFIX/lib/mythtv'   ]);
  &Syscall([ '/bin/rm', '-f', '-r', '$PREFIX/share/mythtv' ]);
  &Syscall([ 'find', $SVNDIR, '-name', '*.o',     '-delete' ]);
  &Syscall([ 'find', $SVNDIR, '-name', '*.a',     '-delete' ]);
  &Syscall([ 'find', $SVNDIR, '-name', '*.dylib', '-delete' ]);
  &Syscall([ 'find', $SVNDIR, '-name', '*.orig',  '-delete' ]);
  &Syscall([ 'find', $SVNDIR, '-name', '*.rej',   '-delete' ]);
  exit;
}

### Check for app present in target location
our $MFE = "$SCRIPTDIR/MythFrontend.app";
if (-d $MFE)
{
  &Complain(<<END);
$MFE already exists

Error: a MythFrontend application exists where we were planning
to build one. Please move this application away before running
this script.

END
  exit;
}

### Third party packages
my (@build_depends, %seen_depends);
my @comps = ('mythtv', @components, 'packaging');

# Deal with user-supplied skip arguments
if ( $OPT{'mythtvskip'} )
{   @comps = grep(!m/mythtv/,      @comps)   }
if ( $OPT{'pluginskip'} )
{   @comps = grep(!m/mythplugins/, @comps)   }
if ( $OPT{'themeskip'} )
{   @comps = grep(!m/myththemes/,  @comps)   }

if ( ! @comps )
{
  &Complain("Nothing to build! Too many ...skip arguments?");
  exit;
}

&Verbose("Including components:", @comps);

# If no SubVersion in path, and we are checking something out, build SVN:
if ( $svn =~ m/no svn in / && ! $OPT{'nohead'} )
{
  $svn = "$PREFIX/bin/svn";
  @build_depends = ('svndeps', 'svn');
}

foreach my $comp (@comps)
{
  foreach my $dep (@{ $depend_order{$comp} })
  {
    unless (exists $seen_depends{$dep})
    {
      push(@build_depends, $dep);
      $seen_depends{$dep} = 1;
    }
  }
}
foreach my $sw (@build_depends)
{
  # Get info about this package
  my $pkg = $depend{$sw};
  my $url = $pkg->{'url'};
  my $filename = $url;
  $filename =~ s|^.+/([^/]+)$|$1|;
  my $dirname = $filename;
  $dirname =~ s|\.tar\.gz$||;
  $dirname =~ s|\.tar\.bz2$||;

  chdir($SRCDIR);
  
  # Download and decompress
  unless (-e $filename)
  {
    &Verbose("Downloading $sw");
    unless (&Syscall([ '/usr/bin/curl', '-f', '-L', $url, '>', $filename ],
                     'munge' => 1))
    {
      &Syscall([ '/bin/rm', $filename ]) if (-e $filename);
      die;
    }
  }
  else
  {
    &Verbose("Using previously downloaded $sw");
  }
  
  if ($pkg->{'skip'})
  { next }

  if ( -d $dirname )
  {
   if ( $OPT{'thirdclean'} )
   {
    &Verbose("Removing previous build of $sw");
    &Syscall([ '/bin/rm', '-f', '-r', $dirname ]) or die;
   }

   if ( $OPT{'thirdskip'} )
   {
    &Verbose("Using previous build of $sw");
    next;
   }

    &Verbose("Using previously unpacked $sw");
  }
  else
  {
    &Verbose("Unpacking $sw");
    if ( substr($filename,-3) eq ".gz" )
    {
      &Syscall([ '/usr/bin/tar', '-xzf', $filename ]) or die;
    }
    elsif ( substr($filename,-4) eq ".bz2" )
    {
      &Syscall([ '/usr/bin/tar', '-xjf', $filename ]) or die;
    }
    else
    {
      &Complain("Cannot unpack file $filename");
      exit;
    }
  }
  
  # Configure
  chdir($dirname);
  unless (-e '.osx-config')
  {
    &Verbose("Configuring $sw");
    if ($pkg->{'pre-conf'})
    { 
      &Syscall([ $pkg->{'pre-conf'} ], 'munge' => 1) or die;
    }

    my (@configure, $munge);
    
    if ($pkg->{'conf-cmd'})
    {
      push(@configure, $pkg->{'conf-cmd'});
      $munge = 1;
    }
    else
    {
      push(@configure, './configure',
                       '--prefix=$PREFIX',
                       '--disable-static',
                       '--enable-shared');
    }
    if ($pkg->{'conf'})
    {
      push(@configure, @{ $pkg->{'conf'} });
    }
    &Syscall(\@configure, 'interpolate' => 1, 'munge' => $munge) or die;
    if ($pkg->{'post-conf'})
    {
      &Syscall([ $pkg->{'post-conf'} ], 'munge' => 1) or die;
    }
    &Syscall([ '/usr/bin/touch', '.osx-config' ]) or die;
  }
  else
  {
    &Verbose("Using previously configured $sw");
  }
  
  # Build and install
  unless (-e '.osx-built')
  {
    &Verbose("Making $sw");
    my (@make);
    
    push(@make, $standard_make);
    if ($pkg->{'parallel-make'} && $parallel_make_flags)
    { push(@make, $parallel_make_flags) }

    if ($pkg->{'make'})
    {
      push(@make, @{ $pkg->{'make'} });
    }
    else
    {
      push(@make, 'all', 'install');
    }
    &Syscall(\@make) or die;
    &Syscall([ '/usr/bin/touch', '.osx-built' ]) or die;
  }
  else
  {
    &Verbose("Using previously built $sw");
  }
}


### build MythTV

# Clean any previously installed libraries
if ( $cleanLibs )
{
  if ( $OPT{'mythtvskip'} )
  {
    &Complain("Cannot skip building mythtv src if also cleaning");
    exit;
  }
&Verbose("Cleaning previous installs of MythTV");
my @mythlibs = glob "$PREFIX/lib/libmyth*";
if (scalar @mythlibs)
{
  &Syscall([ '/bin/rm', @mythlibs ]) or die;
}
foreach my $dir ('include', 'lib', 'share')
{
  if (-d "$PREFIX/$dir/mythtv")
  {
    &Syscall([ '/bin/rm', '-f', '-r', "$PREFIX/$dir/mythtv" ]) or die;
  }
}
}

mkdir $SVNDIR;

# Deal with Subversion branches, revisions and tags:
my $svnrepository = 'http://svn.mythtv.org/svn/';
my @svnrevision   = ();

if ( $OPT{'svnbranch'} )
{
  $svnrepository .= 'branches/' . $OPT{'svnbranch'} . '/';
Die("Note that this script will not build old branches.
Please try the branched version instead. e.g.
http://svn.mythtv.org/svn/branches/release-0-21-fixes/mythtv/contrib/OSX/osx-packager.pl");
}
elsif ( $OPT{'svntag'} )
{
  $svnrepository .= 'tags/' . $OPT{'svntag'} . '/';
}
elsif ( $OPT{'svnrev'} )
{
  $svnrepository .= 'trunk/';

  # This arg. could be '1234', '-r 1234', or '--revision 1234'
  # If the user just specified a number, add appropriate flag:
  if ( $OPT{'svnrev'} =~ m/^\d+$/ )
  {
    push @svnrevision, '--revision';
  }

  push @svnrevision, $OPT{'svnrev'};
}
elsif ( ! $OPT{'nohead'} )
{
  # Lookup and use the HEAD revision so we are guaranteed consistent source
  my $cmd = "$svn log $svnrepository --revision HEAD --xml | grep revision";
  &Verbose($cmd);
  my $rev = `$cmd`;

  if ( $rev =~ m/revision="(\d+)">/ )
  {
    $svnrepository .= 'trunk/';
    @svnrevision = ('--revision', $1);
  }
  else
  {
    &Complain("Cannot get head revision - Got '$rev'");
    die;
  }
}

# Retrieve source
if (! $OPT{'nohead'})
{
  # Empty subdirectory 'config' sometimes causes checkout problems
  &Syscall(['rm', '-fr', $SVNDIR . '/mythtv/config']);
  Verbose("Checking out source code");
  &Syscall([ $svn, 'co', @svnrevision,
            map($svnrepository . $_, @comps), $SVNDIR ]) or die;
}
else
{
  &Syscall("mkdir -p $SVNDIR/mythtv/config")
}

# Build MythTV and any plugins
foreach my $comp (@comps)
{
  my $compdir = "$SVNDIR/$comp/" ;

  chdir $compdir;

  if ( ! -e "$comp.pro" )
  {
    &Complain("$compdir/$comp.pro does not exist.",
              'You must be building really old source?');
    next;
  }
  
  if ($OPT{'clean'} && -e 'Makefile')
  {
    &Verbose("Cleaning $comp");
    &Syscall([ $standard_make, 'distclean' ]) or die;
  }
  else
  {
    # clean the Makefiles, as process requires PREFIX hacking
    &CleanMakefiles();
  }
 
  # Apply any nasty mac-specific patches 
  if ($patches{$comp})
  {
    &Syscall([ "echo '$patches{$comp}' | patch -p0 --forward" ]);
  }
  
  # configure and make
  if ( $makecleanopt{$comp} && -e 'Makefile' )
  {
    my @makecleancom = $standard_make;
    push(@makecleancom, @{ $makecleanopt{$comp} }) if $makecleanopt{$comp};
    &Syscall([ @makecleancom ]) or die;
  }
  if (-e 'configure')
  {
    &Verbose("Configuring $comp");
    my @config = './configure';
    push(@config, @{ $conf{$comp} }) if $conf{$comp};
    if ( $comp eq 'mythtv' && $backend )
    {
      push @config, '--enable-backend'
    }
    if ( $OPT{'profile'} )
    {
      push @config, '--compile-type=profile'
    }
    if ( $OPT{'debug'} )
    {
      push @config, '--compile-type=debug'
    }
    if ( $comp eq 'mythtv' && ! $ENV{'DISTCC_HOSTS'} )
    {
      push @config, '--disable-distcc'
    }
    &Syscall([ @config ]) or die;
  }
  &Verbose("Running qmake for $comp");
  my @qmake_opts = (
    'QMAKE_LFLAGS+=-Wl,-search_paths_first',
    'INCLUDEPATH+="' . $PREFIX . '/include"',
    'LIBS+=-L/usr/lib -L"' . $PREFIX . '/lib"'
   );
  &Syscall([ $PREFIX . '/bin/qmake',
             'PREFIX=../Resources',
             @qmake_opts,
             "$comp.pro" ]) or die;

  if ($comp eq 'mythtv')
  {
    # Remove/add Nigel's frontend building speedup hack
    &DoSpeedupHacks('programs/programs.pro', 'mythfrontend mythtv');
  }
  
  &Verbose("Making $comp");
  &Syscall([ $parallel_make ]) or die;
  # install
  # This requires a change from the compiled-in relative
  # PREFIX to our absolute path of the temp install location.
  &CleanMakefiles();
  &Verbose("Running qmake for $comp install");
  &Syscall([ $PREFIX . '/bin/qmake',
             'PREFIX=' . $PREFIX,
             @qmake_opts,
             "$comp.pro" ]) or die;
  &Verbose("Installing $comp");
  &Syscall([ $standard_make,
             'install' ]) or die;

  if ($cleanLibs && $comp eq 'mythtv')
  {
    # If we cleaned the libs, make install will have recopied them,
    # which means any dynamic libraries that the static libraries depend on
    # are newer than the table of contents. Hence we need to regenerate it:
    my @mythlibs = glob "$PREFIX/lib/libmyth*.a";
    if (scalar @mythlibs)
    {
      &Verbose("Running ranlib on reinstalled static libraries");
      foreach my $lib (@mythlibs)
      { &Syscall("ranlib $lib") or die }
    }
  }
}

### Build version string
our $VERS = `find $PREFIX/lib -name 'libmyth-[0-9].[0-9][0-9].[0-9].dylib'`;
chomp $VERS;
$VERS =~ s/^.*\-(.*)\.dylib$/$1/s;
$VERS .= '.' . $OPT{'version'} if $OPT{'version'};

### Program which creates bundles:
our @bundler = "$SVNDIR/packaging/OSX/build/osx-bundler.pl";
if ( $OPT{'verbose'} )
{   push @bundler, '--verbose'   }


### Framework that has a screwed up link dependency path
my $AVCfw = '/Developer/FireWireSDK*/Examples/' .
            'Framework/AVCVideoServices.framework';
my @AVCfw = split / /, `ls -d $AVCfw`;
$AVCfw = pop @AVCfw;
chop $AVCfw;

### Create each package.
### Note that this is a bit of a waste of disk space,
### because there are now multiple copies of each library.
my @targets = ('MythFrontend', 'MythTV');

if ( $jobtools )
{   push @targets, @targetsJT   }

if ( $backend )
{   push @targets, @targetsBE   }

foreach my $target ( @targets )
{
  my $finalTarget = "$SCRIPTDIR/$target.app";
  my $builtTarget = lc $target;

  # Get a fresh copy of the binary
  &Verbose("Building self-contained $target");
  &Syscall([ 'rm', '-fr', $finalTarget ]) or die;
  &Syscall([ 'cp',  "$SVNDIR/mythtv/programs/$builtTarget/$builtTarget",
             "$SCRIPTDIR/$target" ]) or die;

  # Convert it to a bundled .app
  &Syscall([ @bundler, "$SCRIPTDIR/$target",
             "$PREFIX/lib/", "$PREFIX/lib/mysql" ])
      or die;

  # Remove copy of binary
  unlink "$SCRIPTDIR/$target" or die;

  if ( $AVCfw )
  {  &RecursiveCopy($AVCfw, "$finalTarget/Contents/Frameworks")  }

 if ( $target eq "MythFrontend" or $target =~ m/^MythTV/ )
 {
  my $res  = "$finalTarget/Contents/Resources";
  my $libs = "$res/lib";

  # Install themes, filters, etc.
  &Verbose("Installing resources into $target");
  mkdir $res; mkdir $libs;
  &RecursiveCopy("$PREFIX/lib/mythtv", $libs);
  mkdir "$res/share";
  &RecursiveCopy("$PREFIX/share/mythtv", "$res/share");

  # Correct the library paths for the filters and plugins
  foreach my $lib ( glob "$libs/mythtv/*/*" )
  {   &Syscall([ @bundler, $lib ]) or die   }

  # The icon
  &Syscall([ 'cp',  "$SVNDIR/mythtv/programs/mythfrontend/mythfrontend.icns",
             "$res/application.icns" ]) or die;
  &Syscall([ '/Developer/Tools/SetFile', '-a', 'C', $finalTarget ]) or die;
 }

  if ( $target eq "MythFrontend" )
  {
    foreach my $extra ( 'ignyte', 'mtd' )
    {
      if ( -e "$PREFIX/bin/$extra" )
      {
        &Verbose("Installing $extra into $target");
        &Syscall([ 'cp', "$PREFIX/bin/$extra",
                   "$finalTarget/Contents/MacOS" ]) or die;

        &Verbose("Updating lib paths of $finalTarget/Contents/MacOS/$extra");
        &Syscall([ @bundler, "$finalTarget/Contents/MacOS/$extra" ]) or die;
        &AddFakeBinDir($finalTarget);
      }
    }
  }
}

if ( $backend && grep(m/MythBackend/, @targets) )
{
  my $BE = "$SCRIPTDIR/MythBackend.app";

  # The backend gets all the useful binaries it might call:
  foreach my $binary ( 'mythjobqueue', 'mythcommflag', 'mythtranscode' )
  {
    my $SRC  = "$PREFIX/bin/$binary";
    if ( -e $SRC )
    {
      &Verbose("Installing $SRC into $BE");
      &Syscall([ '/bin/cp', $SRC, "$BE/Contents/MacOS" ]) or die;

      &Verbose("Updating lib paths of $BE/Contents/MacOS/$binary");
      &Syscall([ @bundler, "$BE/Contents/MacOS/$binary" ]) or die;
    }
  }
  &AddFakeBinDir($BE);
}

if ( $jobtools )
{
  # JobQueue also gets some binaries it might call:
  my $JQ   = "$SCRIPTDIR/MythJobQueue.app";
  my $DEST = "$JQ/Contents/MacOS";
  my $SRC  = "$PREFIX/bin/mythcommflag";

  &Syscall([ '/bin/cp', $SRC, $DEST ]) or die;
  &AddFakeBinDir($JQ);
  &Verbose("Updating lib paths of $DEST/mythcommflag");
  &Syscall([ @bundler, "$DEST/mythcommflag" ]) or die;

  $SRC  = "$PREFIX/bin/mythtranscode.app/Contents/MacOS/mythtranscode";
  if ( -e $SRC )
  {
      &Verbose("Installing $SRC into $JQ");
      &Syscall([ '/bin/cp', $SRC, $DEST ]) or die;
      &Verbose("Updating lib paths of $DEST/mythtranscode");
      &Syscall([ @bundler, "$DEST/mythtranscode" ]) or die;
  }
}

# Clean tmp files. Most of these are leftovers from configure:
#
&Verbose('Cleaning build tmp directory');
&Syscall([ 'rm', '-fr', $WORKDIR . '/tmp' ]) or die;
&Syscall([ 'mkdir',     $WORKDIR . '/tmp' ]) or die;

if ($OPT{usehdimage} && !$OPT{leavehdimage})
{
    Verbose("Dismounting case-sensitive build device");
    UnmountHDImage();
}

&Verbose("Build complete. Self-contained package is at:\n\n    $MFE\n");

### end script
exit 0;


######################################
## RecursiveCopy copies a directory tree, stripping out .svn
## directories and properly managing static libraries.
######################################

sub RecursiveCopy($$)
{
    my ($src, $dst) = @_;

    # First copy absolutely everything
    &Syscall([ '/bin/cp', '-pR', "$src", "$dst"]) or die;

    # Then strip out any .svn directories
    my @files = map { chomp $_; $_ } `find $dst -name .svn`;
    if (scalar @files)
    {
        &Syscall([ '/bin/rm', '-f', '-r', @files ]);
    }

    # And make sure any static libraries are properly relocated.
    my @libs = map { chomp $_; $_ } `find $dst -name "lib*.a"`;
    if (scalar @libs)
    {
        &Syscall([ 'ranlib', '-s', @libs ]);
    }
}

######################################
## CleanMakefiles removes every generated Makefile
## from our MythTV build that contains PREFIX.
## Necessary when we change the
## PREFIX variable.
######################################

sub CleanMakefiles
{
  &Verbose("Cleaning MythTV makefiles containing PREFIX");
  &Syscall([ 'find', '.', '-name', 'Makefile', '-exec',
             'egrep', '-q', 'qmake.*PREFIX', '{}', ';', '-delete' ]) or die;
} # end CleanMakefiles


######################################
## Syscall wrappers the Perl "system"
## routine with verbosity and error
## checking.
######################################

sub Syscall($%)
{
  my ($arglist, %opts) = @_;
  
  unless (ref $arglist)
  {
    $arglist = [ $arglist ];
  }
  if ($opts{'interpolate'})
  {
    my @args;
    foreach my $arg (@$arglist)
    {
      $arg =~ s/\$PREFIX/$PREFIX/ge;
      $arg =~ s/\$parallel_make_flags/$parallel_make_flags/ge;
      push(@args, $arg);
    }
    $arglist = \@args;
  } 
  if ($opts{'munge'})
  {
    $arglist = [ join(' ', @$arglist) ];
  }
  # clean out any null arguments
  $arglist = [ map $_, @$arglist ];
  &Verbose(@$arglist);
  my $ret = system(@$arglist);
  if ($ret)
  {
    &Complain('Failed system call: "', @$arglist,
              '" with error code', $ret >> 8);
  }
  return ($ret == 0);
} # end Syscall


######################################
## Verbose prints messages in verbose
## mode.
######################################

sub Verbose
{
  print STDERR '[osx-pkg] ' . join(' ', @_) . "\n"
      if $OPT{'verbose'};
} # end Verbose


######################################
## Complain prints messages in any
## verbosity mode.
######################################

sub Complain
{
  print STDERR '[osx-pkg] ' . join(' ', @_) . "\n";
} # end Complain


######################################
## Manage usehdimage disk image
######################################

sub MountHDImage
{
    if (!HDImageDevice())
    {
        if (-e "$SCRIPTDIR/.osx-packager.dmg")
        {
            Verbose("Mounting existing UFS disk image for the build");
        }
        else
        {
            Verbose("Creating a case-sensitive (UFS) disk image for the build");
            Syscall(['hdiutil', 'create', '-size', '2048m',
                     "$SCRIPTDIR/.osx-packager.dmg", '-volname',
                     'MythTvPackagerHDImage', '-fs', 'UFS', '-quiet']) || die;
        }

        &Syscall(['hdiutil', 'mount',
                  "$SCRIPTDIR/.osx-packager.dmg",
                  '-mountpoint', $WORKDIR, '-quiet']) || die;
    }

    # configure defaults to /tmp and OSX barfs when mv crosses
    # filesystems so tell configure to put temp files on the image

    $ENV{TMPDIR} = $WORKDIR . "/tmp";
    mkdir $ENV{TMPDIR};
}

sub UnmountHDImage
{
    my $device = HDImageDevice();
    if ($device)
    {
        &Syscall(['hdiutil', 'detach', $device, '-force']);
    }
}

sub HDImageDevice
{
    my @dev = split ' ', `/sbin/mount | grep $WORKDIR`;
    $dev[0];
}

sub CaseSensitiveFilesystem
{
  my $funky = $SCRIPTDIR . "/.osx-packager.FunkyStuff";
  my $unfunky = substr($funky, 0, -10) . "FUNKySTuFF";

  unlink $funky if -e $funky;
  `touch $funky`;
  my $sensitivity = ! -e $unfunky;
  unlink $funky;

  return $sensitivity;
}


######################################
## Remove or add Nigel's speedup hacks
######################################

sub DoSpeedupHacks($$)
{
  my ($file, $subdirs) = @_;

  &Verbose("Removing Nigel's hacks from file $file");

  open(IN,  $file)         or die;
  open(OUT, ">$file.orig") or die;
  while ( <IN> )
  {
    if ( m/^# Nigel/ )  # Skip
    {  last  }
    print OUT;
  }
  if ( ! $backend && ! $jobtools )
  {
    # Nigel's hack to speedup building
    print OUT "# Nigel\'s speedup hack:\n";
    print OUT "SUBDIRS = $subdirs\n";
  }
  close IN; close OUT;
  rename("$file.orig", $file);
}

#######################################################
## Parts of MythTV try to call helper apps like this:
## gContext->GetInstallPrefix() + "/bin/mythtranscode";
## which means we need a bin directory.
#######################################################

sub AddFakeBinDir($)
{
  my ($target) = @_;

  &Syscall("mkdir -p $target/Contents/Resources");
  &Syscall(['ln', '-sf', '../MacOS', "$target/Contents/Resources/bin"]);
}

### end file
1;
