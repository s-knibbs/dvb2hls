<?php

function get_channel_info()
{
    $dir = "/run/shm/dvb_hls/";
    $count = 0;
    $channels = array();
    if ($dh = opendir($dir))
    {
        while (($file = readdir($dh)) !== false)
        {
            if (is_file($dir . $file))
            {
                list($name, $ext) = explode(".", $file);
                if ($ext == 'm3u8')
                {
                    $count++;
                }
                else if ($ext == 'csv')
                {
                    $csv = fopen($dir . $file, 'r');
                    while (($data = fgetcsv($csv)) !== false)
                    {
                        $channels[] = $data;
                    }
                }
            }
        }
    }
    $status = (count($channels) == $count) ? 'Ready' : 'Filling channel buffers';
    return array($channels, $status);
}
?>
