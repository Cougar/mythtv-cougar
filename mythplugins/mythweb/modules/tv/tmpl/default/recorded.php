<?php
/**
 * Show recorded programs.
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

// Set the desired page title
    $page_title = 'MythWeb - '.t('Recorded Programs');

// Custom headers
    $headers[] = '<link rel="stylesheet" type="text/css" href="'.skin_url.'/tv_recorded.css" />';

// Print the page header
    require 'modules/_shared/tmpl/'.tmpl.'/header.php';

// Global variables used here
    global $All_Shows, $Total_Programs, $Total_Time, $Total_Used,
           $Groups,    $Program_Titles;

// Show the recgroup?
    if (count($Groups) > 1) {
        $recgroup_cols = 1;
    }
    else {
        $recgroup_cols = 0;
    }

// Setup for grouping by various sort orders
    $group_field = $_SESSION['recorded_sortby'][0]['field'];
    if ($group_field == "") {
        $group_field = "airdate";
    } elseif ( ! (($group_field == "title") || ($group_field == "channum") || ($group_field == "airdate") || ($group_field == "recgroup")) ) {
        $group_field = "";
    }

?>

<script language="JavaScript" type="text/javascript">
<!--

// Some initial values for global counters
    var diskused       = parseInt('<?php echo addslashes(disk_used) ?>');
    var programcount   = parseInt('<?php echo addslashes($Total_Programs) ?>');
    var programs_shown = parseInt('<?php echo count($All_Shows) ?>');
    var totaltime      = parseInt('<?php echo addslashes($Total_Time) ?>');

// Initialize some variables that will get set after the page table is printed
    var rowcount     = new Array();
    var rowsection   = new Array();
    var titles       = new Object;
    var groups       = new Object;

// Load the known shows
    var file  = null;
    var files = new Array();

<?php
    foreach ($All_Shows as $show) {
?>
    file = new Object();
    file.title      = '<?php echo addslashes($show->title)                  ?>';
    file.subtitle   = '<?php echo addslashes($show->subtitle)               ?>';
    file.chanid     = '<?php echo addslashes($show->chanid)                 ?>';
    file.starttime  = '<?php echo addslashes($show->recstartts)             ?>';
    file.group      = '<?php echo addslashes(str_replace('%2F', '/', rawurlencode($show->group)))    ?>';
    file.filename   = '<?php echo addslashes(str_replace('%2F', '/', rawurlencode($show->filename))) ?>';
    file.size       = '<?php echo addslashes($show->filesize)               ?>';
    file.length     = '<?php echo addslashes($show->recendts - $show->recstartts) ?>';
    file.autoexpire = <?php echo $show->auto_expire ? 1 : 0                 ?>;
    files.push(file);

<?php
    }
?>

// Set the autoexpire flag
    function set_autoexpire(id) {
        var file = files[id];
        var r = new Ajax.Request('<?php echo root ?>tv/detail/' + file.chanid + '/' + file.starttime,
                                 {
                                    parameters: 'toggle_autoexpire='+(1 - file.autoexpire),
                                  asynchronous: false
                                 });
        if (r.transport.responseText == 'success') {
        // Update to the new value
            file.autoexpire = 1 - file.autoexpire;
        // Fix the images
            $('autoexpire_'+id).src = '<?php echo skin_url, '/img/flags/' ?>'
                                      + (file.autoexpire
                                         ? ''
                                         : 'no_')
                                      + 'autoexpire.png';
            if (file.autoexpire)
                $('autoexpire_'+id).title = '<?php echo addslashes(t('Click to disable Auto Expire')) ?>';
            else
                $('autoexpire_'+id).title = '<?php echo addslashes(t('Click to enable Auto Expire')) ?>';
        }
        else if (r.transport.responseText) {
            alert('Error: '+r.transport.responseText);
        }
    }

    function confirm_delete(id, forget_old) {
        var file = files[id];
        if (confirm("<?php echo t('Are you sure you want to delete the following show?')
                    ?>\n\n     "+file.title+": "+file.subtitle)) {
            var url = '<?php echo root ?>tv/recorded?delete=yes&chanid='+file.chanid
                      +'&starttime='+file.starttime
                      +(forget_old
                        ? '&forget_old=yes'
                        : '');
        // Do the actual deletion
            if (programs_shown == 1)
                location.href = url;
            else {
                ajax_add_request();
                submit_url(url+'&ajax=yes', http_success, http_failure, id, file);
            }
        // Debug statements - uncomment to verify that the right file is being deleted
            //alert('row number ' + id + ' belonged to section ' + section + ' which now has ' + rowcount[section] + ' elements');
            //alert('just deleted an episode of "' + title + '" which now has ' + episode_count + ' episodes left');
        }
    }

    function http_success(result, args) {
        var id   = args.shift();
        var file = args.shift();
    // Hide the row from view
        toggle_vis('inforow_' + id,   'display');
        toggle_vis('statusrow_' + id, 'display');
    // decrement the number of rows in a section
        var section   = rowsection[id];
        rowcount[section]--;
    // Decrement the number of episodes for this title
        titles[file.title]--;
        var episode_count = titles[file.title]
    // If we just hid the only row in a section, then hide the section break above it as well
        if (rowcount[section] == 0) {
            toggle_vis('breakrow_' + section, 'display');
        }
    // Change the recordings dropdown menu on the fly
        if (episode_count == 0) {
            toggle_vis('Title ' + file.title, 'display');
        }
        else {
            var count_text;
            count_text = (episode_count > 1) ? ' (' + episode_count + ' episodes)' : '';
            $('Title ' + file.title).innerHTML = file.title + count_text;
        }
    // TODO: test changing the groups dropdown on the fly.
    // I can't test it because I haven't set up any recording groups, and probably never will
        if (file.group) {
        // Decrement the number of episodes for this group
            groups[file.group]--;
            var group_count = titles[file.title]
        // Change the groups dropdown menu on the fly
            if (group_count == 0) {
                toggle_vis('Group ' + file.group, 'display');
            }
            else {
                var count_text;
                group_text = (group_count >1) ? ' (' + group_count + ' episodes)' : '';
                $('Group ' + file.group).innerHTML = file.group + group_text;
            }
        }
    // Decrement the total number of shows and update the page
        programs_shown--;
        programcount--;
        $('programcount').innerHTML = programcount;
    // Decrease the total amount of time by the amount of the show
        totaltime -= file.length;
        $('totaltime').innerHTML = nice_length(totaltime, <?php
                                                         echo "'", addslashes(t('$1 hr')),   "', ",
                                                              "'", addslashes(t('$1 hrs')),  "', ",
                                                              "'", addslashes(t('$1 min')),  "', ",
                                                              "'", addslashes(t('$1 mins')), "'";
                                                         ?>);
    // Decrease the disk usage indicator by the amount of the show
        diskused -= file.size;
        $('diskused').innerHTML = nice_filesize(diskused);
    // Adjust the freespace shown
        $('diskfree').innerHTML = nice_filesize(<?php echo disk_size ?> - diskused);
        // Eventually, we should perform the removal-from-the-list here instead
        // of in confirm_delete()
        ajax_remove_request();
    }

    function http_failure(err, errstr, args) {
        var file = args[0];
        alert("Can't delete "+file.title+': '+file.subtitle+".\nHTTP Error:  " + errstr + ' (' + err + ')');
        ajax_remove_request();
    }

// -->
</script>

<form id="change_title" action="<?php echo root ?>tv/recorded" method="get">
<table id="title_choices" class="commandbox commands" border="0" cellspacing="0" cellpadding="4" align="center">
<tr>
<?php if (count($Groups) > 1) { ?>
    <td class="-group"><?php echo t('Show group') ?>:</td>
    <td><select name="recgroup" onchange="$('change_title').submit()">
        <option value=""><?php echo t('All groups') ?></option><?php
        foreach($Groups as $recgroup => $count) {
            echo '<option id="Group '.htmlspecialchars($recgroup).'" value="'.htmlspecialchars($recgroup).'"';
            if ($_GET['recgroup'] == $recgroup)
                echo ' SELECTED';
            echo '>'.html_entities($recgroup)
                .' ('.tn('$1 recording', '$1 recordings', $count)
                .')</option>';
        }
        ?>
    </select></td>
<?php
    }
?>
    <td class="-recordings"><?php echo t('Show recordings') ?>:</td>
    <td><select name="title" onchange="$('change_title').submit()">
        <option id="All recordings" value=""><?php echo t('All recordings') ?></option>
<?php
        foreach($Program_Titles as $title => $count) {
            echo '<option id="Title '.htmlspecialchars($title).'" value="'.htmlspecialchars($title).'"';
            if ($_GET['title'] == $title)
                echo ' SELECTED';
            echo '>'.html_entities($title)
                .($count > 1 ? ' ('.tn('$1 episode', '$1 episodes', $count).')' : "")
                ."</option>\n";
        }
?>
    </select></td>
</tr>
</table>
</form>

<table id="recorded_list" border="0" cellpadding="0" cellspacing="0" class="list small">
<tr class="menu">
    <td class="list"<?php if ($group_field) echo ' colspan="2"' ?>>&nbsp;</td>
    <th class="-title"><?php     echo get_sort_link('title',     t('Title'))      ?></th>
    <th class="-subtitle"><?php  echo get_sort_link('subtitle',  t('Subtitle'))   ?></th>
    <th class="-programid"><?php echo get_sort_link('programid', t('Program ID')) ?></th>
    <th class="-airdate"><?php   echo get_sort_link('airdate',   t('Airdate'))    ?></th>
    <th class="-channum"><?php   echo get_sort_link('channum',   t('Channel'))    ?></th>
<?php
    if ($recgroup_cols)
        echo '    <th class="-recgroup">', get_sort_link('recgroup', t('Recording Group')), "</td>\n";
?>
    <th class="-length"><?php        echo get_sort_link('length',    t('Length')) ?></td>
    <th class="-filesize"><?php echo get_sort_link('file_size', t('File Size')) ?></td>
</tr><?php
    $row     = 0;
    $section = -1;

    $prev_group = '';
    $cur_group  = '';

    foreach ($All_Shows as $show) {

    // Print a dividing row if grouping changes
        switch ($group_field) {
            case 'airdate':
                $cur_group = strftime($_SESSION['date_listing_jump'], $show->starttime);
                break;
            case 'recgroup':
                $cur_group = $show->recgroup;
                break;
            case 'channum':
                $cur_group = $show->channel->channum.' - '.$show->channel->name;
                break;
            case 'title':
                $cur_group = $show->title;
                break;
        }

        if ( $cur_group != $prev_group && $group_field != '' ) {
            $section++;
            $colspan = 10 + $recgroup_cols;
            print <<<EOM
<tr id="breakrow_$section" class="list_separator">
    <td colspan="$colspan" class="list_separator">$cur_group</td>
</tr>
EOM;
        }

        echo "<tr id=\"inforow_$row\" class=\"recorded\">\n";
        if ($group_field != "")
            echo "    <td class=\"list\" rowspan=\"2\">&nbsp;</td>\n";
?>
    <td rowspan="2" class="-pixmap<?php
        if ($_SESSION['recorded_pixmaps']) { ?>">
        <a class="-pixmap" href="<?php echo root ?>tv/detail/<?php echo $show->chanid, '/', $show->recstartts ?>" title="<?php echo t('Recording Details') ?>"
            ><img src="<?php echo $show->thumb_url(100,0) ?>"></a>
<?php   }
        else {
            echo ' -noimg">';
        }
?>
        <a class="-download"
            href="<?php echo video_url($show, true) ?>" title="<?php echo t('ASX Stream') ?>"
            ><img src="<?php echo skin_url ?>/img/play_sm.png"></a>
        <a class="-download"
            href="<?php echo $show->url ?>" title="<?php echo t('Direct Download') ?>"
            ><img src="<?php echo skin_url ?>/img/video_sm.png"></a>
        </td>
    <td class="-title"><?php echo '<a href="', root, 'tv/detail/', $show->chanid, '/', $show->recstartts, '"'
                    .($_SESSION['recorded_pixmaps'] ? '' : " name=\"$row\"")
                    .' title="', t('Recording Details'), '"'
                    .'>'.$show->title.'</a>' ?></td>
    <td class="-subtitle"><?php echo '<a href="', root, 'tv/detail/', $show->chanid, '/', $show->recstartts, '"'
                    .' title="', t('Recording Details'), '"'
                    .'>'.$show->subtitle.'</a>' ?></td>
    <td class="-programid"><?php echo $show->programid ?></td>
    <td class="-airdate"><?php echo strftime($_SESSION['date_recorded'], $show->starttime) ?></td>
    <td class="-channum"><?php echo $show->channel->channum, ' - ', $show->channel->name ?></td>
<?php
    if ($recgroup_cols)
        echo "    <td class=\"-recgroup\">$show->recgroup</td>\n";
?>
    <td class="-length"><?php echo nice_length($show->length) ?></td>
    <td class="-filesize"><?php echo nice_filesize($show->filesize) ?></td>
    <td class="-commands commands" rowspan="2"><?php
        if ($show->is_recording) {
            echo '<a href="', root, 'tv/detail/', $show->chanid, '/', $show->recstartts, '">',
                 t('Still Recording: Edit'),
                 "</a>\n        ";
        }
        if ($show->can_delete) {
        ?><a onclick="javascript:confirm_delete(<?php echo $row ?>, false)"
            title="<?php echo html_entities(t('Delete $1', preg_replace('/: $/', '', $show->title.': '.$show->subtitle))) ?>"
            ><?php echo t('Delete') ?></a>
        <a onclick="javascript:confirm_delete(<?php echo $row ?>, true)"
            title="<?php echo html_entities(t('Delete and rerecord $1', preg_replace('/: $/', '', $show->title.': '.$show->subtitle))) ?>"
            ><?php echo t('Delete + Rerecord') ?></a>
<?php   } ?>
        </td>
</tr><tr id="statusrow_<?php echo $row ?>" class="recorded">
    <td colspan="4" valign="top"><?php echo $show->description ?></td>
    <td colspan="<?php echo 3 + $recgroup_cols ?>" class="-progflags"><?php
        // Auto expire is interactive
            echo '<a onclick="set_autoexpire(', $row, ')" class="_autoexpire">',
                 '<img id="autoexpire_', $row, '" src="', skin_url, '/img/flags/';
            if ($show->auto_expire)
                echo 'autoexpire.png" title="', t('Click to disable Auto Expire'), '"';
            else
                echo 'no_autoexpire.png" title="', t('Click to enable Auto Expire'), '"';
            echo '></a>';
        // The rest of the flags are just for display
            if ($show->closecaptioned)
                echo '<img src="'.skin_url.'/img/flags/cc.png" title="'.t('Closed Captioning').'">';
            if ($show->stereo)
                echo '<img src="'.skin_url.'/img/flags/stereo.png" title="'.t('Stereo').'">';
            if ($show->hdtv)
                echo '<img src="'.skin_url.'/img/flags/hd.png" title="'.t('HD').'">';
            if ($show->has_commflag)
                echo '<img src="'.skin_url.'/img/flags/commflagged.png" title="'.t('Commercials Flagged').'">';
            if ($show->has_cutlist)
                echo '<img src="'.skin_url.'/img/flags/cutlist.png" title="'.t('Has Cutlist').'">';
            if ($show->bookmark)
                echo '<img src="'.skin_url.'/img/flags/bookmark.png" title="'.t('has Bookmark').'">';
            if ($show->is_watched)
                echo '<img src="'.skin_url.'/img/flags/watched.png" title="'.t('Watched').'">';
        ?></td>
</tr><?php
        $prev_group = $cur_group;
    // Keep track of how many shows are visible in each section
        $row_count[$section]++;
    // Keep track of which shows are in which section
        $row_section[$row] = $section;
    // Increment row last
        $row++;
    }
?>

</table>

<script language="JavaScript" type="text/javascript">
<?php
    foreach ($row_count as $count) {
        echo 'rowcount.push(['.escape($count)."]);\n";
    }
    foreach ($row_section as $section) {
        echo 'rowsection.push(['.escape($section)."]);\n";
    }
    foreach($Program_Titles as $title => $count) {
        echo 'titles['.escape($title).'] = '.escape($count).";\n";
    }
    foreach($Groups as $recgroup => $count) {
        echo 'groups['.escape($recgroup).'] = '.escape($count).";\n";
    }
?>
</script>

<?php
    echo '<p align="right" style="padding-right: 75px">'
        .t('$1 programs, using $2 ($3) out of $4 ($5 free).',
           '<span id="programcount">'.t($Total_Programs).'</span>',
           '<span id="diskused">'.nice_filesize($Total_Used).'</span>',
           '<span id="totaltime">'.nice_length($Total_Time).'</span>',
           '<span id="disksize">'.nice_filesize(disk_size).'</span>',
           '<span id="diskfree">'.nice_filesize(disk_size - disk_used).'</span>'
          )
        .'</p>';

// Print the page footer
    require 'modules/_shared/tmpl/'.tmpl.'/footer.php';


