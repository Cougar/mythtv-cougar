<?php

class Video {

    var $intid;
    var $plot;
    var $category;
    var $rating;
    var $title;
    var $director;
    var $inetref;
    var $year;
    var $userrating;
    var $length;
    var $showlevel;
    var $filename;
    var $cover_file;
    var $cover_url;
    var $cover_scaled_width;
    var $cover_scaled_height;
    var $childid;
    var $url;
    var $browse;
    var $genres;

    function Video($intid) {
        $this->__construct($intid);
    }

    function __construct($intid) {
        global $db;
        global $mythvideo_dir;
        $video = $db->query_assoc('SELECT videometadata.*
                                     FROM videometadata
                                    WHERE videometadata.intid = ?',
                                  $intid
                                  );
        $this->intid        = $intid;
        $this->plot         = $video['plot'];
        $this->category     = $video['category'];
        $this->rating       = $video['rating'];
        $this->title        = $video['title'];
        $this->director     = $video['director'];
        $this->inetref      = $video['inetref'];
        $this->year         = $video['year'] ? $video['year'] : 'Unknown';
        $this->userrating   = $video['userrating'] ? $video['userrating'] : 'Unknown';
        $this->length       = $video['length'];
        $this->showlevel    = $video['showlevel'];
        $this->filename     = $video['filename'];
        $this->cover_file   = $video['coverfile'];
        $this->browse       = $video['browse'];
    // And the artwork URL
        if ($this->cover_file != 'No Cover' && file_exists($this->cover_file) ) {
            $this->cover_url = 'data/video_covers/'.substr($this->cover_file, strlen(setting('VideoArtworkDir', hostname)));
            list($width, $height) = getimagesize($this->cover_file);
            $wscale = video_img_width / $width;
            $hscale = video_img_height / $height;
            $scale = $wscale < $hscale ? $wscale : $hscale;
            $this->cover_scaled_width  = floor($width * $scale);
            $this->cover_scaled_height = floor($height * $scale);
        }
        else {
            $this->cover_scaled_height = video_img_height;
            $this->cover_scaled_width  = video_img_width;
        }
        $this->childid      = $video['childid'];
    // Figure out the URL
        $this->url = '#';
        if (file_exists('data/video/'))
            $this->url = root . implode('/', array_map('rawurlencode',
                                             array_map('utf8tolocal',
                                             explode('/',
                                             'data/video/' . preg_replace('#^'.$mythvideo_dir.'/?#', '', $this->filename)
                                       ))));
        $genre = $db->query('SELECT videometadatagenre.idgenre
                               FROM videometadatagenre
                              WHERE videometadatagenre.idvideo = ?',
                            $this->intid
                            );
        while( $id = $genre->fetch_col())
            $this->genres[] = $id;
        $genre->finish();
    }

// This function returns metadata preped for 'ajax' requests to update
    function metadata() {
        global $Category_String;
        return array( 'intid'       => $this->intid,
                      'img'         => '<img width="'.$this->cover_scaled_width.'" height="'.$this->cover_scaled_height.'" alt="'.t('Missing Cover').'"'
                                       .(($_SESSION["show_video_covers"] && file_exists($this->cover_url)) ? ' src="'.root.'data/video_covers/'.basename($this->cover_file).'"' : '')
                                       .'>',
                      'title'       => '<a href="'.$this->url.'">'.$this->title.'</a>',
                      'playtime'    => nice_length($this->length * 60),
                      'category'    => strlen($Category_String[$this->category]) ? $Category_String[$this->category] : 'Uncategorized',
                      'imdb'        => ($this->inetref != '00000000') ? '<a href="http://www.imdb.com/Title?'.$this->inetref.'">'.$this->inetref.'</a>' : '',
                      'plot'        => $this->plot,
                      'rating'      => $this->rating,
                      'director'    => $this->director,
                      'inetref'     => $this->inetref,
                      'year'        => $this->year,
                      'userrating'  => $this->userrating,
                      'length'      => $this->length,
                      'showlevel'   => $this->showlevel
                    );
    }

    function save() {
        global $db;
        $db->query('UPDATE videometadata
                       SET videometadata.plot         = ?,
                           videometadata.category     = ?,
                           videometadata.rating       = ?,
                           videometadata.title        = ?,
                           videometadata.director     = ?,
                           videometadata.inetref      = ?,
                           videometadata.year         = ?,
                           videometadata.userrating   = ?,
                           videometadata.length       = ?,
                           videometadata.showlevel    = ?,
                           videometadata.filename     = ?,
                           videometadata.coverfile    = ?,
                           videometadata.browse       = ?
                     WHERE videometadata.intid        = ?',
                    $this->plot,
                    $this->category,
                    $this->rating,
                    $this->title,
                    $this->director,
                    $this->inetref,
                    $this->year,
                    $this->userrating,
                    $this->length,
                    $this->showlevel,
                    $this->filename,
                    ( @filesize($this->cover_file) > 0 ? $this->cover_file : 'No Cover' ),
                    $this->browse,
                    $this->intid
                    );

        $db->query('DELETE FROM videometadatagenre
                          WHERE videometadatagenre.idvideo = ?',
                    $this->intid
                    );
        if (count($this->genres) > 0)
            foreach ($this->genres as $genre)
                $db->query('INSERT INTO videometadatagenre ( idvideo, idgenre )
                                                    VALUES (       ?,       ? )',
                           $this->intid,
                           $genre
                           );

    }
}
