#!/bin/bash  
picture_camera() {
	if ! ps -ef | grep [i]mage_rect_color 
	then
           rosrun image_view image_saver image:=/zed/zed_node/left/image_rect_color _save_all_image:=false _filename_format:=/home/apsync/dflogger/dataflash/zed_image/zed_left%04i.jpg __name:=zed_image_saver > /dev/null &
	   sleep 4
        fi
	rosservice call /zed_image_saver/save > /dev/null
	echo "picture taken"
	exit 1
	}

picture_depth_map() {
	if ! ps -ef | grep [s]tereo_dnn_ros_viz/output
        then
           rosrun image_view image_saver image:=/stereo_dnn_ros_viz/output _save_all_image:=false _filename_format:=/home/apsync/dflogger/dataflash/zed_image/col_depth%04i.jpg __name:=depth_color_image_saver > /dev/null &
	   sleep 4
	fi
        rosservice call /depth_color_image_saver/save > /dev/null
	echo "color depth pic taken"
	exit 1
	}

stopimage() {
    sudo pkill -f "image_view" 
    exit 0
}

case "$1" in 
    take_picture)   picture_camera ;;
    take_depth_pic)   picture_depth_map ;;
    stop)    stopimage ;;
    *) echo "usage: $0 take_picture|take_depth_pic|stop" >&2
       exit 1
       ;;
esac
exit 0
