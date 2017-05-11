ZJS API for Pattern Matching Engine (PME)
=========================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [API Documentation](#api-documentation)
* [Sample Apps](#sample-apps)

Introduction
------------
The Pattern Matching Engine API is the JavaScript version of the parallel data recognition engine with the following features:

 - 128 parallel Processing Elements (PE) each with
     - 128 byte input vector
     - 128 byte model memory
     - 8-Bit Arithmetic Units
 - Two distance evaluation norms with 16-bit resolution:
    - L1 norm (Manhattan Distance)
    - Lsup (Supremum) norm (Chebyshev Distance)
 - Support for up to 32,768 Categories
 - Classification states:
   - ID  - Identified
   - UNC - Uncertain
   - UNK - Unknown
 - Two Classification Functions:
   - k-nearest neighbors (KNN)
   - Radial Basis Function (RBF)
 - Support for up to 127 Contexts

Web IDL
-------
This IDL provides an overview of the interface; see below for documentation of
specific API functions.

```javascript
// require returns a PME object
// var pme = require('pme');

[NoInterfaceObject]
interface PME {
    begin();
    forget();
    configure(unsigned short context,
              unsigned short classificationMode,
              unsigned short distanceMode,
              unsigned short minInfluence,
              unsigned short maxInfluence);
    learn(number[] pattern, unsigned long category);
    unsigned long classify(number[] pattern);
    Neuron readNeuron(unsigned long id);
    writeVector(unsigned char[] pattern);
    unsigned short getCommittedCount();
    unsigned short getGlobalContext();
    setGlobalContext(unsigned short context);
    unsigned short getNeuronContext();
    setNeuronContext(unsigned short context);
    unsigned short getClassifierMode();
    setClassifierMode(unsigned short mode);
    unsigned short getDistanceMode();
    setDistanceMode(unsigned short mode);

    unsigned short RBF_MODE;        // RBF classification mode
    unsigned short KNN_MODE;        // KNN classification mode
    unsigned short L1_DISTANCE;     // L1 distance mode
    unsigned short LSUP_DISTANCE;   // LSUP distance mode
    unsigned long NO_MATCH;         // indicate a pattern could not be classified
    unsigned short MIN_CONTEXT;     // minimum context value
    unsigned short MAX_CONTEXT;     // maximum context value
    unsigned long MAX_VECTOR_SIZE;  // Maximum pattern size (in bytes)
    unsigned long FIRST_NEURON_ID;  // ID of first neuron in network
    unsigned long LAST_NEURON_ID;   // ID of last neuron in network
    unsigned long MAX_NEURONS;      // Number of neurons in the network
};

[NoInterfaceObject]
interface Neuron {
    unsigned short category;
    unsigned short context;
    unsigned short influence;
    unsigned short minInfluence;
};
```

API Documentation
-----------------
### PME.begin

`void begin();`

Initialize the PME so it is ready for operation.

### PME.forget

`void forget();`

Clear any data committed to the network, making the network ready to learn again.

### PME.configure

`configure(unsigned short context,
           unsigned short classificationMode,
           unsigned short distanceMode,
           unsigned short minInfluence,
           unsigned short maxInfluence);`

Configure the engine with parameters used for training data.

The `context` is valid context value range between 1-127. A context value of 0 enables all neurons, with no regard to their context.
The `classificationMode` is the classifying function to use. Valid values are:

 - PME.RBF_MODE (default)
 - PME.KNN_MODE

The `distanceMode` is the distance function to use. Valid values are:

 - PME.LSUP_DISTANCE
 - PME.L1_DISTANCE

The `minInfluence` is the minimum influence value used on the neuron.
The `maxInfluence` is the maximum influence value used on the neuron.

### PME.learn

`void learn(unsigned char[] pattern, unsigned long category);`

Takes a pattern and commits it to the network as training data for a given category.

The `pattern` is an array of bytes, up to 128 bytes in length.
The `category` indicates to the PME which category this training vector belongs to, that is, if a future input has a sufficiently similar pattern, it will be classified as the same category passed with this pattern.

### PME.classify

`unsigned long classify(unsigned char[] pattern);`

Takes a pattern and uses the committed neurons in the network to classify the pattern.

The `pattern` is an array of bytes, up to 128 bytes in length.

Returns `PME.NO_MATCH` if the input data did not match any of the trained categories. Otherwise, the trained category assigned by the network will be returned.

### PME.readNeuron

`Neuron readNeuron(unsigned long id);`

Read a specific neuron by its ID.

The `id` is value between 1-128 representing a specific neuron.

Returns the `Neuron` object in which to write the neuron data.

### PME.writeVector (KNN_MODE only)

`void writeVector(unsigned char[] pattern);`

(Should only be used in KNN_MODE.) Takes a pattern and uses the committed neurons in the network to classify the pattern.

The `pattern` is an array of bytes, up to 128 bytes in length.

### PME.getCommittedCount

`unsigned short getCommittedCount();`

Gets the number of committed neurons in the network.

Returns the number of comitted neurons in the network (a value between 1-128).

### PME.getGlobalContext

`unsigned short getGlobalContext();`

Reads the Global Context Register.

Returns the contents of the Global Context Register (a value between 0-127).

### PME.setGlobalContext

`void setGlobalContext(unsigned short context);`

Writes a value to the Global Context Register.

The `context` is valid context value range between 1-127. A context value of 0 enables all neurons, with no regard to their context.

### PME.getNeuronContext

`unsigned short getNeuronContext();`

Reads the Neuron Context Register.

Returns the contents of the Neuron Context Register (a value between 0-127).

### PME.setNeuronContext

`void setNeuronContext(unsigned short context);`

Writes a value to the Neuron Context Register.

The `context` is valid context value range between 1-127. A context value of 0 enables all neurons, with no regard to their context.

### PME.getClassifierMode

`unsigned short getClassifierMode();`

Gets the classifying function being used by the network.

Returns the the classifying function being used. Possible values are:

 - PME.RBF_MODE
 - PME.KNN_MODE

### PME.setClassifierMode

`void setClassifierMode(unsigned short mode);`

Sets the classifying function to be used by the network.

The `mode` is the classifying function to use. Valid values are:

 - PME.RBF_MODE (default)
 - PME.KNN_MODE

### PME.getDistanceMode

`unsigned short getDistanceMode();`

Gets the distance function being used by the network.

Returns the the distance function being used. Possible values are:

 - PME.LSUP_DISTANCE
 - PME.L1_DISTANCE

### PME.setDistanceMode

`void setDistanceMode(unsigned short mode);`

Sets the distance function to be used by the network.

The `mode` is the distance function to use. Valid values are:

 - PME.LSUP_DISTANCE
 - PME.L1_DISTANCE

Sample Apps
-----------
* [PME sample](../samples/CuriePME.js)
