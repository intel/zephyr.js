// Copyright (c) 2017, Intel Corporation.

// OCF server which has one resource and two properties. One property (sensor)
// is ready only and other one (light) is read-write.

var ocf = require('ocf');
var server = ocf.server;

var aio = require('aio');
var gpio = require('gpio');
var pins = require('arduino101_pins');

var led = gpio.open({pin: 'LED2', activeLow: true});
var pinA = aio.open({ device: 0, pin: pins.A0 });

console.log('Started OCF server');

var MyProperties = {
    sensor: null,
    light: 0
}

var resourceInit = {
    resourcePath: '/a/sensor',
    resourceTypes: ['core.sensor'],
    interfaces: ['/oic/if/rw'],
    discoverable: true,
    observable: true,
    properties: MyProperties
}

var MyResource = null;

server.register(resourceInit).then(function(resource) {
    console.log('Registered resource');
    MyResource = resource;
    server.on('retrieve', function(request, observe) {
        MyProperties.sensor = pinA.read();
        console.log("on('retrieve'): request.target.resourcePath=" +
                request.target.resourcePath + ' observe=' + observe);
        request.respond(MyProperties).then(function() {
            console.log('respond success');
        }).catch(function(error) {
            console.log('respond error: ' + error.name);
        });
    });
    server.on('update', function(request) {
        console.log("on('update'): request.target.resourcePath=" + request.target.resourcePath);
        if (request.resource.properties) {
            var recvProps = request.resource.properties;
            if (recvProps.light !== undefined) {
                light = recvProps.light ? 1 : 0;
                MyProperties.light = light;
                led.write(light);
                console.log('properties.light=' + light);
            }
        } else {
            console.log('request.properties does not exist');
        }
        request.respond(MyProperties);
    });
}, function(error) {
    console.log("Error registering");
});

ocf.start();
