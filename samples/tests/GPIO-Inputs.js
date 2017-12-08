// Copyright (c) 2017, Intel Corporation.

// Test code for multiple boards that reports input on any of the GPIOs.

console.log('Test external GPIOs as inputs...\n');

console.log('Wire a button to each input in turn!\n');

var board = require('board');
var gpio = require('gpio');

// index into the testpin string names that contains the controller id
//   For example, if the pin names are of the format "GPIOA.3", 'A' is the
//   controller id and it's at offset 4 in the string.
// If -1, we will just try to set up an interrupt on every pin; latest ones
//   will probably win.
var controllerIndex = -1;

// id of the controller we want to test interrupts on
// NOTE: set to A, B, or C for F411RE
var intController = 'A';

var testpins;
if (board.name == 'arduino_101') {
    // expected results: works with IO2-5, IO7-8, IO10-13; IO0/1/6/9 can only
    //   be used from ARC side
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
}
else if (board.name == 'frdm_k64f') {
    // change to 0 to test with full pin names below
    if (1) {
        // expected results: all pins but D8 work (not sure why not D8)
        testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
    }
    else {
        // expected results: With updated pinmux settings merged into ZJS,
        //   D0 - D15 all work, as well as onboard switch SW2; 10 other pins
        //   on the inner side of the left and right connectors as list below
        //   also work; if analog inputs are not used as such they can be used
        //   as GPIOs too, as in this sample. However, onboard switch SW3 still
        //   doesn't work. PTE26 is both wired to one of the pins on the inner
        //   right connector and to the green LED of the RGB and works as an
        //   input too.
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
            'GPIO_4.24', 'GPIO_4.25', 'GPIO_3.1',  'GPIO_3.3', 'GPIO_3.2',
            'GPIO_3.0',  'GPIO_2.4',  'GPIO_2.12', 'GPIO_2.3',  'GPIO_2.2',
            'GPIO_0.2',  'GPIO_1.23', 'GPIO_0.1',  'GPIO_1.9',  'GPIO_2.17',
            'GPIO_2.16',
            // onboard switch SW2 (INT1)
            'GPIO_2.6',
            // onboard switch SW3 (INT2)
            'GPIO_0.4',
        ];
    }
}
else if (board.name == 'nucleo_f411re') {
    // NOTE: On F411RE at least, you can't set up an interrupt to detect a
    //   rising edge on more than one pin with the same number, e.g. GPIOA.9
    //   and GPIOB.9. The one you set up last will win. For that reason, to
    //   test the interrupt part of pin behavior you need to set
    //   intController above to either 'A', 'B', or 'C'. All the other pins
    //   will still report changes when we read them on a timer, but only the
    //   controller you specify will set up rising edge interrupts.
    // expected results:
    //   See the blue boxes in this diagram for all the GPIO pin locations:
    //     http://docs.zephyrproject.org/_images/nucleo_f411re_morpho.png
    //   GPIOA.2 and GPIOA.3 are configured for serial comms in default Zephyr
    //     config and can't be used; attempting to can cause a hang.
    //   GPIOA.13 and GPIOA.14 don't seem to work as inputs
    //   GPOIB.11 doesn't show an external connection in docs
    //   GPIOC.14 and GPIOC.15 don't seem to work as inputs
    //   GPIOD.2, GPIOH.0, and GPIOH.1 don't work; the controllers are disabled
    //     by default in Zephyr configuration, haven't tried enabling yet
    //   All other pins in GPIO[A-C].[0-15] work as inputs, and with interrupts
    //     as long as you avoid conflicts between pins with the same number.
    //   NOTE: The blue "user button" on board is connected to GPIOC.13
    controllerIndex = 4;
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

// open all pins for input and sign up for rising edge events
var pincount = testpins.length;
var gpios = [];
var debounce = false;
for (var i = 0; i < pincount; i++) {
    try {
        var init = {
            pin: testpins[i],
            mode: 'in',
            state: 'pulldown'
        };
        if (controllerIndex < 0 ||
            testpins[i][controllerIndex] == intController) {
            init['edge'] = 'rising';
        }
        gpios[i] = gpio.open(init);
        gpios[i].onchange = (function (event) {
            var name = testpins[i];
            var pin = i;
            function logEvent(event) {
                if (debounce || gpios[pin].read() == 0)
                    return;
                debounce = true;
                console.log('Rising edge event on pin', name);
                setTimeout(resetDebounce, 250);
            }
            return logEvent;
        })();
    }
    catch (e) {
        console.log('Error opening pin ' + testpins[i] + ':', e.message);
    }
}

function resetDebounce() {
    debounce = false;
}

// read initial states
var last = [];
for (var i = 0; i < pincount; i++) {
    if (gpios[i]) {
        last[i] = gpios[i].read();
    }
}

// check for changes to pin state; this catches pins where interrupts don't work
setInterval(function() {
    for (var i = 0; i < pincount; i++) {
        if (gpios[i]) {
            var val = gpios[i].read();
            if (val != last[i]) {
                console.log('Pin ' + testpins[i] + ' changed to ' + val);
                last[i] = val;
            }
        }
    }
}, 250);
