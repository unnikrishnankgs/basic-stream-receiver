# basic-stream-receiver
Pre-requisite:

Install GStreamer.
https://gstreamer.freedesktop.org/download/

We will need gstreamer-base, gstreamer-plugins-good, gstreamer-plugins-bad, gstreamer-plugins-ugly.

BUILD for OS X:
./make.sh

BUILD for other linux based OS’s:
After installing gstreamer, 
Macro DUMP_TO_FILE
This macro could be enabled in receiver.c file to dump the incoming h.264 video frames into a raw file: sample.h264.
To view sample.h264:
./display.sh

receiver.c
This file implements modified_RTP-de-packetisation, decode and display of H.264 video stream from the android server.
Macros:
SELF_IP: should be changed to local IP to listen on.
Use: 
$ifconfig 
- to get your IP address
The receiver shall listen on port: 5600.
Rest of the code is self-explanatory.
We use GStreamer to decode and display the H.264 stream.
Read and understand the code in conjunction with https://github.com/unnikrishnankgs/android_app_camera2_streaming_server/tree/master/app/src/main/java/sjsu/com/camera2demo/MainActivity.java

RUN
./streamrx

Contact:
Research activity done under the guidance of 
Professor Kaikai Liu,
Dept of Computer Engineering,
San Jose State University
RA: Unnikrishnan Sreekumar (011445889)
[unnikrishnankgs@gmail.com]
