#!/usr/bin/perl -w
#
# This is the perl-based module handler.  It is the counterpart of mythweb.php
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Load some required modules
    use CGI qw/:standard/;
    use DBI;
    use File::Basename;

# Create a cgi object;
    our $cgi = new CGI;

# Extract the requested path
    our @Path;
    $Path[0] = ($ENV{'PATH_INFO'} or url_param('PATH_INFO'));
    $Path[0] =~ s#^/+##sg;
    $Path[0] =~ s#\s+$##sg;
    @Path = split('/', $Path[0]);
    shift @Path if ($Path[0] eq 'pl');

# Figure out the root web directory
    our $web_root = dirname($ENV{'SCRIPT_NAME'}).'/';
    $web_root =~ s#//#/#g;

# Add a directory to the search path?
    if ($ENV{'include_path'}) {
        $ENV{'PATH'} .= ':'.$ENV{'include_path'};
    }

# Connect to the database
    END { $dbh->disconnect() if ($dbh); }
    our $dbh = DBI->connect("dbi:mysql:database=$ENV{'db_name'}:host=$ENV{'db_host'}",
                           $ENV{'db_user'},
                           $ENV{'db_pass'})
               or die "Content-type: text/html\n\nCannot connect to database: $!\n\n";

# Find the path to the modules directory
    our $modules_dir = dirname(dirname(find_in_path('modules/tv/init.php')));

# Figure out what the user is trying to do
    if ($Path[0]) {
        if (-e "$modules_dir/$Path[0]") {
            if (-e "$modules_dir/$Path[0]/handler.pl") {
                require "modules/$Path[0]/handler.pl";
            }
            else {
                print header(),
                      "Module '$Path[0]' doesn't have a perl handler.";
            }
        }
        elsif ($Path[0] =~ /\w/) {
                print header(),
                      "Unknown module:  $Path[0]";
        }
    }
    else {
        print header(-location => $web_root);
        print "&nbsp;\n";
        exit;
    }

# Exit nicely
    exit;

################################################################################

# Find a file in the current include path
    sub find_in_path {
        my $file = shift;
    # Split out each of the search paths
        foreach my $path (@INC, split(/:/, $ENV{'PATH'})) {
        # Formulate the absolute path
            my $full_path = "$path/$file";
        # Exists?
            return $full_path if (-e $full_path);
        }
        return undef;
    }

