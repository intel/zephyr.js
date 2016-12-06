// Copyright (c) 2016, Intel Corporation.

// JavaScript library for the tests case

function Assert() {

    // API object
    var assert = {};

    assert.total = 0;
    assert.pass = 0;

    assert.true = function (actual, description) {
        if (typeof actual !== "boolean" ||
            typeof description !== "string") {
            console.log("AssertionError: invalid input type given");
            throw new Error();
            return;
        }

        this.total++;

        var label = "\033[1m\033[31mFAIL\033[0m";
        if (actual === true) {
            this.pass++;
            label = "\033[1m\033[32mPASS\033[0m";
        }

        console.log(label + " - " + description);
    }

    assert.equal = function (argA, argB, description) {
        if (typeof argA !== "object" ||
            typeof argA === "undefined" ||
            typeof argB !== "object" ||
            typeof argB === "undefined" ||
            typeof description !== "string") {
            console.log("AssertionError: invalid input type given");
            throw new Error();
            return;
        }

        this.true(argA === argB, description);
    }

    assert.throws = function (description, func) {
        if (typeof description !== "string" ||
            typeof func !== "function") {
            console.log("AssertionError: invalid input type given");
            throw new Error();
            return;
        }

        var booleanValue = false;

        try {
            func();
        }
        catch (err) {
            booleanValue = true;
        }

        this.true(booleanValue, description);
    }

    assert.result = function () {
        console.log("TOTAL: " + this.pass + " of "
                    + this.total + " passed");
    }

    return assert;
};

module.exports.Assert = new Assert();
