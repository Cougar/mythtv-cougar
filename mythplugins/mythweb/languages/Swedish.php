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
    '$1 min'                    => '$1 min',
    '$1 mins'                   => '$1 min',
    'Airtime'                   => 'S�ndningstid',
    'All recordings'            => 'Alla inspelningar',
    'Auto-expire recordings'    => 'Autoradera inspelningar',
    'Category'                  => 'Kategori',
    'Check for duplicates in'   => 'S�k dubletter i',
    'Current recordings'        => 'Nuvarande inspelningar',
    'Date'                      => 'Datum',
    'Description'               => 'Beskrivning',
    'Duplicate Check method'    => 'Dublettmetod',
    'End Late'                  => 'Sluta senare',
    'Go'                        => 'G�',
    'No. of recordings to keep' => 'Antal inspelningar att beh�lla',
    'None'                      => 'Ingen',
    'Notes'                     => 'Anteckningar',
    'Original Airdate'          => 'Ursprungligt visningsdatum',
    'Previous recordings'       => 'Tidigare inspelningar',
    'Profile'                   => 'Profil',
    'Rating'                    => 'Betyg',
    'Record new and expire old' => 'Spela in nya och radera gamla',
    'Recorded Programs'         => 'Inspelade program',
    'Recording Group'           => 'Inspelningsgrupp',
    'Recording Options'         => 'Inspelningsinst�llningar',
    'Recording Priority'        => 'Inspelningsprioritet',
    'Recording Profile'         => 'Inspelningsprofil',
    'Rerun'                     => 'Repris',
    'Schedule'                  => 'Schema',
    'Scheduled Recordings'      => 'Schemalagda inspelningar',
    'Search'                    => 'S�k',
    'Start Early'               => 'B�rja tidigare',
    'Subtitle'                  => 'Undertitel',
    'Subtitle and Description'  => 'Undertitel och beskrivning',
    'Title'                     => 'Titel',
    'Unknown'                   => 'Ok�nd',
    'Update Recording Settings' => 'Uppdatera inspelningsinst�llningar',
    'airdate'                   => 'visningsdatum',
    'channum'                   => 'kanalnummer',
    'description'               => 'beskrivning',
    'length'                    => 'l�ngd',
    'recgroup'                  => 'inspelningsgrupp',
    'rectype-long: always'      => 'Spela in vilken tid som helst p� alla kanaler',
    'rectype-long: channel'     => 'Spela in vilken tid som helst p� denna kanal',
    'rectype-long: daily'       => 'Spela in denna tid varje dag',
    'rectype-long: findone'     => 'Spela in en visning av detta program',
    'rectype-long: once'        => 'Spela endast in denna visning',
    'rectype-long: weekly'      => 'Spela in denna tid varje vecka',
    'subtitle'                  => 'undertitel',
    'title'                     => 'titel',
// includes/init.php
    'generic_date' => '%Y-%m-%d',
    'generic_time' => '%H:%i',
// includes/programs.php
    'recstatus: cancelled'         => 'Denna visning spelades inte in d�rf�r att den avbr�ts manuellt.',
    'recstatus: conflict'          => 'Ett annat program med en h�gre prioritet kommer att spelas in.',
    'recstatus: currentrecording'  => 'Denna visning kommer inte att spelas in d�rf�r att detta avsnitt redan spelats in och fortfarande �r tillg�ngligt i listan �ver inspelningar.',
    'recstatus: deleted'           => 'Denna visning spelades in men togs bort innan inspelningen var slutf�rd.',
    'recstatus: earliershowing'    => 'Detta avsnitt kommer att spelas in vid en tidigare tidpunkt ist�llet.',
    'recstatus: force_record'      => 'Denna visning sattes manuellt att spela in.',
    'recstatus: latershowing'      => 'Detta avsnitt kommer att spelas in vid en senare tidpunkt ist�llet.',
    'recstatus: lowdiskspace'      => 'Denna visning spelades inte in d�rf�r att det inte fanns tillr�ckligt med ledigt diskutrymme.',
    'recstatus: manualoverride'    => 'Denna visning sattes manuellt till att inte spela in.',
    'recstatus: overlap'           => 'Denna visning t�cks av ett annat inspelningsschema f�r samma program.',
    'recstatus: previousrecording' => 'Detta avsnitt redan spelats in enligt dubblettkontrollen vald f�r detta program.',
    'recstatus: recorded'          => 'Denna visning spelades in.',
    'recstatus: recording'         => 'Denna visning spelas in.',
    'recstatus: stopped'           => 'Denna visning spelades in men stoppades innan den var f�rdiginspelad.',
    'recstatus: toomanyrecordings' => 'F�r m�nga inspelningar av detta program redan gjorts.',
    'recstatus: tunerbusy'         => 'Denna visning spelades inte in d�rf�r att TV-kortet redan anv�ndes.',
    'recstatus: unknown'           => 'Statusen f�r denna visning �r ok�nd.',
    'recstatus: willrecord'        => 'Denna visning kommer att spelas in.',
// includes/recordings.php
    'rectype: always'   => 'Alltid',
    'rectype: channel'  => 'Kanal',
    'rectype: daily'    => 'Daglig',
    'rectype: dontrec'  => 'Spela ej in',
    'rectype: findone'  => 'Hitta en',
    'rectype: once'     => 'Enstaka',
    'rectype: override' => '�verskugga',
    'rectype: weekly'   => 'Veckovis',
// includes/utils.php
    '$1 B'   => '$1 B',
    '$1 GB'  => '$1 GB',
    '$1 KB'  => '$1 KB',
    '$1 MB'  => '$1 MB',
    '$1 TB'  => '$1 TB',
    '$1 hr'  => '$1h',
    '$1 hrs' => '$1h',
// themes/.../channel_detail.php
    'Episode' => 'Avsnitt',
    'Jump to' => 'G� till',
    'Length'  => 'L�ngd',
    'Show'    => 'Program',
    'Time'    => 'Tid',
// themes/.../program_detail.php
    '$1 to $2'                            => '$1 till $2',
    'Back to the program listing'         => 'Tillbaka till programlistan',
    'Back to the recording schedules'     => 'Tillbaka till inspelningsschemat',
    'Cancel this schedule'                => 'Avbryt denna schemal�ggning',
    'Don\'t record this program'          => 'Spela inte in detta program',
    'Find other showings of this program' => 'Hitta andra visningar av detta program',
    'Google'                              => 'Google',
    'IMDB'                                => 'IMDB',
    'TVTome'                              => 'TVTime',
    'What else is on at this time?'       => 'Vad visas mer vid denna tid?',
// themes/.../program_listing.php
    'Currently Browsing:  $1' => 'Just nu visas:  $1',
    'Hour'                    => 'Timme',
    'Jump'                    => 'G�',
    'Jump To'                 => 'G� till',
// themes/.../recorded_programs.php
    '$1 episode'                                          => '$1 avsnitt',
    '$1 episodes'                                         => '$1 avsnitt',
    '$1 programs, using $2 ($3) out of $4.'               => '$1 program ($3), som anv�nder $2 av $4',
    '$1 recording'                                        => '$1 inspelning',
    '$1 recordings'                                       => '$1 inspelningar',
    'Are you sure you want to delete the following show?' => '�r du s�ker p� att du vill da bort f�ljande inspelning?',
    'Delete'                                              => 'Radera',
    'No'                                                  => 'Nej',
    'Show group'                                          => 'Visa grupp',
    'Show recordings'                                     => 'Visa inspelningar',
    'Yes'                                                 => 'Ja',
    'auto-expire'                                         => 'autoradera',
    'file size'                                           => 'filstorlek',
    'has bookmark'                                        => 'bokm�rke',
    'has commflag'                                        => 'markerad reklam',
    'has cutlist'                                         => 'klipplista',
    'is editing'                                          => 'editeras',
    'preview'                                             => 'f�rhandsvisning',
// themes/.../recording_profiles.php
    'Profile Groups'     => 'Profilgrupper',
    'Recording profiles' => 'Inspelningsprofiler',
// themes/.../recording_schedules.php
    'Any'                          => 'Alla',
    'Dup Method'                   => 'Dublettmetod',
    'Sub and Desc (Empty matches)' => 'Undertitel & beskrivning',
    'Type'                         => 'Typ',
    'profile'                      => 'Profil',
    'type'                         => 'Typ',
// themes/.../schedule_manually.php
    'Channel'      => 'Kanal',
    'Length (min)' => 'L�ngd (min)',
    'Start Date'   => 'Startdatum',
    'Start Time'   => 'Starttid',
// themes/.../scheduled_recordings.php
    'Activate'      => 'Aktivera',
    'Commands'      => 'Kommando',
    'Conflicts'     => 'Konflikter',
    'Deactivated'   => 'Avaktiverad',
    'Default'       => 'Standard',
    'Display'       => 'Visning',
    'Don\'t Record' => 'Spela ej in',
    'Duplicates'    => 'Dubletter',
    'Forget Old'    => 'Gl�m gammal',
    'Never Record'  => 'Spela aldrig in',
    'Record This'   => 'Spela in',
    'Scheduled'     => 'Schemalagd',
// themes/.../search.php
    'Category Type'    => 'Kategorityp',
    'Exact Match'      => 'Exakt matchning',
    'No matches found' => 'Inga matchningar funna',
// themes/.../settings.php
    'Channels'           => 'Kanaler',
    'Configure'          => 'Konfigurera',
    'Key Bindings'       => 'Knappar',
    'MythWeb Settings'   => 'MythWeb-inst�llningar',
    'settings: overview' => 'Detta �r startsidan f�r inst�llningarna. Inte helt komplett, men f�ljande finns f�r n�rvarande att v�lja p�: ',
// themes/.../settings_channels.php
    'Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.' => 'OBS! Genom att �ndra dessa inst�llningar utan att veta vad du g�r kan du allvarligt st�ra MythTVs funktionalitet.',
// themes/.../settings_keys.php
    'Edit keybindings on' => 'Editera knappar p�',
// themes/.../settings_mythweb.php
    'Channel &quot;Jump to&quot;'   => 'Kanal &quot;G� till&quot;',
    'Date Formats'                  => 'Datumformat',
    'Hour Format'                   => 'Timformat',
    'Language'                      => 'Spr�k',
    'Listing &quot;Jump to&quot;'   => 'TV-tabl� &quot;G� till&quot;',
    'Listing Time Key'              => 'TV-tabl� tid',
    'MythWeb Theme'                 => 'MythWeb-tema',
    'Reset'                         => '�terst�ll',
    'Save'                          => 'Spara',
    'Scheduled Popup'               => 'Schemalagd popup',
    'Search Results'                => 'S�kresultat',
    'Show descriptions on new line' => 'Visa beskrivning p� ny rad',
    'Status Bar'                    => 'Statusrad',
    'format help'                   => 'formathj�lp',
// themes/.../theme.php
    'Backend Status'      => 'Systemstatus',
    'Category Legend'     => 'Kategorif�rklaring',
    'Favorites'           => 'Favoriter',
    'Go To'               => 'G� till',
    'Listings'            => 'TV-tabl�er',
    'Manually Schedule'   => 'Manuell schemal�ggning',
    'Movies'              => 'Filmer',
    'Recording Schedules' => 'Inspelningsscheman',
    'Settings'            => 'Inst�llningar',
    'advanced'            => 'avancerad',
// themes/.../weather.php
    ' at '               => ' vid ',
    'Current Conditions' => 'Nuvarande f�rh�llanden',
    'Forecast'           => 'Prognos',
    'Friday'             => 'Fredag',
    'High'               => 'H�g',
    'Humidity'           => 'Luftfuktighet',
    'Last Updated'       => 'Senast uppdaterad',
    'Low'                => 'L�g',
    'Monday'             => 'M�ndag',
    'Pressure'           => 'Lufttryck',
    'Radar'              => 'Radar',
    'Saturday'           => 'L�rdag',
    'Sunday'             => 'S�ndag',
    'Thursday'           => 'Torsdag',
    'Today'              => 'Idag',
    'Tomorrow'           => 'Imorgon',
    'Tuesday'            => 'Tisdag',
    'UV Extreme'         => 'UV-str�lning extrem',
    'UV High'            => 'UV-str�lning h�g',
    'UV Index'           => 'UV-str�lning',
    'UV Minimal'         => 'UV-str�lning minimal',
    'UV Moderate'        => 'UV-str�lning m�ttlig',
    'Visibility'         => 'Sikt',
    'Wednesday'          => 'Onsdag',
    'Wind'               => 'Vind',
    'Wind Chill'         => 'Vindkyleffekt'
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
$Categories['Animals']        = array('Djur',            '\\b(?:animal|tiere)');
$Categories['Art_Music']      = array('Konst/musik',     '\\b(?:art|dance|musi[ck]|kunst|[ck]ultur)');
$Categories['Business']       = array('Aff�rer/ekonomi', '\\b(?:biz|busine)');
$Categories['Children']       = array('Barnprogram',     '\\b(?:child|kin?d|infan|animation)');
$Categories['Comedy']         = array('Komedi',          '\\b(?:comed|entertain|sitcom)');
$Categories['Crime_Mystery']  = array('Brott/mysterier', '\\b(?:[ck]rim|myster)');
$Categories['Documentary']    = array('Dokument�r',      '\\b(?:do[ck])');
$Categories['Drama']          = array('Drama',           '\\b(?:drama)');
$Categories['Educational']    = array('Utbildning',      '\\b(?:edu|bildung|interests)');
$Categories['Food']           = array('Mat',             '\\b(?:food|cook|essen|[dt]rink)');
$Categories['Game']           = array('Lek/spel',        '\\b(?:game|spiele)');
$Categories['Health_Medical'] = array('Medicin/hälsa',  '\\b(?:health|medic|gesundheit)');
$Categories['History']        = array('Historia',        '\\b(?:hist|geschichte)');
$Categories['Horror']         = array('Rysare',          '\\b(?:horror)');
$Categories['HowTo']          = array('G�r-det-sj�lv',   '\\b(?:how|home|house|garden)');
$Categories['Misc']           = array('Blandat',         '\\b(?:special|variety|info|collect)');
$Categories['News']           = array('Nyheter',         '\\b(?:news|nyheter|aktuellt|rapport|(Vä|�)stnytt)');
$Categories['Reality']        = array('Dokus�pa',        '\\b(?:reality)');
$Categories['Romance']        = array('Romantik',        '\\b(?:romance|lieb)');
$Categories['SciFi_Fantasy']  = array('Natur/vetenskap', '\\b(?:science|nature|environment|wissenschaft)');
$Categories['Science_Nature'] = array('SciFi/fantasy',   '\\b(?:fantasy|sci\\w*\\W*fi)');
$Categories['Shopping']       = array('Shopping',        '\\b(?:shop)');
$Categories['Soaps']          = array('S�popera',        '\\b(?:soaps)');
$Categories['Spiritual']      = array('Andligt',         '\\b(?:spirit|relig)');
$Categories['Sports']         = array('Sport',           '\\b(?:sport|deportes|futbol)');
$Categories['Talk']           = array('Talkshow',        '\\b(?:talk)');
$Categories['Travel']         = array('Resor',           '\\b(?:travel|reisen)');
$Categories['War']            = array('Krig',            '\\b(?:war|krieg)');
$Categories['Western']        = array('Western',         '\\b(?:west)');

// These are some other classes that we might want to have show up in the
//   category legend (they don't need regular expressions)
$Categories['Unknown']        = array('Ok�nd');
$Categories['movie']          = array('Film');

?>
