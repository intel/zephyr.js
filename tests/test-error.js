// Copyright (c) 2017, Intel Corporation.

// Types defined here: https://github.com/zolkis/iot-js-api/tree/master/api

console.log('Testing error types...\n');

var assert = require("Assert.js");

function expectStandardError(description, type, func) {
    var threw = false;
    try {
        func();
        console.log(description + ": expected exception");
    }
    catch (err) {
        try {
            if (err instanceof Error && err instanceof type) {
                threw = true;
            }
            else {
                console.log(description + ": expected error object");
            }
        }
        catch (e) {
            console.log(description + ": instanceof threw exception");
        }
    }
    assert(threw, description);
}

function testError(errorType, errorName, isSecurityError) {
    var error = new errorType("message text");
    var pass = true;

    if (error.name != errorName) {
        pass = false;
        console.log("found", error.name, "name, expected", errorName);
    }
    else if (!(error instanceof Error)) {
        pass = false;
        console.log("error is not an Error");
    }
    else {
        try {
            pass = error instanceof errorType;
        }
        catch (e) {
            pass = false;
            console.log(e.name + ': ' + e.message);
        }

        if (pass && !isSecurityError) {
            try {
                result = error instanceof SecurityError;
                pass = !result;
            }
            catch (e) {
                pass = false;
                console.log(e.name + ': ' + e.message);
            }
        }
    }

    assert(pass, errorName + " works as expected");
}

testError(SecurityError,     "SecurityError", true);
testError(NotSupportedError, "NotSupportedError");
testError(SyntaxError,       "SyntaxError");
testError(TypeError,         "TypeError");
testError(RangeError,        "RangeError");
testError(TimeoutError,      "TimeoutError");
testError(NetworkError,      "NetworkError");
testError(SystemError,       "SystemError");

expectStandardError("calling require with an integer", TypeError, function() {
    require(42);
});
expectStandardError("calling require with a long name", RangeError, function() {
    require('supercalifragilisticexpialidocious');
});
expectStandardError("calling require with an invalid module name",
                    NotSupportedError, function() {
    require('easterbunny');
});

assert.result();
