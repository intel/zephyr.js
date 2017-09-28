// Copyright (c) 2017, Intel Corporation.

console.log("Starting WebUSB sample...");

var webusb = require('webusb');

// On a Linux PC with the Google Chrome >= 60 running, when you plug the device
//   in via USB, Chrome should detect it and display a notification prompting
//   you to visit the URL specified below.
// Note: You should only use https URLs because Chrome will not display a
//   notification for other URLs.

// suggest that the browser visit this URL to connect to the device
webusb.setURL("https://github.com/01org/zephyrjs");
