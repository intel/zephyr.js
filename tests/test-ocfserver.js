// Copyright (c) 2017, Intel Corporation.

console.log("Test OCF server");

var ocf = require("ocf");
var assert = require("Assert.js");

var server = ocf.server;
assert(typeof server === "object" && !!server,
       "OCFObject: OCF server be defined");

ocf.device = {
    name: "TestOCFServer",
    coreSpecVersion: "2.0",
    dataModels: "2.5"
}

ocf.platform = {
    manufacturerName: "TestManufacturer",
    osVersion: "16.04",
    model: "Arduino101",
    manufacturerURL: "testurl.com",
    manufacturerDate: "11-11-2011",
    platformVersion: "5.0",
    firmwareVersion: "9.0",
    supportURL: "testurl.com/support"
}

var TestProperties = {
    str: "testProperties",
    state: true,
    num: 0
}

var resourceInit = {
    resourcePath: "/test/OCFserver",
    resourceTypes: ["core.test"],
    interfaces: ["/oic/if/rw"],
    discoverable: true,
    observable: true,
    secure: true,
    slow: false,
    properties: TestProperties
}

var retrieveFlage = true;
var updateFlage = true;

function check_attribute(attribute, attributeName, type, attributeValue) {
    assert(typeof attribute === type && attribute !== null &&
           attribute === attributeValue,
           "OCFResource: server register " + attributeName +
           " as '" + attributeValue + "'");
}

function onRetrieve(request, observe) {
    if (retrieveFlage) {
        assert(typeof request === "object" && !!request &&
               typeof observe === "boolean" && observe !== null,
               "OCFServer: callback for event 'retrieve'");

        assert(typeof request.target.resourcePath === "string" &&
               request.target.resourcePath !== undefined,
               "OCFServer: request target/destination resource");
    }

    console.log("on('retrieve'): request.target.resourcePath = " +
                request.target.resourcePath + " observe = " + observe);

    TestProperties.num = TestProperties.num + 1;
    console.log("retrieve properties.num = " + TestProperties.num);

    request.respond(TestProperties).then(function() {
        if (retrieveFlage) {
            assert(true, "OCFServer: respond for event 'retrieve'");

            retrieveFlage = false;
        }
    }).catch(function(error) {
        if (retrieveFlage) {
            assert(false, "OCFServer: respond for event 'retrieve'");

            retrieveFlage = false;
        }
    });
}

function onUpdate(request) {
    if (updateFlage) {
        assert(typeof request === "object" && !!request,
               "OCFServer: callback for event 'update'");

        assert(typeof request.resource.properties === "object" &&
               request.resource.properties !== undefined,
               "OCFServer: request source/origin resource");
    }

    console.log("on('update'): request.target.resourcePath = " +
                request.target.resourcePath);

    if (request.resource.properties) {
        if (TestProperties.state !== undefined &&
            request.resource.properties.state !== undefined) {
            TestProperties.state = request.resource.properties.state;

            console.log("update properties.state = " + TestProperties.state);
        }
    }

    request.respond(TestProperties).then(function() {
        if (updateFlage) {
            assert(true, "OCFServer: respond for event 'update'");

            updateFlage = false;
            assert.result();
        }
    }).catch(function(error) {
        if (updateFlage) {
            assert(false, "OCFServer: respond for event 'update'");

            updateFlage = false;
            assert.result();
        }
    });
}

server.on("retrieve", onRetrieve);
server.on("update", onUpdate);

server.register(resourceInit).then(function(resource) {
    check_attribute(resource.resourcePath, "resource.resourcePath",
                    "string", "/test/OCFserver");
    check_attribute(resource.properties.str, "resource.properties.str",
                    "string", "testProperties");
    check_attribute(resource.properties.state, "resource.properties.state",
                    "boolean", true);
    check_attribute(resource.properties.num, "resource.properties.num",
                    "number", 0);

    assert(true, "OCFServer: register OCF server");

    server.notify(resource);
}).catch(function(error) {
    assert(false, "OCFServer: register OCF server");
});
