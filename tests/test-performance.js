// Copyright (c) 2016, Linaro Limited.

var performance = require("performance");
var assert = require("Assert.js");

var before = performance.now();

setTimeout(function() {
    var after = performance.now();
    var diff = after - before;
    // Allow for some jitter, especially on Linux
    assert(diff >= 979 && diff <= 1021, "performance.now() result over known delay");
    assert.result();
}, 1000);
