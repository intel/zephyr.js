// Copyright (c) 2017-2018, Intel Corporation.

// Test code to use the Magnetometer (subclass of Generic Sensor) API
// to communicate with the FXOS8700 6-Axis Xtrinsic Sensor on the FRDM-K64F
// and measures the ambient magnetic field for the X, Y, and Z axis
console.log("FXOS8700CQ magnetometer test...");

var updateFrequency = 20; // maximum is 100Hz, but in ashell maximum is 20Hz

var sensor = new Magnetometer({
    frequency: updateFrequency
});

sensor.onreading = function() {
    console.log("magnetic field (μT): " +
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
