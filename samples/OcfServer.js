var server = require('ocf').server;

console.log("Started OCF server");

var MyProperties = {
    state: true
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
            //MyProperties.state = request.resource.properties.state;
        } else {
            console.log("request.properties does not exist");
        }
        //request.respond(MyProperties);
        server.notify(MyResource);
    });
}).catch(function(error) {
    console.log("Error registering");
});
