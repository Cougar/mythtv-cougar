#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups based on 
# the russian analog of www.imdb.com website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: Oleksiy Kokachev (kokachev AT gmail DOT com), based on imdb.pl script, made 
# by Tim Harvey. 



use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;


use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P);
use Getopt::Std; 

$title = "ilovecinema query"; 
$version = "v1.0";
$author = "Oleksiy Kokachev";

my @countries = qw(Russia);

binmode(STDOUT, ":utf8");

# display usage
sub usage {
   print "usage: $0 -hdrviMPD [parameters]\n";
   print "       -h           help\n";
   print "       -d           debug\n";
   print "       -r           dump raw query result data only\n";
   print "       -v           display version\n";
   print "       -i           display info\n";
   print "\n";
   print "       -M [options] <query>    get movie list\n";
   print "               some known options are:\n";
   print "                  mode=films   Show only films\n";
   print "               Note: multiple options must be separated by ';'\n";
   print "       -P <movieid>  get movie poster\n";
   print "       -D <movieid>  get movie data\n";
   exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
   print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
   print "Performs queries using the www.ilovecinema.ru website.\n";
}

# display detailed help 
sub help {
   version();
   info();
   usage();
}

sub trim {
   my ($str) = @_;
   $str =~ s/^\s+//;
   $str =~ s/\s+$//;
   return $str;
}

# returns text within 'data' between 'beg' and 'end' matching strings
sub parseBetween {
   my ($data, $beg, $end)=@_; # grab parameters

   my $ldata = lc($data);
   my $start = index($ldata, lc($beg)) + length($beg);
   my $finish = index($ldata, lc($end), $start);
   if ($start != (length($beg) -1) && $finish != -1) {
      my $result = substr($data, $start, $finish - $start);
      # return w/ decoded numeric character references
      # (see http://www.w3.org/TR/html4/charset.html#h-5.3.1)
      decode_entities($result);
      return $result;
   }
   return "";
}

# get Movie Data 
sub getMovieData {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   my $name_link_pat = qr'<a href="/name/[^"]*">([^<]*)</a>'m;

   # get the search results  page
   my $request = "http://www.ilovecinema.ru/films/" . $movieid . "/";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   utf8::decode($response);
   if (defined $opt_r) { printf("%s", $response); }

   # parse title and year
   my $title = "";
   my $year = "";
   my $cast = "";
   my $director = "";
   my $plot = "";
   my $lgenres = "";
   $data = parseBetween($response, "<div class=\"col1\">","/div>");
   $title = parseBetween($data,"<h1>","</h1>");

   #grab original english title if available
   if ($data =~ s/<p class=\"eng_name\">(.+)<//){
      $title = $title.'('.$1.')';
   }

   #grab year   
   $data = parseBetween($data,"<span id=\"filmInfoSubtitle\">","</span>");
   if ($data =~ /(\d+)/){
      $year = $1;
   }

   #parse movie description 
   my $plot_data = parseBetween($response,"<div class=\"film_descr\">","</div>");
   if ($plot_data !~ /.+no_descr/){
      if ($plot_data =~ s/<p>(.+)<\/p>//){
         $plot = $1;
      }
   }

   if (defined $opt_d) {
      printf("############################ plot raw data ##########################\n");
      printf("%s\n",$plot_data);
      printf("######################## end of plot raw data #######################\n");
   }
   #parse cast data
   my $cast_data = parseBetween($response,"<table class=\"film_persons\">","</table>");
   if ($cast_data) {
      $cast = join(",", ($cast_data =~ m/alt=\"(.+)\"/g));
   }

   if (defined $opt_d) {
      printf("############################ cast raw data ##########################\n");
      printf("%s\n",$cast_data);
      printf("######################## end of cast raw data #######################\n");
   }


   # output fields (these field names must match what MythVideo is looking for)
   print "Title:$title\n";
   print "Year:$year\n";
   print "Director:$director\n";
   print "Plot:$plot\n";
   print "Cast: $cast\n";
   print "Genres: $lgenres\n";
}

# dump Movie Poster
sub getMoviePoster {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.ilovecinema.ru/films/" . $movieid;
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   if (!defined $response) {return;}
   my $poster_data;
   my $uri = "";

   $poster_data = parseBetween($response,"<div class=\"main\">","<div class=\"tag_cloud\">");

   if (defined $opt_d) {
      printf("############################ poster raw data ##########################\n");
      printf("%s\n",$poster_data);
      printf("######################## end of poster raw data #######################\n");
   }

   if ($poster_data !~ s/no_film//){
      if ($poster_data =~ /src=\"(.+)_/){
         $uri = "http://www.ilovecinema.ru".$1.".jpg";
         print "$uri\n";
         return;
      }
   }

   if (defined $opt_d) {
      print "Poster not found on ilovecinema. Trying to find it on IMDB";
   }


   #if no poster available on ilovecinema, let's try to find it on imdb.
   $poster_data = parseBetween($response,"<div class=\"details_links\">","</div>");
   if ($poster_data =~ s/<a href=\"http:\/\/.{0,3}imdb.com\/title\/tt(\d+)\///){
      if (defined $opt_d) {
         print "Found IMDB number:".$1;
      }
      getIMDBMoviePoster($1);

   }

}

sub getIMDBMoviePoster {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.imdb.com/title/tt" . $movieid . "/posters";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   if (!defined $response) {return;}

   my $uri = "";

   # look for references to impawards.com posters - they are high quality
   my $site = "http://www.impawards.com";
   my $impsite = parseBetween($response, "<a href=\"".$site, "\">".$site);

   # jersey girl fix
   $impsite = parseBetween($response, "<a href=\"http://impawards.com","\">http://impawards.com") if ($impsite eq "");

   if ($impsite) {
      $impsite = $site . $impsite;

      if (defined $opt_d) { print "# Searching for poster at: ".$impsite."\n"; }
      my $impres = get $impsite;
      if (defined $opt_d) { printf("# got %i bytes\n", length($impres)); }
      if (defined $opt_r) { printf("%s", $impres); }      

      # making sure it isnt redirect
      $uri = parseBetween($impres, "0;URL=..", "\">");
      if ($uri ne "") {
         if (defined $opt_d) { printf("# processing redirect to %s\n",$uri); }
         # this was redirect
         $impsite = $site . $uri;
         $impres = get $impsite;
      }

      # do stuff normally
      $uri = parseBetween($impres, "<img SRC=\"posters/", "\" ALT");
      # uri here is relative... patch it up to make a valid uri
      if (!($uri =~ /http:(.*)/ )) {
         my $path = substr($impsite, 0, rindex($impsite, '/') + 1);
         $uri = $path."posters/".$uri;
      }
      if (defined $opt_d) { print "# found ipmawards poster: $uri\n"; }
   }

   # try looking on nexbase
   if ($uri eq "" && $response =~ m/<a href="([^"]*)">([^"]*?)nexbase/i) {
      if ($1 ne "") {
         if (defined $opt_d) { print "# found nexbase poster page: $1 \n"; }
         my $cinres = get $1;
         if (defined $opt_d) { printf("# got %i bytes\n", length($cinres)); }
         if (defined $opt_r) { printf("%s", $cinres); }

         if ($cinres =~ m/<a id="photo_url" href="([^"]*?)" ><\/a>/i) {
            if (defined $opt_d) { print "# nexbase url retreived\n"; }
            $uri = $1;
         }
      }
   }

   # try looking on cinemablend
   if ($uri eq "" && $response =~ m/<a href="([^"]*)">([^"]*?)cinemablend/i) {
      if ($1 ne "") {
         if (defined $opt_d) { print "# found cinemablend poster page: $1 \n"; }
         my $cinres = get $1;
         if (defined $opt_d) { printf("# got %i bytes\n", length($cinres)); }
         if (defined $opt_r) { printf("%s", $cinres); }

         if ($cinres =~ m/<td align=center><img src="([^"]*?)" border=1><\/td>/i) {
            if (defined $opt_d) { print "# cinemablend url retreived\n"; }
            $uri = "http://www.cinemablend.com/".$1;   
         }
      }
   }

   # if the impawards site attempt didn't give a filename grab it from imdb
   if ($uri eq "") {
      if (defined $opt_d) { print "# looking for imdb posters\n"; }
      my $host = "http://posters.imdb.com/posters/";

      $uri = parseBetween($response, $host, "\"><td><td><a href=\"");
      if ($uri ne "") {
         $uri = $host.$uri;
      } else {
         if (defined $opt_d) { print "# no poster found\n"; }
      }
   }



   my @movie_titles;
   my $found_low_res = 0;
   my $k = 0;

   # no poster found, take lowres image from imdb
   if ($uri eq "") {
      if (defined $opt_d) { print "# looking for lowres imdb posters\n"; }
      my $host = "http://www.imdb.com/title/tt" . $movieid . "/";
      $response = get $host;

      # Better handling for low resolution posters
      # 
      if ($response =~ m/<a name="poster".*<img.*src="([^"]*).*<\/a>/ig) {
         if (defined $opt_d) { print "# found low res poster at: $1\n"; }
         $uri = $1;
         $found_low_res = 1;
      } else {
         if (defined $opt_d) { print "# no low res poster found\n"; }
         $uri = "";
      }

      if (defined $opt_d) { print "# starting to look for movie title\n"; }

      # get main title
      if (defined $opt_d) { print "# Getting possible movie titles:\n"; }
      $movie_titles[$k++] = parseBetween($response, "<title>", "<\/title>");
      if (defined $opt_d) { print "# Title: ".$movie_titles[$k-1]."\n"; }

      # now we get all other possible movie titles and store them in the titles array
      while($response =~ m/>([^>^\(]*)([ ]{0,1}\([^\)]*\)[^\(^\)]*[ ]{0,1}){0,1}\(informal title\)/g) {
         $movie_titles[$k++] = trim($1);
         if (defined $opt_d) { print "# Title: ".$movie_titles[$k-1]."\n"; }
      }

   }

   print "$uri\n";
}




sub getMovieList {
   my ($filename, $options)=@_; # grab parameters

   # If we wanted to inspect the file for any reason we can do that now
   #
   # Convert filename into a query string 
   # (use same rules that Metadata::guesTitle does)
   my $query = $filename;
   $query = uri_unescape($query);  # in case it was escaped
   # Strip off the file extension
   if (rindex($query, '.') != -1) {
      $query = substr($query, 0, rindex($query, '.'));
   }
   # Strip off anything following '(' - people use this for general comments
   if (rindex($query, '(') != -1) {
      $query = substr($query, 0, rindex($query, '(')); 
   }
   # Strip off anything following '[' - people use this for general comments
   if (rindex($query, '[') != -1) {
      $query = substr($query, 0, rindex($query, '[')); 
   }

   # IMDB searches do better if any trailing ,The is left off
   $query =~ /(.*), The$/i;
   if ($1) { $query = $1; }

   # prepare the url 
   $query = uri_escape($query);
   if (!$options) { $options = "" ;}
   if (defined $opt_d) { 
      printf("# query: '%s', options: '%s'\n", $query, $options);
   }

   # get the search results  page
   #    some known ilovecinema options are:  
   #    mode=films   Show only films
   my $request = "http://ilovecinema.ru/search/?q=$query&$options";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) {
      print $response;
      exit(0);
   }

   # extract possible matches
   #    possible matches are grouped in several catagories:  
   #        exact, partial, and approximate
   my $exact_matches = parseBetween($response, "<div class=\"search_result\">","</div>");

   # parse movie list from matches
   if (defined $opt_d) { printf("# exact_matches: '%s'\n", $exact_matches); }
   my $beg = "<tr>";
   my $end = "</tr>";
   my $count = 0;
   my @movies;

   my $data = $exact_matches;
   # resort to approximate matches if no exact or partial
   if ($data eq "") {
      if (defined $opt_d) { printf("# no results\n"); }
      return; 
   }
   my $start = index($data, $beg);
   my $finish = index($data, $end, $start);
   my $year;
   my $type;
   my $title;
   while ($start != -1 && $start < length($data)) {
      $start += length($beg);
      my $entry = substr($data, $start, $finish - $start);
      $start = index($data, $beg, $finish + 1);
      $finish = index($data, $end, $start);

      my $title = "";
      my $eng_title = "";
      my $year = "";
      my $type = "";
      my $movienum = "";

      if ($entry =~ /<a href="\/films\/(.+)\/".+alt="(.+)"/) {
         $movienum = $1;
         $title = $2;
         utf8::decode($title);
#          $year = $3;
#          $type = $4 if ($4);
      } else {
         if (defined $opt_d) {
            print("Unrecognized entry format ($entry)\n");
         }
         next;
      }
      my $skip = 0;


      # add to array
      if (!$skip) {
         my $moviename = $title;
         if ($year ne "") {
            $moviename .= " ($year)";
         }

#         $movies[$count++] = $movienum . ":" . $title;
         $movies[$count++] = $movienum . ":" . $moviename;
      }
   }

   # display array of values
   for $movie (@movies) { print "$movie\n"; }
}

#
# Main Program
#

# parse command line arguments 
getopts('ohrdivDMP');

# print out info 
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }

# print out usage if needed
if (defined $opt_h || $#ARGV<0) { help(); }

if (defined $opt_D) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -D <movieid>\n";
   getMovieData($movieid);
}
elsif (defined $opt_P) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -P <movieid>\n";
   getMoviePoster($movieid);
}
elsif (defined $opt_M) {
   # take query from cmdline arg
   $options = shift || die "Usage : $0 -M [options] <query>\n";
   $query = shift;
   if (!$query) {
      $query = $options;
      $options = "";
   }
   getMovieList($query, $options);
}
# vim: set expandtab ts=3 sw=3 :
