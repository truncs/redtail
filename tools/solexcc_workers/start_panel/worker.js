'use strict';
const spawn = require("child_process").spawn;
const path = require("path");

const ATTRS = {
    id: "ZEDstart_panel",
    // Name/description
    name: "ZED Start Panel",
    description: "ZED ROS Buttons on the start panel",
    // Does this worker want to loop?
    looper: false,
    // Mavlink messages we're interested in
    mavlinkMessages: []
};

function d(str) {
    ATTRS.log(ATTRS.id, str);
}

/*
Return an object describing this worker. If looper is true, this module must expose a loop() export.
*/
function getAttributes() {
    return ATTRS;
}

let loopIterations = 0;

// Called from dispatch.loop()
function loop() {
}

// Called when this worker is loaded.
function onLoad() {
    d("onLoad()");
}

// Called when unloading
function onUnload() {
    d("onUnload()");
}

// Called when a Mavlink message arrives
function onMavlinkMessage(msg) {
    d(`onMavlinkMessage(): msg.name=$msg.name`);
}

// Called when the GCS sends a message to this worker. Message format is 
// entirely dependent on agreement between the FCS and worker implementation.
function onGCSMessage(msg) {
    d(`onGCSMessage(): msg.id=${msg.id}`);

    const result = {
        ok: true
    };
    d(`message received is ${JSON.stringify(msg)}`);

    switch(msg.id) {
        case "zed_ros_start": {
            startZEDrosnode();
            break;
        }
        case "zed_ros_stop": {
            stopZEDrosnode();
            break;
        }
        case "ros_rtsp_start": {
            startros2rtspnode();
            break;
        }
        case "ros_rtsp_stop": {
            stopros2rtspnode();
            break;
        }

        case "ros_mavros_start": {
            startmavrosnode();
            sendZEDButtonUpdate3();
            break;
        }

        case "ros_mavros_stop": {
            stopmavrosnode();
            sendZEDButtonUpdate3();
            break;
        }

        case "show_dialog": {
            sendShowDialogMessage();
            break;
        }

        case "show_dialog2": {
            sendShowDialogMessage2();
            break;
        }

        case "show_dialog3": {
            sendShowDialogMessage3();
            break;
        }

        case "display_dialog": {
            sendShowDialogMessage4();
            break;
        }

        case "shutdown_ok": {
            d(`message is ${JSON.stringify(msg)}`);
            shutdowntx2();    
        break;
        }

        default: {
            result.ok = false;
            result.message = `No message with id ${msg.id}`;
            break;
        }
    }

    return result;
}

var mZEDrosstart = false;
var mros2rtspstart = false;
var mmavrosstart = false;

// Return a UI for the specified screen.
function onScreenEnter(screen) {
    switch(screen) {
        case ATTRS.api.WorkerUI.Const.SCREEN_START: {
            const buttonColor1 = (mZEDrosstart) ? "#ff39aa00" : "black";
            const buttonColor2 = (mros2rtspstart) ? "#ff39aa00" : "black";            
            const buttonColor3 = (mmavrosstart) ? "#ff39aa00" : "black";

const body = loadLayoutFor(ATTRS.api.WorkerUI.Const.PANEL_WORKER_BUTTONS);
            if(body) {
                //body.children[1].text = buttonText;
                body.children[1].background = buttonColor1;
                body.children[2].background = buttonColor2;
                body.children[3].background = buttonColor3;

                return {
                    screen_id: screen,
                    worker_buttons: body
                };
            } else {
                return null;
            }
        }

        default: {
            return null;
        }
    }
}

function onScreenExit(screen) {

}

function sendZEDButtonUpdate() {
    const buttonColor = (mZEDrosstart)? "#ff39aa00": "black";

    ATTRS.sendGCSMessage(ATTRS.id, {
        id: "screen_update",
        screen_id: "start",
        panel_id: "worker_buttons",
        values: {
            do_dialog: {
                //text: buttonText,
                background: buttonColor
            }
        }
    });
}

function sendZEDButtonUpdate2() {
    const buttonColor = (mros2rtspstart)? "#ff39aa00": "black";

    ATTRS.sendGCSMessage(ATTRS.id, {
        id: "screen_update",
        screen_id: "start",
        panel_id: "worker_buttons",
        values: {
            do_dialog2: {
                background: buttonColor
            }
        }
    });
}

function sendZEDButtonUpdate3() {
    const buttonColor = (mmavrosstart)? "#ff39aa00": "black";

    ATTRS.sendGCSMessage(ATTRS.id, {
        id: "screen_update",
        screen_id: "start",
        panel_id: "worker_buttons",
        values: {
            do_dialog3: {
                background: buttonColor
            }
        }
    });
}

// Serve an image if it exists.
function onImageDownload(name) {
    return ATTRS.api.WorkerUI.serveImage(__dirname, name);
}

var mChildProcess = null;
var mChildProcess2 = null;

function startZEDrosnode() {
    mZEDrosstart = true;
    d(`message received is start ZED node!`);

    if(mChildProcess) {
        d(`Child process is already running`);
        return {ok: false, message: "Child process is already running"};
    }

    const server = path.join(__dirname, "startstopZEDimagenodes.sh start");
    d(`server=${server}`);

    const child = spawn("sh", [ server ], { shell: true });
    child.stdin.setEncoding("utf-8");
    d(`child=${child}`);

    child.on("error", function (error) {
        d(`Error starting child process: ${error}`);
        ATTRS.api.WorkerUI.sendSpeechMessage(ATTRS, `Error starting child process: ${error}`, ATTRS.api.WorkerUI.SpeechType.ERROR);
    });

    child.stdout.on("data", function(data) {
        d(`child.stdout RECEIVED: ${data.toString('utf-8')}`);
        if(data.toString('utf-8').includes ("ZED ros launched")) {
           ATTRS.api.WorkerUI.sendSpeechMessage(ATTRS, "ZED node initiated", ATTRS.api.WorkerUI.SpeechType.TEXT);
           sendZEDButtonUpdate();
        } 
    });

    child.stderr.on("data", function(data) {
        d(`child.stderr: ${data.toString('utf-8')}`);
    });

    child.on("close", function(code) {
        d(`Child closed with ${code}`);
        mChildProcess = null;
    });

    mChildProcess = child;

    return {ok: true, message: "started"};
}

function stopZEDrosnode() {
    mZEDrosstart = false;
    d(`message received is stop ZED node!`);
    if(mChildProcess2) {
        d(`Child process is already running`);
        return {ok: false, message: "Child process is already running"};
    }

    const server = path.join(__dirname, "startstopZEDimagenodes.sh stop");
    d(`server=${server}`);

    const child = spawn("sh", [ server ], { shell: true });
    child.stdin.setEncoding("utf-8");
    d(`child=${child}`);

    child.on("error", function (error) {
        d(`Error starting child process: ${error}`);
    });

    child.stdout.on("data", function(data) {
        d(`child.stdout: ${data.toString('utf-8')}`);
        sendZEDButtonUpdate();
    });

    child.stderr.on("data", function(data) {
        d(`child.stderr: ${data.toString('utf-8')}`);
    });

    child.on("close", function(code) {
        d(`Child closed with ${code}`);
        mChildProcess2 = null;
    });

    mChildProcess2 = child;

    return {ok: true, message: "started"};
}

var mChildProcess3 = null;
var mChildProcess4 = null;

function startros2rtspnode(msg) {
    mros2rtspstart = true;
    d(`message received is start ros2rtsp node!`);
    if(mChildProcess3) {
        d(`Child process is already running`);
        return {ok: false, message: "Child process is already running"};
    }

    const server = path.join(__dirname, "startstopROS2RTSPimagenode.sh start");
    d(`server=${server}`);

    const child = spawn("sh", [ server ], { shell: true });
    child.stdin.setEncoding("utf-8");
    d(`child=${child}`);

    child.on("error", function (error) {
        d(`Error starting child process: ${error}`);
    });

    child.stdout.on("data", function(data) {
        d(`child.stdout: ${data.toString('utf-8')}`);
        if(data.toString('utf-8').includes ("ROS2RTSP launched")) {
           ATTRS.api.WorkerUI.sendSpeechMessage(ATTRS, "Video server initiated", ATTRS.api.WorkerUI.SpeechType.TEXT);
           sendZEDButtonUpdate2();
        }
    });

    child.stderr.on("data", function(data) {
        d(`child.stderr: ${data.toString('utf-8')}`);
    });

    child.on("close", function(code) {
        d(`Child closed with ${code}`);
        mChildProcess3 = null;
    });

    mChildProcess3 = child;

    return {ok: true, message: "started"};
}

function stopros2rtspnode(msg) {
    mros2rtspstart = false;
    d(`message received is stop RTSP node!`);
    if(mChildProcess4) {
        d(`Child process is already running`);
        return {ok: false, message: "Child process is already running"};
    }

    const server = path.join(__dirname, "startstopROS2RTSPimagenode.sh stop");
    d(`server=${server}`);

    const child = spawn("sh", [ server ], { shell: true });
    child.stdin.setEncoding("utf-8");
    d(`child=${child}`);

    child.on("error", function (error) {
        d(`Error starting child process: ${error}`);
    });

    child.stdout.on("data", function(data) {
        d(`child.stdout: ${data.toString('utf-8')}`);
        sendZEDButtonUpdate2();
    });

    child.stderr.on("data", function(data) {
        d(`child.stderr: ${data.toString('utf-8')}`);
    });

    child.on("close", function(code) {
        d(`Child closed with ${code}`);
        mChildProcess4 = null;
    });

    mChildProcess4 = child;

    return {ok: true, message: "started"};
}

var mChildProcess5 = null;
var mChildProcess6 = null;

function startmavrosnode(msg) {
    mmavrosstart = true;
    d(`message received is start MAVROS node!`);
}

function stopmavrosnode(msg) {
    mmavrosstart = false;
    d(`message received is stop MAVROS node!`);
}


function sendShowDialogMessage() {
    const body = loadLayoutFor("display_dialog");

    if(body) {
        ATTRS.sendGCSMessage(ATTRS.id, { id: "display_dialog", content: body });
    }
}

function sendShowDialogMessage2() {
    const body = loadLayoutFor("display_dialog2");

    if(body) {
        ATTRS.sendGCSMessage(ATTRS.id, { id: "display_dialog", content: body });
    }
}

function shutdowntx2() {
    d(`message received is shutdown`);

    const server = path.join(__dirname, "shutdowntx2.sh");
    d(`server=${server}`);

    const child = spawn("sh", [ server ], { shell: true });
    child.stdin.setEncoding("utf-8");
    d(`child=${child}`);

    child.on("error", function (error) {
        d(`Error starting child process: ${error}`);
    });

    child.stdout.on("data", function(data) {
        d(`child.stdout RECEIVED: ${data.toString('utf-8')}`);
    });

    child.stderr.on("data", function(data) {
        d(`child.stderr: ${data.toString('utf-8')}`);
    });

    child.on("close", function(code) {
        d(`Child closed with ${code}`);
    });
    return {ok: true, message: "started"};
}



function sendShowDialogMessage3() {
    const body = loadLayoutFor("display_dialog3");

    if(body) {
        ATTRS.sendGCSMessage(ATTRS.id, { id: "display_dialog", content: body });
    }
}

function sendShowDialogMessage4() {
    const body = loadLayoutFor("display_dialog4");

    if(body) {
        ATTRS.sendGCSMessage(ATTRS.id, { id: "display_dialog", content: body });
    }
}

function loadLayoutFor(panel) {
    return ATTRS.api.WorkerUI.loadLayout(__dirname, panel);
}

exports.getAttributes = getAttributes;
exports.loop = loop;
exports.onLoad = onLoad;
exports.onUnload = onUnload;
exports.onMavlinkMessage = onMavlinkMessage;
exports.onGCSMessage = onGCSMessage;
exports.onScreenEnter = onScreenEnter;
exports.onScreenExit = onScreenExit;
exports.onImageDownload = onImageDownload;

