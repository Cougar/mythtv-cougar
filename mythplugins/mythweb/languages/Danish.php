<?php
/***                                                                        ***\
    languages/English.php

    Translation hash for Danish.
\***                                                                        ***/

// Set the locale to Danish UTF-8
setlocale(LC_ALL, 'da_DK.UTF-8');

// Define the language lookup hash ** Do not touch the next line
$L = array(
// Add your translations below here.
// Warning, any custom comments will be lost during translation updates.
//
// Shared Terms
    '$1 Search'                                          => '',
    '$1 hr'                                              => '$1 t',
    '$1 hrs'                                             => '$1 t',
    '$1 min'                                             => '$1 minut',
    '$1 mins'                                            => '$1 minutter',
    '$1 programs, using $2 ($3) out of $4 ($5 free).'    => '$1 programmer, bruger $2 ($3) ud af $4',
    '$1 to $2'                                           => '$1 til $2',
    'Activate'                                           => 'Aktiv�r',
    'Advanced Options'                                   => 'Avancerede indstillinger',
    'Airtime'                                            => 'Visningsdato',
    'All recordings'                                     => 'Alle optagelser',
    'Auto-expire recordings'                             => 'Auto-udl�b optagelser',
    'Auto-flag commercials'                              => 'Marker automatisk reklamer',
    'Auto-transcode'                                     => 'Auto-Transkode',
    'Backend Logs'                                       => '',
    'Backend Status'                                     => 'System status',
    'Cancel this schedule.'                              => 'Annuller denne optagelse',
    'Category'                                           => 'Kategori',
    'Check for duplicates in'                            => 'Kontroller for dubletter i',
    'Create Schedule'                                    => 'Planl�g',
    'Current recordings'                                 => 'Nuv�rende optagelser',
    'Currently Browsing:  $1'                            => 'Lige nu vises: $1',
    'Custom Schedule'                                    => '',
    'Date'                                               => 'Dato',
    'Default'                                            => 'Standard',
    'Description'                                        => 'Beskrivelse',
    'Details for'                                        => 'Detaljer for',
    'Display'                                            => 'Vis',
    'Don\'t Record'                                      => 'Optag ikke',
    'Duplicate Check method'                             => 'dubletkontrol metode',
    'End Late'                                           => 'Slut senere (min)',
    'Episode'                                            => 'Afsnit',
    'Forget Old'                                         => 'Glem gamle',
    'Friday'                                             => 'Fredag',
    'Hour'                                               => 'Time',
    'IMDB'                                               => 'IMDB',
    'Inactive'                                           => 'Ikke aktiv',
    'Jump'                                               => 'G�',
    'Jump to'                                            => 'G� til',
    'Keyword'                                            => '',
    'Listings'                                           => 'Programoversigt',
    'Monday'                                             => 'Mandag',
    'Music'                                              => 'Musik',
    'Never Record'                                       => 'Optag aldrig',
    'No'                                                 => 'Nej',
    'No. of recordings to keep'                          => 'Gem antal optagelser',
    'None'                                               => 'Ingen',
    'Only New Episodes'                                  => 'Kun Nye Afsnit',
    'Original Airdate'                                   => 'Sendt f�rst',
    'People'                                             => '',
    'Power'                                              => '',
    'Previous recordings'                                => 'Tidligere optagelser',
    'Program Listing'                                    => 'Program lister',
    'Rating'                                             => 'Karakterer',
    'Record This'                                        => 'Optag dette',
    'Record new and expire old'                          => 'Optag nye og udl�b gamle',
    'Recorded Programs'                                  => 'Optagede programmer',
    'Recording Group'                                    => 'Optagelsesgruppe',
    'Recording Options'                                  => 'Optageindstillinger',
    'Recording Priority'                                 => 'Optageprioritet',
    'Recording Profile'                                  => 'Optageprofil',
    'Recording Schedules'                                => 'Optagelsesplanl�gning',
    'Repeat'                                             => 'Genudsendelse',
    'Saturday'                                           => 'L�rdag',
    'Save'                                               => 'Gem',
    'Save Schedule'                                      => '',
    'Schedule'                                           => 'Planl�g',
    'Schedule Manually'                                  => '',
    'Schedule Options'                                   => 'Planl�gningsindstillinger',
    'Schedule Override'                                  => 'Planl�g manuelt',
    'Schedule normally.'                                 => 'Planl�g normalt',
    'Search'                                             => 'S�g',
    'Search Results'                                     => 'S�geresultater',
    'Settings'                                           => 'Indstillinger',
    'Start Early'                                        => 'Start tidligere (min)',
    'Subtitle'                                           => 'Undertitel',
    'Subtitle and Description'                           => 'Undertitel og beskrivelse',
    'Sunday'                                             => 'S�ndag',
    'The requested recording schedule has been deleted.' => 'Den planlagte optagelse er blevet slettet',
    'Thursday'                                           => 'Torsdag',
    'Title'                                              => 'Titel',
    'Transcoder'                                         => 'Trankodning',
    'Tuesday'                                            => 'Tirsdag',
    'Type'                                               => 'Type',
    'Unknown'                                            => 'Ukendt',
    'Upcoming Recordings'                                => 'Fremtidige optagelser',
    'Update'                                             => 'Opdat�r',
    'Update Recording Settings'                          => 'Opdat�r optagelsesindstillinger',
    'Weather'                                            => 'Vejret',
    'Wednesday'                                          => 'Onsdag',
    'Yes'                                                => 'Ja',
    'airdate'                                            => 'visningsdato',
    'channum'                                            => 'kanalnummer',
    'description'                                        => 'beskrivelse',
    'generic_date'                                       => '%a %e %b %Y',
    'generic_time'                                       => '%H:%M',
    'length'                                             => 'l�ngde',
    'minutes'                                            => 'minutter',
    'recgroup'                                           => 'Optagelsesgrupper',
    'recpriority'                                        => 'Optagelsesprioritet',
    'rectype-long: always'                               => 'Optag n�r som helst p� alle kanaler.',
    'rectype-long: channel'                              => 'Optag n�r som helst p� kanalen $1.',
    'rectype-long: daily'                                => 'Optag dette program p� dette tidspunkt hver dag.',
    'rectype-long: dontrec'                              => 'Optag ikke denne specifikke visning.',
    'rectype-long: finddaily'                            => 'Find og optag en visning af dette program hver dag.',
    'rectype-long: findone'                              => 'Find og optag en visning af dette program.',
    'rectype-long: findweekly'                           => 'Find og optag en visning af dette program hver uge.',
    'rectype-long: once'                                 => 'Optag kun denne visning.',
    'rectype-long: override'                             => 'Optag denne specifikke visning.',
    'rectype-long: weekly'                               => 'Optag dette program p� dette tidspunkt hver uge.',
    'rectype: always'                                    => 'Altid',
    'rectype: channel'                                   => 'Kanal',
    'rectype: daily'                                     => 'Dagligt',
    'rectype: dontrec'                                   => 'Optag ikke',
    'rectype: findone'                                   => 'Find en',
    'rectype: once'                                      => 'en enkelt gang',
    'rectype: override'                                  => 'Override (record)',
    'rectype: weekly'                                    => 'Ugentligt',
    'subtitle'                                           => 'undertitel',
    'title'                                              => 'titel',
// config/canned_searches.php
    'All HDTV'                           => '',
    'Movies'                             => '',
    'Movies, 3&frac12; Stars or more'    => '',
    'Movies, Stinkers (2 Stars or less)' => '',
    'Music Specials'                     => '',
    'New Titles, Premieres'              => '',
    'Non-Music Specials'                 => '',
    'Non-Series HDTV'                    => '',
    'Science Fiction Movies'             => '',
// includes/programs.php
    'CC'                           => '',
    'HDTV'                         => '',
    'Notes'                        => 'Bem�rkninger',
    'Part $1 of $2'                => '',
    'Stereo'                       => '',
    'Subtitled'                    => '',
    'recstatus: cancelled'         => 'Dette var planlagt til at skulle optages, men blev aflyst manuelt.',
    'recstatus: conflict'          => 'Et andet program med h�jere optageprioritet bliver optaget.',
    'recstatus: currentrecording'  => 'Dette afsnit er optaget tidligere og er stadig tilg�ngeligt i listen af optagede programmer.',
    'recstatus: deleted'           => 'Denne visning blev optaget, men blev slettet inden optagelsen var f�rdig.',
    'recstatus: earliershowing'    => 'Dette afsnit vil blive optaget p� et tidligere tidspunkt.',
    'recstatus: force_record'      => 'Dette program er manuelt sat til at blive optaget.',
    'recstatus: inactive'          => 'Dette program er ikke aktivt',
    'recstatus: latershowing'      => 'Dette afsnit vil blive optaget p� et senere tidspunkt.',
    'recstatus: lowdiskspace'      => 'Der var ikke nok diskplads til at optage dette program.',
    'recstatus: manualoverride'    => 'Denne visning er manuelt sat til ikke at blive optaget.',
    'recstatus: neverrecord'       => 'Optag aldrig dette program',
    'recstatus: notlisted'         => 'Dette program er ikke p� listen',
    'recstatus: previousrecording' => 'Dette afsnit er tidligere optaget j�vnf�r den duplikeringsindstilling der er valgt for dette program.',
    'recstatus: recorded'          => 'Denne visning er optaget.',
    'recstatus: recording'         => 'Optagelse af denne visning er i gang.',
    'recstatus: repeat'            => 'Denne visning er en gentagelse og vil ikke blive optaget.',
    'recstatus: stopped'           => 'Denne visning blev optaget, men blev stoppet inden optagelsen var f�rdig.',
    'recstatus: toomanyrecordings' => 'Der findes for mange optagelser af dette program.',
    'recstatus: tunerbusy'         => 'Tuner-kortet var i brug, da dette program skulle have v�ret optaget.',
    'recstatus: unknown'           => 'Status for denne visning er ukendt.',
    'recstatus: willrecord'        => 'Denne visning vil blive optaget.',
// includes/recording_schedules.php
    'Dup Method'                   => 'Dubletkontrol metode',
    'Profile'                      => 'Profil',
    'Sub and Desc (Empty matches)' => 'Undertitel og beskrivelse',
    'rectype: finddaily'           => 'find 1 dagligt',
    'rectype: findweekly'          => 'find 1 ugentligt',
// includes/utils.php
    '$1 B'  => '',
    '$1 GB' => '',
    '$1 KB' => '',
    '$1 MB' => '',
    '$1 TB' => '',
// modules/backend_log/init.php
    'Logs' => '',
// modules/movietimes/init.php
    'Movie Times' => '',
// modules/settings/init.php
    'MythTV channel info'      => 'MythTV kanal info',
    'MythTV key bindings'      => 'MythTV taste indstillinger',
    'MythWeb session settings' => '',
    'settings'                 => 'ops�tning',
// modules/status/init.php
    'Status' => 'Status',
// modules/stream/init.php
    'Streaming' => 'Streaming',
// modules/tv/detail.php
    'This program is already scheduled to be recorded via a $1custom search$2.' => '',
    'Unknown Program.'                                                          => 'Ukendt program',
    'Unknown Recording Schedule.'                                               => 'Ukendt planlagt optagelse',
// modules/tv/init.php
    'Special Searches' => 'Specielle s�gninger',
    'TV'               => 'Tv',
// modules/tv/recorded.php
    'No matching programs found.'             => '',
    'Showing all programs from the $1 group.' => '',
    'Showing all programs.'                   => '',
// modules/tv/schedules_custom.php
    'Any Category'                               => '',
    'Any Channel'                                => '',
    'Any Program Type'                           => '',
    'Find Time must be of the format:  HH:MM:SS' => '',
// modules/tv/schedules_manual.php
    'Use callsign'  => '',
    'Use date/time' => '',
// modules/tv/search.php
    'Please search for something.' => 'S�g efter noget',
// modules/video/init.php
    'Video' => 'Video',
// themes/default/backend_log/welcome.php
    'welcome: backend_log' => '',
// themes/default/header.php
    'Category Legend'                            => 'Kategoriforklaring',
    'Category Type'                              => 'Kategori Type',
    'Custom'                                     => '',
    'Edit MythWeb and some MythTV settings.'     => 'Rediger MythWeb- og nogle MythTV-indstillinger',
    'Exact Match'                                => 'Pr�cis match',
    'HD Only'                                    => 'Kun HD',
    'Manual'                                     => '',
    'MythMusic on the web.'                      => 'MythMusic p� www',
    'MythVideo on the web.'                      => 'MythVideo p� www',
    'MythWeb Weather.'                           => 'MythWeb Vejrudsigt',
    'Search fields'                              => 'S�gefelter',
    'Search help'                                => 'S�gehj�lp',
    'Search help: movie example'                 => '*** 1/2 Adventure',
    'Search help: movie search'                  => 'movie search',
    'Search help: regex example'                 => '/^Good Eats/',
    'Search help: regex search'                  => 'regex search',
    'Search options'                             => 'S�ge muligheder',
    'Searches'                                   => 'S�gninger',
    'TV functions, including recorded programs.' => 'TV funktioner inkl. optagede programmer',
// themes/default/movietimes/welcome.php
    'welcome: movietimes' => '',
// themes/default/music/music.php
    'Album'               => 'Album',
    'Album (filtered)'    => 'Album (filtreret)',
    'All Music'           => 'Al musik',
    'Artist'              => 'Kunstner',
    'Artist (filtered)'   => 'Kunstner (filtreret)',
    'Displaying'          => 'Viser',
    'Duration'            => 'Varighed',
    'End'                 => 'Slut',
    'Filtered'            => 'Filtreret',
    'Genre'               => 'Genre',
    'Genre (filtered)'    => 'Genre (filtreret)',
    'Next'                => 'N�ste',
    'No Tracks Available' => 'Ingen spor tilg�ngelig',
    'Previous'            => 'Forrige',
    'Top'                 => 'Top',
    'Track Name'          => 'Spor navn',
    'Unfiltered'          => 'Ikke filtr�ret',
// themes/default/music/welcome.php
    'welcome: music' => 'Velkommen: musik',
// themes/default/settings/channels.php
    'Configure Channels'                                                                                                                 => '',
    'Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.' => 'Bem�rk at ved at �ndre i denne tabel uden at vide hvad du laver, kan du �del�gge mythtvs funktioner.',
    'brightness'                                                                                                                         => 'Lys',
    'callsign'                                                                                                                           => 'kortnavn',
    'colour'                                                                                                                             => 'Farve',
    'commfree'                                                                                                                           => 'Reklamefri',
    'contrast'                                                                                                                           => 'Kontrast',
    'delete'                                                                                                                             => 'Slet',
    'finetune'                                                                                                                           => '',
    'freqid'                                                                                                                             => 'Frekvens ID',
    'hue'                                                                                                                                => '',
    'name'                                                                                                                               => 'Navn',
    'sourceid'                                                                                                                           => '',
    'useonairguide'                                                                                                                      => '',
    'videofilters'                                                                                                                       => '',
    'visible'                                                                                                                            => '',
    'xmltvid'                                                                                                                            => '',
// themes/default/settings/keys.php
    'Action'                => '',
    'Configure Keybindings' => '',
    'Context'               => '',
    'Destination'           => '',
    'Edit keybindings on'   => 'Rediger tastaturgenveje for',
    'JumpPoints Editor'     => '',
    'Key bindings'          => '',
    'Keybindings Editor'    => '',
    'Set Host'              => '',
// themes/default/settings/session.php
    'Channel &quot;Jump to&quot;'     => 'Kanal &quot;G� Til&quot;',
    'Date Formats'                    => 'Datoformater',
    'Guide Settings'                  => 'Guide ops�tning',
    'Hour Format'                     => 'Timeformattering',
    'Language'                        => 'Sprog',
    'Listing &quot;Jump to&quot;'     => 'Programoversigt &quot;G� til&quot;',
    'Listing Time Key'                => 'Programoversigt tid',
    'MythWeb Session Settings'        => '',
    'MythWeb Theme'                   => 'MythWeb Tema',
    'Only display favourite channels' => 'Vis kun favorit kanaler',
    'Reset'                           => 'Nulstil',
    'SI Units?'                       => 'SI enheder?',
    'Scheduled Popup'                 => 'Planlagt popup',
    'Show descriptions on new line'   => 'Vis beskrivelser p� ny linie',
    'Status Bar'                      => 'Status linie',
    'Weather Icons'                   => 'Vejr ikoner',
    'format help'                     => 'Formatteringshj�lp',
// themes/default/settings/settings.php
    'settings: overview' => 'Dette er hovedsiden til indstillinger...<p>Den er ikke komplet, og vil senere komme til at se noget p�nere ud. Lige nu kan du v�lge imellem:',
// themes/default/settings/welcome.php
    'welcome: settings' => '',
// themes/default/status/welcome.php
    'welcome: status' => '',
// themes/default/tv/channel.php
    'Channel Detail' => 'Kanal detaljer',
    'Length'         => 'L�ngde',
    'Show'           => 'Program',
    'Time'           => 'Tidspunkt',
// themes/default/tv/detail.php
    'Back to the program listing'         => 'Tilbage til programoversigten',
    'Back to the recording schedules'     => 'Tilbage til planlagte optagelser',
    'Cast'                                => 'Rolleliste',
    'Directed by'                         => 'Instrueret af',
    'Don\'t record this program.'         => 'Optag IKKE dette program',
    'Episode Number'                      => '',
    'Exec. Producer'                      => 'Producer',
    'Find other showings of this program' => 'Find andre udsendelser af dette program',
    'Find showings of this program'       => 'Find udsendelser af dette program',
    'Google'                              => '',
    'Guest Starring'                      => 'G�stestjerner',
    'Guide rating'                        => '',
    'Hosted by'                           => '',
    'MythTV Status'                       => '',
    'Possible conflicts with this show'   => 'Mulige konfligter med denne udsendelse',
    'Presented by'                        => 'Pr�senteret af',
    'Produced by'                         => 'Produceret af',
    'Program Detail'                      => 'Program detaljer',
    'Program ID'                          => '',
    'TV.com'                              => '',
    'Time Stretch Default'                => 'Standard Afspilnings hastighed',
    'What else is on at this time?'       => 'Hvad er der ellers i fjernsynet p� dette tidspunkt?',
    'Written by'                          => 'Skrevet af',
// themes/default/tv/list.php
    'Jump To' => 'G� Til',
// themes/default/tv/list_cell_nodata.php
    'NO DATA' => '',
// themes/default/tv/recorded.php
    '$1 episode'                                          => '$1 afsnit',
    '$1 episodes'                                         => '$1 afsnit',
    '$1 recording'                                        => '$1 optagelse',
    '$1 recordings'                                       => '$1 optagelser',
    'All groups'                                          => '',
    'Are you sure you want to delete the following show?' => 'Er du sikker p� at du vil slette programmet?',
    'Delete'                                              => 'Slet',
    'Delete $1'                                           => 'Slet $1',
    'Delete + Rerecord'                                   => 'Slet + genoptag',
    'Delete and rerecord $1'                              => 'Slet + genoptag $1',
    'Go'                                                  => 'G�',
    'Show group'                                          => 'Vis gruppe',
    'Show recordings'                                     => 'Vis optagelser',
    'auto-expire'                                         => 'auto-udl�b',
    'file size'                                           => 'filst�rrelse',
    'has bookmark'                                        => 'har bookmark(s)',
    'has commflag'                                        => 'har markeret reklameblokke',
    'has cutlist'                                         => 'har klippeliste',
    'is editing'                                          => 'er ved at blive redigeret',
    'preview'                                             => 'forh�ndsvisning',
// themes/default/tv/schedules.php
    'Any'                                       => 'Alle',
    'No recording schedules have been defined.' => 'Ingen optagelser er planlagt',
    'channel'                                   => 'kanal',
    'profile'                                   => 'profil',
    'transcoder'                                => 'trankoder',
    'type'                                      => 'type',
// themes/default/tv/schedules_custom.php
    'Additional Tables'        => '',
    'Find Date & Time Options' => '',
    'Find Day'                 => '',
    'Find Time'                => '',
    'Keyword Search'           => '',
    'People Search'            => '',
    'Power Search'             => '',
    'Search Phrase'            => '',
    'Search Type'              => '',
    'Title Search'             => '',
// themes/default/tv/schedules_manual.php
    'Channel'      => 'Kanal',
    'Length (min)' => 'L�ngde (min)',
    'Start Date'   => 'Startdato',
    'Start Time'   => 'Starttidspunkt',
// themes/default/tv/search.php
    'No matches found'                 => 'Ingen match fundet',
    'No matching programs were found.' => 'Ingen matchende programmer fundet',
    'Search for:  $1'                  => 'S�g efter: $1',
// themes/default/tv/searches.php
    'Handy Predefined Searches' => '',
    'handy: overview'           => '',
// themes/default/tv/upcoming.php
    'Commands'    => 'Kommandoer',
    'Conflicts'   => 'Konflikter',
    'Deactivated' => 'Deaktiveret',
    'Duplicates'  => 'Dubletter',
    'Scheduled'   => 'Planlagt',
// themes/default/tv/welcome.php
    'welcome: tv' => '',
// themes/default/video/video.php
    'Edit'          => '�ndre',
    'Reverse Order' => 'Modsat orden',
    'Videos'        => 'Videoer',
    'category'      => 'kategori',
    'cover'         => 'cover',
    'director'      => 'Instrukt�r',
    'imdb rating'   => '',
    'plot'          => '',
    'rating'        => 'rating',
    'year'          => '�r',
// themes/default/video/welcome.php
    'welcome: video' => '',
// themes/default/weather/weather.php
    ' at '               => ' ved ',
    'Current Conditions' => 'Nuv�rende forhold',
    'Forecast'           => 'Prognose',
    'High'               => 'H�j',
    'Humidity'           => 'Luftfugtighed',
    'Last Updated'       => 'Sidst opdateret',
    'Low'                => 'Lav',
    'Pressure'           => 'Lufttryk',
    'Radar'              => 'Radar',
    'Today'              => 'I dag',
    'Tomorrow'           => 'I morgen',
    'UV Extreme'         => 'UV-str�ling ekstrem',
    'UV High'            => 'UV-str�ling h�j',
    'UV Index'           => 'UV-str�ling indeks',
    'UV Minimal'         => 'UV-str�ling minimal',
    'UV Moderate'        => 'UV-str�ling middel',
    'Visibility'         => 'Sigtbarhed',
    'Wind'               => 'Vind',
    'Wind Chill'         => 'Vindafk�lningseffekt',
// themes/default/weather/welcome.php
    'welcome: weather' => '',
// themes/default/welcome.php
    'Visit $1' => ''
// End of the translation hash ** Do not touch the next line
          );


//Translate chars to utf8
foreach($L as $key => $val){
    $L[$key] = utf8_encode($val);
}

/*
    Show Categories:
    $Categories is a hash of keys corresponding to the css style used for each
    show category.  Each entry is an array containing the name of that category
    in the language this file defines (it will not be translated separately),
    and a regular expression pattern used to match the category against those
    provided in the listings.
*/
$Categories = array();
$Categories['Action']         = array('Action',           '\\b(?:action|even)');
$Categories['Adult']          = array('Erotik',            '\\b(?:adult|eroti|porno)');
$Categories['Animals']        = array('Dyr',          '\\b(?:animal|tiere)');
$Categories['Art_Music']      = array('Kunst/Musik',        '\\b(?:art|kunst|dan(s|ce)|musi[ck]|[ck]ultur)');
$Categories['Business']       = array('Business',         '\\b(?:biz|busine|�kon)');
$Categories['Children']       = array('B�rneprogram',         '\\b(?:child|b�rn|tegnef|animation)');
$Categories['Comedy']         = array('Komedie',           '\\b(?:[ck]omed|entertain|sitcom|underh|stand)');
$Categories['Crime_Mystery']  = array('Krimi/Mysterier',  '\\b(?:[ck]rim|myster)');
$Categories['Documentary']    = array('Dokumentar',      '\\b(?:do[ck])');
$Categories['Drama']          = array('Drama',            '\\b(?:drama)');
$Categories['Educational']    = array('Undervisning',      '\\b(?:edu|interests|undervis)');
$Categories['Food']           = array('Mad',             '\\b(?:food|cook|essen|drink|mad|vin)');
$Categories['Game']           = array('Spil',             '\\b(?:game|spil)');
$Categories['Health_Medical'] = array('Helse/Medicin', '\\b(?:health|medic|sundhe)');
$Categories['History']        = array('Historie',          '\\b(?:hist)');
$Categories['Horror']         = array('Horror',           '\\b(?:horror)');
$Categories['HowTo']          = array('G�r-det-selv',            '\\b(?:how|home|house|garden|g�r|bolig)');
$Categories['Misc']           = array('Diverse',             '\\b(?:special|variety|info|collect)');
$Categories['News']           = array('Nyheder',             '\\b(?:news|current|verdensn|nyheder|nyheter|aktuell?t|rapport)');
$Categories['Reality']        = array('Reality',          '\\b(?:reality)');
$Categories['Romance']        = array('Romantik',          '\\b(?:romance|romanti)');
$Categories['SciFi_Fantasy']  = array('SciFi/Fantasy',  '\\b(?:fantasy|sci\\w*\\W*fi|rum)');
$Categories['Science_Nature'] = array('Naturvidenskab', '\\b(?:science|nature?|environment|vidensk)');
$Categories['Shopping']       = array('Shopping',         '\\b(?:shop)');
$Categories['Soaps']          = array('S�beopera',            '\\b(?:soaps?|s�beop)');
$Categories['Spiritual']      = array('�ndeligt/Religion',        '\\b(?:spirit|relig)');
$Categories['Sports']         = array('Sport',           '\\b(?:sport|fodbold|h�ndbold|badmin|tennis|golf)');
$Categories['Talk']           = array('Talkshow',             '\\b(?:talk)');
$Categories['Travel']         = array('Rejseprogram',           '\\b(?:travel|rejse)');
$Categories['War']            = array('Krig',              '\\b(?:war|krig)');
$Categories['Western']        = array('Western',          '\\b(?:west|cowb)');

// These are some other classes that we might want to have show up in the
//   category legend (they don't need regular expressions)
$Categories['Unknown']        = array('Ukendt');
$Categories['movie']          = array('Film'  );

//Encode localized category names into utf8
foreach($Categories as $category => $definition){
    $Categories[$category][0] = utf8_encode($definition[0]);
}

