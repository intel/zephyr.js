// Copyright (c) 2017, Intel Corporation.

var ocf = require('ocf');
var server = ocf.server;

var aio = require('aio');

var light = aio.open('A0');
var temp = aio.open('A1');

console.log("Started OCF server");

var lightResource = {
    light: null
};

var tempResource = {
    temp: null
};

var lightInit = {
    resourcePath: "/a/light",
    resourceTypes: ["core.light"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: lightResource
};

var tempInit = {
    resourcePath: "/a/temp",
    resourceTypes: ["core.temp"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    properties: tempResource
};

server.register(lightInit).then(function(resource) {
    console.log("Light resource registered");
});

server.register(tempInit).then(function(resource) {
    console.log("Temperature resource registered");
});

server.on('retrieve', function(request, observe) {
    if (request.target.resourcePath == "/a/light") {
        lightResource.light = light.read();
        request.respond(lightResource);
    } else if (request.target.resourcePath == "/a/temp") {
        tempResource.temp = temp.read();
        request.respond(tempResource);
    } else {
        console.log("Resource requested does not exist");
    }
});

ocf.start();
