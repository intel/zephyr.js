// Copyright (c) 2016, Intel Corporation.

console.log("Wire IO3 to IO2");

var total = 0;
var passed = 0;

function assert(actual, description) {
    total += 1;

    var label = "\033[1m\033[31mFAIL\033[0m";
    if (actual === true) {
        passed += 1;
        label = "\033[1m\033[32mPASS\033[0m";
    }

    console.log(label + " - " + description);
}

function expectThrow(description, func) {
    var threw = false;
    try {
        func();
    }
    catch (err) {
        threw = true;
    }
    assert(threw, description);
}

var pwm = require("pwm");
var gpio = require("gpio");
var pins = require("arduino101_pins");
var pinA, pinB, msTimer, cycleTimer;

pinB = gpio.open({ pin: pins.IO2, direction: 'in' });

// PWMPins open
pinA = pwm.open({ channel: pins.IO3 });
assert(pinA !== null && typeof pinA === "object",
      "open: defined pin and default argument");

expectThrow("open: undefined pin", function () {
    pinA = pwm.open({ channel: 1024 });
});

// set Period and PulseWidth with ms
// duty cycle: 30%
var msTrue = 0;
var msFalse = 0;
var msCount = 0;

expectThrow("pwmpin: set pulseWidth without period", function () {
    pinA.setPulseWidth(300);
});

pinA = pwm.open({ channel: pins.IO3, period: 3, pulseWidth: 1 });
assert(pinA !== null && typeof pinA === "object",
       "open: with period and pulseWidth");

pinA.setPeriod(1000);

expectThrow("pwmpin: set pulseWidth greater than period", function () {
    pinA.setPulseWidth(3000);
});

pinA.setPulseWidth(300);

msTimer = setInterval(function () {
    if (pinB.read()) {
        msTrue = msTrue + 1;
    } else {
        msFalse = msFalse + 1;
    }

    msCount = msCount + 1;
}, 50);

setTimeout(function () {
    assert(msTrue === 6 && msFalse === 14 && msCount === 20,
           "pwmpin: set period and pulseWidth");
    clearInterval(msTimer);

    expectThrow("pwmpin: set period with invalid value", function () {
        pinA.setPeriod("Value");
    });

    expectThrow("pwmpin: set pulseWidth with invalid value", function () {
        pinA.setPulseWidth("Value");
    });

    // set Period and PulseWidth with cycle
    // duty cycle: 70%
    var cyclesTrue = 0;
    var cyclesFlase = 0;
    var cyclesCount = 0;
    var periodCount = 0;
    var Flag = false;
    var oldFlag = false;
    pinA = pwm.open({ channel: pins.IO3, polarity: "reverse" });
    assert(pinA !== null && typeof pinA === "object", "open: reverse polarity");

    pinA.setPeriodCycles(10000000);
    pinA.setPulseWidthCycles(3000000);

    cycleTimer = setInterval(function () {
       Flag = pinB.read();

       if (Flag === oldFlag) {
           cyclesCount = cyclesCount + 1;
       } else {
           if (oldFlag) {
               cyclesTrue = cyclesTrue + cyclesCount;
           } else {
               cyclesFlase = cyclesFlase + cyclesCount;
           }

           oldFlag = Flag;
           cyclesCount = 0;

           if (Flag === false) {
               periodCount = periodCount + 1;
           }

           if (periodCount === 3) {
               assert((25 < cyclesFlase) && (cyclesFlase < 29) &&
                      (60 < cyclesTrue) && (cyclesTrue < 64),
                      "pwmpin: set periodCycles and pulseWidthCycles");

               console.log("TOTAL: " + passed + " of " + total + " passed");
               clearInterval(cycleTimer);
           }
       }
    }, 10);
}, 1040);

expectThrow("pwmpin: set periodCycles with invalid value", function () {
    pinA.setPeriodCycles("Value");
});

expectThrow("pwmpin: set pulseWidthCycles with invalid value", function () {
    pinA.setPulseWidthCycles("Value");
});
