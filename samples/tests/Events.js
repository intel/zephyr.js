// Copyright (c) 2016, Intel Corporation.
// Simple test to utilize the event module. This test creates an event emitter,
// registers two event listeners, and emits the events.
var pass = true;

var EventEmitter = require('events');

var myEmitter = new EventEmitter();

function test_event_listener1(arg1, arg2) {
    console.log('test_event: handler 1: arg1=' + arg1 + ' arg2=' + arg2);
}

function test_event_listener2(arg1, arg2) {
    console.log('test_event: handler 2: arg1=' + arg1 + ' arg2=' + arg2);
}

myEmitter.setMaxListeners(20);
var maxListeners = myEmitter.getMaxListeners();
if (maxListeners != 20) {
    console.log("Error, myEmitter.setMaxListeners() did not work");
    pass = false;
}

myEmitter.on('test_event', test_event_listener1);

myEmitter.on('test_event', test_event_listener2);

myEmitter.on('test_event1', function(arg1, arg2) {
    console.log('test_event1: arg1=' + arg1 + ' arg2=' + arg2);
});

var testEventNames = myEmitter.eventNames();
for (var i = 0; i < testEventNames.length - 1; i++) {
    console.log("EventName[" + i + "]: " + testEventNames[i]);
}

var testListenerNames = myEmitter.listeners('test_event');
console.log("testListenerNames.length = " + testListenerNames.length);

if (testListenerNames.length != 2) {
    console.log("Error, myEmitter.listeners() length was wrong");
    pass = false;
}

var testListenerCount = myEmitter.listenerCount('test_event');
console.log("testListenerCount.length = " + testListenerCount);

if (testListenerCount != 2) {
    console.log("Error, myEmitter.listenerCount() was wrong");
    pass = false;
}

myEmitter.emit('test_event', 1234, 4567);
myEmitter.emit('test_event1', 1000, 1111);

/*
 * TODO: This can be removed when the callback ring buffer is implemented
 *       Without this, the event will get removed before its emitted.
 */
setTimeout(function() {
    myEmitter.removeListener('test_event', test_event_listener1);
    myEmitter.emit('test_event', 1234, 4567);
    var numListeners = myEmitter.listenerCount('test_event');
    console.log("Listeners for test_event = " + numListeners);
    if (numListeners != 1) {
        console.log("Error, myEmitter.listenerCount() was wrong");
        pass = false;
    }
    setTimeout(function() {
        myEmitter.removeAllListeners('test_event');
        numListeners = myEmitter.listenerCount('test_event');
        console.log("Listeners for test_event after removeAllListeners = " + numListeners);
        if (numListeners != 0) {
            console.log("Error, myEmitter.listenerCount() was wrong");
            pass = false;
        }
        if (pass == false) {
            console.log("Event test failed");
        } else {
            console.log("Event test passed");
        }
    }, 1000);
}, 1000);

