// Copyright (c) 2016, Intel Corporation.

// Test code to use the Gyroscope (subclass of Generic Sensor) API
// to communicate with the BMI160 inertia sensor on the Arduino 101
// and monitor the rate of rotation around the the X, Y and Z axis
console.log("Gyroscope test...");


var sensor = new Gyroscope({
    frequency: 50
});

sensor.onchange = function(event) {
    console.log("rotation (rad/s): " +
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
