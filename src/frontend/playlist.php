<?php
include('common.php');
list($channels, $status) = get_channel_info();
header("Content-Disposition: attachment; filename=channels.m3u8");
header("Content-Type: application/x-mpegurl");
echo "#EXTM3U\n\n";
foreach ($channels as $chan)
{
    echo "#EXTINF:-1, $chan[0]\n";
    echo "http://{$_SERVER['SERVER_NAME']}/streams/$chan[1]\n\n";
}
?>
