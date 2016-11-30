// Copyright (c) 2016, Intel Corporation.

// Testing I2C APIs

// Hardware Requirements:
//   - A Grove LCD
//   - pull-up resistors for SDA and SCL, we use two 10k resistors,
//   - you should choose resistors that will work with the LCD hardware you have,
//   - the ones we listed here are the ones that have known to work for us,
//   - so your mileage may vary if you have different LCD
// Wiring:
//   For LCD:
//     - Wire SDA on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD to SDA on the Arduino 101
//     - Wire SCL on the LCD to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

console.log("Grove LCD is required...");

var i2c = require("i2c");

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


// I2CBus bus and speed
var speeds = [10, 100, 400, 1000, 34000];
var i2cDevice;
for (var i = 0; i < speeds.length; i++) {
    // I2C open
    i2cDevice = i2c.open({ bus: 0, speed: speeds[i]});

    assert(i2cDevice.speed === speeds[i],
           "I2C: open with speed " + speeds[i]);

    if (i === 0) {
        assert(("bus" in i2cDevice) && (i2cDevice.bus === 0),
              "I2CBus: bus property exist");

        var busValue = i2cDevice.bus;
        i2cDevice.bus = busValue + 1;
        assert(i2cDevice.bus === busValue,
              "I2CBus: bus is readonly property");

        assert("speed" in i2cDevice,
              "I2CBus: speed property exist");

        var speedValue = i2cDevice.speed;
        i2cDevice.speed = speedValue + 1;
        assert(i2cDevice.speed === speedValue,
              "I2CBus: speed is readonly property");
    }
}

// Define various Grove LCD addresses
var GROVE_RGB_BACKLIGHT_ADDR = 0x62;

// RGB FUNCTIONS
var REGISTER_B = 0x02;

function write(regAddr, color) {
    // Valid range for color is 0 - 255
    var blueData = new Buffer([regAddr, color]);
    i2cDevice.write(GROVE_RGB_BACKLIGHT_ADDR, blueData);
}

var b = 255;
i2cDevice = i2c.open({ bus: 0, speed: 100});
// I2C write
write(REGISTER_B, b);

// I2C read
var readValue = i2cDevice.read(GROVE_RGB_BACKLIGHT_ADDR, 1, 0x00);
assert(readValue.readUInt8() === b, "I2C: write() and read()");

// I2C burstRead
var size = 8;
readValue = i2cDevice.burstRead(GROVE_RGB_BACKLIGHT_ADDR, size, 0x00);
assert(!!readValue && readValue.length === size, "I2C: burstRead()");

expectThrow("I2C: open invalid bus", function () {
    i2c.open({ bus: 1, speed: 100 });
});

console.log("TOTAL: " + passed + " of " + total + " passed");
