// Copyright (c) 2016-2017, Intel Corporation.

console.log("Wire IO2 to IO3");

var assert = require("Assert.js");

var pwm = require("pwm");
var gpio = require("gpio");
var pinA, pinB, msTimer, cycleTimer;

pinB = gpio.open({pin: "IO2", mode: 'in'});

// PWMPins open
pinA = pwm.open('IO3');
assert(pinA !== null && typeof pinA === "object",
      "open: defined pin and default argument");

assert.throws(function () {
    pinA = pwm.open({channel: 1024});
}, "open: undefined pin");

// set Period and PulseWidth with ms
// duty cycle: 33%
var msTrue = 0;
var msFalse = 0;

// set pulse width greater than period
assert.throws(function() {
    pinA.setMilliseconds(3000, 5000);
}, "pwmpin: set pulseWidth greater than period");

// set pulse width greater than period
assert.throws(function() {
    pinA.setCycles(3000, 5000);
}, "pwmpin: set pulseWidth greater than period (cycles)");

pinA.setMilliseconds(3000, 1000);
msTimer = setInterval(function () {
    if (pinB.read()) {
        msTrue = msTrue + 1;
    } else {
        msFalse = msFalse + 1;
    }
}, 60);

setTimeout(function () {
    // 16 = 0.33 < 0.3333 < 0.34 = 17
    assert((15 < msTrue) && (msTrue < 18) && (32 < msFalse) && (msFalse < 35),
           "pwmpin: set period and pulseWidth");
    clearInterval(msTimer);

    assert.throws(function () {
        pinA.setMilliseconds("Value", 1000);
    }, "pwmpin: set period with invalid value");

    assert.throws(function () {
        pinA.setMilliseconds(1000, "Value");
    }, "pwmpin: set pulseWidth with invalid value");

    // set Period and PulseWidth with cycle
    // duty cycle: 70%
    var cyclesTrue = 0;
    var cyclesFalse = 0;
    var cyclesCount = 0;
    var periodCount = 0;
    var Flag = 0;
    var oldFlag = 0;
    pinA = pwm.open({ pin: 'IO3', reversePolarity: true });
    assert(pinA !== null && typeof pinA === "object", "open: reverse polarity");

    pinA.setCycles(10000000, 3000000);

    cycleTimer = setInterval(function () {
       Flag = pinB.read();

       if (Flag === oldFlag) {
           cyclesCount = cyclesCount + 1;
       } else {
           if (oldFlag) {
               cyclesTrue = cyclesTrue + cyclesCount;
           } else {
               cyclesFalse = cyclesFalse + cyclesCount;
           }

           oldFlag = Flag;
           cyclesCount = 0;

           if (Flag === 0) {
               periodCount = periodCount + 1;
           }

           if (periodCount === 3) {
               assert((10 < cyclesFalse) && (cyclesFalse < 14) &&
                      (28 < cyclesTrue) && (cyclesTrue < 32),
                      "pwmpin: set periodCycles and pulseWidthCycles");

               assert.result();
               clearInterval(cycleTimer);
           }
       }
    }, 20);
}, 3000);

assert.throws(function () {
    pinA.setCycles("Value", 1000);
}, "pwmpin: set period cycles with invalid value");

assert.throws(function () {
    pinA.setCycles(1000, "Value");
}, "pwmpin: set pulseWidth cycles with invalid value");
