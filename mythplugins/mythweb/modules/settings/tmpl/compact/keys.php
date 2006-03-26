<?php
/**
 * Configure MythTV Key Bindings
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
    $page_title = 'MythWeb - '.t('Configure Keybindings');

// Print the page header
    require 'modules/_shared/tmpl/'.tmpl.'/header.php';

?>
<table align="center" width="40%" cellspacing="2" cellpadding="2">
<tr>
    <td width="50%" class="command command_border_l command_border_t command_border_b command_border_r" align="center">
        <form class="form" method="get" action="<?php echo root ?>settings/keys">
        <table width="100%" border="0" cellspacing="0" cellpadding="2">
        <tr>
            <td nowrap align="center"><?php echo t('Edit keybindings on') ?>:&nbsp;&nbsp;</td>
                <td><select name="host"><?php
                    foreach ($Hosts as $availhost) {
                        echo '<option value='.$availhost['hostname'];
                        if ($availhost['hostname'] == $use_host)
                            echo ' SELECTED';
                        echo '>'.$availhost['hostname'].'</option>';
                    }
                    ?></select>
                    </td>
            <td align="center"><input type="submit" class="submit" value="<?php echo t('Set Host') ?>"></td>
        </tr>
        </table>
        </form>
        </td>
</tr>
</table>

<div class="error" style="width: 80%; margin: 1em auto; padding: 5px">
    <ul>
        <li>This settings page has absolutely no error checking yet. You can easily
            screw things up if you're not careful.</li>
        <li>JumpPoints are globally active.  If you set a keybinding for a JumpPoint
            that is the same as one defined in the Keybindings section, the
            JumpPoint will override the keybinding.</li>
        <li>You probably want to use function keys or keys combined with a modifier
            (alt, control) for JumpPoints, otherwise you may run into some problems.</li>
        <li>Changes to keybindings/jumppoints requires a restart of the affected
            mythfrontend for now.  This will change in a future release.</li>
    </ul>
</div>

<form class="form" method="post" action="<?php echo root ?>settings/keys">

<table border="0" cellpadding="4" cellspacing="2" class="list small" align="center">
<tr class="menu large" align="center">
    <td colspan="3"><?php echo t('JumpPoints Editor') ?></td>
</tr><tr class="menu" align="center">
    <td width="33%"><?php echo t('Destination') ?></td>
    <td width="33%"><?php echo t('Description') ?></td>
    <td width="33%"><?php echo t('Key bindings') ?></td>
</tr><?php
    foreach ($Jumps as $jumppoint) {
?><tr class="settings" align="center">
    <td><?php echo html_entities($jumppoint['destination']) ?></td>
    <td><?php echo html_entities($jumppoint['description']) ?></td>
    <td><input type="text" size="35"
               name="jump:<?php echo $jumppoint['destination'].':'.$use_host ?>"
               value="<?php echo str_replace('\\\\', '\\', html_entities($jumppoint['keylist'])) ?>"></td>
</tr><?php
    }
?>
</table>

<p></p>

<table border="0" cellpadding="4" cellspacing="2" class="list small" align="center">
<tr class="menu large" align="center">
        <td colspan="4"><?php echo t('Keybindings Editor') ?></td>
</tr><tr class="menu" align="center">
        <td width="15%"><?php echo t('Context')      ?></td>
        <td width="25%"><?php echo t('Action')       ?></td>
        <td width="40%"><?php echo t('Description')  ?></td>
        <td width="20%"><?php echo t('Key bindings') ?></td>
</tr><?php
                foreach ($Keys as $keyb) {
?><tr class="settings" align="center">
        <td><?php echo html_entities($keyb['context'])     ?></td>
        <td><?php echo html_entities($keyb['action'])      ?></td>
        <td><?php echo html_entities($keyb['description']) ?></td>
        <td><input type="text" size="25"
                   name="key:<?php echo $keyb['context'].':'.$keyb['action'].':'.$use_host ?>"
                   value="<?php echo str_replace('\\\\', '\\', html_entities($keyb['keylist'])) ?>"></td>
</tr>
<?php
                }
?>
</table>

<p align="center">
<input type="submit" name="save" value="<?php echo t('Save') ?>">
</p>

</form>
<?php

// Print the page footer
    require 'modules/_shared/tmpl/'.tmpl.'/footer.php';

