// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Basics - ReadAnaloglVoltage example
//   - Reads analog input, convert to voltage, and report on serial console

// Hardware Requirements:
//   - A potentiometer or other device that reports an analog voltage, such
//     as the Grove Kit's Rotary Angle Sensor or Temperature Sensor
// Wiring:
//   - Wire the device's power to Arduino 3.3V and ground to GND
//   - Wire the signal pin to Arduino A0

console.log("Starting ReadAnalogVoltage example...");

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

    // convert 12-bit analog reading (0 - 4095) to voltage (0 - 3.3V)
    var voltage = value * 3.3 / 4095;

    // HACK: work around current lack of Math.round
    var intVoltage = (voltage * 1000 | 0) / 1000.0

    // print it to the serial console
    console.log(intVoltage + 'V');
}, 1000);
