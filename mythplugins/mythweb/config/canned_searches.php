<?php
/***                                                                        ***\
    searches.php                             Last Updated: 2005.02.28 (xris)

    Should contain one variable, a hash of the "advanced" searches.
    Each of these searches will show up as an advanced search
\***                                                                        ***/

$Canned_Searches = array(

    "Movies"
        => "category_type='".movie_word."'",

    "Movies, 3 1/2 Stars or more"
        => "category_type='".movie_word."' AND program.stars > 0.8",

    "Non-Series HDTV"
        => "hdtv=1 AND category_type != 'series'",

    "All HDTV"
        => "hdtv=1",

    "Non-Music Specials"
        => "showtype='special' AND category NOT LIKE 'music%'",

    "Music Specials"
        => "showtype='special' AND category LIKE 'music%'",

    "Science Fiction Movies"
        => "category_type='".movie_word."' AND category='science fiction'",

    );

?>
