<?php
/***                                                                        ***\
    mythmusic.php                             Last Updated: 2005.02.06 (xris)

    MythMusic
\***                                                                        ***/

// Which section are we in?
    define('section', 'music');

//
//  Someday, music.php will let us stream
//  entire playlists to any spot on planet earth
//
require_once "includes/init.php";
require_once theme_dir."mythmusic.php";

class mythMusic {
    var $filterPlaylist;
    var $filterArtist;
    var $filterAlbum;
    var $filterGenre;
    var $filterRank;
    var $filterSonglist;
    var $keepFilters;
    var $filter;
    var $totalCount;
    var $offset;

    var $result;


    var $intid;
    var $artist;
    var $album;
    var $title;
    var $genre;
    var $length;
    var $rating;
    var $filename;
    var $urlfilename;

    function mythMusic()
    {
        if($_GET['offset'] >=0 )
            $this->offset=$_GET['offset'];
        else
            $this->offset=0;



        if($_GET['filterPlaylist'])
        {
            $this->filterPlaylist=$_GET['filterPlaylist'];
            $_GET['filterPlaylist'];
        }
        else
            $this->filterPlaylist="_All_";

        if($_GET['filterArtist'])
        {
            $this->filterArtist=$_GET['filterArtist'];
        }
        else
            $this->filterArtist="_All_";

        if($_GET['filterAlbum'])
        {
            $this->filterAlbum=$_GET['filterAlbum'];
        }
        else
            $this->filterAlbum="_All_";
        if($_GET['filterGenre'])
        {
            $this->filterGenre=$_GET['filterGenre'];
        }
        else
            $this->filterGenre="_All_";


        if($_GET['filterRank'])
            $this->filterRank=$_GET['filterRank'];
        else
            $this->filterRank="_All_";
    }

    function readRow()
    {

            if($row=mysql_fetch_row($this->result))
            {
                $this->intid=$row[0];
                $this->artist=$row[1];
                $this->album=$row[2];
                $this->title=$row[3];
                $this->genre=$row[4];
                $this->length=$row[5];
                $this->rating=$row[6];
                $this->filename=$row[7];

                $this->urlfilename=music_url;
                global $musicdir;
                foreach (preg_split('/\//', substr($this->filename, strlen($musicdir))) as $dir) {
                    if (!$dir) continue;
                    if (function_exists('mb_convert_encoding'))
                        $this->urlfilename .= '/'.rawurlencode(mb_convert_encoding($dir, fs_encoding, 'UTF-8'));
                    else
                        $this->urlfilename .= '/'.rawurlencode($dir);
                }

                return(true);
            }
            return(false);
    }



    function display()
    {
        $music = new Theme_mythmusic();
        $this->init($music->getMaxPerPage());
        $music->setOffset($this->offset);
        $music->setTotalCount($this->totalCount);

        $music->print_header($this->filterPlaylist,$this->filterArtist,$this->filterAlbum,$this->filterGenre);
        if($this->totalCount > 0)
        {
            while($this->readRow())
            {
                $music->printDetail($this->title,$this->length,$this->artist,$this->album,$this->genre,$this->urlfilename);
            }
        }
        else
        {
            $music->printNoDetail();
        }
        if($this->result)
            mysql_free_result($this->result);

        $music->print_footer();
    }

    function prepFilter()
    {
        $prevFilter=0;
        $thisFilter="";

        if($this->filterPlaylist != "_All_")
        {
            $playlistResult = mysql_query("select playlistid,name,songlist,hostname from musicplaylist where playlistid=\"" . $this->filterPlaylist . "\"");
            if($playlistResult)
            {
                if(mysql_num_rows($playlistResult)==1)
                {
                    $row=mysql_fetch_row($playlistResult);
                    if($row)
                    {

                        $this->filterSonglist=$row[2];
                        if($prevFilter==1)
                            $this->filter=$this->filter . "and intid in (" . $this->filterSonglist . ")";
                        else
                        {
                            $this->filter="intid in (" . $this->filterSonglist . ")";
                            $prevFilter=1;
                        }

                        $this->keepFilters="&amp;filterPlaylist=" . urlencode($this->filterPlaylist);

                    }
                }
            }
        }

        if($this->filterArtist != "_All_" )
        {
            if($prevFilter==1)
                $this->filter=$this->filter . "and artist=\"" . $this->filterArtist . "\"";
            else
            {
                $this->filter="artist=\"" . $this->filterArtist . "\"";
                $prevFilter=1;
            }

            $this->keepFilters="&amp;filterArtist=" . urlencode($this->filterArtist);

        }
        if($this->filterAlbum != "_All_")
        {
            if($prevFilter==1)
            {
                $this->filter= $this->filter . "and album=\"" . $this->filterAlbum . "\"";
            }
            else
            {
                $this->filter="album=\"" . $this->filterAlbum . "\"";
                $prevFilter=1;
            }
            $this->keepFilters =$this->keepFilters . "&amp;filterAlbum=" . urlencode($this->filterAlbum) ;

        }
        if($this->filterGenre != "_All_")
        {
            if($prevFilter==1)
            {
                $this->filter= $this->filter . "and genre=" . $this->filterGenre ;
            }
            else
            {
                $this->filter="genre=\"" . $this->filterGenre . "\"";
                $prevFilter=1;
            }
            $this->keepFilters =$this->keepFilters . "&amp;filterGenre=" . urlencode($this->filterGenre);

        }

        if($this->filterRank != "_All_")
        {
            if($prevFilter==1)
            {
                $this->filter=$this->filter . "and rank=" . $this->filterRank;
            }
            else
            {
                $this->filter="rank=" . $this->filterRank;
                $prevFilter=1;
            }
            $this->keepFilters =$this->keepFilters . "&amp;filterRank=" . urlencode($this->filterRank);
        }



    }

    function init($maxPerPage) {
        $this->prepFilter();
        if($this->filter != "") {
            $result=mysql_query("select count(*) as cnt from musicmetadata where  $this->filter");
        }
        else
            $result=mysql_query("select count(*) as cnt from musicmetadata");

        if($result)
        {
            $this->totalCount = mysql_result($result,0,"cnt");
            mysql_free_result($result);
        }
        else
            $this->totalCount = 0;

        if($this->totalCount > 0)
        {
            if($this->offset > 0)
            {
                $limitText="LIMIT  " . $this->offset . " , " . $maxPerPage . " ";
            }
            else
                $limitText="LIMIT  " . $maxPerPage . " ";

            if($this->filter != "")
            {
                $this->result=mysql_query("select intid,artist,album,title,genre,length,rating,filename from musicmetadata where  $this->filter order by artist,album,tracknum $limitText");

            }
            else
            {
                $this->result=mysql_query("select intid,artist,album,title,genre,length,rating,filename from musicmetadata order by artist,album,tracknum " . $limitText);

            }
        }
    }
}


$mythmusic = new mythMusic();
$mythmusic->display();


?>