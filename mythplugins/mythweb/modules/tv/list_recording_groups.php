<?php
/**
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  TV
/**/

    $sh = $db->query('SELECT DISTINCT recorded.recgroup
                        FROM recorded');
    while ($group = $sh->fetch_col())
        $Groups[] = $group;

    $Groups[] = 'All';
    $Groups[] = 'iPod viewable';

    $Page_Previous_Location = root.'tv/submenu';
    $Page_Previous_Location_Name = 'Television';
    $Page_Title_Short = 'Rec Groups';

// Load the class for this page
    require_once tmpl_dir.'list_recording_groups.php';
