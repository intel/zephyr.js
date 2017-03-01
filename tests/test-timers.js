// Copyright (c) 2016, Intel Corporation.

// Timers tests

var assert = require("Assert.js");

// test setInterval and clearInterval
var countFlag = 10;
var countNoArg = 0;
var testIntervalNoArg = setInterval(function () {
    countNoArg++;
    if (countNoArg === countFlag) {
        assert(true, "setInterval: set delay");
        clearInterval(testIntervalNoArg);
        assert(true, "clearInterval: intervalID");
    } else if (countNoArg > countFlag) {
        assert(false, "setInterval: set delay");
        assert(false, "clearInterval: intervalID");
    }
}, 100);

var countMoreArg = 0;
var IntervalArg1 = 1;
var IntervalArg2 = "a";
var testIntervalMoreArg = setInterval(function (arg1, arg2) {
    countMoreArg++;
    arg1++;
    if (countMoreArg === countFlag) {
        assert((arg1 === IntervalArg1 + 1) && (arg2 === IntervalArg2),
               "setInterval: set delay and optional arg");
        clearInterval(testIntervalMoreArg);
    }
}, 100, IntervalArg1, IntervalArg2);

assert.throws(function () {
    clearInterval(NotExistedIntervalID);
}, "clearInterval: intervalID does not exist");

// test setTimeout
setTimeout(function () {
    assert(true, "setTimeout: set delay");
}, 1000);

var testArg = "abc";
setTimeout(function (arg) {
    assert(arg === testArg, "setTimeout: set delay and optional arg");
}, 1000, testArg);

var TimeoutArg = [1, 2, 3, 4, 5, 6, 7, 8,
                  9, 10, 11, 12, 13, 14,
                  15, 16, 17, 18, 19, 20];

setTimeout(function (arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                     arg8, arg9, arg10, arg11, arg12, arg13, arg14,
                     arg15, arg16, arg17, arg18, arg19, arg20) {

    var totalNum = arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7 +
                   arg8 + arg9 + arg10 + arg11 + arg12 + arg13 + arg14 +
                   arg15 + arg16 + arg17 + arg18 + arg19 + arg20;

    assert(totalNum === 210, "setTimeout: set 20 arg for timeout");
}, 1000, TimeoutArg[0], TimeoutArg[1], TimeoutArg[2], TimeoutArg[3],
         TimeoutArg[4], TimeoutArg[5], TimeoutArg[6], TimeoutArg[7],
         TimeoutArg[8], TimeoutArg[9], TimeoutArg[10], TimeoutArg[11],
         TimeoutArg[12], TimeoutArg[13], TimeoutArg[14], TimeoutArg[15],
         TimeoutArg[16], TimeoutArg[17], TimeoutArg[18], TimeoutArg[19]);

// test clearTimeout
var clearFlag = true;
var testTimeoutID = setTimeout(function () {
    clearFlag = false;
}, 1000);
clearTimeout(testTimeoutID);

setTimeout(function () {
    assert(clearFlag, "clearTimeout: timeoutID");
}, 1500);

assert.throws(function () {
    clearTimeout(NotExistedTimeoutID);
}, "clearTimeout: timeoutID does not exist");

setTimeout(function () {
    assert.result();
}, 2000);
