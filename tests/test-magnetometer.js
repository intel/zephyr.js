// Copyright (c) 2017, Intel Corporation.

// Testing Magnetometer sensor APIs
console.log("Testing magnetometer sensor APIs on FRDM-K64F");

var genericSensor = require("GenericSensor.js");

var sensor = new Magnetometer({
    frequency: 20
});

genericSensor.test(sensor, "Magnetometer");
