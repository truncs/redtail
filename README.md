# ArduPilot support for NVIDIA Redtail project

## This is work in progress!
Autonomous visual navigation components for drones and ground vehicles using deep learning.
This project is based on the original Redatil project by Nvidia see [wiki](https://github.com/NVIDIA-Jetson/redtail/wiki) and includes the changes needed to make it run with the [Arducopter flightcontroller firmware](http://ardupilot.org/copter/) and the latest Nvidia Jetson 4.2.x firmware for the Nevidia TX2 computer.
It incorporates the work from [GSoC 2018 : Complex Autonomous Tasks Onboard a UAV using a Monocular Camera] (https://discuss.ardupilot.org/t/gsoc-2018-complex-autonomous-tasks-onboard-a-uav-using-a-monocular-camera-nvidia-redtail/31933)

This project contains deep neural networks, computer vision and control code, hardware instructions and other artifacts that allow users to build a drone or a ground vehicle which can autonomously navigate through highly unstructured environments like forest trails, sidewalks, etc. The original Nvidia TrailNet DNN for visual navigation is running on NVIDIA's Jetson embedded platform. [arXiv paper](https://arxiv.org/abs/1705.02550) describes TrailNet and other runtime modules in detail.

The project also contains [Stereo DNN](../master/stereoDNN/) models and runtime which allow to estimate depth from a [ZED stereo camera](https://www.stereolabs.com/zed/) on NVIDIA platforms.

For whitepapers, demos, and a detailed buils and configuration description please refer to the original Nvidia documentation at: https://github.com/NVIDIA-AI-IOT/redtail

Known Issues:
- ROS joy node doesnt work on Jetpack 4.2.x, therefore the joystick needs to be connected on a host PC
- gscam does not produce a video stream 
- stereoDNN with resnet18 results in a memory overflow and crash.
