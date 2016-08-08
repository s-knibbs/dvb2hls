<!DOCTYPE html>
<html>
<head>
<title>DVB - HLS</title>
</head>
<body>
<?php
include('common.php');
list($channels, $status) = get_channel_info();
echo "<h2>Status:</h2>\n";
if (count($channels) == 0)
{
    echo "<p>No channel index found. Check that the daemon is running.</p>\n";
}
else
{
    echo "<p>$status</p>\n";
    echo "<h2>Channel Listing:</h2>\n";
    echo "<ul>\n";
    foreach ($channels as $chan)
    {
        echo "  <li>$chan[0]</li>\n";
    }
    echo "</ul>\n";
    echo "<p><a target=\"_blank\" href=\"playlist.php\">Open playlist</a></p>\n";
}
?>
</body>
</html>
