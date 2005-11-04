<?php
/***                                                                        ***\
    program_detail.php                       Last Updated: 2003.08.22 (xris)

    This file defines a theme class for the program details section.
    It must define one method.   documentation will be added someday.

\***                                                                        ***/

#class theme_program_detail extends Theme {
class Theme_program_detail extends Theme {

    function print_page(&$program, &$schedule, &$channel) {
    // Print the main page header
        parent::print_header("MythWeb - Program Detail:  $program->title");
    // Print the page contents
?>
<a href="channel_detail.php?chanid=<?=$this_channel->chanid?>" >
<?=prefer_channum ? $this_channel->channum : $this_channel->callsign?> &nbsp;
<?=prefer_channum ? $this_channel->callsign : $this_channel->channum?></a><br>

<?=$program->title?><BR>
<?=date('D m/d/y', $program->starttime)?><br>
<?=date('g:i A', $program->starttime)?> to <?=date('g:i A', $program->endtime)?> (<?=(int)($program->length/60)?> minutes)<BR>
                <?
                if ($program->previouslyshown)
                    echo '(Rerun) ';
//              if ($program->category_type == 'movie')
//                  echo " (<a href=\"http://www.imdb.com/Find?select=Titles&for=" . urlencode($program->title) . "\">Search IMDB</a>)";
//              else
//                  echo " (<a href=\"http://www.google.com/search?q=" . urlencode($program->title) . "\">Search Google</a>)";
                ?>
        <? if (strlen($program->subtitle)) { ?>
            Episode: <b><?=$program->subtitle?></b><br>
        <? }
           if (strlen($program->description)) {?>
                Description: <?=$program->description?><br>
        <? } ?>
        <? if (strlen($program->category)) {?>
                Category: <?=$program->category?><br>
        <? }
           if (strlen($program->airdate)) {?>
                Orig. Airdate: <?=$program->airdate?><br>
        <? }
           if (strlen($program->rating)) {?>
                <?=strlen($program->rater) > 0 ? "$program->rater " : ''?>Rating: <?=$program->rating?><br>
        <?
           if (strlen($program->starstring) > 0)
                    echo ", $program->starstring";
                ?><br>
        <? } ?>

        <form name="program_detail" method="post" action="program_detail.php?<?php
            if ($_GET['recordid'])
                echo 'recordid='.urlencode($_GET['recordid']);
            else
                echo 'chanid='.urlencode($_GET['chanid']).'&starttime='.urlencode($_GET['starttime'])
            ?>">
        <center>Schedule Options:</center>
                    <input type="radio" class="radio" name="record" value="record_never" id="record_never"<?=
                    $schedule->recordid ? '' : ' CHECKED'?>></input>
  <a><?php 
    if ($schedule->recordid) 
       echo 'Cancel';
    else
       echo 'Don\'t record';
      ?> 
  </a><br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_once?>" id="record_once"<?=
        $schedule->type == rectype_once ? ' CHECKED' : ''?>></input>
        <a>Record this showing</a><br>
    <input type="radio" class="radio" name="record" value="<?echo rectype_daily ?>" id="record_daily"<?=
        $schedule->type == rectype_daily ? ' CHECKED' : ''?>></input>
        <a>Record every day</a> at this time<br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_weekly?>" id="record_weekly"<?=
        $schedule->type == rectype_weekly ? ' CHECKED' : ''?>></input>
        <a>Record every week</a> at this time<br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_findone ?>" id="record_findone"<?=
        $schedule->type == rectype_findone ? ' CHECKED' : ''?>></input>
        <a>Find one episode</a><br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_finddaily ?>" id="record_finddaily"<?=
        $schedule->type == rectype_finddaily ? ' CHECKED' : ''?>></input>
        <a>Find one episode every day</a><br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_findweekly ?>" id="record_findweekly"<?=
        $schedule->type == rectype_findweekly ? ' CHECKED' : ''?>></input>
        <a>Find one episode every week</a><br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_channel?>" id="record_channel"<?=
        $schedule->type == rectype_channel ? ' CHECKED' : ''?>></input>
        <a>Always record on this channel</a><br>
    <input type="radio" class="radio" name="record" value="<? echo rectype_always ?>" id="record_always"<?=
        $schedule->type == rectype_always ? ' CHECKED' : ''?>></input>
        <a>Always record on any channel</a><br>
                <br>
  Recording Profile<br>
  <?php profile_select($schedule->profile) ?>
                        <br>
  <input type="checkbox" class="radio" name="autocommflag"<?php if ($schedule->autocommflag) echo ' CHECKED' ?> value="1" />
       <a><? echo t('Auto-flag commercials') ?></a><br/>
  <input type="checkbox" class="radio" name="autoexpire"<?php if ($schedule->autoexpire) echo ' CHECKED' ?> value="1" />
       <a><? echo t('Auto-expire recordings') ?></a><br/>
  <input type="checkbox" class="radio" name="maxnewest"<?php if ($schedule->maxnewest) echo ' CHECKED' ?> value="1" />
       <a><? echo t('Record new and expire old') ?></a><br/>
  <input type="checkbox" class="radio" name="inactive"<?php if ($schedule->inactive) echo ' CHECKED' ?> value="1" />
       <a><? echo t('Inactive') ?></a><br/>
  <?php echo t('No. of recordings to keep') ?>:
  <input type="input" class="quantity" name="maxepisodes" value="<?php echo htmlentities($schedule->maxepisodes) ?>" size="2"/><br/>
  <?php echo t('Start Early') ?>:
  <input type="input" class="quantity" name="startoffset" value="<?php echo htmlentities($schedule->startoffset) ?>" size="2"/>
       <?php echo t('minutes') ?><br/>
  <?php echo t('End Late') ?>:
  <input type="input" class="quantity" name="endoffset" value="<?php echo htmlentities($schedule->endoffset) ?>" size="2"/>
  <?php echo t('minutes') ?><br/>
                    <center><input type="submit" class="submit" name="save" value="Update Settings"></center>
                <br>

        </form>

<?
    // Print the main page footer
        parent::print_footer();
    }

}

?>
