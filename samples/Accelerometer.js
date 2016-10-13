// Copyright (c) 2016, Intel Corporation.

// Test code to use the AccelerometerSensor (subclass of Generic Sensor) API
// to communicate with the BMI160 inertia sensor on the Arduino 101
// and obtaining information about acceleration applied to the X, Y and Z axis
console.log("Acclerometer test...");


var sensor = new AccelerometerSensor({
    includeGravity: false, // true is not supported, will throw error
    frequency: 60
});

sensor.onchange = function(event) {
    console.log("acceleration (m/s^2): " +
          " x=" + event.reading.accelerationX +
          " y=" + event.reading.accelerationY +
          " z=" + event.reading.accelerationZ);
};

sensor.onstatechange = function(event) {
    console.log("state: " + event);
};

sensor.onerrorchange = function(error) {
    console.log("error: " + error);
};

sensor.start();
