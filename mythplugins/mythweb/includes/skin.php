<?php
/**
 * This file is part of MythWeb, a php-based interface for MythTV.
 * See http://www.mythtv.org/ for details.
 *
 * @url         $URL$
 * @date        $Date$
 * @version     $Revision$
 * @author      $Author$
 * @license     GPL
 *
 * @package     MythWeb
 *
/**/

/*  So the order we do things is the following:
   If the user is attempting to reset the template, use that value
   Else the cookie is set, use it's value
   Else try to auto-detect mobile or non-js/image browser
   If that fails, use the stored session default
*/

    $tmpl = $_SESSION['tmpl'];
    $skin = $_SESSION['skin'];

    if (isset($_REQUEST['RESET_TMPL']))
        $tmpl = _or($_REQUEST['RESET_TMPL'], 'default');
    elseif (isset($_REQUEST['RESET_TEMPLATE']))
        $tmpl = _or($_REQUEST['RESET_TEMPLATE'], 'default');
    elseif (isset($_COOKIE['mythweb_tmpl']))
        $tmpl = $_COOKIE['mythweb_tmpl'];
    elseif (isMobileUser()) {
        $tmpl = 'wap';
        $skin = 'wap';
    }
    elseif (preg_match('/^(Lynx|ELinks)/i', $_SERVER['HTTP_USER_AGENT']))
        $tmpl = 'lite';

    if (isset($_REQUEST['RESET_SKIN']))
        $skin = _or($_REQUEST['RESET_SKIN'], 'default');

// Verify that the paths are valid
    if (!file_exists(modules_path.'/_shared/tmpl/'.$tmpl.'/welcome.php'))
        $tmpl = 'default';

    if (!file_exists('skins/'.$skin.'/img/'))
        $skin = 'default';

// Store the template in a cookie
    setcookie('mythweb_tmpl', $tmpl, 2147483647, root);

// We do want to over-ride the template for some paths.
// We do this after setting because certain templates
// Should never be stored as the normal end-user view
    if ($Path[0] == 'rss' || $Path[0] == 'ical')
        $tmpl = $Path[0];

// And now we define the paths
    define('skin', $skin);
    define('skin_dir', 'skins/'.skin);
    define('skin_url', root.skin_dir.'/');

    define('tmpl', $tmpl);
    define('tmpl_dir', 'modules/'.module.'/tmpl/'.tmpl.'/');
