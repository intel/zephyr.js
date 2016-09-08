// Copyright (c) 2016, Intel Corporation.

// Test code for Arduino 101 that replicates the WebBluetooth demo
// using BLE to advertise temperature changes and allow LED color changes
// this version uses the Grove LCD instead of the PWM bulb

// Hardware Requirements:
//   - A temperature sensor (TMP36)
//   - A Grove LCD
//   - pulldown resistors for SDA and SCL
// Wiring:
//   For temperature sensor:
//     - Wire the device's power to Arduino 3.3V and ground to GND
//     - Wire the signal pin to Arduino A0
//   For LCD:
//     - Wire SDA on the LCD above the pulldown resistor. Connect that resistor to power (VCC)
//     - Wire SDL on the LCD above the pulldown resistor. Connect that resistor to ground (GND)
//     - Wire the SDA for the Arduino and place it between the wire for the LCD and the resistor
//     - Do the same for SDL
//     - Wire power and ground accordingly

var aio = require("aio");
var ble = require("ble");
var grove_lcd = require("grove_lcd");
var pins = require("arduino101_pins");

var DEVICE_NAME = 'Arduino101';

var glcd = grove_lcd.init();

var funcConfig = 1 << 3              // GLCD_FS_ROWS_2
               | 0 << 2              // GLCD_FS_DOT_SIZE_LITTLE
               | 1 << 4;             // GLCD_FS_8BIT_MODE
glcd.setFunction(funcConfig);

var displayConfig = 1 << 2;          // LCD_DS_DISPLAY_ON
glcd.setDisplayState(displayConfig);

glcd.clear();
glcd.print("Web BLE Demo");

var TemperatureCharacteristic = new ble.Characteristic({
    uuid: 'fc0a',
    properties: ['read', 'notify'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'Temperature'
        })
    ]
});

TemperatureCharacteristic._lastValue = undefined;
TemperatureCharacteristic._onChange = null;

var tmp36 = aio.open({ device: 0, pin: pins.A0 });

TemperatureCharacteristic.onReadRequest = function(offset, callback) {
    if (!this._lastValue) {
        var rawValue = pinA0.read();
        if (rawValue == 0) {
            print("PinA: invalid temperature value");
            callback(this.RESULT_UNLIKELY_ERROR, null);
            return;
        }

        var voltage = (rawValue / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;
        this._lastValue = celsius;
    }

    print("Temperature: " + this._lastValue);
    var data = new Buffer(1);
    data.writeUInt8(this._lastValue);
    callback(this.RESULT_SUCCESS, data);
};

TemperatureCharacteristic.onSubscribe = function(maxValueSize,
                                                 updateValueCallback) {
    print("Subscribed to temperature change.");
    this._onChange = updateValueCallback;
    this._lastValue = undefined;
};

TemperatureCharacteristic.onUnsubscribe = function() {
    print("Unsubscribed to temperature change.");
    this._onChange = null;
};

TemperatureCharacteristic.valueChange = function(value) {
    this._lastValue = value;
    glcd.clear();
    glcd.print("Temp: " + value);

    var data = new Buffer(1);
    data.writeUInt8(value);

    if (this._onChange) {
        this._onChange(data);
    }
};

var ColorCharacteristic = new ble.Characteristic({
    uuid: 'fc0b',
    properties: ['read', 'write'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'LED'
        })
    ]
});

// Default color: red.
ColorCharacteristic._value = new Buffer(3);
ColorCharacteristic._value.writeUInt8(255, 0);
ColorCharacteristic._value.writeUInt8(0, 1);
ColorCharacteristic._value.writeUInt8(0, 2);

glcd.setColor(255, 0, 0);

ColorCharacteristic.onReadRequest = function(offset, callback) {
    print("Color change: #" + this._value.toString('hex'));
    callback(this.RESULT_SUCCESS, this._value);
};

ColorCharacteristic.onWriteRequest = function(data, offset, withoutResponse,
                                              callback) {
    var value = data;
    if (!value) {
        print("Error - color onWriteRequest: buffer not available");
        callback(this.RESULT_UNLIKELY_ERROR);
        return;
    }

    this._value = value;
    if (this._value.length !== 3) {
        callback(this.RESULT_INVALID_ATTRIBUTE_LENGTH);
        return;
    }

    var r = this._value.readUInt8(0);
    var g = this._value.readUInt8(1);
    var b = this._value.readUInt8(2);
    glcd.setColor(r, g, b);

    // TODO: probably only supposed to call this if withoutResponse is false?
    callback(this.RESULT_SUCCESS);
};

ble.on('stateChange', function(state) {
    print("BLE state: " + state);

    if (state === 'poweredOn') {
        ble.startAdvertising(DEVICE_NAME, ['fc00'], "https://goo.gl/QEvyDZ");
    } else {
        if (state === 'unsupported') {
            print("BLE not enabled on board");
        }
        ble.stopAdvertising();
    }
});

ble.on('advertisingStart', function(error) {
    print('advertisingStart: ' + (error ? error : 'success'));

    if (error) {
        return;
    }

    ble.setServices([
        new ble.PrimaryService({
            uuid: 'fc00',
            characteristics: [
                TemperatureCharacteristic,
                ColorCharacteristic
            ]
        })
    ]);
});

ble.on('accept', function(clientAddress) {
    print("Accepted Connection: " + clientAddress);

    tmp36.on("change", function(data) {
        var voltage = (data / 4096.0) * 3.3;
        var celsius = (voltage - 0.5) * 100 + 0.5;

        print("Temperature change: " + celsius + " degrees Celsius");
        TemperatureCharacteristic.valueChange(celsius);
    });
});

ble.on('disconnect', function(clientAddress) {
    print("Disconnected Connection: " + clientAddress);

    tmp36.on("change", null);
});

print("WebBluetooth Demo with BLE...");
