<?php
/**
 * Get a pixmap
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

    $chanid     = $Path[2];
    $starttime  = $Path[3];
    $width      = $Path[4];
    $height     = $Path[5];
    $seconds_in = $Path[6];

    if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
        if (strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']) == $starttime) {
            header("HTTP/1.1 304 Not Modified");
            exit;
        }
    }

    $data = Program::get_preview_pixmap($chanid, $starttime, $width, $height, $seconds_in);
    if (strlen($data)) {
        header('Pragma: public', true);
        header('Content-Type: image/png');
        header("Content-Length: ".strlen($data));
        header("Cache-Control: max-age=".(7*24*60*60*60).", public");
        header("Last-Modified: ".gmdate("D, d M Y H:i:s", $starttime)." GMT");
        header("Expires: ".gmdate("D, d M Y H:i:s", $starttime + (7*24*60*60*60))." GMT");

        echo $data;
    }
    else
        header("Status: 404 Not Found");

    exit;
