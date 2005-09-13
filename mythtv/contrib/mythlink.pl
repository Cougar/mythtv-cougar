#!/usr/bin/perl -w
#
# $Date$
# $Revision$
# $Author$
#
# license: GPL
# author:  Chris Petersen (based on the ideas in mythlink.sh from Dale Gass)
#
# mythlink.pl
#
#   Creates symlinks to more human-readable versions of mythtv recording
#   filenames.  See --help for instructions.
#
#   Automatically detects database settings from mysql.txt, and loads
#   the mythtv recording directory from the database (code from nuvexport).
#

# Includes
    use DBI;
    use Getopt::Long;
    use File::Path;

# Some variables we'll use here
    our ($dest, $format, $usage);
    our ($db_host, $db_user, $db_name, $db_pass, $video_dir);
    our ($hostname, $dbh, $sh);

# Default filename format
    $dformat = '%T - %Y-%m-%d, %g-%i %A - %S';
# Default separator character
    $dseparator_char = '-';
# Default replacement character
    $dreplacement_char = '-';
# Default space character
    $dspace_char = ' ';

# Provide default values for GetOptions
    $format = $dformat;
    $separator_char = $dseparator_char;
    $replacement_char = $dreplacement_char;
    $space_char = $dspace_char;

# Load the destination directory, if one was specified
    GetOptions('dest|destination|path=s'    => \$dest,
               'format=s'                   => \$format,
               'sepchar|separator-char=s'   => \$separator_char,
               'repchar|replacement-char=s' => \$replacement_char,
               'space|space-char=s'         => \$space_char,
               'usage|help|h'               => \$usage,
              );

# Check the separator, replacement, and space characters for illegal characters
    if ($separator_char =~ /(?:[\/\\:*?<>|"])/) {
      print "Illegal separator character specified.  Using default.\n";
      $separator_char = $dseparator_char;
    }
    if ($replacement_char =~ /(?:[\/\\:*?<>|"])/) {
      print "Illegal replacement character specified.  Using default.\n";
      $replacement_char = $dreplacement_char;
    }
    if ($space_char =~ /(?:[\/\\:*?<>|"])/) {
      print "Illegal space character specified.  Using default.\n";
      $space_char = $dspace_char;
    }

# Print usage
    if ($usage) {
        print <<EOF;
$0 usage:

options:

--dest

    By default, links will be created in the show_names directory inside of the
    current mythtv data directory on this machine.  eg:

    /var/video/show_names/

--format

    default:  $dformat

    \%T = title    (aka show name)
    \%S = subtitle (aka episode name)
    \%R = description
    \%C = category (as reported by grabber)
    \%U = recording group
    \%p = separator character
    \%y = year, 2 digits
    \%Y = year, 4 digits
    \%n = month
    \%m = month, leading zero
    \%j = day of month
    \%d = day of month, leading zero
    \%g = 12-hour hour
    \%G = 24-hour hour
    \%h = 12-hour hour, with leading zero
    \%H = 24-hour hour, with leading zero
    \%i = minutes
    \%s = seconds
    \%a = am/pm
    \%A = AM/PM

    * For end time, prepend an "e" to the appropriate time/date format code
      above; i.e. "\%eG" gives the 24-hour hour for the end time.

    * For original airdate, prepend an "o" to the year, month, or day format
      codes above; i.e. "\%oY" gives the year in which the episode was first
      aired.

    * A suffix of .mpg or .nuv will be added where appropriate.

--separator-char

    The character used to separate sections of the link name.  Specifying the
    separator character allows trailing separators to be removed from the link
    name and multiple separators caused by missing data to be consolidated.
    Indicate the separator character in the format string using either a
    literal character or the '%p' specifier.

    default:  '$dseparator_char'

--replacement-char

    Characters in the link name which are not legal on some filesystems will
    be replaced with the given character

    default:  '$dreplacement_char'

--space-char

    Use the specified character instead of space in the link name

    default:  '$dspace_char'

--help

    Show this help text.

EOF
        exit;
    }

# Get the hostname of this machine
    $hostname = `hostname`;
    chomp($hostname);

# Read the mysql.txt file in use by MythTV.
# could be in a couple places, so try the usual suspects
    my $found = 0;
    my @mysql = ('/usr/local/share/mythtv/mysql.txt',
                 '/usr/share/mythtv/mysql.txt',
                 '/etc/mythtv/mysql.txt',
                 '/usr/local/etc/mythtv/mysql.txt',
                 "$ENV{HOME}/.mythtv/mysql.txt",
                 'mysql.txt'
                );
    foreach my $file (@mysql) {
        next unless (-e $file);
        $found = 1;
        open(CONF, $file) or die "Unable to open $file:  $!\n\n";
        while (my $line = <CONF>) {
        # Cleanup
            next if ($line =~ /^\s*#/);
            $line =~ s/^str //;
            chomp($line);
        # Split off the var=val pairs
            my ($var, $val) = split(/\=/, $line, 2);
            next unless ($var && $var =~ /\w/);
            if ($var eq 'DBHostName') {
                $db_host = $val;
            }
            elsif ($var eq 'DBUserName') {
                $db_user = $val;
            }
            elsif ($var eq 'DBName') {
                $db_name = $val;
            }
            elsif ($var eq 'DBPassword') {
                $db_pass = $val;
            }
        # Hostname override
            elsif ($var eq 'LocalHostName') {
                $hostname = $val;
            }
        }
        close CONF;
    }
    die "Unable to locate mysql.txt:  $!\n\n" unless ($found && $db_host);

# Connect to the database
    $dbh = DBI->connect("dbi:mysql:database=$db_name:host=$db_host", $db_user, $db_pass)
        or die "Cannot connect to database: $!\n\n";
    END {
        $dbh->disconnect if ($dbh);
    }

# Find the directory where the recordings are located
    $q = "SELECT data FROM settings WHERE value='RecordFilePrefix' AND hostname=?";
    $sh = $dbh->prepare($q);
        $sh->execute($hostname) or die "Could not execute ($q):  $!\n\n";
    ($video_dir) = $sh->fetchrow_array;
    $sh->finish;
    die "This host not configured for myth.\n(No RecordFilePrefix defined for $hostname in the settings table.)\n\n" unless ($video_dir);
    die "Recordings directory $video_dir doesn't exist!\n\n" unless (-d $video_dir);
    $video_dir =~ s/\/+$//;

# Double-check the destination
    $dest ||= "$video_dir/show_names";
    print "Destination directory:  $dest\n";

# Create nonexistent paths
    unless (-e $dest) {
        mkpath($dest, 0, 0755) or die "Failed to create $dest:  $!\n";
    }

# Bad paths
    die "$dest is not a directory.\n" unless (-d $dest);

# Delete any old links
    foreach my $file (<$dest/*>) {
        next unless (-l $file);
        unlink $file or die "Couldn't remove old symlink $file:  $!\n";
    }

# Prepare a database query
    $sh = $dbh->prepare('SELECT title, subtitle, description, recgroup, category, originalairdate FROM recorded WHERE chanid=? AND starttime=? AND endtime=?');

# Create symlinks for the files on this machine
    foreach my $file (<$video_dir/*.nuv>) {
        next if ($file =~ /^ringbuf/);
    # Pull out the various parts that make up the filename
        my ($channel,
            $syear, $smonth, $sday, $shour, $sminute, $ssecond,
            $eyear, $emonth, $eday, $ehour, $eminute, $esecond) = $file =~/^$video_dir\/([a-z0-9]+)_(....)(..)(..)(..)(..)(..)_(....)(..)(..)(..)(..)(..)\.nuv$/i;
    # Found a bad filename?
        unless ($channel) {
            print "Unknown filename format:  $file\n";
            next;
        }
    # Query the desired info about this recording
        $sh->execute($channel, "$syear$smonth$sday$shour$sminute$ssecond", "$eyear$emonth$eday$ehour$eminute$esecond")
            or die "Could not execute ($q):  $!\n\n";
        my ($title, $subtitle, $description, $recgroup, $category, $oad) = $sh->fetchrow_array;
    # Format some fields we may be parsing below
        # Start time
        my $meridian = ($shour > 12) ? 'AM' : 'PM';
        my $hour = ($shour > 12) ? $shour - 12 : $shour;
        if ($hour < 10) {
            $hour = "0$hour";
        }
        elsif ($hour < 1) {
            $hour = 12;
        }
        # End time
        my $emeridian = ($ehour > 12) ? 'AM' : 'PM';
        my $ethour = ($ehour > 12) ? $ehour - 12 : $ehour;
        if ($ethour < 10) {
            $ethour = "0$ethour";
        }
        elsif ($ethour < 1) {
            $ethour = 12;
        }
        # Original airdate
        # Handle NULL values for original airdate.
        $oad ||= '0000-00-00';
        my ($oyear, $omonth, $oday) = split(/\-/, $oad, 3);
    # Build a list of name format options
        my %fields;
        ($fields{'T'} = ($title       or '')) =~ s/%/%%/g;
        ($fields{'S'} = ($subtitle    or '')) =~ s/%/%%/g;
        ($fields{'R'} = ($description or '')) =~ s/%/%%/g;
        ($fields{'C'} = ($category    or '')) =~ s/%/%%/g;
        ($fields{'U'} = ($recgroup    or '')) =~ s/%/%%/g;
        $fields{'p'} = $separator_char;     # separator character
        # Start time
        $fields{'y'} = substr($syear, 2);   # year, 2 digits
        $fields{'Y'} = $syear;              # year, 4 digits
        $fields{'n'} = int($smonth);        # month
        $fields{'m'} = $smonth;             # month, leading zero
        $fields{'j'} = int($sday);          # day of month
        $fields{'d'} = $sday;               # day of month, leading zero
        $fields{'g'} = int($hour);          # 12-hour hour
        $fields{'G'} = int($shour);         # 24-hour hour
        $fields{'h'} = $hour;               # 12-hour hour, with leading zero
        $fields{'H'} = $shour;              # 24-hour hour, with leading zero
        $fields{'i'} = $sminute;            # minutes
        $fields{'s'} = $ssecond;            # seconds
        $fields{'a'} = lc($meridian);       # am/pm
        $fields{'A'} = $meridian;           # AM/PM
        # End time
        $fields{'ey'} = substr($eyear, 2);  # year, 2 digits
        $fields{'eY'} = $eyear;             # year, 4 digits
        $fields{'en'} = int($emonth);       # month
        $fields{'em'} = $emonth;            # month, leading zero
        $fields{'ej'} = int($eday);         # day of month
        $fields{'ed'} = $eday;              # day of month, leading zero
        $fields{'eg'} = int($ethour);       # 12-hour hour
        $fields{'eG'} = int($ehour);        # 24-hour hour
        $fields{'eh'} = $ethour;            # 12-hour hour, with leading zero
        $fields{'eH'} = $ehour;             # 24-hour hour, with leading zero
        $fields{'ei'} = $eminute;           # minutes
        $fields{'es'} = $esecond;           # seconds
        $fields{'ea'} = lc($emeridian);     # am/pm
        $fields{'eA'} = $emeridian;         # AM/PM
        # Original Airdate
        $fields{'oy'} = substr($oyear, 2);  # year, 2 digits
        $fields{'oY'} = $oyear;             # year, 4 digits
        $fields{'on'} = int($omonth);       # month
        $fields{'om'} = $omonth;            # month, leading zero
        $fields{'oj'} = int($oday);         # day of month
        $fields{'od'} = $oday;              # day of month, leading zero
    # Make the substitution
        my $keys = join('', sort keys %fields);
        my $name = $format;
        $name =~ s/(?<!%)(?:%([eo]?)([$keys]))/$fields{$1.$2}/g;
        $name =~ s/%%/%/g;
    # Some basic cleanup for illegal (windows) filename characters, etc.
        $name =~ s/(?:[\/\\\:\*\?\<\>\|$replacement_char])+/$replacement_char/sg;
        $name =~ tr/"/'/s;
        $name =~ s/[\s$space_char]+/$space_char/sg;
        $name =~ s/($space_char*)(?:$separator_char+($space_char*))+/$1$separator_char$2/sg;
        $name =~ s/^$space_char+//s;
        $name =~ s/$space_char*$separator_char+$space_char*$//s;
    # Get a shell-safe version of the filename (yes, I know it's not needed in this case, but I'm anal about such things)
        my $safe_file = $file;
        $safe_file =~ s/'/'\\''/sg;
        $safe_file = "'$safe_file'";
    # Figure out the suffix
        my $out    = `file $safe_file 2>/dev/null`;
        my $suffix = ($out =~ /mpe?g/i) ? '.mpg' : '.nuv';
    # Check for duplicates
        if (-e "$dest/$name$suffix") {
            my $count = 2;
            while (-e "$dest/$name.$count$suffix") {
                $count++;
            }
            $name .= ".$count";
        }
    # Create the link
        symlink $file, "$dest/$name$suffix"
            or die "Can't create symlink $dest/$name$suffix:  $!\n";
        print "$dest/$name$suffix\n";
    }

    $sh->finish;

