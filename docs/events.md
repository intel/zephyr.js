ZJS API for Events
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides event API's which match Node.js Event's. We describe them here as there could
potentially be minor differences.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript

callback ListenerCallback = void (...);

interface EventEmitter {
    this on(string event, ListenerCallback listener);
    this addListener(string event, ListenerCallback listener);
    boolean emit(string event, optional arg1, ...);
    this removeListener(string event, ListenerCallback listener);
    this removeAllListeners(string event);
    string[] eventNames(void);
    number getMaxListeners(void);
    ListenerCallback[] listeners(string event);
    this setMaxListeners(number max);
};
```

API Documentation
-----------------

### on
`this on(string event, ListenerCallback listener);`

Add an event listener function.

The `event` argument is the name of the event which you are adding a listener too.

The `listener` argument is the function that you wish to be called when this event
is emitted/triggered.

Returns `this` so calls can be chained.

### addListener
`this addListener(string event, ListenerCallback listener);`

Same as `on()`.

### emit
`boolean emit(string event, optional arg1, ...);`

Triggers an event. Any listener functions which have been added to the event emitter
under the event name will be called.

The `event` argument is the name of the event that you want to emit.

All other arguments will be given to any registered listener functions.

Returns true if there were any listener functions called.

### removeListener
`this removeListener(string event, ListenerCallback listener);`

Removes a listener function from an event.

The `event` argument is the name of the event you are removing the listener from.

The `listener` arugment is the actual function you want to remove as a listener.

Returns `this` so calls can be chained.

### removeAllListeners
`this removeAllListeners(string event);`

Removes all listeners from an event

The `event` argument is the name of the event to remove all listeners from

Returns `this` so calls can be chained

### eventNames
`string[] eventNames(void);`

Get a list of event names from an event emitter object.

Returns an array of strings that correspond to any events. Will return undefined
if there are no event's or event listeners for this event emitter.

### getMaxListeners
`number getMaxListeners(void);`

Get the maximum number of listeners allowed for this event emitter object.

Returns the max number of listeners allowed.

### listeners
`ListenerCallback[] listeners(string event);`

Get a list of listeners for an event.

The `event` parameter is the name of the event you are retrieving the listerners from.

Returns an array of functions which correspond to the listeners for the event specified.

### setMaxListeners
`this setMaxListeners(number max);`

Set the max number of listeners for an event emitter object

The `max` argument is the number of listeners the event emitter can have

Returns `this` so calls can be chained.

Sample Apps
-----------
* [Events sample](../samples/tests/Events.js)
