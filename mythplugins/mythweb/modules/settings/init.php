<?php
/**
 * Initialization routines for the MythWeb settings module
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

// The settings module is always enabled.
    $Modules['settings'] = array('path'        => 'settings',
                                 'name'        => t('Settings'),
                                 'description' => t('settings'),
                                 'links'       => array('channels'  => t('MythTV channel info'),
                                                        'keys'      => t('MythTV key bindings'),
                                                        'mythweb'   => t('MythWeb settings'),
                                                       ),
                                );
