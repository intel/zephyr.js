// Copyright (c) 2017, Intel Corporation.

var ocf = require("ocf");
var server = ocf.server;

ocf.device = {
	name: "MyOCFServer",
	coreSpecVersion: "2.0",
	dataModels: "2.5"
}

ocf.platform = {
	manufacturerName: "MyManufacturer",
	osVersion: "10.0",
	model: "Arduino101",
	manufacturerURL: "myurl.com",
	manufacturerDate: "11-11-2011",
	platformVersion: "5.0",
	firmwareVersion: "9.0",
	supportURL: "myurl.com/support"
}

console.log("Started OCF server");

var MyProperties = {
    state: true,
    power: 10
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
    console.log("Registered resource. UUID: " + ocf.device.uuid);
    MyResource = resource;
    server.on("retrieve", function(request, observe) {
        MyProperties.state = (MyProperties.state) ? false : true;
        console.log("on('retrieve'): request.target.resourcePath=" +
                request.target.resourcePath + " observe=" + observe);
        request.respond(MyProperties).then(function() {
            console.log("respond success");
        }, function(error) {
            console.log("respond error: " + error.name);
        });
    });
    server.on("update", function(request) {
        console.log("on('update'): request.target.resourcePath=" + request.target.resourcePath);
        if (request.resource.properties) {
            console.log("properties.state=" + request.resource.properties.state);
            MyProperties.state = request.resource.properties.state;
        } else {
            console.log("request.properties does not exist");
        }
        request.respond(MyProperties);
    });
}, function(error) {
    console.log("Error registering");
});

ocf.start();
