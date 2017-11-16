// Copyright (c) 2017, Intel Corporation.

// Test code for multiple boards that reports input on any of the GPIOs.

console.log('Test external GPIOs as inputs...\n');

console.log('Wire a button to each input in turn!\n');

var board = require('board');
var gpio = require('gpio');

var testpins;
if (board.name == 'arduino_101') {
    // expected results: works with IO2-5, IO7-8, IO10-13; IO0/1/6/9 can only
    //   be used from ARC side
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
}
else if (board.name == 'frdm_k64f') {
    // expected results: all pins but D8 work (not sure why not D8)
    testpins = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
}
else if (board.name == 'nucleo_f411re') {
    // expected results: pins D2-D15 all work as inputs, but only
    //   D3,5,6 and D11-15 will give rising edge interrupts
    // NOTE: D0 (GPIOA.3) must be left out or it will corrupt serial output;
    //   D1 (GPIOA.2) must be left out or it will hang on pin configure
    testpins = ['GPIOA.10', 'GPIOB.3', 'GPIOB.5', 'GPIOB.4', 'GPIOB.10',
                'GPIOA.8', 'GPIOA.9', 'GPIOC.7', 'GPIOB.6', 'GPIOA.7',
                'GPIOA.6', 'GPIOA.5', 'GPIOB.9', 'GPIOB.8', 'GPIOC.4',
                'GPIOB.13'];
}

var debounce = false;

var pincount = testpins.length;
var gpios = [];
for (var i = 0; i < pincount; i++) {
    try {
        gpios[i] = gpio.open({pin: testpins[i], mode: 'in', edge: 'rising',
                              state: 'pulldown'});
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
