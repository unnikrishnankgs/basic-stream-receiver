/Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 filesrc location=sample.h264 ! h264parse ! avdec_h264 ! videorate ! video/x-raw, framerate=25/1 ! autovideosink
