#!/bin/bash  

startvideo() {
rosrun image_view video_recorder image:=/zed/zed_node/left/image_rect_color _filename:=$HOME/dflogger/dataflash/zed_video/zed_video`ls $HOME/dflogger/dataflash/zed_video/zed_video* | wc -l`.avi _codec:="I420" > /dev/null & 
	}

startvideo_depth() {
rosrun image_view video_recorder image:=/stereo_dnn_ros_viz/output/ _filename:=$HOME/dflogger/dataflash/zed_video/zed_depth_video`ls $HOME/dflogger/dataflash/zed_video/zed_depth_video* | wc -l`.avi _codec:="I420" > /dev/null & 
	}

stopvideo() {
    pkill -f "video_recorder" 
}

case "$1" in 
    start_recording)   startvideo ;;
    start_rec_depth)   startvideo_depth ;;
    stop)    stopvideo ;;
    *) echo "usage: $0 start_recording|start_rec_depth|stop" >&2
       exit 1
       ;;
esac
