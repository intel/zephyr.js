// Copyright (c) 2016, Intel Corporation.
// Simple test to utilize the event module. This test creates an event emitter,
// registers two event listeners, and emits the events.

var EventEmitter = require('events');

var myEmitter = new EventEmitter();

myEmitter.on('test_event', function(arg1, arg2) {
    print('test_event: handler 1: arg1=' + arg1 + ' arg2=' + arg2);
});
myEmitter.on('test_event', function(arg1, arg2) {
    print('test_event: handler 2: arg1=' + arg1 + ' arg2=' + arg2);
});

myEmitter.on('test_event1', function(arg1, arg2) {
    print('test_event1: arg1=' + arg1 + ' arg2=' + arg2);
});

myEmitter.emit('test_event', 1234, 4567);
myEmitter.emit('test_event1', 1000, 1111);
