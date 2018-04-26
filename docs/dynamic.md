Dynamic mode - Adds the ability to run JS from the filesystem
============================================================

* [Introduction](#introduction)
* [Compilation](#compilation)
* [How to use](#howtouse)

Introduction
------------
Compiling ZJS in dynamic mode gives you the ability to run a JavaScript file from
the filesystem of the board. Note that running a new JavaScript file will halt
the current one. Also note that you need to include all the modules you wish to
use in the original JavaScript file you compile ZJS with. If the JavaScript you
try to run from the filesystem calls for a module not included in the original
JavaScript file, it will fail.

Compilation
------------
This command will enable dynamic mode:
```bash
$ make dynamic
```

How to use
-------
Build ZJS with ashell, then copy any JavaScript files you want to run onto the
filesystem using 'save'.

In your primary JavaScript file, use 'runJS('filename.js')' whenever you want to load
a different JavaScript file from the filesystem. Remember your current JavaScript
will halt once this command is called. You can add this command anywhere you want
a different JavaScript to load.

Build ZJS with your primary JavaScript file and reboot.  
