<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
    <title>Error</title>
    <link rel="stylesheet" type="text/css" href="<?php echo root ?>skins/errors.css">
</head>

<body>

<div id="message">

<h2>Database Error</h2>

<p>
<?php echo html_entities($db->error) ?>
</p>

</div>

</body>
</html>
