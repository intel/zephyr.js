
var ble = require ("ble");

// setup callbacks
ble.on('stateChange', function(state) {
    print ('State change: ' + state);
    if (state === 'poweredOn') {
        //TODO: Need to define what parameters should go into startAdvertising
        ble.startAdvertising();
    } else {
        ble.stopAdvertising();
    }
});

ble.on('accept', function(clientAddress) {
    print ("Accepted connection from address: " + clientAddress);
});

ble.on('disconnect', function(clientAddress) {
    print ("Disconnected from address: " + clientAddress);
});

ble.on('advertisingStart', function(error) {
    if (error) {
        print ("Advertising start error: " + error);
        return;
    }
    print ("Advertising start success");
    //ToDO: Define your new service
});

// enable ble
ble.enable();
