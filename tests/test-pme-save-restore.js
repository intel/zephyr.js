// Copyright (c) 2017, Intel Corporation.

console.log("Test case for PME save and restore APIs");
var assert = require("Assert.js");
var pme = require("pme");
var fs = require("fs");

var file = "neurons.txt";

var workState = function() {
    var state;
    var category = pme.classify(classifications);

    if (category === pme.NO_MATCH) {
        category = "no match";
        state = false;
    } else if (category === 2) {
        state = true;
    }

    console.log("Classified [50, 52, 54, 52, 50] as category '" + category + "'\n");

    return state;
}

var trainings = [
    [70, 70, 70, 70, 70],
    [50, 50, 50, 50, 50],
    [30, 30, 30, 30, 30]
];

var classifications = [50, 52, 54, 52, 50];

pme.begin();

pme.learn(trainings[0], 1);
pme.learn(trainings[1], 2);
pme.learn(trainings[2], 3);

var neurons = pme.saveNeurons();
var jsonOut = JSON.stringify(neurons);
fs.writeFileSync(file, jsonOut);
console.log("Saved " + neurons.length + " neurons to file '" + file +
            "' with " + jsonOut.length + " bytes");

var workOnNormal = workState();

pme.forget();
console.log("Cleared neurons");

var workOnForget = workState();

var stats = fs.statSync(file);
var buf = fs.readFileSync(file);
neurons = JSON.parse(buf.toString("ascii"));
pme.restoreNeurons(neurons);
console.log("Restoring " + neurons.length + " neurons from file '" + file +
            "' with " + stats.size + " bytes");

var workOnRestore = workState();

assert(workOnNormal === true &&
       workOnForget === false &&
       workOnRestore === true,
       "PMEObject: neurons can be saved and restored");

assert.result();
