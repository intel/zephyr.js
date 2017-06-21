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

var ocf    = require('ocf');
var server = ocf.server;

var resPathTemperature = '/a/temperature',
    resTypeTemperature = 'oic.r.temperature',
    temperatureResource = null,
    temperatureProperties = {
        temperature: 25.0,
        units: 'C'
    },
    temperatureResourceInit = {
        resourcePath : resPathTemperature,
        resourceTypes: [ resTypeTemperature ],
        interfaces   : [ 'oic.if.baseline' ],
        discoverable : true,
        observable   : false,
        properties   : temperatureProperties
    };

var resPathHumidity = '/a/humidity',
    resTypeHumidity = 'oic.r.humidity',
    humidityResource = null,
    humidityProperties = {
        humidity: 80
    },
    humidityResourceInit = {
        resourcePath : resPathHumidity,
        resourceTypes: [ resTypeHumidity ],
        interfaces   : [ 'oic.if.baseline' ],
        discoverable : true,
        observable   : false,
        properties   : humidityProperties
    };

// OCF Resource Registration
server.register(temperatureResourceInit).then(function(resource) {
    console.log("Temperature sensor registered");
    temperatureResource = resource;
}).catch(function(error) {
    console.log('Registration failure: ' + error.name);
});

server.register(humidityResourceInit).then(function(resource) {
    console.log("Humidity sensor registered");
    humidityResource = resource;
}).catch(function(error) {
    console.log('Registration failure: ' + error.name);
});

// Register Listeners
server.on('retrieve', function(request, observe) {
    if (request.target.resourcePath == resPathTemperature) {
        request.respond(temperatureProperties);
    } else if (request.target.resourcePath == resPathHumidity) {
        request.respond(humidityProperties);
    }
});

console.log('Lesson3: Expose temperature & humidity through OCF...');

// Periodically sense temperature & humidity, and update the resource properties
setInterval(function() {
    readTemperature(function(temperature) {
        console.log('temperature: ' + temperature);
        temperatureProperties.temperature = temperature;
    });
    setTimeout(readHumidity, 1000, function(humidity) {
        console.log('humidity: ' + humidity);
        humidityProperties.humidity = humidity;
    });
}, 2000);

/* Start the OCF stack */
ocf.start();
