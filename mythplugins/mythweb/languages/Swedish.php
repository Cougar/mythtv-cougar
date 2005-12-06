<?php
/***                                                                        ***\
    languages/Swedish.php

    Translation hash for Swedish.
\***                                                                        ***/

// Set the locale to Swedish UTF-8
setlocale(LC_ALL, 'sv_SE.UTF-8');

// Define the language lookup hash ** Do not touch the next line
$L = array(
// Add your translations below here.
// Warning, any custom comments will be lost during translation updates.
//
// Shared Terms
    ' at '                                                => ' vid ',
    '$1 Rating'                                           => 'Rangordning $1',
    '$1 episode'                                          => '$1 avsnitt',
    '$1 episodes'                                         => '$1 avsnitt',
    '$1 hr'                                               => '$1h',
    '$1 hrs'                                              => '$1h',
    '$1 min'                                              => '$1 min',
    '$1 mins'                                             => '$1 min',
    '$1 programs, using $2 ($3) out of $4 ($5 free).'     => '$1 program ($3), som använder $2 av $4',
    '$1 recording'                                        => '$1 inspelning',
    '$1 recordings'                                       => '$1 inspelningar',
    '$1 to $2'                                            => '$1 till $2',
    'Activate'                                            => 'Aktivera',
    'Advanced Options'                                    => 'Avancerade inställningar',
    'Airtime'                                             => 'Sändningstid',
    'All recordings'                                      => 'Alla inspelningar',
    'Are you sure you want to delete the following show?' => 'Är du säker på att du vill ta bort följande inspelning?',
    'Auto-expire recordings'                              => 'Autoradera inspelningar',
    'Auto-flag commercials'                               => 'Markera reklam automatiskt',
    'Auto-transcode'                                      => 'Omkoda automatiskt',
    'Back to the program listing'                         => 'Tillbaka till programlistan',
    'Back to the recording schedules'                     => 'Tillbaka till inspelningsschemat',
    'Backend Status'                                      => 'Systemstatus',
    'Cancel this schedule.'                               => 'Avbryt denna schemaläggning',
    'Cast'                                                => 'Rollbesättning',
    'Category'                                            => 'Kategori',
    'Category Legend'                                     => 'Kategoriförklaring',
    'Category Type'                                       => 'Kategorityp',
    'Check for duplicates in'                             => 'Sök dubbletter i',
    'Commands'                                            => 'Kommando',
    'Conflicts'                                           => 'Konflikter',
    'Current Conditions'                                  => 'Nuvarande förhållanden',
    'Current recordings'                                  => 'Nuvarande inspelningar',
    'Currently Browsing:  $1'                             => 'Just nu visas:  $1',
    'Date'                                                => 'Datum',
    'Deactivated'                                         => 'Avaktiverad',
    'Default'                                             => 'Standard',
    'Delete'                                              => 'Radera',
    'Delete $1'                                           => 'Rader $1',
    'Delete + Rerecord'                                   => 'Radera + Återinspela',
    'Delete and rerecord $1'                              => 'Radera och återinspela $1',
    'Description'                                         => 'Beskrivning',
    'Details for'                                         => 'Detaljer för',
    'Directed by'                                         => 'Regi',
    'Display'                                             => 'Visning',
    'Don\'t Record'                                       => 'Spela ej in',
    'Don\'t record this program.'                         => 'Spela inte in detta program.',
    'Duplicate Check method'                              => 'Dubblettmetod',
    'Duplicates'                                          => 'Dubbletter',
    'Edit MythWeb and some MythTV settings.'              => 'Ändra MythWeb och några MythTV-inställningar',
    'End Late'                                            => 'Sluta senare',
    'Episode'                                             => 'Avsnitt',
    'Exact Match'                                         => 'Exakt matchning',
    'Exec. Producer'                                      => 'Producent',
    'Find other showings of this program'                 => 'Hitta andra visningar av detta program',
    'Find showings of this program'                       => 'Hitta visningar av detta program',
    'Forecast'                                            => 'Prognos',
    'Forget Old'                                          => 'Glöm gammal',
    'Friday'                                              => 'Fredag',
    'Go'                                                  => 'Gå',
    'Google'                                              => 'Google',
    'Guest Starring'                                      => 'Gästspel',
    'Guide rating'                                        => '',
    'HD Only'                                             => 'Endast HD',
    'High'                                                => 'Hög',
    'Hosted by'                                           => 'Värd',
    'Hour'                                                => 'Timme',
    'Humidity'                                            => 'Luftfuktighet',
    'IMDB'                                                => 'IMDB',
    'Inactive'                                            => 'Inaktiv',
    'Jump'                                                => 'Gå',
    'Jump To'                                             => 'Gå till',
    'Jump to'                                             => 'Gå till',
    'Last Updated'                                        => 'Senast uppdaterad',
    'Listings'                                            => 'TV-tablåer',
    'Low'                                                 => 'Låg',
    'Manually Schedule'                                   => 'Manuell schemaläggning',
    'Monday'                                              => 'Måndag',
    'Music'                                               => 'Musik',
    'MythMusic on the web.'                               => 'MythMusic på webben',
    'MythVideo on the web.'                               => 'MythVideo på webben',
    'MythWeb Weather.'                                    => 'MythWeb väder.',
    'Never Record'                                        => 'Spela aldrig in',
    'No'                                                  => 'Nej',
    'No. of recordings to keep'                           => 'Antal inspelningar att behålla',
    'None'                                                => 'Ingen',
    'Notes'                                               => 'Anteckningar',
    'Only New Episodes'                                   => 'Endast nya avsnitt',
    'Original Airdate'                                    => 'Sändningsdatum',
    'Please search for something.'                        => 'Välj något att söka efter.',
    'Presented by'                                        => 'Presenteras av',
    'Pressure'                                            => 'Lufttryck',
    'Previous recordings'                                 => 'Tidigare inspelningar',
    'Produced by'                                         => 'Producerad av',
    'Program Detail'                                      => 'Programdetaljer',
    'Program Listing'                                     => 'Programlista',
    'Radar'                                               => 'Radar',
    'Rating'                                              => 'Betyg',
    'Record This'                                         => 'Spela in',
    'Record new and expire old'                           => 'Spela in nya och radera gamla',
    'Recorded Programs'                                   => 'Inspelade program',
    'Recording Group'                                     => 'Inspelningsgrupp',
    'Recording Options'                                   => 'Inspelningsinställningar',
    'Recording Priority'                                  => 'Inspelningsprioritet',
    'Recording Profile'                                   => 'Inspelningsprofil',
    'Recording Schedules'                                 => 'Inspelningsscheman',
    'Rerun'                                               => 'Repris',
    'Saturday'                                            => 'Lördag',
    'Save'                                                => 'Spara',
    'Schedule'                                            => 'Schema',
    'Schedule Options'                                    => 'Schemaläggningsval',
    'Schedule Override'                                   => 'Åsidosätt schemaläggning',
    'Schedule normally.'                                  => 'Schemalägg normalt',
    'Scheduled'                                           => 'Schemalagd',
    'Scheduled Recordings'                                => 'Schemalagda inspelningar',
    'Search'                                              => 'Sök',
    'Search Results'                                      => 'Sökresultat',
    'Search fields'                                       => 'Sökfält',
    'Search help'                                         => 'Sökhjälp',
    'Search help: movie example'                          => '*** Äventyr',
    'Search help: movie search'                           => 'Filmsökning',
    'Search help: regex example'                          => '/^Alpint/',
    'Search help: regex search'                           => 'Reguljärt uttryck',
    'Search options'                                      => 'Sökval',
    'Searches'                                            => 'Sökningar',
    'Settings'                                            => 'Inställningar',
    'Show group'                                          => 'Visa grupp',
    'Show recordings'                                     => 'Visa inspelningar',
    'Start Early'                                         => 'Börja tidigare',
    'Subtitle'                                            => 'Undertitel',
    'Subtitle and Description'                            => 'Undertitel och beskrivning',
    'Sunday'                                              => 'Söndag',
    'TV functions, including recorded programs.'          => 'Tv-funktioner, inkl. inspelade program.',
    'TV.com'                                              => 'TV.com',
    'The requested recording schedule has been deleted.'  => 'Det begärda inspelningsschemat har tagits bort.',
    'Thursday'                                            => 'Torsdag',
    'Time Stretch Default'                                => 'Förvalt tempo',
    'Title'                                               => 'Titel',
    'Today'                                               => 'Idag',
    'Tomorrow'                                            => 'Imorgon',
    'Transcoder'                                          => 'Omkodare',
    'Tuesday'                                             => 'Tisdag',
    'UV Extreme'                                          => 'UV-strålning extrem',
    'UV High'                                             => 'UV-strålning hög',
    'UV Index'                                            => 'UV-strålning',
    'UV Minimal'                                          => 'UV-strålning minimal',
    'UV Moderate'                                         => 'UV-strålning måttlig',
    'Unknown'                                             => 'Okänd',
    'Unknown Program.'                                    => 'Okänt program.',
    'Unknown Recording Schedule.'                         => 'Okänt inspelningsschema.',
    'Upcoming Recordings'                                 => '',
    'Update'                                              => 'Uppdatera',
    'Update Recording Settings'                           => 'Uppdatera inspelningsinställningar',
    'Visibility'                                          => 'Sikt',
    'Weather'                                             => 'Väder',
    'Wednesday'                                           => 'Onsdag',
    'What else is on at this time?'                       => 'Vad visas mer vid denna tid?',
    'Wind'                                                => 'Vind',
    'Wind Chill'                                          => 'Vindkyleffekt',
    'Written by'                                          => 'Skriven av',
    'Yes'                                                 => 'Ja',
    'airdate'                                             => 'visningsdatum',
    'auto-expire'                                         => 'autoradera',
    'channum'                                             => 'kanalnummer',
    'description'                                         => 'beskrivning',
    'file size'                                           => 'filstorlek',
    'generic_date'                                        => '%Y-%m-%d',
    'generic_time'                                        => '%H:%M',
    'has bookmark'                                        => 'bokmärke',
    'has commflag'                                        => 'markerad reklam',
    'has cutlist'                                         => 'klipplista',
    'is editing'                                          => 'redigeras',
    'length'                                              => 'längd',
    'minutes'                                             => 'minuter',
    'preview'                                             => 'förhandsvisning',
    'recgroup'                                            => 'inspelningsgrupp',
    'recpriority'                                         => 'inspelningsprioritet',
    'rectype-long: always'                                => 'Spela in vilken tid som helst på alla kanaler.',
    'rectype-long: channel'                               => 'Spela in vilken tid som helst på kanal $1.',
    'rectype-long: daily'                                 => 'Spela in denna tid varje dag.',
    'rectype-long: dontrec'                               => 'Spela inte in denna specifika visning.',
    'rectype-long: finddaily'                             => 'Spela in en visning av detta program varje dag.',
    'rectype-long: findone'                               => 'Spela in en visning av detta program.',
    'rectype-long: findweekly'                            => 'Spela in en visning av detta program varje vecka.',
    'rectype-long: once'                                  => 'Spela endast in denna visning.',
    'rectype-long: override'                              => 'Spela in denna specifika visning.',
    'rectype-long: weekly'                                => 'Spela in denna tid varje vecka.',
    'rectype: always'                                     => 'Alltid',
    'rectype: channel'                                    => 'Kanal',
    'rectype: daily'                                      => 'Daglig',
    'rectype: dontrec'                                    => 'Spela ej in',
    'rectype: findone'                                    => 'Hitta en',
    'rectype: once'                                       => 'Enstaka',
    'rectype: override'                                   => 'Överskugga',
    'rectype: weekly'                                     => 'Veckovis',
    'subtitle'                                            => 'undertitel',
    'title'                                               => 'titel',
// includes/programs.php
    'recstatus: cancelled'         => 'Denna visning spelades inte in därför att den avbröts manuellt.',
    'recstatus: conflict'          => 'Ett annat program med en högre prioritet kommer att spelas in.',
    'recstatus: currentrecording'  => 'Denna visning kommer inte att spelas in därför att detta avsnitt redan spelats in och fortfarande är tillgängligt i listan över inspelningar.',
    'recstatus: deleted'           => 'Denna visning spelades in men togs bort innan inspelningen var slutförd.',
    'recstatus: earliershowing'    => 'Detta avsnitt kommer att spelas in vid en tidigare tidpunkt istället.',
    'recstatus: force_record'      => 'Denna visning sattes manuellt att spela in.',
    'recstatus: inactive'          => '',
    'recstatus: latershowing'      => 'Detta avsnitt kommer att spelas in vid en senare tidpunkt istället.',
    'recstatus: lowdiskspace'      => 'Denna visning spelades inte in därför att det inte fanns tillräckligt med ledigt diskutrymme.',
    'recstatus: manualoverride'    => 'Denna visning sattes manuellt till att inte spela in.',
    'recstatus: neverrecord'       => 'Spela aldrig in',
    'recstatus: notlisted'         => 'Ej listad',
    'recstatus: previousrecording' => 'Detta avsnitt redan spelats in enligt dubblettkontrollen vald för detta program.',
    'recstatus: recorded'          => 'Denna visning spelades in.',
    'recstatus: recording'         => 'Denna visning spelas in.',
    'recstatus: repeat'            => 'Denna visning är en repris och kommer inte att spelas in.',
    'recstatus: stopped'           => 'Denna visning spelades in men stoppades innan den var färdiginspelad.',
    'recstatus: toomanyrecordings' => 'För många inspelningar av detta program redan gjorts.',
    'recstatus: tunerbusy'         => 'Denna visning spelades inte in därför att TV-kortet redan användes.',
    'recstatus: unknown'           => 'Statusen för denna visning är okänd.',
    'recstatus: willrecord'        => 'Denna visning kommer att spelas in.',
// includes/recording_schedules.php
    'Dup Method'                   => 'Dubblettmetod',
    'Profile'                      => 'Profil',
    'Sub and Desc (Empty matches)' => 'Undertitel & beskrivning',
    'Type'                         => 'Typ',
    'rectype: finddaily'           => 'En per dag',
    'rectype: findweekly'          => 'En per vecka',
// includes/utils.php
    '$1 B'  => '$1 B',
    '$1 GB' => '$1 GB',
    '$1 KB' => '$1 KB',
    '$1 MB' => '$1 MB',
    '$1 TB' => '$1 TB',
// modules/backend_log/init.php
    'Logs' => '',
// modules/movietimes/init.php
    'Movie Times' => '',
// modules/settings/init.php
    'settings' => '',
// modules/status/init.php
    'Status' => '',
// modules/tv/init.php
    'Search TV'        => '',
    'Special Searches' => '',
    'TV'               => '',
// modules/video/init.php
    'Video' => '',
// themes/.../canned_searches.php
    'handy: overview' => 'Denna sida innehåller förberedda komplexa sökningar av programlistorna.',
// themes/.../channel_detail.php
    'Length' => 'Längd',
    'Show'   => 'Program',
    'Time'   => 'Tid',
// themes/.../music.php
    'Album'               => 'Album',
    'Album (filtered)'    => 'Album (filtrerat)',
    'All Music'           => 'All musik',
    'Artist'              => 'Artist',
    'Artist (filtered)'   => 'Artist (filtrerad)',
    'Displaying'          => 'Visar',
    'Duration'            => 'Längd',
    'End'                 => 'Slut',
    'Filtered'            => 'Filtrerat',
    'Genre'               => 'Genre',
    'Genre (filtered)'    => 'Genre (filtrerad)',
    'Next'                => 'Nästa',
    'No Tracks Available' => 'Inga spår tillgängliga',
    'Previous'            => 'Föregående',
    'Top'                 => 'Överst',
    'Track Name'          => 'Spårnamn',
    'Unfiltered'          => 'Ofiltrerat',
// themes/.../recording_profiles.php
    'Profile Groups'     => 'Profilgrupper',
    'Recording profiles' => 'Inspelningsprofiler',
// themes/.../recording_schedules.php
    'Any'                                       => 'Alla',
    'No recording schedules have been defined.' => 'Inga schemalagda inspelningar',
    'channel'                                   => 'kanal',
    'profile'                                   => 'profil',
    'transcoder'                                => 'omkodare',
    'type'                                      => 'typ',
// themes/.../schedule_manually.php
    'Channel'           => 'Kanal',
    'Create Schedule'   => 'Schemalägg',
    'Length (min)'      => 'Längd (min)',
    'Save Schedule'     => 'Spara schema',
    'Schedule Manually' => 'Schemalägg manuellt',
    'Start Date'        => 'Startdatum',
    'Start Time'        => 'Starttid',
// themes/.../search.php
    'No matches found' => 'Inga matchningar funna',
    'Search for:  $1'  => 'Sök efter:  $1',
// themes/.../settings.php
    'Channels'           => 'Kanaler',
    'Configure'          => 'Konfigurera',
    'Key Bindings'       => 'Knappar',
    'MythWeb Settings'   => 'MythWeb-inställningar',
    'settings: overview' => 'Detta är startsidan för inställningarna. Inte helt komplett, men följande finns för närvarande att välja på: ',
// themes/.../settings_channels.php
    'Configure Channels'                                                                                                                 => 'Konfigurera kanaler',
    'Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.' => 'OBS! Genom att ändra dessa inställningar utan att veta vad du gör kan du allvarligt störa MythTVs funktionalitet.',
    'brightness'                                                                                                                         => 'ljusstyrka',
    'callsign'                                                                                                                           => 'kortnamn',
    'colour'                                                                                                                             => 'färg',
    'commfree'                                                                                                                           => 'reklamfri',
    'contrast'                                                                                                                           => 'kontrast',
    'delete'                                                                                                                             => 'radera',
    'finetune'                                                                                                                           => 'finjustera',
    'freqid'                                                                                                                             => 'frekvensid',
    'hue'                                                                                                                                => 'färgton',
    'name'                                                                                                                               => 'namn',
    'sourceid'                                                                                                                           => 'källid',
    'useonairguide'                                                                                                                      => '',
    'videofilters'                                                                                                                       => 'videofilter',
    'visible'                                                                                                                            => 'synlig',
// themes/.../settings_keys.php
    'Action'                => 'Handling',
    'Configure Keybindings' => 'Konfigurera knappar',
    'Context'               => 'Sammanhang',
    'Destination'           => 'Destination',
    'Edit keybindings on'   => 'Editera knappar på',
    'JumpPoints Editor'     => 'Redigera hoppmarkeringar',
    'Key bindings'          => 'Knappbindningar',
    'Keybindings Editor'    => 'Redigera knappar',
    'Set Host'              => 'Sätt värd',
// themes/.../settings_mythweb.php
    'Channel &quot;Jump to&quot;'     => 'Kanal &quot;Gå till&quot;',
    'Date Formats'                    => 'Datumformat',
    'Guide Settings'                  => 'Guideinställningar',
    'Hour Format'                     => 'Timformat',
    'Language'                        => 'Språk',
    'Listing &quot;Jump to&quot;'     => 'TV-tablå &quot;Gå till&quot;',
    'Listing Time Key'                => 'TV-tablå tid',
    'MythWeb Theme'                   => 'MythWeb-tema',
    'Only display favourite channels' => 'Visa endast favoritkanaler',
    'Reset'                           => 'Återställ',
    'SI Units?'                       => 'SI-enheter?',
    'Scheduled Popup'                 => 'Schemalagd popup',
    'Show descriptions on new line'   => 'Visa beskrivning på ny rad',
    'Status Bar'                      => 'Statusrad',
    'Weather Icons'                   => 'Väderikoner',
    'format help'                     => 'formathjälp',
// themes/.../video.php
    'Edit'          => 'Redigera',
    'Reverse Order' => 'Omvänd ordning',
    'Videos'        => 'Videor',
    'category'      => 'kategori',
    'cover'         => 'omslag',
    'director'      => 'regissör',
    'imdb rating'   => 'imdb-betyg',
    'plot'          => 'handling',
    'rating'        => 'betyg',
    'year'          => 'år',
// themes/default/backend_log/backend_log.php
    'Backend Logs' => '',
// themes/default/backend_log/welcome.php
    'Show the server logs.' => '',
// themes/default/movietimes/welcome.php
    'Get listings for movies playing at local theatres.' => '',
// themes/default/music/welcome.php
    'Browse your music collection.' => '',
// themes/default/settings/welcome.php
    'Configure MythWeb.' => '',
// themes/default/status/welcome.php
    'Show the backend status page.' => '',
// themes/default/tv/list_cell_nodata.php
    'NO DATA' => '',
// themes/default/tv/welcome.php
    'See what\'s on tv, schedule recordings and manage shows that you\'ve already recorded.  Please see the following choices:' => '',
// themes/default/video/welcome.php
    'Browse your video collection.' => '',
// themes/default/weather/welcome.php
    'Get the local weather forecast.' => ''
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
/* I'll update this when we've got the new Swedish guide-data in a more mature state. */
$Categories = array();
$Categories['Action']         = array('Action',          '\\b(?:action|adven)');
$Categories['Adult']          = array('Adult',           '\\b(?:adult|erot)');
$Categories['Animals']        = array('Djur',            '\\b(?:animal|djur)');
$Categories['Art_Music']      = array('Konst/musik',     '\\b(?:art|dance|musi[ck]|[ck]ultur)');
$Categories['Business']       = array('Affärer/ekonomi','\\b(?:biz|busine|ekonomi|affärer)');
$Categories['Children']       = array('Barnprogram',     '\\b(?:child|animation|kid|tecknat|barn)');
$Categories['Comedy']         = array('Komedi',          '\\b(?:comed|entertain|sitcom|komedi)');
$Categories['Crime_Mystery']  = array('Brott/mysterier', '\\b(?:[ck]rim|myster)');
$Categories['Documentary']    = array('Dokumentär',     '\\b(?:documentary|dokumentär)');
$Categories['Drama']          = array('Drama',           '\\b(?:drama)');
$Categories['Educational']    = array('Utbildning',      '\\b(?:edu|interests|utbildning)');
$Categories['Food']           = array('Mat',             '\\b(?:food|cook|matlag|dryck)');
$Categories['Game']           = array('Lek/spel',        '\\b(?:game|spel)');
$Categories['Health_Medical'] = array('Medicin/hälsa',  '\\b(?:health|medic)');
$Categories['History']        = array('Historia',        '\\b(?:histor)');
$Categories['Horror']         = array('Rysare',          '\\b(?:horror|rysare)');
$Categories['HowTo']          = array('Gör-det-själv', '\\b(?:how|home|house|garden|hus|hem|trädgård)');
$Categories['Misc']           = array('Blandat',         '\\b(?:special|variety|info|collect|blandat|nöje)');
$Categories['News']           = array('Nyheter',         '\\b(?:news|nyheter)');
$Categories['Reality']        = array('Dokusåpa',       '\\b(?:reality|dokusåpa)');
$Categories['Romance']        = array('Romantik',        '\\b(?:romance|romantik|kärlek)');
$Categories['SciFi_Fantasy']  = array('Natur/vetenskap', '\\b(?:science|nature|environment|natur|vetenskap)');
$Categories['Science_Nature'] = array('SciFi/fantasy',   '\\b(?:fantasy|sci\\w*\\W*fi)');
$Categories['Shopping']       = array('Shopping',        '\\b(?:shop)');
$Categories['Soaps']          = array('Såpopera',       '\\b(?:soaps|såpopera)');
$Categories['Spiritual']      = array('Andligt',         '\\b(?:spirit|andlig)');
$Categories['Sports']         = array('Sport',           '\\b(?:sport)');
$Categories['Talk']           = array('Talkshow',        '\\b(?:talk)');
$Categories['Travel']         = array('Resor',           '\\b(?:travel|resor)');
$Categories['War']            = array('Krig',            '\\b(?:war|krig)');
$Categories['Western']        = array('Western',         '\\b(?:west)');

// These are some other classes that we might want to have show up in the
//   category legend (they don't need regular expressions)
$Categories['Unknown']        = array('Okänd');
$Categories['movie']          = array('Film',            '\\b(?:movie|film)');

