<?php
/**
 * Schedule a custom recording by manually specifying various search options
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 *
 * @package     MythWeb
 * @subpackage  TV
 *
 * http://www.gossamer-threads.com/lists/mythtv/dev/102890?search_string=keyword%20search;#102890
 *
/**/

// Populate the $Channels array
    load_all_channels();

// Path-based
    if ($Path[3])
        $_GET['recordid'] = $Path[3];

// Load an existing schedule?
    if ($_GET['recordid'] && $Schedules[$_GET['recordid']]) {
        $schedule =& $Schedules[$_GET['recordid']];
    // Not a custom search schedule
        if (empty($schedule->search) || $schedule->search == searchtype_manual)
            redirect_browser(root.'tv/schedules');
    }
// Create a new, empty schedule
    else
        $schedule = new Schedule(NULL);

// The user tried to update the recording settings - update the database and the variable in memory
    if (isset($_POST['save'])) {
    // Which type of recording is this?  Make sure an illegal one isn't specified
        $schedule->search_type = '';
        switch ($_POST['searchtype']) {
            case searchtype_power:   $schedule->search_type = 'Power';   break;
            case searchtype_title:   $schedule->search_type = 'Title';   break;
            case searchtype_keyword: $schedule->search_type = 'Keyword'; break;
            case searchtype_people:  $schedule->search_type = 'People';  break;
                break;
        // Everything else generates an error message
            default:
                trigger_error('Unknown search type specified:  '.$_POST['searchtype']);
        }
    // Which type of recording is this?  Make sure an illegal one isn't specified
        switch ($_POST['record']) {
        // Only certain rectypes are allowed
            case rectype_findone:
            case rectype_always:
            case rectype_finddaily:
            case rectype_findweekly:
                break;
        // Everything else gets ignored
            default:
                $_POST['record'] = 0;
        }
    // Cancelling a schedule?
        if ($_POST['record'] == 0) {
        // Cancel this schedule
            if ($schedule && $schedule->recordid) {
            // Delete the schedule
                $schedule->delete();
            // Redirect back to the schedule list
                add_warning(t('The requested recording schedule has been deleted.'));
                save_session_errors();
                header('Location: '.root.'tv/schedules');
                exit;
            }
        }
    // Adding a new schedule
        else {
        // Set things as the user requested
            $schedule->profile       = $_POST['profile'];
            $schedule->recgroup      = $_POST['recgroup'];
            $schedule->autoexpire    = $_POST['autoexpire']   ? 1 : 0;
            $schedule->autocommflag  = $_POST['autocommflag'] ? 1 : 0;
            $schedule->autouserjob1  = $_POST['autouserjob1'] ? 1 : 0;
            $schedule->autouserjob2  = $_POST['autouserjob2'] ? 1 : 0;
            $schedule->autouserjob3  = $_POST['autouserjob3'] ? 1 : 0;
            $schedule->autouserjob4  = $_POST['autouserjob4'] ? 1 : 0;
            $schedule->inactive      = $_POST['inactive']     ? 1 : 0;
            $schedule->maxnewest     = $_POST['maxnewest']    ? 1 : 0;
            $schedule->dupin         = _or($_POST['dupin'],    15);
            $schedule->dupmethod     = _or($_POST['dupmethod'], 6);
            $schedule->recpriority   = intval($_POST['recpriority']);
            $schedule->maxepisodes   = intval($_POST['maxepisodes']);
            $schedule->startoffset   = intval($_POST['startoffset']);
            $schedule->endoffset     = intval($_POST['endoffset']);
        // Some settings specific to manual recordings (since we have no program to match against)
            $schedule->chanid        = $_POST['channel'];
            $schedule->station       = $Channels[$schedule->chanid]->callsign;
            $schedule->starttime     = time();
            $schedule->endtime       = time() + 1;
            $schedule->category      = 'Custom recording';
            $schedule->search        = $_POST['searchtype'];
            $schedule->findday       = $_POST['findday'];
            $schedule->autotranscode = $_POST['autotranscode'] ? 1 : 0;
            $schedule->transcoder    = $_POST['transcoder'];
        // Parse the findtime
            $schedule->findtime      = trim($_POST['findtime']);
            if ($schedule->findtime) {
                if (!preg_match('/^\d{1,2}:\d{2}:\d{2}$/', $schedule->findtime))
                    add_error(t('Find Time must be of the format:  HH:MM:SS'));
            }
            else
                $schedule->findtime = date('H:m:s', $schedule->starttime);
        // Build the special description
            if ($schedule->search == searchtype_power) {
            // Remove any trailing semi colons, and any secondary hackish queries
                $schedule->description = preg_replace('/\s*;\s*(EXPLAIN|DESCRIBE|SHOW|SELECT|DELETE|UPDATE|INSERT|REPLACE).*$/i', '',
                                         preg_replace('/;$/', '',
                                                      $_POST['search_sql']
                                                     ));
            // The subtitle is actually used to store additional SQL tables
                if (preg_match('/\w/', $_POST['additional_tables']))
                    $schedule->subtitle = preg_replace('/^\W*/', ', ',
                                          preg_replace('/\W+$/', '',
                                                       $_POST['additional_tables']
                                                      ));
            // Run a test query
                $db->disable_fatal_errors();
                $sh = $db->query('SELECT NULL FROM program, channel'.str_replace('?', '\\?', $schedule->subtitle)
                                .' WHERE '.str_replace('?', '\\?', $schedule->description));
                $db->enable_fatal_errors();
                if ($db->error) {
                    add_error("There is an error in your custom SQL query:\n\n"
                              .preg_replace('/^.+?SQL\s+syntax;\s*/', '', $db->error)
                             );
                }
            }
            else {
                $schedule->description = $_POST['search_phrase'];
                $schedule->subtitle    = '';
            }
        // Figure out the title
            $schedule->title = _or($_POST['title'], $schedule->description).' ('.t('$1 Search', $schedule->search_type).')';
        // Only save if there are no errors
            if (!errors()) {
            // Save the schedule
                $schedule->save($_POST['record']);
            // Redirect to the new schedule
                header('Location: '.root.'tv/schedules/custom/'.$schedule->recordid);
                exit;
            }
        }
    }
// Load default settings for recpriority, autoexpire etc
    else {
    // Make sure we have a default rectype
        if (!$schedule->type)
            $schedule->type = rectype_always;
    // auto-commercial-flag
        if (!isset($schedule->autocommflag))
            $schedule->autocommflag = get_backend_setting('AutoCommercialFlag');
    // auto-user-jobs
        if (!isset($schedule->autouserjob1))
            $schedule->autouserjob1 = get_backend_setting('AutoRunUserJob1');
        if (!isset($schedule->autouserjob2))
            $schedule->autouserjob2 = get_backend_setting('AutoRunUserJob2');
        if (!isset($schedule->autouserjob3))
            $schedule->autouserjob3 = get_backend_setting('AutoRunUserJob3');
        if (!isset($schedule->autouserjob4))
            $schedule->autouserjob4 = get_backend_setting('AutoRunUserJob4');
    // auto-transcode
        if (!isset($schedule->autotranscode))
            $schedule->autotranscode = get_backend_setting('AutoTranscode');
    // transcoder
        if (!isset($schedule->transcoder))
            $schedule->transcoder = get_backend_setting('DefaultTranscoder');
    // recpriority
        if (!isset($schedule->recpriority)) {
            $result = mysql_query('SELECT recpriority from channel where chanid='.escape($program->chanid));
            list($schedule->recpriority) = mysql_fetch_row($result);
            mysql_free_result($result);
        }
    // autoexpire
        if (!isset($schedule->autoexpire)) {
            $result = mysql_query('SELECT data from settings where value="AutoExpireDefault"');
            list($schedule->autoexpire) = mysql_fetch_row($result);
            mysql_free_result($result);
        }
    // Get the searchtype string
        switch ($schedule->search) {
            case searchtype_power:   $schedule->search_type = t('Power');   break;
            case searchtype_keyword: $schedule->search_type = t('Keyword'); break;
            case searchtype_people:  $schedule->search_type = t('People');  break;
            case searchtype_title:
            default:                 $schedule->search_type = t('Title');   break;
        }
    }

// Create an edit-friendly title
    $schedule->edit_title = preg_replace('/\s*\(\w+\s*search\)\s*$/i', '', $schedule->title);

// Calculate the length
    $schedule->length = intval(($schedule->endtime - $schedule->starttime) / 60);
    if ($schedule->length < 1)
        $schedule->length = 120;

// Load the class for this page
    require_once theme_dir.'tv/schedules_custom.php';

// Exit
    exit;

/**
 * Prints a <select> of the available program categories
/**/
    function category_select() {
        echo '<select name="channel"><option value=""';
        if (empty($chanid))
            echo ' SELECTED';
        echo '>('.t('Any Category').')</option>';
        echo '</select>';
    }

/**
 * Prints a <select> of the available program category_types
/**/
    function category_type_select() {
        echo '<select name="channel"><option value=""';
        if (empty($chanid))
            echo ' SELECTED';
        echo '>('.t('Any Program Type').')</option>';
        echo '</select>';
    }

/**
 * prints a <select> of the available channels
/**/
    function channel_select($chanid) {
        global $Channels;
        echo '<select name="channel"><option value=""';
        if (empty($chanid))
            echo ' SELECTED';
        echo '>('.t('Any Channel').')</option>';
        foreach ($Channels as $channel) {
        // Print the option
            echo '<option value="'.$channel->chanid.'"';
        // Selected?
            if ($channel->chanid == $chanid)
                echo ' SELECTED';
        // Print ther est of the content
            echo '>';
            if (prefer_channum)
                echo $channel->channum.'&nbsp;&nbsp;('.html_entities($channel->callsign).')';
            else
                echo html_entities($channel->callsign).'&nbsp;&nbsp;('.$channel->channum.')';
            echo '</option>';
        }
        echo '</select>';
    }

/**
 * Prints a <select> of the various weekdays
/**/
    function day_select($day, $name='findday') {
        $days = array(t('Sunday'),    t('Monday'),   t('Tuesday'),
                             t('Wednesday'), t('Thursday'), t('Friday'),
                             t('Saturday'));
    // Print the list
        echo "<select name=\"$name\">";
        foreach ($days as $key => $day) {
            $key++;
            echo "<option value=\"$key\"";
            if ($key == $day)
                echo ' SELECTED';
            echo '>'.html_entities($day).'</option>';
        }
        echo '</select>';
    }
