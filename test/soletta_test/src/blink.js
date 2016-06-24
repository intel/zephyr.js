print("Start of GPIO sample run...");

var pinOnboardLed = 8;
var pinExternalLed = 16;

var toggleOnboardLed = 1;
var toggleExternalLed = 1;

GPIO.pinConfigure(pinOnboardLed);
GPIO.pinConfigure(pinExternalLed);

setInterval(function() {
    print("blink the onboard led");
    GPIO.pinWrite(pinOnboardLed, toggleOnboardLed);
    toggleOnboardLed = 1 - toggleOnboardLed;
}, 1000);

setInterval(function() {
    print("blink the external led");
    GPIO.pinWrite(pinExternalLed, toggleExternalLed);
    toggleExternalLed = 1 - toggleExternalLed;
}, 2000);
