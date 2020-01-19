# Extended NVIDIA Redtail project for Arducopter

## Note: This is work in progress!
Autonomous visual navigation components for drones using deep learning.
This project is based on the original Redtail project by Nvidia see [wiki](https://github.com/NVIDIA-Jetson/redtail/wiki) and includes the changes needed to make it run with the [Arducopter flightcontroller firmware](http://ardupilot.org/copter/) based on the work of https://github.com/ArduPilot/redtail. It also incorporates the migration of all project components to Jetpack 4.2.2. 

This project contains deep neural networks, computer vision and control code, hardware instructions and other artifacts that allow users to build a drone which can autonomously navigate through highly unstructured environments like forest trails, sidewalks, etc. The original Nvidia TrailNet DNN for visual navigation is running on NVIDIA's Jetson embedded platform. [arXiv paper](https://arxiv.org/abs/1705.02550) describes TrailNet and other runtime modules in detail.

The project also contains [Stereo DNN](../master/stereoDNN/) models and runtime which allow to estimate depth from a [ZED stereo camera](https://www.stereolabs.com/zed/) on NVIDIA platforms.

For whitepapers, demos, and a detailed build and configuration description please refer to the original Nvidia documentation at: https://github.com/NVIDIA-AI-IOT/redtail

For installation, test and flight instructions please see the wiki in this git: https://github.com/mtbsteve/redtail/wiki 


News:
- 3D CAD files for a 1 axis gimbal for the ZED stereo cam added
- Redtail install script and instructions for Jetpack 4.2.x and Ubuntu 18.04 added
- Wiki added for installation, setup and testing
- Solex-CC workers for ZED camera control and for managing the different ROS nodes
- Darknet-YOLO added for object recognition
- Solex-CC worker to launch drone on a trail (trailnet) added

Known Issues and restrictions:
- ROS joy node doesnt work on Jetpack 4.2.x, therefore the joystick needs to be connected on a host PC
- stereoDNN with nvsmall and resnet18 results in a memory overflow and crash
- Simulation environment has not been touched yet and therefore likely doesn't work yet
