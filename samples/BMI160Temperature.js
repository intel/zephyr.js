// Copyright (c) 2017-2018, Intel Corporation.

// Test code to use the TemperatureSensor (subclass of Generic Sensor) API
// to communicate with the BMI160 inertia sensor on the Arduino 101
// to monitors the onboard chip temperature
console.log("BMI160 temperature test...");

var updateFrequency = 20; // maximum is 100Hz, but in ashell maximum is 20Hz

var sensor = new TemperatureSensor({
    controller: "bmi160",
    frequency: updateFrequency
});

sensor.onreading = function() {
    console.log("BMI160 temperature (celsius): " + sensor.celsius);
};

sensor.onactivate = function() {
    console.log("activated");
};

sensor.onerror = function(event) {
    console.log("error: " + event.error.name +
                " - " + event.error.message);
};

sensor.start();
