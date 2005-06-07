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
    '$1 min'                                             => '$1 minut',
    '$1 mins'                                            => '$1 minutter',
    '$1 programs, using $2 ($3) out of $4.'              => '$1 programmer, bruger $2 ($3) ud af $4',
    '$1 to $2'                                           => '$1 til $2',
    'Advanced Options'                                   => 'Avancerede indstillinger',
    'Airtime'                                            => 'Visningsdato',
    'All recordings'                                     => 'Alle optagelser',
    'Auto-expire recordings'                             => 'Auto-udl�b optagelser',
    'Auto-flag commercials'                              => 'Marker automatisk reklamer',
    'Backend Status'                                     => 'System status',
    'Cancel this schedule.'                              => 'Annuller denne optagelse',
    'Category'                                           => 'Kategori',
    'Channel'                                            => 'Kanal',
    'Check for duplicates in'                            => 'Kontroller for dubletter i',
    'Create Schedule'                                    => 'Planl�g',
    'Current recordings'                                 => 'Nuv�rende optagelser',
    'Date'                                               => 'Dato',
    'Description'                                        => 'Beskrivelse',
    'Duplicate Check method'                             => 'dubletkontrol metode',
    'End Late'                                           => 'Slut senere (min)',
    'Episode'                                            => 'Afsnit',
    'Go'                                                 => 'G�',
    'Hour'                                               => 'Time',
    'Jump'                                               => 'G�',
    'Jump to'                                            => 'G� til',
    'Length (min)'                                       => 'L�ngde (min)',
    'Listings'                                           => 'Programoversigt',
    'No'                                                 => 'Nej',
    'No. of recordings to keep'                          => 'Gem antal optagelser',
    'None'                                               => 'Ingen',
    'Notes'                                              => 'Bem�rkninger',
    'Only New Episodes'                                  => 'Kun Nye Afsnit',
    'Original Airdate'                                   => 'Oprindelig visningsdato',
    'Previous recordings'                                => 'Tidligere optagelser',
    'Rating'                                             => 'Karakterer',
    'Record new and expire old'                          => 'Optag nye og udl�b gamle',
    'Recorded Programs'                                  => 'Optagede programmer',
    'Recording Group'                                    => 'Optagelsesgruppe',
    'Recording Options'                                  => 'Optageindstillinger',
    'Recording Priority'                                 => 'Optageprioritet',
    'Recording Profile'                                  => 'Optageprofil',
    'Rerun'                                              => 'Genudsendelse',
    'Schedule'                                           => 'Planl�g',
    'Schedule Options'                                   => 'Planl�gningsindstillinger',
    'Schedule Override'                                  => 'Planl�g manuelt',
    'Schedule normally.'                                 => 'Planl�g normalt',
    'Scheduled Recordings'                               => 'Planlagte optagelser',
    'Search'                                             => 'S�g',
    'Search Results'                                     => 'S�geresultater',
    'Start Date'                                         => 'Startdato',
    'Start Early'                                        => 'Start tidligere (min)',
    'Start Time'                                         => 'Starttidspunkt',
    'Subtitle'                                           => 'Undertitel',
    'Subtitle and Description'                           => 'Undertitel og beskrivelse',
    'The requested recording schedule has been deleted.' => 'Den planlagte optagelse er blevet slettet',
    'Title'                                              => 'Titel',
    'Unknown'                                            => 'Ukendt',
    'Update Recording Settings'                          => 'Opdater optagelsesindstillinger',
    'Yes'                                                => 'Ja',
    'airdate'                                            => 'visningsdato',
    'channum'                                            => 'kanalnummer',
    'description'                                        => 'beskrivelse',
    'generic_date'                                       => '%a %e %b %Y',
    'generic_time'                                       => '%H:%M',
    'length'                                             => 'l�ngde',
    'minutes'                                            => 'minutter',
    'recgroup'                                           => 'Optagelsesgrupper',
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
// includes/programs.php
    'recstatus: cancelled'         => 'Dette var planlagt til at skulle optages, men blev aflyst manuelt.',
    'recstatus: conflict'          => 'Et andet program med h�jere optageprioritet bliver optaget.',
    'recstatus: currentrecording'  => 'Dette afsnit er optaget tidligere og er stadig tilg�ngeligt i listen af optagede programmer.',
    'recstatus: deleted'           => 'Denne visning blev optaget, men blev slettet inden optagelsen var f�rdig.',
    'recstatus: earliershowing'    => 'Dette afsnit vil blive optaget p� et tidligere tidspunkt.',
    'recstatus: force_record'      => 'Dette program er manuelt sat til at blive optaget.',
    'recstatus: latershowing'      => 'Dette afsnit vil blive optaget p� et senere tidspunkt.',
    'recstatus: lowdiskspace'      => 'Der var ikke nok diskplads til at optage dette program.',
    'recstatus: manualoverride'    => 'Denne visning er manuelt sat til ikke at blive optaget.',
    'recstatus: overlap'           => 'Denne optagelse er allerede h�ndteret af en anden planlagt optagelse af dette program.',
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
    'Dup Method'                   => 'dubletkontrol metode',
    'Profile'                      => 'Profil',
    'Sub and Desc (Empty matches)' => 'Undertitel og beskrivelse',
    'Type'                         => 'Type',
    'rectype: finddaily'           => 'find 1 dagligt',
    'rectype: findweekly'          => 'find 1 ugentligt',
// includes/utils.php
    '$1 B'   => '',
    '$1 GB'  => '',
    '$1 KB'  => '',
    '$1 MB'  => '',
    '$1 TB'  => '',
    '$1 hr'  => '$1 t',
    '$1 hrs' => '$1 t',
// program_detail.php
    'Unknown Program.'            => 'Ukendt program',
    'Unknown Recording Schedule.' => 'Ukendt planlagt optagelse',
// search.php
    'Please search for something.' => '',
// themes/.../canned_searches.php
    'handy: overview' => '',
// themes/.../channel_detail.php
    'Length' => 'L�ngde',
    'Show'   => 'Program',
    'Time'   => 'Tidspunkt',
// themes/.../program_detail.php
    '$1 Rating'                           => '$1 Karakterer',
    'Back to the program listing'         => 'Tilbage til programoversigten',
    'Back to the recording schedules'     => 'Tilbage til planlagte optagelser',
    'Cast'                                => 'Rolleliste',
    'Directed by'                         => 'Instrueret af',
    'Don\'t record this program.'         => '',
    'Exec. Producer'                      => 'Producer',
    'Find other showings of this program' => 'Find andre visninger af dette program',
    'Find showings of this program'       => 'Find visninger af dette program',
    'Google'                              => '',
    'Guest Starring'                      => 'G�stestjerner',
    'Hosted by'                           => '',
    'IMDB'                                => '',
    'Inactive'                            => '',
    'Presented by'                        => 'Pr�senteret af',
    'Produced by'                         => 'Produceret af',
    'TVTome'                              => '',
    'What else is on at this time?'       => 'Hvad er der ellers i fjernsynet p� dette tidspunkt?',
    'Written by'                          => 'Skrevet af',
// themes/.../program_listing.php
    'Currently Browsing:  $1' => 'Lige nu vises: $1',
    'Jump To'                 => 'G� Til',
// themes/.../recorded_programs.php
    '$1 episode'                                          => '$1 afsnit',
    '$1 episodes'                                         => '$1 afsnit',
    '$1 recording'                                        => '$1 optagelse',
    '$1 recordings'                                       => '$1 optagelser',
    'Are you sure you want to delete the following show?' => 'Er du sikker p� at du vil slette programmet?',
    'Delete'                                              => 'Slet',
    'Delete + Rerecord'                                   => '',
    'Show group'                                          => 'Vis gruppe',
    'Show recordings'                                     => 'Vis optagelser',
    'auto-expire'                                         => 'auto-udl�b',
    'file size'                                           => 'filst�rrelse',
    'has bookmark'                                        => 'har bookmark(s)',
    'has commflag'                                        => 'har markeret reklameblokke',
    'has cutlist'                                         => 'har klippeliste',
    'is editing'                                          => 'er ved at blive redigeret',
    'preview'                                             => 'forh�ndsvisning',
// themes/.../recording_profiles.php
    'Profile Groups'     => 'Profilgrupper',
    'Recording profiles' => 'Optagelsesprofiler',
// themes/.../recording_schedules.php
    'Any'                                       => 'Alle',
    'No recording schedules have been defined.' => '',
    'channel'                                   => '',
    'profile'                                   => 'profil',
    'type'                                      => 'type',
// themes/.../schedule_manually.php
    'Save Schedule' => '',
// themes/.../scheduled_recordings.php
    'Activate'      => 'Aktiver',
    'Commands'      => 'Kommandoer',
    'Conflicts'     => 'Konflikter',
    'Deactivated'   => 'Deaktiveret',
    'Default'       => 'Standard',
    'Display'       => 'Vis',
    'Don\'t Record' => 'Optag ikke',
    'Duplicates'    => 'Dubletter',
    'Forget Old'    => 'Glem gamle',
    'Never Record'  => 'Optag aldrig',
    'Record This'   => 'Optag dette',
    'Scheduled'     => 'Planlagt',
    'Update'        => '',
// themes/.../search.php
    'No matches found' => 'Ingen resultater',
    'Search for:  $1'  => '',
// themes/.../settings.php
    'Channels'           => 'Kanaler',
    'Configure'          => 'Konfigurer',
    'Key Bindings'       => 'Tastaturgenveje',
    'MythWeb Settings'   => 'MythWeb indstillinger',
    'settings: overview' => 'Dette er hovedsiden til indstillinger...<p>Den er ikke komplet, og vil senere komme til at se noget p�nere ud. Lige nu kan du v�lge imellem:',
// themes/.../settings_channels.php
    'Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.' => 'Bem�rk at ved at �ndre i denne tabel uden at vide hvad du laver, kan du �del�gge mythtvs funktioner.',
// themes/.../settings_keys.php
    'Edit keybindings on' => 'Rediger tastaturgenveje for',
// themes/.../settings_mythweb.php
    'Channel &quot;Jump to&quot;'     => 'Kanal &quot;G� Til&quot;',
    'Date Formats'                    => 'Datoformater',
    'Guide Settings'                  => '',
    'Hour Format'                     => 'Timeformattering',
    'Language'                        => 'Sprog',
    'Listing &quot;Jump to&quot;'     => 'Programoversigt &quot;G� til&quot;',
    'Listing Time Key'                => 'Programoversigt tid',
    'MythWeb Theme'                   => 'MythWeb Tema',
    'Only display favourite channels' => '',
    'Reset'                           => 'Nulstil',
    'SI Units?'                       => '',
    'Save'                            => 'Gem',
    'Scheduled Popup'                 => 'Planlagt popup',
    'Show descriptions on new line'   => 'Vis beskrivelser p� ny linie',
    'Status Bar'                      => '',
    'Weather Icons'                   => '',
    'format help'                     => 'Formatteringshj�lp',
// themes/.../theme.php
    'Category Legend'                            => 'Kategoriforklaring',
    'Category Type'                              => 'Kategori Type',
    'Edit MythWeb and some MythTV settings.'     => 'Rediger MythWeb- og nogle MythTV-indstillinger',
    'Exact Match'                                => 'Pr�cis match',
    'HD Only'                                    => '',
    'Manually Schedule'                          => 'Planl�g manuelt',
    'MythMusic on the web.'                      => 'MythMusic p� www',
    'MythVideo on the web.'                      => 'MythVideo p� www',
    'MythWeb Weather.'                           => 'MythWeb Vejrudsigt',
    'Recording Schedules'                        => 'Optagelsesplanl�gning',
    'Search fields'                              => '',
    'Search help'                                => '',
    'Search help: movie example'                 => '*** 1/2 Adventure',
    'Search help: movie search'                  => 'movie search',
    'Search help: regex example'                 => '/^Good Eats/',
    'Search help: regex search'                  => 'regex search',
    'Search options'                             => '',
    'Searches'                                   => '',
    'Settings'                                   => 'Indstillinger',
    'TV functions, including recorded programs.' => 'TV funktioner inkl. optagede programmer',
// themes/.../weather.php
    ' at '               => ' ved ',
    'Current Conditions' => 'Nuv�rende forhold',
    'Forecast'           => 'Prognose',
    'Friday'             => 'Fredag',
    'High'               => 'H�j',
    'Humidity'           => 'Luftfugtighed',
    'Last Updated'       => 'Sidst opdateret',
    'Low'                => 'Lav',
    'Monday'             => 'Mandag',
    'Pressure'           => 'Lufttryk',
    'Radar'              => '',
    'Saturday'           => 'L�rdag',
    'Sunday'             => 'S�ndag',
    'Thursday'           => 'Torsdag',
    'Today'              => 'I dag',
    'Tomorrow'           => 'I morgen',
    'Tuesday'            => 'Tirsdag',
    'UV Extreme'         => 'UV-str�ling ekstrem',
    'UV High'            => 'UV-str�ling h�j',
    'UV Index'           => 'UV-str�ling indeks',
    'UV Minimal'         => 'UV-str�ling minimal',
    'UV Moderate'        => 'UV-str�ling middel',
    'Visibility'         => 'Sigtbarhed',
    'Wednesday'          => 'Onsdag',
    'Wind'               => 'Vind',
    'Wind Chill'         => 'Vindafk�lningseffekt'
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
?>
