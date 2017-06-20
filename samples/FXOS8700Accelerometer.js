// Copyright (c) 2017, Intel Corporation.

// Test code to use the Accelerometer (subclass of Generic Sensor) API
// to communicate with the FXOS8700 6-Axis Xtrinsic Sensor on the FRDM-K64F
// and obtaining information about acceleration applied to the X, Y and Z axis
console.log("FXOS8700 accelerometer test...");

var updateFrequency = 20; // maximum is 100Hz, but in ashell maximum is 20Hz

var sensor = new Accelerometer({
    frequency: updateFrequency
});

sensor.onchange = function() {
    console.log("acceleration (m/s^2): " +
                " x=" + sensor.x +
                " y=" + sensor.y +
                " z=" + sensor.z);
};

sensor.onactivate = function() {
    console.log("activated");
};

sensor.onerror = function(event) {
    console.log("error: " + event.error.name +
                " - " + event.error.message);
};

sensor.start();
