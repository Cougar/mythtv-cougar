<?php
/**
 * Welcome page description of the status module.
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

// Open with a div and an image
    echo '<div id="info_backend_log" class="hidden">',
         '<img src="', skin_url, '/img/backend_log.png" class="module_icon" alt="">',

// Print a basic overview of what this module does
         t('welcome: backend_log'),

// Close the div
         "</div>\n";
