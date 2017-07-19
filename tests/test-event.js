// Copyright (c) 2016-2017, Intel Corporation.

// Testing EVENT APIs

var event = require("events");
var assert = require("Assert.js");

var eventEmitter = new event();

var defaultNum = eventEmitter.getMaxListeners();
assert(defaultNum === 10, "event: default number of listeners");

var SetlistenersNum = 20;
eventEmitter.setMaxListeners(SetlistenersNum);
var GetlistenersNum = eventEmitter.getMaxListeners();
assert(SetlistenersNum === GetlistenersNum,
       "event: set and get max of listeners");

var NoeventsName = eventEmitter.eventNames();
assert(NoeventsName !== null && typeof NoeventsName === "object",
       "event: get event name by no-event");

var EmitnoeventFlag, EmiteventFlag;
EmitnoeventFlag = eventEmitter.emit("value");

// set listener without arg
var onFlag = false;
function test_listener_noArg () {
    onFlag = true;
}

eventEmitter.on("event_noArg", test_listener_noArg);
EmiteventFlag = eventEmitter.emit("event_noArg");

// set listener with 20 args
var totalNum = 0;
var eventArg = [1, 2, 3, 4, 5, 6, 7, 8,
                9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20];

function test_listener_moreArg (arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                                arg8, arg9, arg10, arg11, arg12, arg13, arg14,
                                arg15, arg16, arg17, arg18, arg19, arg20) {
    totalNum = arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7 +
               arg8 + arg9 + arg10 + arg11 + arg12 + arg13 + arg14 +
               arg15 + arg16 + arg17 + arg18 + arg19 + arg20;
}

eventEmitter.on("event_moreArg", test_listener_moreArg);

eventEmitter.emit("event_moreArg", eventArg[0], eventArg[1], eventArg[2],
                                   eventArg[3], eventArg[4], eventArg[5],
                                   eventArg[6], eventArg[7], eventArg[8],
                                   eventArg[9], eventArg[10], eventArg[11],
                                   eventArg[12], eventArg[13], eventArg[14],
                                   eventArg[15], eventArg[16], eventArg[17],
                                   eventArg[18], eventArg[19]);

// set 10 listeners on one event
function event_listener_1 () {};
function event_listener_2 () {};
function event_listener_3 () {};
function event_listener_4 () {};
function event_listener_5 () {};
function event_listener_6 () {};
function event_listener_7 () {};
function event_listener_8 () {};
function event_listener_9 () {};
function event_listener_10 () {};

eventEmitter.on("event_listener", event_listener_1);
eventEmitter.on("event_listener", event_listener_2);
eventEmitter.on("event_listener", event_listener_3);
eventEmitter.on("event_listener", event_listener_4);
eventEmitter.on("event_listener", event_listener_5);
eventEmitter.addListener("event_listener", event_listener_6);
eventEmitter.addListener("event_listener", event_listener_7);
eventEmitter.addListener("event_listener", event_listener_8);
eventEmitter.addListener("event_listener", event_listener_9);
eventEmitter.addListener("event_listener", event_listener_10);

// test listeners name
var listenersName;
listenersName = eventEmitter.listeners("event_listener");
assert(listenersName.length === 10,
       "event: add listener and get listeners name");

// test events name
var eventsName;
eventsName = eventEmitter.eventNames();
assert(eventsName.length === 3, "event: get all events name");

// test remove all listeners
var oldAllListenersNum, newAllListenersNum;
oldAllListenersNum = eventEmitter.listenerCount("event_listener");
eventEmitter.removeAllListeners("event_listener");
newAllListenersNum = eventEmitter.listenerCount("event_listener");
assert(oldAllListenersNum !== 0 && newAllListenersNum === 0,
       "event: remove all listeners on event");

// event response time is about 10 ms
var OldlistenerNum, NewlistenerNum;
setTimeout(function() {
    assert(onFlag, "event: listen and emit without args");

    assert(totalNum === 210, "event: set 20 args for listener");

    assert(EmitnoeventFlag === false && typeof EmitnoeventFlag === "boolean",
           "event: emit with invalid event name");

    assert(EmiteventFlag === true && typeof EmiteventFlag === "boolean",
           "event: emit with event name");

    // test remove one listener
    OldlistenerNum = eventEmitter.listenerCount("event_noArg");
    eventEmitter.removeListener("event_noArg", test_listener_noArg);
    NewlistenerNum = eventEmitter.listenerCount("event_noArg");

    assert(OldlistenerNum === 1 && NewlistenerNum === 0,
           "event: remove one listener");

    assert.result();
}, 1000);
