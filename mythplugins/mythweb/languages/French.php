<?php
/***                                                                        ***\
    languages/French.php

    Translation hash for French.
\***                                                                        ***/

// Set locale to French
setlocale(LC_ALL, 'fr_FR');

// Define the language lookup hash ** Do not touch the next line
$L = array(
// Add your translations below here.
// Warning, any custom comments will be lost during translation updates.
//
// Shared Terms
    '$1 min'                    => '',
    '$1 mins'                   => '',
    'Airtime'                   => '',
    'All recordings'            => '',
    'Auto-expire recordings'    => '',
    'Category'                  => '',
    'Check for duplicates in'   => '',
    'Current recordings'        => '',
    'Date'                      => '',
    'Description'               => '',
    'Duplicate Check method'    => '',
    'End Late'                  => '',
    'Go'                        => '',
    'No. of recordings to keep' => '',
    'None'                      => '',
    'Notes'                     => '',
    'Original Airdate'          => '',
    'Previous recordings'       => '',
    'Profile'                   => '',
    'Rating'                    => '',
    'Record new and expire old' => '',
    'Recorded Programs'         => '',
    'Recording Group'           => '',
    'Recording Options'         => '',
    'Recording Priority'        => '',
    'Recording Profile'         => '',
    'Rerun'                     => '',
    'Schedule'                  => '',
    'Scheduled Recordings'      => '',
    'Search'                    => '',
    'Start Early'               => '',
    'Subtitle'                  => '',
    'Subtitle and Description'  => '',
    'Title'                     => '',
    'Unknown'                   => '',
    'Update Recording Settings' => '',
    'airdate'                   => '',
    'channum'                   => '',
    'description'               => '',
    'length'                    => '',
    'recgroup'                  => '',
    'rectype-long: always'      => '',
    'rectype-long: channel'     => '',
    'rectype-long: daily'       => '',
    'rectype-long: findone'     => '',
    'rectype-long: once'        => '',
    'rectype-long: weekly'      => '',
    'subtitle'                  => '',
    'title'                     => '',
// includes/init.php
    'generic_date' => '%e %b, %Y',
    'generic_time' => '%I:%M %p',
// includes/programs.php
    'recstatus: cancelled'         => '',
    'recstatus: conflict'          => '',
    'recstatus: currentrecording'  => '',
    'recstatus: deleted'           => '',
    'recstatus: earliershowing'    => '',
    'recstatus: force_record'      => '',
    'recstatus: latershowing'      => '',
    'recstatus: lowdiskspace'      => '',
    'recstatus: manualoverride'    => '',
    'recstatus: overlap'           => '',
    'recstatus: previousrecording' => '',
    'recstatus: recorded'          => '',
    'recstatus: recording'         => '',
    'recstatus: stopped'           => '',
    'recstatus: toomanyrecordings' => '',
    'recstatus: tunerbusy'         => '',
    'recstatus: unknown'           => '',
    'recstatus: willrecord'        => '',
// includes/recordings.php
    'rectype: always'   => '',
    'rectype: channel'  => '',
    'rectype: daily'    => '',
    'rectype: dontrec'  => '',
    'rectype: findone'  => '',
    'rectype: once'     => '',
    'rectype: override' => '',
    'rectype: weekly'   => '',
// includes/utils.php
    '$1 B'   => '',
    '$1 GB'  => '',
    '$1 KB'  => '',
    '$1 MB'  => '',
    '$1 TB'  => '',
    '$1 hr'  => '',
    '$1 hrs' => '',
// themes/.../channel_detail.php
    'Episode' => '',
    'Jump to' => '',
    'Length'  => '',
    'Show'    => '',
    'Time'    => '',
// themes/.../program_detail.php
    '$1 to $2'                            => '',
    'Back to the program listing'         => '',
    'Back to the recording schedules'     => '',
    'Cancel this schedule'                => '',
    'Don\'t record this program'          => '',
    'Find other showings of this program' => '',
    'Google'                              => '',
    'IMDB'                                => '',
    'TVTome'                              => '',
    'What else is on at this time?'       => '',
// themes/.../program_listing.php
    'Currently Browsing:  $1' => '',
    'Hour'                    => '',
    'Jump'                    => '',
    'Jump To'                 => '',
// themes/.../recorded_programs.php
    '$1 episode'                                          => '',
    '$1 episodes'                                         => '',
    '$1 programs, using $2 ($3) out of $4.'               => '',
    '$1 recording'                                        => '',
    '$1 recordings'                                       => '',
    'Are you sure you want to delete the following show?' => '',
    'Delete'                                              => '',
    'No'                                                  => '',
    'Show group'                                          => '',
    'Show recordings'                                     => '',
    'Yes'                                                 => '',
    'auto-expire'                                         => '',
    'file_size'                                           => '',
    'has bookmark'                                        => '',
    'has commflag'                                        => '',
    'has cutlist'                                         => '',
    'is editing'                                          => '',
    'preview'                                             => '',
// themes/.../recording_profiles.php
    'Profile Groups'     => '',
    'Recording profiles' => '',
// themes/.../recording_schedules.php
    'Any'                          => '',
    'Dup Method'                   => '',
    'Sub and Desc (Empty matches)' => '',
    'Type'                         => '',
    'profile'                      => '',
    'type'                         => '',
// themes/.../schedule_manually.php
    'Channel'      => '',
    'Length (min)' => '',
    'Start Date'   => '',
    'Start Time'   => '',
// themes/.../scheduled_recordings.php
    'Activate'      => '',
    'Commands'      => '',
    'Conflicts'     => '',
    'Deactivated'   => '',
    'Default'       => '',
    'Display'       => '',
    'Don\'t Record' => '',
    'Duplicates'    => '',
    'Forget Old'    => '',
    'Never Record'  => '',
    'Record This'   => '',
    'Scheduled'     => '',
// themes/.../search.php
    'Category Type'    => '',
    'Exact Match'      => '',
    'No matches found' => '',
// themes/.../settings.php
    'Channels'           => '',
    'Configure'          => '',
    'Key Bindings'       => '',
    'MythWeb Settings'   => '',
    'settings: overview' => '',
// themes/.../settings_channels.php
    'Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.' => '',
// themes/.../settings_keys.php
    'Edit keybindings on' => '',
// themes/.../settings_mythweb.php
    'Channel &quot;Jump to&quot;'   => '',
    'Date Formats'                  => '',
    'Hour Format'                   => '',
    'Language'                      => '',
    'Listing &quot;Jump to&quot;'   => '',
    'Listing Time Key'              => '',
    'MythWeb Theme'                 => '',
    'Reset'                         => '',
    'Save'                          => '',
    'Scheduled Popup'               => '',
    'Search Results'                => '',
    'Show descriptions on new line' => '',
    'Status Bar'                    => '',
    'format help'                   => '',
// themes/.../theme.php
    'Backend Status'      => '',
    'Category Legend'     => '',
    'Favorites'           => '',
    'Go To'               => '',
    'Listings'            => '',
    'Manually Schedule'   => '',
    'Movies'              => '',
    'Recording Schedules' => '',
    'Settings'            => '',
    'advanced'            => '',
// themes/.../weather.php
    ' at '               => '',
    'Current Conditions' => '',
    'Forecast'           => '',
    'Friday'             => '',
    'High'               => '',
    'Humidity'           => '',
    'Last Updated'       => '',
    'Low'                => '',
    'Monday'             => '',
    'Pressure'           => '',
    'Radar'              => '',
    'Saturday'           => '',
    'Sunday'             => '',
    'Thursday'           => '',
    'Today'              => '',
    'Tomorrow'           => '',
    'Tuesday'            => '',
    'UV Extreme'         => '',
    'UV High'            => '',
    'UV Index'           => '',
    'UV Minimal'         => '',
    'UV Moderate'        => '',
    'Visibility'         => '',
    'Wednesday'          => '',
    'Wind'               => '',
    'Wind Chill'         => ''
// End of the translation hash ** Do not touch the next line
          );


/*
    Show Categories:
    $Categories is a hash of keys corresponding to the css style used for each
    show category.  Each entry is an array containing the name of that category
    in the language this file defines (it will not be translated separately),
    and a regular expression pattern used to match the category against those
    provided in the listings.
*/
$Categories = array();
$Categories['Action']         = array('Action',                                   '\\b(?:action|adven)');
$Categories['Adult']          = array('Adulte',                                   '\\b(?:adult|erot|sex)');
$Categories['Animals']        = array('Animals',                                  '\\b(?:animal|tiere)');
$Categories['Art_Music']      = array('Musique',                                  '\\b(?:art|dance|musi[ck]|spectacle|musique|kunst|[ck]ultur|culture)');
$Categories['Business']       = array('Divertissement',                           '\\b(?:divertissement)');
$Categories['Children']       = array('Jeunesse',                                 '\\b(?:child|kin?d|infan|jeunesse|animation)');
$Categories['Comedy']         = array('Spectacle',                                '\\b(?:comed|entertain|spectacle|sitcom)');
$Categories['Crime_Mystery']  = array('Surprise',                                 '\\b(?:[ck]rim|myster|surprise)');
$Categories['Documentary']    = array('Documentaire',                             '\\b(?:do[ck])|mag');
$Categories['Drama']          = array('Court-m&eacute;trage',                     '\\b(?:court)');
$Categories['Educational']    = array('Educatif',                                 '\\b(?:cours|edu|bildung|interests)');
$Categories['Food']           = array('Cuisine',                                  '\\b(?:food|cook|essen|gastro|cuisine|[dt]rink)');
$Categories['Game']           = array('Jeu',                                      '\\b(?:game|spiele|jeu)');
$Categories['Health_Medical'] = array('Sant&eacute;',                             '\\b(?:health|medic|gesundheit|sant)');
$Categories['History']        = array('Magazine',                                 '\\b(?:hist|geschichte)');
$Categories['Horror']         = array('Horreur',                                  '\\b(?:horreur)');
$Categories['HowTo']          = array('Th&eacute;matique',                        '\\b(?:th.*matique)');
$Categories['Misc']           = array('Divers',                                   '\\b(?:special|variety|collect)');
$Categories['News']           = array('Information',                              '\\b(?:news|nachrichten|info|current)');
$Categories['Reality']        = array('T&eacute;l&eacute;-r&eacute;alit&eacute;', '\\b(?:reality|realit.*)');
$Categories['Romance']        = array('T&eacute;l&eacute;film',                   '\\b(?:t.*l.*film|romance|lieb)');
$Categories['SciFi_Fantasy']  = array('Nature',                                   '\\b(?:science|nature|environment|wissenschaft)');
$Categories['Science_Nature'] = array('Fantastique',                              '\\b(?:fantasy|fantastique|sci\\w*\\W*fi)');
$Categories['Shopping']       = array('T&eacute;l&eacute;-Shopping',              '\\b(?:shop)');
$Categories['Soaps']          = array('S&eacute;rie',                             '\\b(?:s.*rie|soap|t.*l.*film|feuilleton)');
$Categories['Spiritual']      = array('Spirituel',                                '\\b(?:spirit|relig)');
$Categories['Sports']         = array('Sport',                                    '\\b(?:sport|foot|deportes|futbol)');
$Categories['Talk']           = array('D&eacute;bat',                             '\\b(?:talk|D.*bat)');
$Categories['Travel']         = array('Voyage',                                   '\\b(?:travel|reisen|voyage)');
$Categories['War']            = array('Guerre',                                   '\\b(?:war|krieg|guerre)');
$Categories['Western']        = array('Western',                                  '\\b(?:west)');

// These are some other classes that we might want to have show up in the
//   category legend (they don't need regular expressions)
$Categories['Unknown']        = array('Inconnu');
$Categories['movie']          = array('Film'  );

?>
