# Adding new modules

ZJS provides an easy mechanism to add new native modules. When ZJS is built, it
generates a JSON dependency tree which will be used to include only the native
modules that the JavaScript needs. It is also possible to force the inclusion
of certain modules (e.g. AShell). Inside the `src/` directory there are .c/.h
native module files that, for the most part, have an accompanying .json file.
The .json file provides several details:

* Module name
* Require/Constructor
* Configuration options for ZJS, Zephyr and JerryScript
* Source/Header files
* Init/cleanup functions
* Target restrictions
* Any required dependencies

### Module name

The "module" property of the JSON file should be a string that other modules
can use to declare as a dependency. This is its only purpose, but care should be
taken as to not duplicate module names, otherwise one of the duplicates will
not be included in the JSON tree generation.

Example (IPM module):
```
{
    "module": "ipm",
    ...
}
```
Any other module can then include "ipm" as a dependency:
```
{
    "module": "i2c",
    "depends": ["ipm"],
    ...
}
```

### Require and Constructor

A native module can be included in two ways, either as a dependency or through
the "require"/"constructor" properties.

The "require" property should be a string that is needed to include the module
in JavaScript, e.g.
```
{
    "require": "ocf",
    ...
}
```
The build will then include the above JSON file if it sees the following in
the JavaScript:
```
var ocf = require('ocf');
```
The constructor is very similar, except it will include a certain module if it
finds the use of the specified constructor. For example:
```
{
    "constructor": "Promise",
    ...
}
```
The build will then include this module if the "Promise" constructor is found.

There is one other option that can be used to include a module, the "object"
property. This will search for any use of "object" in the JavaScript. Currently
only the Console module uses this, as the "console" object is global:
{
    "module": "console",
    "object": "console",
    ...
}

### Configuration options

It is possible to specify configuration options for ZJS, Zephyr and JerryScript
via a native modules JSON file. The properties "zjs_config", "zephyr_conf",
and "jrs_config" do just that.

The "zjs_config" property is just an array of gcc compiler flags, e.g.
```
{
    "zjs_config": ["-DBUILD_MODULE_PWM"],
    ...
}
```

The "zephyr_conf" property is an object who's properties are possible Zephyr
targets. Each of these targets should be an array of Zephyr CONFIG_ flags. there
is a special property, "all", which will include the Zephyr flags for all
targets. For example, a simplified version of the "net" module:
```
{
    "module": "net",
    ...
    "zephyr_conf": {
        "all": ["CONFIG_NETWORKING=y"],
        "arduino_101": ["CONFIG_BLUETOOTH=y"],
        "qemu_x86": ["CONFIG_NET_SLIP_TAP=y"],
        "frdm_k64f": ["CONFIG_NET_L2_ETHERNET=y"]
    }
    ...
}
```
Finally, you can include certain JerryScript features with "jrs_config". This
property should be an array of strings that will be commented out of the
JerryScript feature file. The default build is minimal, so essentially all
JerryScript features are compiled out by default. This property is slightly
confusing because the names of the strings you include will say "DISABLE" in
them, but this is because the build system prepends a "#" in front of them
so they are NOT disabled. For example:
```
{
    "module": "promise",
    "jrs_config": ["CONFIG_DISABLE_ES2015_PROMISE_BUILTIN"],
    ...
}
```

### Source and Header files

These properties are what the build actually uses to compile your native
module. The "src" property should include a list of .c files that the build
needs, though this can be an object which will conditionally include certain
source files/options if certain properties in the JavaScript are used. Currently
only OCF uses this feature because it needs to selectively include the client,
server, or both only if its used in the JavaScript e.g.
```
var ocf = require('ocf');
var client = ocf.client;
```
In order to achieve this, the "src" property is an object with "client",
"server", and "common" properties. In the above example "client" would get
included ("common" is always included). Special ZJS config options can also
be included in the objects:
```
{
    "module": "ocf",
    ...
    "src": {
        "client": {
            "src": ["zjs_ocf_client.c"],
            "zjs_config": ["-DOC_CLIENT"]
        },
        "server": {
            "src": ["zjs_ocf_server.c"],
            "zjs_config": ["-DOC_SERVER"]
        },
        "common": ["zjs_ocf_common.c"]
    },
    ...
}
```
The normal "src" case is just an array of strings that are the .c files:
```
{
    "module": "pwm",
    ...
    "src": ["zjs_pwm.c"]
}
```

### Init and Cleanup functions

Each module, whether its a "require", "constructor", or "object" will have at
least an init function, and possibly a cleanup. The JSON format can take two
types, "init"/"cleanup" or "global_init"/"global_cleanup". For the most part, a
module that is included with "require" will just have the standard "init" and
"cleanup" properties. The function listed in "init" is what will be called when
the JavaScript calls `require(module)`.
```
{
    "module": "net",
    ...
    "init": ["zjs_net_init"],
    "cleanup": ["zjs_net_cleanup"]
}
```
The "global_init" and "global_cleanup" are used for modules that use a
"constructor" to initialize. In ZJS these constructors are global functions,
callable from anywhere. Because of this they need to be added to the global
object during initialization, which is what the "global_init" property lets
you do:
```
{
    "module": "console",
    "object": "console",
    ...
    "global_init": ["zjs_console_init"],
    "global_cleanup": ["zjs_console_cleanup"]
}
```

### Target Restrictions
In some cases a module is only supposed to be build for a certain target. In
these cases you can specify a "target" array which will exclude any BOARD
targets not included in the array:
```
{
    "module": "ipm",
    "targets": ["arduino_101"],
    ...
}
```

### Dependencies
In order to simplify the length of JSON files you can merge common JSON code
and include it in multiple modules as dependencies. This will create "virtual"
modules, explained further.
```
{
    "module": "aio",
    "depends": ["ipm"],
    ...
}
```

## Virtual Modules
Any JSON file that does not directly include any source files is considered a
"virtual" module. Virtual modules are quite useful and what makes this JSON
build system so easy to use. Instead of having to include every single
configuration option needed for a module you can split it up and just add a
"depends" property. For example aio, i2c and sensors all require IPM, so IPM
was split into its own JSON file and each just adds a "depends" entry.

Virtual modules can also be used to selectively build certain files for certain
targets. Using the I2C example again, the Arduino 101 require IPM, the the K64F
can just directly call I2C APIs. In this case we have a generic I2C JSON file
(zjs_i2c.json) which has platform agnostic information, but then has a "depends"
array which includes "i2c_k64f" and "i2c_a101". Then these two modules use the
"targets" property so that they wont be included if building for the wrong
target and include only target specific source files.

## Virtual Dependencies
A virtual dependency is one that can be fulfilled by one of a number of
different packages. Instead of directly "including" a specified module, the
JSON should specify default modules to be included for each board if the
virtual dependency goes unfulfilled otherwise.

The initial example of this is the "net-l2" virtual module. This means that
a datalink (layer 2) driver is required, but it's up to the user to decide at
build time which one it will be. For example, on the Arduino 101 the built-in
Bluetooth can be used or an external Ethernet board (the ENC28J60) can be used.
The built-in Bluetooth will be used if one is not specified, based on the
defaults set in zjs_net_l2.json.

To choose the ENC28J60 at build time, build with FORCE=zjs_net_l2_enc28j60.json
on the command line. (We'll work on a more elegant way to do this in the
future.)

The mechanics of using virtual dependencies is as follows. For the virtual
module itself, add a "defaults" property that is an object mapping board name
strings to default module name strings. We also recommend you add the "virtual"
property set to "yes", but this is not currently used; just useful to draw
attention to the usage for human readers of the JSON.

For example, here is the initial definition of the net-l2 module:
```
{
    "module": "net-l2",
    "description": "Virtual module to require an L2 network driver be present",
    "virtual": "yes",
    "defaults": {
        "arduino_101": "net-l2-bluetooth",
        "frdm_k64f": "net-l2-ethernet",
        "qemu_x86": "net-l2-tap"
    }
}
```

## Descriptions
We've also started adding a "description" property to the JSON objects; this is
not used programmatically at this time, but useful to capture a human-readable
comment describing the module to help readers understand what they're looking
at.

## Analyze parameters
The JSON parsing is done with `scripts/analyze`. The main Makefile is what
executes it and provides the required command line options:
```
SCRIPT=     JS script used to build (required)
BOARD=      Target being built (required)
JSON_DIR=   Directory to look for JSON files (required)
FORCE=      Force the inclusion of certain JSON files
PRJCONF=    File to write the prj.conf info too (required)
MAKEFILE=   Makefile to write the source file too (required)
MAKEBASE=   Makefile to use as a base to write MAKEFILE (required)
PROFILE=    Where to write the JerryScript profile
```
