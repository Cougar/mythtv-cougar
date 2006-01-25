<?php
/**
 * Configure MythTV Channel info
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  Settings
 *
/**/

// Set the desired page title
    $page_title = 'MythWeb - '.t('Configure Channels');

// Print the page header
    require_once theme_dir.'/header.php';

    echo t('Please be warned that by altering this table without knowing what you are doing, you could seriously disrupt mythtv functionality.');
?>

<form class="form" method="post" action="<?php echo root ?>settings/channels">

<table width="100%" border="0" cellpadding="4" cellspacing="2" class="list small">
<tr class="menu" align="center">
    <td width="4%"><?php  echo t('delete')        ?></td>
    <td width="4%"><?php  echo t('sourceid')      ?></td>
    <td width="4%"><?php  echo t('xmltvid')       ?></td>
    <td width="5%"><?php  echo t('channum')       ?></td>
    <td width="12%"><?php echo t('callsign')      ?></td>
    <td width="25%"><?php echo t('name')          ?></td>
    <td width="5%"><?php  echo t('freqid')        ?></td>
    <td width="5%"><?php  echo t('finetune')      ?></td>
    <td width="5%"><?php  echo t('videofilters')  ?></td>
    <td width="7%"><?php  echo t('brightness')    ?></td>
    <td width="7%"><?php  echo t('contrast')      ?></td>
    <td width="7%"><?php  echo t('colour')        ?></td>
    <td width="7%"><?php  echo t('hue')           ?></td>
    <td width="5%"><?php  echo t('recpriority')   ?></td>
    <td width="5%"><?php  echo t('commfree')      ?></td>
    <td width="5%"><?php  echo t('visible')       ?></td>
    <td width="5%"><?php  echo t('useonairguide') ?></td>
</tr><?php
    foreach ($Channels as $channel) {
?><tr class="settings" align="center">
    <td><input type="checkbox" name="delete_<?php echo $channel['chanid'] ?>" id="delete_<?php echo $channel['chanid'] ?>" value="true" /></td>
    <td><?php echo html_entities($channel['sourceid']) ?></td>
    <td><input type="text" size="5"  name="xmltvid_<?php      echo $channel['chanid'] ?>" id="xmltvid_<?php      echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['xmltvid'])      ?>" style="text-align: center" /></td>
    <td><input type="text" size="3"  name="channum_<?php      echo $channel['chanid'] ?>" id="channum_<?php      echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['channum'])      ?>" style="text-align: center" /></td>
    <td><input type="text" size="15" name="callsign_<?php     echo $channel['chanid'] ?>" id="callsign_<?php     echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['callsign'])     ?>" /></td>
    <td><input type="text" size="27" name="name_<?php         echo $channel['chanid'] ?>" id="name_<?php         echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['name']) ?>" /></td>
    <td><input type="text" size="4"  name="freqid_<?php       echo $channel['chanid'] ?>" id="freqid_<?php       echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['freqid'])       ?>" style="text-align: center" /></td>
    <td><input type="text" size="3"  name="finetune_<?php     echo $channel['chanid'] ?>" id="finetune_<?php     echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['finetune'])     ?>" style="text-align: center" /></td>
    <td><input type="text" size="3"  name="videofilters_<?php echo $channel['chanid'] ?>" id="videofilters_<?php echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['videofilters']) ?>" style="text-align: center" /></td>
    <td><input type="text" size="6"  name="brightness_<?php   echo $channel['chanid'] ?>" id="brightness_<?php   echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['brightness'])   ?>" style="text-align: center" /></td>
    <td><input type="text" size="6"  name="contrast_<?php     echo $channel['chanid'] ?>" id="contrast_<?php     echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['contrast'])     ?>" style="text-align: center" /></td>
    <td><input type="text" size="6"  name="colour_<?php       echo $channel['chanid'] ?>" id="colour_<?php       echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['colour'])       ?>" style="text-align: center" /></td>
    <td><input type="text" size="6"  name="hue_<?php          echo $channel['chanid'] ?>" id="hue_<?php          echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['hue'])          ?>" style="text-align: center" /></td>
    <td><input type="text" size="2"  name="recpriority_<?php  echo $channel['chanid'] ?>" id="recpriority_<?php  echo $channel['chanid'] ?>" value="<?php echo html_entities($channel['recpriority'])  ?>" style="text-align: center" /></td>
    <td><input type="checkbox" name="commfree_<?php           echo $channel['chanid'] ?>" value="1"<?php if (!empty($channel['commfree']))      echo ' CHECKED' ?> /></td>
    <td><input type="checkbox" name="visible_<?php            echo $channel['chanid'] ?>" value="1"<?php if (!empty($channel['visible']))       echo ' CHECKED' ?> /></td>
    <td><input type="checkbox" name="useonairguide_<?php      echo $channel['chanid'] ?>" value="1"<?php if (!empty($channel['useonairguide'])) echo ' CHECKED' ?> /></td>
</tr><?php
    }
?>
</table>

<p align="center">
<input type="submit" name="save" value="<?php echo t('Save') ?>">
</p>

</form>
<?php

// Print the page footer
    require_once theme_dir.'/footer.php';

