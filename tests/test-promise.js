// Copyright (c) 2017, Intel Corporation.

var test = require('test_promise');
var assert = require('Assert.js');

// test if instance is correct
var p = test.create_promise();
assert(p instanceof Promise, "instance of promise");

// resolve a promise syncrounously
var sync_fulfilled = false;
p.then(function() {
    sync_fulfilled = true;
}, function() {
    assert(false, "promise should be fulfilled");
});
test.fulfill(p);
setTimeout(function() {
    assert(sync_fulfilled, "syncronous promise fulfilled");
    // reject a promise syncronously
    var p1 = test.create_promise();
    var sync_rejected = false;
    p1.then(function() {
        assert(false, "promise should be rejected");
    }, function() {
        sync_rejected = true;
    });
    test.reject(p1);
    setTimeout(function() {
        assert(sync_rejected, "syncrounous promise rejected");
    }, 100);
}, 100);

// fulfill/reject a promise asyncronously
var pasync_ful = test.create_promise();
var pasync_rej = test.create_promise();
var async_fulfilled = false;
var async_rejected = false;
pasync_ful.then(function() {
    async_fulfilled = true;
}, function() {
    assert(false, "promise should be fulfilled");
});
pasync_rej.then(function() {
    assert(false, "promise should be rejected");
}, function() {
    async_rejected = true;
});
setTimeout(function() {
    test.fulfill(pasync_ful);
    test.reject(pasync_rej);
}, 0);
setTimeout(function() {
    assert(async_fulfilled, "async promise was fulfilled");
    assert(async_rejected, "async promise was rejected");
}, 100);

// test that arguments are passed correctly
var pargs = test.create_promise();
var pargs_fulfilled = false;
pargs.then(function(arg) {
    assert(arg === "fulfilled", "argument was correct");
    pargs_fulfilled = true;
}, function(arg) {
    assert(false, "promise should be fulfilled");
});
test.fulfill(pargs, "fulfilled");
setTimeout(function() {
    assert(pargs_fulfilled, "argument promise fulfilled");
}, 100);

// test promies chaining
var pchain = test.create_promise();
pchain.then(function(arg) {
    assert(arg === "chain1", "first chain argument correct");
    if (arg === "chain1") {
        return "chain2";
    }
}).then(function(arg) {
    assert(arg === "chain2", "second chain argument correct");
    if (arg === "chain2") {
        return "chain3";
    }
}).then(function(arg) {
    assert(arg === "chain3", "third chain argument correct");
});
test.fulfill(pchain, "chain1");

// test reject in middle of chain
var pchain1 = test.create_promise();
pchain1.then(function(arg) {
    assert(arg === "chain1", "first chain argument correct");
    if (arg === "chain1") {
        return "chain2";
    }
}).then(function(arg) {
    assert(arg === "chain2", "second chain argument correct");
    if (arg === "chain2") {
        throw Error("rejecting at chain2");
    }
}).then(function(arg) {
    assert(false, "previous promise was fulfilled, not rejected");
}, function(error) {
    assert(error.message === "rejecting at chain2", "promise rejected correctly")
});
test.fulfill(pchain1, "chain1");

setTimeout(function() {
    assert.result();
}, 1000);
