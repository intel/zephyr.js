// Copyright (c) 2017, Intel Corporation.

// Test code for multiple boards that toggles each of the GPIO pins as outputs
// in a sequence, indefinitely.

console.log("Test external GPIOs as outputs...\n");

console.log('Wire an external LED to each output in turn!');

var board = require('board');
var gpio = require('gpio');

var testpins;
if (board.name == 'arduino_101') {
    // expected results: works with IO2, IO4, IO7, IO12-13; not sure why IO8,
    //   IO10-11 not working; others can only be used from ARC side
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
}
else if (board.name == 'frdm_k64f') {
    // change to 0 to test with full pin names below
    if (1) {
        // expected results: D0-D15 will blink (except D8, not sure why)
        testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    }
    else {
        // expected results: With updated pinmux settings merged into ZJS, all
        //   the pins below work. PTE26 is both wired to one of the pins on the
        //   inner right bank and to the green LED of the RGB. The onboard LEDs
        //   appear to be active-low.
        // The diagrams on the page below show the various GPIO pins:
        //     https://os.mbed.com/platforms/FRDM-K64F/
        testpins = [
            // outer left pins (analog inputs)
            'GPIO_1.2', 'GPIO_1.3', 'GPIO_1.10', 'GPIO_1.11', 'GPIO_2.11',
            'GPIO_2.10',
            // inner left pins
            'GPIO_1.20',
            // inner right pins
            'GPIO_4.26', 'GPIO_2.5',  'GPIO_2.7',  'GPIO_2.0',  'GPIO_2.9',
            'GPIO_2.8',  'GPIO_2.1',  'GPIO_1.19', 'GPIO_1.18',
            // outer right pins (D0 - D15)
            //   (reverse order from the top down in the diagram)
            'GPIO_4.24', 'GPIO_4.25', 'GPIO_3.1',  'GPIO_3.3', 'GPIO_3.2',
            'GPIO_3.0',  'GPIO_2.4',  'GPIO_2.12', 'GPIO_2.3',  'GPIO_2.2',
            'GPIO_0.2',  'GPIO_1.23', 'GPIO_0.1',  'GPIO_1.9',  'GPIO_2.17',
            'GPIO_2.16',
            // onboard RGB LED
            'GPIO_1.22', 'GPIO_4.26', 'GPIO_1.21'
        ];
    }
}
else if (board.name == 'nucleo_f411re') {
    // expected results: pins D2-D15 will blink; onboard LED 'LD2' blinks in
    //   sync with D13; D0/D1 are set as serial so can't be used
    //   GPIOA.13 and GPIOA.14 work oddly; when off the LED still shows a faint
    //     glow so there might be a pullup resistor; probably best not to use
    //   GPOIB.11 doesn't show an external connection in docs
    //   GPIOC.14 and GPIOC.15 don't seem to work
    //   GPIOD.2, GPIOH.0, and GPIOH.1 don't work; the controllers are disabled
    //     by default in Zephyr configuration, haven't tried enabling yet
    // NOTE: D0 (GPIOA.3) and D1 (GPIOA.2) must be left out or they will
    //   corrupt serial output
    testpins = ['GPIOA.0',  'GPIOA.1',  // 'GPIOA.2',  'GPIOA.3',
                'GPIOA.4',  'GPIOA.5',  'GPIOA.6',  'GPIOA.7',
                'GPIOA.8',  'GPIOA.9',  'GPIOA.10', 'GPIOA.11',
                'GPIOA.12', 'GPIOA.13', 'GPIOA.14', 'GPIOA.15',
                'GPIOB.0',  'GPIOB.1',  'GPIOB.2',  'GPIOB.3',
                'GPIOB.4',  'GPIOB.5',  'GPIOB.6',  'GPIOB.7',
                'GPIOB.8',  'GPIOB.9',  'GPIOB.10', 'GPIOB.11',
                'GPIOB.12', 'GPIOB.13', 'GPIOB.14', 'GPIOB.15',
                'GPIOC.0',  'GPIOC.1',  'GPIOC.2',  'GPIOC.3',
                'GPIOC.4',  'GPIOC.5',  'GPIOC.6',  'GPIOC.7',
                'GPIOC.8',  'GPIOC.9',  'GPIOC.10', 'GPIOC.11',
                'GPIOC.12', 'GPIOC.13', 'GPIOC.14', 'GPIOC.15',
    ];
}

// open all pins for output
var pincount = testpins.length;
var gpios = [];
for (var i = 0; i < pincount; i++) {
    try {
        gpios[i] = gpio.open({pin: testpins[i]});
    }
    catch (e) {
        console.log('Error opening pin ' + testpins[i] + ':', e.message);
    }
}

// toggle all pins off in sequence, then on in sequence, and so on
var tick = 75, toggle = 0;
var index = 0;
setInterval(function () {
    var gpio = gpios[index++];
    if (gpio) {
        gpio.write(toggle);
    }
    if (index >= pincount) {
        toggle = 1 - toggle;
        index = 0;
    }
}, tick);
