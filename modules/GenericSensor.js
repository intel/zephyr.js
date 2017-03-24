// Copyright (c) 2016-2017, Intel Corporation.

// JavaScript library for the tests sensor case

function GenericSensor() {

    // API object
    var genericSensor = {};

    var total = 0;
    var pass = 0;

    function assert(actual, description) {
        if (typeof actual !== "boolean" ||
            typeof description !== "string") {
            console.log("AssertionError: invalid input type given");
            throw new Error();
            return;
        }

        total++;

        var label = "\033[1m\033[31mFAIL\033[0m";
        if (actual === true) {
            pass++;
            label = "\033[1m\033[32mPASS\033[0m";
        }

        console.log(label + " - " + description);
    }

    function throws(description, func) {
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

        assert(booleanValue, description);
    }

    function result() {
        console.log("TOTAL: " + pass + " of "
                    + total + " passed");
    }

    var currentState = null;
    var StatechangeFlag = true;
    var changeFlag = false;
    var errorFlag = true;
    var defaultState, startState, stopState, middleState, middleNum;

    genericSensor.test = function(sensor, sensorType) {
        assert(typeof sensor === "object" && sensor !== null,
               "sensor: be defined");

        currentState = sensor.state;
        defaultState = sensor.state;

        assert(typeof currentState === "string" && currentState !== null,
               "sensor: current state as '" + currentState + "'");

        console.log("currentstate: " + currentState);

        middleState = sensor.state;
        sensor.state = middleState + "love";
        assert(sensor.state === middleState,
               "sensor: state is readonly property");

        sensor.onstatechange = function(event) {
            if (StatechangeFlag === true) {
                assert(typeof event === "string" && event !== null,
                       "sensor: callback value for 'onstatechange'");

                StatechangeFlag = false;
            }

            assert(currentState !== event,
                   "sensor: '" + currentState +
                   "' change to '" + event + "'");

            currentState = sensor.state;
            assert(typeof currentState === "string" && currentState !== null,
                   "sensor: current state as '" + currentState + "'");

            console.log("currentstate: " + event);

            if (event === "activated") changeFlag = true;
        };

        sensor.onchange = function() {
            if (changeFlag === true) {
                if (sensorType === "AmbientLight") {
                    assert(typeof sensor.illuminance === "number" &&
                           typeof sensor.illuminance !== null,
                           "sensor: reading value for '" + sensorType + "'");

                    middleNum = sensor.illuminance;
                    sensor.illuminance = middleNum + 1;
                    assert(sensor.illuminance === middleNum,
                           "sensor: reading is readonly property");

                    console.log(sensorType + ": " + sensor.illuminance);
                } else if (sensorType === "Accelerometer" ||
                           sensorType === "Gyroscope") {
                    assert(typeof sensor.x === "number" &&
                           typeof sensor.x !== null &&
                           typeof sensor.y === "number" &&
                           typeof sensor.y !== null &&
                           typeof sensor.z === "number" &&
                           typeof sensor.z !== null,
                           "sensor: reading value for '" + sensorType + "'");

                    middleNum = sensor.x;
                    sensor.x = middleNum + 1;
                    assert(sensor.x === middleNum,
                           "sensor: reading is readonly property");

                    console.log(sensorType + ": " +
                                " x=" + sensor.x +
                                " y=" + sensor.y +
                                " z=" + sensor.z);
                }

                changeFlag = false;
            }
        };

        sensor.onerror = function(event) {
            if (errorFlag === true) {
                assert(typeof event.error === "object",
                       "sensor: callback value for 'onerror'");

                assert(typeof event.error.name === "string" &&
                       typeof event.error.message === "string",
                       "sensor: " + event.error.name + " - " +
                       event.error.message);

                errorFlag = false;
            }
        };

        sensor.start();

        setTimeout(function() {
            startState = sensor.state;
            assert(defaultState !== startState &&
                   defaultState === "unconnected" &&
                   startState === "activated",
                   "sensor: be started");
        }, 1000);

        setTimeout(function() {
            sensor.stop();

            stopState = sensor.state;
            assert(startState !== stopState &&
                   stopState === "idle" &&
                   startState === "activated",
                   "sensor: be stopped");
        }, 20000);

        setTimeout(function() {
            result();
        }, 25000);
    }

    return genericSensor;
};

module.exports.GenericSensor = new GenericSensor();
