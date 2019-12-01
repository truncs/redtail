#!/bin/bash -c 
echo $PATH

startros2rtsp() {
roslaunch ros_rtsp rtsp_streams.launch > /dev/null & 
echo "ROS2RTSP launched"
}

stopros2rtsp() {
    pkill -f "ros_rtsp"
echo "ROS2RTSP node shutdown"
}

case "$1" in 
    start)   startros2rtsp ;;
    stop)    stopros2rtsp ;;
    *) echo "usage: $0 start|stop" >&2
       exit 1
       ;;
esac
