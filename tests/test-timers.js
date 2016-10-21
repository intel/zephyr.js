// Copyright (c) 2016, Intel Corporation.

// Timers tests

var total = 0;
var passed = 0;
function assert(actual, description) {
    total += 1;
    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }
    console.log(label + " - " + description);
}

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (e) {
        threw = true;
    }
    assert(threw, description);
}

// test setInterval and clearInterval
var countFlag = 10;
var countNoArg = 0;
var testIntervalNoArg = setInterval(function () {
    countNoArg++;
    if (countNoArg == countFlag) {
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
    if (countMoreArg == countFlag) {
        assert((arg1 === IntervalArg1 + 1) && (arg2 === IntervalArg2), 
               "setInterval: set delay and optional arg");
        clearInterval(testIntervalMoreArg);
    }
}, 100, IntervalArg1, IntervalArg2);

expectThrow("clearInterval: intervalID does not exist", function () {
    clearInterval(NotExistedIntervalID);
});

// test setTimeout
setTimeout(function () {
    assert(true, "setTimeout: set delay");
}, 1000);

var testArg = "abc";
setTimeout(function (arg) {
    assert(arg === testArg, "setTimeout: set delay and optional arg");
}, 1000, testArg);

// test clearTimeout
var clearFlag = true;
var testTimeoutID = setTimeout(function () {
    clearFlag = false;
}, 1000);
clearTimeout(testTimeoutID);

setTimeout(function () {
    assert(clearFlag, "clearTimeout: timeoutID");
}, 1500);

expectThrow("clearTimeout: timeoutID does not exist", function () {
    clearTimeout(NotExistedTimeoutID);
});

setTimeout(function () {
    console.log("TOTAL: " + passed + " of " + total + " passed");
}, 2000);

