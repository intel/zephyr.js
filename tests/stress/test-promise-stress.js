// Copyright (c) 2017, Intel Corporation.

var test = require('test_promise');

var sync_toggle = 0;
var async_toggle = 0;
var sync_count = 0;
var async_count = 0;

setInterval(function() {
    console.log("calling sync promise, count = " + sync_count);
    var p = test.create_promise();
    p.then(function() {
        console.log("fulfilled sync");
    }, function() {
        console.log("rejected sync");
    });
    if (sync_toggle) {
        test.fulfill(p);
    } else {
        test.reject(p);
    }
    sync_toggle = (sync_toggle) ? 0 : 1;
    sync_count++;
}, 100);

setInterval(function() {
    console.log("calling async promise, count = " + async_count);
    var p = test.create_promise();
    p.then(function() {
        console.log("fulfilled async");
    }, function() {
        console.log("rejected async");
    });
    setTimeout(function() {
        if (async_toggle) {
            test.fulfill(p);
        } else {
            test.reject(p);
        }
        async_toggle = (async_toggle) ? 0 : 1;
        async_count++;
    }, 100);
}, 200);
