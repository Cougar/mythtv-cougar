<?
/***                                                                        ***\
	init.php				                 Last Updated: 2003.11.18 (xris)

	This file is part of MythWeb, a php-based interface for MythTV.
	See README and LICENSE for details.

	Initialization routines.  This file basically loads all of the necessary
	shared files for the entire program.
\***                                                                        ***/

// Load in the error libraries before we do anything that might cause some.
	require_once 'includes/errors.php';
	require_once 'includes/errordisplay.php';

// Load the user-defined configuration settings
	require_once 'config/conf.php';

// Clean up some variables
	if (!ereg('/$', $_SERVER['DOCUMENT_ROOT']))
		$_SERVER['DOCUMENT_ROOT'] .= '/';

// Clean up any linefeed messiness we get from the form data
	foreach (array_keys($_GET) as $key) {
		$_GET[$key] = ereg_replace("\r\n", "\n", $_GET[$key]);
	// Process any imagemap submissions to make sure we also get the name itself
		if (ereg('_[xy]$', $key)) {
			$key = ereg_replace('_[xy]$', '', $key);
			if (!isset($_GET[$key]))
				$_GET[$key] = true;
		}
	}
	foreach (array_keys($_POST) as $key) {
		$_POST[$key] = ereg_replace("\r\n", "\n", $_POST[$key]);
	// Process any imagemap submissions to make sure we also get the name itself
		if (ereg('_[xy]$', $key)) {
			$key = ereg_replace('_[xy]$', '', $key);
			if (!isset($_POST[$key]))
				$_POST[$key] = true;
		}
	}

// Start the session, and set the cookie to expire in one year
	session_name('mythweb_id');
	session_set_cookie_params(60 * 60 * 24 * 355, '/', server_domain);
	session_start();

// Connect to the database, or restore a persistent connection
//  please note that calling mysql_close is unnecessary - see php documentation for details
    #$dbh = mysql_pconnect(db_host, db_username, db_password)
    $dbh = mysql_connect(db_host, db_username, db_password)
        or trigger_error("Can't connect to the database server.  Did you use the correct settings in config/conf.php?", FATAL);
    mysql_select_db(db_dbname)
		or trigger_error("Can't access the database file:  " . mysql_error() . " [#" . mysql_errno() . "]", FATAL);

// Include a few useful functions
	require_once "includes/utils.php";

// Connect to the backend and load some more handy utilities
	require_once "includes/mythbackend.php";

// Load in the channel and program classes
	require_once "includes/channels.php";
	require_once "includes/programs.php";

// Load the user's theme settings
	#
	# we'll eventually load theme settings from cookie/session info
	#
	define('theme_dir', 'themes/'.Theme.'/');

// Load the theme config
	require_once 'config/theme_'.Theme.'.php';

// Load the overall page theme class
	require_once theme_dir."theme.php";

// Make sure the image cache path exists
	$path = '';
	foreach (split('/+', image_cache) as $dir) {
		$path .= $path ? '/' . $dir : $dir;
		if(!is_dir($path) && !mkdir($path, 0755))
			trigger_error('Error creating path for '.$path.': Please check permissions.', FATAL);
	}

// Clean out stale thumbnails
	if ($dir = opendir(image_cache)) {
		while (($file = readdir($dir))) {
			if (!is_file(image_cache.'/'.$file) || !ereg('\\.(png|jpg|gif)$', $file))
				continue;
		// Delete files that haven't been touched in the last 3 days
			if (fileatime(image_cache.'/'.$file) < time() - 3 * 24 * 60 * 60)
				unlink(image_cache.'/'.$file);
		}
		closedir($dir);
	}

// Load/set default session data
	if (!$_SESSION['date_statusbar'])       $_SESSION['date_statusbar']       = 'D, M d, g:i A';
	if (!$_SESSION['date_scheduled'])       $_SESSION['date_scheduled']       = 'D, M j (g:i A)';
	if (!$_SESSION['date_scheduled_popup']) $_SESSION['date_scheduled_popup'] = 'D, M j';
	if (!$_SESSION['date_recorded'])        $_SESSION['date_recorded']        = 'D, M j (g:i A)';
	if (!$_SESSION['date_search'])          $_SESSION['date_search']          = 'D, M d, g:i A';
	if (!$_SESSION['date_listing_key'])     $_SESSION['date_listing_key']     = 'D, M d, g:i A';
	if (!$_SESSION['date_listing_jump'])    $_SESSION['date_listing_jump']    = 'D, F j';
	if (!$_SESSION['date_channel_jump'])    $_SESSION['date_channel_jump']    = 'D, F j';
	if (!$_SESSION['time_format'])          $_SESSION['time_format']          = 'h:i A';

?>
