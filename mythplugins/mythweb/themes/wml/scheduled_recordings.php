<?php
/***                                                                        ***\
    scheduled_recordings.php                    Last Updated: 2004.10.25 (jbuckshin)

    This file defines a theme class for the scheduled recordings section.
    It must define one method.   documentation will be added someday.

\***                                                                        ***/

class Theme_scheduled_recordings extends Theme {

    function print_page() {

        // Print the main page header
        parent::print_header('MythWeb - '._LANG_SCHEDULED_RECORDINGS);
        parent::print_menu_content();

        // Print the page contents
        global $All_Shows;

// exclude the sorting for this theme, not needed
/*  <a href="scheduled_recordings.php?sortby=title">show</a><br>
    <a href="scheduled_recordings.php?sortby=channum">station</a><br>
    <a href="scheduled_recordings.php?sortby=airdate">air&nbsp;date</a><br>
    <a href="scheduled_recordings.php?sortby=length">length</a><br>
*/
        $page_size=15;
        $page = $_GET['page'];

        if (! isset($page)) $page=1;
        $page_start = ($page - 1) * $page_size + 1;
        $page_end = $page_start + $page_size;

        $group_field = $_GET['sortby'];
        if ($group_field == "") {
            $group_field = "airdate";
        } elseif ( ! (($group_field == "title") || ($group_field == "channum") || ($group_field == "airdate")) ) {
            $group_field = "";
        }

        $row = 0;
        $card_count = 0;
        $group_no = 0;
        $first_group_has_been_shown = 0;
        $has_output = 0;

        foreach ($All_Shows as $show) {

            $row++;

            // Reset the command variable
            $command = '';

            // Which class does this show fall into?
            if ($show->duplicate == 1) {
                $class = 'duplicate';
                $command = '<a href="scheduled_recordings.php?rerecord=yes&title='.urlencode($show->title).'&subtitle='.urlencode($show->subtitle).'&description='.urlencode($show->description).'">Rerecord</a>';
            }
            elseif ($show->conflicting == 1) {
                $class   = 'conflict';
                $command = '<a href="scheduled_recordings.php?record=yes&chanid='.$show->chanid.'&starttime='.$show->starttime.'&endtime='.$show->endtime.'">Record</a>';
            }
            elseif ($show->recording == 0) {
                $class   = 'deactivated';
                $command = '<a href="scheduled_recordings.php?activate=yes&chanid='.$show->chanid.'&starttime='.$show->starttime.'&endtime='.$show->endtime.'">Activate</a>';
            }
            else {
                $class   = 'scheduled';
                #$command = 'Don\'t&nbsp;Record';
                $command = '';
            }

            // Print a dividing row if grouping changes
            if ($group_field == "airdate")
            {
                $cur_group = strftime(generic_date, $show->starttime); 
                $cur_group_detail = date('D', $show->starttime)." ";
            }
            elseif ($group_field == "channum")
            {
                $cur_group = $show->channel->name;
                $cur_group_detail = "";
            }
            elseif ($group_field == "title")
            {
                $cur_group = $show->title;
                $cur_group_detail = "";
            }

            if ( $cur_group != $prev_group && $group_field != '' ) {
                if (!(($row < $page_start) || ($row >= $page_end))) {
                    if (($first_group_has_been_shown == 0) && ($has_ouput == 1)) {
                        echo $last_group_txt;
                        $first_group_has_been_shown = 1;
                    }
                    echo "<p><a href=\"#subcard".$card_count."\">".$cur_group."</a></p>\n";
                    $card_count++;
                    //echo "<p><a href=\"#subcard".$group_no."\">".$cur_group_detail.$cur_group."</a></p>\n";
                       } else {
                    $last_group_txt = "<p><a href=\"#subcard".$group_no."\">".$cur_group_detail.$cur_group."</a></p>\n";
                }
                $group_no++;
            }
            $prev_group = $cur_group;

            if (!(($row < $page_start) || ($row >= $page_end))) {
                $has_output = 1;
                $card_data[$group_no].="<p><b>".htmlspecialchars($show->title)."</b> ".htmlspecialchars($show->subtitle)."<br/>".$show->recstatus."<br/> ".strftime(generic_time, $show->starttime)."<br/><a href=\"program_detail.php?chanid=".$show->chanid."&amp;starttime=".$show->starttime."\">Details</a><br/></p>\n";
            }
        }

        echo "<p>";
        if ($page != 1) echo '<a href="scheduled_recordings.php?page='.($page - 1).$prev_query.'">&lt; prev</a>';
        if (($page * $page_size) < count($All_Shows)) echo '<a href="scheduled_recordings.php?page='.($page + 1).$prev_query.'">next &gt;</a>';

        // end the main card
        echo "</p></card>";

        $group_no=0;
        $row=0;

        foreach ($card_data as $card) {
            if (! isset($card)) {
                next;
            }
            echo "<card id=\"subcard".$group_no."\" title=\"Details\">";
            echo $card;
            echo "</card>\n";
            $group_no++;
        }

        // Print the main page footer
        parent::print_footer();
    }
}

?>
