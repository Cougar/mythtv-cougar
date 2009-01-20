<?php
/**
 * Welcome page description of the Settings module.
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

// Open with a div and an image
    echo '<div id="info_settings">',
         '<p>',
         '<a href="', root, Modules::getModuleProperity('settings', 'path'), '">',
         '<img src="', skin_url, '/img/settings.png" class="module_icon" border="0" />',
         '<h2>', Modules::getModuleProperity('settings', 'name'), '</h2></a>',
         '</p>',

// Print a basic overview of what this module does
         t('welcome: settings'),

// Next, print a list of possible subsectons
         '<ul>';
    foreach (Modules::getModuleProperity('settings', 'links') as $link => $name) {
        echo '    <li><a href="', root, Modules::getModuleProperity('settings', 'path'), '/', $link, '">', html_entities($name), "</a></li>\n";
    }
    echo '</ul>',

// Close the div
         "</div>\n";
