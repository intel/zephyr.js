var server = require('ocf').server;

console.log("Started OCF server");

var MyProperties = {
    state: true
}

var resourceInit = {
    resourcePath: "/light/1",
    resourceTypes: ["oic.r.light"],
    interfaces: ["/oic/if/w"],
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
        var err = null;
        server.respond(request, err, resourceInit).then(function() {
            console.log("respond success");
        }).catch(function(error) {
            console.log("respond error: " + error.name);
        });
    });
    server.on('update', function(request) {
        console.log("on('update'): request.target.resourcePath=" + request.target.resourcePath);
        if (request.resource.properties) {
            console.log("properties.state=" + request.resource.properties.state);
        } else {
            console.log("request.properties does not exist");
        }
        server.notify(MyResource);
    });
    server.on('delete', function() {
        console.log("DELETE event");
    });
    server.on('create', function() {
        console.log("CREATE event");
    });
}).catch(function(error) {
    console.log("Error registering");
});
