<?php
/**
 * @url         $URL: svn+ssh://svn.mythtv.org/var/lib/svn/trunk/mythplugins/mythweb/modules/tv/recorded.php $
 * @date        $Date: 2008-03-26 12:46:06 -0700 (Wed, 26 Mar 2008) $
 * @version     $Revision: 16806 $
 * @author      $Author: xris $
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  TV
/**/

    if (!isset($_REQUEST['group']) || strtolower($_REQUEST['group']) == 'all')
        $group = 'All';
    else
        $group = $_REQUEST['group'];

    $sh = $db->query('SELECT IF( IFNULL(recorded.subtitle, "") = "",
                                 recorded.title,
                                 recorded.subtitle) AS subtitle,
                             recorded.chanid,
                             UNIX_TIMESTAMP(recorded.starttime)
                        FROM recorded
                       WHERE recorded.recgroup LIKE ?
                         AND recorded.title       = ?
                    ORDER BY recorded.originalairdate ASC
                    ',
                    ($group == 'All' ? '%' : $group),
                    $_REQUEST['title']
                    );
    while ($subtitles = $sh->fetch_row())
        $SubTitles[] = $subtitles;

    $Page_Previous_Location = root.'/tv/list_titles_in_group?group='.htmlentities($group);
    $Page_Previous_Location_Name = $group;
    $Page_Title_Short = $_REQUEST['title'];

// Load the class for this page
    require_once tmpl_dir.'list_shows_in_title_and_group.php';
