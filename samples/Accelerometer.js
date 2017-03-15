// Copyright (c) 2016, Intel Corporation.

// Test code to use the Accelerometer (subclass of Generic Sensor) API
// to communicate with the BMI160 inertia sensor on the Arduino 101
// and obtaining information about acceleration applied to the X, Y and Z axis
console.log("Accelerometer test...");

var updateFrequency = 20 // maximum is 100Hz, but in ashell maximum is 20Hz

var sensor = new Accelerometer({
    includeGravity: false, // true is not supported, will throw error
    frequency: updateFrequency
});

sensor.onchange = function(event) {
    console.log("acceleration (m/s^2): " +
                " x=" + event.reading.x +
                " y=" + event.reading.y +
                " z=" + event.reading.z);
};

sensor.onstatechange = function(event) {
    console.log("state: " + event);
};

sensor.onerror = function(event) {
    console.log("error: " + event.error.name +
                " - " + event.error.message);
};

sensor.start();
