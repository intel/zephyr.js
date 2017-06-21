// Copyright (c) 2017, Intel Corporation.

console.log("Test case for PME APIs");
var assert = require("Assert.js");
var pme = require("pme");

var neuron;
var pmeInfos = function() {
    console.log("PME mode:");
    var mode = pme.getClassifierMode();
    if (mode === pme.RBF_MODE) {
        console.log("    classification: RBF_MODE");
    } else if (mode === pme.KNN_MODE) {
        console.log("    classification: KNN_MODE");
    }

    mode = pme.getDistanceMode();
    if (mode === pme.L1_DISTANCE) {
        console.log("    distance: L1_DISTANCE");
    } else if (mode === pme.LSUP_DISTANCE) {
        console.log("    distance: LSUP_DISTANCE");
    }

    console.log("Neuron count: " + pme.getCommittedCount());
    for (var i = 1; i <= pme.getCommittedCount(); i++) {
        neuron = pme.readNeuron(i);
        console.log("    id=" + i +
                    " category=" + neuron.category +
                    " context=" + neuron.context +
                    " AIF=" + neuron.AIF +
                    " minIF=" + neuron.minIF);
    }
}

var category;
var pmeClassification = function() {
    for (var i = 0; i < classifys.length; i++) {
        category = pme.classify(classifys[i]);

        if (category === pme.NO_MATCH) {
            category = "no match";
        }

        console.log("classifys[" + i + "] as category " + category);
    }
}

var pmeKeyWords = [
    [pme.RBF_MODE, "number", "RBF_MODE"],
    [pme.KNN_MODE, "number", "KNN_MODE"],
    [pme.L1_DISTANCE, "number", "L1_DISTANCE"],
    [pme.LSUP_DISTANCE, "number", "LSUP_DISTANCE"],
    [pme.NO_MATCH, "number", "NO_MATCH"],
    [pme.MIN_CONTEXT, "number", "MIN_CONTEXT"],
    [pme.MAX_CONTEXT, "number", "MAX_CONTEXT"],
    [pme.MAX_VECTOR_SIZE, "number", "MAX_VECTOR_SIZE"],
    [pme.FIRST_NEURON_ID, "number", "FIRST_NEURON_ID"],
    [pme.LAST_NEURON_ID, "number", "LAST_NEURON_ID"],
    [pme.MAX_NEURONS, "number", "MAX_NEURONS"]
];
for (var i = 0; i < pmeKeyWords.length; i++) {
    assert(pmeKeyWords[i][2] in pme,
           "PMEkeyWord: '" + pmeKeyWords[i][2] + "' is a PME keyWord");

    assert(typeof pmeKeyWords[i][0] === pmeKeyWords[i][1] &&
           pmeKeyWords[i][0] !== null,
           "PMEkeyWord: '" + pmeKeyWords[i][2] + "' is defined as '" +
           pmeKeyWords[i][0] + "'");
}

var classifierMode = pme.getClassifierMode();
assert(classifierMode === pme.RBF_MODE,
       "PMEMode: get default value for classifier mode as 'RBF_MODE'");

var distanceMode = pme.getDistanceMode();
assert(distanceMode === pme.L1_DISTANCE,
       "PMEMode: get default value for distance mode as 'L1_DISTANCE'");

var pmeModes = [
    ["string", ""],
    [true, ""],
    [1024, ""],
    [pme.KNN_MODE, "KNN_MODE"],
    [pme.RBF_MODE, "RBF_MODE"],
    [pme.LSUP_DISTANCE, "LSUP_DISTANCE"],
    [pme.L1_DISTANCE, "L1_DISTANCE"]
];

for (var i = 0; i < pmeModes.length; i++) {
    if (i <= 2) {
        assert.throws(function() {
            pme.setClassifierMode(pmeModes[i][0]);
        }, "PMEMode: set classifier mode as '" + pmeModes[i][0] + "'");

        assert.throws(function() {
            pme.setDistanceMode(pmeModes[i][0]);
        }, "PMEMode: set distance mode as '" + pmeModes[i][0] + "'");

        continue;
    }

    if (i === 3 || i === 4) {
        pme.setClassifierMode(pmeModes[i][0]);
        classifierMode = pme.getClassifierMode();
        assert(classifierMode === pmeModes[i][0],
               "PMEMode: set and get classifier mode as '" +
               pmeModes[i][1] + "'");

        continue;
    }

    if (i === 5 || i === 6) {
        pme.setDistanceMode(pmeModes[i][0]);
        distanceMode = pme.getDistanceMode();
        assert(distanceMode === pmeModes[i][0],
               "PMEMode: set and get distance mode as '" +
               pmeModes[i][1] + "'");
    }
}

var globalContext = pme.getGlobalContext();
assert(globalContext === 1,
       "PMEContext: get default value for global context as '" +
       globalContext + "'");

var neuronContext = pme.getNeuronContext();
assert(neuronContext === 127,
       "PMEContext: get default value for neuron context as '" +
       neuronContext + "'");

var contextValues = [ 1, 10, 100, 127, 200, 0, "string", true ];
for (var i = 0; i < contextValues.length; i++) {
    if (i <= 3) {
        pme.setGlobalContext(contextValues[i]);
        globalContext = pme.getGlobalContext();
        assert(globalContext === contextValues[i],
               "PMEContext: set and get global context as '" +
               contextValues[i] + "'");

        pme.setNeuronContext(contextValues[i]);
        neuronContext = pme.getNeuronContext();
        assert(neuronContext === contextValues[i],
               "PMEContext: set and get neuron context as '" +
               contextValues[i] + "'");
    } else {
        assert.throws(function() {
            pme.setGlobalContext(contextValues[i]);
        }, "PMEContext: set global context as '" + contextValues[i] + "'\n");

        assert.throws(function() {
            pme.setNeuronContext(contextValues[i]);
        }, "PMEContext: set neuron context as '" + contextValues[i] + "'\n");
    }
}

var defaultCommittedCount = pme.getCommittedCount();
assert(defaultCommittedCount === 0,
       "PMEObject: get default value for committed neurons as '" +
       defaultCommittedCount + "'");

assert.throws(function() {
    neuron = pme.readNeuron(defaultCommittedCount);
}, "PMEObject: read neuro information by invalid ID");

var trainings = [
    [50, 55, 60, 55, 50],    // upper arch
    [50, 50, 50, 50, 50],    // flat line
    [50, 45, 40, 45, 50]     // under arch
];
var classifys = [
    [50, 60, 70, 60, 50],    // upper arch
    [50, 54, 58, 54, 50],    // upper arch
    [40, 45, 50, 45, 40],    // upper arch
    [50, 52, 50, 48, 50],    // flat line
    [50, 46, 42, 46, 50],    // under arch
    [50, 40, 30, 40, 50],    // under arch
    [60, 55, 50, 55, 60]     // under arch
];

pme.begin();
pme.learn(trainings[0], 80);
pme.learn(trainings[1], 50);
pme.learn(trainings[2], 20);

var committedCount = pme.getCommittedCount();
assert(1 <= committedCount && committedCount <= 128,
       "PMEObject: get value for committed neurons as '" +
       committedCount + "'");
assert(1 <= committedCount && committedCount <= 128 &&
       committedCount !== defaultCommittedCount,
       "PMEObject: PME is beginning and is learning");

neuron = pme.readNeuron(1);
var neuronInfos = [
    [neuron.category, "number", 80, "category"],
    [neuron.context, "number", 1, "context"],
    [neuron.AIF, "number", 20, "AIF"],
    [neuron.minIF, "number", 2, "minIF"]
];

for (var i = 0; i < neuronInfos.length; i++) {
    assert(neuronInfos[i][3] in neuron,
           "NeuronObject: '" + neuronInfos[i][3] + "' is a neuron property");

    assert(typeof neuronInfos[i][0] === neuronInfos[i][1] &&
           neuronInfos[i][0] === neuronInfos[i][2],
           "NeuronObject: '" + neuronInfos[i][3] + "' is defined as '" +
           neuronInfos[i][0] + "'");
}

category = pme.classify(classifys[0]);
assert(category === pme.NO_MATCH,
       "PMEObject: get classifying value as no match");

category = pme.classify(classifys[1]);
assert(category === 80, "PMEObject: get classifying value");

assert.throws(function() {
    pme.writeVector([60, 60, 60, 60, 60]);
}, "PMEObject: write vector with 'RBF_MODE'");

pme.forget();
committedCount = pme.getCommittedCount();
assert(committedCount === defaultCommittedCount,
       "PMEObject: forget learned data and ready to learn again");

var configureParameters = [
    [1, pme.RBF_MODE, pme.LSUP_DISTANCE, 5, 10, "RBF_MODE", "LSUP_DISTANCE"],
    [1, pme.RBF_MODE, pme.L1_DISTANCE, 5, 10, "RBF_MODE", "L1_DISTANCE"],
    [1, pme.KNN_MODE, pme.LSUP_DISTANCE, 5, 10, "KNN_MODE", "LSUP_DISTANCE"],
    [1, pme.KNN_MODE, pme.L1_DISTANCE, 5, 10, "KNN_MODE", "L1_DISTANCE"]
];

for (var i = 0; i < configureParameters.length; i++) {
    pme.forget();

    pme.configure(configureParameters[i][0],
                  configureParameters[i][1],
                  configureParameters[i][2],
                  configureParameters[i][3],
                  configureParameters[i][4]);

    pme.learn(trainings[0], 80);
    pme.learn(trainings[1], 50);
    pme.learn(trainings[2], 20);

    pmeInfos();
    pmeClassification();

    neuron = pme.readNeuron(1);
    assert(pme.getGlobalContext() === configureParameters[i][0] &&
           pme.getClassifierMode() === configureParameters[i][1] &&
           pme.getDistanceMode() === configureParameters[i][2] &&
           neuron.AIF === configureParameters[i][4] &&
           neuron.minIF === configureParameters[i][3],
           "PMEObject: reconfigure with '" + configureParameters[i][5] +
           "' and '" + configureParameters[i][6] + "'\n");
}

assert(pme.getGlobalContext() === 1,
       "PMEObject: reconfigure with context as '1'");

assert(pme.getClassifierMode() === pme.KNN_MODE,
       "PMEObject: reconfigure with classifier as 'KNN_MODE'");

assert(pme.getDistanceMode() === pme.L1_DISTANCE,
       "PMEObject: reconfigure with distance as 'L1_DISTANCE'");

assert(neuron.AIF === 10,
       "PMEObject: reconfigure with max influence as '10'");

assert(neuron.minIF === 5,
       "PMEObject: reconfigure with min influence as '5'");

assert.throws(function() {
    pme.configure(0, pme.KNN_MODE, pme.L1_DISTANCE, 5, 10);
}, "PMEObject: reconfigure with context as '0'");

pme.writeVector([60, 60, 60, 60, 60]);

assert.result();
