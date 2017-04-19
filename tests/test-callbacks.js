// Copyright (c) 2017, Intel Corporation.

var assert = require("Assert.js");
var cb = require('test_callbacks');

var times = 0;
function callback(arg1, arg2, arg3)
{
    assert((arg1 == "one" && arg2 == "two" && arg3 == "three" && this.prop == "a prop"),  "args matched (" + times + ")");
    times++;
}

var list_count = 0;
function callback_l1(arg1, arg2, arg3)
{
    assert((arg1 == "one" && arg2 == "two" && arg3 == "three" && this.prop == "list"),  "list args matched (" + list_count + ")");
    list_count++;
}

function callback_l2(arg1, arg2, arg3)
{
    assert((arg1 == "one" && arg2 == "two" && arg3 == "three" && this.prop == "list"),  "list args matched (" + list_count + ")");
    list_count++;
}

function callback_l3(arg1, arg2, arg3)
{
    assert((arg1 == "one" && arg2 == "two" && arg3 == "three" && this.prop == "list"),  "list args matched (" + list_count + ")");
    list_count++;
}

var id = cb.addCallback(callback, {prop: "a prop"});
cb.signalCallback(id, "one", "two", "three");
cb.signalCallback(id, "one", "two", "three");
cb.signalCallback(id, "one", "two", "three");

setTimeout(function() {
    assert(times == 3, "signalled 3 times");
    cb.removeCallback(id);
    cb.signalCallback(id, "one", "two", "three");
    setTimeout(function() {
        assert(times == 3, "removed callback not called");
        cb.signalCallback(-1);
        cb.signalCallback(-10);
        cb.signalCallback(12345);
        setTimeout(function() {
            assert(true, "with bad ID's didn't crash");
            var list_id = cb.addCallbackList(callback_l1, {prop: "list"}, -1);
            cb.addCallbackList(callback_l2, {prop: "list"}, list_id);
            cb.addCallbackList(callback_l3, {prop: "list"}, list_id);
            cb.signalCallback(list_id, "one", "two", "three");
            setTimeout(function() {
                assert(list_count == 3, "all three list callbacks called");
                cb.removeCallback(list_id);
                cb.signalCallback(list_id, "one", "two", "three");
                setTimeout(function() {
                    assert(list_count == 3, "removed list callback didn't get called");
                    assert.result();
                }, 0);
            }, 0);
        }, 0);
    }, 0);
}, 0);
