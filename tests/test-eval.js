// Copyright (c) 2016, Intel Corporation.

// Test to see whether eval is working; it shouldn't be, as it is a security
// risk

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
    catch (err) {
        threw = true;
    }
    assert(threw, description);
}

expectThrow("Test of the eval() function disabled", function () {
    eval("console.log('Oh no, eval is working!')");
});

console.log("TOTAL: " + passed + " of " + total + " passed");
