<?php
/**
 * Handler for all MythWeb TV routines
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

// Make sure the image cache path exists and is writable
    if (!is_dir('data/tv_icons') && !mkdir('data/tv_icons', 0755)) {
        custom_error('Error creating data/tv_icons: Please check permissions on the data directory.');
        exit;
    }
    if (!is_writable('data/tv_icons')) {
        $process_user = posix_getpwuid(posix_geteuid());
        custom_error('data/tv_icons directory is not writable by '.$process_user['name'].'. Please check permissions.');
        exit;
    }

/**
 * @global  array   $GLOBALS['Categories']
 * @name    $Categories
/**/
    global $Categories;
    $Categories = array();

// Load the tv categories
    if (file_exists(modules_path.'/_shared/lang/'.$_SESSION['language'].'.cat'))
        load_tv_categories(modules_path.'/_shared/lang/'.$_SESSION['language'].'.cat');
    else
        load_tv_categories(modules_path.'/_shared/lang/English.cat');

// Two categories that don't need regex matches, but do need translation
    $Categories['Unknown'] = array(t('Unknown'));
    $Categories['movie']   = array(t('movie'));

// Don't forget to sort
    ksort($Categories);

// Load the tv-related libraries
    require_once 'includes/channels.php';
    require_once 'includes/programs.php';
    require_once 'includes/recording_schedules.php';

// Restore the last used path?
    if (empty($Path[1]) && is_array($_SESSION['tv']['last']))
        array_splice($Path, 1, count($Path), $_SESSION['tv']['last']);

// Flash player?
    if ($Path[1] == 'flvplayer.swf') {
        header('Content-Type: application/x-shockwave-flash');
        readfile('modules/tv/flvplayer.swf');
        exit;
    }

// Unknown section?  Use the default
    if (!in_array($Path[1], array('recording_detail', 'detail', 'channel', 'search', 'movies'))
            && empty($Modules['tv']['links'][$Path[1]]))
        $Path[1] = 'list';

// Keep track of this path for the next visit
    $_SESSION['tv']['last'] = array_slice($Path, 1);

// Show the requested section
    require_once 'modules/tv/'.$Path[1].'.php';

/**
 * Load translation file for tv category names and regular expressions
 *
 * @param string $path The path to the translation file
/**/
    function load_tv_categories($path) {
        $file = file_get_contents($path);
    // Error?
        if ($file === false)
            trigger_error("Failed to open tv category file:  $path", FATAL);
    // Parse the file
        global $Categories;
        foreach (preg_split('/\n(?=\S)/', $file) as $group) {
            list($key, $trans, $regex) = preg_split('/\s*\n\s*/', $group);
        // Cleanup
            if (preg_match('/^["\']/', $key))
                $key = preg_replace('/^(["\'])(.+)\\1$/', '$2', $key);
        // Store
            $Categories[$key] = array($trans, $regex);
        }
    }

