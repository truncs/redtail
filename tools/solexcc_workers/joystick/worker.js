'use strict';

const ATTRS = {
    id: "joystick",
    // Name/description
    name: "Joystick support",
    description: "Joystick support",
    // Does this worker want to loop?
    looper: false,
    // Mavlink messages we're interested in
    mavlinkMessages: []
};

const VERBOSE = true;
// Set the required parameters for the ROS /joy node
var joy_axes = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
var joy_buttons = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
var json_joy_buttons=(JSON.stringify(joy_buttons));
var json_joy_axes= (JSON.stringify(joy_axes));
var sequence_ID = 0;
var time_stamp_sec;
var time_stamp_nsec=0;
var time_stamp_prev=0;
var frame_id="";


function d(str) {
    if(!VERBOSE) return;

    if(process.mainModule === module) {
        console.log(str);
    } else {
        ATTRS.log(ATTRS.id, str);
    }
}

function getAttributes() {
    return ATTRS;
}

function loop() {
}

function onLoad() {
    d("onLoad()");
    const rosnodejs = require('rosnodejs');
    rosnodejs.shutdown();
}

function onUnload() {
    d("onUnload()");
    const rosnodejs = require('rosnodejs');
    rosnodejs.shutdown();
}

function onMavlinkMessage(msg) { }

function onGCSMessage(msg) {
    //d(`onGCSMessage(): msg.id=${msg.id}`);
    // define ROS /joy header parameters
    sequence_ID=sequence_ID+1;
    time_stamp_sec = Date.now();
    time_stamp_nsec = (time_stamp_sec - time_stamp_prev);
    time_stamp_prev = time_stamp_sec;
    
    const result = {
        ok: true
    };

    switch(msg.id) {
        case "on_key_event": {
            onKeyEvent(msg);
            break;
        }

        case "on_motion_event": {
            onMotionEvent(msg);
            break;
        }
    }

    return result;
}

function onKeyEvent(msg) {
    //d(JSON.stringify(msg.code));
    switch(msg.code) {
       case 96: {
             joy_buttons[1]=1;
             break;
             }
       case 97: {
             joy_buttons[2]=1;
             break;
             }
       case 99: {
             joy_buttons[0]=1;
             break;
             }
       case 100: {
             joy_buttons[3]=1;
             break;
             }
       case 102: {
             joy_buttons[4]=1;
             break;
             }
       case 103: {
             joy_buttons[5]=1;
             break;
             }
       case 104: {
             joy_buttons[6]=1;
             break;
             }
       case 105: {
             joy_buttons[7]=1;
             break;
             }
       case 106: {
             joy_buttons[10]=1;
             break;
             }
       case 107: {
             joy_buttons[11]=1;
             break;
             }
       // start and back buttons not yet implemented in Solex
       case 108: {
             joy_buttons[8]=1;
             break;
             }
       case 109: {
             joy_buttons[9]=1;
             break;
             }
    }
    json_joy_buttons=(JSON.stringify(joy_buttons));
    
    //send the movements to /joy_node
    const rosnodejs = require('rosnodejs');
    rosnodejs.initNode('my_node', {onTheFly: true})
    .then(nh => {
        const pub = nh.advertise('/joy', 'sensor_msgs/Joy');
        var joymsg = "{\"header\":{\"seq\":"+sequence_ID+",\"stamp\":{\"secs\":"+time_stamp_sec+",\"nsecs\":"+time_stamp_nsec+"},\"frame_id\":\""+frame_id+"\"},\"axes\":"+json_joy_axes+",\"buttons\":"+json_joy_buttons+"}";
        var joymsgObj=JSON.parse(joymsg);
        pub.publish(joymsgObj);
    })
    .catch(err => {
          d(`error log ${log(err)}`);
    });

    // DEFECT: reset the buttons to zero again to emulate somehow correct button behaviour
    joy_buttons = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    //json_joy_buttons=(JSON.stringify(joy_buttons));
}

function onMotionEvent(msg) {
    //d(JSON.stringify(msg));
    joy_axes[0]=(msg.s_l_x != null) ? msg.s_l_x : 0;
    joy_axes[1]=(msg.s_l_y != null) ? msg.s_l_y : 0;
    joy_axes[2]=(msg.s_r_x != null) ? msg.s_r_x : 0;
    joy_axes[3]=(msg.s_r_y != null) ? msg.s_r_y : 0;
    joy_axes[4]=(msg.dp_x != null) ? msg.dp_x : 0;
    joy_axes[5]=(msg.dp_y != null) ? msg.dp_y : 0;

    json_joy_axes= (JSON.stringify(joy_axes));
    
    //send the movements to /joy_node
    const rosnodejs = require('rosnodejs');
    rosnodejs.initNode('my_node', {onTheFly: true})
    .then(nh => {
        const pub = nh.advertise('/joy', 'sensor_msgs/Joy');
        var joymsg = "{\"header\":{\"seq\":"+sequence_ID+",\"stamp\":{\"secs\":"+time_stamp_sec+",\"nsecs\":"+time_stamp_nsec+"},\"frame_id\":\""+frame_id+"\"},\"axes\":"+json_joy_axes+",\"buttons\":"+json_joy_buttons+"}";
        var joymsgObj=JSON.parse(joymsg);
        pub.publish(joymsgObj);
    })
    .catch(err => {
          d(`error log ${log(err)}`);
    });   
}

function onRosterChanged() {
    d("Roster has been changed");
}

function onBroadcastResponse(msg) {
    // d(`onBroadcastResponse(${JSON.stringify(msg)}`);

    if(msg.request) {
        switch(msg.request.type) {
            case "mission_item_support": {

                if(msg.response) {
                    if(!mMissionItemSupportWorkers) mMissionItemSupportWorkers = [];

                    mMissionItemSupportWorkers.push(msg.response);
                }

                break;
            }
        }
    }
}

function getFeatures() {
    d("getFeatures()");

    var output = {
        // Indicate this worker supports missions
        joystick: { 
            worker_id: ATTRS.id,
            key_event: "on_key_event",
            motion_event: "on_motion_event"
        }
    };

    return output;
}

exports.getAttributes = getAttributes;
exports.loop = loop;
exports.onLoad = onLoad;
exports.onUnload = onUnload;
exports.onMavlinkMessage = onMavlinkMessage;
exports.onGCSMessage = onGCSMessage;
exports.onRosterChanged = onRosterChanged;
exports.getFeatures = getFeatures;
exports.onBroadcastResponse = onBroadcastResponse;

if(process.mainModule === module) {
    d("Hi!");
}
