<?php
/**
 * Configure MythWeb Session info
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
?>

<form class="form" method="post" action="<?php echo form_action ?>">

<table border="0" cellspacing="0" cellpadding="0">
<tr>
    <th><?php echo t('Prefer Channum') ?>:</th>
    <td><input class="radio" type="checkbox" name="prefer_channum"
         title="Prefer channel number over callsign."
         <?php if ($_SESSION['prefer_channum']) echo ' CHECKED' ?>></td>
</tr><tr>
    <th><?php echo t('MythVideo Dir') ?>:</th>
    <td><input type="text" size="36" name="mythvideo_dir"
        value="<?php echo html_entities(setting('VideoStartupDir', hostname))?>"></td>
</tr><tr class="-sep">
    <th><?php echo t('MythVideo Artwork Dir') ?>:</th>
    <td><input type="text" size="36" name="video_artwork_dir"
        value="<?php echo html_entities(setting('VideoArtworkDir', hostname))?>"></td>
</tr><tr>
    <td align="right"><input type="reset"  class="submit" value="<?php echo t('Reset') ?>"></td>
    <td align="center"><input type="submit" class="submit" name="save" value="<?php echo t('Save') ?>"></td>
</tr>
</table>

</form>

