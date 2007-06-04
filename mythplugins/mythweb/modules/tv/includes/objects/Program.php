<?php
/**
 * Program class
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  TV
 *
/**/

/**
 * Program class
/**/
class Program {

// The following fields are (in order) the fields returned from the backend on
// a standard query.
    var $title;
    var $subtitle;
    var $description;
    var $category;
    var $chanid;
    var $channum;
    var $callsign;
    var $channame;
    var $filename;
    var $fs_high;
    var $fs_low;
    var $starttime;
    var $endtime;
    var $duplicate;
    var $shareable;
    var $findid;
    var $hostname;
    var $sourceid;
    var $cardid;
    var $inputid;
    var $recpriority    = 0;
    var $recstatus;
    var $recordid;
    var $rectype;
    var $dupin;
    var $dupmethod;
    var $recstartts;
    var $recendts;
    var $previouslyshown;
    var $progflags;
    var $recgroup;
    var $commfree;
    var $outputfilters;
    var $seriesid;
    var $programid;
    var $lastmodified;
    var $stars;
    var $airdate;
    var $hasairdate;
    var $playgroup      = 'Default';
    var $recpriority2   = 0;
    var $parentid;
    var $storagegroup   = 'Default';

// The rest of these variables (which really need to get organized) are
// calculated or queried separately from the db.
    var $auto_expire    = 0;
    var $bookmark       = 0;
    var $category_type;
    var $channel;           // this should be a reference to the $Channel array value
    var $conflicting    = false;
    var $credits        = array();
    var $css_class;         // css class, based on category and/or category_type
    var $fancy_description;
    var $filesize;
    var $group          = '';
    var $has_commflag   = 0;
    var $has_cutlist    = 0;
    var $is_recording;
    var $is_playing;
    var $stereo;
    var $closecaptioned;
    var $can_delete     = false;
    var $hdtv;
    var $inputname;
    var $is_editing     = 0;
    var $is_movie;
    var $is_watched     = 0;
    var $length;
    var $max_episodes   = 0;
    var $max_newest     = 0;
    var $profile        = 0;
    var $rater;
    var $rating;
    var $recording      = false;
    var $starstring;
    var $timestretch;
    var $url;

    var $jobs          = array();   // recent/pending jobqueue entries
    var $jobs_possible = array();   // Jobs this program can be assigned to

    function Program($data) {
        global $db;
    // This is a mythbackend-formatted program - info about this data structure is stored in libs/libmythtv/programinfo.cpp
        if (!isset($data['chanid']) && isset($data[0])) {
        // Load the remaining info we got from mythbackend
            $this->title           = $data[0];      # program name/title
            $this->subtitle        = $data[1];      # episode name
            $this->description     = $data[2];      # episode description
            $this->category        = $data[3];
            $this->chanid          = $data[4];      # mysql chanid
            $this->channum         = $data[5];
            $this->callsign        = $data[6];
            $this->channame        = $data[7];
            $this->filename        = $data[8];
            $this->fs_high         = $data[9];      # high-word of file size
            $this->fs_low          = $data[10];     # low-word of file size
            $this->starttime       = $data[11];     # show start-time
            $this->endtime         = $data[12];     # show end-time
            $this->duplicate       = $data[13];
            $this->shareable       = $data[14];
            $this->findid          = $data[15];
            $this->hostname        = $data[16];
            $this->sourceid        = $data[17];
            $this->cardid          = $data[18];
            $this->inputid         = $data[19];
            $this->recpriority     = $data[20];
            $this->recstatus       = $data[21];
            $this->recordid        = $data[22];
            $this->rectype         = $data[23];
            $this->dupin           = $data[24];
            $this->dupmethod       = $data[25];
            $this->recstartts      = $data[26];     # ACTUAL start time (also maps to recorded.starttime)
            $this->recendts        = $data[27];     # ACTUAL end time
            $this->previouslyshown = $data[28];     # "repeat" field
            $this->progflags       = $data[29];
            $this->recgroup        = $data[30];
            $this->commfree        = $data[31];
            $this->outputfilters   = $data[32];
            $this->seriesid        = $data[33];
            $this->programid       = $data[34];
            $this->lastmodified    = $data[35];
            $this->stars           = $data[36];
            $this->airdate         = $data[37];
            $this->hasairdate      = $data[38];
            $this->playgroup       = $data[39];
            $this->recpriority2    = $data[40];
            $this->parentid        = $data[41];
            $this->storagegroup    = $data[42];
        // Is this a previously-recorded program?
            if (!empty($this->filename)) {
            // Calculate the filesize
                if (function_exists('gmp_add')) {
                // GMP functions should work better with 64 bit numbers.
                    $size = gmp_add($this->fs_low,
                                     gmp_mul('4294967296',
                                             gmp_add($this->fs_high, $this->fs_low < 0 ? '1' : '0'))
                                   );
                    $this->filesize = gmp_strval($size);
                }
                else {
                // This is inaccurate, but it's the best we can get without GMP.
                    $this->filesize = ($this->fs_high + ($this->fs_low < 0)) * 4294967296 + $this->fs_low;
                }
            // And get some download info
                $this->url = video_url($this);
            }
        // Assign the program flags
            $this->has_commflag   = ($this->progflags & 0x001) ? true : false;    // FL_COMMFLAG       = 0x001
            $this->has_cutlist    = ($this->progflags & 0x002) ? true : false;    // FL_CUTLIST        = 0x002
            $this->auto_expire    = ($this->progflags & 0x004) ? true : false;    // FL_AUTOEXP        = 0x004
            $this->is_editing     = ($this->progflags & 0x008) ? true : false;    // FL_EDITING        = 0x008
            $this->bookmark       = ($this->progflags & 0x010) ? true : false;    // FL_BOOKMARK       = 0x010
            $this->is_recording   = ($this->progflags & 0x020) ? true : false;    // FL_INUSERECORDING = 0x020
            $this->is_playing     = ($this->progflags & 0x040) ? true : false;    // FL_INUSEPLAYING   = 0x040
            $this->stereo         = ($this->progflags & 0x080) ? true : false;    // FL_STEREO         = 0x080
            $this->closecaptioned = ($this->progflags & 0x100) ? true : false;    // FL_CC             = 0x100
            $this->hdtv           = ($this->progflags & 0x200) ? true : false;    // FL_HDTV           = 0x200
                                                                                  // FL_TRANSCODED     = 0x400
            $this->is_watched     = ($this->progflags & 0x800) ? true : false;    // FL_WATCHED        = 0x800
        // Can be deleted?
            $this->can_delete     = (!$this->is_recording && !$this->is_playing) || $this->recgroup != 'LiveTV';
        // Add a generic "will record" variable, too
            $this->will_record = ($this->rectype && $this->rectype != rectype_dontrec) ? true : false;
        }
    // SQL data
        else {
            if (in_array($data['airdate'], array('0000-00-00', '0000', '1900-01-01')))
                $this->airdate = $data['originalairdate'];
            else
                $this->airdate = $data['airdate'];
            $this->category                = _or($data['category'],        t('Unknown'));
            $this->category_type           = _or($data['category_type'],   t('Unknown'));
            $this->chanid                  = $data['chanid'];
            $this->description             = $data['description'];
            $this->endtime                 = $data['endtime_unix'];
            $this->hdtv                    = $data['hdtv'];
            $this->previouslyshown         = $data['previouslyshown'];
            $this->programid               = $data['programid'];
            $this->rater                   = $data['rater'];
            $this->rating                  = $data['rating'];
            $this->seriesid                = $data['seriesid'];
            $this->showtype                = $data['showtype'];
            $this->stars                   = $data['stars'];
            $this->starttime               = $data['starttime_unix'];
            $this->subtitle                = $data['subtitle'];
            $this->subtitled               = $data['subtitled'];
            $this->title                   = $data['title'];
            $this->partnumber              = $data['partnumber'];
            $this->parttotal               = $data['parttotal'];
            $this->stereo                  = $data['stereo'];
            $this->closecaptioned          = $data['closecaptioned'];
            $this->colorcode               = $data['colorcode'];
            $this->syndicatedepisodenumber = $data['syndicatedepisodenumber'];
            $this->title_pronounce         = $data['title_pronounce'];
            $this->recstatus               = $data['recstatus'];

            if ($data['tsdefault']) {
                $this->timestretch = $data['tsdefault'];
            } else {
                $this->timestretch = 1.0;
            }
        }
    // Generate the star string, since mysql has issues with REPEAT() and
    // decimals, and the backend doesn't do it for us, anyway.
        $this->starstring = str_repeat(star_character, intVal($this->stars * max_stars));
        $frac = ($this->stars * max_stars) - intVal($this->stars * max_stars);
        if ($frac >= .75)
            $this->starstring .= '&frac34;';
        elseif ($frac >= .5)
            $this->starstring .= '&frac12;';
        elseif ($frac >= .25)
            $this->starstring .= '&frac14;';
    // Get the name of the input
        if ($this->inputid) {
            $this->inputname = $db->query_col('SELECT displayname
                                                 FROM cardinput
                                                WHERE cardinputid=?',
                                              $this->inputid);
        }
    // Turn recstatus into a word
        if (isset($this->recstatus) && $GLOBALS['RecStatus_Types'][$this->recstatus]) {
            $this->recstatus = $GLOBALS['RecStatus_Types'][$this->recstatus];
            $this->conflicting = ($this->recstatus == 'Conflict');   # conflicts with another scheduled recording?
            $this->recording   = ($this->recstatus == 'WillRecord'); # scheduled to record?
        }
    // No longer a null column, so check for blank entries
        if (in_array($this->airdate, array('0000-00-00', '0000', '1900-01-01')))
            $this->airdate = NULL;
    // Do we have a chanid?  Load some info about it
        if ($this->chanid && !isset($this->channel)) {
        // No channel data?  Load it
            global $Channels;
            if (!is_array($Channels) || !count($Channels))
                load_all_channels();
        // Now we really should scan the $Channel array and add a link to this program's channel
            foreach (array_keys($Channels) as $key) {
                if ($Channels[$key]->chanid == $this->chanid) {
                    $this->channel =& $Channels[$key];
                    break;
                }
            }
        // Not found
            if (!$this->channel)
                $this->channel =& load_one_channel($this->chanid);
        }
    // Calculate the duration
        if ($this->recendts)
            $this->length = $this->recendts - $this->recstartts;
        else
            $this->length = $this->endtime - $this->starttime;

    // A special recstatus for shows that this was manually set to record
        if ($this->rectype == rectype_override)
            $this->recstatus = 'ForceRecord';

    // Find out which css category this program falls into
        if ($this->chanid != '')
            $this->css_class = category_class($this);
    // Create the fancy description
        $this->update_fancy_desc();
    }

    function merge($prog) {
        foreach (get_object_vars($prog) as $name => $value) {
            if ($value && !$this->$name) {
                $this->$name = $value;
            }
        }
    // Special case for the original airdate, which the backend seems to misplace
        if ($prog->has_airdate || !in_array($prog->airdate, array('0000-00-00', '0000', '1900-01-01')))
            $this->airdate = $prog->airdate;
    // update fancy description in case a part of it changed
        $this->update_fancy_desc();
    }

    function update_fancy_desc() {
        // Get a nice description with the full details
        $details = array();
        if ($this->hdtv)
            $details[] = t('HDTV');
        if ($this->parttotal > 1 || $this->partnumber > 1)
            $details[] = t('Part $1 of $2', $this->partnumber, $this->parttotal);
        if ($this->rating)
            $details[] = $this->rating;
        if ($this->subtitled)
            $details[] = t('Subtitled');
        if ($this->closecaptioned)
            $details[] = t('CC');
        if ($this->stereo)
            $details[] = t('Stereo');
        if ($this->previouslyshown)
            $details[] = t('Repeat');

        $this->fancy_description = $this->description;
        if (count($details) > 0)
            $this->fancy_description .= ' ('.implode(', ', $details).')';
    }

/**
 * Load info about any queued or recently finished jobs
/**/
    function load_jobs() {
        if (empty($this->filename))
            return;
    // Make sure the jobqueue constants are defined
        require_once 'includes/jobqueue.php';
    // Keep track of which jobs are possible to be started (due to not already
    // being in the queue).
        global $Jobs;
        $this->jobs_possible = $Jobs;
    // Load the info
        global $db;
        $sh = $db->query('SELECT *,
                                 UNIX_TIMESTAMP(statustime) AS statustime
                            FROM jobqueue
                           WHERE starttime  = FROM_UNIXTIME(?)
                                 AND chanid = ?
                        ORDER BY statustime DESC',
                         $this->recstartts,
                         $this->chanid);
        $this->jobs      = array();
        while ($row = $sh->fetch_assoc()) {
            if ($row['status'] & JOB_DONE)
                $this->jobs['done'][] = $row;
            else {
                $this->jobs['queue'][] = $row;
                unset($this->jobs_possible[$row['type']]);
            }
        }
        $sh->finish();

    }

/**
 * Generate a mythproto-compatible row of data for this show.
/**/
    function backend_row() {
        return implode(backend_sep,
                       array(
                             $this->title          , // 00 title
                             $this->subtitle       , // 01 subtitle
                             $this->description    , // 02 description
                             $this->category       , // 03 category
                             $this->chanid         , // 04 chanid
                             $this->channum        , // 05 chanstr
                             $this->callsign       , // 06 chansign
                             $this->channame       , // 07 channame
                             $this->filename       , // 08 pathname
                             $this->fs_high        , // 09 filesize upper 32 bits
                             $this->fs_low         , // 10 filesize lower 32 bits
                             $this->starttime      , // 11 startts
                             $this->endtime        , // 12 endts
                             $this->duplicate      , // 13 duplicate
                             $this->shareable      , // 14 shareable
                             $this->findid         , // 15 findid
                             $this->hostname       , // 16 hostname
                             $this->sourceid       , // 17 sourceid
                             $this->cardid         , // 18 cardid
                             $this->inputid        , // 19 inputid
                             $this->recpriority    , // 20 recpriority
                             $this->recstatus      , // 21 recstatus
                             $this->recordid       , // 22 recordid
                             $this->rectype        , // 23 rectype
                             $this->dupin          , // 24 dupin
                             $this->dupmethod      , // 25 dupmethod
                             $this->recstartts     , // 26 recstartts
                             $this->recendts       , // 27 recendts
                             $this->previouslyshown, // 28 repeat
                             $this->progflags      , // 29 programflags
                             $this->recgroup       , // 30 recgroup
                             $this->commfree       , // 31 chancommfree
                             $this->outputfilters  , // 32 chanOutputFilters
                             $this->seriesid       , // 33 seriesid
                             $this->programid      , // 34 programid
                             $this->lastmodified   , // 35 lastmodified
                             $this->stars          , // 36 stars
                             $this->airdate        , // 37 originalAirDate
                             $this->hasairdate     , // 38 hasAirDate
                             $this->playgroup      , // 39 playgroup
                             $this->recpriority2   , // 40 recpriority2
                             $this->parentid       , // 41 parentid
                             $this->storagegroup   , // 42 storagegroup
                             '',                     // 43 trailing separator
                            )
                      );
    }

/**
 * Get the last modification date of the pixmap that corresponds to this
 * recording.
/**/
    function pixmap_last_mod() {
        $mod = backend_command('QUERY_PIXMAP_LASTMODIFIED'
                               .backend_sep
                               .$this->backend_row()
                              );
        if ($mod == 'BAD')
            return 0;
        return strtotime($mod);
    }

/**
 * Generate a new preview pixmap for this recording.
/**/
    function generate_pixmap() {
        $ret = backend_command('QUERY_GENPIXMAP'
                               .backend_sep
                               .$this->backend_row()
                              );
        if ($ret == 'BAD') {
            // echo "Unknown error generating pixmap for $this->chanid:$this->starttime\n";
            return 0;
        }
        return 1;
    }

/**
 * Generate a thumbnail of the requested size, and return the URL to its cache
 * location.
/**/
    function thumb_url($width=160, $height=120, $secs_in=null) {
    // Generate the pixmap, just in case
        $this->generate_preview_pixmap($width, $height, $secs_in);
    // Calculate the default secs_in
        static $def_secs_in = null;
        if (is_null($def_secs_in))
            $def_secs_in = _or(get_backend_setting('PreviewPixmapOffset'), 64)
                           + _or(get_backend_setting('RecordPreRoll'), 0);
    // We have to calulate $secs_in from the db
        if (is_null($secs_in))
            $secs_in = $def_secs_in;
    // Now, figure out the filename
        if ($width == 160 && $height == 120 && $secs_in == $def_secs_in) {
            $fileurl .= "$this->filename.png";
        }
        else {
            $fileurl = "$this->filename.{$width}x{$height}x$secs_in.png";
        }
        return root.cache_dir.'/'.str_replace('%2F', '/', rawurlencode(basename($fileurl)));
    }

/**
 * Gets a preview image of the requested show
 *
 * @todo, this should get put into a "recording" class or something like that.
/**/
    function generate_preview_pixmap($width=160, $height=120, $secs_in=null) {
    // Calculate the default secs_in
        static $def_secs_in = null;
        if (is_null($def_secs_in))
            $def_secs_in = _or(get_backend_setting('PreviewPixmapOffset'), 64)
                           + _or(get_backend_setting('RecordPreRoll'), 0);
    // We have to calulate $secs_in from the db
        if (is_null($secs_in))
            $secs_in = $def_secs_in;
    // Now, figure out the filenames
        if ($width == 160 && $height == 120 && $secs_in == $def_secs_in) {
            $fileurl    .= "$this->filename.png";
            $is_default  = true;
        }
        else {
            $fileurl    = "$this->filename.{$width}x{$height}x$secs_in.png";
            $is_default = false;
        }
        $pngpath  = cache_dir . '/' . basename($fileurl);
    // Make sure the local path exists
        $path = '';
        foreach (split('/+', dirname($pngpath)) as $dir) {
            $path .= $path ? '/' . $dir : $dir;
            if(!is_dir($path) && !mkdir($path, 0755))
                trigger_error('Error creating path for '.$path.': Please check permissions.', FATAL);
        }
    // Find out when the pixmap was last modified
        $png_mod = $this->pixmap_last_mod();
    // Regenerate the pixmap if the recording has since been updated
        if ($png_mod < $this->lastmodified) {
            $png_mod = $this->lastmodified;
            if (!$this->generate_pixmap()) {
                return null;
            }
        }
    // Is our target file already up to date?
        if (is_file($pngpath)) {
            $mtime = filemtime($pngpath);
            if ($mtime >= $png_mod) {
                return 1;
            }
        }
    // Nonstandard dimensions currently require the XML interface
        if (!$is_default) {
        // Figure out which host holds the file we need
            $urlparts = parse_url($fileurl);
            $host     = _or($urlparts['host'], $GLOBALS['Master_Host']);
            $port     = _or(get_backend_setting('BackendStatusPort', $host),
                            get_backend_setting('BackendStatusPort'));
        // Make the request and store the result
            $pngfile = fopen($pngpath, 'wb');
            fwrite($pngfile,
                   file_get_contents("http://$host:$port/Myth/GetPreviewImage"
                                    ."?ChanId=$this->chanid"
                                    .'&StartTime='.unix2mythtime($this->recstartts)
                                    ."&Height=$height"
                                    ."&Width=$width"
                                    ."&SecsIn=$secs_in"
                                    )
                  );
            fclose($pngfile);
        }
    // Standard width can be copied locally, or via mythproto
        else {
        // Local path to the png that we can just copy from?
        /** This probably won't work anymore now that the backend always sends
         *  myth:// URIs.  Need to update the Program/Recording object to detect
         *  file locations and storage groups so we can get local_path working */
            if (substr($fileurl, 0, 7) != 'myth://' && is_file($fileurl) && is_readable($fileurl)) {
                copy($fileurl, $pngpath);
                return 2;
            }
        // Figure out which host holds the recording
            $urlparts = parse_url($fileurl);
            $host     = _or($urlparts['host'], $GLOBALS['Master_Host']);
            $port     = _or($urlparts['port'], $GLOBALS['Master_Port']);
        // Transfer the pixmap from the backend
            $recs = explode(backend_sep, backend_command2(array('ANN FileTransfer '.hostname, $fileurl),
                                                          $datasocket,
                                                          $host, $port));
        // Error?
            if ($recs[0] != 'OK') {
                // echo "Unknown error starting transfer of $fileurl.png\n";
                return null;
            }
        // Open the output file for writing, and make sure it's in binmode
            $pngfile = fopen($pngpath, 'wb');
        // Tell the backend to send the data
            backend_command('ANN Playback '.hostname.' 0',
                            $host, $port);
            backend_command(array('QUERY_FILETRANSFER '.$recs[1], 'REQUEST_BLOCK', $recs[3]),
                            $host, $port);
        // Read the data from the socket and save it into the requested file.
        // We have to loop here because sometimes the backend can't send data fast
        // enough, even if we're dealing with small files.
            $length = $recs[3];
            while ($length > 0) {
                $data = fread($datasocket, min(8192, $length));
                if (strlen($data) < 1)
                    break; // EOF
                fwrite($pngfile, $data);
                $length -= strlen($data);
            }
        // Close any file pointers that were opened here
            fclose($pngfile);
            if ($datasocket) {
                fclose($datasocket);
            }
        }
    }

/**
 * The "details list" for each program.
/**/
    function details_list() {
    // Start the list, and print the show airtime and title
        $str = "<dl class=\"details_list\">\n"
            // Airtime
              ."\t<dt>".t('Airtime').":</dt>\n"
              ."\t<dd>".t('$1 to $2',
                          strftime($_SESSION['time_format'], $this->starttime),
                          strftime($_SESSION['time_format'], $this->endtime))
                       ."</dd>\n"
        // Channel
              ."\t<dt>".t('Channel').":</dt>\n"
              ."\t<dd>".(prefer_channum ? $this->channel->channum : $this->channel->callsign)
                       .' - '.$this->channel->name
                       ."</dd>\n"
            // Title
              ."\t<dt>".t('Title').":</dt>\n"
              ."\t<dd>".html_entities($this->title)
                       ."</dd>\n";
    // Subtitle
        if (preg_match('/\\S/', $this->subtitle)) {
            $str .= "\t<dt>".t('Subtitle').":</dt>\n"
                   ."\t<dd>".html_entities($this->subtitle)
                            ."</dd>\n";
        }
    // Description
        if (preg_match('/\\S/', $this->fancy_description)) {
            $str .= "\t<dt>".t('Description').":</dt>\n"
                   ."\t<dd>".nl2br(html_entities($this->fancy_description))
                            ."</dd>\n";
        }
    // Original Airdate
        if (!empty($this->airdate)) {
            $str .= "\t<dt>".t('Original Airdate').":</dt>\n"
                   ."\t<dd>".html_entities($this->airdate)
                            ."</dd>\n";
        }
    // Category
        if (preg_match('/\\S/', $this->category)) {
            $str .= "\t<dt>".t('Category').":</dt>\n"
                   ."\t<dd>".html_entities($this->category)
                            ."</dd>\n";
        }
    // Will be recorded at some point in the future?
        if (!empty($this->will_record)) {
            $str .= "\t<dt>".t('Schedule').":</dt>\n"
                   ."\t<dd>";
            switch ($this->rectype) {
                case rectype_once:       $str .= t('rectype-long: once');       break;
                case rectype_daily:      $str .= t('rectype-long: daily');      break;
                case rectype_channel:    $str .= t('rectype-long: channel', prefer_channum ? $this->channel->channum : $this->channel->callsign);    break;
                case rectype_always:     $str .= t('rectype-long: always');     break;
                case rectype_weekly:     $str .= t('rectype-long: weekly');     break;
                case rectype_findone:    $str .= t('rectype-long: findone');    break;
                case rectype_override:   $str .= t('rectype-long: override');   break;
                case rectype_dontrec:    $str .= t('rectype-long: dontrec');    break;
                case rectype_finddaily:  $str .= t('rectype-long: finddaily');  break;
                case rectype_findweekly: $str .= t('rectype-long: findweekly'); break;
                default:                 $str .= t('Unknown');
            }
            $str .= "</dd>\n";
        }
    // Recording status
        if (!empty($this->recstatus)) {
            $str .= "\t<dt>".t('Notes').":</dt>\n"
                   ."\t<dd>".$GLOBALS['RecStatus_Reasons'][$this->recstatus]
                            ."</dd>\n";
        }
    // Finish off the table and return
        $str .= "\n</dl>";
        return $str;
    }

    function get_credits($role) {
    // Not enough info in this object
        if (!$this->chanid || !$this->starttime)
            return '';
    // No cached value -- load it
        if (!isset($this->credits[$role])) {
        // Get the credits for the requested role
            $query  = 'SELECT people.name FROM credits, people'
                     .' WHERE credits.person=people.person'
                     .' AND credits.role='.escape($role)
                     .' AND credits.chanid='.escape($this->chanid)
                     .' AND credits.starttime=FROM_UNIXTIME('.escape($this->starttime).')';
            $result = mysql_query($query)
                or trigger_error('SQL Error: '.mysql_error(), FATAL);
            $people = array();
            while (list($name) = mysql_fetch_row($result)) {
                $people[] = $name;
            }
            mysql_free_result($result);
        // Cache it
            $this->credits[$role] = trim(implode(', ', $people));
        }
        return $this->credits[$role];
    }

/*
 *  The following methods relate to a program's control over its recording options.
 */

/**
 * Forget everything about a previously recorded program
 *
 * @todo Eventually, all of this should get separated out of the Program class
 * and into something more generic, since this backend command is called from
 * several places depending on if a program or a non-expanded data row is being
 * used.
/**/
    function rec_forget_old() {
        global $db;
    // The FORGET_RECORDING command requires the specific record to be
    // forgotten, so we have to search for matching rows
        $sh = $db->query('SELECT *
                            FROM oldrecorded
                           WHERE title=? AND subtitle=? AND description=?',
                         $this->title,
                         $this->subtitle,
                         $this->description);
        while ($row = $sh->fetch_assoc()) {
            $prog =& new Program($row);
            backend_command(array('FORGET_RECORDING', $prog->backend_row(), '0'));
        }
        $sh->finish();
    // Delay a second so the scheduler can catch up
        sleep(1);
    }

/**
 * "Never" record this show, by telling mythtv that it was already recorded
/**/
    function rec_never_record() {
        $result = mysql_query('REPLACE INTO oldrecorded (chanid,starttime,endtime,title,subtitle,description,category,seriesid,programid,recordid,station,rectype,recstatus,duplicate) VALUES ('
                                .escape($this->chanid)                    .','
                                .'NOW()'                                  .','
                                .'NOW()'                                  .','
                                .escape($this->title)                     .','
                                .escape($this->subtitle)                  .','
                                .escape($this->description)               .','
                                .escape($this->category)                  .','
                                .escape($this->seriesid)                  .','
                                .escape($this->programid)                 .','
                                .escape($this->recordid)                  .','
                                .escape($this->station)                   .','
                                .escape($this->rectype)                   .','
                                .'11'                                     .','
                                .'1'                                      .')')
            or trigger_error('SQL Error: '.mysql_error(), FATAL);
    // Notify the backend of the changes
        backend_notify_changes();
    // Delay a second so the scheduler can catch up
        sleep(1);
    }

/**
 * Revert a show to its default recording schedule settings
/**/
    function rec_default() {
        $schedule =& $GLOBALS['Schedules'][$this->recordid];
        if ($schedule && ($schedule->type == rectype_override || $schedule->type == rectype_dontrec))
            $schedule->delete();
    }

/**
 * Add an override or dontrec record to force this show to/not record pass in
 * rectype_dontrec or rectype_override constants
/**/
    function rec_override($rectype) {
        $schedule =& $GLOBALS['Schedules'][$this->recordid];
    // Unknown schedule?
        if (!$schedule)
            trigger_error('Unknown schedule for this program\'s recordid:  '.$this->recordid, FATAL);
    // Update the schedule with the new program info
        $schedule->chanid      = $this->chanid;
        $schedule->starttime   = $this->starttime;
        $schedule->endtime     = $this->endtime;
        $schedule->title       = $this->title;
        $schedule->subtitle    = $this->subtitle;
        $schedule->description = $this->description;
        $schedule->category    = $this->category;
        $schedule->station     = $this->channel->callsign;       // Note that "callsign" becomes "station"
        $schedule->seriesid    = $this->seriesid;
        $schedule->programid   = $this->programid;
        $schedule->search      = 0;
    // Save the schedule -- it'll know what to do about the override
        $schedule->save($rectype);
    }

/**
 * Intended to be called as program::category_types()
 *
 * @return array sorted list of category_type fields from the program table
/**/
    function category_types() {
        static $cache = array();
        if (empty($cache)) {
            global $db;
            $cache = $db->query_list('SELECT DISTINCT category_type
                                        FROM program
                                    ORDER BY category_type');
        }
        return $cache;
    }

/**
 * Intended to be called as program::categories()
 *
 * @return array sorted list of category fields from the program table
/**/
    function categories() {
        static $cache = array();
        if (empty($cache)) {
            global $db;
            $cache = $db->query_list('SELECT DISTINCT category
                                        FROM program
                                    ORDER BY category');
        }
        return $cache;
    }

}

