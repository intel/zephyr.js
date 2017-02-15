// Copyright (c) 2017, Intel Corporation.

// Sample code for Arduino 101 w/ Grove LCD and Grove pushbutton on IO2
// Clicking the button toggles a sound level meter up and down on the LCD.

console.log("Alarm Clock sample...");

// Configuration

// amount of time each button click should add to the alarm
var minuteIncrement = 1;

// LCD color during setting and countdown
var baseColor = [0, 0, 128];

// LCD colors while alarm is flashing
var alarmColorOn = [128, 0, 0];
var alarmColorOff = [128, 128, 128];

// messages (16 chars)
var noAlarmMessage = 'none            ';
var expiredAlarmMessage = 'expired!        ';

// Application

var pins = require("arduino101_pins");
var gpio = require("gpio");
var lcd = require("grove_lcd");
var perf = require("performance");

// set initial state
var funcConfig = lcd.GLCD_FS_ROWS_2
               | lcd.GLCD_FS_DOT_SIZE_LITTLE
               | lcd.GLCD_FS_8BIT_MODE;

var glcd = lcd.init();
glcd.setFunction(funcConfig);
glcd.setDisplayState(lcd.GLCD_DS_DISPLAY_ON);

glcd.clear();
glcd.setColor(baseColor[0], baseColor[1], baseColor[2]);
glcd.setCursorPos(0, 0);
glcd.print('Alarm:');
glcd.setCursorPos(0, 1);
glcd.print(noAlarmMessage);

var debounce = false;

var button = gpio.open({ pin: pins.IO2, direction: 'in', edge: 'rising' });

// IO4, IO7, IO8 can work here
var buzzer = gpio.open({ pin: pins.IO4, direction: 'out' });

var minutes = 0;
var seconds = 0;
var timer = null;
var displayWidth = 0;
var stopTime = 0;

button.onchange = function (event) {
    if (debounce)
        return;

    debounce = true;

    if (timer)
        clearInterval(timer);

    minutes += minuteIncrement;
    seconds = 0;
    stopTime = perf.now() + ((minutes * 60)) * 1000;
    var msg = "Alarm set for " + minutes + " minute";
    if (minutes != 1)
        msg += 's';
    console.log(msg);

    timer = setInterval(updateLCD, 1000);

    updateLCD();

    setTimeout(resetDebounce, 100);
}

function resetDebounce() {
    debounce = false;
}

function updateLCD() {
    var time = perf.now();
    if (time >= stopTime) {
        glcd.setCursorPos(0, 1);
        glcd.print(expiredAlarmMessage);

        soundAlarm();
        clearInterval(timer);
        timer = null;

        displayWidth = 0;
        return;
    }

    var remain = (stopTime - time + 999) / 1000 | 0;
    minutes = remain / 60 | 0;
    seconds = remain - minutes * 60;

    var display = minutes + ':';
    if (seconds < 10)
        display += '0';
    display += seconds;

    if (display.length > displayWidth)
        displayWidth = display.length;

    // pad with spaces
    while (display.length < displayWidth)
        display = ' ' + display;

    glcd.setCursorPos(0, 1);
    glcd.print(display);
}

var alarm = false;
var timings = [62, 63, 62, 63, 62, 63, 62, 563,
               62, 63, 62, 63, 62, 63, 62, 563,
               62, 63, 62, 63, 62, 63, 62, 563];
var index = 0;

function soundAlarm() {
    console.log("Alarm expired!");
    index = 0;
    toggleAlarm();
}

function toggleAlarm() {
    alarm = !alarm;

    if (index < timings.length) {
        buzzer.write(alarm);
        setTimeout(toggleAlarm, timings[index++]);
        color = alarm ? alarmColorOn : alarmColorOff;
        glcd.setColor(color[0], color[1], color[2]);
    }
    else {
        if (alarm) {
            // ensure we turn off at the end
            alarm = 0;
            buzzer.write(false);
        }
        glcd.setColor(baseColor[0], baseColor[1], baseColor[2]);
        glcd.setCursorPos(0, 1);
        glcd.print(noAlarmMessage);
    }
};
