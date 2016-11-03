var performance = require("performance");

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

var before = performance.now();
//console.log("performance.now() before:", before);

setTimeout(function() {
    var after = performance.now();
    //console.log("performance.now() after:", after);
    var diff = after - before;
    // Allow for some jitter, especially on Linux
    assert(diff >= 989 && diff <= 1011, "performance.now() result over known delay");
    console.log("TOTAL: " + passed + " of " + total + " passed");
}, 1000);
