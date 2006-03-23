<?php
/**
 * Print the program list
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
    $page_title = 'MythWeb - ' . t('Program Listing') . ': '.strftime($_SESSION['date_statusbar'], $list_starttime);

// Print the page header
    require_once theme_dir.'/header.php';

?>

<p>
<table align="center" width="90%" cellspacing="2" cellpadding="2">
<tr>
    <td width="50%" align="center"><?php echo t('Currently Browsing:  $1', strftime($_SESSION['date_statusbar'], $list_starttime)) ?></td>
    <td class="command command_border_l command_border_t command_border_b command_border_r" align="center">
        <form class="form" id="program_listing" action="<?php echo root ?>tv/list" method="get">
        <table border="0" cellspacing="0" cellpadding="2">
        <tr>

            <td nowrap align="center"><?php echo t('Jump To') ?>:&nbsp;&nbsp;</td>
            <td align="right"><?php echo t('Hour') ?>:&nbsp;</td>
            <td><select name="hour" style="text-align: right"><?php
                for ($h=0;$h<24;$h++) {
                    echo "<option value=\"$h\"";
                    if ($h == date('H', $list_starttime))
                        echo ' SELECTED';
                    echo '>'.strftime($_SESSION['time_format'], strtotime("$h:00")).'</option>';
                }
                ?></select></td>
            <td align="right"><?php echo t('Date') ?>:&nbsp;</td>
            <td style="vertical-align:middle;" nowrap><img src="<?php echo theme_url ?>img/left.gif" onclick="MoveProgramListing(-1)">
                <select name="date"><?php
            // Find out how many days into the past we should bother checking
                $result = mysql_query('SELECT TO_DAYS(min(starttime)) - TO_DAYS(NOW()) FROM program')
                    or trigger_error('SQL Error: '.mysql_error(), FATAL);
                list($min_days) = mysql_fetch_row($result);
                mysql_free_result($result);
            // Find out how many days into the future we should bother checking
                $result = mysql_query('SELECT TO_DAYS(max(starttime)) - TO_DAYS(NOW()) FROM program')
                    or trigger_error('SQL Error: '.mysql_error(), FATAL);
                list($max_days) = mysql_fetch_row($result);
                mysql_free_result($result);
            // Print out the list
                for ($i=$min_days;$i<=$max_days;$i++) {
                    $time = mktime(0,0,0, date('m'), date('d') + $i, date('Y'));
                    $date = date("Ymd", $time);
                    echo "<option value=\"$date\"";
                    if ($date == date("Ymd", $list_starttime)) echo " selected";
                    echo ">".strftime($_SESSION['date_listing_jump'] , $time)."</option>";
                }
                ?></select>
                <img src="<?php echo theme_url ?>img/right.gif" onclick="MoveProgramListing(+1)"></td>
            <td align="center"><input type="submit" class="submit" value="<?php echo t('Jump') ?>"></td>
        </tr>
        </table>
        </form></td>
</tr>
</table>
</p>

<p>
<table width="100%" border="0" cellpadding="4" cellspacing="2" class="list small">
<?php

        $timeslot_anchor    = 0;
        $channel_count      = 0;
        $displayed_channels = array();

    // Go through each channel and load/print its info - use references to avoid "copy" overhead

        foreach ($Channels as $channel) {
        // Ignore channels with no number
            if (strlen($channel->channum) < 1)
                continue;
        // Ignore invisible channels
            if ($channel->visible == 0) {
                continue;
            }
        // Skip already-displayed channels
            if ($displayed_channels[$channel->channum])
                continue;
            $displayed_channels[$channel->channum] = 1;
        // Display the timeslot bar?
            if ($channel_count % timeslotbar_skip == 0) {
            // Update the timeslot anchor
                $timeslot_anchor++;
?><tr>
    <td class="menu" width="4%" align="right"><a href="<?php echo root ?>tv/list?time=<?php echo $list_starttime - (timeslot_size * num_time_slots) ?>#anchor<?php echo $timeslot_anchor ?>" name="anchor<?php echo $timeslot_anchor ?>"><img src="<?php echo theme_url ?>img/left.gif" border="0" alt="left"></a></td>
<?php
                $block_count = 0;
                foreach ($Timeslots as $time) {
                    if ($block_count++ % timeslot_blocks)
                        continue;
?>
    <td nowrap class="menu" colspan="<?php echo timeslot_blocks ?>" width="<?php echo intVal(timeslot_blocks * 94 / num_time_slots) ?>%" align="center"><a href="<?php echo root ?>tv/list?time=<?php echo $time.'#anchor'.$timeslot_anchor ?>"><?php echo strftime($_SESSION['time_format'], $time) ?></a></td>
<?php
                }
?>
    <td nowrap class="menu" width="2%"><a href="<?php echo root ?>tv/list?time=<?php echo $list_starttime + (timeslot_size * num_time_slots) ?>#anchor<?php echo $timeslot_anchor ?>"><img src="<?php echo theme_url ?>img/right.gif" border="0" alt="right"></a></td>
</tr><?php
            }
        // Count this channel
            $channel_count++;
        // Print the data
?><tr>
    <td align="center" class="menu" nowrap><?php
            if (show_channel_icons === true) {
        ?><table class="small" width="100%" border="0" cellspacing="0" cellpadding="2">
        <tr>
            <td width="50%" align="center" nowrap><a href="<?php echo root ?>tv/channel/<?php echo $channel->chanid, '/', $list_starttime ?>" class="huge"
                                            onmouseover="return wstatus('Details for: <?php echo preg_replace("/([\"'])/", '\\\$1', $channel->channum.' '.$channel->callsign) ?>')"
                                            onmouseout="return wstatus('')"><?php echo prefer_channum ? $channel->channum : $channel->callsign ?></a>&nbsp;</td>
            <td width="50%" align="right"><?php
                if (!empty($channel->icon)) {
                    ?><a href="<?php echo root ?>tv/channel/<?php echo $channel->chanid, '/', $list_starttime ?>"
                        onmouseover="return wstatus('<?php echo t('Details for') ?>: <?php echo preg_replace("/([\"'])/", '\\\$1', $channel->channum.' '.$channel->callsign) ?>')"
                        onmouseout="return wstatus('')"><img src="<?php echo $channel->icon ?>" height="30" width="30"></a><?php
                } else {
                    echo '&nbsp;';
                } ?></td>
        </tr><tr>
            <td colspan="2" align="center" nowrap><a href="<?php echo root ?>tv/channel/<?php echo $channel->chanid, '/', $list_starttime ?>"
                                            onmouseover="window.status='Details for: <?php echo preg_replace("/([\"'])/", '\\\$1', $channel->channum.' '. $channel->callsign) ?>';return true"
                                            onmouseout="window.status='';return true"><?php echo prefer_channum ? $channel->callsign : $channel->channum ?></a></td>
        </tr>
        </table><?php
            } else {
        ?><a href="<?php echo root ?>tv/channel/<?php echo $channel->chanid ?>" class="huge"
            onmouseover="window.status='Details for: <?php echo $channel->channum ?> <?php echo $channel->callsign ?>';return true"
            onmouseout="window.status='';return true"><?php echo prefer_channum ? $channel->channum : $channel->callsign ?><BR>
        <?php echo prefer_channum ? $channel->callsign : $channel->channum ?></a><?php
            }
        ?></td>
<?php
// Let the channel object figure out how to display its programs
    $channel->display_programs($list_starttime, $list_endtime);
?>
    <td>&nbsp;</td>
</tr><?php
        }
?>
</table>
</p>
<?php

// Print the page footer
    require_once theme_dir.'/footer.php';

