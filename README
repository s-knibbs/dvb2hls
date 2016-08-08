# Dvb2HLS - DVB to HTTP Live Streaming tool

Dvb2HLS is a tool to stream DVB video broadcasts over a local network using Apple's
HTTP Live Streaming standard. The tool has been developed to run on a Raspberry Pi 2
using a USB DVB-T/T2 tuner. The tool runs in the background as a Linux daemon, the streams
are then served using a webserver such as Apache.

## Installation & Setup

Build and installation prerequisites:

- CMake version 2.8 or later.
- Apache & PHP
- Boost libraries: `libboost-dev`, `libboost-program-options1.50-dev`
- [libdvbpsi](http://www.videolan.org/developers/libdvbpsi.html) v1.3.0

**Note:** You will need to build libdvbpsi from the [v1.3.0 tarball](http://download.videolan.org/pub/libdvbpsi/1.3.0/). The version of libdvbpsi in the raspbian repository an older version with an incompatible API.

You will need to install the latest [DTV scan tables](https://www.linuxtv.org/downloads/dtv-scan-tables/). Download
the latest tarball and install with the following:

```
~# tar -Pxjvf dtv-scan-tables.tar.bz2
```

Build and install the project. In the root of the repository, run the following:

```
~$ cmake .
~$ make
```

If everything built OK you can install with:

```
~# make install
```

The tool stores media segments in shared memory to avoid continuous writes to the SD card on the Raspberry Pi 2.
It may be necessary to increase the size of the shared memory file system by adding the following to `/etc/fstab`:

```
none            /run/shm        tmpfs   nosuid,nodev,size=629145600    0     0
```

This will increase the shared memory to 600MB which is large enough for HD broadcasts.

## Usage

The tool has the following usage:

```
DVB - HTTP Live Streaming (HLS) server V0.1

Uses Apple's HTTP Live Streaming protocol to stream channels from a
dvb multiplex. Currently supports DVB-T/T2.
The generated .ts segments and .m3u8 index files are served
up separately with a webserver such as Apache.

Options:
  -h [ --help ]                         Print this help message and exit.
  -t [ --tuning-file ] arg              Name of the tuning file.
  -p [ --tuning-path ] arg (=/usr/share/dvb/dvb-t/)
                                        Path to the tuning files.
  -m [ --multiplex ] arg                Name of the multiplex in the tuning
                                        file to use.
  -a [ --adapter ] arg (=0)             Adapter number to use.
  -d [ --daemon ]                       Run as a daemon.
  -s [ --stop ]                         Stop any existing daemon processes.
```

You will need to find the dtv-scan-table for the nearest TV transmitter. You can find this out from [UKFree](https://ukfree.tv/prediction) for the UK.

You will also need to specify the name of the multiplex. These are listed in the scan table file, for example, to stream the HD channels, use "BBC B HD".

The tool does not require root permissions to run, but you will need to add your user to the video group to access ththe tuner.

After starting the executable, providing that the tuner was able to find a signal and scan for channels, the tool will start running in the background as a daemon. Warnings and errors from the daemon are sent to the syslog.  

To enable the web interface, run the following and restart Apache:

```
~# a2ensite dvb_hls
```

It will take a couple of minutes after starting the daemon for enough video to buffer. You should now be able to
see a channel listing by browsing to `http://yourhostname`.

For playback on a PC, [VLC media player](http://www.videolan.org/vlc/index.html) works best.

## Known Issues

- The tool does not currently restamp the PCR timestamps after demultiplexing the video streams. This causes glitches with
playback in VLC due to PCR jitter.
- Various HLS standards violations currently.

## LICENSE

The project is licensed under the terms of the GPLv3.
