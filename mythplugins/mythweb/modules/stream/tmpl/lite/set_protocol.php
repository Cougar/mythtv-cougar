<?php
/**
 * Configure MythWeb Music protocol preference
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

<div class="error" style="padding: 5px">
<?php echo t('settings/stream: protocol') ?>
</div>

<table border="0" cellspacing="0" cellpadding="0">
<tr class="x-sep">
    <th><label for="force_http"><?php echo t('Force HTTP for streams') ?>:</label></th>
    <td><input type="checkbox" id="force_http" name="force_http" value="1"<?php if ($_SESSION['stream']['force_http']) echo ' CHECKED' ?>></td>
</tr><tr class="x-sep">
    <th><label for="force_http_port"><?php echo t('Force HTTP/HTTPS port for streams'); ?>:</label></th>
    <td><input type="textbox" id="force_http_port" name="force_http_port" value="<?php echo $_SESSION['stream']['force_http_port'] ?>"></td>
</tr><tr>
    <td align="center"><input type="reset" class="submit" value="<?php echo t('Reset') ?>"></td>
    <td align="center"><input type="submit" class="submit" name="save" value="<?php echo t('Save') ?>"></td>
</tr>
</table>

</form>