<?php
/***                                                                        ***\
    recordings.php                           Last Updated: 2005.01.21 (xris)

    The Recording object, and a couple of related subroutines.
\***                                                                        ***/

// Make sure the "Channels" class gets loaded   (yes, I know this is recursive, but require_once will handle things nicely)
    require_once 'includes/channels.php';

// Make sure the recording schedule type data gets loaded
    require_once 'includes/recording_schedules.php';

//
//  Recordings class
//
class Recording {
    var $recordid;
    var $type;
    var $chanid;
    var $starttime;
    var $endtime;
    var $title;
    var $subtitle;
    var $description;
    var $category;
    var $profile;
    var $recgroup;
    var $recpriority;
    var $autoexpire;
    var $maxepisodes;
    var $maxnewest;
    var $dupin;
    var $dupmethod;
    var $startoffset;
    var $endoffset;

    var $seriesid;
    var $programid;

    var $texttype;
    var $channel;

    var $will_record    = false;
    var $record_daily   = false;
    var $record_weekly  = false;
    var $record_once    = false;
    var $record_channel = false;
    var $record_always  = false;

    var $class;         // css class, based on category and/or category_type

    function Recording($recording_data) {

    // SQL data
        if (is_array($recording_data) && isset($recording_data['recordid'])) {
            $this->recordid    = $recording_data['recordid'];
            $this->type        = $recording_data['type'];
            $this->chanid      = $recording_data['chanid'];
            $this->starttime   = $recording_data['starttime_unix'];
            $this->endtime     = $recording_data['endtime_unix'];
            $this->title       = $recording_data['title'];
            $this->subtitle    = $recording_data['subtitle'];
            $this->description = $recording_data['description'];
            $this->category    = $recording_data['category'];
            $this->profile     = $recording_data['profile'];
            $this->recgroup    = $recording_data['recgroup'];
            $this->recpriority = $recording_data['recpriority'];
            $this->autoexpire  = $recording_data['autoexpire'];
            $this->maxepisodes = $recording_data['maxepisodes'];
            $this->maxnewest   = $recording_data['maxnewest'];
            $this->dupin       = $recording_data['dupin'];
            $this->dupmethod   = $recording_data['dupmethod'];
            $this->startoffset = $recording_data['startoffset'];
            $this->endoffset   = $recording_data['endoffset'];
            $this->seriesid    = $recording_data['seriesid'];
            $this->programid   = $recording_data['programid'];
        }
    // Recording object data
        else {
            $tmp = @get_object_vars($recording_data);
            if (count($tmp) > 0) {
                foreach ($tmp as $key => $value) {
                    $this->$key = $value;
                }
            }
        }

    // We get various recording-related information, too
        switch ($this->type) {
            case 1: $this->record_once    = true;  break;
            case 2: $this->record_daily   = true;  break;
            case 3: $this->record_channel = true;  break;
            case 4: $this->record_always  = true;  break;
            case 5: $this->record_weekly  = true;  break;
            case 6: $this->record_findone = true;  break;
        }

    // Add a generic "will record" variable, too
        $this->will_record = ($this->record_daily
                              || $this->record_weekly
                              || $this->record_once
                              || $this->record_findone
                              || $this->record_channel
                              || $this->record_always ) ? true : false;
    // Turn type int a word
        $this->texttype = $GLOBALS['RecTypes'][$this->type];
    // Do we have a chanid?  Load some info about it
        if ($this->chanid && !isset($this->channel)) {
        // No channel data?  Load it
            global $Channels;
            if (!is_array($Channels) || !count($Channels))
                load_all_channels($this->chanid);
        // Now we really should scan the $Channel array and add a link to this recording's channel
            foreach (array_keys($Channels) as $key) {
                if ($Channels[$key]->chanid == $this->chanid) {
                    $this->channel = &$Channels[$key];
                    break;
                }
            }
        }

    // Find out which css category this recording falls into
        if ($this->chanid != '')
            $this->class = category_class($this);
    }

}

?>
