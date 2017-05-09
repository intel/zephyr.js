// Copyright (c) 2017, Intel Corporation.

var assert = require("Assert.js");
var cb = require('test_callbacks');

var times = 0;
function callback(arg1, arg2, arg3, arg4, arg5)
{
    assert(arg1 == "one", "string arg was correct");
    assert(arg2 == false, "bool arg was correct");
    assert(arg3 == undefined, "undef arg was correct");
    assert(arg4 == null, "null arg was correct");
    assert(arg5 == 42, "number arg was correct");
    assert(this.prop == "a prop", "this object was correct");
    times++;
}

var list_count = 0;
function callback_l1(arg1, arg2, arg3, arg4, arg5)
{
    assert(arg1 == "one", "string arg was correct");
    assert(arg2 == false, "bool arg was correct");
    assert(arg3 == undefined, "undef arg was correct");
    assert(arg4 == null, "null arg was correct");
    assert(arg5 == 42, "number arg was correct");
    assert(this.prop == "list", "this object was correct");
    list_count++;
}

function callback_l2(arg1, arg2, arg3, arg4, arg5)
{
    assert(arg1 == "one", "string arg was correct");
    assert(arg2 == false, "bool arg was correct");
    assert(arg3 == undefined, "undef arg was correct");
    assert(arg4 == null, "null arg was correct");
    assert(arg5 == 42, "number arg was correct");
    assert(this.prop == "list", "this object was correct");
    list_count++;
}

function callback_l3(arg1, arg2, arg3, arg4, arg5)
{
    assert(arg1 == "one", "string arg was correct");
    assert(arg2 == false, "bool arg was correct");
    assert(arg3 == undefined, "undef arg was correct");
    assert(arg4 == null, "null arg was correct");
    assert(arg5 == 42, "number arg was correct");
    assert(this.prop == "list", "this object was correct");
    list_count++;
}

var id = cb.addCallback(callback, {prop: "a prop"});
cb.signalCallback(id, "one", false, undefined, null, 42);
cb.signalCallback(id, "one", false, undefined, null, 42);
cb.signalCallback(id, "one", false, undefined, null, 42);

setTimeout(function() {
    assert(times === 3, "signaled 3 times");
    cb.removeCallback(id);
    cb.signalCallback(id, "one", "two", "three");
    setTimeout(function() {
        assert(times === 3, "removed callback not called");
        cb.signalCallback(-1);
        cb.signalCallback(-10);
        cb.signalCallback(12345);
        setTimeout(function() {
            assert(true, "with bad ID's didn't crash");
            var list_id = cb.addCallbackList(callback_l1, {prop: "list"}, -1);
            cb.addCallbackList(callback_l2, {prop: "list"}, list_id);
            cb.addCallbackList(callback_l3, {prop: "list"}, list_id);
            cb.signalCallback(list_id, "one", false, undefined, null, 42);
            setTimeout(function() {
                assert(list_count === 3, "all three list callbacks called");
                cb.removeCallback(list_id);
                cb.signalCallback(list_id, "one", "two", "three");
                setTimeout(function() {
                    assert(list_count === 3,
                           "removed list callback didn't get called");
                    assert.result();
                }, 100);
            }, 0);
        }, 0);
    }, 0);
}, 0);
