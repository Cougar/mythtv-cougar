#!/usr/bin/perl -w
#
# Connects to the mythtv database and repairs/optimizes the tables that it
# finds.  Suggested use is to cron it to run once per day.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Includes
    use DBI;
    use MythTV;

# Connect to mythbackend
    my $Myth = new MythTV({'connect' => 0});

# Connect to the database
    $dbh = $Myth->{'dbh'};

# Repair and optimize each table
    foreach $table ($dbh->tables) {
        unless ($dbh->do("REPAIR TABLE $table")) {
            print "Skipped:  $table\n";
            next;
        };
        if ($dbh->do("OPTIMIZE TABLE $table")) {
            print "Repaired/Optimized: $table\n";
        }
        if ($dbh->do("ANALYZE TABLE $table")) {
            print "Analyzed: $table\n";
        }
    }

