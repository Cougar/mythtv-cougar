<?php
/***                                                                        ***\
    theme.php                             Last Updated: 2005.01.21 (xris)

    This is the main theme class for the Default MythWeb theme.  It should
    not be instantiated directly, but will most likely contain methods
    called from its child classes via the parent:: construct.
\***                                                                        ***/

class Theme {

    function print_header($page_title = 'MythWeb') {
// Print the appropriate header information
        header("Content-Type: text/html; charset=utf-8");
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>

<head>
    <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">

    <title><?php echo $page_title?></title>

    <link rel="stylesheet" href="<?php echo theme_dir?>style.css" type="text/css" />
    <link rel="stylesheet" href="<?php echo theme_dir?>header.css" type="text/css" />
    <link rel="stylesheet" href="<?php echo theme_dir?>menus.css" type="text/css" />
    <link rel="stylesheet" href="<?php echo theme_dir?>programming.css" type="text/css" />

    <script type="text/javascript" src="js/init.js"></script>
</head>

<body bgcolor="#003060" text="#DEDEDE" link="#3181B4" alink="#CC0000" vlink="#3181B4">

<div id="page_header" class="clearfix">
    <div id="logo_box">
        <a id="mythtv_logo" href="http://www.mythtv.org">
        <img src="<?php echo theme_dir?>img/mythtv-logo.png" width="174" height="48" border="0" alt="MythTV" class="alpha_png">
        </a>
    </div>
    <div id="sections" class="clearfix">
        <a id="tv_link" href="program_listing.php" onmouseover="return help_text('<?php echo t('TV functions, including recorded programs.') ?>')" onmouseout="return help_text()">
            <img src="<?php echo theme_dir ?>img/tv.png" width="48" height="48" class="alpha_png" alt="MythTV"/>
        <a id="music_link" href="mythmusic.php" onmouseover="return help_text('<?php echo t('MythMusic on the web.') ?>')" onmouseout="return help_text()">
            <img src="<?php echo theme_dir ?>img/music.png" width="48" height="48" class="alpha_png" alt="MythMusic" />
        </a>
        <a id="video_link" href="video.php" onmouseover="return help_text('<?php echo t('MythVideo on the web.') ?>')" onmouseout="return help_text()">
            <img src="<?php echo theme_dir ?>img/video.png" width="48" height="48" class="alpha_png" alt="MythVideo" />
        </a>
        <a id="weather_link" href="weather.php" onmouseover="return help_text('<?php echo t('MythWeb Weather.') ?>')" onmouseout="return help_text()">
            <img src="<?php echo theme_dir ?>img/weather.png" width="48" height="48" class="alpha_png" alt="MythWeather" />
        </a>
        <a id="settings_link" href="settings.php" onmouseover="return help_text('<?php echo t('Edit MythWeb and some MythTV settings.') ?>')" onmouseout="return help_text()">
            <img src="<?php echo theme_dir ?>img/settings.png" width="48" height="48" class="alpha_png" alt="<?php echo t('Settings') ?>" />
        </a>
    </div>
    <div id="extra_header">
        <div id="help_wrapper">
            <div id="help_box">
                <div id="help_text_default">
                <?php echo t('Welcome to MythWeb!') ?>
                </div>
                <div id="help_text">
                </div>
            </div>
        </div>
        <div id="search">
            <form action="search.php" method="post">
                <div id="simple_search">
                    <input id="search_text" type="text" name="searchstr" size="15" value="<?php echo htmlentities($_SESSION['search']['searchstr'], ENT_COMPAT, 'UTF-8') ?>">
                    <input id="search_submit" type="submit" class="submit" value="<?php echo t('Search') ?>">
                </div>
                <div id="search_options">
                    <a href="search.php">[<? echo t('advanced') ?>]</a>
                </div>
            </form>
        </div>
    </div>
</div>


<table width="100%" border="0" cellspacing="2" cellpadding="0">
<tr>

    <td colspan="2" class="menu menu_border_t menu_border_b"><table class="body" width="100%" border="0" cellspacing="2" cellpadding="2">
        <tr>
            <td><?php $this->print_menu_content() ?></td>
            <td align="right"><?php echo strftime($_SESSION['date_statusbar'], time())?></td>
        </tr>
        </table></td>

</tr>
</table>

<?
    }

    function print_menu_content() {
        ?><a id="category_legend" onmouseover="popup('category_legend'); return true;">MythTV:</a> &nbsp; &nbsp;
                <a href="program_listing.php"><?php echo t('Listings') ?></a>
                &nbsp; | &nbsp;
                <a href="search.php?category_type=<?php echo movie_word?>"><?php echo t('Movies') ?></a>
                <?/*&nbsp; | &nbsp;
                <a href="index.php?mode=favourites"><?php echo t('Favorites') ?></a>*/?>
                &nbsp; | &nbsp;
                <a href="schedule_manually.php"><?php echo t('Manually Schedule') ?></a>
                &nbsp; | &nbsp;
                <a href="recording_schedules.php"><?php echo t('Recording Schedules') ?></a>
                &nbsp; | &nbsp;
                <a href="scheduled_recordings.php"><?php echo t('Scheduled Recordings') ?></a>
                &nbsp; | &nbsp;
                <a href="recorded_programs.php"><?php echo t('Recorded Programs') ?></a>
                &nbsp; | &nbsp;
                <a href="status.php"><?php echo t('Backend Status') ?></a><?php
        # really should move the category_legend footnote to this section,
        # so it doesn't render in other sections
    }

    function print_footer() {
/*?>
<p align="center">
<font size="-1">MythWeb is part of the <a href="http://www.mythtv.org">MythTV</a> Project</font>
</p>

<?*/
// Display footnotes
    global $Footnotes;
    if (is_array($Footnotes)) {
        foreach ($Footnotes as $note) {
            echo $note;
        }
    }
?>

<div id="category_legend_popup">
<table width="400" bgcolor="#003060" border="1" cellpadding="0" cellspacing="0">
<tr>
    <td><table width="400" bgcolor="#003060" class="small" cellpadding="5" cellspacing="5">
        <tr>
            <td colspan="3"><?php echo t('Category Legend') ?>:</td><?php
    $count = 0;
    global $Categories;
    foreach ($Categories as $cat => $details) {
        if ($count++ % 3 == 0)
            echo "\n\t\t</tr><tr>\n";
        echo "\t\t\t<td class=\"cat_$cat\" align=\"center\"><b>".htmlentities($details[0], ENT_COMPAT, 'UTF-8')."</b></td>\n";
    }
        ?>
        </tr>
        </table></td>
</tr>
</table>
</div>

</body>
</html>
<?
    }
}


?>
