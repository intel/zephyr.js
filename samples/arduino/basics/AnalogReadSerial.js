// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - AnaloglReadSerial example
//   - Reads analog input on pin A0, reports value on serial console

// Hardware Requirements:
//   - A potentiometer or other device that reports an analog voltage, such
//     as the Grove Kit's Rotary Angle Sensor or Temperature Sensor
// Wiring:
//   - Wire the device's power to Arduino 3.3V and ground to GND
//   - Wire the signal pin to Arduino A0
// Note: The maximum reading may not reach all the way to 4095

console.log("Starting AnalogReadSerial example...");

var aio = require("aio");
var pins = require("arduino101_pins");

var pin = aio.open({
    device: 0,
    pin: pins.A0
});

// schedule a function to run every 1s (1000)
setInterval(function () {
    // read analog input pin A0
    var value = pin.read();

    // print it to the serial console
    console.log(value);
}, 1000);
