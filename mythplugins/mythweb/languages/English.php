<?php
/***                                                                        ***\
    languages/English.php

    Translation hash for English.  This also doubles as the template for
    other translations, since it's mostly just blank (default) entries.
\***                                                                        ***/

// Set the locale to English UTF-8
setlocale(LC_ALL, 'en_US.UTF-8');

// Define the language lookup hash ** Do not touch the next line
$L = array(
// Add your translations below here.
// Warning, any custom comments will be lost during translation updates.
//
// Shared Terms
    'Category'         => '',
    'Description'      => '',
    'Original Airdate' => '',
    'Rerun'            => '',
    'Search'           => '',
    'Subtitle'         => '',
// includes/init.php
    'generic_date' => '%a %b %e, %Y',
    'generic_time' => '%I:%M %p',
// includes/utils.php
    '$1 B'    => '',
    '$1 GB'   => '',
    '$1 KB'   => '',
    '$1 MB'   => '',
    '$1 TB'   => '',
    '$1 hr'   => '',
    '$1 hrs'  => '',
    '$1 min'  => '',
    '$1 mins' => '',
// themes/.../channel_detail.php
    'Episode' => '',
    'Length'  => '',
    'Show'    => '',
    'Time'    => '',
// themes/.../program_detail.php
    'Auto-expire recordings'     => '',
    'Cancel this schedule'       => '',
    'Check for duplicates in'    => '',
    'Current Recordings'         => '',
    'Don\'t record this program' => '',
    'Duplicate Check method'     => '',
    'End Late'                   => '',
    'Google'                     => '',
    'IMDB'                       => '',
    'No. of recordings to keep'  => '',
    'None'                       => '',
    'Previous Recordings'        => '',
    'Record new and expire old'  => '',
    'Recording Group'            => '',
    'Recording Options'          => '',
    'Recording Priority'         => '',
    'Recording Profile'          => '',
    'Start Early'                => '',
    'Subtitle and Description'   => '',
    'TVTome'                     => '',
    'Update Recording Settings'  => '',
// themes/.../program_listing.php
    'Airtime'                 => '',
    'Currently Browsing:  $1' => '',
    'Date'                    => '',
    'Hour'                    => '',
    'Jump'                    => '',
    'Jump To'                 => '',
    'Notes'                   => '',
    'Rating'                  => '',
    'Schedule'                => '',
    'Title'                   => '',
// themes/.../recorded_programs.php
    '$1 episode'                                          => '',
    '$1 episodes'                                         => '',
    '$1 programs, using $2 ($3) out of $4.'               => '',
    '$1 recording'                                        => '',
    '$1 recordings'                                       => '',
    'All recordings'                                      => '',
    'Are you sure you want to delete the following show?' => '',
    'Delete'                                              => '',
    'Go'                                                  => '',
    'No'                                                  => '',
    'Show group'                                          => '',
    'Show recordings'                                     => '',
    'Yes'                                                 => '',
    'preview'                                             => '',
// themes/.../theme.php
    'Backend Status'       => '',
    'Category Legend'      => '',
    'Favorites'            => '',
    'Go To'                => '',
    'Listings'             => '',
    'Manually Schedule'    => '',
    'Movies'               => '',
    'Recorded Programs'    => '',
    'Recording Schedules'  => '',
    'Scheduled Recordings' => '',
    'Settings'             => '',
    'advanced'             => ''
// End of the translation hash ** Do not touch the next line
          );


/* theme.php */
define ('_LANG_CATEGORY_LEGEND',      'Category Legend');
define ('_LANG_ACTION',               'Action');
define ('_LANG_ADULT',                'Adult');
define ('_LANG_ANIMALS',              'Animals');
define ('_LANG_ART_MUSIC',            'Art_Music');
define ('_LANG_BUSINESS',             'Business');
define ('_LANG_CHILDREN',             'Children');
define ('_LANG_COMEDY',               'Comedy');
define ('_LANG_CRIME_MYSTERY',        'Crime_Mystery');
define ('_LANG_DOCUMENTARY',          'Documentary');
define ('_LANG_DRAMA',                'Drama');
define ('_LANG_EDUCATIONAL',          'Educational');
define ('_LANG_FOOD',                 'Food');
define ('_LANG_GAME',                 'Game');
define ('_LANG_HEALTH_MEDICAL',       'Health_Medical');
define ('_LANG_HISTORY',              'History');
define ('_LANG_HOWTO',                'HowTo');
define ('_LANG_HORROR',               'Horror');
define ('_LANG_MISC',                 'Misc');
define ('_LANG_NEWS',                 'News');
define ('_LANG_REALITY',              'Reality');
define ('_LANG_ROMANCE',              'Romance');
define ('_LANG_SCIENCE_NATURE',       'Science_Nature');
define ('_LANG_SCIFI_FANTASY',        'SciFi_Fantasy');
define ('_LANG_SHOPPING',             'Shopping');
define ('_LANG_SOAPS',                'Soaps');
define ('_LANG_SPIRITUAL',            'Spiritual');
define ('_LANG_SPORTS',               'Sports');
define ('_LANG_TALK',                 'Talk');
define ('_LANG_TRAVEL',               'Travel');
define ('_LANG_WAR',                  'War');
define ('_LANG_WESTERN',              'Western');
define ('_LANG_MOVIES',               'Movies');
define ('_LANG_UNKNOWN',              'Unknown');

define ('_CATMATCH_ACTION',               '\\b(?:action|adven)');
define ('_CATMATCH_ADULT',                '\\b(?:adult|erot)');
define ('_CATMATCH_ANIMALS',              '\\b(?:animal|tiere)');
define ('_CATMATCH_ART_MUSIC',            '\\b(?:art|dance|musi[ck]|kunst|[ck]ultur)');
define ('_CATMATCH_BUSINESS',             '\\b(?:biz|busine)');
define ('_CATMATCH_CHILDREN',             '\\b(?:child|kin?d|infan|animation)');
define ('_CATMATCH_COMEDY',               '\\b(?:comed|entertain|sitcom)');
define ('_CATMATCH_CRIME_MYSTERY',        '\\b(?:[ck]rim|myster)');
define ('_CATMATCH_DOCUMENTARY',          '\\b(?:do[ck])');
define ('_CATMATCH_DRAMA',                '\\b(?:drama)');
define ('_CATMATCH_EDUCATIONAL',          '\\b(?:edu|bildung|interests)');
define ('_CATMATCH_FOOD',                 '\\b(?:food|cook|essen|[dt]rink)');
define ('_CATMATCH_GAME',                 '\\b(?:game|spiele)');
define ('_CATMATCH_HEALTH_MEDICAL',       '\\b(?:health|medic|gesundheit)');
define ('_CATMATCH_HISTORY',              '\\b(?:hist|geschichte)');
define ('_CATMATCH_HOWTO',                '\\b(?:how|home|house|garden)');
define ('_CATMATCH_HORROR',               '\\b(?:horror)');
define ('_CATMATCH_MISC',                 '\\b(?:special|variety|info|collect)');
define ('_CATMATCH_NEWS',                 '\\b(?:news|nachrichten|current)');
define ('_CATMATCH_REALITY',              '\\b(?:reality)');
define ('_CATMATCH_ROMANCE',              '\\b(?:romance|lieb)');
define ('_CATMATCH_SCIENCE_NATURE',       '\\b(?:science|nature|environment|wissenschaft)');
define ('_CATMATCH_SCIFI_FANTASY',        '\\b(?:fantasy|sci\\w*\\W*fi)');
define ('_CATMATCH_SHOPPING',             '\\b(?:shop)');
define ('_CATMATCH_SOAPS',                '\\b(?:soaps)');
define ('_CATMATCH_SPIRITUAL',            '\\b(?:spirit|relig)');
define ('_CATMATCH_SPORTS',               '\\b(?:sport|deportes|futbol)');
define ('_CATMATCH_TALK',                 '\\b(?:talk)');
define ('_CATMATCH_TRAVEL',               '\\b(?:travel|reisen)');
define ('_CATMATCH_WAR',                  '\\b(?:war|krieg)');
define ('_CATMATCH_WESTERN',              '\\b(?:west)');
define ('_CATMATCH_MOVIES',               '');

/* settings.php */
define ('_LANG_SETTINGS_HEADER1',              'This is the index page for the configuration settings...');
define ('_LANG_SETTINGS_HEADER2',              'It should get some nifty images to link to the various sections, but for now, we get:');
define ('_LANG_CHANNELS',                      'Channels');
define ('_LANG_THEME',                         'Theme');
define ('_LANG_LANGUAGE',                      'Language');
define ('_LANG_DATEFORMATS',                   'Date/Date Formats');
define ('_LANG_KEY_BINDINGS',                  'Key Bindings');
define ('_LANG_CONFIGURE',                     'Configure');
define ('_LANG_GO_TO',                         'Go To');
define ('_LANG_ADVANCED',                      'advanced');
define ('_LANG_FORMAT_HELP',                   'format help');
define ('_LANG_STATUS_BAR',                    'Status Bar');
define ('_LANG_SCHEDULED_RECORDINGS',          'Scheduled Recordings');
define ('_LANG_SCHEDULED_POPUP',               'Scheduled Popup');
define ('_LANG_RECORDED_PROGRAMS',             'Recorded Programs');
define ('_LANG_SEARCH_RESULTS',                'Search Results');
define ('_LANG_LISTING_TIME_KEY',              'Listing Time Key');
define ('_LANG_LISTING_JUMP_TO',               'Listing &quot;Jump to&quot;');
define ('_LANG_CHANNEL_JUMP_TO',               'Channel &quot;Jump to&quot;');
define ('_LANG_HOUR_FORMAT',                   'Hour Format');
define ('_LANG_RESET',                         'Reset');
define ('_LANG_SAVE',                          'Save');
define ('_LANG_SHOW_DESCRIPTIONS_ON_NEW_LINE', 'Show descriptions on new line');

/* program_listings.php */
define ('_LANG_CURRENTLY_BROWSING', 'Currently Browsing:');
define ('_LANG_JUMP_TO',            'Jump&nbsp;to');
define ('_LANG_HOUR',               'Hour');
define ('_LANG_DATE',               'Date');
define ('_LANG_JUMP',               'Jump');

/* program_detail.php */
define ('_LANG_SEARCH',                             'Search');
define ('_LANG_IMDB',                               'IMDB');
define ('_LANG_GOOGLE',                             'Google');
define ('_LANG_TVTOME',                             'TV Tome');
define ('_LANG_MINUTES',                            'minutes');
define ('_LANG_TO',                                 'to');
define ('_LANG_CATEGORY',                           'Category');
define ('_LANG_ORIG_AIRDATE',                       'Orig. Airdate');
define ('_LANG_AIRDATE',                            'Airdate');
define ('_LANG_RECGROUP',                           'Group');
define ('_LANG_RECORDING_OPTIONS',                  'Recording Options');
define ('_LANG_DONT_RECORD_THIS_PROGRAM',           'Don\'t record this program.');
define ('_LANG_CANCEL_THIS_SCHEDULE',               'Cancel this schedule.');
define ('_LANG_RECORDING_PROFILE',                  'Recording Profile');
define ('_LANG_RECORDING_GROUP',                    'Recording Group');
define ('_LANG_RECPRIORITY',                        'Recpriority');
define ('_LANG_CHECK_FOR_DUPLICATES_IN',            'Check For Duplicates In');
define ('_LANG_CURRENT_RECORDINGS',                 'Current Recordings');
define ('_LANG_PREVIOUS_RECORDINGS',                'Previous Recordings');
define ('_LANG_ALL_RECORDINGS',                     'All Recordings');
define ('_LANG_DUPLICATE_CHECK_METHOD',             'Duplicate Check Method');
define ('_LANG_NONE',                               'None');
define ('_LANG_SUBTITLE',                           'Subtitle');
define ('_LANG_DESCRIPTION',                        'Description');
define ('_LANG_SUBTITLE_AND_DESCRIPTION',           'Subtitle & Description');
define ('_LANG_SUB_AND_DESC',                       'Sub & Desc (Empty matches)');
define ('_LANG_AUTO_EXPIRE_RECORDINGS',             'Auto-expire Recordings?');
define ('_LANG_NO_OF_RECORDINGS_TO_KEEP',           'No of recordings to keep?');
define ('_LANG_RECORD_NEW_AND_EXPIRE_OLD',          'Record new and expire old?');
define ('_LANG_START_EARLY',                        'Start Early (minutes)');
define ('_LANG_END_LATE',                           'End Late (minutes)');
define ('_LANG_UPDATE_RECORDING_SETTINGS',          'Update Recording Settings');
define ('_LANG_WHAT_ELSE_IS_ON_AT_THIS_TIME',       'What else is on at this time?');
define ('_LANG_BACK_TO_THE_PROGRAM_LISTING',        'Back to the program listing!');
define ('_LANG_FIND_OTHER_SHOWING_OF_THIS_PROGRAM', 'Find other showing of this program');
define ('_LANG_BACK_TO_RECORDING_SCHEDULES',        'Back to Recording Schedules');

/* scheduled_recordings.php */
/* recording_schedules_php */
/* search.php */
define ('_LANG_NO_MATCHES_FOUND', 'No matches found');
define ('_LANG_SEARCH',           'Search');
define ('_LANG_TITLE',            'Program');
define ('_LANG_SUBTITLE',         'Eepisode');
define ('_LANG_CATEGORY_TYPE',    'category&nbsp;type');
define ('_LANG_EXACT_MATCH',      'exact&nbsp;match');
define ('_LANG_CHANNUM',          'station');
define ('_LANG_LENGTH',           'length');
define ('_LANG_COMMANDS',         'commands');
define ('_LANG_DONT_RECORD',      'Don\'t Record');
define ('_LANG_ACTIVATE',         'Activate');
define ('_LANG_NEVER_RECORD',     'Never&nbsp;Record');
define ('_LANG_RECORD_THIS',      'Record This');
define ('_LANG_FORGET_OLD',       'Forget Old');
define ('_LANG_DEFAULT',          'Default');
define ('_LANG_RATING',           'Rating');
define ('_LANG_SCHEDULE',         'Schedule');
define ('_LANG_DISPLAY',          'Display');
define ('_LANG_SCHEDULED',        'Scheduled');
define ('_LANG_DUPLICATES',       'Duplicates');
define ('_LANG_DEACTIVATED',      'Deactivated');
define ('_LANG_CONFLICTS',        'Conflicts');
define ('_LANG_TYPE',             'type');
define ('_LANG_AIRTIME',          'Airtime');
define ('_LANG_RERUN',            'Rerun');
define ('_LANG_SCHEDULE',         'Schedule');
define ('_LANG_PROFILE',          'Profile');
define ('_LANG_NOTES',            'Notes');
define ('_LANG_DUP_METHOD',       'Dup Method');
define ('_LANG_ANY',              'Any');

/* recorded_programs.php */
define ('_LANG_RECORDING', 'Recording');
define ('_LANG_RECORDINGS', 'Recordings');
define ('_LANG_SHOW_GROUP', 'Show Group');
define ('_LANG_CONFIRM_DELETE',  'Are you sure you want to delete the following show?');
define ('_LANG_GO',              'Go');
define ('_LANG_PREVIEW',         'preview');
define ('_LANG_FILE_SIZE',       'file&nbsp;size');
define ('_LANG_DELETE',          'Delete');
define ('_LANG_PROGRAMS_USING',  'programs, using ');
define ('_LANG_OUT_OF',          ' out of ');
define ('_LANG_SHOW_HAS_COMMFLAG',   'flagged commercials');
define ('_LANG_SHOW_HAS_CUTLIST',    'has cutlist');
define ('_LANG_SHOW_IS_EDITING',     'being edited');
define ('_LANG_SHOW_AUTO_EXPIRE',    'auto expire');
define ('_LANG_SHOW_HAS_BOOKMARK',   'has bookmark');
define ('_LANG_YES',                 'Yes');
define ('_LANG_NO',                  'No');

/* recordings.php */
define ('_LANG_RECTYPE_ONCE',    'Once');
define ('_LANG_RECTYPE_DAILY',   'Daily');
define ('_LANG_RECTYPE_CHANNEL', 'Channel');
define ('_LANG_RECTYPE_ALWAYS',  'Always');
define ('_LANG_RECTYPE_WEEKLY',  'Weekly');
define ('_LANG_RECTYPE_FINDONE', 'FindOne');
define ('_LANG_RECTYPE_OVERRIDE', 'Override (record)');
define ('_LANG_RECTYPE_DONTREC', 'Do Not Record');

define ('_LANG_RECTYPE_LONG_ONCE',          'Record only this showing.');
define ('_LANG_RECTYPE_LONG_DAILY',         'Record this program in this timeslot every day.');
define ('_LANG_RECTYPE_LONG_CHANNEL',       'Always record this program on this channel ');
define ('_LANG_RECTYPE_LONG_ALWAYS',        'Always record this program on any channel.');
define ('_LANG_RECTYPE_LONG_WEEKLY',        'Record this program in this timeslot every week.');
define ('_LANG_RECTYPE_LONG_FINDONE',       'Find a showing of this program and record it.');

define ('_LANG_RECSTATUS_LONG_DELETED',           'This showing was recorded but was deleted before recording was completed.');
define ('_LANG_RECSTATUS_LONG_STOPPED',           'This showing was recorded but was stopped before recording was completed.');
define ('_LANG_RECSTATUS_LONG_RECORDED',          'This showing was recorded.');
define ('_LANG_RECSTATUS_LONG_RECORDING',         'This showing is being recorded.');
define ('_LANG_RECSTATUS_LONG_WILLRECORD',        'This showing will be recorded.');
define ('_LANG_RECSTATUS_LONG_UNKNOWN',           'The status of this showing is unknown.');
define ('_LANG_RECSTATUS_LONG_MANUALOVERRIDE',    'This was manually set to not record');
define ('_LANG_RECSTATUS_LONG_PREVIOUSRECORDING', 'This episode was previously recorded according to the duplicate policy chosen for this title.');
define ('_LANG_RECSTATUS_LONG_CURRENTRECORDING',  'This episode was previously recorded and is still available in the list of recordings.');
define ('_LANG_RECSTATUS_LONG_EARLIERSHOWING',    'This episode will be recorded at an earlier time instead.');
define ('_LANG_RECSTATUS_LONG_LATERSHOWING',      'This episode will be recorded at a later time instead.');
define ('_LANG_RECSTATUS_LONG_TOOMANYRECORDINGS', 'Too many recordings of this program have already been recorded.');
define ('_LANG_RECSTATUS_LONG_CANCELLED',         'This was scheduled to be recorded but was manually canceled.');
define ('_LANG_RECSTATUS_LONG_CONFLICT',          'Another program with a higher recording priority will be recorded.');
define ('_LANG_RECSTATUS_LONG_OVERLAP',           'This is covered by another scheduled recording for the same program.');
define ('_LANG_RECSTATUS_LONG_LOWDISKSPACE',      'There wasn\'t enough disk space available to record this program.');
define ('_LANG_RECSTATUS_LONG_TUNERBUSY',         'The tuner card was already being used when this program was scheduled to be recorded.');
define ('_LANG_RECSTATUS_LONG_FORCE_RECORD',      'This show was manually set to record this specific instance.');

/* weather.php */
define ('_LANG_HUMIDITY',		'Humidity');
define ('_LANG_PRESSURE',		'Pressure');
define ('_LANG_WIND',			'Wind');
define ('_LANG_VISIBILITY',		'Visibility');
define ('_LANG_WIND_CHILL',		'Wind Chill');
define ('_LANG_UV_INDEX',		'UV Index');
define ('_LANG_UV_MINIMAL',		'minimal');
define ('_LANG_UV_MODERATE',		'moderate');
define ('_LANG_UV_HIGH',		'high');
define ('_LANG_UV_EXTREME',		'extreme');
define ('_LANG_CURRENT_CONDITIONS',	'Current Conditions');
define ('_LANG_FORECAST',		'Forecast');
define ('_LANG_LAST_UPDATED',		'Last Updated');
define ('_LANG_HIGH',			'High');
define ('_LANG_LOW',			'Low');
define ('_LANG_UNKNOWN',		'Unknown');
define ('_LANG_RADAR',			'Radar');
define ('_LANG_AT',			'at');

define ('_LANG_TODAY',			'Today');
define ('_LANG_TOMORROW',		'Tomorrow');
define ('_LANG_MONDAY',			'Monday');
define ('_LANG_TUESDAY',		'Tuesday');
define ('_LANG_WEDNESDAY',		'Wednesday');
define ('_LANG_THURSDAY',		'Thursday');
define ('_LANG_FRIDAY',			'Friday');
define ('_LANG_SATURDAY',		'Saturday');
define ('_LANG_SUNDAY',			'Sunday');

/* utils.php */
define ('_LANG_HR',              'hr');
define ('_LANG_HRS',             'hrs');
define ('_LANG_MINS',            'mins');

/*
define ('_LANG_', '');
define ('_LANG_', '');
define ('_LANG_', '');
*/

?>
