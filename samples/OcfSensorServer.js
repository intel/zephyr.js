var MY_ID = '02';
var BLE_ADDR = 'E3:C4:7F:24:B3:' + MY_ID;
//setBleAddress(BLE_ADDR);
var server = require('ocf').server;

var aio = require('aio');

var pinA = aio.open({ device: 0, pin: pins.A0 });

console.log("Started OCF server");

var MyProperties = {
    state: true,
    power: 10,
    sensor: null
}

var resourceInit = {
    resourcePath: "/a/light",
    resourceTypes: ["core.light"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: MyProperties
}

var MyResource = null;

server.register(resourceInit).then(function(resource) {
    console.log("Registered resource");
    MyResource = resource;
    server.on('retrieve', function(request, observe) {
        MyProperties.state = (MyProperties.state) ? false : true;
        MyProperties.sensor = pinA.read();
        console.log("on('retrieve'): request.target.resourcePath=" +
                request.target.resourcePath + " observe=" + observe);
        request.respond(MyProperties).then(function() {
            console.log("respond success");
        }).catch(function(error) {
            console.log("respond error: " + error.name);
        });
    });
    server.on('update', function(request) {
        console.log("on('update'): request.target.resourcePath=" + request.target.resourcePath);
        if (request.resource.properties) {
            console.log("properties.state=" + request.resource.properties.state);
            MyProperties = request.resource.properties;
        } else {
            console.log("request.properties does not exist");
        }
        request.respond(MyProperties);
    });
}).catch(function(error) {
    console.log("Error registering");
});
