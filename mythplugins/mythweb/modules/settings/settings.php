<?php
/**
 * Configure MythTV Settings table
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

// Specific host?
    isset($_GET['host']) or $_GET['host'] = $_GET['host'];
    if (empty($_GET['host']))
        $_GET['host'] = null;

// Save?
    if ($_POST['save']) {
        foreach ($_POST['settings'] as $value => $data) {
            if (is_null($_GET['host']))
                $sh = $db->query('UPDATE settings SET data=?
                                   WHERE value=? AND hostname IS NULL',
                                 $data,
                                 $value
                                );
            else
                $sh = $db->query('UPDATE settings SET data=?
                                   WHERE value=? AND hostname=?',
                                 $data,
                                 $value,
                                 $_GET['host']
                                );
        }
        foreach ($_POST['delete'] as $value => $data) {
            if (!$data)
                continue;
            if (is_null($_GET['host']))
                $sh = $db->query('DELETE FROM settings
                                        WHERE value=? AND hostname IS NULL',
                                 $value
                                );
            else
                $sh = $db->query('DELETE FROM settings
                                        WHERE value=? AND hostname=?',
                                 $value,
                                 $_GET['host']
                                );
        }
    }

// Load all of the known mythtv hosts
    $Hosts = array();
    $sh = $db->query('SELECT DISTINCT hostname FROM settings ORDER BY hostname');
    while (list($host) = $sh->fetch_row()) {
        if (empty($host))
            continue;
        $Hosts[] = $host;
    }
    $sh->finish();

// Load all of the settings for the requested host
    $Settings = array();
    if (is_null($_GET['host']))
        $sh = $db->query('SELECT value, data
                            FROM settings
                           WHERE !hostname OR hostname IS NULL
                        ORDER BY value');
    else
        $sh = $db->query('SELECT value, data
                            FROM settings
                           WHERE hostname=?
                        ORDER BY value',
                         $_GET['host']);
    while (list($value, $data) = $sh->fetch_row()) {
        $Settings[$value] = $data;
    }
    $sh->finish();

// Load the class for this page
    require_once tmpl_dir.'settings.php';

// Exit
    exit;

