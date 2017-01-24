// Copyright (c) 2016, Intel Corporation.

// JavaScript library for the tests case

function Assert() {
    // API object
    var assert = function (actual, description) {
        var thisfunc = arguments.callee;
        thisfunc.total++;

        var label = "\033[1m\033[31mFAIL\033[0m";
        if (actual) {
            thisfunc.pass++;
            label = "\033[1m\033[32mPASS\033[0m";
        }

        if (description === undefined)
            description = 'no description provided';
        console.log(label + " - " + description);
    }

    assert.equal = function (argA, argB, description) {
        return this(argA === argB, description);
    }

    assert.throws = function (func, description) {
        var booleanValue = false;

        try {
            func();
        }
        catch (err) {
            booleanValue = true;
        }

        return this(booleanValue, description);
    }

    assert.result = function () {
        console.log("TOTAL: " + this.pass + " of " + this.total + " passed");
    }

    assert.total = 0;
    assert.pass = 0;

    return assert;
};

module.exports.Assert = new Assert();
