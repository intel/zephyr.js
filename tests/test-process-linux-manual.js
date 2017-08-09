// Copyright (c) 2017, Intel Corporation.

// Build this test case on Linux

console.log("Test process API on Linux");

var count = 0;
var timer = setInterval(function () {
    if (count === 20) {
        console.log("process.exit(): expected result 'exit'");

        process.exit();
    } else if (count >= 25) {
        console.log("Testing failed");

        clearInterval(timer);
    } else {
        console.log("count: " + (count + 1));
    }

    count++;
}, 500);
