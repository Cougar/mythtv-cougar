<?php
/***                                                                        ***\
    program_listing.php                      Last Updated: 2004.10.25 (jbuckshin)

    This file defines a theme class for the program listing section.
    It must define several methods, some of which have specific
    parameters.   documentation will be added someday.
\***                                                                        ***/


class Theme_program_listing extends Theme {

    function print_header($start_time, $end_time) {

        // Print the main page header
        parent::print_header('MythWeb - '._LANG_LISTINGS);
        parent::print_menu_content();
    }

    function print_page(&$Channels, &$Timeslots, $list_starttime, $list_endtime) {

        // Display the listing page header
        $this->print_header($list_starttime, $list_endtime);

        if ((! isset($_GET['listbytime'])) && (! isset($_GET['listbychannum'])) && (! isset($_GET['listbycallsign']))) {
?>
<p>
View listings by<br/>
<a href="#card2">Time</a> or <br/><a href="program_listing.php?listbychannum=y">Channel Number</a> or <br/><a href="program_listing.php?listbycallsign">Call Sign</a>
</p>
</card>
<card id="card2" title="By Time">
<p>
<a href="program_listing.php?listbytime=y">Listings Now</a><br/>
<?php
            $seconds_in_day = 60 * 60 * 24;

            // Find out how many days into the future we should bother checking
            $result = mysql_query('SELECT TO_DAYS(max(starttime)) - TO_DAYS(NOW()) FROM program')
            or trigger_error('SQL Error: '.mysql_error(), FATAL);

            list($max_days) = mysql_fetch_row($result);
            mysql_free_result($result);

            // Print out the list
            for ($i=-1;$i<=$max_days;$i++) {
                $time = mktime(0,0,0, date('m'), date('d') + $i, date('Y'));
                $date = date("Ymd", $time);
                echo '<a href="program_listing.php?listbytime=y&amp;date='.$date.'">'.date("D n/j", $time)."</a><br/>\n";
            }
?>
</p>
</card>
<card id="card3" title="By Channel">
<p>
<do type="accept" label="Go">
<go href="channel_detail.php" method="get">
<postfield name="chanid" value="$(chanid)"/>
<postfield name="time" value="<?php echo $start_time; ?>"/>
</go>
</do>
Enter Channel:<br/>
(use chanid)
<input name="chanid" type="text" format="N*" size="4"/>

<?php

        }
        // Go through each channel and load/print its info - use references to avoid "copy" overhead

        $bytime = $_GET['listbytime'];
        $bynum = $_GET['listbychannum'];
        $bycall = $_GET['listbycallsign'];

        if (isset($bynum)) {
            echo '<p>';
            $row=0;
            $page_size=20;
            $page = $_GET['page'];

            if (! isset($page)) $page=1;
            $page_start = ($page - 1) * $page_size + 1;
            $page_end = $page_start + $page_size;

            if ($page != 1) echo '<a href="program_listing.php?listbychannum=y&amp;page='.($page - 1).$prev_query.'">&lt; prev</a>';
            if (($page * $page_size) < count($Channels)) echo ' <a href="program_listing.php?listbychannum=y&amp;page='.($page + 1).$prev_query.'">next &gt;</a>';

            foreach (array_keys($Channels) as $key) {

                $row++;

                // pager code
                if (($row < $page_start) || ($row >= $page_end)) {
                    continue;
                }

                // Ignore channels with no number
                if (strlen($Channels[$key]->channum) < 1) {
                    continue;
                }

                echo "<a href='channel_detail.php?chanid=".$Channels[$key]->chanid."'>".$Channels[$key]->channum."</a> ";
                // Count this channel
                $channel_count++;
            }
        } elseif (isset($bycall)) {

            echo '<p>';

            $row=0;
            $page_size=20;
            $page = $_GET['page'];

            if (! isset($page)) $page=1;
            $page_start = ($page - 1) * $page_size + 1;
            $page_end = $page_start + $page_size;

            if ($page != 1) echo '<a href="program_listing.php?listbycallsign=y&amp;page='.($page - 1).$prev_query.'">&lt; prev</a>';
            if (($page * $page_size) < count($Channels)) echo ' <a href="program_listing.php?listbycallsign=y&amp;page='.($page + 1).$prev_query.'">next &gt;</a>';

            foreach (array_keys($Channels) as $key) {

                $row++;

                // pager code
                if (($row < $page_start) || ($row >= $page_end)) {
                    continue;
                }

                // Ignore channels with no number
                if (strlen($Channels[$key]->channum) < 1) {
                    continue;
                }

                echo "<a href='channel_detail.php?chanid=".$Channels[$key]->chanid."'>".$Channels[$key]->callsign."</a> ";

                // Count this channel
                $channel_count++;
            }
            
        }
        if (isset($bytime)) {
?>
<do type="accept" label="Go">
<go href="program_listing.php" method="get">
<postfield name="listbytime" value="y"/>
<postfield name="hour" value="$(hour)"/>
<postfield name="date" value="<?php echo date("Ymd", $list_starttime); ?>"/>
</go>
</do>
<?php
            echo "<p>\n";
            if (is_numeric($bytime)) {
                $list_starttime = mktime($bytime,0,0, date('g', $list_starttime), date('j', $list_starttime), date('Y', $list_starttime));
            }

            $row=0;
            $page_size=15;
            $page = $_GET['page'];

            if (! isset($page)) $page=1;
            $page_start = ($page - 1) * $page_size + 1;
            $page_end = $page_start + $page_size;

            $prev_time = $_GET['time'];
            $prev_date = $_GET['date'];
            $prev_hour = $_GET['hour'];
            $prev_query = "";
            if (isset($prev_time)) {
                $prev_query = "&amp;time=".$prev_time;
            } elseif (isset($prev_date)) {
                $prev_query = "&amp;date=".$prev_date;
                if (isset($prev_hour)) {
                    $prev_query.="&amp;hour=".$prev_hour;
                }
            }

            if ($page != 1) echo '<a href="program_listing.php?listbytime=y&amp;page='.($page - 1).$prev_query.'">&lt; prev</a>';
            if (($page * $page_size) < count($Channels)) echo ' <a href="program_listing.php?listbytime=y&amp;page='.($page + 1).$prev_query.'">next &gt;</a>';
            echo "<br/>";

            foreach (array_keys($Channels) as $key) {

                $row++;

                // pager code
                if (($row < $page_start) || ($row >= $page_end)) {
                    continue;
                }

                // Ignore channels with no number
                if (strlen($Channels[$key]->channum) < 1) {
                    continue;
                }

                // Count this channel
                $channel_count++;

                // Grab the reference
                $channel = &$Channels[$key];

                // modify the end time so we are only looking at a 1 hour slice,
                // this is critical to insure minimal data returned.
                $channel->display_programs($list_starttime, $list_endtime);

                // Cleanup is a good thing
                unset($channel);
            }

            if ($page != 1) echo '<a href="program_listing.php?listbytime=y&amp;page='.($page - 1).$prev_query.'">&lt; prev</a>';
            if (($page * $page_size) < count($Channels)) echo ' <a href="program_listing.php?listbytime=y&amp;page='.($page + 1).$prev_query.'">next &gt;</a>';

            echo '<br/>'._LANG_JUMP_TO.' '._LANG_HOUR.':';
            echo '<input type="text" name="hour" format="N*" size="2" emptyok="true"/>';
        }

        echo '</p></card>';

        // Display the listing page footer
        $this->print_footer();
    }

    /*
        print_footer:
        This function prints the footer portion of the page specific to the program listing
    */
    function print_footer() {
        // Print the main page's footer
        parent::print_footer();
    }

    /*
        print_channel:

    */
    function print_channel($channel, $start_time, $end_time) {
?>
<a href="channel_detail.php?chanid=<?php echo $channel->chanid?>&amp;time=<?php echo $start_time?>">
<?php echo prefer_channum ? $channel->channum : $channel->callsign?>&nbsp;
<?php echo prefer_channum ? $channel->callsign : $channel->channum?></a><br/>
<?php
    }

    function print_program($program, $timeslots_used, $starttime) {

        echo $program->channel->callsign."  "; //chanid." ";
        echo strftime($_SESSION['time_format'], $program->starttime);
        echo ' - <a href="program_detail.php?chanid='.$program->chanid.'&amp;starttime='.$program->starttime.'">';
        echo htmlspecialchars($program->title);
        echo "</a><br/>\n";

    }

    function print_nodata($timeslots_left) {
    }
}

?>
