<?php
/***                                                                ***\
    errordisplay.php                Last Updated: 2005.02.28 (xris)

    This file contains a number of error-display related routines
\***                                                                ***/

// These are arrays that will contain error messages
    global $Errors, $Warnings;
    $Errors   = array();
    $Warnings = array();

// This is an associative array for keeping track of whether items have been marked as "bad"
// in order that they can properly be displayed with highlight_error()
    global $BadItems;
    $BadItems = array();

// This function will actually display any errors or warnings
    function display_errors($leading='<p align="center">', $trailing = '</p>') {
        global $Errors, $Warnings;
    // Errors or warnings from a previous page?
        if (@count($_SESSION['WARNINGS'])) {
            foreach ($_SESSION['WARNINGS'] as $warning) {
                $Warnings[] = $warning;
            }
            unset($_SESSION['WARNINGS']);
        }
    // Nothing to show?
        if (!count($Errors) && !count($Warnings)) return;
    // Warnings?
        if (Is_Editor && $Warnings)
            array_unshift($Warnings, (count($Errors) ? "\n" : '')."Warning:\n");
    // Load the errors
        $js_errstr = implode("\n", array_merge($Errors, $Warnings));
        $errstr    = str_replace("\n", "<br>\n", htmlentities($js_errstr));
        if (Is_Editor && count($Errors)) {
            $errstr    .= "\n<p>\nNo changes were committed.";
            $js_errstr .= "\n\nNo changes were committed.";
        }
    // Clean up the javascript error string
        $js_errstr = str_replace("\n", "\\n",
                        str_replace('"', '\\"',
                        $js_errstr));
    // Print
        echo <<<EOF
<script language="JavaScript" type="text/javascript">
<!--
on_load.push(display_errors);
function display_errors() { alert("$js_errstr"); };
// -->
</script>
<noscript>
$leading
<div id="error">
$errstr
</div>
$trailing
</noscript>
EOF;
    }

// This function just draws a simple red line around something, to mark it as containing erroneous data
    function highlight_error($content, $field = '') {
        global $BadItems;
        if (!$field || $BadItems[$field])
            echo "<table border=\"0\" cellspacing=\"0\" cellpadding=\"1\" class=\"error\"><tr><td>$content</td></tr></table>";
        else
            echo $content;
    }

// Add an error to the list
    function add_error($error, $fields = NULL) {
        global $Errors, $BadItems;
        if (!preg_match('/\\w/', $error)) return;
        $Errors[] = trim($error);
    // Add any bad item fields
        if (is_array($fields)) {
            foreach ($fields as $field) {
                $BadItems[$field] = true;
            }
        }
        elseif ($fields)
            $BadItems[$fields] = true;
    }

// Add a warning to the list
    function add_warning($warning, $fields = NULL) {
        global $Warnings, $BadItems;
        if (!preg_match('/\\w/', $warning)) return;
        $Warnings[] = trim($warning);
    // Add any bad item fields
        if (is_array($fields)) {
            foreach ($fields as $field) {
                $BadItems[$field] = true;
            }
        }
        elseif ($fields)
            $BadItems[$fields] = true;
    }

// Save errors and warnings into a session error/warning variable
    function save_session_errors() {
        global $Errors, $Warnings;
        $_SESSION['WARNINGS'] = array();
        foreach ($Errors as $error) {
            $_SESSION['WARNINGS'][] = $error;
        }
        foreach ($Warnings as $warning) {
            $_SESSION['WARNINGS'][] = $warning;
        }
    }

// Returns true or false if there are errors/warnings
    function has_errors()   { return !empty($GLOBALS['Errors']);   }
    function has_warnings() { return !empty($GLOBALS['Warnings']); }

?>
