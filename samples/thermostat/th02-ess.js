// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Grove Temperature & Humidity Sensor (High-Accuracy & Mini)
 * http://wiki.seeed.cc/Grove-TemptureAndHumidity_Sensor-High-Accuracy_AndMini-v1.0/
 * Library   : https://github.com/hallard/TH02
 * Datasheet : http://www.hoperf.com/upload/sensor/TH02_V1.1.pdf
 */
var TH02_I2C_ADDR = 0x40,       // TH02 I2C device address
    TH02_STATUS = 0,            // TH02 register addresses
    TH02_DATAh  = 1,
    TH02_DATAl  = 2,
    TH02_CONFIG = 3,
    TH02_ID     = 17,
    TH02_STATUS_RDY   = 0x01,   // Bit fields of TH02 registers
    TH02_CONFIG_START = 0x01,
    TH02_CONFIG_HEAT  = 0x02,
    TH02_CONFIG_TEMP  = 0x10,
    TH02_CONFIG_HUMI  = 0x00,
    TH02_CONFIG_FAST  = 0x20,
    TH02_ID_VAL       = 0x50;

var i2c = require('i2c'),
    i2cBus = i2c.open({ bus: 0, speed: 100 });

function writeRegister(reg, value) {
    i2cBus.write(TH02_I2C_ADDR, new Buffer([ reg, value ]));
}

function readRegister(reg) {
    return i2cBus.burstRead(TH02_I2C_ADDR, 1, reg);
}

function getId() {
    return readRegister(TH02_ID).readUInt8();
}

function getStatus() {
    return readRegister(TH02_STATUS).readUInt8();
}

function getConfig() {
    return readRegister(TH02_CONFIG).readUInt8();
}

function setConfig(config) {
    writeRegister(TH02_CONFIG, config);
}

function readTemperature(cb) {
    /* Check whether the sensor is accessible and not in used */
    if ((getId() == TH02_ID_VAL) && !(getConfig() & TH02_CONFIG_START)) {
        setConfig(TH02_CONFIG_START | TH02_CONFIG_TEMP);
        /* Max. conversion time is 40ms in normal mode, here we checkout the result after 50ms*/
        var tid = setInterval(function() {
            if (!(getConfig() & TH02_CONFIG_START) && (getStatus() & TH02_STATUS_RDY)) {
                var data = i2cBus.burstRead(TH02_I2C_ADDR, 2, TH02_DATAh).readUInt16BE();
                data = (data >> 2) / 32 - 50;
                if (cb != null) cb(data);
                clearInterval(tid);
            }
        }, 50);
    }
}

function readHumidity(cb) {
    /* Check whether the sensor is accessible and not in used */
    if ((getId() == TH02_ID_VAL) && !(getConfig() & TH02_CONFIG_START)) {
        /* Max. conversion time is 40ms in normal mode, here we checkout the result after 50ms*/
        setConfig(TH02_CONFIG_START);
        var tid = setInterval(function() {
            if (!(getConfig() & TH02_CONFIG_START) && (getStatus() & TH02_STATUS_RDY)) {
                var data = i2cBus.burstRead(TH02_I2C_ADDR, 2, TH02_DATAh).readUInt16BE();
                data = (data >> 4) / 16 - 24;
                if (cb != null) cb(data);
                clearInterval(tid);
            }
        }, 50);
    }
}

/*
 * Export sensed temperature & humidity through BLE Environmental Sensing Service (ESS)
 */
var ble  = require('ble'),
    name = 'Thermostat (TH02)',
    uuidServiceEnvironmentalSensing = '181a';

var temperature = 25.0,
    temperatureCharacteristic = new ble.Characteristic({
        // org.bluetooth.characteristic.temperature
        uuid: '2a6e',
        properties: [ 'read' ],
        onReadRequest: function(offset, callback) {
            console.log('Temperature: ' + temperature + '°C');
            callback(this.RESULT_SUCCESS, toBuffer(temperature * 100, true));
        },
    });

var humidity = 75,
    humidityCharacteristic = new ble.Characteristic({
        // org.bluetooth.characteristic.humidity
        uuid: '2a6f',
        properties: [ 'read' ],
        onReadRequest: function(offset, callback) {
            console.log('Humidity: ' + humidity + '%');
            callback(this.RESULT_SUCCESS, toBuffer(humidity * 100, false));
        },
    });

function toBuffer(value, sign) {
    var buffer = new Buffer(2);
    // The ZJS Buffer API currently only supports unsigned integers
    if (sign == true && value < 0)
        value += 65536;
    buffer.writeUInt16LE(value);
    return buffer;
}

ble.on('stateChange', function(state) {
    console.log('State change: ' + state);
    if (state === 'poweredOn') {
        ble.startAdvertising(name, [ uuidServiceEnvironmentalSensing ]);
    }
});

ble.on('advertisingStart', function(error) {
    if (error) {
        console.log('Advertising error: ' + error);
    } else {
        ble.setServices([
            new ble.PrimaryService({
                // Environmental Sensing Service (ESS)
                uuid: uuidServiceEnvironmentalSensing,
                characteristics: [
                    temperatureCharacteristic,
                    humidityCharacteristic
                ]
            })
        ]);
        console.log('Advertising start success');
    }
});

ble.on('accept', function(clientAddress) {
    console.log('Accepted connection from ' + clientAddress);
});

ble.on('disconnect', function(clientAddress) {
    console.log('Disconnected from ' + clientAddress);
});

console.log('Lesson2: Expose temperature & humidity through BLE ESS...');

setInterval(function() {
    readTemperature(function(temperature) {
        console.log('temperature: ' + temperature);
        this.temperature = temperature;
    });
    setTimeout(readHumidity, 1000, function(humidity) {
        console.log('humidity: ' + humidity);
        this.humidity = humidity;
    });
}, 2000);
