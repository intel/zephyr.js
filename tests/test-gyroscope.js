// Copyright (c) 2016, Intel Corporation.

// Testing gyroscope APIs
console.log("Testing gyroscope APIs");

var genericSensor = require("GenericSensor.js");

var sensor = new Gyroscope({
    frequency: 50
});

genericSensor.test(sensor, "Gyroscope");
