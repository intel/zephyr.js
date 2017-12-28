// Copyright (c) 2017, Intel Corporation.

console.log('Starting WebUSB sample...');

var webusb = require('webusb');

// On a Linux PC with the Google Chrome >= 60 running, when you plug the device
//   in via USB, Chrome should detect it and display a notification prompting
//   you to visit the URL specified below.
// Note: You should only use https URLs because Chrome will not display a
//   notification for other URLs.

// suggest that the browser visit this URL to connect to the device
webusb.setURL('https://intel.github.io/zephyr.js/webusb');

sizes = [13, 17, 23, 19, 4, 8];
offset = 0;

// reply back to web page immediately when data received
count = 0;
webusb.on('read', function(bytes) {
    count += 1
    console.log('Received packet', count, '(' + bytes.length + ' bytes)');

    var buflen = sizes[offset];
    var buf = new Buffer(buflen);
    try {
        for (var j = 0; j < buflen; j++)
            buf.writeUInt8(j, j);
    }
    catch (e) {
        console.log('Error:', e.message);
        return;
    }
    webusb.write(buf);

    offset = (offset + 1) % sizes.length;
});
