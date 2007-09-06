package BBCLocation;
use strict;
require Exporter;
use LWP::Simple;

our @EXPORT = qw(Search);
our $VERSION = 0.1;

my @searchresults;

sub Search {

    my $base_url = 'http://www.bbc.co.uk/cgi-perl/weather/search/new_search.pl?search_query=';

    my $search_string = shift;

    my $response = get $base_url . $search_string;
    die unless defined $response;

    my $isresults = 0;
    my $isredirect = 0;
    my $isworld = 0;

    my $resultline = "";

    foreach (split("\n", $response)) {
        if (/<title>/) {
            if (/Search Results\<\/title\>/) {
                $isresults = 1;
            }
            elsif (/5 Day Forecast in Celsius for/) {
                $isredirect = 1;
                my $locname = $_;
                $locname =~ s/.*?Celsius for (.*?)<\/title>.*/$1/;
                $resultline = "::" . $locname;
            }
        }

        my $locname;
        my $locid;

        if ($isresults) {

            if (/No locations were found/) {
                last;
            }
            elsif (/if query returns results close/) {
                last;
            }
            elsif (/non UK towns results open/) {
                $isworld = 1;
            }
            elsif (/\/weather\/5day\.shtml\?id=/) {
                $locid = $_;
                $locid =~ s/.*?\?id=(\d{0,4}).*/$1/s;

                $locname = $_;
                $locname =~ s/.*?<strong>(.*?)<.*/$1/s;

                $resultline = "L" . $locid . "::" . $locname . ", United Kingdom";
                push (@searchresults, $resultline);
            }
            elsif (/\/weather\/5day\.shtml\?world=/) {
                $locid = $_;
                $locid =~ s/.*?\?world=(\d{4}).*/$1/s;

                $locname = $_;
                $locname =~ s/.*?<strong>(.*?)<.*/$1/s;

                $resultline = "W" . $locid . "::" . $locname;

                unless ($isworld) {
                    $resultline = $resultline . ", United Kingdom";
                    push (@searchresults, $resultline);
                }
            }
            elsif ($isworld && /<\/a><\/strong>/) {
                my $country = $_;
                $country =~ s/.*?>([a-zA-Z ,']*)<\/a><\/strong>.*/$1/;
                $resultline = $resultline . ", " . $country;
                push (@searchresults, $resultline);
            }

        }
        elsif ($isredirect) {
            if (s/.*?<link.*?rss\/5day\/id\/(\d{4})\.xml.*/$1/) {
                $resultline = "L" . $_ . $resultline;
                push (@searchresults, $resultline);
                last;
            }
            if (s/.*?<link.*?rss\/5day\/world\/(\d{4})\.xml.*/$1/) {
                $resultline = "W" . $_ . $resultline;
                push (@searchresults, $resultline);
                last;
            }
        }

    }

    return @searchresults;
}

1;
