#!/bin/bash -c 
echo $PATH

startresnet() {
roslaunch stereo_dnn_ros ap_zed_resnet18_2D_fp16.launch > /dev/null & 
sleep 7
roslaunch stereo_dnn_ros_viz ap_debug_viz.launch > /dev/null &
echo "ZED ros launched"
}

stopresnet() {
    pkill -f "stereo_dnn_ros_viz" 
    pkill -f "stereo_dnn_ros" 
echo "ZED node shutdown"
}

case "$1" in 
    start)   startresnet ;;
    stop)    stopresnet ;;
    *) echo "usage: $0 start|stop" >&2
       exit 1
       ;;
esac
