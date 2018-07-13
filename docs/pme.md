ZJS API for Pattern Matching Engine (PME)
=========================================

* [Introduction](#introduction)
* [Web IDL](#web-idl)
* [Class: PME](#pme-api)
  * [pme.begin()](#pmebegin)
  * [pme.forget()](#pmeforget)
  * [pme.configure(context, classificationMode, distanceMode, minInfluence, maxInfluence)](#pmeconfigurecontext-classificationmode-distancemode-mininfluence-maxinfluence)
  * [pme.learn(pattern, category)](#pmelearnpattern-category)
  * [pme.classify(pattern)](#pmeclassifypattern)
  * [pme.readNeuron(id)](#pmereadneuronid)
  * [pme.writeVector(pattern)](#pmewritevectorpattern)
  * [pme.getCommittedCount()](#pmegetcommittedcount)
  * [pme.getGlobalContext()](#pmegetglobalcontext)
  * [pme.getClassifierMode()](#pmegetclassifiermode)
  * [pme.setClassifierMode(mode)](#pmesetclassifiermodemode)
  * [pme.getDistanceMode()](#pmegetdistancemode)
  * [pme.setDistanceMode(mode)](#pmesetdistancemodemode)
  * [pme.saveNeurons()](#pmesaveneurons)
  * [pme.restoreNeurons(objects)](#pmerestoreneuronsobjects)
* [Sample Apps](#sample-apps)

Introduction
------------
The Pattern Matching Engine API is the JavaScript version of the parallel data-recognition engine with the following features:

 - 128 parallel Processing Elements (PE) each with
     - 128 byte input vector
     - 128 byte model memory
     - 8-Bit Arithmetic Units
 - Two distance-evaluation norms with 16-bit resolution:
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
specific API functions.  We also have a short document explaining [ZJS WebIDL conventions](Notes_on_WebIDL.md).
<details>
<summary> Click to show/hide WebIDL</summary>
<pre>
// require returns a PME object
// var pme = require('pme');
[ReturnFromRequire]
interface PME {
    void begin();
    void forget();
    void configure(unsigned short context,
                   unsigned short classificationMode,
                   unsigned short distanceMode,
                   unsigned short minInfluence,
                   unsigned short maxInfluence);
    void learn(sequence < long > pattern, unsigned long category);
    unsigned long classify(sequence < long > pattern);
    Neuron readNeuron(unsigned long id);
    void writeVector(sequence < long > pattern);
    unsigned short getCommittedCount();
    unsigned short getGlobalContext();
    unsigned short getClassifierMode();
    void setClassifierMode(unsigned short mode);
    unsigned short getDistanceMode();
    void setDistanceMode(unsigned short mode);
    sequence < Json > saveNeurons();
    void restoreNeurons(sequence < Json > objects);
<p>
    attribute unsigned short RBF_MODE;       // RBF classification mode
    attribute unsigned short KNN_MODE;       // KNN classification mode
    attribute unsigned short L1_DISTANCE;    // L1 distance mode
    attribute unsigned short LSUP_DISTANCE;  // LSUP distance mode
    attribute unsigned long NO_MATCH;        // indicates a pattern could not
                                             // be classified
    attribute unsigned short MIN_CONTEXT;    // minimum context value
    attribute unsigned short MAX_CONTEXT;    // maximum context value
    attribute unsigned long MAX_VECTOR_SIZE; // Maximum pattern size (in bytes)
    attribute unsigned long FIRST_NEURON_ID; // ID of first neuron in network
    attribute unsigned long LAST_NEURON_ID;  // ID of last neuron in network
    attribute unsigned long MAX_NEURONS;     // Number of neurons in the network
};<p>dictionary Neuron {
    unsigned short category;
    unsigned short context;
    unsigned short AIF;
    unsigned short minIF;
};
</pre>
</details>

PME API
-------
### pme.begin()

Initialize the PME so it is ready for operation.

### pme.forget()

Clear any data committed to the network, making the network ready to learn again.

### pme.configure(context, classificationMode, distanceMode, minInfluence, maxInfluence)
* `context` *unsigned short* This value has a range between 1-127. A context value of 0 enables all neurons, with no regard to their context.
* `classificationMode` *unsigned short* The classifying function to use. Valid values are: PME.RBF_MODE (default) or PME.KNN_MODE.
* `distanceMode` *unsigned short* The distance function to use. Valid values are: PME.LSUP_DISTANCE or PME.L1_DISTANCE.
* `minInfluence` *unsigned short*  The minimum influence value used on the neuron.
* `maxInfluence` *unsigned short* The maximum influence value used on the neuron.

Configure the engine with parameters used for training data.

### pme.learn(pattern, category)
* `pattern` *array of bytes* An array of bytes up to 128 bytes in length.
* `category` *unsigned long* Indicates to the PME to which category this training vector belongs; that is, if a future input has a sufficiently similar pattern, it will be classified as the same category as the passed-in pattern.

Takes a pattern and commits it to the network as training data for a given category.

### pme.classify(pattern)
* `pattern` *array of bytes* An array of bytes up to 128 bytes in length.
* Returns: `PME.NO_MATCH` if the input data did not match any of the trained categories. Otherwise, the trained category assigned by the network will be returned.

Takes a pattern and uses the committed neurons in the network to classify the pattern.

### pme.readNeuron(id)
* `id` *unsigned long* A value between 1-128 representing a specific neuron.
* Returns: the `Neuron` object in which to write the neuron data.

Read a specific neuron by its ID.

### pme.writeVector(pattern)
* `pattern` *array of bytes* An array of bytes up to 128 bytes in length.

(Should only be used in KNN_MODE.) Takes a pattern and uses the committed neurons in the network to classify the pattern.

### pme.getCommittedCount()
*Returns: the number of comitted neurons in the network (a value between 0-128).

Gets the number of committed neurons in the network.

### pme.getGlobalContext()
* Returns: the contents of the Global Context Register (a value between 0-127).

Reads the Global Context Register.

### pme.getClassifierMode()
* Returns: the classifying function being used. Possible values are: PME.RBF_MODE or PME.KNN_MODE.

Gets the classifying function being used by the network.

### pme.setClassifierMode(mode)
* `mode` *unsigned short* The classifying function to use. Valid values are: PME.RBF_MODE (default) or PME.KNN_MODE.

Sets the classifying function to be used by the network.

### pme.getDistanceMode()
* Returns: the distance function being used. Possible values are: PME.LSUP_DISTANCE or PME.L1_DISTANCE.

Gets the distance function being used by the network.

### pme.setDistanceMode(mode)
* `mode` *unsigned short* The distance function to use. Valid values are: PME.LSUP_DISTANCE or PME.L1_DISTANCE.

Sets the distance function to be used by the network.

### pme.saveNeurons()
* Returns: an array of JSON objects.

Export committed neuron data into an array of JSON objects in the following format, maximum number of objects to save is 128:

`{
     "category": 100,
     "context": 1,
     "AIF": 40,
     "minIF": 2,
     "vector": [10,10,10,10] // up to 128 bytes
 }`

### pme.restoreNeurons(objects)
* `objects` *array of JSON objects*

Restore neurons in an array of JSON objects in the following format, maximum number of objects to restore is 128:

`{
     "category": 100,
     "context": 1,
     "AIF": 40,
     "minIF": 2,
     "vector": [10,10,10,10] // up to 128 bytes
 }`

Sample Apps
-----------
* [PME sample](../samples/PME.js)
* [PME Save and Restore sample](../samples/PMESaveRestore.js)
