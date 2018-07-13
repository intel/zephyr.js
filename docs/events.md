ZJS API for Events
==================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: EventEmitter](#eventemitter-api)
  * [EventEmitter.on(event, listener)](#eventemitteronevent-listener)
  * [EventEmitter.addListener(event, listener)](#eventemitteraddlistenerevent-listener)
  * [EventEmitter.emit(event, [args...])](#eventemitteremitevent-args)
  * [EventEmitter.removeListener(event, listener)](#eventemitterremovelistenerevent-listener)
  * [EventEmitter.removeAllListeners(event)](#eventemitterremovealllistenersevent)
  * [EventEmitter.eventNames()](#eventemittereventnames)
  * [EventEmitter.getMaxListeners()](#eventemittergetmaxlisteners)
  * [EventEmitter.listeners(event)](#eventemitterlistenersevent)
  * [EventEmitter.setMaxListeners(max)](#eventemittersetmaxlistenersmax)
* [Sample Apps](#sample-apps)

Introduction
------------
ZJS provides event APIs that match `Node.js` `Event`s. We describe
them here as there could be minor differences.

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
callback ListenerCallback = void (any... params);<p>interface EventEmitter {
    this on(string event, ListenerCallback listener);
    this addListener(string event, ListenerCallback listener);
    boolean emit(string event, any... args);
    this removeListener(string event, ListenerCallback listener);
    this removeAllListeners(string event);
    sequence < string > eventNames();
    long getMaxListeners();
    sequence < ListenerCallback > listeners(string event);
    this setMaxListeners(long max);
};
</pre>
</details>

EventEmitter API
----------------
### EventEmitter.on(event, listener)
* `event` *string* The name of the event that you are adding a listener to.
* `listener` *ListenerCallback* The function that you wish to be called when this event is emitted/triggered.
* Returns: `this` so calls can be chained.

Add an event listener function.

### EventEmitter.addListener(event, listener)
* `event` *string* The name of the event that you are adding a listener to.
* `listener` *ListenerCallback* The function that you wish to be called when this event is emitted/triggered.
* Returns: `this` so calls can be chained.

Same as `EventEmitter.on()`.

### EventEmitter.emit(event, [args...])
* `event` *string* The name of the event that you want to emit.
* `args` *optional* All other arguments will be given to any registered listener functions.
* Returns: true if there were any listener functions called.

Triggers an event. Any listener functions that have been added to the
event emitter under the event name will be called.


### EventEmitter.removeListener(event, listener)
* `event` *string* The name of the event you are removing the listener from.
* `listener` *ListenerCallback* The function you want to remove as a listener.
* Returns: `this` so calls can be chained.

Removes a listener function from an event.

### EventEmitter.removeAllListeners(event)
* `event` *string* The name of the event from which to remove all listeners.
* Returns: `this` so calls can be chained.

Removes all listeners from an event


### EventEmitter.eventNames()
* Returns: an array of strings that correspond to any events. Will return undefined if there are no event's or event listeners for this event emitter.

Get a list of event names from an event emitter object.

### EventEmitter.getMaxListeners()
* Returns: the maximum number of listeners allowed.

Get the maximum number of listeners allowed for this event emitter object.

### EventEmitter.listeners(event)
* `event` *string* The name of the event from which to retrieve the listerners.
* Returns: an array of functions that correspond to the `ListenerCallbacks` for the event specified.

Get a list of listeners for an event.

### EventEmitter.setMaxListeners(max)
* `max` *long*  The number of listeners the event emitter can have.
* Returns: `this`, so calls can be chained.

Set the max number of listeners for an event emitter object

Sample Apps
-----------
* [Events sample](../samples/tests/Events.js)
