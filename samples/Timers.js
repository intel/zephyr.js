// Copyright (c) 2016, Intel Corporation.

// Example using setInterval and setTimeout, passing arguments
// The timeout should stop the setInterval timer after five times

// Hardware Requirements:
//   - None

console.log("Starting Timers example...");

var count = 1;

// Test without arguments
var i1 = setInterval(function() {
    console.log("Interval (no args) #" + count);
}, 1000);

// Test with arguments
var i2 = setInterval(function(a, b) {
    console.log("Interval (args) #" + count + ' arg1: ' + a + ' arg2: ' + b);
    count++;
}, 1000, 1, 2);

setTimeout(function(a, b) {
    console.log("Timeout, clearing interval");
    clearInterval(a);
    clearInterval(b);
}, 5000, i1, i2);
