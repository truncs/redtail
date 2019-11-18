#!/bin/bash  

picture_camera() {
        if ! pgrep -x "image_saver" > /dev/null
	then
           rosrun image_view image_saver image:=/zed/zed_node/left/image_rect_color _save_all_image:=false _filename_format:=$HOME/dflogger/dataflash/zed_image/zed_left%04i.jpg __name:=zed_image_saver &
	   sleep 3
        fi
	rosservice call /zed_image_saver/save
	}

picture_depth_map() {
	if ! pgrep -x "image_saver"
        then
           rosrun image_view image_saver image:=/stereo_dnn_ros_viz/output _save_all_image:=false _filename_format:=$HOME/dflogger/dataflash/zed_image/col_depth%04i.jpg __name:=depth_color_image_saver > /dev/null &
	   sleep 3
	fi
        rosservice call /depth_color_image_saver/save
	}

stopimage() {
    pkill -f "image_saver" 
}

case "$1" in 
    take_picture)   picture_camera ;;
    take_depth_pic)   picture_depth_map ;;
    stop)    stopimage ;;
    *) echo "usage: $0 take_picture|take_depth_pic|stop" >&2
       exit 1
       ;;
esac
