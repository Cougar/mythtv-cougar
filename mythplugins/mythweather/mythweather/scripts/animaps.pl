#!/usr/bin/perl -w
use strict;
use Getopt::Std;
use LWP::Simple;
use Date::Manip;
use MapSearch;
use Data::Dumper;
our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'Animated-Map-Download';
my $version = 0.1;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 10*60;
my $retrieveTimeout = 30;
my @types = ('amdesc', 'updatetime', 'animatedimage');
my $dir = "./";

getopts('Tvtlu:d:');

if (defined $opt_v) {
    print "$name,$version,$author,$email\n";
    exit 0;
}

if (defined $opt_T) {
    print "$updateTimeout,$retrieveTimeout\n";
    exit 0;
}
if (defined $opt_l) {
    MapSearch::AddDescSearch(shift);
    foreach my $result (@{MapSearch::doSearch()}) {
        print "$result->{animation}::$result->{description}\n" if ($result->{animation});
    }
    exit 0;
}

if (defined $opt_t) {
    foreach (@types) {print; print "\n";}
    exit 0;
}

if (defined $opt_d) {
    $dir = $opt_d;
}

my $loc = shift;

if (!defined $loc || $loc eq "") {
    die "Invalid usage";
}

#should only get one location result since its by url, assuming things is fun :)
MapSearch::AddAniSearch($loc);
my $results = MapSearch::doSearch();
my $base = $results->[0]->{animation};
my $desc = $results->[0]->{description};
my $size = $results->[0]->{imgsize};
my $file = $desc;
$file =~ s/[^a-zA-Z0-9]//g;
my $path = "$dir/$file-";
my $i = 0;
foreach my $image (sort @{$results->[0]->{images}}) {
    getstore("$base/$image", $path . "$i");
    ++$i;
}
# assume all the same size, so just check first
if (!$size) {
    use Image::Size;
    my ($x, $y) = imgsize("${path}0");
    $size = "${x}x$y" if ($x && $y);
} 


print "amdesc::$desc\n";
printf "animatedimage::${path}%%1-$i%s\n", ($size && "-$size" || '');
print "updatetime::Last Updated on " . UnixDate("now", "%b %d, %I:%M %p %Z") . "\n";
