// Copyright (c) 2016, Intel Corporation.

// Test code to use the Gyroscope (subclass of Generic Sensor) API
// to communicate with the BMI160 inertia sensor on the Arduino 101
// and monitor the rate of rotation around the the X, Y and Z axis
print("Gyroscope test...");


var sensor = new Gyroscope();

sensor.onchange = function(event) {
    print("rotation (rad/s): " +
          " x=" + event.reading.rotationRateX +
          " y=" + event.reading.rotationRateY +
          " z=" + event.reading.rotationRateZ);
};

sensor.onstatechange = function(event) {
    console.log("state: " + event);
};

sensor.onerrorchange = function(error) {
    console.log("error: " + error);
};

sensor.start();
