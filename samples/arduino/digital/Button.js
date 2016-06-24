// Copyright (c) 2016, Intel Corporation.

// Reimplementation of Arduino - Digital - Button example
// * Turns on an LED whenever a button is being pressed

// pin 8 corresponds to an onboard LED on the Arduino 101
var led = GPIO.open({pin: 8, direction: 'out'});
// pin 19 corresponds to IO4 on the Arduino 101
var button = GPIO.open({pin: 19, direction: 'in', edge: 'any'});

GPIO.set_callback(19, function () {
    led.write(button.read())
});
