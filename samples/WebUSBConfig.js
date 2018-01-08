// Copyright (c) 2017-2018, Intel Corporation.

// this sample shows how device settings such as the WebUSB URL itself could
//   be configured via the web over WebUSB
console.log('Starting WebUSBConfigURL sample...');

var fs = require('fs');
var gpio = require("gpio");
var webusb = require('webusb');

// On a Linux PC with the Google Chrome >= 60 running, when you plug the device
//   in via USB, Chrome should detect it and display a notification prompting
//   you to visit the URL specified below.
// Note: You should only use https URLs because Chrome will not display a
//   notification for other URLs.
var url = 'https://intel.github.io/zephyr.js/webusb/acme';

var anvilDelay = 5000;

function readConfig() {
    // check for configuration file
    if (fs.statSync('device.cfg')) {
        var fd = fs.openSync('device.cfg', 'r');
        var buf = new Buffer(128);
        var bytes = fs.readSync(fd, buf, 0, 128, 0);
        var delay = 0;
        for (var i = 0; i < bytes; i++) {
            delay *= 10;
            delay += buf.readUInt8(i) - 48;  // ascii for 0
        }
        if (delay > 0) {
            anvilDelay = delay;
        }
        fs.closeSync(fd);
    }
}

function handleConfigRequest(data) {
    // reply back to web page immediately when data received
    // expected commands:
    //   read delay
    //   set delay <number>

    var buf = new Buffer(5);
    data.copy(buf, 0, 0, 5);
    if (buf.toString() === 'read ') {
        data.copy(buf, 0, 5, 10);
        if (buf.toString() === 'delay') {
            var sbuf = new Buffer(anvilDelay + '');
            webusb.write(sbuf);
            console.log('Sent current delay value:', anvilDelay);
            return;
        }
        console.log('Error: unknown setting requested');
        return;
    }

    var cmdlen = 10;
    buf = new Buffer(cmdlen);
    data.copy(buf, 0, 0, cmdlen);
    if (buf.toString() !== 'set delay ') {
        console.log('Error: unknown command received');
        return;
    }

    // rest should be int
    buf = new Buffer(data.length - cmdlen);
    data.copy(buf, 0, cmdlen, data.length);

    for (var i = 0; i < buf.length; i++) {
        if (buf[i] < '0' || buf[i] > '9') {
            console.log('Error: expected integer delay value');
            return;
        }
    }

    // write it to the filesystem
    var fd = fs.openSync('device.cfg', 'w');
    var bytes = fs.writeSync(fd, buf, 0, buf.length);
    if (bytes !== buf.length) {
        console.log('Error: delay write failed');
        return;
    }

    console.log('Succesfully updated delay to:', buf.toString());

    fs.closeSync(fd);

    blinkPattern = [200, 100, 200, 500];
}

function bootConfigMode() {
    // suggest that the browser visit this URL to connect to the device
    webusb.setURL(url);
    console.log('Advertising URL on WebUSB:', url);
    webusb.on('read', handleConfigRequest);
}

var led = gpio.open({pin: 'LED1', activeLow: true});
led.write(1);  // indicate operational status

var configButton = gpio.open({pin: 8, mode: 'in'});
var resetButton = gpio.open({pin: 7, mode: 'in', edge: 'rising'});
var blinkPattern = [500];
var blinkIndex = 0;

readConfig();

var toggle = 0;

function blinker() {
    led.write(toggle);
    toggle = 1 - toggle;
    blinkIndex = (blinkIndex + 1) % blinkPattern.length;
    setTimeout(blinker, blinkPattern[blinkIndex]);
}

if (resetButton.read()) {
    if (fs.statSync('device.cfg')) {
        fs.unlinkSync('device.cfg');
        blinkPattern = [100, 100, 100, 100, 100, 500];
        blinker();
    }
}
else if (configButton.read()) {
    bootConfigMode();
    blinkPattern = [100];
    blinker();
}

var debounce = false;

var ledAnvil = gpio.open({pin: 'LED0', activeLow: true});
ledAnvil.write(0);

var timerAnvil = null;
var toggleAnvil = 0;

function dropAnvil() {
    setTimeout(resetAnvil, 1000);
    timerAnvil = setInterval(blinkAnvil, 100);
}

function blinkAnvil() {
    toggleAnvil = 1 - toggleAnvil;
    ledAnvil.write(toggleAnvil);
}

resetButton.onchange = function () {
    if (debounce)
        return;

    debounce = true;
    setTimeout(dropAnvil, anvilDelay);
};

function resetAnvil() {
    ledAnvil.write(0);
    clearInterval(timerAnvil);
    debounce = false;
    toggleAnvil = 0;
}
