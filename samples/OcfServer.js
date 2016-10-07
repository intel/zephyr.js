var server = require('ocf').server;

print("Started OCF server");

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
	print("Registered resource");
	MyResource = resource;
	server.on('retrieve', function(request, observe) {
		MyProperties.state = (MyProperties.state) ? false : true;
		print("on('retrieve'): request.target.resourcePath=" + request.target.resourcePath + " observe=" + observe);
		var err = null;
		server.respond(request, err, resourceInit).then(function() {
			print("respond success");
		}).catch(function(error) {
			print("respond error: " + error.name);
		});
	});
	server.on('update', function(request) {
		print("on('update'): request.target.resourcePath=" + request.target.resourcePath);
		if (request.resource.properties) {
			print("properties.state=" + request.resource.properties.state);
		} else {
			print("request.properties does not exist");
		}
		server.notify(MyResource);
	});
	server.on('delete', function() {
		print("DELETE event");
	});
	server.on('create', function() {
		print("CREATE event");
	})
}).catch(function(error) {
	print("Error registering");
});

