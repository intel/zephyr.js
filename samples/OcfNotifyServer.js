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
var timer = null;

server.on('retrieve', function(request, observe) {
    console.log("retrieve");
    request.respond(MyProperties);
});
server.on('update', function(request) {
    console.log("update");
});

server.register(resourceInit).then(function(resource) {
    console.log("Registered resource");
    MyResource = resource;
    timer = setInterval(function() {
        MyProperties.state = (MyProperties.state) ? false : true;
        server.notify(MyResource);
    }, 1000);
}).catch(function(error) {
    console.log("Error registering");
});
