// Copyright (c) 2016, Intel Corporation.

// Test to see whether eval is working; it shouldn't be, as it is a security
// risk

var assert = require("Assert.js");

assert.throws(function () {
    eval("console.log('Oh no, eval is working!')");
}, "Test of the eval() function disabled");

assert.result();
