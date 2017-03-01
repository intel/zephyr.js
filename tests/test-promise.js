// Copyright (c) 2017, Intel Corporation.

var test = require('test_promise');

var sync_toggle = 0;
var async_toggle = 0;
var sync_fulfilled = false;
var sync_rejected = false;
var async_fulfilled = false;
var async_rejected = false;

var sync_int = setInterval(function() {
    console.log("calling promise");
    var p = test.create_promise();
    p.then(function() {
        console.log("fulfilled sync");
        sync_fulfilled = true;
        clearInterval(sync_int);
    }).catch(function() {
        console.log("rejected sync");
        sync_rejected = true;
    });
    if (sync_toggle) {
        test.fulfill(p);
    } else {
        test.reject(p);
    }
    sync_toggle = (sync_toggle) ? 0 : 1;
}, 100);


var async_int = setInterval(function() {
    var p = test.create_promise();
    p.then(function() {
        console.log("fulfilled async");
        async_fulfilled = true;
        clearInterval(async_int);
    }).catch(function() {
        console.log("rejected async");
        async_rejected = true;
    });
    setTimeout(function() {
        if (async_toggle) {
            test.fulfill(p);
        } else {
            test.reject(p);
        }
        async_toggle = (async_toggle) ? 0 : 1;
    }, 100);
}, 200);

setTimeout(function() {
    if (sync_fulfilled && sync_rejected && async_fulfilled && async_rejected) {
        console.log("\033[1m\033[32mPASS\033[0m");
    } else {
        console.log("\033[1m\033[31mFAIL\033[0m");
    }
}, 2000);
