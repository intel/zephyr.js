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
    var changeFlag = false;
    var errorFlag = true;
    var defaultState, startState, stopState, middleState, middleNum;
    var middleNumX, middleNumY, middleNumZ, tmpTimestamp;

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

        sensor.onactivate = function() {
            currentState = sensor.state
            console.log("currentstate: " + currentState);

            assert(currentState === "activated",
                   "sensor: state is activated");

            changeFlag = true;
        };

        sensor.onchange = function() {
            tmpTimestamp = sensor.timestamp;

            if (changeFlag === true) {
                assert(typeof sensor.timestamp === "number" &&
                       sensor.timestamp !== null,
                       "sensor: timestamp value is defined");

                middleNum = sensor.timestamp;
                sensor.timestamp = middleNum + 1;
                assert(sensor.timestamp === middleNum,
                       "sensor: timestamp is readonly property");

                if (sensorType === "AmbientLight") {
                    assert(typeof sensor.illuminance === "number" &&
                           sensor.illuminance !== null,
                           "sensor: reading value for '" + sensorType + "'");

                    assert(sensor.controller === "ADC_0",
                           "sensor: controller name as default");
                    console.log("sensor.controller : " + sensor.controller);

                    middleNum = sensor.illuminance;
                    sensor.illuminance = middleNum + 1;
                    assert(sensor.illuminance === middleNum,
                           "sensor: reading is readonly property");

                    console.log("sensor.timestamp: " + sensor.timestamp);
                    console.log(sensorType + ": " + sensor.illuminance);
                } else if (sensorType === "Accelerometer" ||
                           sensorType === "Gyroscope" ||
                           sensorType === "Magnetometer") {
                    assert(typeof sensor.x === "number" &&
                           sensor.x !== null &&
                           typeof sensor.y === "number" &&
                           sensor.y !== null &&
                           typeof sensor.z === "number" &&
                           sensor.z !== null,
                           "sensor: reading value for '" + sensorType + "'");

                    if (sensorType === "Magnetometer") {
                        assert(sensor.controller === "FXOS8700",
                               "sensor: controller name as default");
                    } else {
                        assert(sensor.controller === "bmi160",
                               "sensor: controller name as default");
                    }
                    console.log("sensor.controller : " + sensor.controller);

                    middleNumX = sensor.x;
                    sensor.x = middleNumX + 1;
                    middleNumY = sensor.y;
                    sensor.y = middleNumY + 1;
                    middleNumZ = sensor.z;
                    sensor.z = middleNumZ + 1;
                    assert(sensor.x === middleNumX &&
                           sensor.y === middleNumY &&
                           sensor.z === middleNumZ,
                           "sensor: reading is readonly property");

                    console.log("sensor.timestamp: " + sensor.timestamp);
                    console.log(sensorType + ": " +
                                " x=" + sensor.x +
                                " y=" + sensor.y +
                                " z=" + sensor.z);
                } else if (sensorType === "Temperature") {
                    assert(typeof sensor.celsius === "number" &&
                           sensor.celsius !== null,
                           "sensor: reading value for '" + sensorType + "'");

                    assert(sensor.controller === "bmi160",
                           "sensor: controller name as default");
                    console.log("sensor.controller : " + sensor.controller);

                    middleNum = sensor.celsius;
                    sensor.celsius = middleNum + 1;
                    assert(sensor.celsius === middleNum,
                           "sensor: reading is readonly property");

                    console.log("sensor.timestamp: " + sensor.timestamp);
                    console.log(sensorType + ": " + sensor.celsius);
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
            assert(tmpTimestamp === sensor.timestamp,
                       "sensor: timestamp value is latest reading value");

            result();
        }, 25000);
    }

    return genericSensor;
};

module.exports.GenericSensor = new GenericSensor();
