// Copyright (c) 2017, Intel Corporation.

// test console.time() and console.timeEnd()

var performance = require("performance");

var before = performance.now();

console.log("test: 20 timers run at the same time");
for (var i = 1; i < 21; i++) {
    console.time("Timer" + i);
}

setTimeout(function() {
    for (var j = 1; j < 20; j++) {
        console.timeEnd("Timer" + j);

        var after = performance.now();
        var diff = after - before;
        console.log("expected result: " + (diff - 20) +
                    "ms ~ " + (diff + 20) + "ms\n");
    }

    console.log("test: reposition the starting time");
    console.time("Timer20");
    console.timeEnd("Timer20");
    console.log("expected result: 0ms\n");

    console.log("test: set invalid timer name");
    var Timer21;
    console.time(Timer21);
    console.timeEnd(Timer21);
    console.log("expected result: throw error\n");

    console.log("test: set timer name as null");
    var Timer22 = "";
    console.time(Timer22);
    console.timeEnd(Timer22);
    console.log("expected result: throw error\n");
}, 3000);
