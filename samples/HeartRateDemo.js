// Copyright (c) 2016, Intel Corporation.

// This implementation includes the source code from PulseSensor.com,
// which is licensed under MIT license.
// https://github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino

// Reimplementation of the heart rate monitor sample
// to measure heart rate using a pulse sensor on an Arduino 101
// The measured data will then be sent to a connected smart phone over BLE,
// you can then use an BLE app like "nRF Toolbox" on Android to
// track the heart rate changes.

// Hardware Requirements
//   - A pulse sensor that reports pulse rate using an analog voltage,
//       the one in this sample uses the sensor from www.pulsesensor.com
//   - A Grove LCD
//   - Two pull-up resistors for SDA and SCL, we use two 10k resistors,
//       you should choose resistors that will work with the LCD hardware
//       you have, the ones we listed here are the ones that have known
//       to work for us, so your mileage may vary if you have different LCD
// Wiring:
//   For the pulse sensor:
//     - Wire the Pulse sensor's RED to 3.3V or 5V
//     - Wire the Pulse sensor's BLACK to GND
//     - Wire the Pulse sensor's PURPLE to Arduino A1
//   For the LCD:
//     - Wire SDA on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SCL on the LCD to the pull-up resistor and connect that resistor to power (VCC)
//     - Wire SDA on the LCD to SDA on the Arduino 101
//     - Wire SCL on the LCD to SCL on the Arduino 101
//     - Wire power(5V) and ground accordingly

var aio = require("aio");
var ble = require("ble");
var grove_lcd = require("grove_lcd");
var pins = require("arduino101_pins");

// heart rate calculation
var IBI = 600;               // value holds the time interval between beats! Must be seeded!
var pulse = false;           // "True" when User's live heartbeat is detected. "False" when not a "live beat".
var rate = [];               // array to hold last ten IBI values
var sampleCounter = 0;       // used to determine pulse timing
var lastBeatTime = 0;        // used to find IBI
var P = 2048;                // used to find peak in pulse wave, seeded
var T = 2048;                // used to find trough in pulse wave, seeded
var thresh = 2100;           // used to find instant moment of heart beat, seeded
var amp = 400;               // used to hold amplitude of pulse waveform, seeded
var firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
var secondBeat = false;      // used to seed rate array so we startup with reasonable BPM

// grove lcd
var fadeRate = 0;
var color = [];
color[0] = [  0,   0, 255];  // blue
color[1] = [  0, 255,   0];  // green
color[2] = [255, 255,   0];  // yellow
color[3] = [ 55,   0,   0];  // red

// setInterval loop
var interval = 10;
var count = 0;
var checkTime = 2000;
var colorIndex = 0;

// pins
var pin = aio.open({ device: 0, pin: pins.A1 });

// Bluetooth LE
var deviceName = 'Zephyr Heartrate Monitor';

var gapUuid = '1800';
var gapDeviceNameUuid = '2a00';
var gapAppearanceUuid = '2a01';

var hrsUuid = '180d';
var hrsMeasurementUuid = '2a37';

var gapDeviceNameCharacteristic = new ble.Characteristic({
    uuid: gapDeviceNameUuid,
    properties: ['read'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: deviceName
        })
    ]
});

var gapAppearanceCharacteristic = new ble.Characteristic({
    uuid: gapAppearanceUuid,
    properties: ['read'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: '0341'
        })
    ]
});

var heartRateCharacteristic = new ble.Characteristic({
    uuid: hrsMeasurementUuid,
    properties: ['read', 'notify'],
    descriptors: [
        new ble.Descriptor({
            uuid: '2901',
            value: 'Heart Rate'
        })
    ]
});

heartRateCharacteristic.onSubscribe = function(maxValueSize,
                                               updateValueCallback) {
    console.log("Subscribed to heart rate change.");
    this._onChange = updateValueCallback;
};

heartRateCharacteristic.onUnsubscribe = function() {
    console.log("Unsubscribed to heart rate change.");
    this._onChange = null;
};

heartRateCharacteristic.valueChange = function(value) {
    var data = new Buffer(2);

    // we don't support writeUInt16() yet
    // so manually shift the bytes
    data.writeUInt8(0x06, 0);
    data.writeUInt8(value, 1);

    if (this._onChange) {
        this._onChange(data);
    }
};

function measureHeartRate(signal) {
    var HR = 0;

    sampleCounter += 10;                        // keep track of the time in mS with this variable
    var N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise

    // find the peak and trough of the pulse wave
    if (signal < thresh && N > (IBI / 5) * 3) { // avoid dichrotic noise by waiting 3/5 of last IBI
        if (signal < T) {                       // T is the trough
            T = signal;                         // keep track of lowest point in pulse wave
        }
    }

    if (signal > thresh && signal > P) {        // thresh condition helps avoid noise
        P = signal;                             // P is the peak
    }                                           // keep track of highest point in pulse wave

    // NOW IT'S TIME TO LOOK FOR THE HEART BEAT
    // signal surges up in value every time there is a pulse
    if (N > 250 && N < 2000) {                  // avoid noise. 30 bpm < possible human heart beat < 240
        if ((signal > thresh) && (pulse == false) && (N > (IBI / 5) * 3)) {
            pulse = true;                       // set the pulse flag when we think there is a pulse
            IBI = sampleCounter - lastBeatTime; // measure time between beats in mS
            lastBeatTime = sampleCounter;       // keep track of time for next pulse

            if (secondBeat) {                   // if this is the second beat, if secondBeat == TRUE
                console.log("Measured 2nd beat");
                secondBeat = false;             // clear secondBeat flag
                for(var i = 0; i <= 9; i++) {   // seed the running total to get a realisitic BPM at startup
                    rate[i] = IBI;
                }
            }

            if (firstBeat) {                    // if it's the first time we found a beat, if firstBeat == TRUE
                console.log("Measured 1st beat");
                firstBeat = false;              // clear firstBeat flag
                secondBeat = true;              // set the second beat flag
                return 0;                       // IBI value is unreliable so discard it
            }

            // keep a running total of the last 10 IBI values
            var runningTotal = 0;               // clear the runningTotal variable

            for (var i = 0; i <= 8; i++) {      // shift data in the rate array
                rate[i] = rate[i + 1];          // and drop the oldest IBI value
                runningTotal += rate[i];        // add up the 9 oldest IBI values
            }

            rate[9] = IBI;                      // add the latest IBI to the rate array
            runningTotal += rate[9];            // add the latest IBI to runningTotal
            runningTotal /= 10;                 // average the last 10 IBI values
            HR = 60000 / runningTotal;          // how many beats can fit into a minute? that's BPM!
        }
    }

    if (signal < thresh && pulse == true) {     // when the values are going down, the beat is over
        pulse = false;                          // reset the pulse flag so we can do it again
        amp = P - T;                            // get amplitude of the pulse wave
        thresh = (amp * 2 / 3) + T;             // set thresh at 67% of the amplitude
        P = thresh;                             // reset these for next time
        T = thresh;
    }

    if (N > 2500) {                             // if 2.5 seconds go by without a beat
        if (glcd) {
            glcd.clear();
            glcd.print("Lost signal");
        }
        thresh = 2048;                          // set thresh default
        P = 2048;                               // set P default
        T = 2048;                               // set T default
        lastBeatTime = sampleCounter;           // bring the lastBeatTime up to date
        firstBeat = true;                       // set these to avoid noise
        secondBeat = false;                     // when we get the heartbeat back
   }

    return HR | 0;                              // return integer instead of float
}

function showHeartBeatUsingFadeEffect(index) {
    var red, green, blue;

    if (!glcd || fadeRate < 0 || index < 0 || index > 3) {
        return;
    }

    red = (color[index][0] * fadeRate / 255) & 0xff;
    green = (color[index][1] * fadeRate / 255) & 0xff;
    blue = (color[index][2] * fadeRate / 255) & 0xff;
    glcd.setColor(red, green, blue);

    fadeRate -= 15;
}

ble.on('stateChange', function(state) {
    console.log("BLE state: " + state);

    if (state === 'poweredOn') {
        ble.startAdvertising(deviceName, [hrsUuid]);
    } else {
        if (state === 'unsupported') {
            console.log("BLE not enabled on board");
        }
        ble.stopAdvertising();
    }
});

ble.on('advertisingStart', function(error) {
    console.log('advertisingStart: ' + (error ? error : 'success'));

    if (error) {
        return;
    }

    ble.setServices([
        new ble.PrimaryService({
            uuid: gapUuid,
            characteristics: [
                gapDeviceNameCharacteristic,
                gapAppearanceCharacteristic
            ]
        }),
        new ble.PrimaryService({
            uuid: hrsUuid,
            characteristics: [
                heartRateCharacteristic
            ]
        })
    ]);
});

setInterval(function() {
    var isValid = false;

    var rawSignal = pin.read();
    var heartRate = measureHeartRate(rawSignal);

    if (heartRate > 0) {
        if (heartRate > 50 && heartRate < 200) {
            // normal heart rate
            isValid = true;
        } else {
            // for abnormal heartbeat, check if it is stable for two seconds
            checkTime -= interval;
            if (checkTime < 0) {
                isValid = true;
            }
        }

        if (isValid) {
            // print to LCD screen
            if (glcd) {
                glcd.clear();
                glcd.setCursorPos(0, 0);
                glcd.print("HR: " + heartRate + " BPM" );
            }

            // send notification over BLE
            heartRateCharacteristic.valueChange(heartRate);

            // reset fading effect and check time for abnormal heart beat
            fadeRate = 255;
            checkTime = 2000;
        }

        if (heartRate < 60) {
            colorIndex = 0;
        }
        else if (heartRate < 80) {
            colorIndex = 1;
        }
        else if (heartRate < 100) {
            colorIndex = 2;
        }
        else {
            colorIndex = 3;
        }
    }

    // blink the LCD to show the heartbeat
    showHeartBeatUsingFadeEffect(colorIndex);
}, interval);

// Initialize Grove LCD
var glcd = grove_lcd.init();

var funcConfig = grove_lcd.GLCD_FS_ROWS_2
               | grove_lcd.GLCD_FS_DOT_SIZE_LITTLE
               | grove_lcd.GLCD_FS_8BIT_MODE;
glcd.setFunction(funcConfig);

var displayStateConfig = grove_lcd.GLCD_DS_DISPLAY_ON;
glcd.setDisplayState(displayStateConfig);

glcd.clear();
glcd.print("Heart Rate Demo");
