// Copyright (c) 2017-2018, Intel Corporation.

// this sample shows how device settings such as the WebUSB URL itself could
//   be configured via the web over WebUSB
console.log('Starting WebUSBConfig sample...');

var fs = require('fs');
var gpio = require('gpio');
var lcd = require('grove_lcd');
var webusb = require('webusb');

// On a Linux PC with the Google Chrome >= 60 running, when you plug the device
//   in via USB, Chrome should detect it and display a notification prompting
//   you to visit the URL specified below.
// Note: You should only use https URLs because Chrome will not display a
//   notification for other URLs.
var url = 'https://intel.github.io/zephyr.js/webusb/acme';

var anvilDelay = 6875;

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

    try {
        var num = buf.toString() | 0;
        anvilDelay = num;
    }
    catch (e) {
        console.log('Error: expected integer delay value:', e.message);
    }

    // write it to the filesystem
    var fd = fs.openSync('device.cfg', 'w');
    var bytes = fs.writeSync(fd, buf, 0, buf.length);
    if (bytes != buf.length) {
        console.log('Error: delay write failed');
        return;
    }

    console.log('Succesfully updated delay to:', buf.toString());

    fs.closeSync(fd);

    blinkPattern = [200, 100, 200, 500];
}

var configMode = false;

var led = gpio.open({pin: 'LED1', activeLow: true});
led.write(1);  // indicate operational status

var configButton = gpio.open({pin: 8, mode: 'in', edge: 'rising'});
var resetButton = gpio.open({pin: 7, mode: 'in', edge: 'rising'});
var blinkPattern = [500];
var blinkIndex = 0;

readConfig();

var toggle = 0;
var debounce = false;

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

function bootConfigMode() {
    // suggest that the browser visit this URL to connect to the device
    configMode = true;
    webusb.setURL(url);
    console.log('Advertising URL on WebUSB:', url);
    webusb.on('read', handleConfigRequest);
    debounce = true;
    setTimeout(function () {
        debounce = false;
    }, 3000);
}

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

    if (screenState && !timer) {
        animate();
    }
};

function resetAnvil() {
    ledAnvil.write(0);
    clearInterval(timerAnvil);
    debounce = false;
    toggleAnvil = 0;
}

//
// Grove LCD Support
//

var glcd = lcd.init();

function glcdInit() {
    // set initial state
    var funcConfig = lcd.GLCD_FS_ROWS_2
        | lcd.GLCD_FS_DOT_SIZE_LITTLE
        | lcd.GLCD_FS_8BIT_MODE;

    glcd.setFunction(funcConfig);
    glcd.clear();
    screenOff();
}

var screenState = 0;

function screenOff() {
    screenState = 0;
    glcd.setDisplayState(lcd.GLCD_DS_DISPLAY_OFF);
    glcd.setColor(0, 0, 0);
}

function resetScreen() {
    screenState = 1;

    glcd.setCursorPos(0, 0);
    glcd.print('     A         S');
    glcd.setCursorPos(0, 1);
    glcd.print('________________');

    glcd.setDisplayState(lcd.GLCD_DS_DISPLAY_ON);
    glcd.setColor(128, 128, 255);
}

glcdInit();

count = 15;

var timer = null;

function drawSections(sections) {
    for (var i = 0; i < sections.length; i++) {
        var section = sections[i];
        if (typeof(sections[0]) !== 'object') {
            section = sections;
        }
        glcd.setCursorPos(section[0], section[1]);
        glcd.print(section[2]);
    }
}

function animate() {
    var fEscape = 20, fClient = 40, fAnvil = 55, fOuch = 65, fSlink = 76,
        fAgain = 100, fVictory = 31, fDone = 110;
    var fMessageTime = 8;
    var fClientSpeed = 2, fSlinkSpeed = 4;

    var winner = anvilDelay === 1250;
    var rrFrames = 16;
    if (winner) {
        rrFrames = 11;
        fAnvil = 11;
        fOuch = 21;
    }

    var rate = 125;
    if (anvilDelay >= 2750) {
        rate = anvilDelay / fAnvil;
    }

    var frame = 0;
    timer = setInterval(function () {
        // target runs across road
        if (frame < rrFrames) {
            drawSections([16 - frame - 1, 1, 'R_']);
        }

        if (!winner) {
            if (frame === 16) {
                drawSections([0, 1, '_']);
            }

            if (frame === fEscape) {
                glcd.setColor(128, 255, 128);
                drawSections([[5, 0, 'Target     '],
                              [0, 1, '    Escaped!    ']]);
            }

            if (frame === fEscape + fMessageTime) {
                glcd.setColor(128, 128, 255);
                drawSections([[3, 0, '  A         S'],
                              [0, 1, '________________']]);
            }

            // client walks in
            if (frame === fClient) {
                drawSections([0, 1, 'W']);
            }
            if (frame >= fClient + fClientSpeed &&
                frame <= fClient + 5 * fClientSpeed &&
                frame % fClientSpeed === 0) {
                drawSections([(frame - (fClient + fClientSpeed)) /
                              fClientSpeed, 1, '_W']);
            }
        }

        // anvil falls
        if (frame === fAnvil) {
            drawSections([[5, 0, ' '], [5, 1, 'A']]);
        }
        if (frame === fAnvil + 1) {
            drawSections([[5, 0, '*'], [4, 1, '*A*']]);
        }
        if (frame === fAnvil + 2) {
            drawSections([[4, 0, '* *'], [3, 1, '*_A_*']]);
        }
        if (frame === fAnvil + 3) {
            drawSections([[3, 0, '*   *'], [2, 1, '*__A__*']]);
        }
        if (frame === fAnvil + 4) {
            drawSections([[3, 0, '     '], [2, 1, '___A___']]);
        }

        if (frame === fOuch) {
            glcd.setColor(255, 32, 32);
            drawSections([3, 0, 'Ouch!']);
        }
        if (frame === fOuch + fMessageTime) {
            glcd.setColor(128, 128, 255);
            drawSections([3, 0, '     ']);
        }

        if (winner) {
            if (frame === fVictory) {
                glcd.setColor(128, 255, 128);
                drawSections([[0, 0, '    Victory!    '],
                              [0, 1, '                ']]);
            }
            if (frame === fVictory + 10) {
                drawSections([3, 1, 'Or was it?']);
            }
            if (frame === fVictory + 20) {
                drawSections([4, 0, 'Beep!   ']);
            }
            if (frame === fVictory + 21) {
                drawSections([3, 1, '    Beep! ']);
                fDone = frame + 6;
            }
        }
        else {
            // client slinks out
            if (frame === fSlink) {
                drawSections([4, 1, 'w']);
            }
            if (frame >= fSlink + fSlinkSpeed && frame <= fSlink + 4 * fSlinkSpeed && frame % fSlinkSpeed === 0) {
                drawSections([3 - (frame - (fSlink + fSlinkSpeed)) / fSlinkSpeed, 1, 'w_']);
            }
            if (frame === fSlink + 5 * fSlinkSpeed) {
                drawSections([0, 1, '_']);
            }

            if (frame === fAgain) {
                drawSections([[0, 0, '     Maybe      '],
                              [0, 1, '   Next Time!   ']]);
            }
        }

        if (frame === fDone) {
            screenOff();
            clearInterval(timer);
            timer = null;
        }

        frame += 1;
    }, rate);
}

configButton.onchange = function() {
    if (!screenState) {
        resetScreen();
    }
};
